/***********************************************************************
* b5500emulator
************************************************************************
* Copyright (c) 2016, Reinhard Meyer, DL5UY
* Licensed under the MIT License,
*       see LICENSE
* based on (C) work by Nigel Williams and Paul Kimpel
* see: https://github.com/pkimpel/retro-b5500
************************************************************************
* double precision arithmetic
************************************************************************
* 2016-02-1921  R.Meyer
*   Converted Paul's work from Javascript to C
* 2017-07-17  R.Meyer
*   changed "this" to "cpu" to avoid errors when using g++
***********************************************************************/

#include <stdio.h>
#include "b5500_common.h"

/*
 * Add Aa to Bb
 */
void doublePrecisionAdd(CPU *cpu, BIT add)
{
        printf("*\tfaking DADD\n");
        // Fake it by rearranging the stack and calling the single precision variant
        adjustABFull(cpu);      // get double precision Aa
        cpu->r.BROF = false;    // eliminate a
        adjustBFull(cpu);       // get B
        cpu->r.S--;             // eliminate b
        singlePrecisionAdd(cpu, add); // single precision add leaves result in B
        cpu->r.A = cpu->r.B;    // high part of result to A
        cpu->r.B = 0ll;         // clear lower part of result
        cpu->r.AROF = true;     // both AROF and BROF are now set
}

/*
 * Multiply Aa into Bb
 */
void doublePrecisionMultiply(CPU *cpu)
{
        printf("*\tfaking DMUL\n");
        // Fake it by rearranging the stack and calling the single precision variant
        adjustABFull(cpu);      // get double precision Aa
        cpu->r.BROF = false;    // eliminate a
        adjustBFull(cpu);       // get B
        cpu->r.S--;             // eliminate b
        singlePrecisionMultiply(cpu); // single precision mul leaves result in B
        cpu->r.A = cpu->r.B;    // high part of result to A
        cpu->r.B = 0ll;         // clear lower part of result
        cpu->r.AROF = true;     // both AROF and BROF are now set
}

/*
 * Divide Aa by Bb
 */
void doublePrecisionDivide(CPU *cpu)
{
        printf("*\tfaking DDIV\n");
        // Fake it by rearranging the stack and calling the single precision variant
        adjustABFull(cpu);      // get double precision Aa
        cpu->r.BROF = false;    // eliminate a
        adjustBFull(cpu);       // get B
        cpu->r.S--;             // eliminate b
        singlePrecisionDivide(cpu); // single precision div leaves result in B
        cpu->r.A = cpu->r.B;    // high part of result to A
        cpu->r.B = 0ll;         // clear lower part of result
        cpu->r.AROF = true;     // both AROF and BROF are now set
}

