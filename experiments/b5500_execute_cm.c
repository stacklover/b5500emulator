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
***********************************************************************/

#include "b5500_common.h"

/***********************************************************
*  Character Mode Syllables                                *
***********************************************************/
void b5500_execute_cm(CPU *this)
{
	WORD12	opcode;
	WORD12	variant;
	WORD48	t1, t2;
	BIT		noSECL;

	opcode = this->r.T; 
	do {
		variant = opcode >> 6;
		// force off by default (set by CRF)
		noSECL = 0;

		switch (opcode & 077) {
		case 000:	// XX00: CMX, EXC: Exit character mode
			if (this->r.BROF) {
				// store destination string
				storeBviaS(this);
			}
			this->r.S = this->r.F;
			// B = [S], fetch the RCW
			loadBviaS(this);
			// 0=exit, 1=exit inline
			exitSubroutine(this, variant & 1);
			this->r.AROF = this->r.BROF = false;
			this->r.X = 0;
			this->r.M = 0;
			this->r.N = 0;
			this->r.CWMF = 0;
			break;

		case 002:	// XX02: BSD=Skip bit destination
			this->cycleCount += variant;
			t1 = this->r.K*6 + this->r.V + variant;
			while (t1 >= 48) {
				if (this->r.BROF) {
					// skipped off initial word, so
					// [S] = B
					storeBviaS(this);
					// invalidate B
					this->r.BROF = false;
				}
				++this->r.S;
				t1 -= 48;
			}
			this->r.V = t1 % 6;
			this->r.K = t1 / 6;
			break;

		case 003:	// XX03: BSS=Skip bit source
			this->cycleCount += variant;
			t1 = this->r.G*6 + this->r.H + variant;
			while (t1 >= 48) {
				// skipped off initial word, so
				++this->r.M;
				// invalidate A
				this->r.AROF = false;
				t1 -= 48;
			}
			this->r.H = t1 % 6;
			this->r.G = t1 / 6;
			break;

		case 004:	// XX04: RDA=Recall destination address
			this->cycleCount += variant;
			if (this->r.BROF) {
				// [S] = B
				storeBviaS(this);
				this->r.BROF = false;
			}
			this->r.V = 0;
			this->r.S = this->r.F - variant;
			// B = [S]
			loadBviaS(this);
			this->r.BROF = false;
			t1 = this->r.B;
			this->r.S = t1 & MASKMEM;
			if (OPERAND(t1)) {
				// if it's an operand,
				// set K from [30:3]
				this->r.K = (t1 >> 15) & 7;
			} else {
				// otherwise, force K to zero and
				this->r.K = 0;
				// just take the side effect of any p-bit interrupt
				presenceTest(this, t1);
			}
			break;

		case 005:	// XX05: TRW=Transfer words
			if (this->r.BROF) {
				// [S] = B
				storeBviaS(this);
				this->r.BROF = false;
			}
			if (this->r.G || this->r.H) {
				this->r.G = this->r.H = 0;
				++this->r.M;
				this->r.AROF = false;
			}
			if (this->r.K || this->r.V) {
				this->r.K = this->r.V = 0;
				++this->r.S;
			}
			if (variant) {
				// count > 0
				if (!this->r.AROF) {
					// A = [M]
					loadAviaM(this);
				}
				do {
					// [S] = A
					storeAviaS(this);
					++this->r.S;
					++this->r.M;
					if (--variant) {
						// A = [M]
						loadAviaM(this);
					} else {
						break;
					}
				} while (true);
			}
			this->r.AROF = false;
			break;

		case 006:	// XX06: SED=Set destination address
			this->cycleCount += variant;
			if (this->r.BROF) {
				// [S] = B
				storeBviaS(this);
				this->r.BROF = false;
			}
			this->r.S = this->r.F - variant;
			this->r.K = this->r.V = 0;
			break;

		case 007:	// XX07: TDA=Transfer destination address
			this->cycleCount += 6;
			streamAdjustDestChar(this);
			if (this->r.BROF) {
				// [S] = B, store B at dest addresss
				storeBviaS(this);
			}
			// save M (not the way the hardware did it)
			t1 = this->r.M;
			// save G (ditto)
			t2 = this->r.G;
			// copy dest address to source address
			this->r.M = this->r.S;
			this->r.G = this->r.K;
			// save B
			this->r.A = this->r.B;
			this->r.AROF = this->r.BROF;
			if (!this->r.AROF) {
				// A = [M], load A from source address
				loadAviaM(this);
			}
			for (variant = 3; variant > 0; --variant) {
				this->r.Y = fieldIsolate(this->r.A, this->r.G*6, 6);
				this->r.B = (this->r.B << 6) | this->r.Y;
				// make sure B is not exceeding 48 bits
				this->r.B &= 07777777777777777ll;
				if (this->r.G < 7) {
					++this->r.G;
				} else {
					this->r.G = 0;
					++this->r.M;
					// A = [M]
					loadAviaM(this);
				}
			}
			this->r.S = this->r.B & MASKMEM;
			this->r.K = (this->r.B >> 15) & 7;
			// restore M & G
			this->r.M = t1;
			this->r.G = t2;
			// invalidate A & B
			this->r.AROF = this->r.BROF = false;
			break;

		case 011:	// XX11: Control State ops
			switch (variant) {
			case 024:	// 2411: ZPI=Conditional Halt
				if (this->r.US14X) {
					// STOP OPERATOR switch on
					stop(this);
				}
				break;

			case 030:	// 3011: SFI=Store for Interrupt
				storeForInterrupt(this, false, false);
				break;

			case 034:	// 3411: SFT=Store for Test
				storeForInterrupt(this, false, true);
				break;

			default:	// Anything else is a no-op
				break;
			} // end switch for XX11 ops
			break;

		case 012:	// XX12: TBN=Transfer blanks for non-numeric
			streamBlankForNonNumeric(this, variant);
			break;

		case 014:	// XX14: SDA=Store destination address
			this->cycleCount += variant;
			streamAdjustDestChar(this);
			// save B
			this->r.A = this->r.B;
			this->r.AROF = this->r.BROF;
			this->r.B = ((WORD48)this->r.K << 15) | this->r.S;
			// save S (not the way the hardware did it)
			t1 = this->r.S;
			this->r.S = this->r.F - variant;
			// [S] = B
			storeBviaS(this);
			// restore S
			this->r.S = t1;
			// restore B from A
			this->r.B = this->r.A;
			this->r.BROF = this->r.AROF;
			// invalidate A
			this->r.AROF = false;
			break;

		case 015:	// XX15: SSA=Store source address
			this->cycleCount += variant;
			streamAdjustSourceChar(this);
			// save B
			this->r.A = this->r.B;
			this->r.AROF = this->r.BROF;
			this->r.B = ((WORD48)this->r.G << 15) | this->r.M;
			// save M (not the way the hardware did it)
			t1 = this->r.M;
			this->r.M = this->r.F - variant;
			// [M] = B
			storeBviaM(this);
			// restore M
			this->r.M = t1;
			// restore B from A
			this->r.B = this->r.A;
			this->r.BROF = this->r.AROF;
			// invalidate A
			this->r.AROF = false;
			break;

		case 016:	// XX16: SFD=Skip forward destination
			this->cycleCount += (variant >> 3) + (variant & 7);
			streamAdjustDestChar(this);
			if (this->r.BROF && ((this->r.K + variant) >= 8)) {
				// will skip off the current word,
				// so store and invalidate B
				storeBviaS(this);
				this->r.BROF = false;
			}
			t1 = (this->r.S << 3) + this->r.K + variant;
			this->r.S = t1 >> 3;
			this->r.K = t1 & 7;
			break;

		case 017:	// XX17: SRD=Skip reverse destination
			this->cycleCount += (variant >> 3) + (variant & 7);
			streamAdjustDestChar(this);
			if (this->r.BROF && (this->r.K < variant)) {
				// will skip off the current word,
				// so store and invalidate B
				storeBviaS(this);
				this->r.BROF = false;
			}
			t1 = (this->r.S << 3) + this->r.K - variant;
			this->r.S = t1 >> 3;
			this->r.K = t1 & 7;
			break;

		case 022:	// XX22: SES=Set source address
			this->cycleCount += variant;
			this->r.M = this->r.F - variant;
			this->r.G = this->r.H = 0;
			this->r.AROF = false;
			break;

		case 024:	// XX24: TEQ=Test for equal
			streamAdjustSourceChar(this);
			if (!this->r.AROF) {
				// A = [M]
				loadAviaM(this);
			}
			t1 = fieldIsolate(this->r.A, this->r.G*6, 6);
			this->r.TFFF = (t1 == variant ? true : false);
			break;

		case 025:	// XX25: TNE=Test for not equal
			streamAdjustSourceChar(this);
			if (!this->r.AROF) {
				// A = [M]
				loadAviaM(this);
			}
			t1 = fieldIsolate(this->r.A, this->r.G*6, 6);
			this->r.TFFF = (t1 != variant ? true : false);
			break;

		case 026:	// XX26: TEG=Test for equal or greater
			streamAdjustSourceChar(this);
			if (!this->r.AROF) {
				// A = [M]
				loadAviaM(this);
			}
			t1 = collation[fieldIsolate(this->r.A, this->r.G*6, 6)];
			t2 = collation[variant];
			this->r.TFFF = (t1 >= t2 ? true : false);
			break;

		case 027:	// XX27: TGR=Test for greater
			streamAdjustSourceChar(this);
			if (!this->r.AROF) {
				// A = [M]
				loadAviaM(this);
			}
			t1 = collation[fieldIsolate(this->r.A, this->r.G*6, 6)];
			t2 = collation[variant];
			this->r.TFFF = (t1 > t2 ? true : false);
			break;

		case 030:	// XX30: SRS=Skip reverse source
			this->cycleCount += (variant >> 3) + (variant & 7);
			streamAdjustSourceChar(this);
			if (this->r.G < variant) {
				// will skip off the current word
				this->r.AROF = 0;
			}
			t1 = this->r.M*8 + this->r.G - variant;
			this->r.M = t1 >> 3;
			this->r.G = t1 & 7;
			break;

		case 031:	// XX31: SFS=Skip forward source
			this->cycleCount += (variant >> 3) + (variant & 7);
			streamAdjustSourceChar(this);
			if (this->r.G + variant >= 8) {
				// will skip off the current word
				this->r.AROF = false;
			}
			t1 = this->r.M*8 + this->r.G + variant;
			this->r.M = t1 >> 3;
			this->r.G = t1 & 7;
			break;

		case 032:	// XX32: xxx=Field subtract (aux)
			fieldArithmetic(this, variant, false);
			break;

		case 033:	// XX33: xxx=Field add (aux)
			fieldArithmetic(this, variant, true);
			break;

		case 034:	// XX34: TEL=Test for equal or less
			streamAdjustSourceChar(this);
			if (!this->r.AROF) {
				// A = [M]
				loadAviaM(this);
			}
			t1 = collation[fieldIsolate(this->r.A, this->r.G*6, 6)];
			t2 = collation[variant];
			this->r.TFFF = (t1 <= t2 ? true : false);
			break;

		case 035:	// XX35: TLS=Test for less
			streamAdjustSourceChar(this);
			if (!this->r.AROF) {
				// A = [M]
				loadAviaM(this);
			}
			t1 = collation[fieldIsolate(this->r.A, this->r.G*6, 6)];
			t2 = collation[variant];
			this->r.TFFF = (t1 < t2 ? true : false);
			break;

		case 036:	// XX36: TAN=Test for alphanumeric
			streamAdjustSourceChar(this);
			if (!this->r.AROF) {
				// A = [M]
				loadAviaM(this);
			}
			this->r.Y = t1 = fieldIsolate(this->r.A, this->r.G*6, 6);
			this->r.Z = variant;	// for display only
			if (collation[t1] > collation[variant]) {
				this->r.TFFF = t1 == 0x20 ? false : (t1 == 0x3C ? false : true);
				// alphanumeric unless | or !
			} else {
				// alphanumeric if equal
				this->r.Q03F = true;
				// set Q03F (display only)
				this->r.TFFF = (t1 == variant ? true : false);
			}
			break;

		case 037:	// XX37: BIT=Test bit
			if (!this->r.AROF) {
				// A = [M]
				loadAviaM(this);
			}
			this->r.Y = fieldIsolate(this->r.A, this->r.G*6, 6);
			t1 = this->r.Y >> (5-this->r.H);
			this->r.TFFF = (t1 & 1) == (variant & 1) ? true : false;
			break;

		case 040:	// XX40: INC=Increase TALLY
			if (variant) {
				this->r.R = (this->r.R + variant) & 63;
			}
			// else it's a character-mode no-op
			break;

		case 041:	// XX41: STC=Store TALLY
			this->cycleCount += variant;
			// save B
			this->r.A = this->r.B;
			// invalidate A
			this->r.AROF = false;
			// save RCW address in B (why??)
			this->r.B = this->r.F;
			if (this->r.BROF) {
				// [S] = A, save original B contents
				storeAviaS(this);
				this->r.BROF = false;
			}
			// move saved F address to A (why??)
			this->r.A = this->r.B;
			// copy the TALLY value to B
			this->r.B = this->r.R;
			// save S (not the way the hardware did it)
			t1 = this->r.S;
			this->r.S = this->r.F - variant;
			// [S] = B, store the TALLY value
			storeBviaS(this);
			// restore F address from A (why??)
			this->r.B = this->r.A;
			// restore S
			this->r.S = t1;
			// invalidate B
			this->r.BROF = false;
			break;

		case 042:	// XX42: SEC=Set TALLY
			this->r.R = variant;
			break;

		case 043:	// XX43: CRF=Call repeat field
			this->cycleCount += variant;
			// save B in A
			this->r.A = this->r.B;
			this->r.AROF = this->r.BROF;
			// save S (not the way the hardware did it)
			t1 = this->r.S;
			// compute parameter address
			this->r.S = this->r.F - variant;
			// B = [S]
			loadBviaS(this);
			// dynamic repeat count is low-order 6 bits
			variant = this->r.B & 63;
			// restore S
			this->r.S = t1;
			// restore B from A
			this->r.B = this->r.A;
			this->r.BROF = this->r.AROF;
			// invalidate A
			this->r.AROF = 0;
			if (!this->r.PROF) {
				// fetch the program word, if necessary
				loadPviaC(this);
			}
			opcode = fieldIsolate(this->r.P, this->r.L*12, 12);
			if (variant) {
				// if repeat count from parameter > 0,
				// apply it to the next syllable
				this->r.T = opcode = (opcode & 00077) + (variant << 6);
			} else {
				// otherwise, construct JFW (XX47) using repeat
				// count from next syl (whew!)
				this->r.T = opcode = (opcode & 07700) + 047;
			}

			// Since we are bypassing normal SECL behavior,
			// bump the instruction pointer here.
			// >>> override normal instruction fetch <<<
			noSECL = 1;
			this->r.PROF = false;
			if (this->r.L < 3) {
				++this->r.L;
			} else {
				this->r.L = 0;
				++this->r.C;
			}
			break;

		case 044:	// XX44: JNC=Jump out of loop conditional
			if (!this->r.TFFF) {
				jumpOutOfLoop(this, variant);
			}
			break;

		case 045:	// XX45: JFC=Jump forward conditional
			if (!this->r.TFFF) {
				// conditional on TFFF
				this->cycleCount += (variant >> 2) + (variant & 3);
				jumpSyllables(this, variant);
			}
			break;

		case 046:	// XX46: JNS=Jump out of loop
			jumpOutOfLoop(this, variant);
			break;

		case 047:	// XX47: JFW=Jump forward unconditional
			this->cycleCount += (variant >> 2) + (variant & 3);
			jumpSyllables(this, variant);
			break;

		case 050:	// XX50: RCA=Recall control address
			this->cycleCount += variant;
			// save B in A
			this->r.A = this->r.B;
			this->r.AROF = this->r.BROF;
			// save S (not the way the hardware did it)
			t1 = this->r.S;
			this->r.S = this->r.F - variant;
			// B = [S]
			loadBviaS(this);
			this->r.S = t1;
			t2 = this->r.B;
			if (DESCRIPTOR(t2)) {
				// if it's a descriptor,
				if (presenceTest(this, t2)) {
					// if present, initiate a fetch to P
					// get the word address,
					this->r.C = this->r.B & MASKMEM;
					// force L to zero and
					this->r.L = 0;
					// require fetch at SECL
					this->r.PROF = false;
				}
			} else {
				this->r.C = t2 & MASKMEM;
				t1 = (t2 >> 36) & 3;
				if (t1 < 3) {
					// if not a descriptor, increment the address
					this->r.L = t1+1;
				} else {
					this->r.L = 0;
					++this->r.C;
				}
				// require fetch at SECL
				this->r.PROF = false;
			}
			// restore B
			this->r.B = this->r.A;
			this->r.BROF = this->r.AROF;
			// invalidate A
			this->r.AROF = false;
			break;

		case 051:	// XX51: ENS=End loop
			this->cycleCount += 4;
			// save B in A
			this->r.A = this->r.B;
			this->r.AROF = this->r.BROF;
			t1 = this->r.X;
			// get repeat count
			t2 = (t1 & MASK_LCWrpt) >> SHFT_LCWrpt;
			// loop count exhausted?
			if (t2) {
					// no, restore C, L, and P to loop again
					this->r.C = (t1 & MASK_LCWrC) >> SHFT_LCWrC;
					this->r.L = (t1 & MASK_LCWrL) >> SHFT_LCWrL;
					// require fetch at SECL
					this->r.PROF = false;
					// store decremented count in X
					--t2;
					this->r.X = (this->r.X & ~MASK_LCWrpt) | (t2 << SHFT_LCWrpt);
			} else {
				// save S (not the way the hardware did it)
				t2 = this->r.S;
				// get prior LCW addr from X value
				this->r.S = (t1 & MASK_LCWrF) >> SHFT_LCWrF;
				// B = [S], fetch prior LCW from stack
				loadBviaS(this);
				// restore S
				this->r.S = t2;
				// store prior LCW (less control bits) in X
				this->r.X = this->r.B & MASK_MANTISSA;
			}
			// restore B
			this->r.B = this->r.A;
			this->r.BROF = this->r.AROF;
			// invalidate A
			this->r.AROF = false;
			break;

		case 052:	// XX52: BNS=Begin loop
			this->cycleCount += 4;
			// save B in A (note that BROF is not altered)
			this->r.A = this->r.B;

			// construct new LCW - keep previous F field
			t1 = this->r.X & MASK_LCWrF;
			// insert repeat count
			// decrement count for first iteration
			t1 |= (WORD48)(variant ? variant-1 : 0) << SHFT_LCWrpt;
			// insert L
			t1 |= (WORD48)(this->r.L) << SHFT_LCWrL;
			// insert C
			t1 |= (WORD48)(this->r.C) << SHFT_LCWrC;

			// save current loop control word
			// set control bits [0:2]=3
			this->r.B = this->r.X | INIT_LCW;
			// save S (not the way the hardware did it)
			t2 = this->r.S;
			// get F value from X value and ++
			this->r.S = (this->r.X & MASK_LCWrF) >> SHFT_LCWrF;
			this->r.S++;
			// [S] = B, save prior LCW in stack
			storeBviaS(this);
			// update F value in X
			t1 |= (this->r.S << SHFT_LCWrF);
			this->r.X = t1;
			// restore S
			this->r.S = t2;

			// restore B (note that BROF is still relevant)
			this->r.B = this->r.A;
			// invalidate A
			this->r.AROF = false;
			break;

		case 053:	// XX53: RSA=Recall source address
			this->cycleCount += variant;
			// save B
			this->r.A = this->r.B;
			this->r.AROF = this->r.BROF;
			this->r.H = 0;
			this->r.M = this->r.F - variant;
			// B = [M]
			loadBviaM(this);
			t1 = this->r.B;
			this->r.M = t1 & MASKMEM;
			if (OPERAND(t1)) {
				// if it's an operand,
				// set G from [30:3]
				this->r.G = (t1 >> 15) & 7;
			} else {
				// otherwise, force G to zero and
				this->r.G = 0;
				// just take the side effect of any p-bit interrupt
				presenceTest(this, t1);
			}
			// restore B from A
			this->r.B = this->r.A;
			this->r.BROF = this->r.AROF;
			// invalidate A
			this->r.AROF = false;
			break;

		case 054:	// XX54: SCA=Store control address
			this->cycleCount += variant;
			// save B
			this->r.A = this->r.B;
			this->r.AROF = this->r.BROF;
			// save S (not the way the hardware did it)
			t2 = this->r.S;
			// compute store address
			this->r.S = this->r.F - variant;
			this->r.B = this->r.C |
				((WORD48)this->r.F << 15) |
				((WORD48)this->r.L << 36);
			// [S] = B
			storeBviaS(this);
			// restore S
			this->r.S = t2;
			// restore B from A
			this->r.B = this->r.A;
			this->r.BROF = this->r.AROF;
			// invalidate A
			this->r.AROF = false;
			break;

		case 055:	// XX55: JRC=Jump reverse conditional
			if (!this->r.TFFF) {
				// conditional on TFFF
				this->cycleCount += (variant >> 2) + (variant & 3);
				jumpSyllables(this, -variant);
			}
			break;

		case 056:	// XX56: TSA=Transfer source address
			streamAdjustSourceChar(this);
			if (this->r.BROF) {
				// [S] = B, store B at dest addresss
				storeBviaS(this);
				this->r.BROF = false;
			}
			if (!this->r.AROF) {
				// A = [M], load A from source address
				loadAviaM(this);
			}
			for (variant = 3; variant > 0; --variant) {
				this->r.Y = fieldIsolate(this->r.A, this->r.G*6, 6);
				this->r.B = (this->r.B << 6) + this->r.Y;
				// make sure B is not exceeding 48 bits
				this->r.B &= 07777777777777777ll;
				if (this->r.G < 7) {
					++this->r.G;
				} else {
					this->r.G = 0;
					++this->r.M;
					// A = [M]
					loadAviaM(this);
				}
			}
			this->r.M = this->r.B & MASKMEM;
			this->r.G = (this->r.B >> 15) & 7;
			// invalidate A
			this->r.AROF = false;
			break;

		case 057:	// XX57: JRV=Jump reverse unconditional
			this->cycleCount += (variant >> 2) + (variant & 3);
			jumpSyllables(this, -variant);
			break;

		case 060:	// XX60: CEQ=Compare equal
			compareSourceWithDest(this, variant, false);
			this->r.H = this->r.V = 0;
			// if !Q03F, S=D
			this->r.TFFF = this->r.Q03F ? false : true;
			break;

		case 061:	// XX61: CNE=Compare not equal
			compareSourceWithDest(this, variant, false);
			this->r.H = this->r.V = 0;
			// if Q03F, S!=D
			this->r.TFFF = this->r.Q03F ? true : false;
			break;

		case 062:	// XX62: CEG=Compare greater or equal
			compareSourceWithDest(this, variant, false);
			this->r.H = this->r.V = 0;
			// if Q03F&TFFF, S>D; if !Q03F, S=D
			this->r.TFFF = this->r.Q03F ? this->r.TFFF : true;
			break;

		case 063:	// XX63: CGR=Compare greater
			compareSourceWithDest(this, variant, false);
			this->r.H = this->r.V = 0;
			// if Q03F&TFFF, S>D
			this->r.TFFF = this->r.Q03F ? this->r.TFFF : false;
			break;

		case 064:	// XX64: BIS=Set bit
			streamBitsToDest(this, variant, 07777777777777777ll);
			break;

		case 065:	// XX65: BIR=Reset bit
			streamBitsToDest(this, variant, 0);
			break;

		case 066:	// XX66: OCV=Output convert
			streamOutputConvert(this, variant);
			break;

		case 067:	// XX67: ICV=Input convert
			streamInputConvert(this, variant);
			break;

		case 070:	// XX70: CEL=Compare equal or less
			compareSourceWithDest(this, variant, false);
			this->r.H = this->r.V = 0;
			// if Q03F&!TFFF, S<D; if !Q03F, S=D
			this->r.TFFF = this->r.Q03F ? !this->r.TFFF : true;
			break;

		case 071:	// XX71: CLS=Compare less
			compareSourceWithDest(this, variant, false);
			this->r.H = this->r.V = 0;
			// if Q03F&!TFFF, S<D
			this->r.TFFF = this->r.Q03F ? !this->r.TFFF : false;
			break;

		case 072:	// XX72: FSU=Field subtract
			fieldArithmetic(this, variant, false);
			break;

		case 073:	// XX73: FAD=Field add
			fieldArithmetic(this, variant, true);
			break;

		case 074:	// XX74: TRP=Transfer program characters
			streamProgramToDest(this, variant);
			break;

		case 075:	// XX75: TRN=Transfer source numerics
			// initialize for negative sign test
			this->r.TFFF = false;
			streamNumericToDest(this, variant, false);
			break;

		case 076:	// XX76: TRZ=Transfer source zones
			streamNumericToDest(this, variant, true);
			break;

		case 077:	// XX77: TRS=Transfer source characters
			streamCharacterToDest(this, variant);
			break;

		default:	// everything else is a no-op
			break;

		} // end switch for character mode operators
	} while (noSECL);
}
