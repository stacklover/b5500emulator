/***********************************************************************
* b5500emulator
************************************************************************
* Copyright (c)	2016, Reinhard Meyer, DL5UY
* Licensed under the MIT License,
*	see LICENSE
* based	on (C) work by Nigel Williams and Paul Kimpel
* see: https://github.com/pkimpel/retro-b5500
************************************************************************
* jumps	and calls
************************************************************************
* 2016-02-1921	R.Meyer
*   Converted Paul's work from Javascript to C
* 2017-07-17  R.Meyer
*   changed "this" to "cpu" to avoid errors when using g++
***********************************************************************/

#include <stdio.h>
#include "b5500_common.h"

/*
 * Adjusts the C and L registers by "count" syllables (which may be negative).
 * Forces a fetch to reload the	P register after C and L are adjusted.
 * On entry, C and L are assumed to be pointing	to the next instruction
 * to be executed, not the current one
 */
void jumpSyllables(CPU *cpu, int count)
{
	unsigned addr;

	addr = (cpu->r.C << 2) + cpu->r.L + count;
	cpu->r.C = (addr >> 2) & MASKMEM;
	cpu->r.L = addr	& 3;
	// require fetch at SECL
	cpu->r.PROF = 0;
}

/*
 * Adjusts the C register by "count" words (which may be negative). L is set
 * to zero. Forces a fetch to reload the P register after C and	L are adjusted.
 * On entry, C is assumed to be	pointing to the	CURRENT	instruction word, i.e.,
 * Inhibit Fetch and Inhibit Count for Fetch have both been asserted. Any adjustment
 * to C	to account for the emulator's automatic	C/L increment at SECL is the
 * responsibility of the caller	*/
void jumpWords(CPU *cpu, int count)
{
	cpu->r.C = (cpu->r.C + count) &	MASKMEM;
	cpu->r.L = 0;
	// require fetch at SECL
	cpu->r.PROF = 0;
}

/*
 * Terminates the current character-mode loop by restoring the prior LCW
 * (or RCW) from the stack to X. If "count" is not zero, adjusts C & L forward
 * by that number of syllables and reloads P to	branch to the jump-out location,
 * otherwise continues in sequence. Uses A to restore X	and invalidates	A
 */
void jumpOutOfLoop(CPU *cpu, int count)
{
	ADDR15	t1 = cpu->r.S; // save S (not the way the hardware did it)

	cpu->cycleCount	+= 2;
	// get prior LCW addr from X value
	cpu->r.S = (cpu->r.X & MASK_LCWrF) >> SHFT_LCWrF;
	loadAviaS(cpu);	// A = [S], fetch prior	LCW from stack
	if (count) {
		cpu->cycleCount	+= (count >> 2)	+ (count & 3);
		jumpSyllables(cpu, count);
	}
	// store prior LCW (39 bits: less control bits)	in X
	cpu->r.X = cpu->r.A & MASK_MANTISSA;
	// restore S
	cpu->r.S = t1;
	// invalidate A
	cpu->r.AROF = 0;
}

/*
 * Return a Mark Stack Control Word from current processor state
 */
WORD48 buildMSCW(CPU *cpu)
{
	return	INIT_MSCW |
		((WORD48)cpu->r.F << SHFT_MSCWrF) |
		((WORD48)cpu->r.SALF <<	SHFT_MSCWSALF) |
		((WORD48)cpu->r.MSFF <<	SHFT_MSCWMSFF) |
		((WORD48)cpu->r.R << SHFT_MSCWrR);
}

/*
 * Set	processor state	from fields of the Mark	Stack Control
 * Word	in the "word" parameter
 */
void applyMSCW(CPU *cpu, WORD48	word)
{
	cpu->r.F = (word & MASK_MSCWrF)	>> SHFT_MSCWrF;
	cpu->r.SALF = (word & MASK_MSCWSALF) >>	SHFT_MSCWSALF;
	cpu->r.MSFF = (word & MASK_MSCWMSFF) >>	SHFT_MSCWMSFF;
	cpu->r.R = (word & MASK_MSCWrR)	>> SHFT_MSCWrR;
}

/*
 * Return a Return Control Word	from the current processor state
 */
WORD48 buildRCW(CPU *cpu, BIT descriptorCall)
{
	return	INIT_RCW |
		((WORD48)cpu->r.C << SHFT_RCWrC) |
		((WORD48)cpu->r.F << SHFT_RCWrF) |
		((WORD48)cpu->r.K << SHFT_RCWrK) |
		((WORD48)cpu->r.G << SHFT_RCWrG) |
		((WORD48)cpu->r.L << SHFT_RCWrL) |
		((WORD48)cpu->r.V << SHFT_RCWrV) |
		((WORD48)cpu->r.H << SHFT_RCWrH) |
		((WORD48)descriptorCall	<< SHFT_RCWTYPE);
}

/*
 * Set processor state from fields of the Return Control Word in
 * the "word" parameter. If "inline" is	truthy,	C & L are NOT restored from
 * the RCW. Returns the	state of the OPDC/DESC bit [2:1]
 */
BIT applyRCW(CPU *cpu, WORD48 word, BIT	in_line)
{
	if (!in_line) {
		cpu->r.C = (word & MASK_RCWrC) >> SHFT_RCWrC;
		cpu->r.L = (word & MASK_RCWrL) >> SHFT_RCWrL;
		cpu->r.PROF = false;	// require fetch at SECL
	}
	cpu->r.F = (word & MASK_RCWrF) >> SHFT_RCWrF;
	cpu->r.K = (word & MASK_RCWrK) >> SHFT_RCWrK;
	cpu->r.G = (word & MASK_RCWrG) >> SHFT_RCWrG;
	cpu->r.V = (word & MASK_RCWrV) >> SHFT_RCWrV;
	cpu->r.H = (word & MASK_RCWrH) >> SHFT_RCWrH;
	return (word & MASK_RCWTYPE) >>	SHFT_RCWTYPE;
}

/*
 * OPDC, the moral equivalent of "load accumulator" on lesser
 * machines. Assumes the syllable has already loaded a word into A.
 * See Figures 6-1, 6-3, and 6-4 in the	B5500 Reference	Manual
 */
void operandCall(CPU *cpu)
{
	WORD48	aw = cpu->r.A;		// local copy of A reg value
	BIT	interrupted = 0;	// interrupt occurred

//printf("descriptorCall: A=%016llo->",	cpu->r.A);
	// If A	contains a simple operand, just	leave it there,	otherwise...
	if (aw & MASK_FLAG) {
		// It's	not a simple operand
		switch ((aw & MASK_TYPE) >> SHFT_TYPE) { // aw.[1:3]
		case 2:	// CODE=0, PBIT=1, XBIT=0
		case 3:	// CODE=0, PBIT=1, XBIT=1
			// Present data	descriptor: see	if it must be indexed
			if (aw & MASK_DDWC) { // aw.[8:10]
				interrupted = indexDescriptor(cpu);
				// else	descriptor is already indexed (word count 0)
			}
			if (!interrupted) {
				cpu->r.M = cpu->r.A & MASKMEM;
				loadAviaM(cpu);	// A = [M]
				if ((cpu->r.A &	MASK_FLAG) && cpu->r.NCSF) { //	Flag bit is set
					cpu->r.I = (cpu->r.I & 0x0F) | 0x80; //	set I08: flag-bit interrupt
					signalInterrupt(cpu);
					// B5500DumpState("Flag	Bit: OPDC"); //	<<< DEBUG >>>
				}
			}
			break;
		case 7:	//  CODE=1, PBIT=1, XBIT=1
			// Present program descriptor
			enterSubroutine(cpu, false);
			break;
		case 0:	// CODE=0, PBIT=0, XBIT=0
		case 1:	// CODE=0, PBIT=0, XBIT=1
		case 5:	// CODE=1, PBIT=0, XBIT=1
			// Absent data or program descriptor
			if (cpu->r.NCSF) {
				cpu->r.I = (cpu->r.I & 0x0F) | 0x70; //	set I05/6/7: p-bit
				signalInterrupt(cpu);
				// else	if Control State, we're	done
			}
			break;
		default: // cases 4, 6	// CODE=1, PBIT=0/1, XBIT=0
			// Miscellaneous control word -- leave as is
			break;
		}
	}
}

/*
 * DESC, the moral equivalent of "load address"	on lesser machines.
 * Assumes the syllable	has already loaded a word into A, and that the
 * address of that word	is in M.
 * See Figures 6-2, 6-3, and 6-4 in the	B5500 Reference	Manual
 */
void descriptorCall(CPU	*cpu)
{
	WORD48	aw = cpu->r.A;		// local copy of A reg value
	BIT	interrupted = 0;	// interrupt occurred
//printf("descriptorCall: A=%016llo->",	cpu->r.A);
	if (!(aw & MASK_FLAG)) {
		// It's	a simple operand
//printf("operand");
		cpu->r.A = cpu->r.M | (MASK_FLAG | MASK_PBIT);
	} else {
		// It's	not a simple operand
		switch ((aw & MASK_TYPE) >> SHFT_TYPE) { // aw.[1:3]
		case 2:	// CODE=0, PBIT=1, XBIT=0
		case 3:	// CODE=0, PBIT=1, XBIT=1
//printf("present data");
			// Present data	descriptor: see	if it must be indexed
			if (aw & MASK_DDWC) { // aw.[8:10]
				interrupted = indexDescriptor(cpu);
				// else	descriptor is already indexed (word count 0)
				if (!interrupted) {
					// set word count to zero
					cpu->r.A &= ~MASK_DDWC;
				}
				// else	descriptor is already indexed (word count 0)
			}
			break;
		case 7:	//  CODE=1, PBIT=1, XBIT=1
//printf("present program");
			// Present program descriptor
			enterSubroutine(cpu, true);
			break;
		case 0:	// CODE=0, PBIT=0, XBIT=0
		case 1:	// CODE=0, PBIT=0, XBIT=1
		case 5:	// CODE=1, PBIT=0, XBIT=1
//printf("absent program/data");
			// Absent data or program descriptor
			if (cpu->r.NCSF) {
				cpu->r.I = (cpu->r.I & 0x0F) | 0x70; //	set I05/6/7: p-bit
				signalInterrupt(cpu);
				// else	if Control State, we're	done
			}
			break;
		default: // cases 4, 6	// CODE=1, PBIT=0/1, XBIT=0
//printf("misc");
			// Miscellaneous control word
			cpu->r.A = cpu->r.M | (MASK_FLAG | MASK_PBIT);
			break;
		}
	}
//printf("\n");
}

/*
 * Enters a subroutine via the present Program Descriptor in A as part
 * of an OPDC or DESC syllable.	Also handles accidental	entry
 */
void enterSubroutine(CPU *cpu, BIT descriptorCall)
{
	WORD48	aw = cpu->r.A;	// local copy of word in A reg
	BIT	arg = (aw & MASK_PCWARGS) >> SHFT_PCWARGS;
	BIT	mode = (aw & MASK_PCWMODE) >> SHFT_PCWMODE;
//printf("enterSubroutine: MSFF=%u\n", cpu->r.MSFF);
	if (arg	&& !cpu->r.MSFF) {
		// just	leave the Program Descriptor on	TOS
	} else if (mode	&& !arg) {
		// ditto
	} else {
		// Now we are really going to enter the	subroutine
		adjustBEmpty(cpu);
		if (!arg) {
			// Accidental entry -- mark the	stack
			cpu->r.B = buildMSCW(cpu);
			cpu->r.BROF = true;
			adjustBEmpty(cpu);
			cpu->r.F = cpu->r.S;
		}

		// Push	a RCW
		cpu->r.B = buildRCW(cpu, descriptorCall);
		cpu->r.BROF = true;
		adjustBEmpty(cpu);

		// Fetch the first word	of subroutine code
		cpu->r.C = (aw & MASK_PCWADDR) >> SHFT_PCWADDR;
		cpu->r.L = 0;
		// require fetch at SECL
		cpu->r.PROF = false;

		// Fix up the rest of the registers
		if (arg) {
			cpu->r.F = cpu->r.S;
		} else {
			cpu->r.F = (aw & MASK_PCWrF) >>	SHFT_PCWrF;
			// aw.[18:15]
		}
		cpu->r.AROF = false;
		cpu->r.BROF = false;
		cpu->r.SALF = true;
		cpu->r.MSFF = false;
		if (mode) {
			cpu->r.CWMF = 1;
			cpu->r.R = 0;
			cpu->r.X = fieldInsert(cpu->r.X, 18, 15, cpu->r.S);
			cpu->r.S = 0;
		}
	}
}

/*
 * Exits a subroutine by restoring the processor state from RCW	and MSCW words
 * in the stack. "inline" indicates the	C & L registers	are NOT	restored from the
 * RCW.	The RCW	is assumed to be in the	B register, pointing to	the MSCW.
 * The A register is not affected by cpu routine. If SALF & MSFF bits in the MSCW
 * are set, link back through the MSCWs	until one is found that	has either bit not
 * set,	and store that MSCW at [R]+7. This is the last prior MSCW that actually
 * points to a RCW, thus skipping over any pending subroutine calls that are still
 * building their parameters in	the stack. Returns results as follows:
 * 0 = entered by OPDC
 * 1 = entered by DESC
 * 2 = flag bit	interrupt set, terminate operator
 */
int exitSubroutine(CPU *cpu, int in_line)
{
	int	result;

	if (!(cpu->r.B & MASK_FLAG)) {
		// flag	bit not	set
		result = 2;
		if (cpu->r.NCSF) {
			cpu->r.I = (cpu->r.I & 0x0F) | 0x80; //	set I08: flag-bit
			signalInterrupt(cpu);
		}
	} else {
		// flag	bit is set
		result = applyRCW(cpu, cpu->r.B, in_line);
		cpu->r.X = cpu->r.B & MASK_MANTISSA;
		// save	F setting from RCW to restore S	at end
		cpu->r.S = cpu->r.F;
		loadBviaS(cpu);	// B = [S], fetch the MSCW
		applyMSCW(cpu, cpu->r.B);

		if (cpu->r.MSFF	&& cpu->r.SALF)	{
			cpu->r.Q06F = true; // set Q06F, not used except for display
			do {
				cpu->r.S = (cpu->r.B & MASK_MSCWrF) >> SHFT_MSCWrF;
				loadBviaS(cpu);	// B = [S], fetch prior	MSCW
			} while	(cpu->r.B & MASK_MSCWMSFF); // MSFF
			cpu->r.S = (cpu->r.R<<6) + 7;
			storeBviaS(cpu); // [S]	= B, store last	MSCW at	[R]+7
		}
		cpu->r.S = ((cpu->r.X &	MASK_MSCWrF) >>	SHFT_MSCWrF) - 1;
		cpu->r.BROF = false;
	}
//printf("exitSubroutine: %d\n", result);
	return result;
}
