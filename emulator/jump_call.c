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
* 2016-02-19  R.Meyer
*   Converted Paul's work from Javascript to C
* 2017-07-17  R.Meyer
*   changed "this" to "cpu" to avoid errors when using g++
* 2017-09-30  R.Meyer
*   overhaul of file names
* 2017-10-28  R.Meyer
*   adaption to new CPU structure
***********************************************************************/

#include <stdio.h>
#include "common.h"

/*
 * Return a Mark Stack Control Word from current processor state
 */
WORD48 buildMSCW(CPU *cpu)
{
        WORD48 t1 = INIT_MSCW |
                ((WORD48)cpu->rF << SHFT_FREG) |
                ((WORD48)cpu->rR/*TODO SHIFT*/ << SHFT_RREG);
        if (cpu->bSALF)
              t1 |= MASK_SALF;
        if (cpu->bMSFF)
              t1 |= MASK_MSFF;
        return t1;
}

/*
 * Set  processor state from fields of the Mark Stack Control
 * Word in the "word" parameter
 */
void applyMSCW(CPU *cpu, WORD48 word)
{
        cpu->rF = (word & MASK_FREG) >> SHFT_FREG;
        cpu->rR/*TODO SHIFT*/ = (word & MASK_RREG) >> SHFT_RREG;
        cpu->bSALF = (word & MASK_SALF) ? true : false;
        cpu->bMSFF = (word & MASK_MSFF) ? true : false;
}

/*
 * Return a Return Control Word from the current processor state
 */
WORD48 buildRCW(CPU *cpu, BIT descriptorCall)
{
        WORD48 t1 = INIT_RCW |
                ((WORD48)cpu->rC << SHFT_CREG) |
                ((WORD48)cpu->rF << SHFT_FREG) |
                ((WORD48)cpu->rKV/*TODO K*/ << SHFT_KREG) |
                ((WORD48)cpu->rGH/*TODO G*/ << SHFT_GREG) |
                ((WORD48)cpu->rL << SHFT_LREG) |
                ((WORD48)cpu->rKV/*TODO V*/ << SHFT_VREG) |
                ((WORD48)cpu->rGH/*TODO H*/ << SHFT_HREG);
        if (descriptorCall)
                t1 |= MASK_RCWTYPE;
        return t1;
}

/*
 * Set processor state from fields of the Return Control Word in
 * the "word" parameter. If "inline" is truthy, C & L are NOT restored from
 * the RCW. Returns the state of the OPDC/DESC bit [2:1]
 */
BIT applyRCW(CPU *cpu, WORD48 word, BIT no_set_lc, BIT no_bits)
{
	if (!no_set_lc) {
		cpu->rC = (word & MASK_CREG) >> SHFT_CREG;
		cpu->rL = (word & MASK_LREG) >> SHFT_LREG;
		cpu->bPROF = false;    // require fetch at SECL
	}
	cpu->rF = (word & MASK_FREG) >> SHFT_FREG;
	if (!no_bits) {
		cpu->rKV/*TODO K*/ = (word & MASK_KREG) >> SHFT_KREG;
		cpu->rGH/*TODO G*/ = (word & MASK_GREG) >> SHFT_GREG;
		cpu->rKV/*TODO V*/ = (word & MASK_VREG) >> SHFT_VREG;
		cpu->rGH/*TODO H*/ = (word & MASK_HREG) >> SHFT_HREG;
	}
	return (word & MASK_PBIT) ? true : false;
}

/*
 * Enters a subroutine via the present Program Descriptor in A as part
 * of an OPDC or DESC syllable. Also handles accidental entry
 */
void enterSubroutine(CPU *cpu, BIT descriptorCall)
{
        WORD48  aw = cpu->rA;  // local copy of word in A reg
        BIT     arg = (aw & MASK_ARGS) ? true : false;
        BIT     mode = (aw & MASK_MODE) ? true : false;
        //printf("enterSubroutine: MSFF=%u\n", cpu->bMSFF);
        if (arg && !cpu->bMSFF) {
                // just leave the Program Descriptor on TOS
        } else if (mode && !arg) {
                // ditto
        } else {
                // Now we are really going to enter the subroutine
                adjustBEmpty(cpu);
                if (!arg) {
                        // Accidental entry -- mark the stack
                        cpu->rB = buildMSCW(cpu);
                        cpu->bBROF = true;
                        adjustBEmpty(cpu);
                        cpu->rF = cpu->rS;
                }

                // Push a RCW
                cpu->rB = buildRCW(cpu, descriptorCall);
                cpu->bBROF = true;
                adjustBEmpty(cpu);

                // Fetch the first word of subroutine code
                cpu->rC = (aw & MASK_ADDR) >> SHFT_ADDR;
                cpu->rL = 0;
                // require fetch at SECL
                cpu->bPROF = false;

                // Fix up the rest of the registers
                if (arg) {
                        cpu->rF = cpu->rS;
                } else {
                        cpu->rF = (aw & MASK_FREG) >> SHFT_FREG;
                        // aw.[18:15]
                }
                cpu->bAROF = false;
                cpu->bBROF = false;
                cpu->bSALF = true;
                cpu->bMSFF = false;
                if (mode) {
                        cpu->bCWMF = 1;
                        cpu->rR/*TODO SHIFT*/ = 0;
                        // TODO: make this into mask and shift
                        cpu->rX = fieldInsert(cpu->rX, 18, 15, cpu->rS);
                        cpu->rS = 0;
                }
        }
}

#if 1
/*
 * Exits a subroutine by restoring the processor state from RCW and MSCW words
 * in the stack. "inline" indicates the C & L registers are NOT restored from the
 * RCW. The RCW is assumed to be in the B register, pointing to the MSCW.
 * The A register is not affected by cpu routine. If SALF & MSFF bits in the MSCW
 * are set, link back through the MSCWs until one is found that has either bit not
 * set, and store that MSCW at [R]+7. This is the last prior MSCW that actually
 * points to a RCW, thus skipping over any pending subroutine calls that are still
 * building their parameters in the stack. Returns results as follows:
 * 0 = entered by OPDC
 * 1 = entered by DESC
 * 2 = flag bit interrupt set, terminate operator
 */
int exitSubroutine(CPU *cpu, int in_line)
{
        int     result;

        if (OPERAND(cpu->rB)) {
                // flag bit not set
                result = 2;
                if (cpu->bNCSF) {
                        cpu->rI = (cpu->rI & 0x0F) | 0x80; // set I08: flag-bit
                        signalInterrupt(cpu->id, "XIT no FLAG");
                }
        } else {
                // flag bit is set
                result = applyRCW(cpu, cpu->rB, in_line, false);
                cpu->rX = cpu->rB & MASK_MANTISSA;
                // save F setting from RCW to restore S at end
                cpu->rS = cpu->rF;
                loadBviaS(cpu); // B = [S], fetch the MSCW
                applyMSCW(cpu, cpu->rB);

                if (cpu->bMSFF && cpu->bSALF) {
                        cpu->bQ06F = true; // set Q06F, not used except for display
                        do {
                                cpu->rS = (cpu->rB & MASK_FREG) >> SHFT_FREG;
                                loadBviaS(cpu); // B = [S], fetch prior MSCW
                        } while (cpu->rB & MASK_MSFF); // MSFF
                        cpu->rS = (cpu->rR/*TODO SHIFT*/<<6) + 7;
                        storeBviaS(cpu); // [S] = B, store last MSCW at [R]+7
                }
                cpu->rS = ((cpu->rX & MASK_FREG) >> SHFT_FREG) - 1;
                cpu->bBROF = false;
        }
        //printf("exitSubroutine: %d\n", result);
        return result;
}
#endif


