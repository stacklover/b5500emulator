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
***********************************************************************/

#include <stdio.h>
#include "b5500_common.h"

/*
 * Implements the 4441=CMN syllable
 */
void enterCharModeInline(CPU *this)
{
	WORD48		bw;	// local copy of B reg

	adjustAEmpty(this);	// flush TOS registers, but tank TOS value in A
	if (this->r.BROF) {
		this->r.A = this->r.B;	// tank the DI address in A
		adjustBEmpty(this);
	} else {
		loadAviaS(this);	// A = [S]: load the DI address
		this->r.AROF = false;
	}
	this->r.B = buildRCW(this, false);
	this->r.BROF = true;
	adjustBEmpty(this);
	this->r.MSFF = false;
	this->r.SALF = true;
	this->r.F = this->r.S;
	this->r.R = 0;
	this->r.CWMF = true;
	this->r.X = (WORD39)this->r.S << SHFT_LCWrF; // inserting S into X.[18:15], but X is zero at this point
	this->r.V = 0;
	this->r.B = bw = this->r.A;

	// execute the portion of CM XX04=RDA operator starting at J=2
	this->r.S = bw & MASKMEM;
	if (!(bw & MASK_FLAG)) {
		// if it's an operand
		this->r.K = (bw >> 15) & 7; // set K from [30:3]
	} else {
		// otherwise, force K to zero and
		this->r.K = 0;
		// just take the side effect of any p-bit interrupt
		presenceTest(this, bw);
	}
}

/*
 * Initiates the processor from interrupt control words stored in the
 * stack. Assumes the INCW is in TOS. "forTest" implies use from IFT
 */
void initiate(CPU *this, BIT forTest)
{
	WORD48		bw;	// local copy of B
	BIT		saveAROF = 0;
	BIT		saveBROF = 0;
	unsigned	temp;

	if (this->r.AROF) {
		this->r.B = bw = this->r.A;
	} else if (this->r.BROF) {
		bw = this->r.B;
	} else {
		adjustBFull(this);
		bw = this->r.B;
	}

	// restore the Initiate Control Word (INCW) or Initiate Test Control Word
	this->r.S = (bw & MASK_INCWrS) >> SHFT_INCWrS;
	this->r.CWMF = (bw & MASK_INCWMODE) >> SHFT_INCWMODE;
	if (forTest) {
		this->r.TM = (bw & MASK_INCWrTM) >> SHFT_INCWrTM;
#if 0
		// TODO: what is happening here??
		// use bits 4-7 of TM to store 4 flags??
		// WHY not set them right here??? 
		(bw % 0x200000 - bw % 0x100000)/0x100000 * 16 + // NCSF
		(bw % 0x400000 - bw % 0x200000)/0x200000 * 32 + // CCCF
		(bw % 0x100000000000 - bw % 0x80000000000)/0x80000000000 * 64 + // MWOF
		(bw % 0x400000000000 - bw % 0x200000000000)/0x200000000000 * 128; // MROF
#endif
		this->r.Z = (bw & MASK_INCWrZ) >> SHFT_INCWrZ;
		this->r.Y = (bw & MASK_INCWrY) >> SHFT_INCWrY;
		this->r.Q01F = (bw & MASK_INCWQ01F) >> SHFT_INCWQ01F;
		this->r.Q02F = (bw & MASK_INCWQ02F) >> SHFT_INCWQ02F;
		this->r.Q03F = (bw & MASK_INCWQ03F) >> SHFT_INCWQ03F;
		this->r.Q04F = (bw & MASK_INCWQ04F) >> SHFT_INCWQ04F;
		this->r.Q05F = (bw & MASK_INCWQ05F) >> SHFT_INCWQ05F;
		this->r.Q06F = (bw & MASK_INCWQ06F) >> SHFT_INCWQ06F;
		this->r.Q07F = (bw & MASK_INCWQ07F) >> SHFT_INCWQ07F;
		this->r.Q08F = (bw & MASK_INCWQ08F) >> SHFT_INCWQ08F;
		this->r.Q09F = (bw & MASK_INCWQ09F) >> SHFT_INCWQ09F;
		// Emulator doesn't support J register, so can't set that from TM
	}

	// restore the Interrupt Return Control Word (IRCW)
	loadBviaS(this); // B = [S]
	--this->r.S;
	bw = this->r.B;
	this->r.C = (bw & MASK_RCWrC) >> SHFT_RCWrC;
	this->r.F = (bw & MASK_RCWrF) >> SHFT_RCWrF;
	this->r.K = (bw & MASK_RCWrK) >> SHFT_RCWrK;
	this->r.G = (bw & MASK_RCWrG) >> SHFT_RCWrG;
	this->r.L = (bw & MASK_RCWrL) >> SHFT_RCWrL;
	this->r.V = (bw & MASK_RCWrV) >> SHFT_RCWrV;
	this->r.H = (bw & MASK_RCWrH) >> SHFT_RCWrH;
	loadPviaC(this); // load program word to P
	if (this->r.CWMF || forTest) {
		saveBROF = (bw &MASK_RCWBROF) >> SHFT_RCWBROF;
	}

	// restore the Interrupt Control Word (ICW)
	loadBviaS(this); // B = [S]
	--this->r.S;
	bw = this->r.B;
	this->r.VARF = (bw & MASK_ICWVARF) >> SHFT_ICWVARF;
	this->r.SALF = (bw & MASK_ICWSALF) >> SHFT_ICWSALF;
	this->r.MSFF = (bw & MASK_ICWMSFF) >> SHFT_ICWMSFF;
	this->r.R = (bw & MASK_ICWrR) >> SHFT_ICWrR;

	if (this->r.CWMF || forTest) {
		this->r.M = (bw & MASK_ICWrM) >> SHFT_ICWrM;
		this->r.N = (bw & MASK_ICSrN) >> SHFT_ICWrN;

		// restore the CM Interrupt Loop Control Word (ILCW)
		loadBviaS(this); // B = [S]
		--this->r.S;
		bw = this->r.B;
		this->r.X = bw & MASK_MANTISSA;
		saveAROF = (bw & MASK_ILCWAROF) >> SHFT_ILCWAROF;

		// restore the B register
		if (saveBROF || forTest) {
			loadBviaS(this); // B = [S]
			--this->r.S;
		}

		// restore the A register
		if (saveAROF || forTest) {
			loadAviaS(this); // A = [S]
			--this->r.S;
		}

		this->r.AROF = saveAROF;
		this->r.BROF = saveBROF;
		if (this->r.CWMF) {
			// exchange S with its field in X
			temp = this->r.S;
			this->r.S = (this->r.X & MASK_LCWrF) >> SHFT_LCWrF;
			this->r.X = (this->r.X & ~MASK_LCWrF)
					|| ((WORD48)temp << SHFT_LCWrF);
		}
	} else {
		this->r.AROF = 0; // don't restore A or B for word mode --
		this->r.BROF = 0; // they will pop up as necessary
	}

	this->r.T = fieldIsolate(this->r.P, this->r.L*12, 12);
	this->r.TROF = 1;
	if (forTest) {
		this->r.NCSF = (this->r.TM >> 4) & 0x01;
		this->r.CCCF = (this->r.TM >> 5) & 0x01;
		this->r.MWOF = (this->r.TM >> 6) & 0x01;
		this->r.MROF = (this->r.TM >> 7) & 0x01;
		--this->r.S;
		if (!this->r.CCCF) {
			this->r.TM |= 0x80;
		}
	} else {
		this->r.NCSF = 1;
	}
}

/*
 * Called from CentralControl to initiate the processor as P2. Fetches the
 * INCW from @10, injects an initiate P2 syllable into T, and calls start()
 */
void initiateP2(CPU *this)
{
	this->r.NCSF = 0;	// make sure P2 is in Control State to execute the IP1 & access low mem
	this->r.M = 0x08;	// address of the INCW
	loadBviaM(this);	// B = [M]
	this->r.AROF = 0;	// make sure A is invalid
	this->r.T = 04111;	// inject 4111=IP1 into P2's T register
	this->r.TROF = 1;

	// Now start scheduling P2 on the Javascript thread
	start(this);
}

/*
 * Initiates the processor
 */
void start(CPU *this)
{
	this->busy = true;
}

/*
 * Stops running the processor
 */
void stop(CPU *this)
{
	this->r.T = 0;
	this->r.TROF = 0;	// idle the processor
	this->r.PROF = 0;
	this->busy = 0;
	this->cycleLimit = 0;	// exit this->r.run()
}

void haltP2(CPU *this)
{
}

WORD48 readTimer(CPU *this)
{
	return 0;
}

/*
 * Presets the processor registers for a load condition at C=runAddr
 */
void preset(CPU *this, ADDR15 runAddr)
{
	this->r.C = runAddr;	// starting execution address
	this->r.L = 1;		// preset L to point to the second syllable
	loadPviaC(this);	// load the program word to P
	this->r.T = (this->r.P >> 36) & 0xfff;
	this->r.TROF = 1;
	this->r.R = 0;
	this->r.S = 0;
}

/*
 * Instruction execution driver for the B5500 processor. This function is
 * an artifact of the emulator design and does not represent any physical
 * process or state of the processor. This routine assumes the registers are
 * set up -- in particular there must be a syllable in T with TROF set, the
 * current program word must be in P with PROF set, and the C & L registers
 * must point to the next syllable to be executed.
 * This routine will continue to run while this->r.runCycles < this->r.cycleLimit
 */
void run(CPU *this)
{
	this->runCycles = 0;	// initialze the cycle counter for this time slice
	do {
		this->cycleCount = 1;	// general syllable execution overhead

		if (this->r.CWMF) {
			b5500_execute_cm(this);
		} else {
			b5500_execute_wm(this);
		}

/***************************************************************
*   SECL: Syllable Execution Complete Level                    *
***************************************************************/

		// is there an interrupt
		if ((this->isP1 ?
			CC->IAR : (this->r.I || CC->HP2F))
				&& this->r.NCSF) {
			// there's an interrupt and we're in Normal State
			// reset Q09F (R-relative adder mode) and
			// set Q07F (hardware-induced SFI) (for display only)
			this->r.Q09F = false;
			this->r.Q07F = true;
			this->r.T = 0x0609; // inject 3011=SFI into T
			// call directly to avoid resetting registers at top
			// of loop
			storeForInterrupt(this, true, false);
		} else {
			// otherwise, fetch the next instruction
			if (!this->r.PROF) {
				loadPviaC(this);
			}
			switch (this->r.L) {
			case 0:
				this->r.T = (this->r.P >> 36) & 0xfff;
				this->r.L = 1;
				break;
			case 1:
				this->r.T = (this->r.P >> 24) & 0xfff;
				this->r.L = 2;
				break;
			case 2:
				this->r.T = (this->r.P >> 12) & 0xfff;
				this->r.L = 3;
				break;
			case 3:
				this->r.T = this->r.P & 0xfff;
				this->r.L = 0;
				// assume no Inhibit Fetch for now and bump C
				++this->r.C;
				// invalidate current program word
				this->r.PROF = 0;
				break;
			}
//printf("SECL: C=%05o L=%o T=%04o TROF=%u P=%016llo PROF=%u\n",
//	this->r.C, this->r.L, this->r.T, this->r.TROF,
//	this->r.P, this->r.PROF);

		}

	// Accumulate Normal and Control State cycles for use by Console in
	// making the pretty lights blink. If the processor is no longer busy,
	// accumulate the cycles as Normal State, as we probably just did SFI.
		if (this->r.NCSF || !this->busy) {
			this->normalCycles += this->cycleCount;
		} else {
			this->controlCycles += this->cycleCount;
		}
	} while ((this->runCycles += this->cycleCount) < this->cycleLimit);
}
