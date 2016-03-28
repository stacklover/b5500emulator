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
* 2016-02-1921  R.Meyer
*   Converted Paul's work from Javascript to C
***********************************************************************/

#include <stdio.h>
#include "b5500_common.h"

/*
 * Tests and returns the presence bit [2:1] of the "word" parameter,
 * which it assumes is a control word. If [2:1] is 0, the p-bit interrupt
 * is set; otherwise no further action
 */
BIT presenceTest(CPU *this, WORD48 word)
{
	if (word & MASK_PBIT)
		return true;

	if (this->r.NCSF) {
		this->r.I = (this->r.I & 0x0F) | 0x70; // set I05/6/7: p-bit
		signalInterrupt(this);
	}
	return false;
}

int interrogateUnitStatus(CPU *this)
{
	return 0;
}

int interrogateIOChannel(CPU *this)
{
	return 0;
}

/*
 * Implements the 3011=SFI operator and the parts of 3411=SFT that are
 * common to it. "forced" implies Q07F: a hardware-induced SFI syllable.
 * "forTest" implies use from SFT
 */
void storeForInterrupt(CPU *this, BIT forced, BIT forTest)
{
	BIT		saveAROF = this->r.AROF;
	BIT		saveBROF = this->r.BROF;
	unsigned	temp;

	if (forced || forTest) {
		this->r.NCSF = 0; // switch to Control State
	}

	if (this->r.CWMF) {
		// in Character Mode, get the correct TOS address from X
		temp = this->r.S;
		this->r.S = (this->r.X & MASK_RCWrF) >> SHFT_RCWrF;
		this->r.X = (this->r.X & MASK_RCWrC) | (temp << SHFT_RCWrF);

		if (saveAROF || forTest) {
			++this->r.S;
			storeAviaS(this); // [S] = A
		}

		if (saveBROF || forTest) {
			++this->r.S;
			storeBviaS(this); // [S] = B
		}

		// store Character Mode Interrupt Loop-Control Word (ILCW)
		// 444444443333333333222222222211111111110000000000
		// 765432109876543210987654321098765432109876543210
		// 11A000000XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
		this->r.B = INIT_ILCW | this->r.X |
			((WORD48)saveAROF << SHFT_ILCWAROF); 
		++this->r.S;
		if (dotrcins)
			printf("ILCW:");
		storeBviaS(this); // [S] = ILCW
	} else {
		// in word mode, save B and A if not empty
		if (saveBROF || forTest) {
			++this->r.S;
			storeBviaS(this); // [S] = B
		}

		if (saveAROF || forTest) {
			++this->r.S;
			storeAviaS(this); // [S] = A
		}
	}

	// store Interrupt Control Word (ICW)
	// 444444443333333333222222222211111111110000000000
	// 765432109876543210987654321098765432109876543210
	// 110000RRRRRRRRR0MS00000V00000NNNNMMMMMMMMMMMMMMM
	this->r.B = INIT_ICW |
		((WORD48)this->r.M << SHFT_ICWrM) |
		((WORD48)this->r.N << SHFT_ICWrN) |
		((WORD48)this->r.VARF << SHFT_ICWVARF) |
		((WORD48)this->r.SALF << SHFT_ICWSALF) |
		((WORD48)this->r.MSFF << SHFT_ICWMSFF) |
		((WORD48)this->r.R << SHFT_ICWrR);
	++this->r.S;
	if (dotrcins)
		printf("ICW: ");
	storeBviaS(this); // [S] = ICW

	// store Interrupt Return Control Word (IRCW)
	// 444444443333333333222222222211111111110000000000
	// 765432109876543210987654321098765432109876543210
	// 11B0HHHVVVLLGGGKKKFFFFFFFFFFFFFFFCCCCCCCCCCCCCCC
	this->r.B = INIT_RCW |
		((WORD48)this->r.C << SHFT_RCWrC) |
		((WORD48)this->r.F << SHFT_RCWrF) |
		((WORD48)this->r.K << SHFT_RCWrK) |
		((WORD48)this->r.G << SHFT_RCWrG) |
		((WORD48)this->r.L << SHFT_RCWrL) |
		((WORD48)this->r.V << SHFT_RCWrV) |
		((WORD48)this->r.H << SHFT_RCWrH) |
		((WORD48)saveBROF << SHFT_RCWBROF);
	++this->r.S;
	if (dotrcins)
		printf("IRCW:");
	storeBviaS(this); // [S] = IRCW

	if (this->r.CWMF) {
		// if CM, get correct R value from last MSCW
		temp = this->r.F;
		this->r.F = this->r.S;
		this->r.S = temp;

		loadBviaS(this); // B = [S]: get last RCW
		this->r.S = (this->r.B & MASK_RCWrF) >> SHFT_RCWrF;

		loadBviaS(this); // B = [S]: get last MSCW
		this->r.R = (this->r.B & MASK_MSCWrR) >> SHFT_MSCWrR;
		this->r.S = this->r.F;
	}

	// build the Initiate Control Word (INCW)
	// 444444443333333333222222222211111111110000000000
	// 765432109876543210987654321098765432109876543210
	// 11000QQQQQQQQQYYYYYYZZZZZZ0TTTTTCSSSSSSSSSSSSSSS
	this->r.B = INIT_INCW |
		((WORD48)this->r.S << SHFT_INCWrS) |
		((WORD48)this->r.CWMF << SHFT_INCWMODE) |
		(((WORD48)this->r.TM << SHFT_INCWrTM) & MASK_INCWrTM) |
		((WORD48)this->r.Z << SHFT_INCWrZ) |
		((WORD48)this->r.Y << SHFT_INCWrY) |
		((WORD48)this->r.Q01F << SHFT_INCWQ01F) |
		((WORD48)this->r.Q02F << SHFT_INCWQ02F) |
		((WORD48)this->r.Q03F << SHFT_INCWQ03F) |
		((WORD48)this->r.Q04F << SHFT_INCWQ04F) |
		((WORD48)this->r.Q05F << SHFT_INCWQ05F) |
		((WORD48)this->r.Q06F << SHFT_INCWQ06F) |
		((WORD48)this->r.Q07F << SHFT_INCWQ07F) |
		((WORD48)this->r.Q08F << SHFT_INCWQ08F) |
		((WORD48)this->r.Q09F << SHFT_INCWQ09F);
	this->r.M = (this->r.R<<6) + 8; // store initiate word at R+@10
	if (dotrcins)
		printf("INCW:");
	storeBviaM(this); // [M] = INCW

	this->r.M = 0;
	this->r.R = 0;
	this->r.MSFF = 0;
	this->r.SALF = 0;
	this->r.BROF = 0;
	this->r.AROF = 0;
	if (forTest) {
		this->r.TM = 0;
		this->r.MROF = 0;
		this->r.MWOF = 0;
	}

	if (forced || forTest) {
		this->r.CWMF = 0;
	}

	if (this->isP1) {
		// if it's P1
		if (forTest) {
			loadBviaM(this); // B = [M]: load DD for test
			this->r.C = (this->r.B & MASK_RCWrC) >> SHFT_RCWrC;
			this->r.L = 0;
			this->r.PROF = 0; // require fetch at SECL
			this->r.G = 0;
			this->r.H = 0;
			this->r.K = 0;
			this->r.V = 0;
		} else {
			if (dotrcins)
				printf("injected ITI\n");
			this->r.T = 0211; // inject 0211=ITI into P1's T register
		}
	} else {
		// if it's P2
		stop(this); // idle the P2 processor
		CC->P2BF = 0; // tell CC and P1 we've stopped
	}
}

void clearInterrupt(CPU *this)
{
}

void initiateIO(CPU *this)
{
}

