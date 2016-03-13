/***********************************************************************
* b5500emulator
************************************************************************
* Copyright (c) 2016, Reinhard Meyer, DL5UY
* Licensed under the MIT License,
*       see LICENSE
* based on (C) work by Nigel Williams and Paul Kimpel
* see: https://github.com/pkimpel/retro-b5500
************************************************************************
* execute one word mode instruction
************************************************************************
* 2016-02-19  R.Meyer
*   Converted Paul's work from Javascript to C
***********************************************************************/

#include "b5500_common.h"

/***********************************************************
*  Word Mode Syllables                                     *
***********************************************************/
void b5500_execute_wm(CPU *this)
{
	WORD12 opcode = this->r.T;
	WORD12 variant;
	WORD48 t1, t2;

	// clear some vars 
	this->r.Q01F = false;
	this->r.Q02F = false;
	this->r.Q03F = false;
	this->r.Q04F = false;
	this->r.Q05F = false;
	this->r.Q06F = false;
	this->r.Q07F = false;
	this->r.Q08F = false;
	this->r.Q09F = false;
	this->r.Y = 0;
	this->r.Z = 0;
	this->r.M = 0;
	this->r.N = 0;
	this->r.X = 0;

	// last 2 bits of opcode
	switch (opcode & 3) {
	case 0:	// LITC: Literal Call
		adjustAEmpty(this);
		this->r.A = opcode >> 2;
		this->r.AROF = true;
		break;

	case 2:	// OPDC: Operand Call
		adjustAEmpty(this);
		computeRelativeAddr(this, opcode >> 2, true);
		loadAviaM(this);
		if (DESCRIPTOR(this->r.A)) {
			// if it's a control word, evaluate it
			operandCall(this);
		}
		// otherwise, just leave it in A
		break;

	case 3: // DESC: Descriptor (name) Call
		adjustAEmpty(this);
		computeRelativeAddr(this, opcode >> 2, true);
		loadAviaM(this);
		descriptorCall(this);
		break;

	case 1:	// all other word-mode operators
		variant = opcode >> 6;
		switch (opcode & 077) {
		case 001: // XX01: single-precision numerics
			switch (variant) {
			case 001: // 0101: ADD=single-precision add
				singlePrecisionAdd(this, true);
				break;
			case 003: // 0301: SUB=single-precision subtract
				singlePrecisionAdd(this, false);
				break;
			case 004: // 0401: MUL=single-precision multiply
				singlePrecisionMultiply(this);
				break;
			case 010: // 1001: DIV=single-precision floating divide
				singlePrecisionDivide(this);
				break;
			case 030: // 3001: IDV=integer divide
				integerDivide(this);
				break;
			case 070: // 7001: RDV=remainder divide
				remainderDivide(this);
				break;
			}
			break;
		case 005: // XX05: double-precision numerics
			switch (variant) {
			case 001: // 0105: DLA=double-precision add
				doublePrecisionAdd(this, true);
				break;
			case 003: // 0305: DLS=double-precision subtract
				doublePrecisionAdd(this, false);
				break;
			case 004: // 0405: DLM=double-precision multiply
				doublePrecisionMultiply(this);
				break;
			case 010: // 1005: DLD=double-precision floating divide
				doublePrecisionDivide(this);
				break;
			}
			break;
		case 011: // XX11: Control State and communication ops
			switch (variant) {
			case 001: // 0111: PRL=Program Release
				// TOS should be operand or descriptor 
				// t1 = copy of A
				// t2 = presence bit or value valid
				// get it into A and copy into t1
				adjustAFull(this);
				t1 = this->r.A;
				if (OPERAND(t1)) {
					// it's an operand
					computeRelativeAddr(this, t1, false);
					t2 = true;
				} else if (presenceTest(this, t1)) {
					// present descriptor
					this->r.M = t1 & MASKMEM;
					t2 = true;
				} else {
					// absent descriptor
					t2 = false;
				}
				if (t2) {
					// fetch IO Descriptor
					loadAviaM(this);
					if (this->r.NCSF) {
						// not in control state
						// test continuity bit, [20:1]
						if (this->r.A & MASK_DDCONT) {
							// set I07/6: continuity bit
							this->r.I = (this->r.I & IRQ_MASKL) | IRQ_CONT;
						} else {
							// set I07/5: program release
							this->r.I = (this->r.I & IRQ_MASKL) | IRQ_PREL;
						}
						signalInterrupt(this);
						this->r.A = this->r.M;
						// store IOD address in PRT[9]
						this->r.M = (this->r.R<<6) + 9;
						storeAviaM(this);
					} else {
						// in control state
						// clear presence bit
						this->r.A &= ~MASK_PBIT;
						storeAviaM(this);
					}
					this->r.AROF = false;
				}
				break;
			case 002: // 0211: ITI=Interrogate Interrupt
				// control-state only
				if (CC->IAR && !this->r.NCSF) {
					this->r.C = CC->IAR;
					this->r.L = 0;
					// stack address @100
					this->r.S = AA_IRQSTACK;
					clearInterrupt(this);
					// require fetch at SECL
					this->r.PROF = false;
				}
				break;
			case 004: // 0411: RTR=Read Timer
				// control-state only
				if (!this->r.NCSF) {
					adjustAEmpty(this);
					this->r.A = readTimer(this);
					this->r.AROF = true;
				}
				break;
			case 010: // 1011: COM=Communicate
				// no-op in Control State
				if (this->r.NCSF) {
					// address = R+@11
					this->r.M = this->r.R*64 + RR_COM;
					if (this->r.AROF) {
						storeAviaM(this);
						// [M] = A
						this->r.AROF = false;
					} else if (this->r.BROF) {
						storeBviaM(this);
						// [M] = B
						this->r.BROF = false;
					} else {
						adjustBFull(this);
						storeBviaM(this);
						// [M] = B
						this->r.BROF = false;
					}
					// set I07: communicate
					this->r.I = (this->r.I & IRQ_MASKL) | IRQ_COM;
					signalInterrupt(this);
				}
				break;
			case 021: // 2111: IOR=I/O Release
				// no-op in Normal State
				if (!this->r.NCSF) {
					adjustAFull(this);
					t1 = this->r.A;
					if (OPERAND(t1)) {
						// it's an operand
						computeRelativeAddr(this, t1, 0);
						t2 = true;
					} else if (PRESENT(t1)) {
						this->r.M = t1 & MASKMEM;
						// present descriptor
						t2 = true;
					} else {
						// for an absent descriptor, just leave it on the stack
						t2 = false;
					}
					if (t2) {
						loadAviaM(this);
						this->r.A |= MASK_PBIT;
						storeAviaM(this);
						this->r.AROF = false;
					}
				}
				break;
			case 022: // 2211: HP2=Halt Processor 2
				// control-state only
				if (!(this->r.NCSF || CC->HP2F)) {
					haltP2(this);
				}
				break;
			case 024: // 2411: ZPI=Conditional Halt
				if (this->r.US14X) {
					// STOP OPERATOR switch on
					stop(this);
				}
				break;
			case 030: // 3011: SFI=Store for Interrupt
				storeForInterrupt(this, false, false);
				break;
			case 034: // 3411: SFT=Store for Test
				storeForInterrupt(this, false, true);
				break;
			case 041: // 4111: IP1=Initiate Processor 1
				// control-state only
				if (!this->r.NCSF) {
					initiate(this, false);
				}
				break;
			case 042: // 4211: IP2=Initiate Processor 2
				// control-state only
				if (!this->r.NCSF) {
					// INCW is stored in @10
					this->r.M = AA_IODESC;
					if (this->r.AROF) {
						storeAviaM(this);
						// [M] = A
						this->r.AROF = false;
					} else if (this->r.BROF) {
						storeBviaM(this);
						// [M] = B
						this->r.BROF = false;
					} else {
						adjustAFull(this);
						storeAviaM(this);
						// [M] = A
						this->r.AROF = false;
					}
					initiateP2(this);
				}
				break;
			case 044: // 4411: IIO=Initiate I/O
				if (!this->r.NCSF) {
					// address of IOD is stored in @10
					this->r.M = AA_IODESC;
					if (this->r.AROF) {
						storeAviaM(this);
						// [M] = A
						this->r.AROF = false;
					} else if (this->r.BROF) {
						storeBviaM(this);              // [M] = B
						this->r.BROF = false;
					} else {
						adjustAFull(this);
						storeAviaM(this);
						// [M] = A
						this->r.AROF = false;
					}
					// let CentralControl choose the I/O Unit
					initiateIO(this);
				}
				break;
			case 051: // 5111: IFT=Initiate For Test
				if (!this->r.NCSF) {
					// control-state only
					initiate(this, 1);
				}
				break;
			} // end switch for XX11 ops
			break;
		case 015: // XX15: logical (bitmask) ops
			switch (variant) {
			case 001: // 0115: LNG=logical negate
				adjustAFull(this);
				this->r.A ^= MASK_NUMBER;
				break;
			case 002: // 0215: LOR=logical OR
				adjustABFull(this);
				this->r.A = (this->r.A & MASK_NUMBER) | this->r.B;
				this->r.BROF = false;
				break;
			case 004: // 0415: LND=logical AND
				adjustABFull(this);
				this->r.A = (this->r.A | MASK_FLAG) & this->r.B;
				this->r.BROF = false;
				break;
			case 010: // 1015: LQV=logical EQV
				adjustABFull(this);
				this->r.B ^= (~this->r.A) & MASK_NUMBER;
				this->r.AROF = false;
				break;

			case 020: // 2015: MOP=reset flag bit (make operand)
				adjustAFull(this);
				this->r.A &= MASK_NUMBER;
				break;
			case 040: // 4015: MDS=set flag bit (make descriptor)
				adjustAFull(this);
				this->r.A |= MASK_FLAG; // set [0:1]
				break;
			}
			break;
		case 021: // XX21: load & store ops
			switch (variant) {
			case 001: // 0121: CID=Conditional integer store destructive
				integerStore(this, true, true);
				break;
			case 002: // 0221: CIN=Conditional integer store nondestructive
				integerStore(this, true, false);
				break;
			case 004: // 0421: STD=Store destructive
				adjustABFull(this);
				if (OPERAND(this->r.A)) {
					// it's an operand
					computeRelativeAddr(this, this->r.A, false);
					storeBviaM(this);
					this->r.AROF = this->r.BROF = false;
				} else {
					// it's a descriptor
					if (presenceTest(this, this->r.A)) {
						this->r.M = this->r.A & MASKMEM;
						storeBviaM(this);
						this->r.AROF = this->r.BROF = false;
					}
				}
				break;
			case 010: // 1021: SND=Store nondestructive
				adjustABFull(this);
				if (OPERAND(this->r.A)) {
					// it's an operand
					computeRelativeAddr(this, this->r.A, false);
					storeBviaM(this);
					this->r.AROF = false;
				} else {
					// it's a descriptor
					if (presenceTest(this, this->r.A)) {
						this->r.M = this->r.A & MASKMEM;
						storeBviaM(this);
						this->r.AROF = false;
					}
				}
				break;
			case 020: // 2021: LOD=Load operand
				adjustAFull(this);
				if (OPERAND(this->r.A)) {
					// simple operand
					computeRelativeAddr(this, this->r.A, true);
					loadAviaM(this);
				} else if (presenceTest(this, this->r.A)) {
					// present descriptor
					this->r.M = this->r.A & MASKMEM;
					loadAviaM(this);
				}
				break;
			case 041: // 4121: ISD=Integer store destructive
				integerStore(this, false, true);
				break;
			case 042: // 4221: ISN=Integer store nondestructive
				integerStore(this, false, false);
				break;
			}
			break;
		case 025: // XX25: comparison & misc. stack ops
			switch (variant) {
			case 001: // 0125: GEQ=compare B greater or equal to A
				this->r.B = (singlePrecisionCompare(this) >= 0) ? true : false;
				break;
			case 002: // 0225: GTR=compare B greater to A
				this->r.B = (singlePrecisionCompare(this) > 0) ? true : false;
				break;
			case 004: // 0425: NEQ=compare B not equal to A
				this->r.B = (singlePrecisionCompare(this) != 0) ? true : false;
				break;
			case 041: // 4125: LEQ=compare B less or equal to A
				this->r.B = (singlePrecisionCompare(this) <= 0) ? true : false;
				break;
			case 042: // 4225: LSS=compare B less to A
				this->r.B = (singlePrecisionCompare(this) < 0) ? true : false;
				break;
			case 044: // 4425: EQL=compare B equal to A
				this->r.B = (singlePrecisionCompare(this) == 0) ? true : false;
				break;

			case 010: // 1025: XCH=exchange TOS words
				exchangeTOS(this);
				break;
			case 020: // 2025: DUP=Duplicate TOS
				if (this->r.AROF) {
					adjustBEmpty(this);
					this->r.B = this->r.A;
					this->r.BROF = true;
				} else {
					adjustBFull(this);
					this->r.A = this->r.B;
					this->r.AROF = true;
				}
				break;

			case 014: // 1425: FTC=F field to C field
				adjustABFull(this);
				t1 = (this->r.A & MASK_RCWrF) >> SHFT_RCWrF;
				this->r.B = (this->r.B & ~MASK_RCWrC) | (t1 << SHFT_RCWrC);
				this->r.AROF = false;
				break;
			case 034: // 3425: FTF=F field to F field
				adjustABFull(this);
				t1 = (this->r.A & MASK_RCWrF) >> SHFT_RCWrF;
				this->r.B = (this->r.B & ~MASK_RCWrF) | (t1 << SHFT_RCWrF);
				break;
			case 054: // 5425: CTC=C field to C field
				adjustABFull(this);
				t1 = (this->r.A & MASK_RCWrC) >> SHFT_RCWrC;
				this->r.B = (this->r.B & ~MASK_RCWrC) | (t1 << SHFT_RCWrC);
				this->r.AROF = false;
				break;
			case 074: // 7425: CTF=C field to F field
				adjustABFull(this);
				t1 = (this->r.A & MASK_RCWrC) >> SHFT_RCWrC;
				this->r.B = (this->r.B & ~MASK_RCWrF) | (t1 << SHFT_RCWrF);
				this->r.AROF = false;
				break;
			}
			break;
		case 031: // XX31: branch, sign-bit, interrogate ops
			switch (variant) {
			case 001: // 0131: BBC=branch backward conditional
			case 002: // 0231: BFC=branch forward conditional
				adjustABFull(this);
				if (this->r.B & 1) {
					// true => no branch
					this->r.AROF = this->r.BROF = false;
					break;
				}
				this->r.BROF = false;
				goto common_branch;
			case 041: // 4131: BBW=branch backward unconditional
			case 042: // 4231: BFW=branch forward unconditional
				adjustAFull(this);
common_branch:
				if (OPERAND(this->r.A)) {
					// simple operand
					if (variant == 001 || variant == 041)
						jumpSyllables(this, -(this->r.A & MASKMEM));
					else
						jumpSyllables(this, this->r.A & MASKMEM);
					this->r.AROF = false;
				} else {
					// descriptor
					if (this->r.L == 0) {
						--this->r.C;
						// adjust for Inhibit Fetch
					}
					if (presenceTest(this, this->r.A)) {
						this->r.C = this->r.A & MASKMEM;
						this->r.L = 0;
						// require fetch at SECL
						this->r.PROF = false;
						this->r.AROF = false;
					}
				}
				break;

			case 021: // 2131: LBC=branch backward word conditional
			case 022: // 2231: LFC=branch forward word conditional
				adjustABFull(this);
				if (this->r.B & 1) {
					// true => no branch
					this->r.AROF = this->r.BROF = false;
					break;
				}
				this->r.BROF = false;
				goto common_branch_word;
			case 061: // 6131: LBU=branch backward word unconditional
			case 062: // 6231: LFU=branch forward word unconditional
				adjustAFull(this);
common_branch_word:
				if (this->r.L == 0) {
					--this->r.C;
					// adjust for Inhibit Fetch
				}
				if (OPERAND(this->r.A)) {
					// simple operand
					if (variant == 021 || variant == 061)
						jumpWords(this, -(this->r.A & 0x03ff));
					else
						jumpWords(this, this->r.A & 0x03ff);
					this->r.AROF = false;
				} else {
					// descriptor
					if (presenceTest(this, this->r.A)) {
						this->r.C = this->r.A & MASKMEM;
						this->r.L = 0;
						// require fetch at SECL
						this->r.PROF = false;
						this->r.AROF = false;
					}
				}
				break;

			case 004: // 0431: SSN=set sign bit (set negative)
				adjustAFull(this);
				this->r.A |= MASK_SIGNMANT;
				break;
			case 010: // 1031: CHS=change sign bit
				adjustAFull(this);
				this->r.A ^= MASK_SIGNMANT;
				break;
			case 020: // 2031: TOP=test flag bit (test for operand)
				adjustAEmpty(this);
				adjustBFull(this);
				this->r.A = OPERAND(this->r.B) ? true : false;
				this->r.AROF = true;
				break;
			case 024: // 2431: TUS=interrogate peripheral status
				adjustAEmpty(this);
				this->r.A = interrogateUnitStatus(this);
				this->r.AROF = true;
				break;
			case 044: // 4431: SSP=reset sign bit (set positive)
				adjustAFull(this);
				this->r.A &= ~MASK_SIGNMANT;
				break;
			case 064: // 6431: TIO=interrogate I/O channel
				adjustAEmpty(this);
				this->r.A = interrogateIOChannel(this);
				this->r.AROF = true;
				break;
			case 070: // 7031: FBS=stack search for flag
				adjustAFull(this);
				this->r.M = this->r.A & MASKMEM;
				loadAviaM(this);
				while (OPERAND(this->r.A)) {
					this->r.M = (this->r.M+1) & MASKMEM;
					loadAviaM(this);
				}
				// flag bit found: stop the search
				this->r.A = INIT_DD | MASK_PBIT | this->r.M;
				break;
			}
			break;
		case 035: // XX35: exit & return ops
			switch (variant) {
			case 001: // 0135: BRT=branch return
				adjustAEmpty(this);
				if (!this->r.BROF) {
					this->r.Q03F = true;
					// Q03F: not used, except for display purposes
					adjustBFull(this);
				}
				if (presenceTest(this, this->r.B)) {
					this->r.S = (this->r.B >> 15) & MASKMEM;
					this->r.C = this->r.B & MASKMEM;
					this->r.L = 0;
					this->r.PROF = false;
					// require fetch at SECL
					loadBviaS(this);
					// B = [S], fetch MSCW
					--this->r.S;
					applyMSCW(this, this->r.B);
					this->r.BROF = false;
				}
				break;
			case 002: // 0235: RTN=return normal
			case 012: // 1235: RTS=return special
				adjustAFull(this);
				// If A is an operand or a present descriptor,
				// proceed with the return,
				// otherwise throw a p-bit interrupt
				// (this isn't well-documented)
				if (OPERAND(this->r.A) || presenceTest(this, this->r.A)) {
					if (variant == 002) {
						// RTN - reset stack to F to be at RCW
						this->r.S = this->r.F;
					} else {
						// RTS - stack already at RCW
					}
					loadBviaS(this);
					// B = [S], fetch the RCW
					switch (exitSubroutine(this, false)) {
					case 0:
						this->r.X = 0;
						operandCall(this);
						break;
					case 1:
						// set Q05F, for display only
						this->r.Q05F = true;
						this->r.X = 0;
						descriptorCall(this);
						break;
					case 2: // flag-bit interrupt occurred, do nothing
						break;
					}
				}
				break;

			case 004: // 0435: XIT=exit procedure
				this->r.AROF = false;
				this->r.S = this->r.F;
				loadBviaS(this);
				// B = [S], fetch the RCW
				exitSubroutine(this, false);
				break;
			}
			break;
		case 041: // XX41: index, mark stack, etc.
			switch (variant) {
			case 001: // 0141: INX=index
				adjustABFull(this);
				this->r.M = (this->r.A + this->r.B) & MASKMEM;
				this->r.A = (this->r.A & ~MASKMEM) | this->r.M;
				this->r.BROF = false;
				break;

			case 002: // 0241: COC=construct operand call
				exchangeTOS(this);
				this->r.A |= MASK_FLAG;
				// set [0:1]
				operandCall(this);
				break;
			case 004: // 0441: MKS=mark stack
				adjustABEmpty(this);
				this->r.B = buildMSCW(this);
				this->r.BROF = true;
				adjustBEmpty(this);
				this->r.F = this->r.S;
				if (!this->r.MSFF) {
					if (this->r.SALF) {
						// store the MSCW at R+7
						this->r.M = this->r.R*64 + 7;
						storeBviaM(this);
						// [M] = B
					}
					this->r.MSFF = true;
				}
				break;
			case 012: // 1241: CDC=construct descriptor call
				exchangeTOS(this);
				this->r.A |= MASK_FLAG;
				// set [0:1]
				descriptorCall(this);
				break;
			case 021: // 2141: SSF=F & S register set/store
				adjustABFull(this);
				switch (this->r.A & 3) {
				case 0: // store F into B.[18:15]
					this->r.B = (this->r.B & MASK_RCWrF) | (this->r.F << SHFT_RCWrF);
					break;
				case 1: // store S into B.[33:15]
					this->r.B = (this->r.B & ~MASK_RCWrC) | (this->r.S << SHFT_RCWrC);
					break;
				case 2: // set   F from B.[18:15]
					this->r.F = (this->r.B & MASK_RCWrF) >> SHFT_RCWrF;
					this->r.SALF = true;
					this->r.BROF = false;
					break;
				case 3: // set   S from B.[33:15]
					this->r.S = (this->r.B & MASK_RCWrC) >> SHFT_RCWrC;
					this->r.BROF = false;
					break;
				}
				this->r.AROF = false;
				break;
			case 025: // 2541: LLL=link list look-up
				adjustABFull(this);
				// get test field
				t1 = this->r.A & MASK_MANTISSA;
				// test value
				this->r.M = this->r.B & MASKMEM;
				// starting link address
				do {
					this->cycleCount += 2;
					// approximate the timing
					loadBviaM(this);
					t2 = this->r.B & MASK_MANTISSA;
					if (t2 < t1) {
						this->r.M = t2 & MASKMEM;
					} else {
						this->r.A = INIT_DD | MASK_PBIT | this->r.M;
						break;
						// B >= A: stop look-up
					}
				} while (true);
				break;
			case 044: // 4441: CMN=enter character mode inline
				enterCharModeInline(this);
				break;
			}
			break;
		case 045: // XX45: ISO=Variable Field Isolate op
			adjustAFull(this);
			t2 = variant >> 3; // number of whole chars
			if (t2) {
				t1 = this->r.G*6 + this->r.H; // starting source bit position
				t2 = t2*6 - (variant & 7) - this->r.H; // number of bits
				if (t1+t2 <= 48) {
					this->r.A = fieldIsolate(this->r.A, t1, t2);
				} else {
					// handle wrap-around in the source value
					this->r.A = fieldInsert(
						fieldIsolate(this->r.A, 0, t2-48+t1),
						48-t2, 48-t1,
						fieldIsolate(this->r.A, t1, 48-t1));
				}
				// approximate the shift cycle counts
				this->cycleCount += (variant >> 3) + (variant & 7) + this->r.G + this->r.H;
				this->r.G = (this->r.G + (variant >> 3)) & 7;
				this->r.H = 0;
			}
			break;
		case 051: // XX51: delete & conditional branch ops
			if (variant < 4) {
				// 0051=DEL: delete TOS (or field branch with zero-length field)
				if (this->r.AROF) {
					this->r.AROF = false;
				} else if (this->r.BROF) {
					this->r.BROF = false;
				} else {
					--this->r.S;
				}
			} else {
				adjustABFull(this);
				// field length (1-15 bits)
				t2 = variant >> 2;
				t1 = fieldIsolate(this->r.B, this->r.G*6+this->r.H, t2);
				// approximate the shift counts
				this->cycleCount += this->r.G + this->r.H + (t2 >> 1);
				// A is unconditionally empty at end
				this->r.AROF = false;
				switch (variant & 0x03) {
				case 0x02: // X251/X651: CFD=non-zero field branch forward destructive
					this->r.BROF = false;
					// no break: fall through
				case 0x00: // X051/X451: CFN=non-zero field branch forward nondestructive
					if (t1) {
						if (OPERAND(this->r.A)) {
							// simple operand
							jumpSyllables(this, this->r.A & 0x0fff);
						} else {
							// descriptor
							if (this->r.L == 0) {
								// adjust for Inhibit Fetch
								--this->r.C;
							}
							if (presenceTest(this, this->r.A)) {
								this->r.C = this->r.A & MASKMEM;
								this->r.L = 0;
								// require fetch at SEQL
								this->r.PROF = false;
							}
						}
					}
					break;
				case 0x03: // X351/X751: CBD=non-zero field branch backward destructive
					this->r.BROF = false;
					// no break: fall through
				case 0x01: // X151/X551: CBN=non-zero field branch backward nondestructive
					if (t1) {
						if (OPERAND(this->r.A)) {
							// simple operand
							jumpSyllables(this, -(this->r.A & 0x0fff));
						} else {
							// descriptor
							if (this->r.L == 0) {
								// adjust for Inhibit Fetch
								--this->r.C;
							}
							if (presenceTest(this, this->r.A)) {
								this->r.C = this->r.A & MASKMEM;
								this->r.L = 0;
								// require fetch at SEQL
								this->r.PROF = false;
							}
						}
					}
				break;
				}
			}
			break;
		case 055: // XX55: NOP & DIA=Dial A ops
			if (opcode & 0xFC0) {
				this->r.G = variant >> 3;
				this->r.H = variant & 7;
			// else // 0055: NOP=no operation (the official one, at least)
			}
			break;
		case 061: // XX61: XRT & DIB=Dial B ops
			if (opcode & 0xFC0) {
				this->r.K = variant >> 3;
				this->r.V = variant & 7;
			} else { // 0061=XRT: temporarily set full PRT addressing mode
				this->r.VARF = this->r.SALF;
				this->r.SALF = false;
			}
			break;
		case 065: // XX65: TRB=Transfer Bits
			adjustABFull(this);
			if (variant > 0) {
				t1 = this->r.G*6 + this->r.H; // A register starting bit nr
				if (t1+variant > 48) {
					variant = 48-t1;
				}
				t2 = this->r.K*6 + this->r.V; // B register starting bit nr
				if (t2+variant > 48) {
					variant = 48-t2;
				}
				fieldTransfer(&this->r.B, t2, variant, this->r.A, t1);
			}
			this->r.AROF = false;
			this->cycleCount += variant + this->r.G + this->r.K;       // approximate the shift counts
			break;
		case 071: // XX71: FCL=Compare Field Low
			adjustABFull(this);
			t1 = this->r.G*6 + this->r.H;     // A register starting bit nr
			if (t1+variant > 48) {
				variant = 48-t1;
			}
			t2 = this->r.K*6 + this->r.V;     // B register starting bit nr
			if (t2+variant > 48) {
				variant = 48-t2;
			}
			if (variant == 0) {
				this->r.A = true;
			} else if (fieldIsolate(this->r.B, t2, variant) < fieldIsolate(this->r.A, t1, variant)) {
				this->r.A = true;
			} else {
				this->r.A = false;
			}
			this->cycleCount += variant + this->r.G + this->r.K;       // approximate the shift counts
			break;
		case 075: // XX75: FCE=Compare Field Equal
			adjustABFull(this);
			t1 = this->r.G*6 + this->r.H;     // A register starting bit nr
			if (t1+variant > 48) {
				variant = 48-t1;
			}
			t2 = this->r.K*6 + this->r.V;     // B register starting bit nr
			if (t2+variant > 48) {
				variant = 48-t2;
			}
			if (variant == 0) {
				this->r.A = true;
			} else if (fieldIsolate(this->r.B, t2, variant) == fieldIsolate(this->r.A, t1, variant)) {
				this->r.A = true;
			} else {
				this->r.A = false;
			}
			this->cycleCount += variant + this->r.G + this->r.K;       // approximate the shift counts
			break;
		default:
			break;
			// anything else is a no-op
		} // end switch for non-LITC/OPDC/DESC operators
		break;
	} // end switch for word-mode operators
}
