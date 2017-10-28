/***********************************************************************
* b5500emulator
************************************************************************
* Copyright (c) 2016, Reinhard Meyer, DL5UY
* Licensed under the MIT License,
*       see LICENSE
* based on (C) work by Nigel Williams and Paul Kimpel
* see: https://github.com/pkimpel/retro-b5500
************************************************************************
* character mode
************************************************************************
* 2016-02-19 R.Meyer
*   Converted Paul's work from Javascript to C
* 2017-07-17 R.Meyer
*   changed "this" to "cpu" to avoid errors when using g++
*   changed "compl" to "compla" to avoid errors when using g++
* 2017-09-23 R.Meyer
*   some recoding and checking
* 2017-09-30  R.Meyer
*   overhaul of file names
* 2017-10-28  R.Meyer
*   adaption to new CPU structure
***********************************************************************/

#include <stdio.h>
#include "common.h"

/***********************************************************************
* Generally
*
* M/C Source address
* A/P Source word
* G Source char index in word 0..7 (for display in Y)
* H Source bit index in char 0..5
*
* S Destination address
* B Destination word
* K Destination char index in word 0..7 (for display in Z)
* V Destination bit index in char 0..5
***********************************************************************/

/***********************************************************************
* Adjusts the character-mode source pointer to the next character
* boundary, as necessary. If the adjustment crosses a word boundary,
* AROF is reset to force reloading later at the new source address
* VERIFIED against Paul's JavaScript 17-10-16
***********************************************************************/
void streamAdjustSourceChar(CPU *cpu)
{
	if (cpu->rGH/*TODO H*/ > 0) {
		// not at bit position 0
		// reset bit position
		cpu->rGH/*TODO H*/ = 0;
		// move to next char
		if (cpu->rGH/*TODO G*/ < 7) {
			++cpu->rGH/*TODO G*/;
		} else {
			// move to next word
			cpu->rGH/*TODO G*/ = 0;
			++cpu->rM;
			cpu->bAROF = false;
		}
	}
}

/***********************************************************************
* Adjusts the character-mode destination pointer to the next character
* boundary, as necessary. If the adjustment crosses a word boundary and
* BROF is set, B is stored at S before S is incremented and BROF is reset
* to force reloading later at the new destination address
* VERIFIED against Paul's JavaScript 17-10-16
***********************************************************************/
void streamAdjustDestChar(CPU *cpu)
{
	if (cpu->rKV/*TODO V*/ > 0) {
		// not at bit position 0
		// reset bit position
		cpu->rKV/*TODO V*/ = 0;
		// move to next char
		if (cpu->rKV/*TODO K*/ < 7) {
			++cpu->rKV/*TODO K*/;
		} else {
			// move to next word
			cpu->rKV/*TODO K*/ = 0;
			// store current word when touched
			if (cpu->bBROF) {
				// store it
				storeBviaS(cpu);
				cpu->bBROF = false;
			}
			++cpu->rS;
		}
	}
}

/***********************************************************************
* Compares source characters to destination characters according to the
* processor collating sequence.
* "count" is the number of source characters to process.
* The result of the comparison is left in two flip-flops:
* Q03F=1: an inequality was detected
* TFFF=1: the inequality was source > destination
* If the two strings are equal, Q03F and TFFF will both be zero. Once an
* inequality is encountered, Q03F will be set to 1 and TFFF (also known as
* MSFF) will be set based on the nature of inequality. After this point, the
* processor merely advances its address pointers to exhaust the count and does
* not fetch additional words from memory. Note that the processor uses Q04F to
* inhibit storing the B register at the end of a word boundary. This store
* may be required only for the first word in the destination string, if B may
* have been left in an updated state by a prior syllable
* VERIFIED against Paul's JavaScript 17-10-22
***********************************************************************/
void compareSourceWithDest(CPU *cpu, unsigned count, BIT numeric)
{
	// Note: all Q cleared at begin of each execution
	cpu->bTFFF = false;

	streamAdjustSourceChar(cpu);
	streamAdjustDestChar(cpu);

	// only do the next if count > 0
	if (count) {
		// ensure B is filled
		if (cpu->bBROF) {
			// B already full
			if (cpu->rKV/*TODO K*/ == 0) {
				// set Q04F if pointer is at start of word, no need to store B later
				cpu->bQ04F = true;
			}
		} else {
			// fill B
			loadBviaS(cpu); // B = [S]
			// set Q04F -- just loaded B, no need to store it later
			cpu->bQ04F = true;
		}
		// ensure A is filled
		if (!cpu->bAROF) {
		        loadAviaM(cpu); // A = [M]
		}

		// setting Q06F and saving the count in H & V is only significant if this
		// routine is executed as part of Field Add (FAD) or Field Subtract (FSU).
		cpu->bQ06F = true; // set Q06F
		cpu->rGH/*TODO H*/ = count >> 3;
		cpu->rKV/*TODO V*/ = count & 7;

		// loop over count
		do {
		        ++cpu->cycleCount; // approximate the timing
		        if (cpu->bQ03F) {
		                // inequality already detected -- just count down
		                if (count >= 8) {
					// skip a full word - G and K not changed!
		                        count -= 8;
	                                // test Q04F to see if B may be dirty
		                        if (!cpu->bQ04F) {
						// B dirty - save it
		                                storeBviaS(cpu); // [S] = B
		                                // set Q04F so we won't store B anymore
		                                cpu->bQ04F = true;
		                        }
					// mark A and B empty
		                        cpu->bBROF = false;
		                        cpu->bAROF = false;
					// advance to next word
					++cpu->rS;
		                        ++cpu->rM;
		                } else {
					// skip characters
		                        --count;
		                        if (cpu->rKV/*TODO K*/ < 7) {
						// next char
		                                ++cpu->rKV/*TODO K*/;
		                        } else {
						// next word
		                                cpu->rKV/*TODO K*/ = 0;
	                                        // test Q04F to see if B may be dirty
		                                if (!cpu->bQ04F) {
		                                        storeBviaS(cpu); // [S] = B
		                                        // set Q04F so we won't store B anymore
		                                        cpu->bQ04F = true;
		                                }
		                                cpu->bBROF = false;
						++cpu->rS;
		                        }
		                        if (cpu->rGH/*TODO G*/ < 7) {
						// next char
		                                ++cpu->rGH/*TODO G*/;
		                        } else {
						// next word
		                                cpu->rGH/*TODO G*/ = 0;
		                                cpu->bAROF = false;
				                ++cpu->rM;
		                        }
		                }
		        } else {
		                // strings still equal -- compare current characters
				cpu->rY = fieldIsolate(cpu->rA, cpu->rGH/*TODO G*/*6, 6);
				cpu->rZ = fieldIsolate(cpu->rB, cpu->rKV/*TODO K*/*6, 6);
		                if (numeric) {
					// if numeric compare, clip B and A bits
		                        cpu->rY &= 0xf;
		                        cpu->rZ &= 0xf;
		                }
				// now compare
		                if (cpu->rY != cpu->rZ) {
					// inequality detected - set Q03F to stop further comparison
		                        cpu->bQ03F = true;
		                        if (numeric) {
		                                cpu->bTFFF = cpu->rY > cpu->rZ ? true : false;
		                        } else {
		                                cpu->bTFFF = collation[cpu->rY] > collation[cpu->rZ] ? true : false;
		                        }
		                } else {
		                        // strings still equal -- advance to next character
		                        --count;
		                        if (cpu->rKV/*TODO K*/ < 7) {
						// next char
		                                ++cpu->rKV/*TODO K*/;
		                        } else {
						// next word
		                                cpu->rKV/*TODO K*/ = 0;
		                                // test Q04F to see if B may be dirty
		                                if (!cpu->bQ04F) {
		                                        storeBviaS(cpu); // [S] = B
		                                        // set Q04F so we won't store B anymore
		                                        cpu->bQ04F = true;
		                                }
						++cpu->rS;
						// more to compare ?
		                                if (count > 0) {
		                                        loadBviaS(cpu); // B = [S]
		                                } else {
		                                        cpu->bBROF = false;
		                                }
		                        }
		                        if (cpu->rGH/*TODO G*/ < 7) {
		                                // next char
		                                ++cpu->rGH/*TODO G*/;
		                        } else {
		                                // next word
		                                cpu->rGH/*TODO G*/ = 0;
				                ++cpu->rM;
						// more to compare ?
		                                if (count > 0) {
		                                        loadAviaM(cpu); // A = [M]
		                                } else {
		                                        cpu->bAROF = false;
		                                }
		                        }
		                }
		        }
			// check whether S or M went over the limit
			if (cpu->rS >= MAXMEM) {
				prepMessage(cpu);
				printf("compareSourceWithDest S>MAX\n");
				stop(cpu);
			}
			if (cpu->rM >= MAXMEM) {
				prepMessage(cpu);
				printf("compareSourceWithDest M>MAX\n");
				stop(cpu);
			}
		} while (count);
	}
}

/*
 * Handles the Field Add (FAD) or Field Subtract (FSU) syllables.
 * "count" indicates the length of the fields to be operated upon.
 * "adding" will be false if cpu call is for FSU, otherwise it's for FAD
 */
void fieldArithmetic(CPU *cpu, unsigned count, BIT adding)
{
        BIT             carry = false;  // carry/borrow bit
        BIT             compla = false; // complement addition (i.e., subtract the digits)
        BIT             resultNegative; // sign of result is negative
        unsigned        sd;             // digit sum
        BIT             ycompl = false; // complement source digits
        BIT             zcompl = false; // complement destination digits

	// scan the fields to beyond their ends - leaves Q06F set if count > 0
        compareSourceWithDest(cpu, count, true);

        cpu->cycleCount += 2;   // approximate the timing thus far

	// Q06F => count > 0, so there's characters to add
        if (cpu->bQ06F) {
		// reset Q06F and Q04F
                cpu->bQ06F = false;
                cpu->bQ04F = false;
		// results of compareSourceWithDest are TFFF and Q03F

                // Back the pointers to the last characters of their respective fields
                if (cpu->rKV/*TODO K*/ > 0) {
                        --cpu->rKV/*TODO K*/;
                } else {
                        cpu->rKV/*TODO K*/ = 7;
                        cpu->bBROF = false;
                        --cpu->rS;
                }

                if (cpu->rGH/*TODO G*/ > 0) {
                        --cpu->rGH/*TODO G*/;
                } else {
                        cpu->rGH/*TODO G*/ = 7;
                        cpu->bAROF = false;
                        --cpu->rM;
                }

                if (!cpu->bBROF) {
                        loadBviaS(cpu); // B = [S]
                }

                if (!cpu->bAROF) {
                        loadAviaM(cpu); // A = [M]
                }

                cpu->bQ08F = true; // set Q08F (for display only)

                cpu->rY = fieldIsolate(cpu->rA, cpu->rGH/*TODO G*/*6, 2) == 2 ? 2 : 0; // source sign
                cpu->rZ = fieldIsolate(cpu->rB, cpu->rKV/*TODO K*/*6, 2) == 2 ? 2 : 0; // dest sign
                compla = (cpu->rY == cpu->rZ ? !adding : adding); // determine if complement needed

                resultNegative = !( // determine sign of result
                        (cpu->rZ == 0 && !compla) ||
                        (cpu->rZ == 0 && cpu->bQ03F && !cpu->bTFFF) ||
                        (cpu->rZ != 0 && compla && cpu->bQ03F && cpu->bTFFF) ||
                        (compla && !cpu->bQ03F));
                if (compla) {
                        cpu->bQ07F = true;
                        cpu->bQ02F = true; // set Q07F and Q02F (for display only)
                        carry = true; // preset the carry/borrow bit (Q07F)
                        if (cpu->bTFFF) {
                                cpu->bQ04F = true; // set Q04F (for display only)
                                zcompl = true;
                        } else {
                                ycompl = true;
                        }
                }

                cpu->cycleCount += 4;
                do {
                        --count;
                        cpu->cycleCount += 2;
                        cpu->rY = fieldIsolate(cpu->rA, cpu->rGH/*TODO G*/*6+2, 4); // get the source digit
                        cpu->rZ = fieldIsolate(cpu->rB, cpu->rKV/*TODO K*/*6+2, 4); // get the dest digit
                        sd = (ycompl ? 9-cpu->rY : cpu->rY) + (zcompl ? 9-cpu->rZ : cpu->rZ) + carry; // develop binary digit sum
                        if (sd <= 9) {
                                carry = false;
                        } else {
                                carry = true;
                                sd -= 10;
                        }
                        if (resultNegative) {
                                sd += 0x20; // set sign (BA) bits in char to binary 10
                                resultNegative = false;
                        }

                        cpu->rB = fieldInsert(cpu->rB, cpu->rKV/*TODO K*/*6, 6, sd);

                        if (count == 0) {
                                storeBviaS(cpu); // [S] = B, store final dest word
                        } else {
                                if (cpu->rKV/*TODO K*/ > 0) {
                                        --cpu->rKV/*TODO K*/;
                                } else {
                                        cpu->rKV/*TODO K*/ = 7;
                                        storeBviaS(cpu); // [S] = B
                                        --cpu->rS;
                                        loadBviaS(cpu); // B = [S]
                                }
                                if (cpu->rGH/*TODO G*/ > 0) {
                                        --cpu->rGH/*TODO G*/;
                                } else {
                                        cpu->rGH/*TODO G*/ = 7;
                                        --cpu->rM;
                                        loadAviaM(cpu); // A = [M]
                                }
                        }
                } while (count);

                // Now restore the character pointers
                count = cpu->rGH/*TODO H*/*8 + cpu->rKV/*TODO V*/;
                while (count >= 8) {
                        count -= 8;
                        ++cpu->cycleCount;
                        ++cpu->rS;
                        ++cpu->rM;
                }
                cpu->cycleCount += count;
                while (count > 0) {
                        --count;
                        if (cpu->rKV/*TODO K*/ < 7) {
                                ++cpu->rKV/*TODO K*/;
                        } else {
                                cpu->rKV/*TODO K*/ = 0;
                                ++cpu->rS;
                        }
                        if (cpu->rGH/*TODO G*/ < 7) {
                                ++cpu->rGH/*TODO G*/;
                        } else {
                                cpu->rGH/*TODO G*/ = 0;
                                ++cpu->rM;
                        }
                }
                cpu->bAROF = cpu->bBROF = false;
                cpu->rGH/*TODO H*/ = cpu->rKV/*TODO V*/ = cpu->rN = 0;
                cpu->bTFFF = (compla ? 1-carry : carry); // TFFF = overflow indicator
        }
}

/*
 * Streams a pattern of bits to the destination specified by S, K, and V,
 * as supplied by the 48-bit "mask" argument. Partial words are filled from
 * the low-order bits of the mask. Implements the guts of Character-Mode
 * Bit Set (XX64) and Bit Reset (XX65). Leaves the registers pointing at the
 * next bit in sequence
 */
void streamBitsToDest(CPU *cpu, unsigned count, WORD48 mask)
{
        unsigned        bn;     // field starting bit number
        unsigned        fl;     // field length in bits

//printf("sBTD\n");
        if (count) {
                cpu->cycleCount += count;
                if (!cpu->bBROF) {
                        loadBviaS(cpu); // B = [S]
                }
                do {
                        bn = cpu->rKV/*TODO K*/*6 + cpu->rKV/*TODO V*/; // starting bit nr.
                        fl = 48-bn; // bits remaining in the word
                        if (count < fl) {
                                fl = count;
                        }
                        if (fl < 48) {
                                cpu->rB = fieldInsert(cpu->rB, bn, fl, mask);
                        } else {
                                cpu->rB = mask; // set the whole word
                        }
                        count -= fl; // decrement by number of bits modified
                        bn += fl; // increment the starting bit nr.
                        if (bn < 48) {
                                cpu->rKV/*TODO V*/ = bn % 6;
                                cpu->rKV/*TODO K*/ = (bn - cpu->rKV/*TODO V*/)/6;
                        } else {
                                cpu->rKV/*TODO K*/ = cpu->rKV/*TODO V*/ = 0;
                                storeBviaS(cpu); // [S] = B, save the updated word
                                ++cpu->rS;
                                if (count > 0) {
                                        loadBviaS(cpu); // B = [S], fetch next word in sequence
                                } else {
                                        cpu->bBROF = false;
                                }
                        }
                } while (count);
        }
}

/*
 * Implements the TRP (Transfer Program Characters)
 * character-mode syllable
 */
void streamProgramToDest(CPU *cpu, unsigned count)
{
        unsigned        bBit;   // B register bit nr
        WORD48          bw;     // current B register value
        unsigned        c;      // current character
        unsigned        pBit;   // P register bit nr
        WORD48          pw;     // current P register value

//printf("sPTD\n");
        streamAdjustDestChar(cpu);
        if (count) { // count > 0
                if (!cpu->bBROF) {
                        loadBviaS(cpu); // B = [S]
                }
                if (!cpu->bPROF) {
                        loadPviaC(cpu); // fetch the program word, if necessary
                }
                cpu->cycleCount += count; // approximate the timing
                pBit = (cpu->rL*2 + (count % 2))*6; // P-reg bit number
                pw = cpu->rP;
                bBit = cpu->rKV/*TODO K*/*6; // B-reg bit number
                bw = cpu->rB;
                do {
                        c = fieldIsolate(pw, pBit, 6);
                        bw = fieldInsert(bw, bBit, 6, c);
                        --count;
                        if (bBit < 42) {
                                bBit += 6;
                                ++cpu->rKV/*TODO K*/;
                        } else {
                                bBit = false;
                                cpu->rKV/*TODO K*/ = 0;
                                cpu->rB = bw;
                                storeBviaS(cpu); // [S] = B
                                ++cpu->rS;
                                if (count > 0 && count < 8) { // only need to load B if a partial word is left
                                        loadBviaS(cpu); // B = [S]
                                        bw = cpu->rB;
                                } else {
                                        cpu->bBROF = false;
                                }
                        }
                        if (pBit < 42) {
                                pBit += 6;
                                if (!(count % 2)) {
                                        ++cpu->rL;
                                }
                        } else {
                                pBit = false;
                                cpu->rL = 0;
                                ++cpu->rC;
                                loadPviaC(cpu); // P = [C]
                                pw = cpu->rP;
                        }
                } while (count);
                cpu->rB = bw;
                cpu->rY = c;   // for display purposes only
        }
}

/*
 * Transfers character transfers from source to destination for the TRS syllable.
 * "count" is the number of source characters to transfer
 */
void streamCharacterToDest(CPU *cpu, unsigned count)
{
        unsigned        aBit;   // A register bit nr
        WORD48          aw;     // current A register word
        unsigned        bBit;   // B register bit nr
        WORD48          bw;     // current B register word
        unsigned        c;      // current character

//printf("sCTD %u '", count);
        streamAdjustSourceChar(cpu);
        streamAdjustDestChar(cpu);
        if (count) {
                if (!cpu->bBROF) {
                        loadBviaS(cpu); // B = [S]
                }
                if (!cpu->bAROF) {
                        loadAviaM(cpu); // A = [M]
                }
                cpu->cycleCount += 10 + count*2; // approximate the timing
                aBit = cpu->rGH/*TODO G*/*6; // A-bit number
                aw = cpu->rA;
                bBit = cpu->rKV/*TODO K*/*6; // B-bit number
                bw = cpu->rB;
                do {
                        c = fieldIsolate(aw, aBit, 6);
//printf("%c", translatetable_bic2ascii[c]);
                        bw = fieldInsert(bw, bBit, 6, c);
                        --count;
                        if (bBit < 42) {
                                bBit += 6;
                                ++cpu->rKV/*TODO K*/;
                        } else {
                                bBit = false;
                                cpu->rKV/*TODO K*/ = 0;
                                cpu->rB = bw;
                                storeBviaS(cpu); // [S] = B
                                ++cpu->rS;
                                if (count > 0 && count < 8) { // only need to load B if a partial word is left
                                        loadBviaS(cpu); // B = [S]
                                        bw = cpu->rB;
                                } else {
                                        cpu->bBROF = false;
                                }
                        }
                        if (aBit < 42) {
                                aBit += 6;
                                ++cpu->rGH/*TODO G*/;
                        } else {
                                aBit = false;
                                cpu->rGH/*TODO G*/ = 0;
                                ++cpu->rM;
                                if (count > 0) { // only need to load A if there's more to do
                                        loadAviaM(cpu); // A = [M]
                                        aw = cpu->rA;
                                } else {
                                        cpu->bAROF = false;
                                }
                        }
                } while (count);
                cpu->rB = bw;
                cpu->rY = c; // for display purposes only
        }
//printf("'\n");
}

/*
 * Transfers from source to destination for the TRN and TRZ syllables. "count"
 * is the number of source characters to transfer. If transferring numerics and the
 * low-order character has a negative sign (BA=10), sets TFFF=1
 */
void streamNumericToDest(CPU *cpu, unsigned count, unsigned zones)
{
        unsigned        aBit;   // A register bit nr
        WORD48          aw;     // current A register word
        unsigned        bBit;   // B register bit nr
        WORD48          bw;     // current B register word
        unsigned        c;      // current character

//printf("sNTD %u %c '", count, zones ? 'Z' : 'N');
        streamAdjustSourceChar(cpu);
        streamAdjustDestChar(cpu);
        if (count) {
                if (!cpu->bBROF) {
                        loadBviaS(cpu); // B = [S]
                }
                if (!cpu->bAROF) {
                        loadAviaM(cpu); // A = [M]
                }
                if (zones) { // approximate the timing
                        cpu->cycleCount += 5 + count*4;
                } else {
                        cpu->cycleCount += 10 + count*3;
                }

                aBit = cpu->rGH/*TODO G*/*6; // A-bit number
                aw = cpu->rA;
                bBit = cpu->rKV/*TODO K*/*6; // B-bit number
                bw = cpu->rB;
                do {
                        c = fieldIsolate(aw, aBit, 6);
                        if (zones) { // transfer only the zone portion of the char
//printf("%c", translatetable_bic2ascii[c >> 4]);
                                bw = fieldInsert(bw, bBit, 2, c >> 4);
                        } else { // transfer the numeric portion with a zero zone
//printf("%c", translatetable_bic2ascii[c & 0x0f]);
                                bw = fieldInsert(bw, bBit, 6, (c & 0x0F));
                        }
                        --count;
                        if (bBit < 42) {
                                bBit += 6;
                                ++cpu->rKV/*TODO K*/;
                        } else {
                                bBit = false;
                                cpu->rKV/*TODO K*/ = 0;
                                cpu->rB = bw;
                                storeBviaS(cpu); // [S] = B
                                ++cpu->rS;
                                if (count > 0) {
                                        loadBviaS(cpu); // B = [S]
                                        bw = cpu->rB;
                                } else {
                                        cpu->bBROF = false;
                                }
                        }
                        if (aBit < 42) {
                                aBit += 6;
                                ++cpu->rGH/*TODO G*/;
                        } else {
                                aBit = false;
                                cpu->rGH/*TODO G*/ = 0;
                                ++cpu->rM;
                                if (count > 0) { // only need to load A if there's more to do
                                        loadAviaM(cpu); // A = [M]
                                        aw = cpu->rA;
                                } else {
                                        cpu->bAROF = false;
                                }
                        }
                } while (count);
                cpu->rB = bw;
                cpu->rY = c; // for display purposes only
                if (!zones && (c & 0x30) == 0x20) {
                        cpu->bTFFF = true; // last char had a negative sign
                }
        }
//printf("'\n");
}

/*
 * Implements the TBN (Transfer Blanks for Non-Numeric) syllable, which is
 * generally used to suppress leading zeroes in numeric strings. Transfers blanks
 * to the destination under control of the count as long as the destination characters
 * are not in the range "1"-"9". Sets TFFF (MSFF) true if the count is exhausted.
 * "count" is the maximum number of characters to blank
 */
void streamBlankForNonNumeric(CPU *cpu, unsigned count)
{
        unsigned        bBit;   // B register bit nr
        WORD48          bw;     // current B register word
        unsigned        c;      // current destination character

//printf("sBFNN %u '", count);
        cpu->bTFFF = true; // assume the count will be exhausted
        streamAdjustDestChar(cpu);
        if (count) {
                if (!cpu->bBROF) {
                        loadBviaS(cpu); // B = [S]
                }
                bBit = cpu->rKV/*TODO K*/*6; // B-bit number
                bw = cpu->rB;
                do {
                        cpu->cycleCount += 2; // approximate the timing
                        c = fieldIsolate(bw, bBit, 6);
//printf("%c", translatetable_bic2ascii[c]);
                        if (c > 0 && c <= 9) {
                                // is numeric and non-zero: stop blanking
                                cpu->bTFFF = false;
                                // set Q03F (display only)
                                cpu->bQ03F = true;
                                // terminate, pointing at cpu char
                                break;
                        } else {
                                bw = fieldInsert(bw, bBit, 6, 0x30); // replace with blank
                                --count;
                                if (bBit < 42) {
                                        bBit += 6;
                                        ++cpu->rKV/*TODO K*/;
                                } else {
                                        bBit = false;
                                        cpu->rKV/*TODO K*/ = 0;
                                        cpu->rB = bw;
                                        storeBviaS(cpu); // [S] = B
                                        ++cpu->rS;
                                        if (count > 0) {
                                                loadBviaS(cpu); // B = [S]
                                                bw = cpu->rB;
                                        } else {
                                                cpu->bBROF = false;
                                        }
                                }
                        }
                } while (count);
                cpu->rB = bw;
                cpu->rZ = c; // for display purposes only
        }
//printf("'\n");
}

/*
 * Converts a signed-numeric character field at the source M & G address
 * from decimal to binary, storing the resulting word at the S address and then
 * incrementing S. Normally, decimal to binary conversion shouldn't be cpu
 * complex, so we must do it more or less the way the B5500 hardware did, by
 * repeated remainder division (i.e., shifting right) and adjusting the
 * low-order digit by -3 when a one was shifted into the high-order bit of the
 * low-order digit from the higher digit locations. The problem with doing it a
 * more direct and efficient way is with digits that are not in the range 0-9.
 * Doing it the hardware way should yield the same (albeit questionable)
 * result. See Section 2.6 in the B5281 Training Manual for details. This
 * process took at least 27 clocks on the B5500, so we can afford to be slow
 * here, too. Note that a maximum of 8 characters are converted
 */
void streamInputConvert(CPU *cpu, unsigned count)
{
        WORD48  a = 0;          // local working copy of A
        WORD48  b = 0;          // local working copy of B
        WORD48  power = 1;      // A-register shift factor

//printf("sIC %u '", count);
        streamAdjustSourceChar(cpu);
        if (cpu->bBROF) {
                storeBviaS(cpu); // [S] = B
                cpu->bBROF = false;
        }
        if (cpu->rKV/*TODO K*/ || cpu->rKV/*TODO V*/) { // adjust dest to word boundary
                cpu->rKV/*TODO K*/ = cpu->rKV/*TODO V*/ = 0;
                ++cpu->rS;
        }
        if (count) { // no conversion if count is zero
                cpu->cycleCount += count*2 + 27;
                count = ((count-1) & 0x07) + 1; // limit the count to 8
                if (!cpu->bAROF) {
                        loadAviaM(cpu); // A = [M]
                }

                // First, assemble the digits into B as 4-bit BCD
                do {
                        b = (b << 4) | ((cpu->rY = fieldIsolate(cpu->rA, cpu->rGH/*TODO G*/*6, 6)) & 0x0F);
//printf("%c", translatetable_bic2ascii[cpu->rY]);
                        if (cpu->rGH/*TODO G*/ < 7) {
                                ++cpu->rGH/*TODO G*/;
                        } else {
                                cpu->rGH/*TODO G*/ = 0;
                                ++cpu->rM;
                                if (count > 1) {
                                        loadAviaM(cpu); // A = [M], only if more chars are needed
                                } else {
                                        cpu->bAROF = false;
                                }
                        }
                } while (--count);

                // Then do the artful shifting to form the binary value in A
                cpu->bAROF = false;
                cpu->rB = b; // for display purposes only
                while (b) {
                        if (b & 0x01) {
                                a += power;
                        }
                        power *= 2;
                        b >>= 1;

                        /*
                         * This next part is tricky, and was done by a switching network in the B5500.
                         * When a 1 bit is shifted into the high-order position of a BCD decade from its
                         * decade to the left, that bit has a place value of 8, but because the number
                         * is decimal, it should have a place value of five. Therefore, in EACH such
                         * decade, we need to subtract 3 to get the correct place value. The following
                         * statement constructs a mask of 3s in each decade where the high-order bit is
                         * set after the shift above, then subtracts that mask from the working B value.
                         * See the discussion in Section 2.6 in the Training Manual cited above
                         */

                        b -= ((b & 0x88888888) >> 3)*3;
                }

                // Finally, fix up the binary sign and store the result
                if (a) { // zero results have sign bit reset
                        if ((cpu->rY & 0x30) == 0x20) {
                                a |= MASK_SIGNMANT; // set the sign bit
                        }
                }
                cpu->rA = a;
                storeAviaS(cpu); // [S] = A
                ++cpu->rS;
        }
//printf("'\n");
}

/*
 * Converts the binary word addressed by M (after word-boundary adjustment)
 * to decimal BIC at the destination address of S & K. The maximum number of
 * digits to convert is 8. If the binary value can be represented in "count"
 * digits (or the count is zero), the true-false FF, TFFF, is set; otherwise it
 * is reset. The sign is stored in low-order character of the result
 */
void streamOutputConvert(CPU *cpu, unsigned count)
{
        WORD48          a;      // local working copy of A
        WORD48          b = 0;  // local working copy of B
        unsigned        c;      // converted decimal character
        unsigned        d = 0;  // digit counter
        WORD48  power = 1;      // power-of-64 factor for result digits

//printf("%08u sOC %d ", instr_count, count);
        cpu->bTFFF = true; // set TFFF unless there's overflow
        streamAdjustDestChar(cpu);
        if (cpu->bBROF) {
                storeBviaS(cpu); // [S] = B, but leave BROF set
        }
        if (cpu->rGH/*TODO G*/ || cpu->rGH/*TODO H*/) { // adjust source to word boundary
                cpu->rGH/*TODO G*/ = cpu->rGH/*TODO H*/ = 0;
                cpu->bAROF = false;
                ++cpu->rM;
        }
        if (count) { // count > 0
                cpu->cycleCount += count*2 + 27;
                if (!cpu->bAROF) {
                        loadAviaM(cpu); // A = [M]
                }
//printf("A=%016llo '", cpu->rA);
                count = ((count-1) & 0x07) + 1; // limit the count to 8
                a = cpu->rA & MASK_MANTISSA; // get absolute mantissa value, ignore exponent
                if (a) { // mantissa is non-zero, so conversion is required
                        if (cpu->rA & MASK_SIGNMANT) {
                                // result is negative, so preset the sign in the low-order digit
                                b = 040;        // BA'8421=10'0000
                        }
                        do { // Convert the binary value in A to BIC digits in B
                                c = a % 10;
                                a = (a-c)/10;
                                if (c) {
                                        b += c*power;
                                }
                                power *= 64;
                        } while (a && (++d < count));
                        if (a) {
                                // overflow occurred, so reset TFFF
                                cpu->bTFFF = false;
                        }
                }
                cpu->bAROF = false; // invalidate A
                ++cpu->rM; // and advance to the next source word

                // Finally, stream the digits from A (whose value is still in local b) to the destination
                cpu->rA = b; // for display purposes only
                loadBviaS(cpu); // B = [S], restore original value of B
                d = 48 - count*6; // starting bit in A
                do {
                        c = fieldIsolate(b, d, 6);
//printf("%c", translatetable_bic2ascii[c]);
                        cpu->rB = fieldInsert(cpu->rB, cpu->rKV/*TODO K*/*6, 6, c);
                        d += 6;
                        if (cpu->rKV/*TODO K*/ < 7) {
                                ++cpu->rKV/*TODO K*/;
                        } else {
                                storeBviaS(cpu); // [S] = B
                                cpu->rKV/*TODO K*/ = 0;
                                ++cpu->rS;
                                if (count > 1) {
                                        loadBviaS(cpu); // B = [S]
                                } else {
                                        cpu->bBROF = false;
                                }
                        }
                } while (--count);
        }
//printf("'\n");
}
