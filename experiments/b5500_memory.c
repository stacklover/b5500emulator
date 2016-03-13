/***********************************************************************
* b5500emulator
************************************************************************
* Copyright (c) 2016, Reinhard Meyer, DL5UY
* Licensed under the MIT License,
*       see LICENSE
* based on (C) work by Nigel Williams and Paul Kimpel
* see: https://github.com/pkimpel/retro-b5500
************************************************************************
* memory related functions
************************************************************************
* 2016-02-1921  R.Meyer
*   Converted Paul's work from Javascript to C
***********************************************************************/

#include <stdio.h>
#include "b5500_common.h"

/*
 * Called by a requestor module passing accessor object "acc" to fetch a
 * word from memory.
 */
void fetch(ACCESSOR *acc)
{
	// For now, we assume memory parity can never happen
	if (acc->MAIL) {
		acc->MPED = false;	// no memory parity error
		acc->MAED = true;	// memory address error
		// no .word value is returned in this case
	} else {
		acc->MPED = false;	// no parity error
		acc->MAED = false;	// no address error
		acc->word = MAIN[acc->addr & MASKMEM];
	}
	if (dotrcmem)
		printf(" addr=%05o word=%016llo MPED=%u MAED=%u\n",
			acc->addr, acc->word, acc->MPED, acc->MAED);
}

/*
 * Called by requestor module passing accessor object "acc" to store a
 * word into memory.
 */
void store(ACCESSOR *acc)
{
	// For now, we assume memory parity can never happen
	if (acc->MAIL) {
		acc->MPED = false;	// no memory parity error
		acc->MAED = true;	// memory address error
		// no .word value is returned in this case
	} else {
		acc->MPED = false;	// no parity error
		acc->MAED = false;	// no address error
		MAIN[acc->addr & MASKMEM] = acc->word;
	}
	if (dotrcmem)
		printf(" addr=%05o word=%016llo MPED=%u MAED=%u\n",
			acc->addr, acc->word, acc->MPED, acc->MAED);
}

/*
 * Common error handling routine for all memory acccesses
 */
void accessError(CPU *this)
{
	if (this->acc.MAED) {
		// set I02F: memory address/inhibit error
		this->r.I |= 0x02;
		signalInterrupt(this);
	} else if (this->acc.MPED) {
		// set I01F: memory parity error
		this->r.I |= 0x01;
		signalInterrupt(this);
		if (this->isP1 && !this->r.NCSF) {
			// P1 memory parity in Control State stops the proc
			stop(this);
		}
	}
}

/*
 * Computes an absolute memory address from the relative "offset" parameter
 * and leaves it in the M register. See Table 6-1 in the B5500 Reference
 * Manual. "cEnable" determines whether C-relative addressing is permitted.
 * This offset must be in (0..1023)
 */
void computeRelativeAddr(CPU *this, unsigned offset, BIT cEnabled)
{
	if (dotrcmem)
		printf("RelAddr(%04o,%u) -> ",
			offset, cEnabled);
	this->cycleCount += 2; // approximate the timing
	if (this->r.SALF) {
		// subroutine level - check upper 3 bits of the 10 bit offset
		switch ((offset >> 7) & 7) {
		case 0:
		case 1:
		case 2:
		case 3:
			// pattern 0xx xxxxxxx - R+ relative
			// reach 0..511
			offset &= 0x1ff;
			this->r.M = (this->r.R<<6) + offset;
			if (dotrcmem)
				printf("R+%o -> M=%05o\n", offset, this->r.M);
			break;
		case 4:
		case 5:
			// pattern 10x xxxxxxx - F+ or (R+7)+ relative
			// reach 0..255
			offset &= 0xff;
			if (this->r.MSFF) {
				// during function parameter loading its (R+7)+
				this->r.M = (this->r.R<<6) + RR_MSCW;
				loadMviaM(this); // M = [M].[18:15]
				this->r.M += offset;
				if (dotrcmem)
					printf("(R+7)+%o -> M=%05o\n", offset, this->r.M);
			} else {
				// inside function its F+
				this->r.M = this->r.F + (offset & 0xff);
				if (dotrcmem)
					printf("F+%o -> M=%05o\n", offset, this->r.M);
			}
			break;
		case 6:
			// pattern 110 xxxxxxx - C+ relative on read, R+ on write
			// reach 0..127
			offset &= 0x7f;
			if (cEnabled) {
				// adjust C for fetch offset from
				// syllable the instruction was in
				this->r.M = (this->r.L ? this->r.C : this->r.C-1)
					+ offset;
				if (dotrcmem)
					printf("C+%o -> M=%05o\n", offset, this->r.M);
			} else {
				this->r.M = (this->r.R<<6) + offset;
				if (dotrcmem)
					printf("R+%o -> M=%05o\n", offset, this->r.M);
			}
			break;
		case 7:
			// pattern 111 xxxxxxx - F- or (R+7)- relative
			// reach 0..127 (negative direction)
			offset &= 0x7f;
			if (this->r.MSFF) {
				this->r.M = (this->r.R<<6) + RR_MSCW;
				loadMviaM(this); // M = [M].[18:15]
				this->r.M -= offset;
				if (dotrcmem)
					printf("(R+7)-%o -> M=%05o\n", offset, this->r.M);
			} else {
				this->r.M = this->r.F - offset;
				if (dotrcmem)
					printf("F-%o -> M=%05o\n", offset, this->r.M);
			}
			break;
		} // switch
	} else {
		// program level - all 10 bits are offset
		offset &= 0x3ff;
		this->r.M = (this->r.R<<6) + offset;
		if (dotrcmem)
			printf("R+%o,M=%05o\n", offset, this->r.M);
	}

	// Reset variant-mode R-relative addressing, if it was enabled
	if (this->r.VARF) {
		if (dotrcmem)
			printf("Resetting VARF\n");
		this->r.SALF = true;
		this->r.VARF = false;
	}
}

/*
 * Indexes a descriptor and, if successful leaves the indexed value in
 * the A register. Returns 1 if an interrupt is set and the syllable is
 * to be exited
 */
BIT indexDescriptor(CPU *this)
{
	WORD48	aw;	// local copy of A reg
	BIT		interrupted = false;	// fatal error, interrupt set
	NUM		I;	// index

	adjustABFull(this);
	aw = this->r.A;
	num_extract(&this->r.B, &I);

	// Normalize the index, if necessary
	if (I.e < 0) {
		// index exponent is negative
		this->cycleCount += num_right_shift_exp(&I, 0);
		// round up the index
		num_round(&I);
	} else if (I.e > 0) {
		// index exponent is positive
		this->cycleCount += num_left_shift_exp(&I, 0);
		if (I.e != 0) {
			// oops... integer overflow normalizing the index
			interrupted = true;
			if (this->r.NCSF) {
				// set I07/8: integer overflow
				this->r.I = (this->r.I & IRQ_MASKL) | IRQ_INTO;
				signalInterrupt(this);
			}
		}
	}
	// look only at lowest 10 bits
	I.m &= 0x3ff;

	// Now we have an integerized index value in I
	if (!interrupted) {
		if (I.s && I.m) {
			// Oops... index is negative
			interrupted = true;
			if (this->r.NCSF) {
				// set I05/8: invalid-index
				this->r.I = (this->r.I & IRQ_MASKL) | IRQ_INDEX;
				signalInterrupt(this);
			}
		} else if (I.m < ((aw & MASK_DDWC) >> SHFT_DDWC)) {
			// We finally have a valid index
			I.m = (aw + I.m) & MASK_DDADDR;
			this->r.A = (aw & ~MASK_DDADDR) | I.m;
			this->r.BROF = false;
		} else {
			// Oops... index not less than size
			interrupted = true;
			if (this->r.NCSF) {
				// set I05/8: invalid-index
				this->r.I = (this->r.I & IRQ_MASKL) | IRQ_INDEX;
				signalInterrupt(this);
			}
		}
	}
	return interrupted;
}

/*
 * load registers from memory
 */
void loadAviaS(CPU *this)
{
	if (dotrcmem)
		printf("A=[S]");
	this->r.E = 2;		// Just to show the world what's happening
	this->acc.addr = this->r.S;
	this->acc.MAIL = (this->r.S < AA_USERMEM) && this->r.NCSF;
	fetch(&this->acc);
	//this->cycleCount += B5500CentralControl.memReadCycles;
	if (this->acc.MAED || this->acc.MPED) {
		accessError(this);
	} else {
		this->r.A = this->acc.word;
		this->r.AROF = true;
	}
}

void loadBviaS(CPU *this)
{
	if (dotrcmem)
		printf("B=[S]");
	this->r.E = 3;		// Just to show the world what's happening
	this->acc.addr = this->r.S;
	this->acc.MAIL = (this->r.S < AA_USERMEM) && this->r.NCSF;
	fetch(&this->acc);
	//this->cycleCount += B5500CentralControl.memReadCycles;
	if (this->acc.MAED || this->acc.MPED) {
		accessError(this);
	} else {
		this->r.B = this->acc.word;
		this->r.BROF = true;
	}
}

void loadAviaM(CPU *this)
{
	if (dotrcmem)
		printf("A=[M]");
	this->r.E = 4;		// Just to show the world what's happening
	this->acc.addr = this->r.M;
	this->acc.MAIL = (this->r.M < AA_USERMEM) && this->r.NCSF;
	fetch(&this->acc);
	//this->cycleCount += B5500CentralControl.memReadCycles;
	if (this->acc.MAED || this->acc.MPED) {
		accessError(this);
	} else {
		this->r.A = this->acc.word;
		this->r.AROF = true;
	}
}

void loadBviaM(CPU *this)
{
	if (dotrcmem)
		printf("B=[M]");
	this->r.E = 5;		// Just to show the world what's happening
	this->acc.addr = this->r.M;
	this->acc.MAIL = (this->r.M < AA_USERMEM) && this->r.NCSF;
	fetch(&this->acc);
	//this->cycleCount += B5500CentralControl.memReadCycles;
	if (this->acc.MAED || this->acc.MPED) {
		accessError(this);
	} else {
		this->r.B = this->acc.word;
		this->r.BROF = true;
	}
}

void loadMviaM(CPU *this)
{
	// note: this is only used to get the saved F registers value from
	// the saved MSCW at R+7.
	if (dotrcmem)
		printf("M=[M]");
	this->r.E = 6;		// Just to show the world what's happening
	this->acc.addr = this->r.M;
	this->acc.MAIL = (this->r.M < AA_USERMEM) && this->r.NCSF;
	fetch(&this->acc);
	//this->cycleCount += B5500CentralControl.memReadCycles;
	if (this->acc.MAED || this->acc.MPED) {
		accessError(this);
	} else {
		this->r.M = (this->acc.word & MASK_MSCWrF) >> SHFT_MSCWrF;
	}
}

void loadPviaC(CPU *this)
{
	if (dotrcmem)
		printf("P=[C]");
	this->r.E = 48;		// Just to show the world what's happening
	this->acc.addr = this->r.C;
	this->acc.MAIL = (this->r.C < AA_USERMEM) && this->r.NCSF;
	fetch(&this->acc);
	// TODO: should this not be in the else part?
	this->r.PROF = true;
	//this->cycleCount += B5500CentralControl.memReadCycles;
	if (this->acc.MAED || this->acc.MPED) {
		accessError(this);
	} else {
		this->r.P = this->acc.word;
	}
}

/*
 * store registers to memory
 */
void storeAviaS(CPU *this)
{
	if (dotrcmem)
		printf("[S]=A");
	this->r.E = 10;		// Just to show the world what's happening
	this->acc.addr = this->r.S;
	this->acc.MAIL = (this->r.S < AA_USERMEM) && this->r.NCSF;
	this->acc.word = this->r.A;
	store(&this->acc);
	//this->cycleCount += B5500CentralControl.memReadCycles;
	if (this->acc.MAED || this->acc.MPED) {
		accessError(this);
	}
}

void storeBviaS(CPU *this)
{
	if (dotrcmem)
		printf("[S]=B");
	this->r.E = 11;		// Just to show the world what's happening
	this->acc.addr = this->r.S;
	this->acc.MAIL = (this->r.S < AA_USERMEM) && this->r.NCSF;
	this->acc.word = this->r.B;
	store(&this->acc);
	//this->cycleCount += B5500CentralControl.memReadCycles;
	if (this->acc.MAED || this->acc.MPED) {
		accessError(this);
	}
}

void storeAviaM(CPU *this)
{
	if (dotrcmem)
		printf("[M]=A");
	this->r.E = 12;		// Just to show the world what's happening
	this->acc.addr = this->r.M;
	this->acc.MAIL = (this->r.M < AA_USERMEM) && this->r.NCSF;
	this->acc.word = this->r.A;
	store(&this->acc);
	//this->cycleCount += B5500CentralControl.memReadCycles;
	if (this->acc.MAED || this->acc.MPED) {
		accessError(this);
	}
}

void storeBviaM(CPU *this)
{
	if (dotrcmem)
		printf("[M]=B");
	this->r.E = 12;		// Just to show the world what's happening
	this->acc.addr = this->r.M;
	this->acc.MAIL = (this->r.M < AA_USERMEM) && this->r.NCSF;
	this->acc.word = this->r.B;
	store(&this->acc);
	//this->cycleCount += B5500CentralControl.memReadCycles;
	if (this->acc.MAED || this->acc.MPED) {
		accessError(this);
	}
}

/*
 * Store the value in the B register at the address in the A register (relative
 * or descriptor) and marks the A register empty. "conditional" indicates that
 * integerization is conditional on the type of word in A, and if a descriptor,
 * whether it has the integer bit set
 */
void integerStore(CPU *this, BIT conditional, BIT destructive)
{
	WORD48	aw;	// local copy of A reg
	NUM		B;	// B mantissa
	BIT		doStore = true;		// okay to store
	BIT		normalize = true;	// okay to integerize

	adjustABFull(this);
	aw = this->r.A;
	if (!(aw & MASK_FLAG)) {
		// it's an operand
		computeRelativeAddr(this, aw, false);
	} else {
		// it's a descriptor
		if (presenceTest(this, aw)) {
			this->r.M = aw & MASKMEM;
			if (conditional) {
				if (!(aw & MASK_DDINT)) { // [19:1] is the integer bit
					normalize = false;
				}
			}
		} else {
			doStore = normalize = false;
		}
	}

	if (normalize) {
		num_extract(&this->r.B, &B);

		if (B.e < 0) {
			// exponent is negative
			this->cycleCount += num_right_shift_exp(&B, 0);
			// round up the number
			num_round(&B);
		} else if (B.e > 0) {
			// exponent is positive
			this->cycleCount += num_left_shift_exp(&B, 0);
			if (B.e != 0) {
				// oops... integer overflow normalizing the mantisa
				doStore = false;
				if (this->r.NCSF) {
					// set I07/8: integer overflow
					this->r.I = (this->r.I & IRQ_MASKL) | IRQ_INTO;
					signalInterrupt(this);
				}
			}
			if (doStore) {
				num_compose(&B, &this->r.B);
			}
		}
	}

	if (doStore) {
		storeBviaM(this);
		this->r.AROF = false;
		if (destructive) {
			this->r.BROF = false;
		}
	}
}
