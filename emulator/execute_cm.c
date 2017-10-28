/***********************************************************************
* b5500emulator
************************************************************************
* Copyright (c) 2016, Reinhard Meyer, DL5UY
* Licensed under the MIT License,
*       see LICENSE
* based on (C) work by Nigel Williams and Paul Kimpel
* see: https://github.com/pkimpel/retro-b5500
************************************************************************
* execute one character mode instruction
************************************************************************
* 2016-03-19  R.Meyer
*   Converted Paul's work from Javascript to C
* 2017-07-17  R.Meyer
*   Added "long long" qualifier to constants with long long value
*   changed "this" to "cpu" to avoid errors when using g++
* 2017-09-30  R.Meyer
*   overhaul of file names
* 2017-10-28  R.Meyer
*   adaption to new CPU structure
***********************************************************************/

#include <stdio.h>
#include "common.h"

/***********************************************************
*  Character Mode Syllables                                *
***********************************************************/
void b5500_execute_cm(CPU *cpu)
{
        WORD12  opcode;
        WORD12  variant;
        WORD48  t1, t2;
        BIT     repeat;

        // clear some vars
        cpu->bQ01F = false;
        cpu->bQ02F = false;
        cpu->bQ03F = false;
        cpu->bQ04F = false;
        cpu->bQ05F = false;
        cpu->bQ06F = false;
        cpu->bQ07F = false;
        cpu->bQ08F = false;
        cpu->bQ09F = false;
        cpu->rY = 0;
        cpu->rZ = 0;

        opcode = cpu->rT;

again:
	variant = opcode >> 6;
	opcode &= 077;

        // force off by default (set by CRF)
        repeat = 0;

        switch (opcode) {
        case 000:       // XX00: CMX, EXC: Exit character mode
                if (cpu->bBROF) {
                        // store destination string
                        storeBviaS(cpu);
                }
                cpu->rS = cpu->rF;
                // B = [S], fetch the RCW
                loadBviaS(cpu);
                // 0=exit, 1=exit inline
                exitSubroutine(cpu, variant & 1);
                cpu->bAROF = cpu->bBROF = false;
                cpu->rX = 0;
                cpu->rM = 0;
                cpu->rN = 0;
                cpu->bCWMF = 0;
                break;
	case 001:	// XX01: unknown
		break;

        case 002:       // XX02: BSD=Skip bit destination
                cpu->cycleCount += variant;
                t1 = cpu->rKV/*TODO K*/*6 + cpu->rKV/*TODO V*/ + variant;
                while (t1 >= 48) {
                        if (cpu->bBROF) {
                                // skipped off initial word, so
                                // [S] = B
                                storeBviaS(cpu);
                                // invalidate B
                                cpu->bBROF = false;
                        }
                        ++cpu->rS;
                        t1 -= 48;
                }
                cpu->rKV/*TODO V*/ = t1 % 6;
                cpu->rKV/*TODO K*/ = t1 / 6;
                break;

        case 003:       // XX03: BSS=Skip bit source
                cpu->cycleCount += variant;
                t1 = cpu->rGH/*TODO G*/*6 + cpu->rGH/*TODO H*/ + variant;
                while (t1 >= 48) {
                        // skipped off initial word, so
                        ++cpu->rM;
                        // invalidate A
                        cpu->bAROF = false;
                        t1 -= 48;
                }
                cpu->rGH/*TODO H*/ = t1 % 6;
                cpu->rGH/*TODO G*/ = t1 / 6;
                break;

        case 004:       // XX04: RDA=Recall destination address
                cpu->cycleCount += variant;
                if (cpu->bBROF) {
                        // [S] = B
                        storeBviaS(cpu);
                        cpu->bBROF = false;
                }
                cpu->rKV/*TODO V*/ = 0;
                cpu->rS = cpu->rF - variant;
                // B = [S]
                loadBviaS(cpu);
                cpu->bBROF = false;
                t1 = cpu->rB;
                cpu->rS = t1 & MASKMEM;
                if (OPERAND(t1)) {
                        // if it's an operand,
                        // set K from [30:3]
                        cpu->rKV/*TODO K*/ = (t1 >> 15) & 7;
                } else {
                        // otherwise, force K to zero and
                        cpu->rKV/*TODO K*/ = 0;
                        // just take the side effect of any p-bit interrupt
                        presenceTest(cpu, t1);
                }
                break;

        case 005:       // XX05: TRW=Transfer words
                if (cpu->bBROF) {
                        // [S] = B
                        storeBviaS(cpu);
                        cpu->bBROF = false;
                }
                if (cpu->rGH/*TODO G*/ || cpu->rGH/*TODO H*/) {
                        cpu->rGH/*TODO G*/ = cpu->rGH/*TODO H*/ = 0;
                        ++cpu->rM;
                        cpu->bAROF = false;
                }
                if (cpu->rKV/*TODO K*/ || cpu->rKV/*TODO V*/) {
                        cpu->rKV/*TODO K*/ = cpu->rKV/*TODO V*/ = 0;
                        ++cpu->rS;
                }
                if (variant) {
                        // count > 0
                        if (!cpu->bAROF) {
                                // A = [M]
                                loadAviaM(cpu);
                        }
                        do {
                                // [S] = A
                                storeAviaS(cpu);
                                ++cpu->rS;
                                ++cpu->rM;
                                if (--variant) {
                                        // A = [M]
                                        loadAviaM(cpu);
                                } else {
                                        break;
                                }
                        } while (true);
                }
                cpu->bAROF = false;
                break;

        case 006:       // XX06: SED=Set destination address
                cpu->cycleCount += variant;
                if (cpu->bBROF) {
                        // [S] = B
                        storeBviaS(cpu);
                        cpu->bBROF = false;
                }
                cpu->rS = cpu->rF - variant;
                cpu->rKV/*TODO K*/ = cpu->rKV/*TODO V*/ = 0;
                break;

        case 007:       // XX07: TDA=Transfer destination address
                cpu->cycleCount += 6;
                streamAdjustDestChar(cpu);
                if (cpu->bBROF) {
                        // [S] = B, store B at dest addresss
                        storeBviaS(cpu);
                }
                // save M (not the way the hardware did it)
                t1 = cpu->rM;
                // save G (ditto)
                t2 = cpu->rGH/*TODO G*/;
                // copy dest address to source address
                cpu->rM = cpu->rS;
                cpu->rGH/*TODO G*/ = cpu->rKV/*TODO K*/;
                // save B
                cpu->rA = cpu->rB;
                cpu->bAROF = cpu->bBROF;
                if (!cpu->bAROF) {
                        // A = [M], load A from source address
                        loadAviaM(cpu);
                }
                for (variant = 3; variant > 0; --variant) {
                        cpu->rY = fieldIsolate(cpu->rA, cpu->rGH/*TODO G*/*6, 6);
                        cpu->rB = (cpu->rB << 6) | cpu->rY;
                        // make sure B is not exceeding 48 bits
                        cpu->rB &= MASK_WORD48;
                        if (cpu->rGH/*TODO G*/ < 7) {
                                ++cpu->rGH/*TODO G*/;
                        } else {
                                cpu->rGH/*TODO G*/ = 0;
                                ++cpu->rM;
                                // A = [M]
                                loadAviaM(cpu);
                        }
                }
                cpu->rS = cpu->rB & MASKMEM;
                cpu->rKV/*TODO K*/ = (cpu->rB >> 15) & 7;
                // restore M & G
                cpu->rM = t1;
                cpu->rGH/*TODO G*/ = t2;
                // invalidate A & B
                cpu->bAROF = cpu->bBROF = false;
                break;

        case 011:       // XX11: Control State ops
                switch (variant) {
                case 024:       // 2411: ZPI=Conditional Halt
                        if (cpu->bUS14X) {
                                // STOP OPERATOR switch on
                                stop(cpu);
                        }
                        break;

                case 030:       // 3011: SFI=Store for Interrupt
                        storeForInterrupt(cpu, false, false, "cmSFI");
                        break;

                case 034:       // 3411: SFT=Store for Test
                        storeForInterrupt(cpu, false, true, "cmSFT");
                        break;

                default:        // Anything else is a no-op
                        break;
                } // end switch for XX11 ops
                break;

        case 012:       // XX12: TBN=Transfer blanks for non-numeric
                streamBlankForNonNumeric(cpu, variant);
                break;

        case 014:       // XX14: SDA=Store destination address
                cpu->cycleCount += variant;
                streamAdjustDestChar(cpu);
                // save B
                cpu->rA = cpu->rB;
                cpu->bAROF = cpu->bBROF;
                cpu->rB = ((WORD48)cpu->rKV/*TODO K*/ << 15) | cpu->rS;
                // save S (not the way the hardware did it)
                t1 = cpu->rS;
                cpu->rS = cpu->rF - variant;
                // [S] = B
                storeBviaS(cpu);
                // restore S
                cpu->rS = t1;
                // restore B from A
                cpu->rB = cpu->rA;
                cpu->bBROF = cpu->bAROF;
                // invalidate A
                cpu->bAROF = false;
                break;

        case 015:       // XX15: SSA=Store source address
                cpu->cycleCount += variant;
                streamAdjustSourceChar(cpu);
                // save B
                cpu->rA = cpu->rB;
                cpu->bAROF = cpu->bBROF;
                cpu->rB = ((WORD48)cpu->rGH/*TODO G*/ << 15) | cpu->rM;
                // save M (not the way the hardware did it)
                t1 = cpu->rM;
                cpu->rM = cpu->rF - variant;
                // [M] = B
                storeBviaM(cpu);
                // restore M
                cpu->rM = t1;
                // restore B from A
                cpu->rB = cpu->rA;
                cpu->bBROF = cpu->bAROF;
                // invalidate A
                cpu->bAROF = false;
                break;

        case 016:       // XX16: SFD=Skip forward destination
                cpu->cycleCount += (variant >> 3) + (variant & 7);
                streamAdjustDestChar(cpu);
                if (cpu->bBROF && ((cpu->rKV/*TODO K*/ + variant) >= 8)) {
                        // will skip off the current word,
                        // so store and invalidate B
                        storeBviaS(cpu);
                        cpu->bBROF = false;
                }
                t1 = (cpu->rS << 3) + cpu->rKV/*TODO K*/ + variant;
                cpu->rS = t1 >> 3;
                cpu->rKV/*TODO K*/ = t1 & 7;
                break;

        case 017:       // XX17: SRD=Skip reverse destination
                cpu->cycleCount += (variant >> 3) + (variant & 7);
                streamAdjustDestChar(cpu);
                if (cpu->bBROF && (cpu->rKV/*TODO K*/ < variant)) {
                        // will skip off the current word,
                        // so store and invalidate B
                        storeBviaS(cpu);
                        cpu->bBROF = false;
                }
                t1 = (cpu->rS << 3) + cpu->rKV/*TODO K*/ - variant;
                cpu->rS = t1 >> 3;
                cpu->rKV/*TODO K*/ = t1 & 7;
                break;

        case 022:       // XX22: SES=Set source address
                cpu->cycleCount += variant;
                cpu->rM = cpu->rF - variant;
                cpu->rGH/*TODO G*/ = cpu->rGH/*TODO H*/ = 0;
                cpu->bAROF = false;
                break;

        case 024:       // XX24: TEQ=Test for equal
                streamAdjustSourceChar(cpu);
                if (!cpu->bAROF) {
                        // A = [M]
                        loadAviaM(cpu);
                }
                t1 = fieldIsolate(cpu->rA, cpu->rGH/*TODO G*/*6, 6);
                cpu->bTFFF = (t1 == variant ? true : false);
                break;

        case 025:       // XX25: TNE=Test for not equal
                streamAdjustSourceChar(cpu);
                if (!cpu->bAROF) {
                        // A = [M]
                        loadAviaM(cpu);
                }
                t1 = fieldIsolate(cpu->rA, cpu->rGH/*TODO G*/*6, 6);
                cpu->bTFFF = (t1 != variant ? true : false);
                break;

        case 026:       // XX26: TEG=Test for equal or greater
                streamAdjustSourceChar(cpu);
                if (!cpu->bAROF) {
                        // A = [M]
                        loadAviaM(cpu);
                }
                t1 = collation[fieldIsolate(cpu->rA, cpu->rGH/*TODO G*/*6, 6)];
                t2 = collation[variant];
                cpu->bTFFF = (t1 >= t2 ? true : false);
                break;

        case 027:       // XX27: TGR=Test for greater
                streamAdjustSourceChar(cpu);
                if (!cpu->bAROF) {
                        // A = [M]
                        loadAviaM(cpu);
                }
                t1 = collation[fieldIsolate(cpu->rA, cpu->rGH/*TODO G*/*6, 6)];
                t2 = collation[variant];
                cpu->bTFFF = (t1 > t2 ? true : false);
                break;

        case 030:       // XX30: SRS=Skip reverse source
                cpu->cycleCount += (variant >> 3) + (variant & 7);
                streamAdjustSourceChar(cpu);
                if (cpu->rGH/*TODO G*/ < variant) {
                        // will skip off the current word
                        cpu->bAROF = 0;
                }
                t1 = cpu->rM*8 + cpu->rGH/*TODO G*/ - variant;
                cpu->rM = t1 >> 3;
                cpu->rGH/*TODO G*/ = t1 & 7;
                break;

        case 031:       // XX31: SFS=Skip forward source
                cpu->cycleCount += (variant >> 3) + (variant & 7);
                streamAdjustSourceChar(cpu);
                if (cpu->rGH/*TODO G*/ + variant >= 8) {
                        // will skip off the current word
                        cpu->bAROF = false;
                }
                t1 = cpu->rM*8 + cpu->rGH/*TODO G*/ + variant;
                cpu->rM = t1 >> 3;
                cpu->rGH/*TODO G*/ = t1 & 7;
                break;

        case 032:       // XX32: xxx=Field subtract (aux)
                fieldArithmetic(cpu, variant, false);
                break;

        case 033:       // XX33: xxx=Field add (aux)
                fieldArithmetic(cpu, variant, true);
                break;

        case 034:       // XX34: TEL=Test for equal or less
                streamAdjustSourceChar(cpu);
                if (!cpu->bAROF) {
                        // A = [M]
                        loadAviaM(cpu);
                }
                t1 = collation[fieldIsolate(cpu->rA, cpu->rGH/*TODO G*/*6, 6)];
                t2 = collation[variant];
                cpu->bTFFF = (t1 <= t2 ? true : false);
                break;

        case 035:       // XX35: TLS=Test for less
                streamAdjustSourceChar(cpu);
                if (!cpu->bAROF) {
                        // A = [M]
                        loadAviaM(cpu);
                }
                t1 = collation[fieldIsolate(cpu->rA, cpu->rGH/*TODO G*/*6, 6)];
                t2 = collation[variant];
                cpu->bTFFF = (t1 < t2 ? true : false);
                break;

        case 036:       // XX36: TAN=Test for alphanumeric
                streamAdjustSourceChar(cpu);
                if (!cpu->bAROF) {
                        // A = [M]
                        loadAviaM(cpu);
                }
                cpu->rY = t1 = fieldIsolate(cpu->rA, cpu->rGH/*TODO G*/*6, 6);
                cpu->rZ = variant;     // for display only
                if (collation[t1] > collation[variant]) {
                        cpu->bTFFF = t1 == 0x20 ? false : (t1 == 0x3C ? false : true);
                        // alphanumeric unless | or !
                } else {
                        // alphanumeric if equal
                        cpu->bQ03F = true;
                        // set Q03F (display only)
                        cpu->bTFFF = (t1 == variant ? true : false);
                }
                break;

        case 037:       // XX37: BIT=Test bit
                if (!cpu->bAROF) {
                        // A = [M]
                        loadAviaM(cpu);
                }
                cpu->rY = fieldIsolate(cpu->rA, cpu->rGH/*TODO G*/*6, 6);
                t1 = cpu->rY >> (5-cpu->rGH/*TODO H*/);
                cpu->bTFFF = (t1 & 1) == (variant & 1) ? true : false;
                break;

        case 040:       // XX40: INC=Increase TALLY
                if (variant) {
                        cpu->rR = (cpu->rR + variant) & 63;
                }
                // else it's a character-mode no-op
                break;

        case 041:       // XX41: STC=Store TALLY
                cpu->cycleCount += variant;
                // save B
                cpu->rA = cpu->rB;
                // invalidate A
                cpu->bAROF = false;
                // save RCW address in B (why??)
                cpu->rB = cpu->rF;
                if (cpu->bBROF) {
                        // [S] = A, save original B contents
                        storeAviaS(cpu);
                        cpu->bBROF = false;
                }
                // move saved F address to A (why??)
                cpu->rA = cpu->rB;
                // copy the TALLY value to B
                cpu->rB = cpu->rR;
                // save S (not the way the hardware did it)
                t1 = cpu->rS;
                cpu->rS = cpu->rF - variant;
                // [S] = B, store the TALLY value
                storeBviaS(cpu);
                // restore F address from A (why??)
                cpu->rB = cpu->rA;
                // restore S
                cpu->rS = t1;
                // invalidate B
                cpu->bBROF = false;
                break;

        case 042:       // XX42: SEC=Set TALLY
                cpu->rR = variant;
                break;

        case 043:       // XX43: CRF=Call repeat field
                cpu->cycleCount += variant;
                // save B in A
                cpu->rA = cpu->rB;
                cpu->bAROF = cpu->bBROF;
                // save S (not the way the hardware did it)
                t1 = cpu->rS;
                // compute parameter address
                cpu->rS = cpu->rF - variant;
                // B = [S]
                loadBviaS(cpu);
                // dynamic repeat count is low-order 6 bits
                variant = cpu->rB & 63;
                // restore S
                cpu->rS = t1;
                // restore B from A
                cpu->rB = cpu->rA;
                cpu->bBROF = cpu->bAROF;
                // invalidate A
                cpu->bAROF = 0;
                if (!cpu->bPROF) {
                        // fetch the program word, if necessary
                        loadPviaC(cpu);
                }
                opcode = fieldIsolate(cpu->rP, cpu->rL*12, 12);
                if (variant) {
                        // if repeat count from parameter > 0,
                        // apply it to the next syllable
                        cpu->rT = opcode = (opcode & 00077) + (variant << 6);
                } else {
                        // otherwise, construct JFW (XX47) using repeat
                        // count from next syl (whew!)
                        cpu->rT = opcode = (opcode & 07700) + 047;
                }

                // Since we are bypassing normal SECL behavior,
                // bump the instruction pointer here.
                // >>> override normal instruction fetch <<<
                repeat = 1;
                cpu->bPROF = false;
                if (cpu->rL < 3) {
                        ++cpu->rL;
                } else {
                        cpu->rL = 0;
                        ++cpu->rC;
                }
                break;

/***********************************************************************
* XX44: JNC=Jump out of loop conditional
* XX46: JNS=Jump out of loop
***********************************************************************/
	case 044:
		if (cpu->bTFFF)
			return; // TFFF set - no jump
		// else fall through
	case 046:
		// save S
		t1 = cpu->rS;
		// get prior LCW addr from X value
		cpu->rS = (cpu->rX & MASK_FREG) >> SHFT_FREG;
		loadAviaS(cpu); // A = [S], fetch prior LCW from stack
		// invalidate A
		cpu->bAROF = 0;
		// store prior LCW (39 bits: less control bits) in X
		cpu->rX = cpu->rA & MASK_MANTISSA;
		// restore S
		cpu->rS = t1;
		if (variant) {
			// convert C:L to word, adjust it, convert back
			t1 = (cpu->rC << 2) + cpu->rL;
			t1 += variant;
			cpu->rC = t1 >> 2;
			cpu->rL = t1 & 3;
			cpu->bPROF = false;
		}
		return;

/***********************************************************************
* XX45: JFC=Jump forward conditional
* XX47: JFW=Jump forward unconditional
* XX55: JRC=Jump reverse conditional
* XX57: JRV=Jump reverse unconditional
***********************************************************************/
	case 045:
	case 055:
		if (cpu->bTFFF)
			return; // TFFF set - no jump
		// else fall through
	case 047:
	case 057:
		if (variant) {
			// convert C:L to word, adjust it, convert back
			t1 = (cpu->rC << 2) + cpu->rL;
			if (opcode & 010) {
				// reverse
				t1 -= variant;
			} else {
				// forward
				t1 += variant;
			}
			cpu->rC = t1 >> 2;
			cpu->rL = t1 & 3;
			cpu->bPROF = false;
		}
		return;

        case 050:       // XX50: RCA=Recall control address
                cpu->cycleCount += variant;
                // save B in A
                cpu->rA = cpu->rB;
                cpu->bAROF = cpu->bBROF;
                // save S (not the way the hardware did it)
                t1 = cpu->rS;
                cpu->rS = cpu->rF - variant;
                // B = [S]
                loadBviaS(cpu);
                cpu->rS = t1;
                t2 = cpu->rB;
                if (DESCRIPTOR(t2)) {
                        // if it's a descriptor,
                        if (presenceTest(cpu, t2)) {
                                // if present, initiate a fetch to P
                                // get the word address,
                                cpu->rC = cpu->rB & MASKMEM;
                                // force L to zero and
                                cpu->rL = 0;
                                // require fetch at SECL
                                cpu->bPROF = false;
                        }
                } else {
                        cpu->rC = t2 & MASKMEM;
                        t1 = (t2 >> 36) & 3;
                        if (t1 < 3) {
                                // if not a descriptor, increment the address
                                cpu->rL = t1+1;
                        } else {
                                cpu->rL = 0;
                                ++cpu->rC;
                        }
                        // require fetch at SECL
                        cpu->bPROF = false;
                }
                // restore B
                cpu->rB = cpu->rA;
                cpu->bBROF = cpu->bAROF;
                // invalidate A
                cpu->bAROF = false;
                break;

        case 051:       // XX51: ENS=End loop
                cpu->cycleCount += 4;
                // save B in A
                cpu->rA = cpu->rB;
                cpu->bAROF = cpu->bBROF;
                t1 = cpu->rX;
                // get repeat count
                t2 = (t1 & MASK_LCWrpt) >> SHFT_LCWrpt;
                // loop count exhausted?
                if (t2) {
                                // no, restore C, L, and P to loop again
                                cpu->rC = (t1 & MASK_CREG) >> SHFT_CREG;
                                cpu->rL = (t1 & MASK_LREG) >> SHFT_LREG;
                                // require fetch at SECL
                                cpu->bPROF = false;
                                // store decremented count in X
                                --t2;
                                cpu->rX = (cpu->rX & ~MASK_LCWrpt) | (t2 << SHFT_LCWrpt);
                } else {
                        // save S (not the way the hardware did it)
                        t2 = cpu->rS;
                        // get prior LCW addr from X value
                        cpu->rS = (t1 & MASK_FREG) >> SHFT_FREG;
                        // B = [S], fetch prior LCW from stack
                        loadBviaS(cpu);
                        // restore S
                        cpu->rS = t2;
                        // store prior LCW (less control bits) in X
                        cpu->rX = cpu->rB & MASK_MANTISSA;
                }
                // restore B
                cpu->rB = cpu->rA;
                cpu->bBROF = cpu->bAROF;
                // invalidate A
                cpu->bAROF = false;
                break;

        case 052:       // XX52: BNS=Begin loop
                cpu->cycleCount += 4;
                // save B in A (note that BROF is not altered)
                cpu->rA = cpu->rB;

                // construct new LCW - insert repeat count
                // decrement count for first iteration
                t1 = (WORD48)(variant ? variant-1 : 0) << SHFT_LCWrpt;
                // insert L
                t1 |= (WORD48)(cpu->rL) << SHFT_LREG;
                // insert C
                t1 |= (WORD48)(cpu->rC) << SHFT_CREG;

                // save current loop control word
                // set control bits [0:2]=3
                cpu->rB = cpu->rX | INIT_LCW;

                // save S (not the way the hardware did it)
                t2 = cpu->rS;
                // get F value from X value and ++
                cpu->rS = ((cpu->rX & MASK_FREG) >> SHFT_FREG) + 1;
                // [S] = B, save prior LCW in stack
                storeBviaS(cpu);
                // update F value in X
                t1 |= (WORD48)(cpu->rS) << SHFT_FREG;
                cpu->rX = t1;
                // restore S
                cpu->rS = t2;

                // restore B (note that BROF is still relevant)
                cpu->rB = cpu->rA;
                // invalidate A
                cpu->bAROF = false;
                break;

        case 053:       // XX53: RSA=Recall source address
                cpu->cycleCount += variant;
                // save B
                cpu->rA = cpu->rB;
                cpu->bAROF = cpu->bBROF;
                cpu->rGH/*TODO H*/ = 0;
                cpu->rM = cpu->rF - variant;
                // B = [M]
                loadBviaM(cpu);
                t1 = cpu->rB;
                cpu->rM = t1 & MASKMEM;
                if (OPERAND(t1)) {
                        // if it's an operand,
                        // set G from [30:3]
                        cpu->rGH/*TODO G*/ = (t1 >> 15) & 7;
                } else {
                        // otherwise, force G to zero and
                        cpu->rGH/*TODO G*/ = 0;
                        // just take the side effect of any p-bit interrupt
                        presenceTest(cpu, t1);
                }
                // restore B from A
                cpu->rB = cpu->rA;
                cpu->bBROF = cpu->bAROF;
                // invalidate A
                cpu->bAROF = false;
                break;

        case 054:       // XX54: SCA=Store control address
                cpu->cycleCount += variant;
                // save B
                cpu->rA = cpu->rB;
                cpu->bAROF = cpu->bBROF;
                // save S (not the way the hardware did it)
                t2 = cpu->rS;
                // compute store address
                cpu->rS = cpu->rF - variant;
                cpu->rB = cpu->rC |
                        ((WORD48)cpu->rF << 15) |
                        ((WORD48)cpu->rL << 36);
                // [S] = B
                storeBviaS(cpu);
                // restore S
                cpu->rS = t2;
                // restore B from A
                cpu->rB = cpu->rA;
                cpu->bBROF = cpu->bAROF;
                // invalidate A
                cpu->bAROF = false;
                break;

        case 056:       // XX56: TSA=Transfer source address
                streamAdjustSourceChar(cpu);
                if (cpu->bBROF) {
                        // [S] = B, store B at dest addresss
                        storeBviaS(cpu);
                        cpu->bBROF = false;
                }
                if (!cpu->bAROF) {
                        // A = [M], load A from source address
                        loadAviaM(cpu);
                }
                for (variant = 3; variant > 0; --variant) {
                        cpu->rY = fieldIsolate(cpu->rA, cpu->rGH/*TODO G*/*6, 6);
                        cpu->rB = (cpu->rB << 6) + cpu->rY;
                        // make sure B is not exceeding 48 bits
                        cpu->rB &= MASK_WORD48;
                        if (cpu->rGH/*TODO G*/ < 7) {
                                ++cpu->rGH/*TODO G*/;
                        } else {
                                cpu->rGH/*TODO G*/ = 0;
                                ++cpu->rM;
                                // A = [M]
                                loadAviaM(cpu);
                        }
                }
                cpu->rM = cpu->rB & MASKMEM;
                cpu->rGH/*TODO G*/ = (cpu->rB >> 15) & 7;
                // invalidate A
                cpu->bAROF = false;
                break;

        case 060:       // XX60: CEQ=Compare equal
                compareSourceWithDest(cpu, variant, false);
                cpu->rGH/*TODO H*/ = cpu->rKV/*TODO V*/ = 0;
                // if !Q03F, S=D
                cpu->bTFFF = cpu->bQ03F ? false : true;
                break;

        case 061:       // XX61: CNE=Compare not equal
                compareSourceWithDest(cpu, variant, false);
                cpu->rGH/*TODO H*/ = cpu->rKV/*TODO V*/ = 0;
                // if Q03F, S!=D
                cpu->bTFFF = cpu->bQ03F ? true : false;
                break;

        case 062:       // XX62: CEG=Compare greater or equal
                compareSourceWithDest(cpu, variant, false);
                cpu->rGH/*TODO H*/ = cpu->rKV/*TODO V*/ = 0;
                // if Q03F&TFFF, S>D; if !Q03F, S=D
                cpu->bTFFF = cpu->bQ03F ? cpu->bTFFF : true;
                break;

        case 063:       // XX63: CGR=Compare greater
                compareSourceWithDest(cpu, variant, false);
                cpu->rGH/*TODO H*/ = cpu->rKV/*TODO V*/ = 0;
                // if Q03F&TFFF, S>D
                cpu->bTFFF = cpu->bQ03F ? cpu->bTFFF : false;
                break;

        case 064:       // XX64: BIS=Set bit
                streamBitsToDest(cpu, variant, MASK_WORD48);
                break;

        case 065:       // XX65: BIR=Reset bit
                streamBitsToDest(cpu, variant, 0);
                break;

        case 066:       // XX66: OCV=Output convert
                streamOutputConvert(cpu, variant);
                break;

        case 067:       // XX67: ICV=Input convert
                streamInputConvert(cpu, variant);
                break;

        case 070:       // XX70: CEL=Compare equal or less
                compareSourceWithDest(cpu, variant, false);
                cpu->rGH/*TODO H*/ = cpu->rKV/*TODO V*/ = 0;
                // if Q03F&!TFFF, S<D; if !Q03F, S=D
                cpu->bTFFF = cpu->bQ03F ? !cpu->bTFFF : true;
                break;

        case 071:       // XX71: CLS=Compare less
                compareSourceWithDest(cpu, variant, false);
                cpu->rGH/*TODO H*/ = cpu->rKV/*TODO V*/ = 0;
                // if Q03F&!TFFF, S<D
                cpu->bTFFF = cpu->bQ03F ? !cpu->bTFFF : false;
                break;

        case 072:       // XX72: FSU=Field subtract
                fieldArithmetic(cpu, variant, false);
                break;

        case 073:       // XX73: FAD=Field add
                fieldArithmetic(cpu, variant, true);
                break;

        case 074:       // XX74: TRP=Transfer program characters
                streamProgramToDest(cpu, variant);
                break;

        case 075:       // XX75: TRN=Transfer source numerics
                // initialize for negative sign test
                cpu->bTFFF = false;
                streamNumericToDest(cpu, variant, false);
                break;

        case 076:       // XX76: TRZ=Transfer source zones
                streamNumericToDest(cpu, variant, true);
                break;

        case 077:       // XX77: TRS=Transfer source characters
                streamCharacterToDest(cpu, variant);
                break;

        default:        // everything else is a no-op
		// warn about it
		printf("*\tWARNING: charmode opcode %04o execute as no-op\n", opcode);
                break;

        } // end switch for character mode operators

        // repeat for call repeat field operator
        if (repeat)
                goto again;
}
