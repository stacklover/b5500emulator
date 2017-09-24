/***********************************************************************
* b5500emulator
************************************************************************
* Copyright (c) 2016, Reinhard Meyer, DL5UY
* Licensed under the MIT License,
*       see LICENSE
* based on (C) work by Nigel Williams and Paul Kimpel
* see: https://github.com/pkimpel/retro-b5500
************************************************************************
* misc and CPU control
************************************************************************
* 2016-02-19  R.Meyer
*   Converted Paul's work from Javascript to C
* 2017-07-17  R.Meyer
*   changed "this" to "cpu" to avoid errors when using g++
***********************************************************************/

#include <stdio.h>
#include "b5500_common.h"

#define DPRINTF if(0)printf
#define TRCMEM false

/*
 * Implements the 4441=CMN syllable
 */
void enterCharModeInline(CPU *cpu)
{
        WORD48          bw;     // local copy of B reg

        adjustAEmpty(cpu);      // flush TOS registers, but tank TOS value in A
        if (cpu->r.BROF) {
                cpu->r.A = cpu->r.B;    // tank the DI address in A
                adjustBEmpty(cpu);
        } else {
                loadAviaS(cpu); // A = [S]: load the DI address
                cpu->r.AROF = false;
        }
        cpu->r.B = buildRCW(cpu, false);
        cpu->r.BROF = true;
        adjustBEmpty(cpu);
        cpu->r.MSFF = false;
        cpu->r.SALF = true;
        cpu->r.F = cpu->r.S;
        cpu->r.R = 0;
        cpu->r.CWMF = true;
        cpu->r.X = (WORD39)cpu->r.S << SHFT_FREG; // inserting S into X.[18:15], but X is zero at this point
        cpu->r.V = 0;
        cpu->r.B = bw = cpu->r.A;

        // execute the portion of CM XX04=RDA operator starting at J=2
        cpu->r.S = bw & MASKMEM;
        if (!(bw & MASK_FLAG)) {
                // if it's an operand
                cpu->r.K = (bw >> 15) & 7; // set K from [30:3]
        } else {
                // otherwise, force K to zero and
                cpu->r.K = 0;
                // just take the side effect of any p-bit interrupt
                presenceTest(cpu, bw);
        }
}

/*
 * Initiates the processor from interrupt control words stored in the
 * stack. Assumes the INCW is in TOS. "forTest" implies use from IFT
 */
void initiate(CPU *cpu, BIT forTest)
{
        WORD48          bw;     // local copy of B
        BIT             saveAROF = 0;
        BIT             saveBROF = 0;
        unsigned        temp;
        BIT             save_dotrcmem = dotrcmem;

        DPRINTF("*\t%s: initiate forTest=%u\n", cpu->id, forTest);
        dotrcmem = TRCMEM;

        if (cpu->r.AROF) {
                cpu->r.B = bw = cpu->r.A;
        } else if (cpu->r.BROF) {
                bw = cpu->r.B;
        } else {
                adjustBFull(cpu);
                bw = cpu->r.B;
        }

        // restore the Initiate Control Word (INCW) or Initiate Test Control Word
        cpu->r.S = (bw & MASK_INCWrS) >> SHFT_INCWrS;
        cpu->r.CWMF = (bw & MASK_MODE) ? true : false;
        if (forTest) {
                cpu->r.TM = (bw & MASK_INCWrTM) >> SHFT_INCWrTM;
#if 0
                // TODO: what is happening here??
                // use bits 4-7 of TM to store 4 flags??
                // WHY not set them right here???
                (bw % 0x200000 - bw % 0x100000)/0x100000 * 16 + // NCSF
                (bw % 0x400000 - bw % 0x200000)/0x200000 * 32 + // CCCF
                (bw % 0x100000000000 - bw % 0x80000000000)/0x80000000000 * 64 + // MWOF
                (bw % 0x400000000000 - bw % 0x200000000000)/0x200000000000 * 128; // MROF
#endif
                cpu->r.Z = (bw & MASK_INCWrZ) >> SHFT_INCWrZ;
                cpu->r.Y = (bw & MASK_INCWrY) >> SHFT_INCWrY;
                cpu->r.Q01F = (bw & MASK_INCWQ01F) ? true : false;
                cpu->r.Q02F = (bw & MASK_INCWQ02F) ? true : false;
                cpu->r.Q03F = (bw & MASK_INCWQ03F) ? true : false;
                cpu->r.Q04F = (bw & MASK_INCWQ04F) ? true : false;
                cpu->r.Q05F = (bw & MASK_INCWQ05F) ? true : false;
                cpu->r.Q06F = (bw & MASK_INCWQ06F) ? true : false;
                cpu->r.Q07F = (bw & MASK_INCWQ07F) ? true : false;
                cpu->r.Q08F = (bw & MASK_INCWQ08F) ? true : false;
                cpu->r.Q09F = (bw & MASK_INCWQ09F) ? true : false;
                // Emulator doesn't support J register, so can't set that from TM
        }

        // restore the Interrupt Return Control Word (IRCW)
        DPRINTF("\tIRCW");
        loadBviaS(cpu); // B = [S]
        --cpu->r.S;
        bw = cpu->r.B;
        cpu->r.C = (bw & MASK_CREG) >> SHFT_CREG;
        cpu->r.F = (bw & MASK_FREG) >> SHFT_FREG;
        cpu->r.K = (bw & MASK_KREG) >> SHFT_KREG;
        cpu->r.G = (bw & MASK_GREG) >> SHFT_GREG;
        cpu->r.L = (bw & MASK_LREG) >> SHFT_LREG;
        cpu->r.V = (bw & MASK_VREG) >> SHFT_VREG;
        cpu->r.H = (bw & MASK_HREG) >> SHFT_HREG;
        DPRINTF("\tloadP");
        loadPviaC(cpu); // load program word to P
        if (cpu->r.CWMF || forTest) {
                saveBROF = (bw & MASK_RCWBROF) ? true : false;
        }

        // restore the Interrupt Control Word (ICW)
        DPRINTF("\tICW ");
        loadBviaS(cpu); // B = [S]
        --cpu->r.S;
        bw = cpu->r.B;
        cpu->r.VARF = (bw & MASK_VARF) ? true : false;
        cpu->r.SALF = (bw & MASK_SALF) ? true : false;
        cpu->r.MSFF = (bw & MASK_MSFF) ? true : false;
        cpu->r.R = (bw & MASK_RREG) >> SHFT_RREG;

        if (cpu->r.CWMF || forTest) {
                cpu->r.M = (bw & MASK_MREG) >> SHFT_MREG;
                cpu->r.N = (bw & MASK_NREG) >> SHFT_NREG;

                // restore the CM Interrupt Loop Control Word (ILCW)
                DPRINTF("\tILCW");
                loadBviaS(cpu); // B = [S]
                --cpu->r.S;
                bw = cpu->r.B;
                cpu->r.X = bw & MASK_MANTISSA;
                saveAROF = (bw & MASK_ILCWAROF) ? true : false;

                // restore the B register
                if (saveBROF || forTest) {
                        DPRINTF("\tload B");
                        loadBviaS(cpu); // B = [S]
                        --cpu->r.S;
                }

                // restore the A register
                if (saveAROF || forTest) {
                        DPRINTF("\tload A");
                        loadAviaS(cpu); // A = [S]
                        --cpu->r.S;
                }

                cpu->r.AROF = saveAROF;
                cpu->r.BROF = saveBROF;
                if (cpu->r.CWMF) {
                        // exchange S with its field in X
                        temp = cpu->r.S;
                        cpu->r.S = (cpu->r.X & MASK_FREG) >> SHFT_FREG;
                        cpu->r.X = (cpu->r.X & ~MASK_FREG)
                                        || ((WORD48)temp << SHFT_FREG);
                }
        } else {
                cpu->r.AROF = 0; // don't restore A or B for word mode --
                cpu->r.BROF = 0; // they will pop up as necessary
        }

        cpu->r.T = fieldIsolate(cpu->r.P, cpu->r.L*12, 12);
        cpu->r.TROF = 1;
        if (forTest) {
                cpu->r.NCSF = (cpu->r.TM >> 4) & 0x01;
                cpu->r.CCCF = (cpu->r.TM >> 5) & 0x01;
                cpu->r.MWOF = (cpu->r.TM >> 6) & 0x01;
                cpu->r.MROF = (cpu->r.TM >> 7) & 0x01;
                --cpu->r.S;
                if (!cpu->r.CCCF) {
                        cpu->r.TM |= 0x80;
                }
        } else {
                cpu->r.NCSF = 1;
        }
        dotrcmem = save_dotrcmem;
}

/*
 * Called from CentralControl to initiate the processor as P2. Fetches the
 * INCW from @10, injects an initiate P2 syllable into T, and calls start()
 */
void initiateP2(CPU *cpu)
{
        DPRINTF("*\t%s: initiateP2\n", cpu->id);
        cpu->r.NCSF = 0;        // make sure P2 is in Control State to execute the IP1 & access low mem
        cpu->r.M = 0x08;        // address of the INCW
        loadBviaM(cpu); // B = [M]
        cpu->r.AROF = 0;        // make sure A is invalid
        cpu->r.T = 04111;       // inject 4111=IP1 into P2's T register
        cpu->r.TROF = 1;
        // Now start scheduling P2 on the Javascript thread
        start(cpu);
}

/*
 * Initiates the processor
 */
void start(CPU *cpu)
{
        DPRINTF("*\t%s: start\n", cpu->id);
        cpu->busy = true;
}

/*
 * Stops running the processor
 */
void stop(CPU *cpu)
{
        DPRINTF("*\t%s: stop\n", cpu->id);
        //cpu->r.T = 0;
        //cpu->r.TROF = 0;      // idle the processor
        //cpu->r.PROF = 0;
        cpu->busy = 0;
        cpu->cycleLimit = 0;    // exit cpu->r.run()
}

void haltP2(CPU *cpu)
{
        DPRINTF("*\t%s: haltP2\n", cpu->id);
}

/*
 * Presets the processor registers for a load condition at C=runAddr
 */
void preset(CPU *cpu, ADDR15 runAddr)
{
        DPRINTF("*\t%s: preset %05o\n", cpu->id, runAddr);

        cpu->r.C = runAddr;     // starting execution address
        cpu->r.L = 1;           // preset L to point to the second syllable
        loadPviaC(cpu);         // load the program word to P
        cpu->r.T = (cpu->r.P >> 36) & 07777;
        cpu->r.TROF = 1;
        cpu->r.R = 0;
        cpu->r.S = 0;
}

/*
 * Instruction execution driver for the B5500 processor. This function is
 * an artifact of the emulator design and does not represent any physical
 * process or state of the processor. This routine assumes the registers are
 * set up -- in particular there must be a syllable in T with TROF set, the
 * current program word must be in P with PROF set, and the C & L registers
 * must point to the next syllable to be executed.
 * This routine will continue to run while cpu->r.runCycles < cpu->r.cycleLimit
 */
void run(CPU *cpu)
{
        cpu->runCycles = 0;     // initialze the cycle counter for cpu time slice
        do {
                cpu->cycleCount = 1;    // general syllable execution overhead

                if (cpu->r.CWMF) {
                        b5500_execute_cm(cpu);
                } else {
                        b5500_execute_wm(cpu);
                }

/***************************************************************
*   SECL: Syllable Execution Complete Level                    *
***************************************************************/

                // is there an interrupt
                if (cpu->r.NCSF && (cpu->isP1 ?
                        CC->IAR :
                        (cpu->r.I || CC->HP2F))) {
                        // there's an interrupt and we're in Normal State
                        // reset Q09F (R-relative adder mode) and
                        // set Q07F (hardware-induced SFI) (for display only)
                        cpu->r.Q09F = false;
                        cpu->r.Q07F = true;
                        cpu->r.T = 03011; // inject 3011=SFI into T
                        // call directly to avoid resetting registers at top
                        // of loop
                        storeForInterrupt(cpu, true, false, "atSECL");
                } else {
                        // otherwise, fetch the next instruction
                        if (!cpu->r.PROF) {
                                loadPviaC(cpu);
                        }
                        switch (cpu->r.L) {
                        case 0:
                                cpu->r.T = (cpu->r.P >> 36) & 0xfff;
                                cpu->r.L = 1;
                                break;
                        case 1:
                                cpu->r.T = (cpu->r.P >> 24) & 0xfff;
                                cpu->r.L = 2;
                                break;
                        case 2:
                                cpu->r.T = (cpu->r.P >> 12) & 0xfff;
                                cpu->r.L = 3;
                                break;
                        case 3:
                                cpu->r.T = cpu->r.P & 0xfff;
                                cpu->r.L = 0;
                                // invalidate current program word
                                cpu->r.PROF = 0;
                                // assume no Inhibit Fetch for now and bump C
                                if (++cpu->r.C > 077777) {
                                        DPRINTF("C reached end of memory\n");
                                        cpu->r.C = 0;
                                        stop(cpu);
                                }
                                break;
                        }
//printf("SECL: C=%05o L=%o T=%04o TROF=%u P=%016llo PROF=%u\n",
//      cpu->r.C, cpu->r.L, cpu->r.T, cpu->r.TROF,
//      cpu->r.P, cpu->r.PROF);

                }

        // Accumulate Normal and Control State cycles for use by Console in
        // making the pretty lights blink. If the processor is no longer busy,
        // accumulate the cycles as Normal State, as we probably just did SFI.
                if (cpu->r.NCSF || !cpu->busy) {
                        cpu->normalCycles += cpu->cycleCount;
                } else {
                        cpu->controlCycles += cpu->cycleCount;
                }
        } while ((cpu->runCycles += cpu->cycleCount) < cpu->cycleLimit);
}
