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

#define	TRACE_IRQ 0

#include <stdio.h>
#include "common.h"

#define DPRINTF if(0)printf
#define TRCMEM false

/***********************************************************************
* Prepare a debug message
* message must be completed and ended by caller
***********************************************************************/
void prepMessage(CPU *cpu) {
	if (cpu->r.C != 0)
		printf("*\t%s at %05o:%o ",
			cpu->id,
			cpu->r.L == 0 ? cpu->r.C-1 : cpu->r.C,
			(cpu->r.L - 1) & 3);
	else
		printf("*\t%s at xxxxx:x ", cpu->id);
}

/***********************************************************************
* Cause a memory access based IRQ
***********************************************************************/
void causeMemoryIrq(CPU *cpu, WORD8 irq, const char *reason) {
	cpu->r.I |= irq;
	signalInterrupt(cpu->id, reason);
#if TRACE_IRQ
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
#if TRACE_IRQ
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

#if 0
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
#endif

#if 0
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
#endif

/***********************************************************************
* Implements the 3011=SFI operator and the parts of 3411=SFT that are
* common to it
* "forced" implies Q07F: a hardware-induced SFI syllable
* "forTest" implies use from SFT
* VERIFIED against Paul's JavaScript 17-10-22
***********************************************************************/
void storeForInterrupt(CPU *cpu, BIT forced, BIT forTest, const char *cause)
{
	BIT		saveAROF = cpu->r.AROF;
	BIT		saveBROF = cpu->r.BROF;
	ADDR15		temp;
	BIT		save_dotrcmem = dotrcmem;

#if DEBUG
        DPRINTF("*\t%s: %s storeForInterrupt forced=%u forTest=%u\n", cpu->id, cause, forced, forTest);
#endif
        dotrcmem = TRCMEM;

        if (forced || forTest) {
                cpu->r.NCSF = false; // switch to Control State
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
                (((WORD48)cpu->r.M) << SHFT_MREG) |
                (((WORD48)cpu->r.N) << SHFT_NREG) |
                (((WORD48)cpu->r.R) << SHFT_RREG);
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
                (((WORD48)cpu->r.C) << SHFT_CREG) |
                (((WORD48)cpu->r.F) << SHFT_FREG) |
                (((WORD48)cpu->r.K) << SHFT_KREG) |
                (((WORD48)cpu->r.G) << SHFT_GREG) |
                (((WORD48)cpu->r.L) << SHFT_LREG) |
                (((WORD48)cpu->r.V) << SHFT_VREG) |
                (((WORD48)cpu->r.H) << SHFT_HREG);
        if (saveBROF)
                cpu->r.B |= MASK_RCWBROF;
        ++cpu->r.S;
        DPRINTF("\tIRCW");
        storeBviaS(cpu); // [S] = IRCW

	// in order to store the Initiate Control Word at [R+8]
	// and if we are in character mode
	// we need to get the true R value from the last MSCW
        if (cpu->r.CWMF) {
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
                (((WORD48)cpu->r.S) << SHFT_INCWrS) |
                ((((WORD48)cpu->r.TM) << SHFT_INCWrTM) & MASK_INCWrTM) |
                (((WORD48)cpu->r.Z) << SHFT_INCWrZ) |
                (((WORD48)cpu->r.Y) << SHFT_INCWrY);
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
        cpu->r.M = (cpu->r.R<<RSHIFT) + RR_INCW; // store initiate word at R+@10
        DPRINTF("\tINCW");
        storeBviaM(cpu); // [M] = INCW

	// clean some registers
        cpu->r.M = 0;
        cpu->r.R = 0;
        cpu->r.MSFF = false;
        cpu->r.SALF = false;
        cpu->r.BROF = false;
        cpu->r.AROF = false;
        if (forTest) {
		prepMessage(cpu); printf("storeForInterrupt(forTest=true)\n");
                cpu->r.TM = 0;
                cpu->r.MROF = false;
                cpu->r.MWOF = false;
        }

	// if SECL detected IRQ or TEST, ensure word more
        if (forced || forTest) {
                cpu->r.CWMF = false;
        }

	// are we P1?
        if (cpu->isP1) {
                // we are P1
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
                // we are P2
                stop(cpu); // idle the P2 processor
                CC->P2BF = false; // tell CC and P1 we've stopped
        }
        dotrcmem = save_dotrcmem;
}

/***********************************************************************
* Implements the 4441=CMN syllable
* VERIFIED against Paul's JavaScript 17-10-22
***********************************************************************/
void enterCharModeInline(CPU *cpu)
{
	WORD48          bw;     // local copy of B reg TODO: really required?

	// flush both A and B to stack, but get TOS value in A without AROF being set:
	// make sure A is empty
	// if B is full, copy B value to A (AROF stays reset) and flush B to stack
	// if B is empty, load A fromstack and reset AROF
	// a now holds the DI
	adjustAEmpty(cpu);
	if (cpu->r.BROF) {
		cpu->r.A = cpu->r.B;
		adjustBEmpty(cpu);
	} else {
		loadAviaS(cpu);
		cpu->r.AROF = false;
	}

	// build a RCW(OPDC) and push it onto the stack
	cpu->r.B = buildRCW(cpu, false);
	cpu->r.BROF = true;
	adjustBEmpty(cpu);

	// reset and set flags to show we are in subroutine mode, RCW is last
	cpu->r.MSFF = false;
	cpu->r.SALF = true;

	// save S in F for character mode
	cpu->r.F = cpu->r.S;

	// clear tally
	cpu->r.R = 0;

	// now in character mode
	cpu->r.CWMF = true;

	// create first loop control word
	// insert S into X.[18:15], but X is zero at this point	
	cpu->r.X = ((WORD39)cpu->r.S) << SHFT_FREG;

	// clear V
	cpu->r.V = 0;

	// save DI in bw and also to B
	cpu->r.B = bw = cpu->r.A;

	// execute the portion of CM XX04=RDA operator starting at J=2
	// S = DI
	cpu->r.S = bw & MASKMEM;

	// if DI is an operand
	if (OPERAND(bw)) {
		// it's an operand - set K from F field
		cpu->r.K = (bw >> SHFT_FREG) & 7;
	} else {
		// otherwise, force K to zero and
		cpu->r.K = 0;
		// just take the side effect of any p-bit interrupt
		presenceTest(cpu, bw);
	}
}

/***********************************************************************
* Initiates the processor from interrupt control words stored in the stack
* Assumes the INCW is in TOS
* "forTest" implies use from IFT
* VERIFIED against Paul's JavaScript 17-10-22
***********************************************************************/
void initiate(CPU *cpu, BIT forTest)
{
        WORD48          bw;     // local copy of B reg TODO: really required?
        BIT             saveAROF = false;
        BIT             saveBROF = false;
        BIT             save_dotrcmem = dotrcmem;

        DPRINTF("*\t%s: initiate forTest=%u\n", cpu->id, forTest);
        dotrcmem = TRCMEM;

	// make sure A is empty and TOS is in B (and bw)
	// after this AROF and BROF do not matter
        if (cpu->r.AROF) {
                cpu->r.B = bw = cpu->r.A;
        } else if (cpu->r.BROF) {
                bw = cpu->r.B;
        } else {
                adjustBFull(cpu);
                bw = cpu->r.B;
        }

        // restore the Initiate Control Word (INCW) in B (or bw) to the registers
        // 444444443333333333222222222211111111110000000000
        // 765432109876543210987654321098765432109876543210
        // 11000QQQQQQQQQYYYYYYZZZZZZ0TTTTTCSSSSSSSSSSSSSSS
        cpu->r.S = (bw & MASK_INCWrS) >> SHFT_INCWrS;
        cpu->r.CWMF = (bw & MASK_INCWMODE) ? true : false;

        if (forTest) {
		// TODO: what exactly is supposed to happen here?
		prepMessage(cpu); printf("initiate(forTest=true)\n");
		// TM holds MROF MWOF CCCF NCSF J J J J
                cpu->r.TM = (bw & MASK_INCWrTM) >> SHFT_INCWrTM;
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

        // get the Interrupt Return Control Word (IRCW) from stack to B (and bw)
        DPRINTF("\tIRCW");
        loadBviaS(cpu); // B = [S]
        --cpu->r.S;
        bw = cpu->r.B;

        // restore the Interrupt Return Control Word (IRCW) in B (or bw) to the registers
        // 444444443333333333222222222211111111110000000000
        // 765432109876543210987654321098765432109876543210
        // 11B0HHHVVVLLGGGKKKFFFFFFFFFFFFFFFCCCCCCCCCCCCCCC
        cpu->r.C = (bw & MASK_CREG) >> SHFT_CREG;
        cpu->r.F = (bw & MASK_FREG) >> SHFT_FREG;
        cpu->r.K = (bw & MASK_KREG) >> SHFT_KREG;
        cpu->r.G = (bw & MASK_GREG) >> SHFT_GREG;
        cpu->r.L = (bw & MASK_LREG) >> SHFT_LREG;
        cpu->r.V = (bw & MASK_VREG) >> SHFT_VREG;
        cpu->r.H = (bw & MASK_HREG) >> SHFT_HREG;

	// load now addressed program word to P
        DPRINTF("\tloadP");
        loadPviaC(cpu); // load program word to P

	// restore BROF if character mode or test
        if (cpu->r.CWMF || forTest) {
                saveBROF = (bw & MASK_RCWBROF) ? true : false;
        }

        // get the Interrupt Control Word (ICW) from stack to B (and bw)
        DPRINTF("\tICW ");
        loadBviaS(cpu); // B = [S]
        --cpu->r.S;
        bw = cpu->r.B;

        // restore the Interrupt Control Word (ICW) in B (or bw) to the registers
        // 444444443333333333222222222211111111110000000000
        // 765432109876543210987654321098765432109876543210
        // 110000RRRRRRRRR0MS00000V00000NNNNMMMMMMMMMMMMMMM
        cpu->r.VARF = (bw & MASK_VARF) ? true : false;
        cpu->r.SALF = (bw & MASK_SALF) ? true : false;
        cpu->r.MSFF = (bw & MASK_MSFF) ? true : false;
        cpu->r.R = (bw & MASK_RREG) >> SHFT_RREG;

	// restore M, N, LOOP, B and A if character mode or test
        if (cpu->r.CWMF || forTest) {
                cpu->r.M = (bw & MASK_MREG) >> SHFT_MREG;
                cpu->r.N = (bw & MASK_NREG) >> SHFT_NREG;

	        // get the Interrupt Loop Control Word (ILCW) from stack to B (and bw)
                DPRINTF("\tILCW");
                loadBviaS(cpu); // B = [S]
                --cpu->r.S;
                bw = cpu->r.B;

                // restore the Interrupt Loop Control Word (ILCW) in B (or bw) to the registers
                // 444444443333333333222222222211111111110000000000
                // 765432109876543210987654321098765432109876543210
                // 11A000000XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
                cpu->r.X = bw & MASK_MANTISSA;
                saveAROF = (bw & MASK_ILCWAROF) ? true : false;

                // restore the B register if it was occupied or test
                if (saveBROF || forTest) {
                        DPRINTF("\tload B");
                        loadBviaS(cpu); // B = [S]
                        --cpu->r.S;
                }

                // restore the A register if it was occupied or test
                if (saveAROF || forTest) {
                        DPRINTF("\tload A");
                        loadAviaS(cpu); // A = [S]
                        --cpu->r.S;
                }

		// save AROF and BROF to the real flip-flops
                cpu->r.AROF = saveAROF;
                cpu->r.BROF = saveBROF;

		// in charcater mode exchange S with the F(!) field in X
                if (cpu->r.CWMF) {
                        ADDR15 temp = cpu->r.S;
                        cpu->r.S = (cpu->r.X & MASK_FREG) >> SHFT_FREG;
                        cpu->r.X = (cpu->r.X & ~MASK_FREG) || (temp << SHFT_FREG);
                }
        } else {
		// in word mode and not test, do not restore A and B
		// they will pop up as necessary
                cpu->r.AROF = 0;
                cpu->r.BROF = 0;
        }

	// load the first instruction into T
        cpu->r.T = fieldIsolate(cpu->r.P, cpu->r.L*12, 12);
        cpu->r.TROF = 1;

	// in test, set some flags from the INCW (saved earlier in TM)
        if (forTest) {
                cpu->r.NCSF = (cpu->r.TM >> 4) & 1;
                cpu->r.CCCF = (cpu->r.TM >> 5) & 1;
                cpu->r.MWOF = (cpu->r.TM >> 6) & 1;
                cpu->r.MROF = (cpu->r.TM >> 7) & 1;
		// decrement stack? TODO: why?
                --cpu->r.S;
		// TODO: and what does this?
                if (!cpu->r.CCCF) {
                        cpu->r.TM |= 0x80;
                }
        } else {
		// otherwise make sure we are in normal state
                cpu->r.NCSF = 1;
        }

        dotrcmem = save_dotrcmem;
}

/***********************************************************************
* Initiates the processor
* VERIFIED against Paul's JavaScript 17-10-22
***********************************************************************/
void start(CPU *cpu)
{
	prepMessage(cpu); printf("start\n");
	cpu->busy = true;
}

/***********************************************************************
* Stops running the processor
* VERIFIED against Paul's JavaScript 17-10-22
***********************************************************************/
void stop(CPU *cpu)
{
	prepMessage(cpu); printf("stop\n");
	cpu->r.T = 0;
	cpu->r.TROF = 0;	// idle the processor
	cpu->r.PROF = 0;
	cpu->busy = 0;
	cpu->cycleLimit = 0;	// exit the loop
}

/***********************************************************************
* Called from CC initiate this(cpu) processor as P2. Fetches the
* INCW from @10, injects an initiate syllable into T, and calls start()
* VERIFIED against Paul's JavaScript 17-10-22
***********************************************************************/
void initiateAsP2(CPU *cpu)
{
	prepMessage(cpu); printf("initiateAsP2\n");
	cpu->r.NCSF = false;	// make P2 is in Control State to execute the IP1 & access low mem
	cpu->r.M = AA_IODESC;	// address of the INCW
	loadBviaM(cpu);		// B = [M]
	cpu->r.AROF = 0;	// make sure A is invalid
	cpu->r.T = 04111;	// inject 4111=IP1 into P2's T register
	cpu->r.TROF = 1;
	start(cpu);
}

/***********************************************************************
* Presets the processor registers for a load condition at C=runAddr
* VERIFIED against Paul's JavaScript 17-10-22
***********************************************************************/
void preset(CPU *cpu, ADDR15 runAddr)
{
	prepMessage(cpu); printf("preset to %05o\n", runAddr);
        cpu->r.C = runAddr;     // starting execution address
        cpu->r.L = 1;           // preset L to point to the second syllable
        loadPviaC(cpu);         // load the program word to P
        cpu->r.T = (cpu->r.P >> 36) & 07777;	// get leftmost syllable
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
	if (!cpu->r.TROF) {
		prepMessage(cpu); printf("run, TROF not set!\n");
		while (1) ;
	}
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
		// check for any registers gone wild
#define CHECK(R,M,T) if((cpu->r.R) & ~(M))printf("*\tCHECK "T" = %llo\n", (WORD48)(cpu->r.R))
		CHECK(A, MASK_WORD48, "A");
		CHECK(B, MASK_WORD48, "B");
		CHECK(C, MASK_ADDR15, "C");
		CHECK(F, MASK_ADDR15, "F");
		CHECK(M, MASK_ADDR15, "M");
		CHECK(P, MASK_WORD48, "P");
		CHECK(R, MASK_ADDR9,  "R");
		CHECK(S, MASK_ADDR15, "S");
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


