/***********************************************************************
* b5500emulator
************************************************************************
* Copyright (c) 2016, Reinhard Meyer, DL5UY
* Licensed under the MIT License,
*       see LICENSE
* based on (C) work by Nigel Williams and Paul Kimpel
* see: https://github.com/pkimpel/retro-b5500
************************************************************************
* jumps and calls
************************************************************************
* 2016-02-1921  R.Meyer
*   Converted Paul's work from Javascript to C
***********************************************************************/

#include "b5500_common.h"

/*
 * Adjusts the C and L registers by "count" syllables (which may be negative).
 * Forces a fetch to reload the P register after C and L are adjusted.
 * On entry, C and L are assumed to be pointing to the next instruction
 * to be executed, not the current one
 */
void jumpSyllables(CPU *this, int count)
{
	unsigned addr;

	addr = (this->r.C << 2) + this->r.L + count;
	this->r.C = (addr >> 2) & MASKMEM;
	this->r.L = addr & 3;
	// require fetch at SECL
	this->r.PROF = 0;
}

/*
 * Adjusts the C register by "count" words (which may be negative). L is set
 * to zero. Forces a fetch to reload the P register after C and L are adjusted.
 * On entry, C is assumed to be pointing to the CURRENT instruction word, i.e.,
 * Inhibit Fetch and Inhibit Count for Fetch have both been asserted. Any adjustment
 * to C to account for the emulator's automatic C/L increment at SECL is the
 * responsibility of the caller */
void jumpWords(CPU *this, int count)
{
	this->r.C = (this->r.C + count) & MASKMEM;
	this->r.L = 0;
	// require fetch at SECL
	this->r.PROF = 0;
}

/*
 * Terminates the current character-mode loop by restoring the prior LCW
 * (or RCW) from the stack to X. If "count" is not zero, adjusts C & L forward
 * by that number of syllables and reloads P to branch to the jump-out location,
 * otherwise continues in sequence. Uses A to restore X and invalidates A
 */
void jumpOutOfLoop(CPU *this, int count)
{
	ADDR15	t1 = this->r.S; // save S (not the way the hardware did it)

	this->cycleCount += 2;
	// get prior LCW addr from X value
	this->r.S = fieldIsolate(this->r.X, 18, 15);
	loadAviaS(this); // A = [S], fetch prior LCW from stack
	if (count) {
		this->cycleCount += (count >> 2) + (count & 3);
		jumpSyllables(this, count);
	}
	// store prior LCW (39 bits: less control bits) in X
	this->r.X = this->r.A & MASK_MANTISSA;
	// restore S
	this->r.S = t1;
	// invalidate A
	this->r.AROF = 0;
}

WORD48 buildMSCW(CPU *this)
{
	return 0;
}

void applyMSCW(CPU *this, WORD48 mscw)
{
}

void operandCall(CPU *this)
{
}

void descriptorCall(CPU *this)
{
}

int exitSubroutine(CPU *this, int how)
{
	return 0;
}

