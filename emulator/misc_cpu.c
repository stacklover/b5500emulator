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
* 2017-09-30  R.Meyer
*   overhaul of file names
***********************************************************************/

#include <stdio.h>
#include "common.h"

#define DPRINTF if(0)printf
#define TRCMEM false

/***********************************************************************
* Prepare a debug message
* message must be completed and ended by caller
***********************************************************************/
void prepMessage(CPU *cpu) {
	printf("*\t%s at %05o:%o: ",
		cpu->id,
		cpu->r.L == 0 ? cpu->r.C-1 : cpu->r.C,
		(cpu->r.L - 1) & 3);
}

/***********************************************************************
* Cause a memory access based IRQ
***********************************************************************/
void causeMemoryIrq(CPU *cpu, WORD8 irq, const char *reason) {
	cpu->r.I |= irq;
	signalInterrupt(cpu->id, reason);
#if 1
	prepMessage(cpu);
	printf("IRQ %02x caused reason %s (I now %02x)\n",
		irq, reason, cpu->r.I);
#endif
}

/***********************************************************************
* Cause a syllable based IRQ
***********************************************************************/
void causeSyllableIrq(CPU *cpu, WORD8 irq, const char *reason) {
	cpu->r.I = (cpu->r.I & IRQ_MASKL) | irq;
	signalInterrupt(cpu->id, reason);
#if 1
	prepMessage(cpu);
	printf("IRQ %02x caused reason %s (I now %02x)\n",
		irq, reason, cpu->r.I);
#endif
}

/***********************************************************************
* Computes an absolute memory address from the relative "offset" parameter
* and leaves it in the M register. See Table 6-1 in the B5500 Reference
* Manual. "cEnable" determines whether C-relative addressing is permitted.
* VERIFIED against Paul's JavaScript 17-10-16
***********************************************************************/
void computeRelativeAddr(CPU *cpu, unsigned offset, BIT cEnabled)
{
	// This offset must be in (0..1023)
	if (offset > 01777) {
		prepMessage(cpu);
		printf("computeRelativeAddr offset(%04o) >01777\n", offset);
		stop(cpu);
	}

        if (dotrcmem)
                printf("\tRelAddr(%04o,%u) -> ", offset, cEnabled);

        cpu->cycleCount += 2; // approximate the timing

        if (cpu->r.SALF) {
                // subroutine level - check upper 3 bits of the 10 bit offset
                switch ((offset >> 7) & 7) {
                case 0: case 1: case 2: case 3:
                        // pattern 0 xxx xxx xxx - R+ relative
                        // reach 0..511
                        offset &= 0777;
                        cpu->r.M = (cpu->r.R<<RSHIFT) + offset;
                        if (dotrcmem)
                                printf("R+%o -> M=%05o\n", offset, cpu->r.M);
                        break;
                case 4: case 5:
                        // pattern 1 0xx xxx xxx - F+ or (R+7)+ relative
                        // reach 0..255
                        offset &= 0377;
                        if (cpu->r.MSFF) {
                                // during function parameter loading its (R+7)+
                                cpu->r.M = (cpu->r.R<<RSHIFT) + RR_MSCW;
                                loadMviaM(cpu); // M = [M].[18:15]
                                cpu->r.M += offset;
                                if (dotrcmem)
                                        printf("(R+7)+%o -> M=%05o\n", offset, cpu->r.M);
                        } else {
                                // inside function its F+
                                cpu->r.M = cpu->r.F + offset;
                                if (dotrcmem)
                                        printf("F+%o -> M=%05o\n", offset, cpu->r.M);
                        }
                        break;
                case 6:
                        // pattern 1 10x xxx xxx - C+ relative on read, R+ on write
                        // reach 0..127
                        offset &= 0177;
                        if (cEnabled) {
                                // adjust C for fetch offset from
                                // syllable the instruction was in
                                cpu->r.M = (cpu->r.L > 0 ? cpu->r.C : cpu->r.C-1) + offset;
                                if (dotrcmem)
                                        printf("C+%o -> M=%05o\n", offset, cpu->r.M);
                        } else {
                                cpu->r.M = (cpu->r.R<<RSHIFT) + offset;
                                if (dotrcmem)
                                        printf("R+%o -> M=%05o\n", offset, cpu->r.M);
                        }
                        break;
                case 7:
                        // pattern 1 11x xxx xxx - F- or (R+7)- relative
                        // reach 0..127 (negative direction)
                        offset &= 0177;
                        if (cpu->r.MSFF) {
                                // during function parameter loading its (R+7)-
                                cpu->r.M = (cpu->r.R<<RSHIFT) + RR_MSCW;
                                loadMviaM(cpu); // M = [M].[18:15]
                                cpu->r.M -= offset;
                                if (dotrcmem)
                                        printf("(R+7)-%o -> M=%05o\n", offset, cpu->r.M);
                        } else {
                                // inside function its F-
                                cpu->r.M = cpu->r.F - offset;
                                if (dotrcmem)
                                        printf("F-%o -> M=%05o\n", offset, cpu->r.M);
                        }
                        break;
                } // switch
        } else {
                // program level - all 10 bits are offset
                offset &= 001777; // redundant
                cpu->r.M = (cpu->r.R<<RSHIFT) + offset;
                if (dotrcmem)
                        printf("R+%o,M=%05o\n", offset, cpu->r.M);
        }

        // Reset variant-mode R-relative addressing, if it was enabled
        if (cpu->r.VARF) {
                if (dotrcmem)
                        printf("Resetting VARF\n");
                cpu->r.SALF = true;
                cpu->r.VARF = false;
        }
}

/***********************************************************************
* Test the presence bit [2:1] of the "word" parameter.
*
* if SET: returns true
*
* if RESET and in NORMAL STATE:
* optionally a warning is printed
* causes presence bit IRQ and returns false
*
* if RESET and in CONTROL STATE:
* an error message is printed
* not a good idea: stops the cpu and returns false
* VERIFIED against Paul's JavaScript 17-10-16
***********************************************************************/
BIT presenceTest(CPU *cpu, WORD48 word)
{
	if (word & MASK_PBIT)
		return true;

	if (cpu->r.NCSF) {
		// NORMAL STATE
#if 1
		prepMessage(cpu);
		printf("presenceTest failed (%016llo)\n", word);
#endif
		causeSyllableIrq(cpu, IRQ_PBIT, "PBIT=0");
		return false;
	}
	// CONTROL STATE
	prepMessage(cpu);
	printf("presenceTest failed (%016llo) in CONTROL STATE\n", word);
	//stop(cpu);
	return false;
}

/***********************************************************************
* OPDC, the moral equivalent of "load accumulator" on lesser
* machines. Assumes the syllable has already loaded a word into A.
* See Figures 6-1, 6-3, and 6-4 in the B5500 Reference Manual
* VERIFIED against Paul's JavaScript 17-10-16
***********************************************************************/
void operandCall(CPU *cpu)
{
	BIT interrupted = false; // interrupt occurred

	// If A contains a simple operand, just leave it there
	if (OPERAND(cpu->r.A))
		return;

	// It's not a simple operand
	switch ((cpu->r.A & MASK_TYPE) >> SHFT_TYPE) { // A.[1:3]

	case 2: // CODE=0, PBIT=1, XBIT=0
	case 3: // CODE=0, PBIT=1, XBIT=1
		// Present data descriptor: see if it must be indexed
		if (cpu->r.A & MASK_WCNT) { // A.[8:10]
			// TODO: indexDescriptor ensures AB full (again) and implicitly uses A
			interrupted = indexDescriptor(cpu);
			// else descriptor is already indexed (word count 0)
		}
		if (!interrupted) {
			// indexing reported no issue
			cpu->r.M = cpu->r.A & MASKMEM;
			// get the value, which now should be an operand
			loadAviaM(cpu); // A = [M]
			if (DESCRIPTOR(cpu->r.A) && cpu->r.NCSF) {
				// Flag bit is set and NORMAL state
			        causeSyllableIrq(cpu, IRQ_FLAG, "operandCall FLAG SET");
			}
		}
		break;

	case 7: // CODE=1, PBIT=1, XBIT=1
		// Present program descriptor
		enterSubroutine(cpu, false);
		break;

	case 0: // CODE=0, PBIT=0, XBIT=0
	case 1: // CODE=0, PBIT=0, XBIT=1
	case 5: // CODE=1, PBIT=0, XBIT=1
		// Absent data or program descriptor
		if (cpu->r.NCSF) {
			causeSyllableIrq(cpu, IRQ_PBIT, "operandCall NOT PBIT");
		}
		// else if Control State, we're done
		break;

	default: // cases 4, 6  // CODE=1, PBIT=0/1, XBIT=0
		// Miscellaneous control word -- leave as is
		break;
	}
}

/***********************************************************************
* DESC, the moral equivalent of "load address" on lesser machines.
* Assumes the syllable has already loaded a word into A, and that the
* address of that word is in M.
* See Figures 6-2, 6-3, and 6-4 in the B5500 Reference Manual
* VERIFIED against Paul's JavaScript 17-10-16
***********************************************************************/
void descriptorCall(CPU *cpu)
{
	BIT interrupted = false; // interrupt occurred

	if (OPERAND(cpu->r.A)) {
		// It's a simple operand
		// create a present data descriptor to its address (is in M)
		cpu->r.A = cpu->r.M | MASK_FLAG | MASK_PBIT;
		return;
	}

	// It's not a simple operand
	switch ((cpu->r.A & MASK_TYPE) >> SHFT_TYPE) { // A.[1:3]

	case 2: // CODE=0, PBIT=1, XBIT=0
	case 3: // CODE=0, PBIT=1, XBIT=1
	        // Present data descriptor: see if it must be indexed
	        if (cpu->r.A & MASK_WCNT) { // aw.[8:10]
			// TODO: indexDescriptor ensures AB full (again) and implicitly uses A
	                interrupted = indexDescriptor(cpu);
	                if (!interrupted) {
	                        // set word count to zero
	                        cpu->r.A &= ~MASK_WCNT;
	                }
	                // else descriptor is already indexed (word count 0)
	        }
	        break;

	case 7: // CODE=1, PBIT=1, XBIT=1
	        // Present program descriptor
	        enterSubroutine(cpu, true);
	        break;

	case 0: // CODE=0, PBIT=0, XBIT=0
	case 1: // CODE=0, PBIT=0, XBIT=1
	case 5: // CODE=1, PBIT=0, XBIT=1
	        // Absent data or program descriptor
		if (cpu->r.NCSF) {
			causeSyllableIrq(cpu, IRQ_PBIT, "descriptorCall NOT PBIT");
		}
                // else if Control State, we're done
	        break;

	default: // cases 4, 6  // CODE=1, PBIT=0/1, XBIT=0
	        // Miscellaneous control word
		// create a present data descriptor to its address (is in M)
	        cpu->r.A = cpu->r.M | MASK_FLAG | MASK_PBIT;
	        break;
	}
}

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
        cpu->r.CWMF = (bw & MASK_INCWMODE) ? true : false;
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

/*
 * Called from CC initiate this(cpu) processor as P2. Fetches the
 * INCW from @10, injects an initiate syllable into T, and calls start()
 */
void initiateAsP2(CPU *cpu)
{
	printf("*\t%s: initiateAsP2\n", cpu->id);
	cpu->r.NCSF = 0;	// make P2 is in Control State to execute the IP1 & access low mem
	cpu->r.M = AA_IODESC;	// address of the INCW
	loadBviaM(cpu);		// B = [M]
	cpu->r.AROF = 0;	// make sure A is invalid
	cpu->r.T = 04111;	// inject 4111=IP1 into P2's T register
	cpu->r.TROF = 1;
	start(cpu);
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
                        // the following is for display only, since we ...
                        cpu->r.T = 03011; // inject 3011=SFI into T
                        // ... call directly to avoid resetting registers at top
                        // of loop. And SFI will inject a 0211 (ITI)
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
