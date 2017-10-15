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
* 2017-07-17  R.Meyer
*   changed "this" to "cpu" to avoid errors when using g++
* 2017-09-30  R.Meyer
*   overhaul of file names
* 2017-10-10  R.Meyer
*   some refactoring in the functions, added documentation
***********************************************************************/

#include "common.h"

/*
 * note that in all cases A and B are extensions of the stack
 *
 * A is the top of the stack value S[0]
 * B is second of the stack value S[-1]
 */

/*
 * function to increment the Stack Pointer - return true if OK
 * cause a "stack overflow" in normal state when S >= R
 */
BIT incrementS(CPU *cpu) {
	cpu->r.S++;
	if (((cpu->r.S >> 6) >= cpu->r.R) && cpu->r.NCSF) {
		// set I03F: stack overflow
		cpu->r.I |= 0x04;
		signalInterrupt(cpu->id, "StackOverflow");
		return false;
	}
	return true;
}

/*
 * function to decrement the Stack Pointer
 * TODO: do we have any failure clues we could use to detect stack underrun ???
 */
BIT decrementS(CPU *cpu) {
	cpu->r.S--;
	return true;
}

/*
 * Ensures both TOS registers are occupied
 */
void adjustABFull(CPU *cpu)
{
	adjustAFull(cpu); // ensure A occupied
	adjustBFull(cpu); // ensure B occupied
}

/*
 * Ensure A is occupied (B occupied is don't care)
 * get it from B or from stack
 */
void adjustAFull(CPU *cpu)
{
        if (!cpu->r.AROF) {
		// A needs to be filled
                if (cpu->r.BROF) {
			// get it from B
			cpu->r.A = cpu->r.B;
			cpu->r.AROF = true;
			cpu->r.BROF = false;
		} else {
			// get it from stack
                        loadAviaS(cpu);
                        decrementS(cpu);
		}
	}
}

/*
 * Ensure B is occupied (A occupied is don't care)
 * move from stack
 */
void adjustBFull(CPU *cpu)
{
        if (!cpu->r.BROF) {
		// B needs to be filled
                loadBviaS(cpu);
                decrementS(cpu);
        }
}

/*
 * Ensure A and B registers are empty
 */
void adjustABEmpty(CPU *cpu)
{
        // B occupied ?
        if (cpu->r.BROF) {
                // empty B to stack
		if (incrementS(cpu))
                        storeBviaS(cpu);
                cpu->r.BROF = false;
        }
        // A occupied ?
        if (cpu->r.AROF) {
                // empty A to stack
		if (incrementS(cpu))
                        storeAviaS(cpu);
                cpu->r.AROF = false;
        }
}

/*
 * Ensure A is empty
 * B to stack and move A to B
 */
void adjustAEmpty(CPU *cpu)
{
        // A occupied ?
        if (cpu->r.AROF) {
                // B occupied ?
                if (cpu->r.BROF) {
                        // empty B to stack
                        if (incrementS(cpu))
                                storeBviaS(cpu);
                }
                // B is now empty, move A to B
                cpu->r.B = cpu->r.A;
                cpu->r.AROF = false;
                cpu->r.BROF = true;
        }
}

/*
 * Ensure B is empty
 */
void adjustBEmpty(CPU *cpu)
{
        // B occupied ?
        if (cpu->r.BROF) {
                // empty B to stack
                if (incrementS(cpu))
                        storeBviaS(cpu);
                cpu->r.BROF = false;
        }
}

/*
 * Exchanges the two top-of-stack values
 */
void exchangeTOS(CPU *cpu)
{
        if (cpu->r.AROF) {
                if (cpu->r.BROF) {
                        // A and B are full, so simply exchange them
		        WORD48 temp = cpu->r.A;
                        cpu->r.A = cpu->r.B;
                        cpu->r.B = temp;
                } else {
                        // A is full and B is empty, so push A to B and load A from [S]
                        cpu->r.B = cpu->r.A;
                        cpu->r.BROF = true;
                        loadAviaS(cpu);
                        decrementS(cpu);
                }
        } else {
                if (cpu->r.BROF) {
                        // A is empty and B is full, so load A from [S]
                        loadAviaS(cpu);
                        decrementS(cpu);
                } else {
                        // A and B are empty, so simply load them in reverse order
                        loadBviaS(cpu);
                        decrementS(cpu);
                        loadAviaS(cpu);
                        decrementS(cpu);
                }
        }
}

