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
	this->r.Q01F = 0;
	this->r.Q02F = 0;
	this->r.Q03F = 0;
	this->r.Q04F = 0;
	this->r.Q05F = 0;
	this->r.Q06F = 0;
	this->r.Q07F = 0;
	this->r.Q08F = 0;
	this->r.Q09F = 0;
	this->r.Q12F = 0;
	this->r.Y = 0;
	this->r.Z = 0;
	this->r.M = 0;
	this->r.N = 0;
	this->r.X = 0;
printf("OP=%04o\n", opcode);
	// last 2 bits of opcode
	switch (opcode & 3) {
	case 0:	// LITC: Literal Call
		adjustAEmpty(this);
		this->r.A = opcode >> 2;
		this->r.AROF = 1;
		break;
	case 2:	// OPDC: Operand Call
		adjustAEmpty(this);
		computeRelativeAddr(this, opcode >> 2, 1);
		loadAviaM(this);
		if (this->r.A & MASK_CONTROLW) {
			// optimization: if it's a control word, evaluate it
			operandCall(this);
		}
		// otherwise, just leave it in A
		break;

	case 3: // DESC: Descriptor (name) Call
		adjustAEmpty(this);
		computeRelativeAddr(this, opcode >> 2, 1);
		loadAviaM(this);
		descriptorCall(this);
		break;

	case 1:	// all other word-mode operators
		variant = opcode >> 6;
		switch (opcode & 0x3F) {
		case 0x01: // XX01: single-precision numerics
			switch (variant) {
			case 0x01: // 0101: ADD=single-precision add
				singlePrecisionAdd(this, true);
				break;
			case 0x03: // 0301: SUB=single-precision subtract
				singlePrecisionAdd(this, false);
				break;
			case 0x04: // 0401: MUL=single-precision multiply
				singlePrecisionMultiply(this);
				break;
			case 0x08: // 1001: DIV=single-precision floating divide
				singlePrecisionDivide(this);
				break;
			case 0x18: // 3001: IDV=integer divide
				integerDivide(this);
				break;
			case 0x38: // 7001: RDV=remainder divide
				remainderDivide(this);
				break;
			}
			break;
		case 0x05: // XX05: double-precision numerics
			switch (variant) {
			case 0x01: // 0105: DLA=double-precision add
				doublePrecisionAdd(this, true);
				break;
			case 0x03: // 0305: DLS=double-precision subtract
				doublePrecisionAdd(this, false);
				break;
			case 0x04: // 0405: DLM=double-precision multiply
				doublePrecisionMultiply(this);
				break;
			case 0x08: // 1005: DLD=double-precision floating divide
				doublePrecisionDivide(this);
				break;
			}
			break;
		case 0x09: // XX11: Control State and communication ops
			switch (variant) {
			case 0x01: // 0111: PRL=Program Release
				adjustAFull(this);
				t1 = this->r.A;
				if (!(t1 & MASK_CONTROLW)) {
					// it's an operand
					computeRelativeAddr(this, t1, 0);
					t2 = 1;
				} else if (presenceTest(this, t1)) {
					// present descriptor
					this->r.M = t1 & MASKMEM;
					t2 = 1;
				} else {
					// absent descriptor
					t2 = 0;
				}
				if (t2) {
					loadAviaM(this); // fetch IOD
					if (this->r.NCSF) {
						// test continuity bit, [20:1]
						if (!(this->r.A & 0x8000000)) {
							this->r.I = (this->r.I & 0x0F) | 0x50; // set I07/5: program release
						} else {
							this->r.I = (this->r.I & 0x0F) | 0x60; // set I07/6: continuity bit
						}
						signalInterrupt(this);
						this->r.A = this->r.M;
						this->r.M = this->r.R*64 + 9; // store IOD address in PRT[9]
						storeAviaM(this);
					} else {
						bitReset(&this->r.A, 2);
						storeAviaM(this);
					}
					this->r.AROF = 0;
				}
				break;
			case 0x02: // 0211: ITI=Interrogate Interrupt
				if (this->cc->IAR && !this->r.NCSF) {
					// control-state only
					this->r.C = this->cc->IAR;
					this->r.L = 0;
					this->r.S = 0x40;
					// stack address @100
					clearInterrupt(this);
					this->r.PROF = 0;
					// require fetch at SECL
				}
				break;
			case 0x04: // 0411: RTR=Read Timer
				if (!this->r.NCSF) {
					// control-state only
					adjustAEmpty(this);
					this->r.A = readTimer(this);
					this->r.AROF = 1;
				}
				break;
			case 0x08: // 1011: COM=Communicate
				if (this->r.NCSF) {
					// no-op in Control State
					this->r.M = this->r.R*64 + 9;
					// address = R+@11
					if (this->r.AROF) {
						storeAviaM(this);
						// [M] = A
						this->r.AROF = 0;
					} else if (this->r.BROF) {
						storeBviaM(this);
						// [M] = B
						this->r.BROF = 0;
					} else {
						adjustBFull(this);
						storeBviaM(this);
						// [M] = B
						this->r.BROF = 0;
					}
					this->r.I = (this->r.I & 0x0F) | 0x40;
					// set I07: communicate
					signalInterrupt(this);
				}
				break;
			case 0x11: // 2111: IOR=I/O Release
				if (!this->r.NCSF) {
					// no-op in Normal State
					adjustAFull(this);
					t1 = this->r.A;
					if (!(t1 & MASK_CONTROLW)) {
						// it's an operand
						computeRelativeAddr(this, t1, 0);
						t2 = 1;
					} else if (t1 & MASK_PBIT) {
						this->r.M = t1 % MASKMEM;
						// present descriptor
						t2 = 1;
					} else {
						// for an absent descriptor, just leave it on the stack
						t2 = 0;
					}
					if (t2) {
						loadAviaM(this);
						bitSet(&this->r.A, 2);
						storeAviaM(this);
						this->r.AROF = 0;
					}
				}
				break;
			case 0x12: // 2211: HP2=Halt Processor 2
				if (!(this->r.NCSF || this->cc->HP2F)) {
					// control-state only
					haltP2(this);
				}
				break;
			case 0x14: // 2411: ZPI=Conditional Halt
				if (this->r.US14X) {
					// STOP OPERATOR switch on
					stop(this);
				}
				break;
			case 0x18: // 3011: SFI=Store for Interrupt
				storeForInterrupt(this, 0, 0);
				break;
			case 0x1C: // 3411: SFT=Store for Test
				storeForInterrupt(this, 0, 1);
				break;
			case 0x21: // 4111: IP1=Initiate Processor 1
				if (!this->r.NCSF) {
					// control-state only
					initiate(this, 0);
				}
				break;
			case 0x22: // 4211: IP2=Initiate Processor 2
				if (!this->r.NCSF) {
					// control-state only
					this->r.M = 0x08;
					// INCW is stored in @10
					if (this->r.AROF) {
						storeAviaM(this);
						// [M] = A
						this->r.AROF = 0;
					} else if (this->r.BROF) {
						storeBviaM(this);
						// [M] = B
						this->r.BROF = 0;
					} else {
						adjustAFull(this);
						storeAviaM(this);
						// [M] = A
						this->r.AROF = 0;
					}
					initiateP2(this);
				}
				break;
			case 0x24: // 4411: IIO=Initiate I/O
				if (!this->r.NCSF) {
					// address of IOD is stored in @10
					this->r.M = 010;
					if (this->r.AROF) {
						storeAviaM(this);
						// [M] = A
						this->r.AROF = 0;
					} else if (this->r.BROF) {
						storeBviaM(this);              // [M] = B
						this->r.BROF = 0;
					} else {
						adjustAFull(this);
						storeAviaM(this);
						// [M] = A
						this->r.AROF = 0;
					}
					// let CentralControl choose the I/O Unit
					initiateIO(this);
				}
				break;
			case 0x29: // 5111: IFT=Initiate For Test
				if (!this->r.NCSF) {
					// control-state only
					initiate(this, 1);
				}
				break;
			} // end switch for XX11 ops
			break;
		case 0x0D: // XX15: logical (bitmask) ops
		switch (variant) {
			case 0x01: // 0115: LNG=logical negate
				adjustAFull(this);
				this->r.A ^= MASK_NUMBER;
				break;
			case 0x02: // 0215: LOR=logical OR
				adjustABFull(this);
				this->r.A = (this->r.A & MASK_NUMBER) | this->r.B;
				this->r.BROF = 0;
				break;
			case 0x04: // 0415: LND=logical AND
				adjustABFull(this);
				this->r.A = (this->r.A | MASK_CONTROLW) & this->r.B;
				this->r.BROF = 0;
				break;
			case 0x08: // 1015: LQV=logical EQV
				adjustABFull(this);
				this->r.B ^= (~this->r.A) & MASK_NUMBER;
				this->r.AROF = 0;
				break;
			case 0x10: // 2015: MOP=reset flag bit (make operand)
				adjustAFull(this);
				this->r.A &= MASK_NUMBER;
				break;
			case 0x20: // 4015: MDS=set flag bit (make descriptor)
				adjustAFull(this);
				this->r.A |= MASK_CONTROLW; // set [0:1]
				break;
			}
			break;
		case 0x11: // XX21: load & store ops
			switch (variant) {
			case 0x01: // 0121: CID=Conditional integer store destructive
				integerStore(this, 1, 1);
				break;
			case 0x02: // 0221: CIN=Conditional integer store nondestructive
				integerStore(this, 1, 0);
				break;
			case 0x04: // 0421: STD=Store destructive
				adjustABFull(this);
				if (!(this->r.A & MASK_CONTROLW)) {
					// it's an operand
					computeRelativeAddr(this, this->r.A, 0);
					storeBviaM(this);
					this->r.AROF = this->r.BROF = 0;
				} else {
					// it's a descriptor
					if (presenceTest(this, this->r.A)) {
						this->r.M = this->r.A & MASKMEM;
						storeBviaM(this);
						this->r.AROF = this->r.BROF = 0;
					}
				}
				break;
			case 0x08: // 1021: SND=Store nondestructive
				adjustABFull(this);
				if (!(this->r.A & MASKMEM)) {
					// it's an operand
					computeRelativeAddr(this, this->r.A, 0);
					storeBviaM(this);
					this->r.AROF = 0;
				} else {
					// it's a descriptor
					if (presenceTest(this, this->r.A)) {
						this->r.M = this->r.A & MASKMEM;
						storeBviaM(this);
						this->r.AROF = 0;
					}
				}
				break;
			case 0x10: // 2021: LOD=Load operand
				adjustAFull(this);
				if (!(this->r.A & MASKMEM)) {
					// simple operand
					computeRelativeAddr(this, this->r.A, 1);
					loadAviaM(this);
				} else if (presenceTest(this, this->r.A)) {
					// present descriptor
					this->r.M = this->r.A & MASKMEM;
					loadAviaM(this);
				}
				break;
			case 0x21: // 4121: ISD=Integer store destructive
				integerStore(this, 0, 1);
				break;
			case 0x22: // 4221: ISN=Integer store nondestructive
				integerStore(this, 0, 0);
				break;
			}
			break;
		case 0x15: // XX25: comparison & misc. stack ops
			switch (variant) {
			case 0x01: // 0125: GEQ=compare B greater or equal to A
				this->r.B = (singlePrecisionCompare(this) >= 0) ? 1 : 0;
				break;
			case 0x02: // 0225: GTR=compare B greater to A
				this->r.B = (singlePrecisionCompare(this) > 0) ? 1 : 0;
				break;
			case 0x04: // 0425: NEQ=compare B not equal to A
				this->r.B = (singlePrecisionCompare(this) != 0) ? 1 : 0;
				break;
			case 0x08: // 1025: XCH=exchange TOS words
				exchangeTOS(this);
				break;
			case 0x0C: // 1425: FTC=F field to core field
				adjustABFull(this);
				t1 = (this->r.A & MASK_RCWrF) >> SHFT_RCWrF;
				this->r.B = (this->r.B & ~MASK_RCWrC) | (t1 << SHFT_RCWrC);
				this->r.AROF = 0;
				break;
			case 0x10: // 2025: DUP=Duplicate TOS
				if (this->r.AROF) {
					adjustBEmpty(this);
					this->r.B = this->r.A;
					this->r.BROF = 1;
				} else {
					adjustBFull(this);
					this->r.A = this->r.B;
					this->r.AROF = 1;
				}
				break;
			case 0x1C: // 3425: FTF=F field to F field
				adjustABFull(this);
				t1 = (this->r.A & MASK_RCWrF) >> SHFT_RCWrF;
				this->r.B = (this->r.B & ~MASK_RCWrF) | (t1 << SHFT_RCWrF);
				break;
			case 0x21: // 4125: LEQ=compare B less or equal to A
				this->r.B = (singlePrecisionCompare(this) <= 0) ? 1 : 0;
				break;
			case 0x22: // 4225: LSS=compare B less to A
				this->r.B = (singlePrecisionCompare(this) < 0) ? 1 : 0;
				break;
			case 0x24: // 4425: EQL=compare B equal to A
				this->r.B = (singlePrecisionCompare(this) == 0) ? 1 : 0;
				break;
			case 0x2C: // 5425: CTC=core field to C field
				adjustABFull(this);
				t1 = (this->r.A & MASK_RCWrC) >> SHFT_RCWrC;
				this->r.B = (this->r.B & ~MASK_RCWrC) | (t1 << SHFT_RCWrC);
				this->r.AROF = 0;
				break;
			case 0x3C: // 7425: CTF=core field to F field
				adjustABFull(this);
				t1 = (this->r.A & MASK_RCWrC) >> SHFT_RCWrC;
				this->r.B = (this->r.B & ~MASK_RCWrF) | (t1 << SHFT_RCWrF);
				this->r.AROF = 0;
				break;
			}
			break;
		case 0x19: // XX31: branch, sign-bit, interrogate ops
			switch (variant) {
			case 0x01: // 0131: BBC=branch backward conditional
				adjustABFull(this);
				if (this->r.B & 1) {
					// true => no branch
					this->r.AROF = this->r.BROF = 0;
				} else {
					this->r.BROF = 0;
					if (!(this->r.A & MASK_CONTROLW)) {
						// simple operand
						jumpSyllables(this, -(this->r.A & 0x0fff));
						this->r.AROF = 0;
					} else {
						// descriptor
						if (this->r.L == 0) {
							--this->r.C;
							// adjust for Inhibit Fetch
						}
						if (presenceTest(this, this->r.A)) {
							this->r.C = this->r.A & MASKMEM;
							this->r.L = 0;
							this->r.PROF = 0; // require fetch at SECL
							this->r.AROF = 0;
						}
					}
				}
				break;
			case 0x02: // 0231: BFC=branch forward conditional
				adjustABFull(this);
				if (this->r.B & 1) {
					// true => no branch
					this->r.AROF = this->r.BROF = 0;
				} else {
					this->r.BROF = 0;
					if (!(this->r.A & MASK_CONTROLW)) {
						// simple operand
						jumpSyllables(this, this->r.A & 0x0fff);
						this->r.AROF = 0;
					} else {
						// descriptor
						if (this->r.L == 0) {
							--this->r.C;
							// adjust for Inhibit Fetch
						}
						if (presenceTest(this, this->r.A)) {
							this->r.C = this->r.A & 0x0fff;
							this->r.L = 0;
							this->r.PROF = 0; // require fetch at SECL
							this->r.AROF = 0;
						}
					}
				}
				break;
			case 0x04: // 0431: SSN=set sign bit (set negative)
				adjustAFull(this);
				this->r.A |= MASK_SIGNMANT;
				break;
			case 0x08: // 1031: CHS=change sign bit
				adjustAFull(this);
				this->r.A ^= MASK_SIGNMANT;
				break;
			case 0x10: // 2031: TOP=test flag bit (test for operand)
				adjustAEmpty(this);
				adjustBFull(this);
				this->r.A = this->r.B & MASK_CONTROLW ? 1 : 0;
				this->r.AROF = 1;
				break;
			case 0x11: // 2131: LBC=branch backward word conditional
				adjustABFull(this);
				if (this->r.B & 1) {
					// true => no branch
					this->r.AROF = this->r.BROF = 0;
				} else {
					this->r.BROF = 0;
					if (this->r.L == 0) {
						--this->r.C;
						// adjust for Inhibit Fetch
					}
					if (!(this->r.A & MASK_CONTROLW)) {
						// simple operand
						jumpWords(this, -(this->r.A & 0x03ff));
						this->r.AROF = 0;
					} else {
						// descriptor
						if (presenceTest(this, this->r.A)) {
							this->r.C = this->r.A & MASKMEM;
							this->r.L = 0;
							this->r.PROF = 0; // require fetch at SECL
							this->r.AROF = 0;
						}
					}
				}
				break;
			case 0x12: // 2231: LFC=branch forward word conditional
				adjustABFull(this);
				if (this->r.B & 1) {
					// true => no branch
					this->r.AROF = this->r.BROF = 0;
				} else {
					this->r.BROF = 0;
					if (this->r.L == 0) {
						--this->r.C;
						// adjust for Inhibit Fetch
					}
					if (!(this->r.A & MASK_CONTROLW)) {
						// simple operand
						jumpWords(this, this->r.A & 0x03ff);
						this->r.AROF = 0;
					} else {
						// descriptor
						if (presenceTest(this, this->r.A)) {
							this->r.C = this->r.A & MASKMEM;
							this->r.L = 0;
							this->r.PROF = 0;
							// require fetch at SECL
							this->r.AROF = 0;
						}
					}
				}
				break;
			case 0x14: // 2431: TUS=interrogate peripheral status
				adjustAEmpty(this);
				this->r.A = interrogateUnitStatus(this);
				this->r.AROF = 1;
				break;
			case 0x21: // 4131: BBW=branch backward unconditional
				adjustAFull(this);
				if (!(this->r.A & MASK_CONTROLW)) {
					// simple operand
					jumpSyllables(this, -(this->r.A & 0x0fff));
					this->r.AROF = 0;
				} else {
					// descriptor
					if (this->r.L == 0) {
						--this->r.C;
						// adjust for Inhibit Fetch
					}
					if (presenceTest(this, this->r.A)) {
						this->r.C = this->r.A & MASKMEM;
						this->r.L = 0;
						this->r.PROF = 0;
						// require fetch at SECL
						this->r.AROF = 0;
					}
				}
				break;
			case 0x22: // 4231: BFW=branch forward unconditional
				adjustAFull(this);
				if (!(this->r.A & MASK_CONTROLW)) {
					// simple operand
					jumpSyllables(this, this->r.A & 0x0fff);
					this->r.AROF = 0;
				} else {
					// descriptor
					if (this->r.L == 0) {
						--this->r.C;
						// adjust for Inhibit Fetch
					}
					if (presenceTest(this, this->r.A)) {
						this->r.C = this->r.A & MASKMEM;
						this->r.L = 0;
						this->r.PROF = 0;
						// require fetch at SECL
						this->r.AROF = 0;
					}
				}
				break;
			case 0x24: // 4431: SSP=reset sign bit (set positive)
				adjustAFull(this);
				this->r.A &= ~MASK_SIGNMANT;
				break;
			case 0x31: // 6131: LBU=branch backward word unconditional
				adjustAFull(this);
				if (this->r.L == 0) {
					--this->r.C;
					// adjust for Inhibit Fetch
				}
				if (!(this->r.A & MASK_CONTROLW)) {
					// simple operand
					jumpWords(this, -(this->r.A & 0x03ff));
					this->r.AROF = 0;
				} else {
					// descriptor
					if (presenceTest(this, this->r.A)) {
						this->r.C = this->r.A & MASKMEM;
						this->r.L = 0;
						this->r.PROF = 0;
						// require fetch at SECL
						this->r.AROF = 0;
					}
				}
				break;
			case 0x32: // 6231: LFU=branch forward word unconditional
				adjustAFull(this);
				if (this->r.L == 0) {
					--this->r.C;
					// adjust for Inhibit Fetch
				}
				if (!(this->r.A & MASK_CONTROLW)) {
					// simple operand
					jumpWords(this, this->r.A % 0x0400);
					this->r.AROF = 0;
				} else {
					// descriptor
					if (presenceTest(this, this->r.A)) {
						this->r.C = this->r.A & MASKMEM;
						this->r.L = 0;
						this->r.PROF = 0;
						// require fetch at SECL
						this->r.AROF = 0;
					}
				}
				break;
			case 0x34: // 6431: TIO=interrogate I/O channel
				adjustAEmpty(this);
				this->r.A = interrogateIOChannel(this);
				this->r.AROF = 1;
				break;
			case 0x38: // 7031: FBS=stack search for flag
				adjustAFull(this);
				this->r.M = this->r.A & MASKMEM;
				do {
					loadAviaM(this);
					if (!(this->r.A & MASK_CONTROLW)) {
						this->r.M = (this->r.M+1) & MASKMEM;
					} else {
						this->r.A = this->r.M | (MASK_CONTROLW | MASK_PBIT);
						break;
						// flag bit found: stop the search
					}
				} while (true);
				break;
			}
			break;
		case 0x1D: // XX35: exit & return ops
			switch (variant) {
			case 0x01: // 0135: BRT=branch return
				adjustAEmpty(this);
				if (!this->r.BROF) {
					this->r.Q03F = 1;
					// Q03F: not used, except for display purposes
					adjustBFull(this);
				}
				if (presenceTest(this, this->r.B)) {
					this->r.S = (this->r.B >> 15) & MASKMEM;
					this->r.C = this->r.B & MASKMEM;
					this->r.L = 0;
					this->r.PROF = 0;
					// require fetch at SECL
					loadBviaS(this);
					// B = [S], fetch MSCW
					--this->r.S;
					applyMSCW(this, this->r.B);
					this->r.BROF = 0;
				}
				break;
			case 0x02: // 0235: RTN=return normal
				adjustAFull(this);
				// If A is an operand or a present descriptor,
				// proceed with the return,
				// otherwise throw a p-bit interrupt
				// (this isn't well-documented)
				if (!(this->r.A & MASK_CONTROLW) ||
					presenceTest(this, this->r.A)) {
					this->r.S = this->r.F;
					loadBviaS(this);
					// B = [S], fetch the RCW
					switch (exitSubroutine(this, 0)) {
					case 0:
						this->r.X = 0;
						operandCall(this);
						break;
					case 1:
						this->r.Q05F = 1;
						// set Q05F, for display only
						this->r.X = 0;
						descriptorCall(this);
						break;
					case 2: // flag-bit interrupt occurred, do nothing
						break;
					}
				}
				break;
			case 0x04: // 0435: XIT=exit procedure
				this->r.AROF = 0;
				this->r.S = this->r.F;
				loadBviaS(this);
				// B = [S], fetch the RCW
				exitSubroutine(this, 0);
				break;
			case 0x0A: // 1235: RTS=return special
				adjustAFull(this);
				// If A is an operand or a present descriptor,
				// proceed with the return,
				// otherwise throw a p-bit interrupt
				// (this isn't well-documented)
				if (!(this->r.A & MASK_CONTROLW) ||
					presenceTest(this, this->r.A)) {
					// Note that RTS assumes the RCW
					// is pointed to by S, not F
					loadBviaS(this);
					// B = [S], fetch the RCW
					switch (exitSubroutine(this, 0)) {
					case 0:
						this->r.X = 0;
						operandCall(this);
						break;
					case 1:
						this->r.Q05F = 1;
						// set Q05F, for display only
						this->r.X = 0;
						descriptorCall(this);
						break;
					case 2: // flag-bit interrupt occurred, do nothing
						break;
					}
				}
				break;
			}
			break;
		case 0x21: // XX41: index, mark stack, etc.
			switch (variant) {
			case 0x01: // 0141: INX=index
				adjustABFull(this);
				this->r.M = (this->r.A + this->r.B) & MASKMEM;
				this->r.A = (this->r.A & ~MASKMEM) | this->r.M;
				this->r.BROF = 0;
				break;

			case 0x02: // 0241: COC=construct operand call
				exchangeTOS(this);
				this->r.A |= MASK_CONTROLW;
				// set [0:1]
				operandCall(this);
				break;
			case 0x04: // 0441: MKS=mark stack
				adjustABEmpty(this);
				this->r.B = buildMSCW(this);
				this->r.BROF = 1;
				adjustBEmpty(this);
				this->r.F = this->r.S;
				if (!this->r.MSFF) {
					if (this->r.SALF) {
						// store the MSCW at R+7
						this->r.M = this->r.R*64 + 7;
						storeBviaM(this);
						// [M] = B
					}
					this->r.MSFF = 1;
				}
				break;
			case 0x0A: // 1241: CDC=construct descriptor call
				exchangeTOS(this);
				this->r.A |= MASK_CONTROLW;
				// set [0:1]
				descriptorCall(this);
				break;
			case 0x11: // 2141: SSF=F & S register set/store
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
					this->r.SALF = 1;
					this->r.BROF = 0;
					break;
				case 3: // set   S from B.[33:15]
					this->r.S = (this->r.B & MASK_RCWrC) >> SHFT_RCWrC;
					this->r.BROF = 0;
					break;
				}
				this->r.AROF = 0;
				break;
			case 0x15: // 2541: LLL=link list look-up
				adjustABFull(this);
				t1 = this->r.A % 0x8000000000;
				// test value
				this->r.M = this->r.B % 0x8000;
				// starting link address
				do {
					this->cycleCount += 2;
					// approximate the timing
					loadBviaM(this);
					t2 = this->r.B % 0x8000000000;
					if (t2 < t1) {
						this->r.M = t2 % 0x8000;
					} else {
						this->r.A = this->r.M + 0xA00000000000;
						break;
						// B >= A: stop look-up
					}
				} while (true);
				break;
			case 0x24: // 4441: CMN=enter character mode inline
				enterCharModeInline(this);
				break;
			}
			break;
		case 0x25: // XX45: ISO=Variable Field Isolate op
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
		case 0x29: // XX51: delete & conditional branch ops
			if (variant < 4) {
				// 0051=DEL: delete TOS (or field branch with zero-length field)
				if (this->r.AROF) {
					this->r.AROF = 0;
				} else if (this->r.BROF) {
					this->r.BROF = 0;
				} else {
					--this->r.S;
				}
			} else {
				adjustABFull(this);
				t2 = variant >> 2;
				// field length (1-15 bits)
				t1 = fieldIsolate(this->r.B, this->r.G*6+this->r.H, t2);
				this->cycleCount += this->r.G + this->r.H + (t2 >> 1);
				// approximate the shift counts
				this->r.AROF = 0;
				// A is unconditionally empty at end
				switch (variant & 0x03) {
				case 0x02: // X251/X651: CFD=non-zero field branch forward destructive
					this->r.BROF = 0;
					// no break: fall through
				case 0x00: // X051/X451: CFN=non-zero field branch forward nondestructive
					if (t1) {
						if (this->r.A < 0x800000000000) {
							// simple operand
							jumpSyllables(this, this->r.A & 0x0fff);
						} else {
							// descriptor
							if (this->r.L == 0) {
								--this->r.C;
								// adjust for Inhibit Fetch
							}
							if (presenceTest(this, this->r.A)) {
								this->r.C = this->r.A % 0x8000;
								this->r.L = 0;
								this->r.PROF = 0;
								// require fetch at SEQL
							}
						}
					}
					break;
				case 0x03: // X351/X751: CBD=non-zero field branch backward destructive
					this->r.BROF = 0;
					// no break: fall through
				case 0x01: // X151/X551: CBN=non-zero field branch backward nondestructive
					if (t1) {
						if (this->r.A < 0x800000000000) {
							// simple operand
							jumpSyllables(this, -(this->r.A & 0x0fff));
						} else {
							// descriptor
							if (this->r.L == 0) {
								--this->r.C;
								// adjust for Inhibit Fetch
							}
							if (presenceTest(this, this->r.A)) {
								this->r.C = this->r.A % 0x8000;
								this->r.L = 0;
								this->r.PROF = 0;
								// require fetch at SEQL
							}
						}
					}
				break;
				}
			}
			break;
		case 0x2D: // XX55: NOP & DIA=Dial A ops
			if (opcode & 0xFC0) {
				this->r.G = variant >> 3;
				this->r.H = variant & 7;
			// else // 0055: NOP=no operation (the official one, at least)
			}
			break;
		case 0x31: // XX61: XRT & DIB=Dial B ops
			if (opcode & 0xFC0) {
				this->r.K = variant >> 3;
				this->r.V = variant & 7;
			} else { // 0061=XRT: temporarily set full PRT addressing mode
				this->r.VARF = this->r.SALF;
				this->r.SALF = 0;
			}
			break;
		case 0x35: // XX65: TRB=Transfer Bits
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
			this->r.AROF = 0;
			this->cycleCount += variant + this->r.G + this->r.K;       // approximate the shift counts
			break;
		case 0x39: // XX71: FCL=Compare Field Low
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
				this->r.A = 1;
			} else if (fieldIsolate(this->r.B, t2, variant) < fieldIsolate(this->r.A, t1, variant)) {
				this->r.A = 1;
			} else {
				this->r.A = 0;
			}
			this->cycleCount += variant + this->r.G + this->r.K;       // approximate the shift counts
			break;
		case 0x3D: // XX75: FCE=Compare Field Equal
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
				this->r.A = 1;
			} else if (fieldIsolate(this->r.B, t2, variant) == fieldIsolate(this->r.A, t1, variant)) {
				this->r.A = 1;
			} else {
				this->r.A = 0;
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
