/***********************************************************************
* b5500emulator
************************************************************************
* Copyright (c) 2016-2017, Reinhard Meyer, DL5UY
* Licensed under the MIT License,
*       see LICENSE
* based on (C) work by Nigel Williams and Paul Kimpel
* see: https://github.com/pkimpel/retro-b5500
************************************************************************
* IRQ and I/O
************************************************************************
* 2016-02-21 R.Meyer
*   Converted Paul's work from Javascript to C
* 2017-07-17 R.Meyer
*   changed "this" to "cpu" to avoid errors when using g++
* 2017-09-17 R.Meyer
*   added unit table
***********************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "b5500_common.h"

/*
 * table of I/O units
 * indexed by unit designator and read bit of I/O descriptor
 */
UNIT unit[32][2] = {
        /*00*/ {{NULL, 0},   {NULL, 0}},
        /*01*/ {{"MTA", 47-47}, {"MTA", 47-47}},
        /*02*/ {{NULL, 0},   {NULL, 0}},
        /*03*/ {{"MTB", 47-46}, {"MTB", 47-46}},
        /*04*/ {{"DRA", 47-31}, {"DRA", 47-31}},
        /*05*/ {{"MTC", 47-45}, {"MTC", 47-45}},
        /*06*/ {{"DF1", 47-29}, {"DF1", 47-29}},
        /*07*/ {{"MTD", 47-44}, {"MTD", 47-44}},
        /*08*/ {{"DRB", 47-30}, {"DRB", 47-30}},
        /*09*/ {{"MTE", 47-43}, {"MTE", 47-43}},
        /*10*/ {{"CPA", 47-25}, {"CRA", 47-24}},
        /*11*/ {{"MTF", 47-42}, {"MTF", 47-42}},
        /*12*/ {{"DF2", 47-28}, {"DF2", 47-28}},
        /*13*/ {{"MTH", 47-41}, {"MTH", 47-41}},
        /*14*/ {{NULL, 0},   {"CRB", 23}},
        /*15*/ {{"MTJ", 47-40}, {"MTJ", 47-40}},
        /*16*/ {{"DCC", 47-17}, {"DCC", 47-17}},
        /*17*/ {{"MTK", 47-39}, {"MTK", 47-39}},
        /*18*/ {{"PP1", 47-21}, {"PR1", 47-20}},
        /*19*/ {{"MTL", 47-38}, {"MTL", 47-38}},
        /*20*/ {{"PP1", 47-19}, {"PR1", 47-18}},
        /*21*/ {{"MTM", 47-37}, {"MTM", 47-37}},
        /*22*/ {{"LP1", 47-27}, {NULL, 0}},
        /*23*/ {{"MTN", 47-36}, {"MTN", 47-36}},
        /*24*/ {{NULL, 0},   {NULL, 0}},
        /*25*/ {{"MTP", 47-35}, {"MTP", 47-35}},
        /*26*/ {{"LP1", 47-27}, {NULL, 0}},
        /*27*/ {{"MTR", 47-34}, {"MTR", 47-34}},
        /*28*/ {{NULL, 0},   {NULL, 0}},
        /*29*/ {{"MTS", 47-33}, {"MTS", 47-33}},
        /*30*/ {{"SPO", 47-22}, {"SPO", 47-22}},
        /*31*/ {{"MTT", 47-32}, {"MTT", 47-32}},
};

/*
 * Tests and returns the presence bit [2:1] of the "word" parameter,
 * which it assumes is a control word. If [2:1] is 0, the p-bit interrupt
 * is set; otherwise no further action
 */
BIT presenceTest(CPU *cpu, WORD48 word)
{
        if (word & MASK_PBIT)
                return true;

        if (cpu->r.NCSF) {
                cpu->r.I = (cpu->r.I & 0x0F) | 0x70; // set I05/6/7: p-bit
                signalInterrupt(cpu);
        }
        return false;
}

WORD48 interrogateUnitStatus(CPU *cpu)
{
        WORD48 result = 0;
        if (dotrcins)
                printf("******************** TUS ********************\n");
        // report MTA, DF1, CR1, SPO as ready
        result |= 1ll << unit[ 1][1].readybit;
        result |= 1ll << unit[ 6][1].readybit;
        result |= 1ll << unit[11][1].readybit;
        result |= 1ll << unit[30][1].readybit;
        if (dotrcins)
                printf("*\tresult=%016llo\n", result);
        // simulate timer
        CC->RTC++;
        if (CC->RTC >= 63) {
                CC->RTC = 0;
                CC->IAR = 00022;
                if (dotrcins)
                        printf("*\tTIMER!!!\n");
                sleep(1);
                //dotrcins = true;
        }
        return result;
}

WORD48 interrogateIOChannel(CPU *cpu)
{
        WORD48 result = 0;
        if (dotrcins)
                printf("******************** TIO ********************\n");
        // report I/O control unit 1
        result = 1ll;
        if (dotrcins)
                printf("*\tresult=%016llo\n", result);
        return result;
}

/*
 * Implements the 3011=SFI operator and the parts of 3411=SFT that are
 * common to it. "forced" implies Q07F: a hardware-induced SFI syllable.
 * "forTest" implies use from SFT
 */
void storeForInterrupt(CPU *cpu, BIT forced, BIT forTest)
{
        BIT             saveAROF = cpu->r.AROF;
        BIT             saveBROF = cpu->r.BROF;
        unsigned        temp;

        printf("*** storeForInterrupt ***\n");

        if (forced || forTest) {
                cpu->r.NCSF = 0; // switch to Control State
        }

        if (cpu->r.CWMF) {
                // in Character Mode, get the correct TOS address from X
                temp = cpu->r.S;
                cpu->r.S = (cpu->r.X & MASK_RCWrF) >> SHFT_RCWrF;
                cpu->r.X = (cpu->r.X & MASK_RCWrC) | (temp << SHFT_RCWrF);

                if (saveAROF || forTest) {
                        ++cpu->r.S;
                        storeAviaS(cpu); // [S] = A
                }

                if (saveBROF || forTest) {
                        ++cpu->r.S;
                        storeBviaS(cpu); // [S] = B
                }

                // store Character Mode Interrupt Loop-Control Word (ILCW)
                // 444444443333333333222222222211111111110000000000
                // 765432109876543210987654321098765432109876543210
                // 11A000000XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
                cpu->r.B = INIT_ILCW | cpu->r.X |
                        ((WORD48)saveAROF << SHFT_ILCWAROF);
                ++cpu->r.S;
                if (dotrcins)
                        printf("ILCW:");
                storeBviaS(cpu); // [S] = ILCW
        } else {
                // in word mode, save B and A if not empty
                if (saveBROF || forTest) {
                        ++cpu->r.S;
                        storeBviaS(cpu); // [S] = B
                }

                if (saveAROF || forTest) {
                        ++cpu->r.S;
                        storeAviaS(cpu); // [S] = A
                }
        }

        // store Interrupt Control Word (ICW)
        // 444444443333333333222222222211111111110000000000
        // 765432109876543210987654321098765432109876543210
        // 110000RRRRRRRRR0MS00000V00000NNNNMMMMMMMMMMMMMMM
        cpu->r.B = INIT_ICW |
                ((WORD48)cpu->r.M << SHFT_ICWrM) |
                ((WORD48)cpu->r.N << SHFT_ICWrN) |
                ((WORD48)cpu->r.VARF << SHFT_ICWVARF) |
                ((WORD48)cpu->r.SALF << SHFT_ICWSALF) |
                ((WORD48)cpu->r.MSFF << SHFT_ICWMSFF) |
                ((WORD48)cpu->r.R << SHFT_ICWrR);
        ++cpu->r.S;
        if (dotrcins)
                printf("ICW: ");
        storeBviaS(cpu); // [S] = ICW

        // store Interrupt Return Control Word (IRCW)
        // 444444443333333333222222222211111111110000000000
        // 765432109876543210987654321098765432109876543210
        // 11B0HHHVVVLLGGGKKKFFFFFFFFFFFFFFFCCCCCCCCCCCCCCC
        cpu->r.B = INIT_RCW |
                ((WORD48)cpu->r.C << SHFT_RCWrC) |
                ((WORD48)cpu->r.F << SHFT_RCWrF) |
                ((WORD48)cpu->r.K << SHFT_RCWrK) |
                ((WORD48)cpu->r.G << SHFT_RCWrG) |
                ((WORD48)cpu->r.L << SHFT_RCWrL) |
                ((WORD48)cpu->r.V << SHFT_RCWrV) |
                ((WORD48)cpu->r.H << SHFT_RCWrH) |
                ((WORD48)saveBROF << SHFT_RCWBROF);
        ++cpu->r.S;
        if (dotrcins)
                printf("IRCW:");
        storeBviaS(cpu); // [S] = IRCW

        if (cpu->r.CWMF) {
                // if CM, get correct R value from last MSCW
                temp = cpu->r.F;
                cpu->r.F = cpu->r.S;
                cpu->r.S = temp;

                loadBviaS(cpu); // B = [S]: get last RCW
                cpu->r.S = (cpu->r.B & MASK_RCWrF) >> SHFT_RCWrF;

                loadBviaS(cpu); // B = [S]: get last MSCW
                cpu->r.R = (cpu->r.B & MASK_MSCWrR) >> SHFT_MSCWrR;
                cpu->r.S = cpu->r.F;
        }

        // build the Initiate Control Word (INCW)
        // 444444443333333333222222222211111111110000000000
        // 765432109876543210987654321098765432109876543210
        // 11000QQQQQQQQQYYYYYYZZZZZZ0TTTTTCSSSSSSSSSSSSSSS
        cpu->r.B = INIT_INCW |
                ((WORD48)cpu->r.S << SHFT_INCWrS) |
                ((WORD48)cpu->r.CWMF << SHFT_INCWMODE) |
                (((WORD48)cpu->r.TM << SHFT_INCWrTM) & MASK_INCWrTM) |
                ((WORD48)cpu->r.Z << SHFT_INCWrZ) |
                ((WORD48)cpu->r.Y << SHFT_INCWrY) |
                ((WORD48)cpu->r.Q01F << SHFT_INCWQ01F) |
                ((WORD48)cpu->r.Q02F << SHFT_INCWQ02F) |
                ((WORD48)cpu->r.Q03F << SHFT_INCWQ03F) |
                ((WORD48)cpu->r.Q04F << SHFT_INCWQ04F) |
                ((WORD48)cpu->r.Q05F << SHFT_INCWQ05F) |
                ((WORD48)cpu->r.Q06F << SHFT_INCWQ06F) |
                ((WORD48)cpu->r.Q07F << SHFT_INCWQ07F) |
                ((WORD48)cpu->r.Q08F << SHFT_INCWQ08F) |
                ((WORD48)cpu->r.Q09F << SHFT_INCWQ09F);
        cpu->r.M = (cpu->r.R<<6) + 8; // store initiate word at R+@10
        if (dotrcins)
                printf("INCW:");
        storeBviaM(cpu); // [M] = INCW

        cpu->r.M = 0;
        cpu->r.R = 0;
        cpu->r.MSFF = 0;
        cpu->r.SALF = 0;
        cpu->r.BROF = 0;
        cpu->r.AROF = 0;
        if (forTest) {
                cpu->r.TM = 0;
                cpu->r.MROF = 0;
                cpu->r.MWOF = 0;
        }

        if (forced || forTest) {
                cpu->r.CWMF = 0;
        }

        if (cpu->isP1) {
                // if it's P1
                if (forTest) {
                        loadBviaM(cpu); // B = [M]: load DD for test
                        cpu->r.C = (cpu->r.B & MASK_RCWrC) >> SHFT_RCWrC;
                        cpu->r.L = 0;
                        cpu->r.PROF = 0; // require fetch at SECL
                        cpu->r.G = 0;
                        cpu->r.H = 0;
                        cpu->r.K = 0;
                        cpu->r.V = 0;
                } else {
                        if (dotrcins)
                                printf("injected ITI\n");
                        cpu->r.T = 0211; // inject 0211=ITI into P1's T register
                }
        } else {
                // if it's P2
                stop(cpu); // idle the P2 processor
                CC->P2BF = 0; // tell CC and P1 we've stopped
        }
}

ADDR15 getandclearInterrupt(CPU *cpu)
{
        ADDR15 temp = CC->IAR;
        CC->IAR = 0;
        if (dotrcins) {
                printf("******************** IRQ ********************\n");
                printf("*\tIAR=%05o\n", temp);
                printf("******************** IRQ ********************\n");
        }
        return temp;
}

extern WORD48 iohandler(WORD48);

void initiateIO(CPU *cpu)
{
        ACCESSOR acc;
        WORD48 result;

        acc.id = "IO";
        if (dotrcins)
                printf("******************** IO ********************\n");
        acc.addr = 010;
        acc.MAIL = false;
        fetch(&acc);
        acc.addr = acc.word;
        acc.MAIL = false;
        fetch(&acc);
        result = iohandler(acc.word);
        // return IO RESULT
        acc.addr = 014;
        acc.word = result;
        store(&acc);
        CC->IAR = 027;
        if (dotrcins)
                printf("******************** IO ********************\n");
}

