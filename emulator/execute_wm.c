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
* 2017-07-17  R.Meyer
*   changed "this" to "cpu" to avoid errors when using g++
* 2017-09-30  R.Meyer
*   overhaul of file names
* 2017-10-10  R.Meyer
*   some refactoring in the functions, added documentation
***********************************************************************/

#include <stdio.h>
#include "common.h"

#define DEBUG_TRB false
#define DEBUG_FCE false
#define DEBUG_FCL false

#define	CONTROL_STATE_ONLY if (cpu->r.NCSF) return
#define	NORMAL_STATE_ONLY if (!cpu->r.NCSF) return

/***********************************************************************
* execute one word mode instruction
***********************************************************************/
void b5500_execute_wm(CPU *cpu)
{
        WORD12 opcode = cpu->r.T;
        WORD12 variant;
        WORD48 t1, t2;

        // clear all Q flags, Y, Z, M, N, X registers for each word mode instruction
        cpu->r.Q01F = false; cpu->r.Q02F = false; cpu->r.Q03F = false;
        cpu->r.Q04F = false; cpu->r.Q05F = false; cpu->r.Q06F = false;
        cpu->r.Q07F = false; cpu->r.Q08F = false; cpu->r.Q09F = false;
        cpu->r.Y = 0; cpu->r.Z = 0; cpu->r.M = 0; cpu->r.N = 0;
        cpu->r.X = 0;

/***********************************************************************
* Word Mode Opcode Analysis
***********************************************************************/
        // check last 2 bits of opcode first
	variant = opcode & 3;

/***********************************************************************
* LITC: Literal Call
* Constant to top of stack
***********************************************************************/
	if (variant == 0) {
                adjustAEmpty(cpu);
                cpu->r.A = opcode >> 2;
                cpu->r.AROF = true;
                return;
	}

/***********************************************************************
* OPDC: Operand Call
* Value to top of stack
* Subroutines might be called
***********************************************************************/
	if (variant == 2) {
                adjustAEmpty(cpu);
                computeRelativeAddr(cpu, opcode >> 2, true);
                loadAviaM(cpu);
                if (DESCRIPTOR(cpu->r.A)) {
                        // if it's a control word, evaluate it
                        operandCall(cpu);
                }
                // otherwise, just leave it in A
                return;
	}

/***********************************************************************
* DESC: Descriptor (name) Call
* Address to top of stack
* Subroutines might be called
***********************************************************************/
	if (variant == 3) {
                adjustAEmpty(cpu);
                computeRelativeAddr(cpu, opcode >> 2, true);
                loadAviaM(cpu);
                descriptorCall(cpu);
                return;
        }

/***********************************************************************
* XXX1, XXX5: all other word-mode operators
***********************************************************************/
        
        variant = opcode >> 6;

        switch (opcode & 077) {

/***********************************************************************
* XX01: single-precision numerics
* 0101: ADD=single-precision add
* 0301: SUB=single-precision subtract
* 0401: MUL=single-precision multiply
* 1001: DIV=single-precision floating divide
* 3001: IDV=integer divide
* 7001: RDV=remainder divide
***********************************************************************/
        case 001:
                switch (variant) {
                case 001: singlePrecisionAdd(cpu, true); return;
                case 003: singlePrecisionAdd(cpu, false); return;
                case 004: singlePrecisionMultiply(cpu); return;
                case 010: singlePrecisionDivide(cpu); return;
                case 030: integerDivide(cpu); return;
                case 070: remainderDivide(cpu); return;
		default: goto unused;
                }
		return;

/***********************************************************************
* XX05: double-precision numerics
* 0105: DLA=double-precision add
* 0305: DLS=double-precision subtract
* 0405: DLM=double-precision multiply
* 1005: DLD=double-precision floating divide
***********************************************************************/
	case 005:
		switch (variant) {
		case 001: doublePrecisionAdd(cpu, true); return;
		case 003: doublePrecisionAdd(cpu, false); return;
		case 004: doublePrecisionMultiply(cpu); return;
		case 010: doublePrecisionDivide(cpu); return;
		default: goto unused;
		}
		return;

/***********************************************************************
* XX11: Control State and communication ops
***********************************************************************/
        case 011:
                switch (variant) {

/***********************************************************************
* 0111: PRL=Program Release
* Get TOS into A (TOS should be operand or descriptor)
* if Operand then its a relative address. Compute it into M
* if Descriptor
*   if present, load A into M;
*   otherwise cause presence bit IRQ and exit
* load A from [M]
* in control state clear presence bit and
*   store A back into [M] and exit
* in normal state check continuity bit
*   if set cause continuity IRQ
*   if clear cause program release IRQ
*   save M (address) in [R+9]
* set A empty
***********************************************************************/
                case 001:
                        adjustAFull(cpu);
                        if (OPERAND(cpu->r.A)) {
                                // operand
                                computeRelativeAddr(cpu, cpu->r.A, false);
                        } else if (presenceTest(cpu, cpu->r.A)) {
                                // present descriptor
                                cpu->r.M = cpu->r.A & MASKMEM;
                        } else {
				// not present
				// leave address on stack and exit
				return;
                        }
			// fetch the value
			loadAviaM(cpu);
			if (cpu->r.NCSF) {
				// normal state
				// test continuity bit, [20:1]
				if (cpu->r.A & MASK_CONT) {
					causeSyllableIrq(cpu, IRQ_CONT, "PRL-CONT");
				} else {
					causeSyllableIrq(cpu, IRQ_PREL, "PRL-PRL");
				}
				// store IOD address in PRT[9]
				cpu->r.A = cpu->r.M;
				cpu->r.M = (cpu->r.R<<RSHIFT) + RR_COM;
				storeAviaM(cpu);
			} else {
				// control state
				// just clear presence bit and store back
				cpu->r.A &= ~MASK_PBIT;
				storeAviaM(cpu);
			}
			// remove address
			cpu->r.AROF = false;
                        return;

/***********************************************************************
* 0211: ITI=Interrogate Interrupt
* poll pending IRQs
* if any pending, set stack and transfer to IRQ routine
* otherwise exit
***********************************************************************/
                case 002:
			CONTROL_STATE_ONLY;
			// use M as temporary
			cpu->r.M = CC->IAR;
			if (cpu->r.M) {
				cpu->r.PROF = false; // require fetch at SECL
				cpu->r.C = cpu->r.M;
				cpu->r.L = 0;
				cpu->r.S = AA_IRQSTACK; // stack address @100
				clearInterrupt(cpu->r.M); // clear IRQ
			}
                        return;

/***********************************************************************
* 0411: RTR=Read Timer
* put 7 bits of timer IRQ pending (1 bit) and the timer count (6 bits)
* in A right jusitfied. Rest of A is zero
***********************************************************************/
                case 004:
                        CONTROL_STATE_ONLY;
                        adjustAEmpty(cpu);
                        cpu->r.A = readTimer(cpu);
                        cpu->r.AROF = true;
                        return;

/***********************************************************************
* 1011: COM=Communicate
* in normal state store word at TOS in [R+9]
* set communicate IRQ
* delete TOS
* NOP in control state
***********************************************************************/
                case 010:
                        NORMAL_STATE_ONLY;
                        cpu->r.M = (cpu->r.R<<RSHIFT) + RR_COM;
                        if (cpu->r.AROF) { // TOS in A
                                storeAviaM(cpu);
                                cpu->r.AROF = false;
                        } else if (cpu->r.BROF) { // TOS in B
                                storeBviaM(cpu);
                                cpu->r.BROF = false;
                        } else { // load TOS to B
                                adjustBFull(cpu);
                                storeBviaM(cpu);
                                cpu->r.BROF = false;
                        }
			causeSyllableIrq(cpu, IRQ_COM, "COM");
                        return;

/***********************************************************************
* 2111: IOR=I/O Release
* in normal state just exit
* Get TOS into A (TOS should be operand or descriptor)
* if Operand then its a relative address. Compute it into M
* if Descriptor
*   if present, load A into M;
*   otherwise just exit (cause no IRQ)
* load A from [M]
* set presence bit
* store A back into [M]
* set A empty
***********************************************************************/
                case 021:
                        CONTROL_STATE_ONLY;
			adjustAFull(cpu);
			if (OPERAND(cpu->r.A)) {
				// it's an operand
				computeRelativeAddr(cpu, cpu->r.A, 0);
			} else if (PRESENT(cpu->r.A)) {
				cpu->r.M = cpu->r.A & MASKMEM;
				// present descriptor
			} else {
				// leave address on stack and exit
				return;
			}
			// set the presence bit of the word at the designed address
			loadAviaM(cpu);
			cpu->r.A |= MASK_PBIT;
			storeAviaM(cpu);
			// remove address
			cpu->r.AROF = false;
                        return;

/***********************************************************************
* 2211: HP2=Halt Processor 2
* NOP when not in control state
* NOP when HP2F already set
***********************************************************************/
                case 022:
			CONTROL_STATE_ONLY;
			if (!CC->HP2F) haltP2(cpu);
			return;

/***********************************************************************
* 2411: ZPI=Conditional Halt
* if operator switch is on, halt the processor
***********************************************************************/
		case 024:
			if (cpu->r.US14X)
				stop(cpu);
			return;

/***********************************************************************
* 3011: SFI=Store for Interrupt
* here: caused by instruction (forced=false, forTest=false)
* 3411: SFT=Store for Test
* here: caused by instruction (forced=false, forTest=true)
* for a detailed description of operation see storeForInterrupt()
***********************************************************************/
                case 030: storeForInterrupt(cpu, false, false, "SFI"); return;
                case 034: storeForInterrupt(cpu, false, true, "SFT"); return;

/***********************************************************************
* 4111: IP1=Initiate Processor 1
* INCW is in TOS (A, B or [S])
* here: (forTest=false)
* for a detailed description of operation see initiate()
***********************************************************************/
                case 041:
                        CONTROL_STATE_ONLY;
                        initiate(cpu, false);
                        return;

/***********************************************************************
* 4211: IP2=Initiate Processor 2
* INCW is in TOS (A, B or [S]), store it in @10
* initate P2
***********************************************************************/
                case 042:
                        CONTROL_STATE_ONLY;
			cpu->r.M = AA_IODESC;
			if (cpu->r.AROF) {
				storeAviaM(cpu);
				cpu->r.AROF = false;
			} else if (cpu->r.BROF) {
				storeBviaM(cpu);
				cpu->r.BROF = false;
			} else {
				adjustAFull(cpu);
				storeAviaM(cpu);
				cpu->r.AROF = false;
			}
			// send signal to central control
			initiateP2(cpu);
                        return;

/***********************************************************************
* 4411: IIO=Initiate I/O
***********************************************************************/
                case 044:
                        CONTROL_STATE_ONLY;
                        // address of IOD is stored in @10
                        cpu->r.M = AA_IODESC;
                        if (cpu->r.AROF) {
                                storeAviaM(cpu);
                                cpu->r.AROF = false;
                        } else if (cpu->r.BROF) {
                                storeBviaM(cpu);
                                cpu->r.BROF = false;
                        } else {
                                adjustAFull(cpu);
                                storeAviaM(cpu);
                                // [M] = A
                                cpu->r.AROF = false;
                        }
                        // let CentralControl choose the I/O Unit
                        initiateIO(cpu);
                        return;

/***********************************************************************
* 5111: IFT=Initiate For Test
***********************************************************************/
		case 051:
			CONTROL_STATE_ONLY;
			initiate(cpu, true);
			return;

		default: goto unused;
		} // end switch for XX11 ops
		return;


/***********************************************************************
* XX15: logical (bitmask) ops
* ensure A full, ensure B full for LOR, LND, LQV
* 0115: LNG=logical negate
* complement A except for flag
* 0215: LOR=logical OR
* bitwise or B into A, except for flag, set A.flag = B.flag, set B empty
* 0415: LND=logical AND
* bitwise and B into A, except for flag, set A.flag = B.flag, set B empty
* 1015: LQV=logical EQV
* bitwise equivalence A into B, except for flag, preserve B.flag, set A empty
* 2015: MOP=reset flag bit (make operand)
* set A.flag = 0
* 4015: MDS=set flag bit (make descriptor)
* set A.flag = 1
***********************************************************************/
        case 015:
                switch (variant) {
                case 001: adjustAFull(cpu);
			cpu->r.A ^= MASK_NUMBER; return;
                case 002: adjustABFull(cpu);
			cpu->r.A = (cpu->r.A & MASK_NUMBER) | cpu->r.B;
			cpu->r.BROF = false; return;
                case 004: adjustABFull(cpu);
			cpu->r.A = (cpu->r.A | MASK_FLAG) & cpu->r.B;
			cpu->r.BROF = false; return;
                case 010: adjustABFull(cpu);
                        cpu->r.B ^= (~cpu->r.A) & MASK_NUMBER;
                        cpu->r.AROF = false; return;
                case 020: adjustAFull(cpu);
                        cpu->r.A &= MASK_NUMBER; return;
                case 040: adjustAFull(cpu);
                        cpu->r.A |= MASK_FLAG; return;
		default: goto unused;
                }
                return;

/***********************************************************************
* XX21: load & store ops
***********************************************************************/
        case 021:
                switch (variant) {

/***********************************************************************
* 0121: CID=Conditional integer store destructive
* 0221: CIN=Conditional integer store nondestructive
* if A.integer bit then convert B to integer, cause integerger overflow is so happens
* store to address designated by A
* 4121: ISD=Integer store destructive
* 4221: ISN=Integer store nondestructive
* convert B to integer, cause integerger overflow is so happens
* store B to address designated by A
***********************************************************************/
                case 001: integerStore(cpu, true, true); return;
                case 002: integerStore(cpu, true, false); return;
                case 041: integerStore(cpu, false, true); return;
                case 042: integerStore(cpu, false, false); return;

/***********************************************************************
* 0421: STD=Store destructive
* 1021: SND=Store nondestructive
* ensure A and B full
* store B to address designated by A, set A empty
* if destructive, set B empty
***********************************************************************/
		case 004:
		case 010:
                        adjustABFull(cpu);
                        if (OPERAND(cpu->r.A)) {
                                // operand
                                computeRelativeAddr(cpu, cpu->r.A, false);
                        } else if (presenceTest(cpu, cpu->r.A)) {
				// present descriptor
				cpu->r.M = cpu->r.A & MASKMEM;
			} else {
				// not present
				// leave address and value on stack and exit
				return;
			}
			// now store
                        storeBviaM(cpu);
			// remove address
                        cpu->r.AROF = false;
			// if destructive, remove value
			if (variant == 004)
				cpu->r.BROF = false;
                        return;

/***********************************************************************
* 2021: LOD=Load operand
* ensure A full
* load A from address designated by A
***********************************************************************/
                case 020:
                        adjustAFull(cpu);
                        if (OPERAND(cpu->r.A)) {
                                // operand
                                computeRelativeAddr(cpu, cpu->r.A, true);
                        } else if (presenceTest(cpu, cpu->r.A)) {
                                // present descriptor
                                cpu->r.M = cpu->r.A & MASKMEM;
                        } else {
				// not present
				// leave address on stack and exit
				return;
			}
			// now load value
                        loadAviaM(cpu);
                        return;

		default: goto unused;
                }
                return;

/***********************************************************************
* XX25: comparison & misc. stack ops
***********************************************************************/
        case 025:
                switch (variant) {

/***********************************************************************
* 0125: GEQ=compare B greater or equal to A
* 0225: GTR=compare B greater to A
* 0425: NEQ=compare B not equal to A
* 4125: LEQ=compare B less or equal to A
* 4225: LSS=compare B less to A
* 4425: EQL=compare B equal to A
***********************************************************************/
		case 001: cpu->r.B = (singlePrecisionCompare(cpu) >= 0) ? true : false; return;
		case 002: cpu->r.B = (singlePrecisionCompare(cpu) > 0) ? true : false; return;
		case 004: cpu->r.B = (singlePrecisionCompare(cpu) != 0) ? true : false; return;
		case 041: cpu->r.B = (singlePrecisionCompare(cpu) <= 0) ? true : false; return;
		case 042: cpu->r.B = (singlePrecisionCompare(cpu) < 0) ? true : false; return;
		case 044: cpu->r.B = (singlePrecisionCompare(cpu) == 0) ? true : false; return;

/***********************************************************************
* 1025: XCH=exchange TOS words
***********************************************************************/
		case 010: exchangeTOS(cpu); return;

/***********************************************************************
* 2025: DUP=Duplicate TOS
* if A is full, make B empty and copy to B
* if A is empty, make B full and copy to A
***********************************************************************/
                case 020:
			if (cpu->r.AROF) {
				adjustBEmpty(cpu);
				cpu->r.B = cpu->r.A;
				cpu->r.BROF = true;
			} else {
				adjustBFull(cpu);
				cpu->r.A = cpu->r.B;
				cpu->r.AROF = true;
			}
			return;

/***********************************************************************
* 1425: FTC=F field to C field
* 3425: FTF=F field to F field
* 5425: CTC=C field to C field
* 7425: CTF=C field to F field
* F field = bits 18..32
* C field = bits 33..47
* ensure A and B full
* move field from A into B
* mark A empty
***********************************************************************/
#if SHFT_CREG > SHFT_FREG
#error SHFT_CREG > SHFT_FREG
#endif
		case 014: adjustABFull(cpu);
			cpu->r.B = (cpu->r.B & ~MASK_CREG) | ((cpu->r.A & MASK_FREG) >> (SHFT_FREG - SHFT_CREG));
			cpu->r.AROF = false; return;
		case 034: adjustABFull(cpu);
			cpu->r.B = (cpu->r.B & ~MASK_FREG) | (cpu->r.A & MASK_FREG);
			cpu->r.AROF = false; return;
		case 054: adjustABFull(cpu);
			cpu->r.B = (cpu->r.B & ~MASK_CREG) | (cpu->r.A & MASK_CREG);
			cpu->r.AROF = false; return;
		case 074: adjustABFull(cpu);
			cpu->r.B = (cpu->r.B & ~MASK_FREG) | ((cpu->r.A & MASK_CREG) << (SHFT_FREG - SHFT_CREG));
			cpu->r.AROF = false; return;

		default: goto unused;
		}
		return;

/***********************************************************************
* XX31: branch, sign-bit, interrogate ops
***********************************************************************/
        case 031:
                switch (variant) {

/***********************************************************************
* 0131: BBC=branch backward conditional
* 0231: BFC=branch forward conditional
***********************************************************************/
                case 001:
                case 002:
                        adjustABFull(cpu);
                        if (cpu->r.B & 1) {
                                // true => no branch
                                cpu->r.AROF = cpu->r.BROF = false;
                                break;
                        }
                        cpu->r.BROF = false;
                        goto common_branch;

/***********************************************************************
* 4131: BBW=branch backward unconditional
* 4231: BFW=branch forward unconditional
***********************************************************************/
                case 041:
                case 042: // 4231: BFW=branch forward unconditional
                        adjustAFull(cpu);
common_branch:
                        if (OPERAND(cpu->r.A)) {
                                // simple operand
                                if (variant == 001 || variant == 041)
                                        jumpSyllables(cpu, -(cpu->r.A & 0xfff));
                                else
                                        jumpSyllables(cpu, cpu->r.A & 0xfff);
                                cpu->r.AROF = false;
                        } else {
                                // descriptor
                                if (cpu->r.L == 0) {
                                        --cpu->r.C;
                                        // adjust for Inhibit Fetch
                                }
                                if (presenceTest(cpu, cpu->r.A)) {
                                        cpu->r.C = cpu->r.A & MASKMEM;
                                        cpu->r.L = 0;
                                        // require fetch at SECL
                                        cpu->r.PROF = false;
                                        cpu->r.AROF = false;
                                }
                        }
                        return;

/***********************************************************************
* 2131: LBC=branch backward word conditional
* 2231: LFC=branch forward word conditional
***********************************************************************/
                case 021:
                case 022:
                        adjustABFull(cpu);
                        if (cpu->r.B & 1) {
                                // true => no branch
                                cpu->r.AROF = cpu->r.BROF = false;
                                break;
                        }
                        cpu->r.BROF = false;
                        goto common_branch_word;

/***********************************************************************
* 6131: LBU=branch backward word unconditional
* 6231: LFU=branch forward word unconditional
***********************************************************************/
                case 061:
                case 062:
                        adjustAFull(cpu);
common_branch_word:
                        if (cpu->r.L == 0) {
                                --cpu->r.C;
                                // adjust for Inhibit Fetch
                        }
                        if (OPERAND(cpu->r.A)) {
                                // simple operand
                                if (variant == 021 || variant == 061)
                                        jumpWords(cpu, -(cpu->r.A & 0x03ff));
                                else
                                        jumpWords(cpu, cpu->r.A & 0x03ff);
                                cpu->r.AROF = false;
                        } else {
                                // descriptor
                                if (presenceTest(cpu, cpu->r.A)) {
                                        cpu->r.C = cpu->r.A & MASKMEM;
                                        cpu->r.L = 0;
                                        // require fetch at SECL
                                        cpu->r.PROF = false;
                                        cpu->r.AROF = false;
                                }
                        }
                        return;

/***********************************************************************
* 0431: SSN=set sign bit (set negative)
* 1031: CHS=change sign bit
* 4431: SSP=reset sign bit (set positive)
***********************************************************************/
                case 004: adjustAFull(cpu); cpu->r.A |= MASK_SIGNMANT; return;
                case 010: adjustAFull(cpu); cpu->r.A ^= MASK_SIGNMANT; return;
                case 044: adjustAFull(cpu); cpu->r.A &= ~MASK_SIGNMANT; return;

/***********************************************************************
* 2031: TOP=test flag bit (test for operand)
* test flag bit of TOS and add result to stack!
***********************************************************************/
                case 020: adjustAEmpty(cpu); adjustBFull(cpu);
                        cpu->r.A = OPERAND(cpu->r.B) ? true : false;
                        cpu->r.AROF = true; return;

/***********************************************************************
* 2431: TUS=interrogate peripheral status
***********************************************************************/
                case 024: adjustAEmpty(cpu);
                        cpu->r.A = interrogateUnitStatus(cpu);
                        cpu->r.AROF = true; return;

/***********************************************************************
* 6431: TIO=interrogate I/O channel
***********************************************************************/
                case 064: adjustAEmpty(cpu);
                        cpu->r.A = interrogateIOChannel(cpu);
                        cpu->r.AROF = true; return;

/***********************************************************************
* 7031: FBS=stack search for flag
***********************************************************************/
                case 070: adjustAFull(cpu);
                        cpu->r.M = cpu->r.A & MASKMEM;
                        loadAviaM(cpu);
                        while (OPERAND(cpu->r.A)) {
                                cpu->r.M = (cpu->r.M+1) & MASKMEM;
                                loadAviaM(cpu);
                        }
                        // flag bit found: stop the search
                        cpu->r.A = INIT_DD | MASK_PBIT | cpu->r.M;
                        return;

		default: goto unused;
                }
                return;

/***********************************************************************
* XX35: exit & return ops
***********************************************************************/
        case 035:
                switch (variant) {

/***********************************************************************
* 0135: BRT=branch return
* ?
***********************************************************************/
                case 001:
                        adjustAEmpty(cpu);
                        if (!cpu->r.BROF) {
                                cpu->r.Q03F = true;
                                // Q03F: not used, except for display purposes
                                adjustBFull(cpu);
                        }
                        if (presenceTest(cpu, cpu->r.B)) {
                                cpu->r.S = (cpu->r.B >> SHFT_FREG) & MASKMEM;
                                cpu->r.C = cpu->r.B & MASKMEM;
                                cpu->r.L = 0;
                                cpu->r.PROF = false;
                                // require fetch at SECL
                                loadBviaS(cpu);
                                // B = [S], fetch MSCW
                                --cpu->r.S;
                                applyMSCW(cpu, cpu->r.B);
                                cpu->r.BROF = false;
                        }
                        return;

/***********************************************************************
* 0235: RTN=return normal
* 1235: RTS=return special
* ?
***********************************************************************/
                case 002:
                case 012:
                        adjustAFull(cpu);
                        // If A is an operand or a present descriptor,
                        // proceed with the return,
                        // otherwise throw a p-bit interrupt
                        // (cpu isn't well-documented)
                        if (OPERAND(cpu->r.A) || presenceTest(cpu, cpu->r.A)) {
                                if (variant == 002) {
                                        // RTN - reset stack to F to be at RCW
                                        cpu->r.S = cpu->r.F;
                                } else {
                                        // RTS - stack already at RCW
                                }
                                loadBviaS(cpu);
                                // B = [S], fetch the RCW
                                switch (exitSubroutine(cpu, false)) {
                                case 0:
                                        cpu->r.X = 0;
                                        operandCall(cpu);
                                        break;
                                case 1:
                                        // set Q05F, for display only
                                        cpu->r.Q05F = true;
                                        cpu->r.X = 0;
                                        descriptorCall(cpu);
                                        break;
                                case 2: // flag-bit interrupt occurred, do nothing
                                        break;
                                }
                        }
                        return;

                case 004: // 0435: XIT=exit procedure
                        cpu->r.AROF = false;
                        cpu->r.S = cpu->r.F;
                        loadBviaS(cpu);
                        // B = [S], fetch the RCW
                        exitSubroutine(cpu, false);
                        return;

		default: goto unused;
                }
                return;

        case 041: // XX41: index, mark stack, etc.
                switch (variant) {
                case 001: // 0141: INX=index
                        adjustABFull(cpu);
                        cpu->r.M = (cpu->r.A + cpu->r.B) & MASKMEM;
                        cpu->r.A = (cpu->r.A & ~MASKMEM) | cpu->r.M;
                        cpu->r.BROF = false;
                        return;

                case 002: // 0241: COC=construct operand call
                        exchangeTOS(cpu);
                        cpu->r.A |= MASK_FLAG;
                        // set [0:1]
                        operandCall(cpu);
                        return;
                case 004: // 0441: MKS=mark stack
                        adjustABEmpty(cpu);
                        cpu->r.B = buildMSCW(cpu);
                        cpu->r.BROF = true;
                        adjustBEmpty(cpu);
                        cpu->r.F = cpu->r.S;
                        if (!cpu->r.MSFF) {
                                if (cpu->r.SALF) {
                                        // store the MSCW at R+7
                                        cpu->r.M = (cpu->r.R<<6) + RR_MSCW;
                                        storeBviaM(cpu);
                                }
                                cpu->r.MSFF = true;
                        }
                        return;
                case 012: // 1241: CDC=construct descriptor call
                        exchangeTOS(cpu);
                        cpu->r.A |= MASK_FLAG;
                        // set [0:1]
                        descriptorCall(cpu);
                        return;
                case 021: // 2141: SSF=F & S register set/store
                        adjustABFull(cpu);
                        switch (cpu->r.A & 3) {
                        case 0: // store F into B.[18:15]
                                cpu->r.B = (cpu->r.B & ~MASK_FREG) | (cpu->r.F << SHFT_FREG);
                                break;
                        case 1: // store S into B.[33:15]
                                cpu->r.B = (cpu->r.B & ~MASK_CREG) | (cpu->r.S << SHFT_CREG);
                                break;
                        case 2: // set F from B.[18:15]
                                cpu->r.F = (cpu->r.B & MASK_FREG) >> SHFT_FREG;
                                cpu->r.SALF = true;
                                cpu->r.BROF = false;
                                break;
                        case 3: // set S from B.[33:15]
                                cpu->r.S = (cpu->r.B & MASK_CREG) >> SHFT_CREG;
                                cpu->r.BROF = false;
                                break;
                        }
                        cpu->r.AROF = false;
                        return;
                case 025: // 2541: LLL=link list look-up
                        adjustABFull(cpu);
                        // get test field
                        t1 = cpu->r.A & MASK_MANTISSA;
                        // test value
                        cpu->r.M = cpu->r.B & MASKMEM;
                        // starting link address
                        do {
                                cpu->cycleCount += 2;
                                // approximate the timing
                                loadBviaM(cpu);
                                t2 = cpu->r.B & MASK_MANTISSA;
                                if (t2 < t1) {
                                        cpu->r.M = t2 & MASKMEM;
                                } else {
                                        cpu->r.A = INIT_DD | MASK_PBIT | cpu->r.M;
                                        break;
                                        // B >= A: stop look-up
                                }
                        } while (true);
                        return;
                case 044: // 4441: CMN=enter character mode inline
                        enterCharModeInline(cpu);
                        return;
		default: goto unused;
                }
                return;
        case 045: // XX45: ISO=Variable Field Isolate op
                adjustAFull(cpu);
                t2 = variant >> 3; // number of whole chars
                if (t2) {
                        t1 = cpu->r.G*6 + cpu->r.H; // starting source bit position
                        t2 = t2*6 - (variant & 7) - cpu->r.H; // number of bits
                        if (t1+t2 <= 48) {
                                cpu->r.A = fieldIsolate(cpu->r.A, t1, t2);
                        } else {
                                // handle wrap-around in the source value
                                cpu->r.A = fieldInsert(
                                        fieldIsolate(cpu->r.A, 0, t2-48+t1),
                                        48-t2, 48-t1,
                                        fieldIsolate(cpu->r.A, t1, 48-t1));
                        }
                        // approximate the shift cycle counts
                        cpu->cycleCount += (variant >> 3) + (variant & 7) + cpu->r.G + cpu->r.H;
                        cpu->r.G = (cpu->r.G + (variant >> 3)) & 7;
                        cpu->r.H = 0;
                }
                return;
        case 051: // XX51: delete & conditional branch ops
                if (variant < 4) {
                        // 0051=DEL: delete TOS (or field branch with zero-length field)
                        if (cpu->r.AROF) {
                                cpu->r.AROF = false;
                        } else if (cpu->r.BROF) {
                                cpu->r.BROF = false;
                        } else {
                                --cpu->r.S;
                        }
                } else {
                        adjustABFull(cpu);
                        // field length (1-15 bits)
                        t2 = variant >> 2;
                        t1 = fieldIsolate(cpu->r.B, cpu->r.G*6+cpu->r.H, t2);
                        // approximate the shift counts
                        cpu->cycleCount += cpu->r.G + cpu->r.H + (t2 >> 1);
                        // A is unconditionally empty at end
                        cpu->r.AROF = false;
                        switch (variant & 0x03) {
                        case 0x02: // X251/X651: CFD=non-zero field branch forward destructive
                                cpu->r.BROF = false;
                                // no break: fall through
                        case 0x00: // X051/X451: CFN=non-zero field branch forward nondestructive
                                if (t1) {
                                        if (OPERAND(cpu->r.A)) {
                                                // simple operand
                                                jumpSyllables(cpu, cpu->r.A & 0x0fff);
                                        } else {
                                                // descriptor
                                                if (cpu->r.L == 0) {
                                                        // adjust for Inhibit Fetch
                                                        --cpu->r.C;
                                                }
                                                if (presenceTest(cpu, cpu->r.A)) {
                                                        cpu->r.C = cpu->r.A & MASKMEM;
                                                        cpu->r.L = 0;
                                                        // require fetch at SEQL
                                                        cpu->r.PROF = false;
                                                }
                                        }
                                }
                                return;
                        case 0x03: // X351/X751: CBD=non-zero field branch backward destructive
                                cpu->r.BROF = false;
                                // no break: fall through
                        case 0x01: // X151/X551: CBN=non-zero field branch backward nondestructive
                                if (t1) {
                                        if (OPERAND(cpu->r.A)) {
                                                // simple operand
                                                jumpSyllables(cpu, -(cpu->r.A & 0x0fff));
                                        } else {
                                                // descriptor
                                                if (cpu->r.L == 0) {
                                                        // adjust for Inhibit Fetch
                                                        --cpu->r.C;
                                                }
                                                if (presenceTest(cpu, cpu->r.A)) {
                                                        cpu->r.C = cpu->r.A & MASKMEM;
                                                        cpu->r.L = 0;
                                                        // require fetch at SEQL
                                                        cpu->r.PROF = false;
                                                }
                                        }
                                }
                        return;
                        }
                }
                return;

/***********************************************************************
* 0055: NOP=no operation
* gh55: DIA=Dial A
* set G and H registers
***********************************************************************/
        case 055:
                if (variant) {
                        cpu->r.G = variant >> 3;
                        cpu->r.H = variant & 7;
                }
                return;

/***********************************************************************
* 0061: XRT=Temporarily set full PRT addressing mode
* kv61: DIB=Dial B
* set K and V registers
***********************************************************************/
        case 061:
                if (variant) {
                        cpu->r.K = variant >> 3;
                        cpu->r.V = variant & 7;
                } else {
                        cpu->r.VARF = cpu->r.SALF;
                        cpu->r.SALF = false;
                }
                return;


/***********************************************************************
* cc65: TRB=Transfer Bits
* transfer 'cc' bits from A to B
* starting in A at position G/H
* starting in B at position K/V
* stopping when either variant(aka count) is exhausted
* or the last bit of either A or B is reached
* A is marked empty
* note: bit numbering is 1..48
***********************************************************************/
	case 065:
                adjustABFull(cpu);
                // do it the hard way.. BIT-WISE!
                t1 = MASK_FLAG >> (cpu->r.G*6 + cpu->r.H); // A register starting bit mask
                t2 = MASK_FLAG >> (cpu->r.K*6 + cpu->r.V); // B register starting bit mask
                // note: t1/t2 turn zero when the test bit is shifted out at the right
                while (variant && t1 && t2) {
                        if (cpu->r.A & t1)
                                cpu->r.B |= t2;
                        else
                                cpu->r.B &= ~t2;
                        --variant;
                        t1 >>= 1;
                        t2 >>= 1;
                        ++cpu->cycleCount; // approximate the shift counts
                }
                cpu->r.AROF = false;
                return;

/***********************************************************************
* cc71: FCL=Compare Field Low
* compare 'cc' bits from A with B
* starting in A at position G/H
* starting in B at position K/V
* stopping when either variant(aka count) is exhausted
* or the last bit of either A or B is reached
* or A > B is detected
* A is set to true or false
* note: bit numbering is 1..48
***********************************************************************/
        case 071:
                adjustABFull(cpu);
                // do it the hard way.. BIT-WISE!
                t1 = MASK_FLAG >> (cpu->r.G*6 + cpu->r.H); // A register starting bit mask
                t2 = MASK_FLAG >> (cpu->r.K*6 + cpu->r.V); // B register starting bit mask
                // note: t1/t2 turn zero when the test bit is shifted out at the right
                while (variant && t1 && t2) {
                        if (cpu->r.A & t1 && !(cpu->r.B & t2)) {
                                // A > B: we are done
                                cpu->r.A = true;
                                return;
                        }
                        --variant;
                        t1 >>= 1;
                        t2 >>= 1;
                        ++cpu->cycleCount; // approximate the shift counts
                }
                cpu->r.A = false;
                return;

/***********************************************************************
* cc75: FCE=Compare Field Equal
* compare 'cc' bits from A with B
* starting in A at position G/H
* starting in B at position K/V
* stopping when either variant(aka count) is exhausted
* or the last bit of either A or B is reached
* or A <> B is detected
* A is set to true or false
* note: bit numbering is 1..48
***********************************************************************/
        case 075:
                adjustABFull(cpu);
                // do it the hard way.. BIT-WISE!
                t1 = MASK_FLAG >> (cpu->r.G*6 + cpu->r.H); // A register starting bit mask
                t2 = MASK_FLAG >> (cpu->r.K*6 + cpu->r.V); // B register starting bit mask
                // note: t1/t2 turn zero when the test bit is shifted out at the right
                while (variant && t1 && t2) {
                        if (!(cpu->r.A & t1) != !(cpu->r.B & t2)) {
                                // A <> B: we are done
                                cpu->r.A = false;
                                return;
                        }
                        --variant;
                        t1 >>= 1;
                        t2 >>= 1;
                        ++cpu->cycleCount; // approximate the shift counts
                }
                cpu->r.A = true;
		return;

/***********************************************************************
* common finals for all branches
* branch_word_forward:
* branch_word_backward:
* descriptorbranch_adjust_c:
*   since C:L already points to the next syllable, backup C if that is
*   in the next word (L==0)
* descriptorbranch:
*   test for presence bit, cause IRQ in normal state if not present
*   set C:L to address in descriptor
*   mark A register empty
*   mark P register empty
***********************************************************************/
#if 0
	branch_syll_forward:
		if (OPERAND(cpu->r.A)) {
			// simple operand
			jumpSyllables(cpu, cpu->r.A & 0xfff);
			cpu->r.AROF = false;
			return;
		}
		goto descriptorbranch_adjust_c;

	branch_syll_backward:
		if (OPERAND(cpu->r.A)) {
			// simple operand
			jumpSyllables(cpu, -(cpu->r.A & 0xfff));
			cpu->r.AROF = false;
			return;
		}
		goto descriptorbranch_adjust_c;

	branch_word_forward:
		if (OPERAND(cpu->r.A)) {
			// simple operand
			if (cpu->r.L == 0) --cpu->r.C;
			jumpWords(cpu, cpu->r.A & MASK_ADDR10);
			cpu->r.AROF = false;
			return;
		}
		goto descriptorbranch_adjust_c;

	branch_word_backward:
		if (OPERAND(cpu->r.A)) {
			// simple operand
			if (cpu->r.L == 0) --cpu->r.C;
			jumpWords(cpu, -(cpu->r.A & MASK_ADDR10));
			cpu->r.AROF = false;
			return;
		}
		goto descriptorbranch_adjust_c;

	descriptorbranch_adjust_c:
		// TODO: adjust before presence test?
		if (cpu->r.L == 0) --cpu->r.C;
		if (presenceTest(cpu, cpu->r.A)) {
		        cpu->r.C = cpu->r.A & MASKMEM;
		        cpu->r.L = 0;
		        // require fetch at SECL
		        cpu->r.PROF = false;
		        cpu->r.AROF = false;
		}
		return;
#endif

/***********************************************************************
* inofficially, all unused opcodes are NOP
* due to partial decoding, that may not always be true
* for the time being we consider all unused opcodes a fatal error
***********************************************************************/
	default:
	unused:
		prepMessage(cpu);
		printf("opcode %04o encoundered (word mode)\n", opcode);
		stop(cpu);
		return;

        } // end switch for non-LITC/OPDC/DESC operators
}


