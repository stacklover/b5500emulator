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
* 2017-09-30  R.Meyer
*   overhaul of file names
***********************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "common.h"

#define DPRINTF if(0)printf
#define TRCMEM false

/*
 * Tests and returns the presence bit [2:1] of the "word" parameter,
 * which it assumes is a control word. If [2:1] is 0, the p-bit interrupt
 * is set; otherwise no further action
 */
BIT presenceTest(CPU *cpu, WORD48 word)
{
        if (word & MASK_PBIT)
                return true;

#if DEBUG
        DPRINTF("*\t%s: presenceTest failed %016llo\n", cpu->id, word);
#endif
        if (cpu->r.NCSF) {
                cpu->r.I = (cpu->r.I & 0x0F) | 0x70; // set I05/6/7: p-bit
                signalInterrupt(cpu->id, "NOT PBIT");
        }
        return false;
}

/*
 * Implements the 3011=SFI operator and the parts of 3411=SFT that are
 * common to it. "forced" implies Q07F: a hardware-induced SFI syllable.
 * "forTest" implies use from SFT
 */
void storeForInterrupt(CPU *cpu, BIT forced, BIT forTest, const char *cause)
{
        BIT             saveAROF = cpu->r.AROF;
        BIT             saveBROF = cpu->r.BROF;
        unsigned        temp;
        BIT             save_dotrcmem = dotrcmem;

#if DEBUG
        DPRINTF("*\t%s: %s storeForInterrupt forced=%u forTest=%u\n", cpu->id, cause, forced, forTest);
#endif
        dotrcmem = TRCMEM;

        if (forced || forTest) {
                cpu->r.NCSF = 0; // switch to Control State
        }

        if (cpu->r.CWMF) {
                // in Character Mode, get the correct TOS address from X
                // exchange S with F-field of X
                temp = cpu->r.S;
                cpu->r.S = (cpu->r.X & MASK_FREG) >> SHFT_FREG;
                cpu->r.X = (cpu->r.X & ~MASK_FREG) | (temp << SHFT_FREG);

                if (saveAROF || forTest) {
                        ++cpu->r.S;
                        DPRINTF("\tsave A");
                        storeAviaS(cpu); // [S] = A
                }

                if (saveBROF || forTest) {
                        ++cpu->r.S;
                        DPRINTF("\tsave B");
                        storeBviaS(cpu); // [S] = B
                }

                // store Character Mode Interrupt Loop-Control Word (ILCW)
                // 444444443333333333222222222211111111110000000000
                // 765432109876543210987654321098765432109876543210
                // 11A000000XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
                cpu->r.B = INIT_ILCW | cpu->r.X;
                if (saveAROF)
                        cpu->r.B |= MASK_ILCWAROF;
                ++cpu->r.S;
                DPRINTF("\tILCW");
                storeBviaS(cpu); // [S] = ILCW
        } else {
                // in word mode, save B and A if not empty
                if (saveBROF || forTest) {
                        ++cpu->r.S;
                        DPRINTF("\tsave B");
                        storeBviaS(cpu); // [S] = B
                }

                if (saveAROF || forTest) {
                        ++cpu->r.S;
                        DPRINTF("\tsave A");
                        storeAviaS(cpu); // [S] = A
                }
        }

        // store Interrupt Control Word (ICW)
        // 444444443333333333222222222211111111110000000000
        // 765432109876543210987654321098765432109876543210
        // 110000RRRRRRRRR0MS00000V00000NNNNMMMMMMMMMMMMMMM
        cpu->r.B = INIT_ICW |
                ((WORD48)cpu->r.M << SHFT_MREG) |
                ((WORD48)cpu->r.N << SHFT_NREG) |
                ((WORD48)cpu->r.R << SHFT_RREG);
        if (cpu->r.VARF)
                cpu->r.B |= MASK_VARF;
        if (cpu->r.SALF)
                cpu->r.B |= MASK_SALF;
        if (cpu->r.MSFF)
                cpu->r.B |= MASK_MSFF;
        ++cpu->r.S;
        DPRINTF("\tICW ");
        storeBviaS(cpu); // [S] = ICW

        // store Interrupt Return Control Word (IRCW)
        // 444444443333333333222222222211111111110000000000
        // 765432109876543210987654321098765432109876543210
        // 11B0HHHVVVLLGGGKKKFFFFFFFFFFFFFFFCCCCCCCCCCCCCCC
        cpu->r.B = INIT_RCW |
                ((WORD48)cpu->r.C << SHFT_CREG) |
                ((WORD48)cpu->r.F << SHFT_FREG) |
                ((WORD48)cpu->r.K << SHFT_KREG) |
                ((WORD48)cpu->r.G << SHFT_GREG) |
                ((WORD48)cpu->r.L << SHFT_LREG) |
                ((WORD48)cpu->r.V << SHFT_VREG) |
                ((WORD48)cpu->r.H << SHFT_HREG);
        if (saveBROF)
                cpu->r.B |= MASK_RCWBROF;
        ++cpu->r.S;
        DPRINTF("\tIRCW");
        storeBviaS(cpu); // [S] = IRCW

        if (cpu->r.CWMF) {
                // if CM, get correct R value from last MSCW
                // exchange S with F
                temp = cpu->r.F;
                cpu->r.F = cpu->r.S;
                cpu->r.S = temp;

                DPRINTF("\tlast RCW ");
                loadBviaS(cpu); // B = [S]: get last RCW
                cpu->r.S = (cpu->r.B & MASK_FREG) >> SHFT_FREG;

                DPRINTF("\tlast MSCW");
                loadBviaS(cpu); // B = [S]: get last MSCW
                cpu->r.R = (cpu->r.B & MASK_RREG) >> SHFT_RREG;
                cpu->r.S = cpu->r.F;
        }

        // build the Initiate Control Word (INCW)
        // 444444443333333333222222222211111111110000000000
        // 765432109876543210987654321098765432109876543210
        // 11000QQQQQQQQQYYYYYYZZZZZZ0TTTTTCSSSSSSSSSSSSSSS
        cpu->r.B = INIT_INCW |
                ((WORD48)cpu->r.S << SHFT_INCWrS) |
                (((WORD48)cpu->r.TM << SHFT_INCWrTM) & MASK_INCWrTM) |
                ((WORD48)cpu->r.Z << SHFT_INCWrZ) |
                ((WORD48)cpu->r.Y << SHFT_INCWrY);
        if (cpu->r.CWMF)
                cpu->r.B |= MASK_INCWMODE;
        if (cpu->r.Q01F)
                cpu->r.B |= MASK_INCWQ01F;
        if (cpu->r.Q02F)
                cpu->r.B |= MASK_INCWQ02F;
        if (cpu->r.Q03F)
                cpu->r.B |= MASK_INCWQ03F;
        if (cpu->r.Q04F)
                cpu->r.B |= MASK_INCWQ04F;
        if (cpu->r.Q05F)
                cpu->r.B |= MASK_INCWQ05F;
        if (cpu->r.Q06F)
                cpu->r.B |= MASK_INCWQ06F;
        if (cpu->r.Q07F)
                cpu->r.B |= MASK_INCWQ07F;
        if (cpu->r.Q08F)
                cpu->r.B |= MASK_INCWQ08F;
        if (cpu->r.Q09F)
                cpu->r.B |= MASK_INCWQ09F;
        cpu->r.M = (cpu->r.R<<6) + 8; // store initiate word at R+@10
        DPRINTF("\tINCW");
        storeBviaM(cpu); // [M] = INCW

        cpu->r.M = 0;
        cpu->r.R = 0;
        cpu->r.MSFF = false;
        cpu->r.SALF = false;
        cpu->r.BROF = false;
        cpu->r.AROF = false;
        if (forTest) {
                cpu->r.TM = 0;
                cpu->r.MROF = false;
                cpu->r.MWOF = false;
        }

        if (forced || forTest) {
                cpu->r.CWMF = false;
        }

        if (cpu->isP1) {
                // if it's P1
                if (forTest) {
                        DPRINTF("\tload DD");
                        loadBviaM(cpu); // B = [M]: load DD for test
                        cpu->r.C = (cpu->r.B & MASK_CREG) >> SHFT_CREG;
                        cpu->r.L = 0;
                        cpu->r.PROF = false; // require fetch at SECL
                        cpu->r.G = 0;
                        cpu->r.H = 0;
                        cpu->r.K = 0;
                        cpu->r.V = 0;
                } else {
                        cpu->r.T = 0211; // inject 0211=ITI into P1's T register
                }
        } else {
                // if it's P2
                stop(cpu); // idle the P2 processor
                CC->P2BF = false; // tell CC and P1 we've stopped
        }
        dotrcmem = save_dotrcmem;
}

