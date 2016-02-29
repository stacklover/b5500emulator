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
}

void accessError(CPU *this)
{
}

/*
 * Computes an absolute memory address from the relative "offset" parameter
 * and leaves it in the M register. See Table 6-1 in the B5500 Reference
 * Manual. "cEnable" determines whether C-relative addressing is permitted.
 * This offset must be in (0..1023)
 */
void computeRelativeAddr(CPU *this, unsigned offset, BIT cEnabled)
{
	this->cycleCount += 2; // approximate the timing
	if (!this->r.SALF) {
		this->r.M = (this->r.R<<6) + (offset & 1023);
	} else {
		switch ((offset & 1023) >> 7) {
		case 0:
		case 1:
		case 2:
		case 3:
			this->r.M = (this->r.R<<6) + (offset & 511);
			break;
		case 4:
		case 5:
			if (this->r.MSFF) {
				this->r.M = (this->r.R<<6) + 7;
				loadMviaM(this); // M = [M].[18:15]
				this->r.M += (offset & 255);
			} else {
				this->r.M = this->r.F + (offset & 255);
			}
			break;
		case 6:
			if (cEnabled) {
				this->r.M = (this->r.L ? this->r.C : this->r.C-1)
					+ (offset & 127); // adjust C for fetch
			} else {
				this->r.M = (this->r.R<<6) + (offset & 127);
			}
			break;
		case 7:
			if (this->r.MSFF) {
				this->r.M = (this->r.R<<6) + 7;
				loadMviaM(this); // M = [M].[18:15]
				this->r.M -= (offset & 127);
			} else {
				this->r.M = this->r.F - (offset & 127);
			}
			break;
		} // switch
	}

	// Reset variant-mode R-relative addressing, if enabled
	if (this->r.VARF) {
		this->r.SALF = 1;
		this->r.VARF = 0;
	}
}

/*
 * Indexes a descriptor and, if successful leaves the indexed value in
 * the A register. Returns 1 if an interrupt is set and the syllable is
 * to be exited
 */
BIT indexDescriptor(CPU *this)
{
	WORD48		aw;	// local copy of A reg
	WORD48		bw;	// local copy of B reg
	BIT		interrupted = 0;	// fatal error, interrupt set
	int		xe;	// index exponent
	WORD49		xm;	// index mantissa
	BIT		xs;	// index mantissa sign
	BIT		xt;	// index exponent sign

	adjustABFull(this);
	aw = this->r.A;
	bw = this->r.B;
	xm = (bw & MASK_MANTISSA) << SHFT_MANTISSALJ;
	xe = (bw & MASK_EXPONENT) >> SHFT_EXPONENT;
	xs = (bw & MASK_SIGNMANT) >> SHFT_SIGNMANT;
	xt = (bw & MASK_SIGNEXPO) >> SHFT_SIGNEXPO;
	if (xt)
		xe = -xe;

	// Normalize the index, if necessary
	if (xe < 0) { // index exponent is negative
		do {
			++this->cycleCount;
			xm >>= 3;
			++xe;
		} while (xe < 0);
		xm += VALU_ROUNDUP; // round up the index
	} else if (xe > 0) { // index exponent is positive
		do {
			++this->cycleCount;
			if (!(xm & MASK_MANTHIGHLJ)) {
				xm <<= 3;
			} else { // oops... integer overflow normalizing the index
				xe = 0; // kill the loop
				interrupted = 1;
				if (this->r.NCSF) {
					this->r.I = (this->r.I & 0x0F) | 0xC0; // set I07/8: integer overflow
					signalInterrupt(this);
				}
			}
		} while (--xe > 0);
	}
	// right align mantissa in word
	xm >>= SHFT_MANTISSALJ;

	// look only at lowest 10 bits
	xm &= 1023;

	// Now we have an integerized index value in xm
	if (!interrupted) {
		if (xs && xm) {
			// Oops... index is negative
			interrupted = 1;
			if (this->r.NCSF) {
				this->r.I = (this->r.I & 0x0F) | 0x90; // set I05/8: invalid-index
				signalInterrupt(this);
			}
		} else if (xm < ((aw & MASK_DDWC) >> SHFT_DDWC)) {
			// We finally have a valid index
			xm = (aw + xm) & MASK_DDADDR;
			this->r.A = (aw & ~MASK_DDADDR) | xm;
			this->r.BROF = 0;
		} else {
			// Oops... index not less than size
			interrupted = 1;
			if (this->r.NCSF) {
				this->r.I = (this->r.I & 0x0F) | 0x90; // set I05/8: invalid-index
				signalInterrupt(this);
			}
		}
	}
	return interrupted;
}

void loadAviaS(CPU *this)
{
	this->r.E = 2;		// Just to show the world what's happening
	this->acc.addr = this->r.S;
	this->acc.MAIL = (this->r.S < 0x0200) && this->r.NCSF;
	fetch(&this->acc);
	//this->cycleCount += B5500CentralControl.memReadCycles;
	if (this->acc.MAED || this->acc.MPED) {
		accessError(this);
	} else {
		this->r.A = this->acc.word;
		this->r.AROF = 1;
	}
}

void loadBviaS(CPU *this)
{
	this->r.E = 3;		// Just to show the world what's happening
	this->acc.addr = this->r.S;
	this->acc.MAIL = (this->r.S < 0x0200) && this->r.NCSF;
	fetch(&this->acc);
	//this->cycleCount += B5500CentralControl.memReadCycles;
	if (this->acc.MAED || this->acc.MPED) {
		accessError(this);
	} else {
		this->r.B = this->acc.word;
		this->r.BROF = 1;
	}
}

void loadAviaM(CPU *this)
{
	this->r.E = 4;		// Just to show the world what's happening
	this->acc.addr = this->r.M;
	this->acc.MAIL = (this->r.M < 0x0200) && this->r.NCSF;
	fetch(&this->acc);
	//this->cycleCount += B5500CentralControl.memReadCycles;
	if (this->acc.MAED || this->acc.MPED) {
		accessError(this);
	} else {
		this->r.A = this->acc.word;
		this->r.AROF = 1;
	}
}

void loadBviaM(CPU *this)
{
	this->r.E = 5;		// Just to show the world what's happening
	this->acc.addr = this->r.M;
	this->acc.MAIL = (this->r.M < 0x0200) && this->r.NCSF;
	fetch(&this->acc);
	//this->cycleCount += B5500CentralControl.memReadCycles;
	if (this->acc.MAED || this->acc.MPED) {
		accessError(this);
	} else {
		this->r.B = this->acc.word;
		this->r.BROF = 1;
	}
}

void loadMviaM(CPU *this)
{
	this->r.E = 6;		// Just to show the world what's happening
	this->acc.addr = this->r.M;
	this->acc.MAIL = (this->r.M < 0x0200) && this->r.NCSF;
	fetch(&this->acc);
	//this->cycleCount += B5500CentralControl.memReadCycles;
	if (this->acc.MAED || this->acc.MPED) {
		accessError(this);
	} else {
		this->r.M = this->acc.word;
	}
}

void loadPviaC(CPU *this)
{
	this->r.E = 48;		// Just to show the world what's happening
	this->acc.addr = this->r.C;
	this->acc.MAIL = (this->r.C < 0x0200) && this->r.NCSF;
	fetch(&this->acc);
	this->r.PROF = 1;
	//this->cycleCount += B5500CentralControl.memReadCycles;
	if (this->acc.MAED || this->acc.MPED) {
		accessError(this);
	} else {
		this->r.P = this->acc.word;
	}
}

void storeAviaS(CPU *this)
{
	this->r.E = 10;		// Just to show the world what's happening
	this->acc.addr = this->r.S;
	this->acc.MAIL = (this->r.S < 0x0200) && this->r.NCSF;
	this->acc.word = this->r.A;
	store(&this->acc);
	this->r.PROF = 1;
	//this->cycleCount += B5500CentralControl.memReadCycles;
	if (this->acc.MAED || this->acc.MPED) {
		accessError(this);
	}
}

void storeBviaS(CPU *this)
{
	this->r.E = 11;		// Just to show the world what's happening
	this->acc.addr = this->r.S;
	this->acc.MAIL = (this->r.S < 0x0200) && this->r.NCSF;
	this->acc.word = this->r.B;
	store(&this->acc);
	this->r.PROF = 1;
	//this->cycleCount += B5500CentralControl.memReadCycles;
	if (this->acc.MAED || this->acc.MPED) {
		accessError(this);
	}
}

void storeAviaM(CPU *this)
{
	this->r.E = 12;		// Just to show the world what's happening
	this->acc.addr = this->r.M;
	this->acc.MAIL = (this->r.M < 0x0200) && this->r.NCSF;
	this->acc.word = this->r.A;
	store(&this->acc);
	this->r.PROF = 1;
	//this->cycleCount += B5500CentralControl.memReadCycles;
	if (this->acc.MAED || this->acc.MPED) {
		accessError(this);
	}
}

void storeBviaM(CPU *this)
{
	this->r.E = 12;		// Just to show the world what's happening
	this->acc.addr = this->r.M;
	this->acc.MAIL = (this->r.M < 0x0200) && this->r.NCSF;
	this->acc.word = this->r.B;
	store(&this->acc);
	this->r.PROF = 1;
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
	WORD48		aw;	// local copy of A reg
	WORD48		bw;	// local copy of B reg
	int		be;	// B exponent
	WORD49		bm;	// B mantissa
	BIT		bs;	// B mantissa sign
	BIT		bt;	// B exponent sign
	BIT		doStore = 1;	// okay to store
	BIT		normalize = 1;	// okay to integerize

	adjustABFull(this);
	aw = this->r.A;
	if (!(aw & MASK_CONTROLW)) {
		// it's an operand
		computeRelativeAddr(this, aw, 0);
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
		bw = this->r.B;
		bm = (bw & MASK_MANTISSA) << SHFT_MANTISSALJ;
		be = (bw & MASK_EXPONENT) >> SHFT_EXPONENT;
		bs = (bw & MASK_SIGNMANT) >> SHFT_SIGNMANT;
		bt = (bw & MASK_SIGNEXPO) >> SHFT_SIGNEXPO;
		if (bt)
			be = -be;

		if (be != 0) { // is B non-integer?
			if (be < 0) { // B exponent is negative
				do {
					++this->cycleCount;
					bm >>= 3;
				} while (++be < 0);
				bm += VALU_ROUNDUP; // round up
			} else { // B exponent is positive and not zero
				do {
					++this->cycleCount;
					if (!(bm & MASK_MANTHIGHLJ)) {
						bm <<= 3;
					} else {
						// oops... integer overflow normalizing the mantisa
						doStore = false;
						if (this->r.NCSF) {
							this->r.I = (this->r.I & 0x0F) | 0xC0; // set I07/8: integer overflow
							signalInterrupt(this);
						}
						break; // kill the loop
					}
				} while (--be > 0);
			}
			// right align mantissa in word
			bm >>= SHFT_MANTISSALJ;

			if (doStore) {
				this->r.B = ((WORD48)bs << SHFT_SIGNMANT) + bm;
			}
		}
	}

	if (doStore) {
		storeBviaM(this);
		this->r.AROF = 0;
		if (destructive) {
			this->r.BROF = 0;
		}
	}
}
