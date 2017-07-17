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
* 2016-02-1921  R.Meyer
*   Converted Paul's work from Javascript to C
* 2017-07-17  R.Meyer
*   changed "this" to "cpu" to avoid errors when using g++
***********************************************************************/

#include <stdio.h>
#include "b5500_common.h"

/*
 * Implements the 4441=CMN syllable
 */
void enterCharModeInline(CPU *cpu)
{
	WORD48		bw;	// local copy of B reg

	adjustAEmpty(cpu);	// flush TOS registers, but tank TOS value in A
	if (cpu->r.BROF) {
		cpu->r.A = cpu->r.B;	// tank the DI address in A
		adjustBEmpty(cpu);
	} else {
		loadAviaS(cpu);	// A = [S]: load the DI address
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
	cpu->r.X = (WORD39)cpu->r.S << SHFT_LCWrF; // inserting S into X.[18:15], but X is zero at cpu point
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
	WORD48		bw;	// local copy of B
	BIT		saveAROF = 0;
	BIT		saveBROF = 0;
	unsigned	temp;

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
	cpu->r.CWMF = (bw & MASK_INCWMODE) >> SHFT_INCWMODE;
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
		cpu->r.Q01F = (bw & MASK_INCWQ01F) >> SHFT_INCWQ01F;
		cpu->r.Q02F = (bw & MASK_INCWQ02F) >> SHFT_INCWQ02F;
		cpu->r.Q03F = (bw & MASK_INCWQ03F) >> SHFT_INCWQ03F;
		cpu->r.Q04F = (bw & MASK_INCWQ04F) >> SHFT_INCWQ04F;
		cpu->r.Q05F = (bw & MASK_INCWQ05F) >> SHFT_INCWQ05F;
		cpu->r.Q06F = (bw & MASK_INCWQ06F) >> SHFT_INCWQ06F;
		cpu->r.Q07F = (bw & MASK_INCWQ07F) >> SHFT_INCWQ07F;
		cpu->r.Q08F = (bw & MASK_INCWQ08F) >> SHFT_INCWQ08F;
		cpu->r.Q09F = (bw & MASK_INCWQ09F) >> SHFT_INCWQ09F;
		// Emulator doesn't support J register, so can't set that from TM
	}

	// restore the Interrupt Return Control Word (IRCW)
	loadBviaS(cpu); // B = [S]
	--cpu->r.S;
	bw = cpu->r.B;
	cpu->r.C = (bw & MASK_RCWrC) >> SHFT_RCWrC;
	cpu->r.F = (bw & MASK_RCWrF) >> SHFT_RCWrF;
	cpu->r.K = (bw & MASK_RCWrK) >> SHFT_RCWrK;
	cpu->r.G = (bw & MASK_RCWrG) >> SHFT_RCWrG;
	cpu->r.L = (bw & MASK_RCWrL) >> SHFT_RCWrL;
	cpu->r.V = (bw & MASK_RCWrV) >> SHFT_RCWrV;
	cpu->r.H = (bw & MASK_RCWrH) >> SHFT_RCWrH;
	loadPviaC(cpu); // load program word to P
	if (cpu->r.CWMF || forTest) {
		saveBROF = (bw &MASK_RCWBROF) >> SHFT_RCWBROF;
	}

	// restore the Interrupt Control Word (ICW)
	loadBviaS(cpu); // B = [S]
	--cpu->r.S;
	bw = cpu->r.B;
	cpu->r.VARF = (bw & MASK_ICWVARF) >> SHFT_ICWVARF;
	cpu->r.SALF = (bw & MASK_ICWSALF) >> SHFT_ICWSALF;
	cpu->r.MSFF = (bw & MASK_ICWMSFF) >> SHFT_ICWMSFF;
	cpu->r.R = (bw & MASK_ICWrR) >> SHFT_ICWrR;

	if (cpu->r.CWMF || forTest) {
		cpu->r.M = (bw & MASK_ICWrM) >> SHFT_ICWrM;
		cpu->r.N = (bw & MASK_ICSrN) >> SHFT_ICWrN;

		// restore the CM Interrupt Loop Control Word (ILCW)
		loadBviaS(cpu); // B = [S]
		--cpu->r.S;
		bw = cpu->r.B;
		cpu->r.X = bw & MASK_MANTISSA;
		saveAROF = (bw & MASK_ILCWAROF) >> SHFT_ILCWAROF;

		// restore the B register
		if (saveBROF || forTest) {
			loadBviaS(cpu); // B = [S]
			--cpu->r.S;
		}

		// restore the A register
		if (saveAROF || forTest) {
			loadAviaS(cpu); // A = [S]
			--cpu->r.S;
		}

		cpu->r.AROF = saveAROF;
		cpu->r.BROF = saveBROF;
		if (cpu->r.CWMF) {
			// exchange S with its field in X
			temp = cpu->r.S;
			cpu->r.S = (cpu->r.X & MASK_LCWrF) >> SHFT_LCWrF;
			cpu->r.X = (cpu->r.X & ~MASK_LCWrF)
					|| ((WORD48)temp << SHFT_LCWrF);
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
}

/*
 * Called from CentralControl to initiate the processor as P2. Fetches the
 * INCW from @10, injects an initiate P2 syllable into T, and calls start()
 */
void initiateP2(CPU *cpu)
{
	cpu->r.NCSF = 0;	// make sure P2 is in Control State to execute the IP1 & access low mem
	cpu->r.M = 0x08;	// address of the INCW
	loadBviaM(cpu);	// B = [M]
	cpu->r.AROF = 0;	// make sure A is invalid
	cpu->r.T = 04111;	// inject 4111=IP1 into P2's T register
	cpu->r.TROF = 1;

	// Now start scheduling P2 on the Javascript thread
	start(cpu);
}

/*
 * Initiates the processor
 */
void start(CPU *cpu)
{
	cpu->busy = true;
}

/*
 * Stops running the processor
 */
void stop(CPU *cpu)
{
	cpu->r.T = 0;
	cpu->r.TROF = 0;	// idle the processor
	cpu->r.PROF = 0;
	cpu->busy = 0;
	cpu->cycleLimit = 0;	// exit cpu->r.run()
}

void haltP2(CPU *cpu)
{
}

WORD48 readTimer(CPU *cpu)
{
	return 0;
}

/*
 * Presets the processor registers for a load condition at C=runAddr
 */
void preset(CPU *cpu, ADDR15 runAddr)
{
	cpu->r.C = runAddr;	// starting execution address
	cpu->r.L = 1;		// preset L to point to the second syllable
	loadPviaC(cpu);	// load the program word to P
	cpu->r.T = (cpu->r.P >> 36) & 0xfff;
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
	cpu->runCycles = 0;	// initialze the cycle counter for cpu time slice
	do {
		cpu->cycleCount = 1;	// general syllable execution overhead

		if (cpu->r.CWMF) {
			b5500_execute_cm(cpu);
		} else {
			b5500_execute_wm(cpu);
		}

/***************************************************************
*   SECL: Syllable Execution Complete Level                    *
***************************************************************/

		// is there an interrupt
		if ((cpu->isP1 ?
			CC->IAR : (cpu->r.I || CC->HP2F))
				&& cpu->r.NCSF) {
			// there's an interrupt and we're in Normal State
			// reset Q09F (R-relative adder mode) and
			// set Q07F (hardware-induced SFI) (for display only)
			cpu->r.Q09F = false;
			cpu->r.Q07F = true;
			cpu->r.T = 0x0609; // inject 3011=SFI into T
			// call directly to avoid resetting registers at top
			// of loop
			storeForInterrupt(cpu, true, false);
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
				// assume no Inhibit Fetch for now and bump C
				++cpu->r.C;
				// invalidate current program word
				cpu->r.PROF = 0;
				break;
			}
//printf("SECL: C=%05o L=%o T=%04o TROF=%u P=%016llo PROF=%u\n",
//	cpu->r.C, cpu->r.L, cpu->r.T, cpu->r.TROF,
//	cpu->r.P, cpu->r.PROF);

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
