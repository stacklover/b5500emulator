/***********************************************************************
* b5500emulator
************************************************************************
* Copyright (c) 2016, Reinhard Meyer, DL5UY
* Licensed under the MIT License,
*       see LICENSE
* based on (C) work by Nigel Williams and Paul Kimpel
* see: https://github.com/pkimpel/retro-b5500
************************************************************************
* IRQ and I/O
************************************************************************
* 2016-02-21    R.Meyer
*   Converted Paul's work from Javascript to C
* 2017-07-17  R.Meyer
*   changed "this" to "cpu" to avoid errors when using g++
***********************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "b5500_common.h"

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
        printf("******************** TUS ********************\n");
        sleep(1);
        return 0; //x0000ffffffffll;
}

WORD48 interrogateIOChannel(CPU *cpu)
{
        printf("******************** TIO ********************\n");
        sleep(1);
        return 1;
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
        printf("******************** IRQ ********************\n");
        printf("*\tIAR=%05o\n", temp);
        printf("******************** IRQ ********************\n");
        return temp;
}

extern WORD48 iohandler(WORD48);

void initiateIO(CPU *cpu)
{
        ACCESSOR acc;
        WORD48 result;

        acc.id = "IO";
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
        printf("******************** IO ********************\n");
}

