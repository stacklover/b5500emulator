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
***********************************************************************/

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
        cpu->r.Q01F = false;
        cpu->r.Q02F = false;
        cpu->r.Q03F = false;
        cpu->r.Q04F = false;
        cpu->r.Q05F = false;
        cpu->r.Q06F = false;
        cpu->r.Q07F = false;
        cpu->r.Q08F = false;
        cpu->r.Q09F = false;
        cpu->r.Y = 0;
        cpu->r.Z = 0;

        opcode = cpu->r.T;

again:
        variant = opcode >> 6;

        // force off by default (set by CRF)
        repeat = 0;

        switch (opcode & 077) {
        case 000:       // XX00: CMX, EXC: Exit character mode
                if (cpu->r.BROF) {
                        // store destination string
                        storeBviaS(cpu);
                }
                cpu->r.S = cpu->r.F;
                // B = [S], fetch the RCW
                loadBviaS(cpu);
                // 0=exit, 1=exit inline
                exitSubroutine(cpu, variant & 1);
                cpu->r.AROF = cpu->r.BROF = false;
                cpu->r.X = 0;
                cpu->r.M = 0;
                cpu->r.N = 0;
                cpu->r.CWMF = 0;
                break;

        case 002:       // XX02: BSD=Skip bit destination
                cpu->cycleCount += variant;
                t1 = cpu->r.K*6 + cpu->r.V + variant;
                while (t1 >= 48) {
                        if (cpu->r.BROF) {
                                // skipped off initial word, so
                                // [S] = B
                                storeBviaS(cpu);
                                // invalidate B
                                cpu->r.BROF = false;
                        }
                        ++cpu->r.S;
                        t1 -= 48;
                }
                cpu->r.V = t1 % 6;
                cpu->r.K = t1 / 6;
                break;

        case 003:       // XX03: BSS=Skip bit source
                cpu->cycleCount += variant;
                t1 = cpu->r.G*6 + cpu->r.H + variant;
                while (t1 >= 48) {
                        // skipped off initial word, so
                        ++cpu->r.M;
                        // invalidate A
                        cpu->r.AROF = false;
                        t1 -= 48;
                }
                cpu->r.H = t1 % 6;
                cpu->r.G = t1 / 6;
                break;

        case 004:       // XX04: RDA=Recall destination address
                cpu->cycleCount += variant;
                if (cpu->r.BROF) {
                        // [S] = B
                        storeBviaS(cpu);
                        cpu->r.BROF = false;
                }
                cpu->r.V = 0;
                cpu->r.S = cpu->r.F - variant;
                // B = [S]
                loadBviaS(cpu);
                cpu->r.BROF = false;
                t1 = cpu->r.B;
                cpu->r.S = t1 & MASKMEM;
                if (OPERAND(t1)) {
                        // if it's an operand,
                        // set K from [30:3]
                        cpu->r.K = (t1 >> 15) & 7;
                } else {
                        // otherwise, force K to zero and
                        cpu->r.K = 0;
                        // just take the side effect of any p-bit interrupt
                        presenceTest(cpu, t1);
                }
                break;

        case 005:       // XX05: TRW=Transfer words
                if (cpu->r.BROF) {
                        // [S] = B
                        storeBviaS(cpu);
                        cpu->r.BROF = false;
                }
                if (cpu->r.G || cpu->r.H) {
                        cpu->r.G = cpu->r.H = 0;
                        ++cpu->r.M;
                        cpu->r.AROF = false;
                }
                if (cpu->r.K || cpu->r.V) {
                        cpu->r.K = cpu->r.V = 0;
                        ++cpu->r.S;
                }
                if (variant) {
                        // count > 0
                        if (!cpu->r.AROF) {
                                // A = [M]
                                loadAviaM(cpu);
                        }
                        do {
                                // [S] = A
                                storeAviaS(cpu);
                                ++cpu->r.S;
                                ++cpu->r.M;
                                if (--variant) {
                                        // A = [M]
                                        loadAviaM(cpu);
                                } else {
                                        break;
                                }
                        } while (true);
                }
                cpu->r.AROF = false;
                break;

        case 006:       // XX06: SED=Set destination address
                cpu->cycleCount += variant;
                if (cpu->r.BROF) {
                        // [S] = B
                        storeBviaS(cpu);
                        cpu->r.BROF = false;
                }
                cpu->r.S = cpu->r.F - variant;
                cpu->r.K = cpu->r.V = 0;
                break;

        case 007:       // XX07: TDA=Transfer destination address
                cpu->cycleCount += 6;
                streamAdjustDestChar(cpu);
                if (cpu->r.BROF) {
                        // [S] = B, store B at dest addresss
                        storeBviaS(cpu);
                }
                // save M (not the way the hardware did it)
                t1 = cpu->r.M;
                // save G (ditto)
                t2 = cpu->r.G;
                // copy dest address to source address
                cpu->r.M = cpu->r.S;
                cpu->r.G = cpu->r.K;
                // save B
                cpu->r.A = cpu->r.B;
                cpu->r.AROF = cpu->r.BROF;
                if (!cpu->r.AROF) {
                        // A = [M], load A from source address
                        loadAviaM(cpu);
                }
                for (variant = 3; variant > 0; --variant) {
                        cpu->r.Y = fieldIsolate(cpu->r.A, cpu->r.G*6, 6);
                        cpu->r.B = (cpu->r.B << 6) | cpu->r.Y;
                        // make sure B is not exceeding 48 bits
                        cpu->r.B &= MASK_WORD48;
                        if (cpu->r.G < 7) {
                                ++cpu->r.G;
                        } else {
                                cpu->r.G = 0;
                                ++cpu->r.M;
                                // A = [M]
                                loadAviaM(cpu);
                        }
                }
                cpu->r.S = cpu->r.B & MASKMEM;
                cpu->r.K = (cpu->r.B >> 15) & 7;
                // restore M & G
                cpu->r.M = t1;
                cpu->r.G = t2;
                // invalidate A & B
                cpu->r.AROF = cpu->r.BROF = false;
                break;

        case 011:       // XX11: Control State ops
                switch (variant) {
                case 024:       // 2411: ZPI=Conditional Halt
                        if (cpu->r.US14X) {
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
                cpu->r.A = cpu->r.B;
                cpu->r.AROF = cpu->r.BROF;
                cpu->r.B = ((WORD48)cpu->r.K << 15) | cpu->r.S;
                // save S (not the way the hardware did it)
                t1 = cpu->r.S;
                cpu->r.S = cpu->r.F - variant;
                // [S] = B
                storeBviaS(cpu);
                // restore S
                cpu->r.S = t1;
                // restore B from A
                cpu->r.B = cpu->r.A;
                cpu->r.BROF = cpu->r.AROF;
                // invalidate A
                cpu->r.AROF = false;
                break;

        case 015:       // XX15: SSA=Store source address
                cpu->cycleCount += variant;
                streamAdjustSourceChar(cpu);
                // save B
                cpu->r.A = cpu->r.B;
                cpu->r.AROF = cpu->r.BROF;
                cpu->r.B = ((WORD48)cpu->r.G << 15) | cpu->r.M;
                // save M (not the way the hardware did it)
                t1 = cpu->r.M;
                cpu->r.M = cpu->r.F - variant;
                // [M] = B
                storeBviaM(cpu);
                // restore M
                cpu->r.M = t1;
                // restore B from A
                cpu->r.B = cpu->r.A;
                cpu->r.BROF = cpu->r.AROF;
                // invalidate A
                cpu->r.AROF = false;
                break;

        case 016:       // XX16: SFD=Skip forward destination
                cpu->cycleCount += (variant >> 3) + (variant & 7);
                streamAdjustDestChar(cpu);
                if (cpu->r.BROF && ((cpu->r.K + variant) >= 8)) {
                        // will skip off the current word,
                        // so store and invalidate B
                        storeBviaS(cpu);
                        cpu->r.BROF = false;
                }
                t1 = (cpu->r.S << 3) + cpu->r.K + variant;
                cpu->r.S = t1 >> 3;
                cpu->r.K = t1 & 7;
                break;

        case 017:       // XX17: SRD=Skip reverse destination
                cpu->cycleCount += (variant >> 3) + (variant & 7);
                streamAdjustDestChar(cpu);
                if (cpu->r.BROF && (cpu->r.K < variant)) {
                        // will skip off the current word,
                        // so store and invalidate B
                        storeBviaS(cpu);
                        cpu->r.BROF = false;
                }
                t1 = (cpu->r.S << 3) + cpu->r.K - variant;
                cpu->r.S = t1 >> 3;
                cpu->r.K = t1 & 7;
                break;

        case 022:       // XX22: SES=Set source address
                cpu->cycleCount += variant;
                cpu->r.M = cpu->r.F - variant;
                cpu->r.G = cpu->r.H = 0;
                cpu->r.AROF = false;
                break;

        case 024:       // XX24: TEQ=Test for equal
                streamAdjustSourceChar(cpu);
                if (!cpu->r.AROF) {
                        // A = [M]
                        loadAviaM(cpu);
                }
                t1 = fieldIsolate(cpu->r.A, cpu->r.G*6, 6);
                cpu->r.TFFF = (t1 == variant ? true : false);
                break;

        case 025:       // XX25: TNE=Test for not equal
                streamAdjustSourceChar(cpu);
                if (!cpu->r.AROF) {
                        // A = [M]
                        loadAviaM(cpu);
                }
                t1 = fieldIsolate(cpu->r.A, cpu->r.G*6, 6);
                cpu->r.TFFF = (t1 != variant ? true : false);
                break;

        case 026:       // XX26: TEG=Test for equal or greater
                streamAdjustSourceChar(cpu);
                if (!cpu->r.AROF) {
                        // A = [M]
                        loadAviaM(cpu);
                }
                t1 = collation[fieldIsolate(cpu->r.A, cpu->r.G*6, 6)];
                t2 = collation[variant];
                cpu->r.TFFF = (t1 >= t2 ? true : false);
                break;

        case 027:       // XX27: TGR=Test for greater
                streamAdjustSourceChar(cpu);
                if (!cpu->r.AROF) {
                        // A = [M]
                        loadAviaM(cpu);
                }
                t1 = collation[fieldIsolate(cpu->r.A, cpu->r.G*6, 6)];
                t2 = collation[variant];
                cpu->r.TFFF = (t1 > t2 ? true : false);
                break;

        case 030:       // XX30: SRS=Skip reverse source
                cpu->cycleCount += (variant >> 3) + (variant & 7);
                streamAdjustSourceChar(cpu);
                if (cpu->r.G < variant) {
                        // will skip off the current word
                        cpu->r.AROF = 0;
                }
                t1 = cpu->r.M*8 + cpu->r.G - variant;
                cpu->r.M = t1 >> 3;
                cpu->r.G = t1 & 7;
                break;

        case 031:       // XX31: SFS=Skip forward source
                cpu->cycleCount += (variant >> 3) + (variant & 7);
                streamAdjustSourceChar(cpu);
                if (cpu->r.G + variant >= 8) {
                        // will skip off the current word
                        cpu->r.AROF = false;
                }
                t1 = cpu->r.M*8 + cpu->r.G + variant;
                cpu->r.M = t1 >> 3;
                cpu->r.G = t1 & 7;
                break;

        case 032:       // XX32: xxx=Field subtract (aux)
                fieldArithmetic(cpu, variant, false);
                break;

        case 033:       // XX33: xxx=Field add (aux)
                fieldArithmetic(cpu, variant, true);
                break;

        case 034:       // XX34: TEL=Test for equal or less
                streamAdjustSourceChar(cpu);
                if (!cpu->r.AROF) {
                        // A = [M]
                        loadAviaM(cpu);
                }
                t1 = collation[fieldIsolate(cpu->r.A, cpu->r.G*6, 6)];
                t2 = collation[variant];
                cpu->r.TFFF = (t1 <= t2 ? true : false);
                break;

        case 035:       // XX35: TLS=Test for less
                streamAdjustSourceChar(cpu);
                if (!cpu->r.AROF) {
                        // A = [M]
                        loadAviaM(cpu);
                }
                t1 = collation[fieldIsolate(cpu->r.A, cpu->r.G*6, 6)];
                t2 = collation[variant];
                cpu->r.TFFF = (t1 < t2 ? true : false);
                break;

        case 036:       // XX36: TAN=Test for alphanumeric
                streamAdjustSourceChar(cpu);
                if (!cpu->r.AROF) {
                        // A = [M]
                        loadAviaM(cpu);
                }
                cpu->r.Y = t1 = fieldIsolate(cpu->r.A, cpu->r.G*6, 6);
                cpu->r.Z = variant;     // for display only
                if (collation[t1] > collation[variant]) {
                        cpu->r.TFFF = t1 == 0x20 ? false : (t1 == 0x3C ? false : true);
                        // alphanumeric unless | or !
                } else {
                        // alphanumeric if equal
                        cpu->r.Q03F = true;
                        // set Q03F (display only)
                        cpu->r.TFFF = (t1 == variant ? true : false);
                }
                break;

        case 037:       // XX37: BIT=Test bit
                if (!cpu->r.AROF) {
                        // A = [M]
                        loadAviaM(cpu);
                }
                cpu->r.Y = fieldIsolate(cpu->r.A, cpu->r.G*6, 6);
                t1 = cpu->r.Y >> (5-cpu->r.H);
                cpu->r.TFFF = (t1 & 1) == (variant & 1) ? true : false;
                break;

        case 040:       // XX40: INC=Increase TALLY
                if (variant) {
                        cpu->r.R = (cpu->r.R + variant) & 63;
                }
                // else it's a character-mode no-op
                break;

        case 041:       // XX41: STC=Store TALLY
                cpu->cycleCount += variant;
                // save B
                cpu->r.A = cpu->r.B;
                // invalidate A
                cpu->r.AROF = false;
                // save RCW address in B (why??)
                cpu->r.B = cpu->r.F;
                if (cpu->r.BROF) {
                        // [S] = A, save original B contents
                        storeAviaS(cpu);
                        cpu->r.BROF = false;
                }
                // move saved F address to A (why??)
                cpu->r.A = cpu->r.B;
                // copy the TALLY value to B
                cpu->r.B = cpu->r.R;
                // save S (not the way the hardware did it)
                t1 = cpu->r.S;
                cpu->r.S = cpu->r.F - variant;
                // [S] = B, store the TALLY value
                storeBviaS(cpu);
                // restore F address from A (why??)
                cpu->r.B = cpu->r.A;
                // restore S
                cpu->r.S = t1;
                // invalidate B
                cpu->r.BROF = false;
                break;

        case 042:       // XX42: SEC=Set TALLY
                cpu->r.R = variant;
                break;

        case 043:       // XX43: CRF=Call repeat field
                cpu->cycleCount += variant;
                // save B in A
                cpu->r.A = cpu->r.B;
                cpu->r.AROF = cpu->r.BROF;
                // save S (not the way the hardware did it)
                t1 = cpu->r.S;
                // compute parameter address
                cpu->r.S = cpu->r.F - variant;
                // B = [S]
                loadBviaS(cpu);
                // dynamic repeat count is low-order 6 bits
                variant = cpu->r.B & 63;
                // restore S
                cpu->r.S = t1;
                // restore B from A
                cpu->r.B = cpu->r.A;
                cpu->r.BROF = cpu->r.AROF;
                // invalidate A
                cpu->r.AROF = 0;
                if (!cpu->r.PROF) {
                        // fetch the program word, if necessary
                        loadPviaC(cpu);
                }
                opcode = fieldIsolate(cpu->r.P, cpu->r.L*12, 12);
                if (variant) {
                        // if repeat count from parameter > 0,
                        // apply it to the next syllable
                        cpu->r.T = opcode = (opcode & 00077) + (variant << 6);
                } else {
                        // otherwise, construct JFW (XX47) using repeat
                        // count from next syl (whew!)
                        cpu->r.T = opcode = (opcode & 07700) + 047;
                }

                // Since we are bypassing normal SECL behavior,
                // bump the instruction pointer here.
                // >>> override normal instruction fetch <<<
                repeat = 1;
                cpu->r.PROF = false;
                if (cpu->r.L < 3) {
                        ++cpu->r.L;
                } else {
                        cpu->r.L = 0;
                        ++cpu->r.C;
                }
                break;

        case 044:       // XX44: JNC=Jump out of loop conditional
                if (!cpu->r.TFFF) {
                        jumpOutOfLoop(cpu, variant);
                }
                break;

        case 045:       // XX45: JFC=Jump forward conditional
                if (!cpu->r.TFFF) {
                        // conditional on TFFF
                        cpu->cycleCount += (variant >> 2) + (variant & 3);
                        jumpSyllables(cpu, variant);
                }
                break;

        case 046:       // XX46: JNS=Jump out of loop
                jumpOutOfLoop(cpu, variant);
                break;

        case 047:       // XX47: JFW=Jump forward unconditional
                cpu->cycleCount += (variant >> 2) + (variant & 3);
                jumpSyllables(cpu, variant);
                break;

        case 050:       // XX50: RCA=Recall control address
                cpu->cycleCount += variant;
                // save B in A
                cpu->r.A = cpu->r.B;
                cpu->r.AROF = cpu->r.BROF;
                // save S (not the way the hardware did it)
                t1 = cpu->r.S;
                cpu->r.S = cpu->r.F - variant;
                // B = [S]
                loadBviaS(cpu);
                cpu->r.S = t1;
                t2 = cpu->r.B;
                if (DESCRIPTOR(t2)) {
                        // if it's a descriptor,
                        if (presenceTest(cpu, t2)) {
                                // if present, initiate a fetch to P
                                // get the word address,
                                cpu->r.C = cpu->r.B & MASKMEM;
                                // force L to zero and
                                cpu->r.L = 0;
                                // require fetch at SECL
                                cpu->r.PROF = false;
                        }
                } else {
                        cpu->r.C = t2 & MASKMEM;
                        t1 = (t2 >> 36) & 3;
                        if (t1 < 3) {
                                // if not a descriptor, increment the address
                                cpu->r.L = t1+1;
                        } else {
                                cpu->r.L = 0;
                                ++cpu->r.C;
                        }
                        // require fetch at SECL
                        cpu->r.PROF = false;
                }
                // restore B
                cpu->r.B = cpu->r.A;
                cpu->r.BROF = cpu->r.AROF;
                // invalidate A
                cpu->r.AROF = false;
                break;

        case 051:       // XX51: ENS=End loop
                cpu->cycleCount += 4;
                // save B in A
                cpu->r.A = cpu->r.B;
                cpu->r.AROF = cpu->r.BROF;
                t1 = cpu->r.X;
                // get repeat count
                t2 = (t1 & MASK_LCWrpt) >> SHFT_LCWrpt;
                // loop count exhausted?
                if (t2) {
                                // no, restore C, L, and P to loop again
                                cpu->r.C = (t1 & MASK_CREG) >> SHFT_CREG;
                                cpu->r.L = (t1 & MASK_LREG) >> SHFT_LREG;
                                // require fetch at SECL
                                cpu->r.PROF = false;
                                // store decremented count in X
                                --t2;
                                cpu->r.X = (cpu->r.X & ~MASK_LCWrpt) | (t2 << SHFT_LCWrpt);
                } else {
                        // save S (not the way the hardware did it)
                        t2 = cpu->r.S;
                        // get prior LCW addr from X value
                        cpu->r.S = (t1 & MASK_FREG) >> SHFT_FREG;
                        // B = [S], fetch prior LCW from stack
                        loadBviaS(cpu);
                        // restore S
                        cpu->r.S = t2;
                        // store prior LCW (less control bits) in X
                        cpu->r.X = cpu->r.B & MASK_MANTISSA;
                }
                // restore B
                cpu->r.B = cpu->r.A;
                cpu->r.BROF = cpu->r.AROF;
                // invalidate A
                cpu->r.AROF = false;
                break;

        case 052:       // XX52: BNS=Begin loop
                cpu->cycleCount += 4;
                // save B in A (note that BROF is not altered)
                cpu->r.A = cpu->r.B;

                // construct new LCW - insert repeat count
                // decrement count for first iteration
                t1 = (WORD48)(variant ? variant-1 : 0) << SHFT_LCWrpt;
                // insert L
                t1 |= (WORD48)(cpu->r.L) << SHFT_LREG;
                // insert C
                t1 |= (WORD48)(cpu->r.C) << SHFT_CREG;

                // save current loop control word
                // set control bits [0:2]=3
                cpu->r.B = cpu->r.X | INIT_LCW;

                // save S (not the way the hardware did it)
                t2 = cpu->r.S;
                // get F value from X value and ++
                cpu->r.S = ((cpu->r.X & MASK_FREG) >> SHFT_FREG) + 1;
                // [S] = B, save prior LCW in stack
                storeBviaS(cpu);
                // update F value in X
                t1 |= (WORD48)(cpu->r.S) << SHFT_FREG;
                cpu->r.X = t1;
                // restore S
                cpu->r.S = t2;

                // restore B (note that BROF is still relevant)
                cpu->r.B = cpu->r.A;
                // invalidate A
                cpu->r.AROF = false;
                break;

        case 053:       // XX53: RSA=Recall source address
                cpu->cycleCount += variant;
                // save B
                cpu->r.A = cpu->r.B;
                cpu->r.AROF = cpu->r.BROF;
                cpu->r.H = 0;
                cpu->r.M = cpu->r.F - variant;
                // B = [M]
                loadBviaM(cpu);
                t1 = cpu->r.B;
                cpu->r.M = t1 & MASKMEM;
                if (OPERAND(t1)) {
                        // if it's an operand,
                        // set G from [30:3]
                        cpu->r.G = (t1 >> 15) & 7;
                } else {
                        // otherwise, force G to zero and
                        cpu->r.G = 0;
                        // just take the side effect of any p-bit interrupt
                        presenceTest(cpu, t1);
                }
                // restore B from A
                cpu->r.B = cpu->r.A;
                cpu->r.BROF = cpu->r.AROF;
                // invalidate A
                cpu->r.AROF = false;
                break;

        case 054:       // XX54: SCA=Store control address
                cpu->cycleCount += variant;
                // save B
                cpu->r.A = cpu->r.B;
                cpu->r.AROF = cpu->r.BROF;
                // save S (not the way the hardware did it)
                t2 = cpu->r.S;
                // compute store address
                cpu->r.S = cpu->r.F - variant;
                cpu->r.B = cpu->r.C |
                        ((WORD48)cpu->r.F << 15) |
                        ((WORD48)cpu->r.L << 36);
                // [S] = B
                storeBviaS(cpu);
                // restore S
                cpu->r.S = t2;
                // restore B from A
                cpu->r.B = cpu->r.A;
                cpu->r.BROF = cpu->r.AROF;
                // invalidate A
                cpu->r.AROF = false;
                break;

        case 055:       // XX55: JRC=Jump reverse conditional
                if (!cpu->r.TFFF) {
                        // conditional on TFFF
                        cpu->cycleCount += (variant >> 2) + (variant & 3);
                        jumpSyllables(cpu, -variant);
                }
                break;

        case 056:       // XX56: TSA=Transfer source address
                streamAdjustSourceChar(cpu);
                if (cpu->r.BROF) {
                        // [S] = B, store B at dest addresss
                        storeBviaS(cpu);
                        cpu->r.BROF = false;
                }
                if (!cpu->r.AROF) {
                        // A = [M], load A from source address
                        loadAviaM(cpu);
                }
                for (variant = 3; variant > 0; --variant) {
                        cpu->r.Y = fieldIsolate(cpu->r.A, cpu->r.G*6, 6);
                        cpu->r.B = (cpu->r.B << 6) + cpu->r.Y;
                        // make sure B is not exceeding 48 bits
                        cpu->r.B &= MASK_WORD48;
                        if (cpu->r.G < 7) {
                                ++cpu->r.G;
                        } else {
                                cpu->r.G = 0;
                                ++cpu->r.M;
                                // A = [M]
                                loadAviaM(cpu);
                        }
                }
                cpu->r.M = cpu->r.B & MASKMEM;
                cpu->r.G = (cpu->r.B >> 15) & 7;
                // invalidate A
                cpu->r.AROF = false;
                break;

        case 057:       // XX57: JRV=Jump reverse unconditional
                cpu->cycleCount += (variant >> 2) + (variant & 3);
                jumpSyllables(cpu, -variant);
                break;

        case 060:       // XX60: CEQ=Compare equal
                compareSourceWithDest(cpu, variant, false);
                cpu->r.H = cpu->r.V = 0;
                // if !Q03F, S=D
                cpu->r.TFFF = cpu->r.Q03F ? false : true;
                break;

        case 061:       // XX61: CNE=Compare not equal
                compareSourceWithDest(cpu, variant, false);
                cpu->r.H = cpu->r.V = 0;
                // if Q03F, S!=D
                cpu->r.TFFF = cpu->r.Q03F ? true : false;
                break;

        case 062:       // XX62: CEG=Compare greater or equal
                compareSourceWithDest(cpu, variant, false);
                cpu->r.H = cpu->r.V = 0;
                // if Q03F&TFFF, S>D; if !Q03F, S=D
                cpu->r.TFFF = cpu->r.Q03F ? cpu->r.TFFF : true;
                break;

        case 063:       // XX63: CGR=Compare greater
                compareSourceWithDest(cpu, variant, false);
                cpu->r.H = cpu->r.V = 0;
                // if Q03F&TFFF, S>D
                cpu->r.TFFF = cpu->r.Q03F ? cpu->r.TFFF : false;
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
                cpu->r.H = cpu->r.V = 0;
                // if Q03F&!TFFF, S<D; if !Q03F, S=D
                cpu->r.TFFF = cpu->r.Q03F ? !cpu->r.TFFF : true;
                break;

        case 071:       // XX71: CLS=Compare less
                compareSourceWithDest(cpu, variant, false);
                cpu->r.H = cpu->r.V = 0;
                // if Q03F&!TFFF, S<D
                cpu->r.TFFF = cpu->r.Q03F ? !cpu->r.TFFF : false;
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
                cpu->r.TFFF = false;
                streamNumericToDest(cpu, variant, false);
                break;

        case 076:       // XX76: TRZ=Transfer source zones
                streamNumericToDest(cpu, variant, true);
                break;

        case 077:       // XX77: TRS=Transfer source characters
                streamCharacterToDest(cpu, variant);
                break;

        default:        // everything else is a no-op
                break;

        } // end switch for character mode operators

        // repeat for call repeat field operator
        if (repeat)
                goto again;
}
