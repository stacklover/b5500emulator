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
* 2016-02-19  R.Meyer
*   Converted Paul's work from Javascript to C
* 2017-07-17  R.Meyer
*   changed "this" to "cpu" to avoid errors when using g++
* 2017-09-30  R.Meyer
*   overhaul of file names
* 2017-10-10  R.Meyer
*   some refactoring in the functions, added documentation
***********************************************************************/

#include <stdio.h>
#include "common.h"

/*
 * local function to integerize a REAL value to an INTEGER
 * return true on integer overflow
 *
 * Note: this function does round as follows
 *  0.50000 ->  1
 *  0.49999 ->  0
 * -0.50000 ->  0
 * -0.50001 -> -1
 * This rounding also takes place when indexing arrays!
 * This is intentional on the B5500
 */
static BIT integerize(WORD48 *v) {
	NUM n;

	num_extract(v, &n);

	if (n.e < 0) {
		// exponent is negative
		do {
			// store rightmost octet in x for later rounding
			n.x = n.m & 7;
			n.m >>= 3; // shift one octade right
			n.e++;
		} while (n.e < 0);
		// round up the number
		if (n.s ? n.x > 4 : n.x >= 4) {
			n.m++; // round the mantissa
		}
	} else if (n.e > 0) {
		// exponent is positive
		do {
			if (n.m & MASK_MANTHIGH) {
				// integer overflow while normalizing the mantisa
				return true;
			} else {
				n.m <<= 3; // shift one octade left
			}
			n.e--;
		} while (n.e > 0);
	}

        num_compose(&n, v);

	return false;
}

/*
 * Called by a requestor module passing accessor object "acc" to fetch a
 * word from memory.
 */
void fetch(ACCESSOR *acc)
{
        BIT watched = dotrcmem;
        //if (acc->addr == 0200)
        //        watched = true;
        // For now, we assume memory parity can never happen
        if (acc->MAIL) {
                acc->MPED = false;      // no memory parity error
                acc->MAED = true;       // memory address error
                acc->word = 0;
                if (watched)
                        printf("\t[%05o] ERROR MPED=%u MAED=%u (%s)\n",
                                acc->addr, acc->MPED, acc->MAED, acc->id);
        } else {
                acc->MPED = false;      // no parity error
                acc->MAED = false;      // no address error
                acc->word = MAIN[acc->addr & MASKMEM];
                if (watched)
                        printf("\t[%05o]->%016llo OK (%s)\n",
                                acc->addr, acc->word, acc->id);
        }
}

/*
 * Called by requestor module passing accessor object "acc" to store a
 * word into memory.
 */
void store(ACCESSOR *acc)
{
        BIT watched = dotrcmem;
        //if (acc->addr == 0200)
        //        watched = true;
        // For now, we assume memory parity can never happen
        if (acc->MAIL) {
                acc->MPED = false;      // no memory parity error
                acc->MAED = true;       // memory address error
                if (watched)
                        printf("\t[%05o] ERROR MPED=%u MAED=%u (%s)\n",
                        acc->addr, acc->MPED, acc->MAED, acc->id);
        } else {
                acc->MPED = false;      // no parity error
                acc->MAED = false;      // no address error
                MAIN[acc->addr & MASKMEM] = acc->word;
                if (watched)
                        printf("\t[%05o]<-%016llo OK (%s)\n",
                                acc->addr, acc->word, acc->id);
        }
}

/*
 * Common error handling routine for all memory acccesses
 */
void accessError(CPU *cpu)
{
        printf("*\t%s accessError addr=%05o\n", cpu->id, cpu->acc.addr);
        if (cpu->acc.MAED) {
                // set I02F: memory address/inhibit error
                cpu->r.I |= 0x02;
                signalInterrupt(cpu->id, "MAE");
        } else if (cpu->acc.MPED) {
                // set I01F: memory parity error
                cpu->r.I |= 0x01;
                signalInterrupt(cpu->id, "MPE");
                if (cpu->isP1 && !cpu->r.NCSF) {
                        // P1 memory parity in Control State stops the processor
                        stop(cpu);
                }
        }
}

/*
 * Indexes a descriptor and, if successful leaves the indexed value in
 * the A register. Returns 1 if an interrupt is set and the syllable is
 * to be exited
 */
BIT indexDescriptor(CPU *cpu)
{
        WORD48  aw;     // local copy of A reg
        BIT             interrupted = false;    // fatal error, interrupt set
	ADDR15	index;
	BIT	sign;

        adjustABFull(cpu);
        aw = cpu->r.A;

        // Normalize the index
	if (integerize(&cpu->r.B)) {
		// oops... integer overflow normalizing the index
		interrupted = true;
		if (cpu->r.NCSF) {
			//causeSyllableIrq(cpu, IRQ_INTO, "INX Overflow");
                        // set I07/8: integer overflow
                        cpu->r.I = (cpu->r.I & IRQ_MASKL) | IRQ_INTO;
                        signalInterrupt(cpu->id, "INX Overflow");
		}
        }

        // look only at lowest 10 bits
        index = cpu->r.B & 01777;
	sign = (cpu->r.B & MASK_SIGNMANT);

        // Now we have an integerized index value in I
        if (!interrupted) {
                if (sign && index) {
                        // index is negative
                        interrupted = true;
                        if (cpu->r.NCSF) {
				//causeSyllableIrq(cpu, IRQ_INDEX, "INX<0");
                                // set I05/8: invalid-index
                                cpu->r.I = (cpu->r.I & IRQ_MASKL) | IRQ_INDEX;
                                signalInterrupt(cpu->id, "INX<0");
                        }
                } else if (index < ((aw & MASK_WCNT) >> SHFT_WCNT)) {
                        // We finally have a valid index
                        index = (aw + index) & MASK_ADDR;
                        cpu->r.A = (aw & ~MASK_ADDR) | index;
                        cpu->r.BROF = false;
                } else {
                        // index not less than size
                        interrupted = true;
                        if (cpu->r.NCSF) {
				//causeSyllableIrq(cpu, IRQ_INDEX, "INX>WC");
                                // set I05/8: invalid-index
                                cpu->r.I = (cpu->r.I & IRQ_MASKL) | IRQ_INDEX;
                                signalInterrupt(cpu->id, "INX>WC");
                        }
                }
        }
        return interrupted;
}

/*
 * load registers from memory
 */
void loadAviaS(CPU *cpu)
{
        if (dotrcmem)
                printf("\t[S]->A ");
        cpu->r.E = 2;           // Just to show the world what's happening
        cpu->acc.addr = cpu->r.S;
        cpu->acc.MAIL = (cpu->r.S < AA_USERMEM) && cpu->r.NCSF;
        fetch(&cpu->acc);
        //cpu->cycleCount += B5500CentralControl.memReadCycles;
        if (cpu->acc.MAED || cpu->acc.MPED) {
                accessError(cpu);
        } else {
                cpu->r.A = cpu->acc.word;
                cpu->r.AROF = true;
        }
}

void loadBviaS(CPU *cpu)
{
        if (dotrcmem)
                printf("\t[S]->B ");
        cpu->r.E = 3;           // Just to show the world what's happening
        cpu->acc.addr = cpu->r.S;
        cpu->acc.MAIL = (cpu->r.S < AA_USERMEM) && cpu->r.NCSF;
        fetch(&cpu->acc);
        //cpu->cycleCount += B5500CentralControl.memReadCycles;
        if (cpu->acc.MAED || cpu->acc.MPED) {
                accessError(cpu);
        } else {
                cpu->r.B = cpu->acc.word;
                cpu->r.BROF = true;
        }
}

void loadAviaM(CPU *cpu)
{
        if (dotrcmem)
                printf("\t[M]->A ");
        cpu->r.E = 4;           // Just to show the world what's happening
        cpu->acc.addr = cpu->r.M;
        cpu->acc.MAIL = (cpu->r.M < AA_USERMEM) && cpu->r.NCSF;
        fetch(&cpu->acc);
        //cpu->cycleCount += B5500CentralControl.memReadCycles;
        if (cpu->acc.MAED || cpu->acc.MPED) {
                accessError(cpu);
        } else {
                cpu->r.A = cpu->acc.word;
                cpu->r.AROF = true;
        }
}

void loadBviaM(CPU *cpu)
{
        if (dotrcmem)
                printf("\t[M]->B ");
        cpu->r.E = 5;           // Just to show the world what's happening
        cpu->acc.addr = cpu->r.M;
        cpu->acc.MAIL = (cpu->r.M < AA_USERMEM) && cpu->r.NCSF;
        fetch(&cpu->acc);
        //cpu->cycleCount += B5500CentralControl.memReadCycles;
        if (cpu->acc.MAED || cpu->acc.MPED) {
                accessError(cpu);
        } else {
                cpu->r.B = cpu->acc.word;
                cpu->r.BROF = true;
        }
}

void loadMviaM(CPU *cpu)
{
        // note: this is only used to get the saved F registers value from
        // the saved MSCW at R+7.
        if (dotrcmem)
                printf("\t[M]~>M ");
        cpu->r.E = 6;           // Just to show the world what's happening
        cpu->acc.addr = cpu->r.M;
        cpu->acc.MAIL = (cpu->r.M < AA_USERMEM) && cpu->r.NCSF;
        fetch(&cpu->acc);
        //cpu->cycleCount += B5500CentralControl.memReadCycles;
        if (cpu->acc.MAED || cpu->acc.MPED) {
                accessError(cpu);
        } else {
                cpu->r.M = (cpu->acc.word & MASK_FREG) >> SHFT_FREG;
        }
}

void loadPviaC(CPU *cpu)
{
        if (dotrcmem)
                printf("\t[C]->P ");
        cpu->r.E = 48;          // Just to show the world what's happening
        cpu->acc.addr = cpu->r.C;
        cpu->acc.MAIL = (cpu->r.C < AA_USERMEM) && cpu->r.NCSF;
        fetch(&cpu->acc);
        // TODO: should cpu not be in the else part?
        cpu->r.PROF = true;
        //cpu->cycleCount += B5500CentralControl.memReadCycles;
        if (cpu->acc.MAED || cpu->acc.MPED) {
                accessError(cpu);
        } else {
                cpu->r.P = cpu->acc.word;
        }
}

/*
 * store registers to memory
 */
void storeAviaS(CPU *cpu)
{
        if (dotrcmem)
                printf("\t[S]<-A ");
        cpu->r.E = 10;          // Just to show the world what's happening
        cpu->acc.addr = cpu->r.S;
        cpu->acc.MAIL = (cpu->r.S < AA_USERMEM) && cpu->r.NCSF;
        cpu->acc.word = cpu->r.A;
        store(&cpu->acc);
        //cpu->cycleCount += B5500CentralControl.memReadCycles;
        if (cpu->acc.MAED || cpu->acc.MPED) {
                accessError(cpu);
        }
}

void storeBviaS(CPU *cpu)
{
        if (dotrcmem)
                printf("\t[S]<-B ");
        cpu->r.E = 11;          // Just to show the world what's happening
        cpu->acc.addr = cpu->r.S;
        cpu->acc.MAIL = (cpu->r.S < AA_USERMEM) && cpu->r.NCSF;
        cpu->acc.word = cpu->r.B;
        store(&cpu->acc);
        //cpu->cycleCount += B5500CentralControl.memReadCycles;
        if (cpu->acc.MAED || cpu->acc.MPED) {
                accessError(cpu);
        }
}

void storeAviaM(CPU *cpu)
{
        if (dotrcmem)
                printf("\t[M]<-A ");
        cpu->r.E = 12;          // Just to show the world what's happening
        cpu->acc.addr = cpu->r.M;
        cpu->acc.MAIL = (cpu->r.M < AA_USERMEM) && cpu->r.NCSF;
        cpu->acc.word = cpu->r.A;
        store(&cpu->acc);
        //cpu->cycleCount += B5500CentralControl.memReadCycles;
        if (cpu->acc.MAED || cpu->acc.MPED) {
                accessError(cpu);
        }
}

void storeBviaM(CPU *cpu)
{
        if (dotrcmem)
                printf("\t[M]<-B ");
        cpu->r.E = 12;          // Just to show the world what's happening
        cpu->acc.addr = cpu->r.M;
        cpu->acc.MAIL = (cpu->r.M < AA_USERMEM) && cpu->r.NCSF;
        cpu->acc.word = cpu->r.B;
        store(&cpu->acc);
        //cpu->cycleCount += B5500CentralControl.memReadCycles;
        if (cpu->acc.MAED || cpu->acc.MPED) {
                accessError(cpu);
        }
}

/*
 * Store the value in the B register at the address in the A register (relative
 * or descriptor) and marks the A register empty. "conditional" indicates that
 * integerization is conditional on the type of word in A, and if a descriptor,
 * whether it has the integer bit set
 */
void integerStore(CPU *cpu, BIT conditional, BIT destructive)
{
        WORD48  aw;     // local copy of A reg
        NUM             B;      // B mantissa
        BIT             doStore = true;         // okay to store
        BIT             normalize = true;       // okay to integerize

        adjustABFull(cpu);
        aw = cpu->r.A;
        if (OPERAND(aw)) {
                // it's an operand
                computeRelativeAddr(cpu, aw, false);
        } else {
                // it's a descriptor
                if (presenceTest(cpu, aw)) {
                        cpu->r.M = aw & MASKMEM;
                        if (conditional) {
                                if (!(aw & MASK_INTG)) { // [19:1] is the integer bit
                                        normalize = false;
                                }
                        }
                } else {
                        doStore = normalize = false;
                }
        }

        if (normalize) {

		if (integerize(&cpu->r.B)) {
                        // oops... integer overflow normalizing the mantisa
                        doStore = false;
			if (cpu->r.NCSF) {
				//causeSyllableIrq(cpu, IRQ_INTO, "ISD/ISN Overflow");
                                // set I07/8: integer overflow
                                cpu->r.I = (cpu->r.I & IRQ_MASKL) | IRQ_INTO;
                                signalInterrupt(cpu->id, "ISD/ISN Overflow");
				return;
			}
		}
        }

        if (doStore) {
                storeBviaM(cpu);
                cpu->r.AROF = false;
                if (destructive) {
                        cpu->r.BROF = false;
                }
        }
}
