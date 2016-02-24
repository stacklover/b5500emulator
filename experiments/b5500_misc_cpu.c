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

#include "b5500_common.h"

void enterCharModeInline(CPU *this)
{
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
		// WHY notset them right here??? 
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
		this->r.X = (bw & MASK_ILCWrX) >> SHFT_ILCWrX;
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
			this->r.S = (this->r.X & MASK_ILCWrX_S) >> SHFT_ILCWrX_S;
			this->r.X = (this->r.X & ~MASK_ILCWrX_S)
					|| ((WORD48)temp << SHFT_ILCWrX_S);
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

void start(CPU *this)
{
/* Initiates the processor by scheduling it on the Javascript thread */
//var stamp = performance.now();
//this->r.busy = 1;
//this->r.procStart = stamp;
//this->r.procTime -= stamp;
//this->r.delayLastStamp = stamp;
//this->r.delayRequested = 0;
//this->r.scheduler = setCallback(this->r.mnemonic, this, 0, this->r.schedule);
}

void stop(CPU *this)
{
/* Stops running the processor on the Javascript thread */
//var stamp = performance.now();
//this->r.T = 0;
//this->r.TROF = 0;              // idle the processor
//this->r.PROF = 0;
//this->r.busy = 0;
//this->r.cycleLimit = 0;        // exit this->r.run()
//if (this->r.scheduler) {
//	clearCallback(this->r.scheduler);
//	this->r.scheduler = 0;
//}
//while (this->r.procTime < 0) {
//	this->r.procTime += stamp;
//}
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
	this->r.T = fieldIsolate(this->r.P, 0, 12);
	this->r.TROF = 1;
	this->r.R = 0;
	this->r.S = 0;
}
