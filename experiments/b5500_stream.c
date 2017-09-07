/***********************************************************************
* b5500emulator
************************************************************************
* Copyright (c)	2016, Reinhard Meyer, DL5UY
* Licensed under the MIT License,
*	see LICENSE
* based	on (C) work by Nigel Williams and Paul Kimpel
* see: https://github.com/pkimpel/retro-b5500
************************************************************************
* character mode
************************************************************************
* 2016-02-1921	R.Meyer
*   Converted Paul's work from Javascript to C
* 2017-07-17  R.Meyer
*   changed "this" to "cpu" to avoid errors when using g++
*   changed "compl" to "compla"	to avoid errors	when using g++
***********************************************************************/

#include "b5500_common.h"

// index by BIC	to get collation value
const WORD6 collation[64] = {
	53, 54,	55, 56,	57, 58,	59, 60,		// @00:	0 1 2 3	4 5 6 7
	61, 62,	19, 20,	63, 21,	22, 23,		// @10:	8 9 # @	? : > }
	24, 25,	26, 27,	28, 29,	30, 31,		// @20:	+ A B C	D E F G
	32, 33,	 1,  2,	 6,  3,	 4,  5,		// @30:	H I . [	& ( < ~
	34, 35,	36, 37,	38, 39,	40, 41,		// @40:	| J K L	M N O P
	42, 43,	 7,  8,	12,  9,	10, 11,		// @50:	Q R $ *	- ) ; {
	 0, 13,	45, 46,	47, 48,	49, 50,		// @60:	_ / S T	U V W X	 (_ = blank)
	51, 52,	14, 15,	44, 16,	17, 18};	// @70:	Y Z , %	! = ] "

/*
 * Generally
 *
 * M/C Source address
 * A/P Source word
 * G Source char index in word 0..7 (for display in Y)
 * H Source bit	index in char 0..5
 *
 * S Destination address
 * B Destination word
 * K Destination char index in word 0..7 (for display in Z)
 * V Destination bit index in char 0..5
 */

/*
 * Adjusts the character-mode source pointer to	the next character
 * boundary, as	necessary. If the adjustment crosses a word boundary,
 * AROF	is reset to force reloading later at the new source address
 */
void streamAdjustSourceChar(CPU	*cpu)
{
	if (cpu->r.H > 0) {
		// not at bit position 0
		// reset bit position
		cpu->r.H = 0;
		// more	to next	char
		if (cpu->r.G < 7) {
			// in same word
			++cpu->r.G;
		} else {
			// in next word
			cpu->r.G = 0;
			// make	sure its loaded	next
			cpu->r.AROF = false;
			++cpu->r.M;
		}
	}
}

/*
 * Adjusts the character-mode destination pointer to the next character
 * boundary, as	necessary. If the adjustment crosses a word boundary and
 * BROF	is set,	B is stored at S before	S is incremented and BROF is reset
 * to force reloading later at the new destination address
 */
void streamAdjustDestChar(CPU *cpu)
{
	if (cpu->r.V > 0) {
		// not at bit position 0
		// reset bit position
		cpu->r.V = 0;
		// more	to next	char
		if (cpu->r.K < 7) {
			// in same word
			++cpu->r.K;
		} else {
			// in next word
			cpu->r.K = 0;
			// current word	touched?
			if (cpu->r.BROF) {
				// store it
				storeBviaS(cpu);
				cpu->r.BROF = false;
			}
			++cpu->r.S;
		}
	}
}

/*
 * Compares source characters to destination characters	according to the
 * processor collating sequence.
 * "count" is the number of source characters to process.
 * The result of the comparison	is left	in two flip-flops:
 * Q03F=1: an inequality was detected
 * TFFF=1: the inequality was source > destination
 * If the two strings are equal, Q03F and TFFF will both be zero. Once an
 * inequality is encountered, Q03F will	be set to 1 and	TFFF (also known as
 * MSFF) will be set based on the nature of inequality.	After cpu point, the
 * processor merely advances its address pointers to exhaust the count and does
 * not fetch additional	words from memory. Note	that the processor uses	Q04F to
 * inhibit storing the B register at the end of	a word boundary. This store
 * may be required only	for the	first word in the destination string, if B may
 * have	been left in an	updated	state by a prior syllable
 */
void compareSourceWithDest(CPU *cpu, unsigned count, BIT numeric)
{
	unsigned	aBit;	// A register bit nr
	WORD48		aw;		// current A register word
	unsigned	bBit;	// B register bit nr
	WORD48		bw;		// current B register word
	BIT			Q03F = cpu->r.Q03F; // local copy of Q03F: inequality detected
	BIT			Q04F = cpu->r.Q04F; // local copy of Q04F: B not dirty
	unsigned	yc = 0;	// local Y register
	unsigned	zc = 0;	// local Z register

	cpu->r.TFFF = false;
	streamAdjustSourceChar(cpu);
	streamAdjustDestChar(cpu);
	if (count) {
		if (cpu->r.BROF) {
			if (cpu->r.K ==	0) {
				// set Q04F -- at start	of word, no need to store B later
				Q04F = true;
			}
		} else {
			loadBviaS(cpu);	// B = [S]
			// set Q04F -- just loaded B, no need to store it later
			Q04F = true;
		}
		if (!cpu->r.AROF) {
			loadAviaM(cpu);	// A = [M]
		}

		// setting Q06F	and saving the count in	H & V is only significant if cpu
		// routine is executed as part of Field	Add (FAD) or Field Subtract (FSU).
		cpu->r.Q06F = true; // set Q06F
		cpu->r.H = count >> 3;
		cpu->r.V = count & 7;

		aBit = cpu->r.G*6;	// A-bit number
		aw = cpu->r.A;
		bBit = cpu->r.K*6;	// B-bit number
		bw = cpu->r.B;
		do {
			++cpu->cycleCount; // approximate the timing
			if (Q03F) {
				// inequality already detected -- just count down
				if (count >= 8)	{
					count -= 8;
					if (!Q04F) {
						// test	Q04F to	see if B may be	dirty
						storeBviaS(cpu); // [S]	= B
						// set Q04F so we won't	store B	anymore
						Q04F = true;
					}
					cpu->r.BROF = false;
					++cpu->r.S;
					cpu->r.AROF = false;
					++cpu->r.M;
				} else {
					--count;
					if (cpu->r.K < 7) {
						++cpu->r.K;
					} else {
						if (!Q04F) {
							// test	Q04F to	see if B may be	dirty
							storeBviaS(cpu); // [S]	= B
							// set Q04F so we won't	store B	anymore
							Q04F = true;
						}
						cpu->r.K = 0;
						cpu->r.BROF = false;
						++cpu->r.S;
					}
					if (cpu->r.G < 7) {
						++cpu->r.G;
					} else {
						cpu->r.G = 0;
						cpu->r.AROF = false;
						++cpu->r.M;
					}
				}
			} else {
				// strings still equal -- check	cpu character
				if (numeric) {
					yc = fieldIsolate(aw, aBit+2, 4);
					zc = fieldIsolate(bw, bBit+2, 4);
				} else {
					yc = fieldIsolate(aw, aBit, 6);
					zc = fieldIsolate(bw, bBit, 6);
				}
				if (yc != zc) {
					// set Q03F to stop further comparison
					Q03F = true;
					if (numeric) {
						cpu->r.TFFF = yc > zc ?	1 : 0;
					} else {
						cpu->r.TFFF = collation[yc] > collation[zc] ? 1	: 0;
					}
				} else {
					// strings still equal -- advance to next character
					--count;
					if (bBit < 42) {
						bBit +=	6;
						++cpu->r.K;
					} else {
						bBit = 0;
						cpu->r.K = 0;
						// test	Q04F to	see if B may be	dirty
						if (!Q04F) {
							storeBviaS(cpu); // [S]	= B
							// set Q04F so we won't	store B	anymore
							Q04F = true;
						}
						++cpu->r.S;
						if (count > 0) {
							loadBviaS(cpu);	// B = [S]
							bw = cpu->r.B;
						} else {
							cpu->r.BROF = false;
						}
					}
					if (aBit < 42) {
						aBit +=	6;
						++cpu->r.G;
					} else {
						aBit = 0;
						cpu->r.G = 0;
						++cpu->r.M;
						if (count > 0) {
							loadAviaM(cpu);	// A = [M]
							aw = cpu->r.A;
						} else {
							cpu->r.AROF = false;
						}
					}
				}
			}
		} while	(count);

		cpu->r.Q03F = Q03F;
		cpu->r.Q04F = Q04F;
		cpu->r.Y = yc;		// for display only
		cpu->r.Z = zc;		// for display only
	}
}

/*
 * Handles the Field Add (FAD) or Field	Subtract (FSU) syllables.
 * "count" indicates the length	of the fields to be operated upon.
 * "adding" will be false if cpu call is for FSU, otherwise it's for FAD
 */
void fieldArithmetic(CPU *cpu, unsigned	count, BIT adding)
{
	unsigned	aBit;		// A register bit nr
	WORD48		aw;		// current A register word
	unsigned	bBit;		// B register bit nr
	WORD48		bw;		// current B register word
	BIT		carry =	false;	// carry/borrow	bit
	BIT		compla = false;	// complement addition (i.e., subtract the digits)
	BIT		TFFF;		// local copy of TFFF/TFFF
	BIT		Q03F;		// local copy of Q03F
	BIT		resultNegative;	// sign	of result is negative
	unsigned	sd;		// digit sum
	BIT		ycompl = false;	// complement source digits
	unsigned	yd;		// source digit
	BIT		zcompl = false;	// complement destination digits
	unsigned	zd;		// destination digit

	compareSourceWithDest(cpu, count, true);
	cpu->cycleCount	+= 2;	// approximate the timing thus far
	if (cpu->r.Q06F) {	// Q06F	=> count > 0, so there's characters to add
		cpu->r.Q06F = false;
		cpu->r.Q04F = false;	// reset Q06F and Q04F
		TFFF = cpu->r.TFFF;	// get TFFF as a Boolean
		Q03F = cpu->r.Q03F;	// get Q03F as a Boolean

		// Back	down the pointers to the last characters of their respective fields
		if (cpu->r.K > 0) {
			--cpu->r.K;
		} else {
			cpu->r.K = 7;
			cpu->r.BROF = false;
			--cpu->r.S;
		}
		if (cpu->r.G > 0) {
			--cpu->r.G;
		} else {
			cpu->r.G = 7;
			cpu->r.AROF = false;
			--cpu->r.M;
		}

		if (!cpu->r.BROF) {
			loadBviaS(cpu);	// B = [S]
		}
		if (!cpu->r.AROF) {
			loadAviaM(cpu);	// A = [M]
		}

		cpu->r.Q08F = true; // set Q08F	(for display only)
		aBit = cpu->r.G*6; // A-bit number
		aw = cpu->r.A;
		bBit = cpu->r.K*6; // B-bit number
		bw = cpu->r.B;
		yd = fieldIsolate(aw, aBit, 2) == 2 ? 2	: 0; //	source sign
		zd = fieldIsolate(bw, bBit, 2) == 2 ? 2	: 0; //	dest sign
		compla = (yd ==	zd ? !adding : adding);	// determine if	complement needed
		resultNegative = !( // determine sign of result
			(zd == 0 && !compla) ||
			(zd == 0 && Q03F && !TFFF) ||
			(zd != 0 && compla && Q03F && TFFF) ||
			(compla	&& !Q03F));
		if (compla) {
			cpu->r.Q07F = true;
			cpu->r.Q02F = true; // set Q07F	and Q02F (for display only)
			carry =	true; // preset	the carry/borrow bit (Q07F)
			if (TFFF) {
				cpu->r.Q04F = true; // set Q04F	(for display only)
				zcompl = true;
			} else {
				ycompl = true;
			}
		}

		cpu->cycleCount	+= 4;
		do {
			--count;
			cpu->cycleCount	+= 2;
			yd = fieldIsolate(aw, aBit+2, 4); // get the source digit
			zd = fieldIsolate(bw, bBit+2, 4); // get the dest digit
			sd = (ycompl ? 9-yd : yd) + (zcompl ? 9-zd : zd) + carry; // develop binary digit sum
			if (sd <= 9) {
				carry =	false;
			} else {
				carry =	true;
				sd -= 10;
			}
			if (resultNegative) {
				sd += 0x20; // set sign	(BA) bits in char to binary 10
				resultNegative = false;
			}

			bw = fieldInsert(bw, bBit, 6, sd);

			if (count == 0)	{
				cpu->r.B = bw;
				storeBviaS(cpu); // [S]	= B, store final dest word
			} else {
				if (bBit > 0) {
					bBit -=	6;
					--cpu->r.K;
				} else {
					bBit = 42;
					cpu->r.K = 7;
					cpu->r.B = bw;
					storeBviaS(cpu); // [S]	= B
					--cpu->r.S;
					loadBviaS(cpu);	// B = [S]
					bw = cpu->r.B;
				}
				if (aBit > 0) {
					aBit -=	6;
					--cpu->r.G;
				} else {
					aBit = 42;
					cpu->r.G = 7;
					--cpu->r.M;
					loadAviaM(cpu);	// A = [M]
					aw = cpu->r.A;
				}
			}
		} while	(count);

		// Now restore the character pointers
		count =	cpu->r.H*8 + cpu->r.V;
		while (count >=	8) {
			count -= 8;
			++cpu->cycleCount;
			++cpu->r.S;
			++cpu->r.M;
		}
		cpu->cycleCount	+= count;
		while (count > 0) {
			--count;
			if (cpu->r.K < 7) {
				++cpu->r.K;
			} else {
				cpu->r.K = 0;
				++cpu->r.S;
			}
			if (cpu->r.G < 7) {
				++cpu->r.G;
			} else {
				cpu->r.G = 0;
				++cpu->r.M;
			}
		}
		cpu->r.A = aw;
		cpu->r.B = bw;
		cpu->r.AROF = cpu->r.BROF = false;
		cpu->r.H = cpu->r.V = cpu->r.N = 0;
		cpu->r.TFFF = (compla ?	1-carry	: carry); // TFFF/TFFF = overflow indicator
	}
}

/*
 * Streams a pattern of	bits to	the destination	specified by S,	K, and V,
 * as supplied by the 48-bit "mask" argument. Partial words are	filled from
 * the low-order bits of the mask. Implements the guts of Character-Mode
 * Bit Set (XX64) and Bit Reset	(XX65).	Leaves the registers pointing at the
 * next	bit in sequence
 */
void streamBitsToDest(CPU *cpu,	unsigned count,	WORD48 mask)
{
	unsigned	bn;	// field starting bit number
	unsigned	fl;	// field length	in bits

	if (count) {
		cpu->cycleCount	+= count;
		if (!cpu->r.BROF) {
			loadBviaS(cpu);	// B = [S]
		}
		do {
			bn = cpu->r.K*6	+ cpu->r.V; // starting	bit nr.
			fl = 48-bn; // bits remaining in the word
			if (count < fl)	{
				fl = count;
			}
			if (fl < 48) {
				cpu->r.B = fieldInsert(cpu->r.B, bn, fl, mask);
			} else {
				cpu->r.B = mask; // set	the whole word
			}
			count -= fl; //	decrement by number of bits modified
			bn += fl; // increment the starting bit	nr.
			if (bn < 48) {
				cpu->r.V = bn %	6;
				cpu->r.K = (bn - cpu->r.V)/6;
			} else {
				cpu->r.K = cpu->r.V = 0;
				storeBviaS(cpu); // [S]	= B, save the updated word
				++cpu->r.S;
				if (count > 0) {
					loadBviaS(cpu);	// B = [S], fetch next word in sequence
				} else {
					cpu->r.BROF = false;
				}
			}
		} while	(count);
	}
}

/*
 * Implements the TRP (Transfer	Program	Characters)
 * character-mode syllable
 */
void streamProgramToDest(CPU *cpu, unsigned count)
{
	unsigned	bBit;	// B register bit nr
	WORD48		bw;		// current B register value
	unsigned	c;		// current character
	unsigned	pBit;	// P register bit nr
	WORD48		pw;		// current P register value

	streamAdjustDestChar(cpu);
	if (count) { //	count >	0
		if (!cpu->r.BROF) {
			loadBviaS(cpu);	// B = [S]
		}
		if (!cpu->r.PROF) {
			loadPviaC(cpu);	// fetch the program word, if necessary
		}
		cpu->cycleCount	+= count; // approximate the timing
		pBit = (cpu->r.L*2 + (count % 2))*6; //	P-reg bit number
		pw = cpu->r.P;
		bBit = cpu->r.K*6; // B-reg bit	number
		bw = cpu->r.B;
		do {
			c = fieldIsolate(pw, pBit, 6);
			bw = fieldInsert(bw, bBit, 6, c);
			--count;
			if (bBit < 42) {
				bBit +=	6;
				++cpu->r.K;
			} else {
				bBit = false;
				cpu->r.K = 0;
				cpu->r.B = bw;
				storeBviaS(cpu); // [S]	= B
				++cpu->r.S;
				if (count > 0 && count < 8) { // only need to load B if	a partial word is left
					loadBviaS(cpu);	// B = [S]
					bw = cpu->r.B;
				} else {
					cpu->r.BROF = false;
				}
			}
			if (pBit < 42) {
				pBit +=	6;
				if (!(count % 2)) {
					++cpu->r.L;
				}
			} else {
				pBit = false;
				cpu->r.L = 0;
				++cpu->r.C;
				loadPviaC(cpu);	// P = [C]
				pw = cpu->r.P;
			}
		} while	(count);
		cpu->r.B = bw;
		cpu->r.Y = c;	// for display purposes	only
	}
}

/*
 * Transfers character transfers from source to	destination for	the TRS	syllable.
 * "count" is the number of source characters to transfer
 */
void streamCharacterToDest(CPU *cpu, unsigned count)
{
	unsigned	aBit;	// A register bit nr
	WORD48		aw;		// current A register word
	unsigned	bBit;	// B register bit nr
	WORD48		bw;		// current B register word
	unsigned	c;		// current character

	streamAdjustSourceChar(cpu);
	streamAdjustDestChar(cpu);
	if (count) {
		if (!cpu->r.BROF) {
			loadBviaS(cpu);	// B = [S]
		}
		if (!cpu->r.AROF) {
			loadAviaM(cpu);	// A = [M]
		}
		cpu->cycleCount	+= 10 +	count*2; // approximate	the timing
		aBit = cpu->r.G*6; // A-bit number
		aw = cpu->r.A;
		bBit = cpu->r.K*6; // B-bit number
		bw = cpu->r.B;
		do {
			c = fieldIsolate(aw, aBit, 6);
			bw = fieldInsert(bw, bBit, 6, c);
			--count;
			if (bBit < 42) {
				bBit +=	6;
				++cpu->r.K;
			} else {
				bBit = false;
				cpu->r.K = 0;
				cpu->r.B = bw;
				storeBviaS(cpu); // [S]	= B
				++cpu->r.S;
				if (count > 0 && count < 8) { // only need to load B if	a partial word is left
					loadBviaS(cpu);	// B = [S]
					bw = cpu->r.B;
				} else {
					cpu->r.BROF = false;
				}
			}
			if (aBit < 42) {
				aBit +=	6;
				++cpu->r.G;
			} else {
				aBit = false;
				cpu->r.G = 0;
				++cpu->r.M;
				if (count > 0) { // only need to load A	if there's more	to do
					loadAviaM(cpu);	// A = [M]
					aw = cpu->r.A;
				} else {
					cpu->r.AROF = false;
				}
			}
		} while	(count);
		cpu->r.B = bw;
		cpu->r.Y = c; // for display purposes only
	}
}

/*
 * Transfers from source to destination	for the	TRN and	TRZ syllables. "count"
 * is the number of source characters to transfer. If transferring numerics and	the
 * low-order character has a negative sign (BA=10), sets TFFF=1
 */
void streamNumericToDest(CPU *cpu, unsigned count, unsigned zones)
{
	unsigned	aBit;	// A register bit nr
	WORD48		aw;		// current A register word
	unsigned	bBit;	// B register bit nr
	WORD48		bw;		// current B register word
	unsigned	c;		// current character

	streamAdjustSourceChar(cpu);
	streamAdjustDestChar(cpu);
	if (count) {
		if (!cpu->r.BROF) {
			loadBviaS(cpu);	// B = [S]
		}
		if (!cpu->r.AROF) {
			loadAviaM(cpu);	// A = [M]
		}
		if (zones) { //	approximate the	timing
			cpu->cycleCount	+= 5 + count*4;
		} else {
			cpu->cycleCount	+= 10 +	count*3;
		}

		aBit = cpu->r.G*6; // A-bit number
		aw = cpu->r.A;
		bBit = cpu->r.K*6; // B-bit number
		bw = cpu->r.B;
		do {
			c = fieldIsolate(aw, aBit, 6);
			if (zones) { //	transfer only the zone portion of the char
				bw = fieldInsert(bw, bBit, 2, c	>> 4);
			} else { // transfer the numeric portion with a	zero zone
				bw = fieldInsert(bw, bBit, 6, (c & 0x0F));
			}
			--count;
			if (bBit < 42) {
				bBit +=	6;
				++cpu->r.K;
			} else {
				bBit = false;
				cpu->r.K = 0;
				cpu->r.B = bw;
				storeBviaS(cpu); // [S]	= B
				++cpu->r.S;
				if (count > 0) {
					loadBviaS(cpu);	// B = [S]
					bw = cpu->r.B;
				} else {
					cpu->r.BROF = false;
				}
			}
			if (aBit < 42) {
				aBit +=	6;
				++cpu->r.G;
			} else {
				aBit = false;
				cpu->r.G = 0;
				++cpu->r.M;
				if (count > 0) { // only need to load A	if there's more	to do
					loadAviaM(cpu);	// A = [M]
					aw = cpu->r.A;
				} else {
					cpu->r.AROF = false;
				}
			}
		} while	(count);
		cpu->r.B = bw;
		cpu->r.Y = c; // for display purposes only
		if (!zones && (c & 0x30) == 0x20) {
			cpu->r.TFFF = true; // last char had a negative	sign
		}
	}
}

/*
 * Implements the TBN (Transfer	Blanks for Non-Numeric)	syllable, which	is
 * generally used to suppress leading zeroes in	numeric	strings. Transfers blanks
 * to the destination under control of the count as long as the	destination characters
 * are not in the range	"1"-"9". Sets TFFF (MSFF) true if the count is exhausted.
 * "count" is the maximum number of characters to blank
 */
void streamBlankForNonNumeric(CPU *cpu,	unsigned count)
{
	unsigned	bBit;	// B register bit nr
	WORD48		bw;		// current B register word
	unsigned	c;		// current destination character

	cpu->r.TFFF = true; // assume the count	will be	exhausted
	streamAdjustDestChar(cpu);
	if (count) {
		if (!cpu->r.BROF) {
			loadBviaS(cpu);	// B = [S]
		}
		bBit = cpu->r.K*6; // B-bit number
		bw = cpu->r.B;
		do {
			cpu->cycleCount	+= 2; // approximate the timing
			c = fieldIsolate(bw, bBit, 6);
			if (c >	0 && c <= 9) {
				// is numeric and non-zero: stop blanking
				cpu->r.TFFF = false;
				// set Q03F (display only)
				cpu->r.Q03F = true;
				// terminate, pointing at cpu char
				break;
			} else {
				bw = fieldInsert(bw, bBit, 6, 0x30); //	replace	with blank
				--count;
				if (bBit < 42) {
					bBit +=	6;
					++cpu->r.K;
				} else {
					bBit = false;
					cpu->r.K = 0;
					cpu->r.B = bw;
					storeBviaS(cpu); // [S]	= B
					++cpu->r.S;
					if (count > 0) {
						loadBviaS(cpu);	// B = [S]
						bw = cpu->r.B;
					} else {
						cpu->r.BROF = false;
					}
				}
			}
		} while	(count);
		cpu->r.B = bw;
		cpu->r.Z = c; // for display purposes only
	}
}

/*
 * Converts a signed-numeric character field at	the source M & G address
 * from	decimal	to binary, storing the resulting word at the S address and then
 * incrementing	S. Normally, decimal to	binary conversion shouldn't be cpu
 * complex, so we must do it more or less the way the B5500 hardware did, by
 * repeated remainder division (i.e., shifting right) and adjusting the
 * low-order digit by -3 when a	one was	shifted	into the high-order bit	of the
 * low-order digit from	the higher digit locations. The	problem	with doing it a
 * more	direct and efficient way is with digits	that are not in	the range 0-9.
 * Doing it the	hardware way should yield the same (albeit questionable)
 * result. See Section 2.6 in the B5281	Training Manual	for details. This
 * process took	at least 27 clocks on the B5500, so we can afford to be	slow
 * here, too. Note that	a maximum of 8 characters are converted
 */
void streamInputConvert(CPU *cpu, unsigned count)
{
	WORD48	a = 0;		// local working copy of A
	WORD48	b = 0;		// local working copy of B
	WORD48	power =	1;	// A-register shift factor

	streamAdjustSourceChar(cpu);
	if (cpu->r.BROF) {
		storeBviaS(cpu); // [S]	= B
		cpu->r.BROF = false;
	}
	if (cpu->r.K ||	cpu->r.V) { // adjust dest to word boundary
		cpu->r.K = cpu->r.V = 0;
		++cpu->r.S;
	}
	if (count) { //	no conversion if count is zero
		cpu->cycleCount	+= count*2 + 27;
		count =	((count-1) & 0x07) + 1;	// limit the count to 8
		if (!cpu->r.AROF) {
			loadAviaM(cpu);	// A = [M]
		}

		// First, assemble the digits into B as	4-bit BCD
		do {
			b = (b << 4) | ((cpu->r.Y = fieldIsolate(cpu->r.A, cpu->r.G*6, 6)) & 0x0F);
			if (cpu->r.G < 7) {
				++cpu->r.G;
			} else {
				cpu->r.G = 0;
				++cpu->r.M;
				if (count > 1) {
					loadAviaM(cpu);	// A = [M], only if more chars are needed
				} else {
					cpu->r.AROF = false;
				}
			}
		} while	(--count);

		// Then	do the artful shifting to form the binary value	in A
		cpu->r.AROF = false;
		cpu->r.B = b; // for display purposes only
		while (b) {
			if (b &	0x01) {
				a += power;
			}
			power *= 2;
			b >>= 1;

			/*
			 * This	next part is tricky, and was done by a switching network in the	B5500.
			 * When	a 1 bit	is shifted into	the high-order position	of a BCD decade	from its
			 * decade to the left, that bit	has a place value of 8,	but because the	number
			 * is decimal, it should have a	place value of five. Therefore,	in EACH	such
			 * decade, we need to subtract 3 to get	the correct place value. The following
			 * statement constructs	a mask of 3s in	each decade where the high-order bit is
			 * set after the shift above, then subtracts that mask from the	working	B value.
			 * See the discussion in Section 2.6 in	the Training Manual cited above
			 */

			b -= ((b & 0x88888888) >> 3)*3;
		}

		// Finally, fix	up the binary sign and store the result
		if (a) { // zero results have sign bit reset
			if ((cpu->r.Y &	0x30) == 0x20) {
				a |= MASK_SIGNMANT; // set the sign bit
			}
		}
		cpu->r.A = a;
		storeAviaS(cpu); // [S]	= A
		++cpu->r.S;
	}
}

/*
 * Converts the	binary word addressed by M (after word-boundary	adjustment)
 * to decimal BIC at the destination address of	S & K. The maximum number of
 * digits to convert is	8. If the binary value can be represented in "count"
 * digits (or the count	is zero), the true-false FF, TFFF, is set; otherwise it
 * is reset. The sign is stored	in low-order character of the result
 */
void streamOutputConvert(CPU *cpu, unsigned count)
{
	WORD48		a;		// local working copy of A
	WORD48		b = 0;	// local working copy of B
	unsigned	c;		// converted decimal character
	unsigned	d = 0;	// digit counter
	WORD48	power =	1;	// power-of-64 factor for result digits

	cpu->r.TFFF = true; // set TFFF	unless there's overflow
	streamAdjustDestChar(cpu);
	if (cpu->r.BROF) {
		storeBviaS(cpu); // [S]	= B, but leave BROF set
	}
	if (cpu->r.G ||	cpu->r.H) { // adjust source to	word boundary
		cpu->r.G = cpu->r.H = 0;
		cpu->r.AROF = false;
		++cpu->r.M;
	}
	if (count) { //	count >	0
		cpu->cycleCount	+= count*2 + 27;
		if (!cpu->r.AROF) {
			loadAviaM(cpu);	// A = [M]
		}
		count =	((count-1) & 0x07) + 1;	// limit the count to 8
		a = cpu->r.A & MASK_MANTISSA; // get absolute mantissa value, ignore exponent
		if (a) { // mantissa is	non-zero, so conversion	is required
			if (cpu->r.A & MASK_SIGNMANT) {
				// result is negative, so preset the sign in the low-order digit
				b = 040;	// BA'8421=10'0000
			}
			do { //	Convert	the binary value in A to BIC digits in B
				c = a %	10;
				a = (a-c)/10;
				if (c) {
					b += c*power;
				}
				power *= 64;
			} while	(a && (++d < count));
			if (a) {
				// overflow occurred, so reset TFFF
				cpu->r.TFFF = false;
			}
		}
		cpu->r.AROF = false; //	invalidate A
		++cpu->r.M; // and advance to the next source word

		// Finally, stream the digits from A (whose value is still in local b) to the destination
		cpu->r.A = b; // for display purposes only
		loadBviaS(cpu);	// B = [S], restore original value of B
		d = 48 - count*6; // starting bit in A
		do {
			fieldTransfer(&cpu->r.B, cpu->r.K*6, 6,	b, d);
			d += 6;
			if (cpu->r.K < 7) {
				++cpu->r.K;
			} else {
				storeBviaS(cpu); // [S]	= B
				cpu->r.K = 0;
				++cpu->r.S;
				if (count > 1) {
					loadBviaS(cpu);	// B = [S]
				} else {
					cpu->r.BROF = false;
				}
			}
		} while	(--count);
	}
}
