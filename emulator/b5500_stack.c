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
***********************************************************************/

#include "b5500_common.h"

/*
 * Ensures both TOS registers are occupied,
 * pushing up from memory as required
 */
void adjustABFull(CPU *cpu)
{
        if (cpu->r.AROF) {
                if (cpu->r.BROF) {
                        // A and B are already full, so we're done
                } else {
                        // A is full and B is empty, so load B from [S]
                        loadBviaS(cpu); // B = [S]
                        --cpu->r.S;
                }
        } else {
                if (cpu->r.BROF) {
                        // A is empty and B is full, so copy B to A and load B from [S]
                        cpu->r.A = cpu->r.B;
                        cpu->r.AROF = 1;
                } else {
                        // A and B are empty, so simply load them from [S]
                        loadAviaS(cpu); // A = [S]
                        --cpu->r.S;
                }
                loadBviaS(cpu); // B = [S]
                --cpu->r.S;
        }
}

/*
 * Adjusts the A register so that it is full, popping the contents of
 * B or [S] into A, as necessary.
 */
void adjustAFull(CPU *cpu)
{
        if (!cpu->r.AROF) {
                if (cpu->r.BROF) {
                        cpu->r.A = cpu->r.B;
                        cpu->r.AROF = 1;
                        cpu->r.BROF = 0;
                } else {
                        loadAviaS(cpu); // A = [S]
                        --cpu->r.S;
                }
        }
        // else we're done -- A is already full
}

/*
 * Adjusts the B register so that it is full, popping the contents of
 * [S] into B, as necessary.
 */
void adjustBFull(CPU *cpu)
{
        if (!cpu->r.BROF) {
                loadBviaS(cpu); // B = [S]
                --cpu->r.S;
        }
        // else we're done -- B is already full
}

/*
 * Adjusts the A and B registers so that both are empty, pushing the
 * prior contents into memory, as necessary.
 */
void adjustABEmpty(CPU *cpu)
{
        // B occupied ?
        if (cpu->r.BROF) {
                // empty B to stack
                if (((cpu->r.S >> 6) == cpu->r.R) && cpu->r.NCSF) {
                        // set I03F: stack overflow
                        cpu->r.I |= 0x04;
                        signalInterrupt(cpu->id, "StackOverflow");
                } else {
                        ++cpu->r.S;
                        storeBviaS(cpu); // [S] = B
                }
                // B is now empty
                cpu->r.BROF = 0;
        }
        // else we're done -- B is already empty

        // A occupied ?
        if (cpu->r.AROF) {
                // empty A to stack
                if (((cpu->r.S >> 6) == cpu->r.R) && cpu->r.NCSF) {
                        // set I03F: stack overflow
                        cpu->r.I |= 0x04;
                        signalInterrupt(cpu->id, "StackOverflow");
                } else {
                        ++cpu->r.S;
                        storeAviaS(cpu); // [S] = B
                }
                // A is now empty
                cpu->r.AROF = 0;
        }
        // else we're done -- A is already empty
}

/*
 * Adjusts the A register so that it is empty, pushing the prior
 * contents of A into B and B into memory, as necessary.
 */
void adjustAEmpty(CPU *cpu)
{
        // A occupied ?
        if (cpu->r.AROF) {
                // B occupied ?
                if (cpu->r.BROF) {
                        // empty B to stack
                        if (((cpu->r.S >> 6) == cpu->r.R) && cpu->r.NCSF) {
                                // set I03F: stack overflow
                                cpu->r.I |= 0x04;
                                signalInterrupt(cpu->id, "StackOverflow");
                        } else {
                                ++cpu->r.S;
                                storeBviaS(cpu); // [S] = B
                        }
                }
                // B is now empty, move A to B
                cpu->r.B = cpu->r.A;
                cpu->r.AROF = 0;
                cpu->r.BROF = 1;
        }
        // else we're done -- A is already empty
}

/*
 * Adjusts the B register so that it is empty, pushing the prior
 * contents of B into memory, as necessary.
 */
void adjustBEmpty(CPU *cpu)
{
        // B occupied ?
        if (cpu->r.BROF) {
                // empty B to stack
                if (((cpu->r.S >> 6) == cpu->r.R) && cpu->r.NCSF) {
                        // set I03F: stack overflow
                        cpu->r.I |= 0x04;
                        signalInterrupt(cpu->id, "StackOverflow");
                } else {
                        ++cpu->r.S;
                        storeBviaS(cpu); // [S] = B
                }
                // B is now empty
                cpu->r.BROF = 0;
        }
        // else we're done -- B is already empty
}

/*
 * Exchanges the two top-of-stack values
 */
void exchangeTOS(CPU *cpu)
{
        WORD48 temp;

        if (cpu->r.AROF) {
                if (cpu->r.BROF) {
                        // A and B are full, so simply exchange them
                        temp = cpu->r.A;
                        cpu->r.A = cpu->r.B;
                        cpu->r.B = temp;
                } else {
                        // A is full and B is empty, so push A to B and load A from [S]
                        cpu->r.B = cpu->r.A;
                        cpu->r.BROF = 1;
                        loadAviaS(cpu); // A = [S]
                        --cpu->r.S;
                }
        } else {
                if (cpu->r.BROF) {
                        // A is empty and B is full, so load A from [S]
                        loadAviaS(cpu); // A = [S]
                        --cpu->r.S;
                } else {
                        // A and B are empty, so simply load them in reverse order
                        loadBviaS(cpu); // B = [S]
                        --cpu->r.S;
                        loadAviaS(cpu); // A = [S]
                        --cpu->r.S;
                }
        }
}
