/***********************************************************************
* b5500emulator
************************************************************************
* Copyright (c) 2016, Reinhard Meyer, DL5UY
* Licensed under the MIT License,
*       see LICENSE
* based on (C) work by Nigel Williams and Paul Kimpel
* see: https://github.com/pkimpel/retro-b5500
************************************************************************
* stack related functions
************************************************************************
* 2016-02-21  R.Meyer
*   Converted Paul's work from Javascript to C
***********************************************************************/

#include "b5500_common.h"

/*
 * Ensures both TOS registers are occupied,
 * pushing up from memory as required
 */
void adjustABFull(CPU *this)
{
	if (this->r.AROF) {
		if (this->r.BROF) {
			// A and B are already full, so we're done
		} else {
			// A is full and B is empty, so load B from [S]
			loadBviaS(this); // B = [S]
			--this->r.S;
		}
	} else {
		if (this->r.BROF) {
			// A is empty and B is full, so copy B to A and load B from [S]
			this->r.A = this->r.B;
			this->r.AROF = 1;
		} else {
			// A and B are empty, so simply load them from [S]
			loadAviaS(this); // A = [S]
			--this->r.S;
		}
		loadBviaS(this); // B = [S]
		--this->r.S;
	}
}

/*
 * Adjusts the A register so that it is full, popping the contents of
 * B or [S] into A, as necessary.
 */
void adjustAFull(CPU *this)
{
	if (!this->r.AROF) {
		if (this->r.BROF) {
			this->r.A = this->r.B;
			this->r.AROF = 1;
			this->r.BROF = 0;
		} else {
			loadAviaS(this); // A = [S]
			--this->r.S;
		}
	}
	// else we're done -- A is already full
}

/*
 * Adjusts the B register so that it is full, popping the contents of
 * [S] into B, as necessary.
 */
void adjustBFull(CPU *this)
{
	if (!this->r.BROF) {
		loadBviaS(this); // B = [S]
		--this->r.S;
	}
	// else we're done -- B is already full
}

/*
 * Adjusts the A and B registers so that both are empty, pushing the
 * prior contents into memory, as necessary.
 */
void adjustABEmpty(CPU *this)
{
	// B occupied ?
	if (this->r.BROF) {
		// empty B to stack
		if (((this->r.S >> 6) == this->r.R) && this->r.NCSF) {
			// set I03F: stack overflow
			this->r.I |= 0x04;
			signalInterrupt(this);
		} else {
			++this->r.S;
			storeBviaS(this); // [S] = B
		}
		// B is now empty
		this->r.BROF = 0;
	}
	// else we're done -- B is already empty

	// A occupied ?
	if (this->r.AROF) {
		// empty A to stack
		if (((this->r.S >> 6) == this->r.R) && this->r.NCSF) {
			// set I03F: stack overflow
			this->r.I |= 0x04;
			signalInterrupt(this);
		} else {
			++this->r.S;
			storeAviaS(this); // [S] = B
		}
		// A is now empty
		this->r.AROF = 0;
	}
	// else we're done -- A is already empty
}

/*
 * Adjusts the A register so that it is empty, pushing the prior
 * contents of A into B and B into memory, as necessary.
 */
void adjustAEmpty(CPU *this)
{
	// A occupied ?
	if (this->r.AROF) {
		// B occupied ?
		if (this->r.BROF) {
			// empty B to stack
			if (((this->r.S >> 6) == this->r.R) && this->r.NCSF) {
				// set I03F: stack overflow
				this->r.I |= 0x04;
				signalInterrupt(this);
			} else {
				++this->r.S;
				storeBviaS(this); // [S] = B
			}
			this->r.BROF = 1;
		}
		// B is now empty, move A to B
		this->r.B = this->r.A;
		this->r.AROF = 0;
	}
	// else we're done -- A is already empty
}

/*
 * Adjusts the B register so that it is empty, pushing the prior
 * contents of B into memory, as necessary.
 */
void adjustBEmpty(CPU *this)
{
	// B occupied ?
	if (this->r.BROF) {
		// empty B to stack
		if (((this->r.S >> 6) == this->r.R) && this->r.NCSF) {
			// set I03F: stack overflow
			this->r.I |= 0x04;
			signalInterrupt(this);
		} else {
			++this->r.S;
			storeBviaS(this); // [S] = B
		}
		// B is now empty
		this->r.BROF = 0;
	}
	// else we're done -- B is already empty
}

/*
 * Exchanges the two top-of-stack values
 */
void exchangeTOS(CPU *this)
{
	WORD48 temp;

	if (this->r.AROF) {
		if (this->r.BROF) {
			// A and B are full, so simply exchange them
			temp = this->r.A;
			this->r.A = this->r.B;
			this->r.B = temp;
		} else {
			// A is full and B is empty, so push A to B and load A from [S]
			this->r.B = this->r.A;
			this->r.BROF = 1;
			loadAviaS(this); // A = [S]
			--this->r.S;
		}
	} else {
		if (this->r.BROF) {
			// A is empty and B is full, so load A from [S]
			loadAviaS(this); // A = [S]
			--this->r.S;
		} else {
			// A and B are empty, so simply load them in reverse order
			loadBviaS(this); // B = [S]
			--this->r.S;
			loadAviaS(this); // A = [S]
			--this->r.S;
		}
	}
}
