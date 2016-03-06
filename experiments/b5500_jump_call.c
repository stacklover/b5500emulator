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

#include <stdio.h>
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

/*
 * Return a Mark Stack Control Word from current processor state
 */
WORD48 buildMSCW(CPU *this)
{
	return	INIT_MSCW |
		((WORD48)this->r.F << SHFT_MSCWrF) |
		((WORD48)this->r.SALF << SHFT_MSCWSALF) |
		((WORD48)this->r.MSFF << SHFT_MSCWMSFF) |
		((WORD48)this->r.R << SHFT_MSCWrR);
}

/*
 * Set  processor state from fields of the Mark Stack Control
 * Word in the "word" parameter
 */
void applyMSCW(CPU *this, WORD48 word)
{
	this->r.F = (word & MASK_MSCWrF) >> SHFT_MSCWrF;
	this->r.SALF = (word & MASK_MSCWSALF) >> SHFT_MSCWSALF;
	this->r.MSFF = (word & MASK_MSCWMSFF) >> SHFT_MSCWMSFF;
	this->r.R = (word & MASK_MSCWrR) >> SHFT_MSCWrR;
}

/*
 * Return a Return Control Word from the current processor state
 */
WORD48 buildRCW(CPU *this, BIT descriptorCall)
{
	return	INIT_RCW |
		((WORD48)this->r.C << SHFT_RCWrC) |
		((WORD48)this->r.F << SHFT_RCWrF) |
		((WORD48)this->r.K << SHFT_RCWrK) |
		((WORD48)this->r.G << SHFT_RCWrG) |
		((WORD48)this->r.L << SHFT_RCWrL) |
		((WORD48)this->r.V << SHFT_RCWrV) |
		((WORD48)this->r.H << SHFT_RCWrH) |
		((WORD48)descriptorCall << SHFT_RCWTYPE);
}

/*
 * Set processor state from fields of the Return Control Word in
 * the "word" parameter. If "inline" is truthy, C & L are NOT restored from
 * the RCW. Returns the state of the OPDC/DESC bit [2:1]
 */
BIT applyRCW(CPU *this, WORD48 word, BIT in_line)
{
	if (!in_line) {
		this->r.C = (word & MASK_RCWrC) >> SHFT_RCWrC;
		this->r.L = (word & MASK_RCWrL) >> SHFT_RCWrL;
		this->r.PROF = false;	// require fetch at SECL
	}
	this->r.F = (word & MASK_RCWrF) >> SHFT_RCWrF;
	this->r.K = (word & MASK_RCWrK) >> SHFT_RCWrK;
	this->r.G = (word & MASK_RCWrG) >> SHFT_RCWrG;
	this->r.V = (word & MASK_RCWrV) >> SHFT_RCWrV;
	this->r.H = (word & MASK_RCWrH) >> SHFT_RCWrH;
	return (word & MASK_RCWTYPE) >> SHFT_RCWTYPE;
}

/*
 * OPDC, the moral equivalent of "load accumulator" on lesser
 * machines. Assumes the syllable has already loaded a word into A.
 * See Figures 6-1, 6-3, and 6-4 in the B5500 Reference Manual
 */
void operandCall(CPU *this)
{
	WORD48	aw = this->r.A;		// local copy of A reg value
	BIT	interrupted = 0;	// interrupt occurred

	// If A contains a simple operand, just leave it there, otherwise...
	if (aw & MASK_FLAG) {
		// It's not a simple operand
		switch ((aw & MASK_TYPE) >> SHFT_TYPE) { // aw.[1:3]
		case 2:	// CODE=0, PBIT=1, XBIT=0
		case 3: // CODE=0, PBIT=1, XBIT=1
			// Present data descriptor: see if it must be indexed
			if (aw & MASK_DDWC) { // aw.[8:10]
				interrupted = indexDescriptor(this);
				// else descriptor is already indexed (word count 0)
			}
			if (!interrupted) {
				this->r.M = this->r.A & MASKMEM;
				loadAviaM(this); // A = [M]
				if ((this->r.A & MASK_FLAG) && this->r.NCSF) { // Flag bit is set
					this->r.I = (this->r.I & 0x0F) | 0x80; // set I08: flag-bit interrupt
					signalInterrupt(this);
					// B5500DumpState("Flag Bit: OPDC"); // <<< DEBUG >>>
				}
			}
			break;
		case 7:	//  CODE=1, PBIT=1, XBIT=1
			// Present program descriptor
			enterSubroutine(this, false);
			break;
		case 0:	// CODE=0, PBIT=0, XBIT=0
		case 1:	// CODE=0, PBIT=0, XBIT=1
		case 5:	// CODE=1, PBIT=0, XBIT=1
			// Absent data or program descriptor
			if (this->r.NCSF) {
				this->r.I = (this->r.I & 0x0F) | 0x70; // set I05/6/7: p-bit
				signalInterrupt(this);
				// else if Control State, we're done
			}
			break;
		default: // cases 4, 6	// CODE=1, PBIT=0/1, XBIT=0
			// Miscellaneous control word -- leave as is
			break;
		}
	}
}

/*
 * DESC, the moral equivalent of "load address" on lesser machines.
 * Assumes the syllable has already loaded a word into A, and that the
 * address of that word is in M.
 * See Figures 6-2, 6-3, and 6-4 in the B5500 Reference Manual
 */
void descriptorCall(CPU *this)
{
	WORD48	aw = this->r.A;		// local copy of A reg value
	BIT	interrupted = 0;	// interrupt occurred
//printf("descriptorCall: A=%016llo->", this->r.A);
	if (!(aw & MASK_FLAG)) {
		// It's a simple operand
//printf("operand");
		this->r.A = this->r.M | (MASK_FLAG | MASK_PBIT);
	} else {
		// It's not a simple operand
		switch ((aw & MASK_TYPE) >> SHFT_TYPE) { // aw.[1:3]
		case 2:	// CODE=0, PBIT=1, XBIT=0
		case 3: // CODE=0, PBIT=1, XBIT=1
//printf("present data");
			// Present data descriptor: see if it must be indexed
			if (aw & MASK_DDWC) { // aw.[8:10]
				interrupted = indexDescriptor(this);
				// else descriptor is already indexed (word count 0)
				if (!interrupted) {
					// set word count to zero
					this->r.A &= ~MASK_DDWC;
				}
				// else descriptor is already indexed (word count 0)
			}
			break;
		case 7:	//  CODE=1, PBIT=1, XBIT=1
//printf("present program");
			// Present program descriptor
			enterSubroutine(this, true);
			break;
		case 0:	// CODE=0, PBIT=0, XBIT=0
		case 1:	// CODE=0, PBIT=0, XBIT=1
		case 5:	// CODE=1, PBIT=0, XBIT=1
//printf("absent program/data");
			// Absent data or program descriptor
			if (this->r.NCSF) {
				this->r.I = (this->r.I & 0x0F) | 0x70; // set I05/6/7: p-bit
				signalInterrupt(this);
				// else if Control State, we're done
			}
			break;
		default: // cases 4, 6	// CODE=1, PBIT=0/1, XBIT=0
//printf("misc");
			// Miscellaneous control word
			this->r.A = this->r.M | (MASK_FLAG | MASK_PBIT);
			break;
		}
	}
//printf("\n");
}

/*
 * Enters a subroutine via the present Program Descriptor in A as part
 * of an OPDC or DESC syllable. Also handles accidental entry
 */
void enterSubroutine(CPU *this, BIT descriptorCall)
{
	WORD48	aw = this->r.A;	// local copy of word in A reg
	BIT	arg = (aw & MASK_PCWARGS) >> SHFT_PCWARGS;
	BIT	mode = (aw & MASK_PCWMODE) >> SHFT_PCWMODE;
//printf("enterSubroutine: MSFF=%u\n", this->r.MSFF);
	if (arg && !this->r.MSFF) {
		// just leave the Program Descriptor on TOS
	} else if (mode && !arg) {
		// ditto
	} else {
		// Now we are really going to enter the subroutine
		adjustBEmpty(this);
		if (!arg) {
			// Accidental entry -- mark the stack
			this->r.B = buildMSCW(this);
			this->r.BROF = true;
			adjustBEmpty(this);
			this->r.F = this->r.S;
		}

		// Push a RCW
		this->r.B = buildRCW(this, descriptorCall);
		this->r.BROF = true;
		adjustBEmpty(this);

		// Fetch the first word of subroutine code
		this->r.C = (aw & MASK_PCWADDR) >> SHFT_PCWADDR;
		this->r.L = 0;
		// require fetch at SECL
		this->r.PROF = false;

		// Fix up the rest of the registers
		if (arg) {
			this->r.F = this->r.S;
		} else {
			this->r.F = (aw & MASK_PCWrF) >> SHFT_PCWrF;
			// aw.[18:15]
		}
		this->r.AROF = false;
		this->r.BROF = false;
		this->r.SALF = true;
		this->r.MSFF = false;
		if (mode) {
			this->r.CWMF = 1;
			this->r.R = 0;
			this->r.X = fieldInsert(this->r.X, 18, 15, this->r.S);
			this->r.S = 0;
		}
	}
}

/*
 * Exits a subroutine by restoring the processor state from RCW and MSCW words
 * in the stack. "inline" indicates the C & L registers are NOT restored from the
 * RCW. The RCW is assumed to be in the B register, pointing to the MSCW.
 * The A register is not affected by this routine. If SALF & MSFF bits in the MSCW
 * are set, link back through the MSCWs until one is found that has either bit not
 * set, and store that MSCW at [R]+7. This is the last prior MSCW that actually
 * points to a RCW, thus skipping over any pending subroutine calls that are still
 * building their parameters in the stack. Returns results as follows:
 * 0 = entered by OPDC
 * 1 = entered by DESC
 * 2 = flag bit interrupt set, terminate operator
 */
int exitSubroutine(CPU *this, int in_line)
{
	int	result;

	if (!(this->r.B & MASK_FLAG)) {
		// flag bit not set
		result = 2;
		if (this->r.NCSF) {
			this->r.I = (this->r.I & 0x0F) | 0x80; // set I08: flag-bit
			signalInterrupt(this);
		}
	} else {
		// flag bit is set
		result = applyRCW(this, this->r.B, in_line);
		this->r.X = this->r.B & MASK_MANTISSA;
		// save F setting from RCW to restore S at end
		this->r.S = this->r.F;
		loadBviaS(this); // B = [S], fetch the MSCW
		applyMSCW(this, this->r.B);

		if (this->r.MSFF && this->r.SALF) {
			this->r.Q06F = true; // set Q06F, not used except for display
			do {
				this->r.S = (this->r.B & MASK_MSCWrF) >> SHFT_MSCWrF;
				loadBviaS(this); // B = [S], fetch prior MSCW
			} while (this->r.B & MASK_MSCWMSFF); // MSFF
			this->r.S = (this->r.R<<6) + 7;
			storeBviaS(this); // [S] = B, store last MSCW at [R]+7
		}
		this->r.S = ((this->r.X & MASK_MSCWrF) >> SHFT_MSCWrF) - 1;
		this->r.BROF = false;
	}
//printf("exitSubroutine: %d\n", result);
	return result;
}
