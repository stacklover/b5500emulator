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
* 2017-10-28  R.Meyer
*   adaption to new CPU structure
***********************************************************************/

#include <stdio.h>
#include "common.h"

#define DEBUG_TRB false
#define DEBUG_FCE false
#define DEBUG_FCL false

#define	CONTROL_STATE_ONLY if (cpu->bNCSF) return
#define	NORMAL_STATE_ONLY if (!cpu->bNCSF) return

/***********************************************************************
* execute one word mode instruction
***********************************************************************/
void b5500_execute_wm(CPU *cpu)
{
        WORD12 opcode = cpu->rT;
        WORD12 variant;
        WORD48 t1, t2;

        // clear all Q flags, Y, Z, M, N, X registers for each word mode instruction
        cpu->bQ01F = false; cpu->bQ02F = false; cpu->bQ03F = false;
        cpu->bQ04F = false; cpu->bQ05F = false; cpu->bQ06F = false;
        cpu->bQ07F = false; cpu->bQ08F = false; cpu->bQ09F = false;
        cpu->rY = 0; cpu->rZ = 0; cpu->rM = 0; cpu->rN = 0;
        cpu->rX = 0;

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
                cpu->rA = opcode >> 2;
                cpu->bAROF = true;
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
		cpu->bSALF |= cpu->bVARF;
		cpu->bVARF = false;
operandcall:	// entry fŕom COC
		if (DESCRIPTOR(cpu->rA)) {
			if ((cpu->rA & MASK_CODE) != 0 && (cpu->rA & MASK_XBIT) == 0) {
				// control word, just leave it on stack
				return;
			}
			if (presenceTest(cpu, cpu->rA)) {
				// present descriptor
				if ((cpu->rA & MASK_CODE) != 0 && (cpu->rA & MASK_XBIT) != 0) {
					// program descriptor
					enterSubroutine(cpu, false);
					return;
				}
				// data descriptor
				if (indexDescriptor(cpu)) {
					// indexing failed, IRQ already caused
					return;
				}
				// get value
				loadAviaM(cpu); // A = [M]
				if (DESCRIPTOR(cpu->rA) && cpu->bNCSF) {
					// Flag bit is set and NORMAL state
				        causeSyllableIrq(cpu, IRQ_FLAG, "OPDC FLAG SET");
				}
				
			}
			// not present, IRQ already caused
			return;
		}
		// simple operand, just leave it in A
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
		cpu->bSALF |= cpu->bVARF;
		cpu->bVARF = false;
descriptorcall:	// entry fŕom CDC
		if (DESCRIPTOR(cpu->rA)) {
			if ((cpu->rA & MASK_CODE) != 0 && (cpu->rA & MASK_XBIT) == 0) {
				// control word, just leave its address present on stack
				cpu->rA = MASK_FLAG | MASK_PBIT | cpu->rM;
				return;
			}
			if (presenceTest(cpu, cpu->rA)) {
				// present descriptor
				if ((cpu->rA & MASK_CODE) != 0 && (cpu->rA & MASK_XBIT) != 0) {
					// program descriptor
					enterSubroutine(cpu, true);
					return;
				}
				// Data descriptor
				if (indexDescriptor(cpu))
					return;
				cpu->rA |= MASK_FLAG | MASK_PBIT;
			}
			return;
		}
		cpu->rA = MASK_FLAG | MASK_PBIT | cpu->rM;
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
                        if (OPERAND(cpu->rA)) {
                                // operand
                                computeRelativeAddr(cpu, cpu->rA, false);
                        } else if (presenceTest(cpu, cpu->rA)) {
                                // present descriptor
                                cpu->rM = cpu->rA & MASKMEM;
                        } else {
				// not present
				// leave address on stack and exit
				return;
                        }
			// fetch the value
			loadAviaM(cpu);
			if (cpu->bNCSF) {
				// normal state
				// test continuity bit, [20:1]
				if (cpu->rA & MASK_CONT) {
					causeSyllableIrq(cpu, IRQ_CONT, "PRL-CONT");
				} else {
					causeSyllableIrq(cpu, IRQ_PREL, "PRL-PRL");
				}
				// store IOD address in PRT[9]
				cpu->rA = cpu->rM;
				cpu->rM = (cpu->rR/*TODO SHIFT*/<<RSHIFT) + RR_COM;
				storeAviaM(cpu);
			} else {
				// control state
				// just clear presence bit and store back
				cpu->rA &= ~MASK_PBIT;
				storeAviaM(cpu);
			}
			// remove address
			cpu->bAROF = false;
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
			cpu->rM = CC->IAR;
			if (cpu->rM) {
				cpu->bPROF = false; // require fetch at SECL
				cpu->rC = cpu->rM;
				cpu->rL = 0;
				cpu->rS = AA_IRQSTACK; // stack address @100
				clearInterrupt(cpu->rM); // clear IRQ
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
                        cpu->rA = readTimer(cpu);
                        cpu->bAROF = true;
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
                        cpu->rM = (cpu->rR/*TODO SHIFT*/<<RSHIFT) + RR_COM;
                        if (cpu->bAROF) { // TOS in A
                                storeAviaM(cpu);
                                cpu->bAROF = false;
                        } else if (cpu->bBROF) { // TOS in B
                                storeBviaM(cpu);
                                cpu->bBROF = false;
                        } else { // load TOS to B
                                adjustBFull(cpu);
                                storeBviaM(cpu);
                                cpu->bBROF = false;
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
			if (OPERAND(cpu->rA)) {
				// it's an operand
				computeRelativeAddr(cpu, cpu->rA, 0);
			} else if (PRESENT(cpu->rA)) {
				cpu->rM = cpu->rA & MASKMEM;
				// present descriptor
			} else {
				// leave address on stack and exit
				return;
			}
			// set the presence bit of the word at the designed address
			loadAviaM(cpu);
			cpu->rA |= MASK_PBIT;
			storeAviaM(cpu);
			// remove address
			cpu->bAROF = false;
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
			if (cpu->bUS14X)
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
			cpu->rM = AA_IODESC;
			if (cpu->bAROF) {
				storeAviaM(cpu);
				cpu->bAROF = false;
			} else if (cpu->bBROF) {
				storeBviaM(cpu);
				cpu->bBROF = false;
			} else {
				adjustAFull(cpu);
				storeAviaM(cpu);
				cpu->bAROF = false;
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
                        cpu->rM = AA_IODESC;
                        if (cpu->bAROF) {
                                storeAviaM(cpu);
                                cpu->bAROF = false;
                        } else if (cpu->bBROF) {
                                storeBviaM(cpu);
                                cpu->bBROF = false;
                        } else {
                                adjustAFull(cpu);
                                storeAviaM(cpu);
                                // [M] = A
                                cpu->bAROF = false;
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
			cpu->rA ^= MASK_NUMBER; return;
                case 002: adjustABFull(cpu);
			cpu->rA = (cpu->rA & MASK_NUMBER) | cpu->rB;
			cpu->bBROF = false; return;
                case 004: adjustABFull(cpu);
			cpu->rA = (cpu->rA | MASK_FLAG) & cpu->rB;
			cpu->bBROF = false; return;
                case 010: adjustABFull(cpu);
                        cpu->rB ^= (~cpu->rA) & MASK_NUMBER;
                        cpu->bAROF = false; return;
                case 020: adjustAFull(cpu);
                        cpu->rA &= MASK_NUMBER; return;
                case 040: adjustAFull(cpu);
                        cpu->rA |= MASK_FLAG; return;
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
                        if (OPERAND(cpu->rA)) {
                                // operand
                                computeRelativeAddr(cpu, cpu->rA, false);
                        } else if (presenceTest(cpu, cpu->rA)) {
				// present descriptor
				cpu->rM = cpu->rA & MASKMEM;
			} else {
				// not present
				// leave address and value on stack and exit
				return;
			}
			// now store
                        storeBviaM(cpu);
			// remove address
                        cpu->bAROF = false;
			// if destructive, remove value
			if (variant == 004)
				cpu->bBROF = false;
                        return;

/***********************************************************************
* 2021: LOD=Load operand
* ensure A full
* load A from address designated by A
***********************************************************************/
                case 020:
                        adjustAFull(cpu);
                        if (OPERAND(cpu->rA)) {
                                // operand
                                computeRelativeAddr(cpu, cpu->rA, true);
                        } else if (presenceTest(cpu, cpu->rA)) {
                                // present descriptor
                                cpu->rM = cpu->rA & MASKMEM;
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
		case 001: cpu->rB = (singlePrecisionCompare(cpu) >= 0) ? true : false; return;
		case 002: cpu->rB = (singlePrecisionCompare(cpu) > 0) ? true : false; return;
		case 004: cpu->rB = (singlePrecisionCompare(cpu) != 0) ? true : false; return;
		case 041: cpu->rB = (singlePrecisionCompare(cpu) <= 0) ? true : false; return;
		case 042: cpu->rB = (singlePrecisionCompare(cpu) < 0) ? true : false; return;
		case 044: cpu->rB = (singlePrecisionCompare(cpu) == 0) ? true : false; return;

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
			if (cpu->bAROF) {
				adjustBEmpty(cpu);
				cpu->rB = cpu->rA;
				cpu->bBROF = true;
			} else {
				adjustBFull(cpu);
				cpu->rA = cpu->rB;
				cpu->bAROF = true;
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
			cpu->rB = (cpu->rB & ~MASK_CREG) | ((cpu->rA & MASK_FREG) >> (SHFT_FREG - SHFT_CREG));
			cpu->bAROF = false; return;
		case 034: adjustABFull(cpu);
			cpu->rB = (cpu->rB & ~MASK_FREG) | (cpu->rA & MASK_FREG);
			cpu->bAROF = false; return;
		case 054: adjustABFull(cpu);
			cpu->rB = (cpu->rB & ~MASK_CREG) | (cpu->rA & MASK_CREG);
			cpu->bAROF = false; return;
		case 074: adjustABFull(cpu);
			cpu->rB = (cpu->rB & ~MASK_FREG) | ((cpu->rA & MASK_CREG) << (SHFT_FREG - SHFT_CREG));
			cpu->bAROF = false; return;

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
* 2131: LBC=branch backward word conditional
* 2231: LFC=branch forward word conditional
***********************************************************************/
                case 001:
                case 002:
                case 021:
                case 022:
                        adjustABFull(cpu);	// condition in B, destination in A
                        cpu->bBROF = false;	// condition used
                        if (cpu->rB & 1) {
                                // true => no branch
                                cpu->bAROF = false;
                                return;
                        }
			goto common_branch;

/***********************************************************************
* 4131: BBW=branch backward unconditional
* 4231: BFW=branch forward unconditional
* 6131: LBU=branch backward word unconditional
* 6231: LFU=branch forward word unconditional
***********************************************************************/
                case 041:
                case 042:
                case 061:
                case 062:
                        adjustAFull(cpu);
		common_branch:
			if (DESCRIPTOR(cpu->rA)) {
				// descriptor
                                if (presenceTest(cpu, cpu->rA)) {
					// present descriptor contains absolute address!
                                        cpu->rC = cpu->rA & MASKMEM;
                                        cpu->rL = 0;
                                        // require fetch at SECL
                                        cpu->bPROF = false;
                                        cpu->bAROF = false;
					return;
				}
				// absent descriptor. IRQ caused by presenceTest
				// backup to branch word. syllable not changed. TODO: why??
                                if (cpu->rL == 0)
                                        --cpu->rC;
				// set BROF for word branches. TODO: why?
				// more logical is to set it for conditional branches!
                                if ((variant & 040) == 0)
					cpu->bBROF = true;
				return;
                        }
			// operand
			// back up to branch op
			if (cpu->rL == 0) {
				cpu->rL = 3;
				--cpu->rC;
				cpu->bPROF = false;
			} else {
				--cpu->rL;
			}
			// Follow logic based on real hardware
			if ((variant & 020) == 0) {
				// Syllable branch
				if (variant & 002) {
					// Forward
					if (cpu->rA & 1) {
						// N = 0
						cpu->rL++;
						cpu->rC += (cpu->rL >> 2);
						cpu->rL &= 3;
					}
					cpu->rA >>= 1;
					if (cpu->rA & 1) {
						cpu->rL+=2;
						cpu->rC += (cpu->rL >> 2);
						cpu->rL &= 3;
					}
					cpu->rA >>= 1;
				} else {
					// Backward
					if (cpu->rA & 1) {
						// N = 0
						if (cpu->rL == 0) {
							cpu->rC--;
							cpu->rL = 3;
						} else {
							cpu->rL--;
						}
					}
					cpu->rA >>= 1;
					if (cpu->rA & 1) {
						// N = 1
						if (cpu->rL < 2) {
							cpu->rC--;
							cpu->rL += 2;
						} else {
							cpu->rL -= 2;
						}
					}
					cpu->rA >>= 1;
				}
				// Fix up for backward step
				if (cpu->rL == 3) {
					// N = 3
					cpu->rC++;
					cpu->rL = 0;
				} else {
					cpu->rL++;
				}
			} else {
				cpu->rL = 0;
			}
			if (variant & 02) {
				// Forward
				cpu->rC += cpu->rA & 01777;
			} else {
				// Backward
				cpu->rC -= cpu->rA & 01777;
			}
			// now transfer
			cpu->rC &= MASK_ADDR15;
			cpu->bAROF = 0;
			cpu->bPROF = 0;
			return;

/***********************************************************************
* 0431: SSN=set sign bit (set negative)
* 1031: CHS=change sign bit
* 4431: SSP=reset sign bit (set positive)
***********************************************************************/
                case 004: adjustAFull(cpu); cpu->rA |= MASK_SIGNMANT; return;
                case 010: adjustAFull(cpu); cpu->rA ^= MASK_SIGNMANT; return;
                case 044: adjustAFull(cpu); cpu->rA &= ~MASK_SIGNMANT; return;

/***********************************************************************
* 2031: TOP=test flag bit (test for operand)
* test flag bit of TOS and add result to stack!
***********************************************************************/
                case 020: adjustAEmpty(cpu); adjustBFull(cpu);
                        cpu->rA = OPERAND(cpu->rB) ? true : false;
                        cpu->bAROF = true; return;

/***********************************************************************
* 2431: TUS=interrogate peripheral status
***********************************************************************/
                case 024: adjustAEmpty(cpu);
                        cpu->rA = interrogateUnitStatus(cpu);
                        cpu->bAROF = true; return;

/***********************************************************************
* 6431: TIO=interrogate I/O channel
***********************************************************************/
                case 064: adjustAEmpty(cpu);
                        cpu->rA = interrogateIOChannel(cpu);
                        cpu->bAROF = true; return;

/***********************************************************************
* 7031: FBS=stack search for flag
***********************************************************************/
                case 070: adjustAFull(cpu);
                        cpu->rM = cpu->rA & MASKMEM;
                        loadAviaM(cpu);
                        while (OPERAND(cpu->rA)) {
                                cpu->rM = (cpu->rM+1) & MASKMEM;
                                loadAviaM(cpu);
                        }
                        // flag bit found: stop the search
                        cpu->rA = INIT_DD | MASK_PBIT | cpu->rM;
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
                        if (!cpu->bBROF) {
                                cpu->bQ03F = true;
                                // Q03F: not used, except for display purposes
                                adjustBFull(cpu);
                        }
                        if (presenceTest(cpu, cpu->rB)) {
                                cpu->rS = (cpu->rB >> SHFT_FREG) & MASKMEM;
                                cpu->rC = cpu->rB & MASKMEM;
                                cpu->rL = 0;
                                // require fetch at SECL
                                cpu->bPROF = false;
                                loadBviaS(cpu);
                                // B = [S], fetch MSCW
                                --cpu->rS;
                                applyMSCW(cpu, cpu->rB);
                                cpu->bBROF = false;
                        }
                        return;

/***********************************************************************
* 0235: RTN=return normal
* 1235: RTS=return special
* If A is an operand or a present descriptor,
* proceed with the return,
* otherwise throw a p-bit interrupt
* 0435: XIT=exit procedure
***********************************************************************/
                case 002:
                case 012:
			adjustAFull(cpu);
			if (DESCRIPTOR(cpu->rA) && !presenceTest(cpu, cpu->rA)) {
				return;
			}
			// fall through
                case 004:
			if (variant & 04) // XIT only
				cpu->bAROF = 0;
			cpu->bBROF = 0;
			cpu->bPROF = 0;
			if ((variant & 010) == 0) // RTN and XIT only
				cpu->rS = cpu->rF;	// reset stack to last RCW
			loadBviaS(cpu);		// get RCW
			if (OPERAND(cpu->rB)) {
				// OOPS, not a control word
				if (cpu->bNCSF)
					causeSyllableIrq(cpu, IRQ_FLAG, "RTN/RTS/XIT RCW FLAG RESET");
				return;
			}
			// set registers from RCW
			t1 = applyRCW(cpu, cpu->rB, false, false); /* Restore registers */
			cpu->rS = cpu->rF;	// reset stack to MSCW
			cpu->bBROF = 0;
			loadBviaS(cpu);		// get MSCW
			cpu->rS--; // TODO check for wrap
			// set registers from MSCW
			applyMSCW(cpu, cpu->rB);
			// TODO: whats this for?
			if (cpu->bMSFF && cpu->bSALF) {
				cpu->rM = cpu->rF;
				do {
					/* B = [M], M = B[F-FIELD]; */
					loadMviaM(cpu);	/* Grab previous MCSW */
				} while(cpu->rB & MASK_SALF);
				cpu->rM = (cpu->rR/*TODO SHIFT*/ << RSHIFT) | RR_MSCW;
				storeBviaS(cpu);
			}
			cpu->bBROF = 0;
			if (variant & 02) {	/* RTS and RTN */
				if (t1)
					goto descriptorcall;
				else
					goto operandcall;
			}
                        return;

		default: goto unused;
                }
                return;

/***********************************************************************
* XX41: index, mark stack, etc.
* ?
***********************************************************************/
        case 041:
                switch (variant) {

/***********************************************************************
* 0141: INX=index
* ?
***********************************************************************/
                case 001:
                        adjustABFull(cpu);
                        cpu->rM = (cpu->rA + cpu->rB) & MASKMEM;
                        cpu->rA = (cpu->rA & ~MASKMEM) | cpu->rM;
                        cpu->bBROF = false;
                        return;


/***********************************************************************
* 0241: COC=construct operand call
* 1241: CDC=construct descriptor call
***********************************************************************/
                case 002:
                        adjustABFull(cpu);
			t1 = cpu->rA;
			cpu->rA = cpu->rB | MASK_FLAG;
			cpu->rB = t1;
			if (variant & 010)
				goto descriptorcall;
			else
				goto operandcall;

/***********************************************************************
* 0441: MKS=mark stack
* ?
***********************************************************************/
                case 004:
                        adjustABEmpty(cpu);
                        cpu->rB = buildMSCW(cpu);
                        cpu->bBROF = true;
                        adjustBEmpty(cpu);
                        cpu->rF = cpu->rS;
                        if (!cpu->bMSFF) {
                                if (cpu->bSALF) {
                                        // store the MSCW at R+7
                                        cpu->rM = (cpu->rR/*TODO SHIFT*/<<6) + RR_MSCW;
                                        storeBviaM(cpu);
                                }
                                cpu->bMSFF = true;
                        }
                        return;

/***********************************************************************
* 2141: SSF=F & S register set/store
* ?
***********************************************************************/
                case 021:
                        adjustABFull(cpu);
                        switch (cpu->rA & 3) {
                        case 0: // store F into B.[18:15]
                                cpu->rB = (cpu->rB & ~MASK_FREG) | (cpu->rF << SHFT_FREG);
                                break;
                        case 1: // store S into B.[33:15]
                                cpu->rB = (cpu->rB & ~MASK_CREG) | (cpu->rS << SHFT_CREG);
                                break;
                        case 2: // set F from B.[18:15]
                                cpu->rF = (cpu->rB & MASK_FREG) >> SHFT_FREG;
                                cpu->bSALF = true;
                                cpu->bBROF = false;
                                break;
                        case 3: // set S from B.[33:15]
                                cpu->rS = (cpu->rB & MASK_CREG) >> SHFT_CREG;
                                cpu->bBROF = false;
                                break;
                        }
                        cpu->bAROF = false;
                        return;

/***********************************************************************
* 2541: LLL=link list look-up
* ?
***********************************************************************/
                case 025:
                        adjustABFull(cpu);
                        // get test field
                        t1 = cpu->rA & MASK_MANTISSA;
                        // test value
                        cpu->rM = cpu->rB & MASKMEM;
                        // starting link address
                        do {
                                cpu->cycleCount += 2;
                                // approximate the timing
                                loadBviaM(cpu);
                                t2 = cpu->rB & MASK_MANTISSA;
                                if (t2 < t1) {
                                        cpu->rM = t2 & MASKMEM;
                                } else {
                                        cpu->rA = INIT_DD | MASK_PBIT | cpu->rM;
                                        break;
                                        // B >= A: stop look-up
                                }
                        } while (true);
                        return;

/***********************************************************************
* 4441: CMN=enter character mode inline
* ?
***********************************************************************/
                case 044:
                        enterCharModeInline(cpu);
                        return;

		default: goto unused;
                }
                return;

/***********************************************************************
* XX45: ISO=Variable Field Isolate op
* ?
***********************************************************************/
        case 045:
                adjustAFull(cpu);
                t2 = variant >> 3; // number of whole chars
                if (t2) {
                        t1 = cpu->rGH/*TODO G*/*6 + cpu->rGH/*TODO H*/; // starting source bit position
                        t2 = t2*6 - (variant & 7) - cpu->rGH/*TODO H*/; // number of bits
                        if (t1+t2 <= 48) {
                                cpu->rA = fieldIsolate(cpu->rA, t1, t2);
                        } else {
                                // handle wrap-around in the source value
                                cpu->rA = fieldInsert(
                                        fieldIsolate(cpu->rA, 0, t2-48+t1),
                                        48-t2, 48-t1,
                                        fieldIsolate(cpu->rA, t1, 48-t1));
                        }
                        // approximate the shift cycle counts
                        cpu->cycleCount += (variant >> 3) + (variant & 7) + cpu->rGH/*TODO G*/ + cpu->rGH/*TODO H*/;
                        cpu->rGH/*TODO G*/ = (cpu->rGH/*TODO G*/ + (variant >> 3)) & 7;
                        cpu->rGH/*TODO H*/ = 0;
                }
                return;

/***********************************************************************
* XX51: delete & conditional branch ops
* 0051:      DEL=delete TOS (or field branch with zero-length field)
* X051/X451: CFN=non-zero field branch forward nondestructive
* X151/X551: CBN=non-zero field branch backward nondestructive
* X251/X651: CFD=non-zero field branch forward destructive
* X351/X751: CBD=non-zero field branch backward destructive
***********************************************************************/
        case 051:
		// get field length (1-15 bits)
		t2 = variant >> 2;
                if (t2 == 0) {
			// field length 0 means false, so just delete word on TOS
                        if (cpu->bAROF) {
                                cpu->bAROF = false;
                        } else if (cpu->bBROF) {
                                cpu->bBROF = false;
                        } else {
                                --cpu->rS;
                        }
			return;
                }
		// non-zero field
		adjustABFull(cpu);
		// isolate the number of bits to test
		t1 = fieldIsolate(cpu->rB, cpu->rGH/*TODO G*/*6+cpu->rGH/*TODO H*/, t2);
		// approximate the shift counts
		cpu->cycleCount += cpu->rGH/*TODO G*/ + cpu->rGH/*TODO H*/ + (t2 >> 1);
		// A is unconditionally empty at end
		cpu->bAROF = false;
		// destructive branch?
		if (variant & 002)
			cpu->bBROF = false;
		// test the field
		if (t1) {
			// continue at other branch code
			if (variant & 001) {
				// branch backwards
				variant = 041;
			} else {
				// branch forwards
				variant = 042;
			}
			goto common_branch;
		}
		// branch not taken, remove destination
		cpu->bAROF = false;
		return;

/***********************************************************************
* 0055: NOP=no operation
* gh55: DIA=Dial A - set G and H registers
***********************************************************************/
        case 055:
                if (variant) {
                        cpu->rGH/*TODO G*/ = variant >> 3;
                        cpu->rGH/*TODO H*/ = variant & 7;
                }
                return;

/***********************************************************************
* 0061: XRT=Temporarily set full PRT addressing mode
* kv61: DIB=Dial B - set K and V registers
***********************************************************************/
        case 061:
                if (variant) {
                        cpu->rKV/*TODO K*/ = variant >> 3;
                        cpu->rKV/*TODO V*/ = variant & 7;
                } else {
                        cpu->bVARF = cpu->bSALF;
                        cpu->bSALF = false;
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
                t1 = MASK_FLAG >> (cpu->rGH/*TODO G*/*6 + cpu->rGH/*TODO H*/); // A register starting bit mask
                t2 = MASK_FLAG >> (cpu->rKV/*TODO K*/*6 + cpu->rKV/*TODO V*/); // B register starting bit mask
                // note: t1/t2 turn zero when the test bit is shifted out at the right
                while (variant && t1 && t2) {
                        if (cpu->rA & t1)
                                cpu->rB |= t2;
                        else
                                cpu->rB &= ~t2;
                        --variant;
                        t1 >>= 1;
                        t2 >>= 1;
                        ++cpu->cycleCount; // approximate the shift counts
                }
                cpu->bAROF = false;
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
                t1 = MASK_FLAG >> (cpu->rGH/*TODO G*/*6 + cpu->rGH/*TODO H*/); // A register starting bit mask
                t2 = MASK_FLAG >> (cpu->rKV/*TODO K*/*6 + cpu->rKV/*TODO V*/); // B register starting bit mask
                // note: t1/t2 turn zero when the test bit is shifted out at the right
                while (variant && t1 && t2) {
                        if (cpu->rA & t1 && !(cpu->rB & t2)) {
                                // A > B: we are done
                                cpu->rA = true;
                                return;
                        }
                        --variant;
                        t1 >>= 1;
                        t2 >>= 1;
                        ++cpu->cycleCount; // approximate the shift counts
                }
                cpu->rA = false;
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
                t1 = MASK_FLAG >> (cpu->rGH/*TODO G*/*6 + cpu->rGH/*TODO H*/); // A register starting bit mask
                t2 = MASK_FLAG >> (cpu->rKV/*TODO K*/*6 + cpu->rKV/*TODO V*/); // B register starting bit mask
                // note: t1/t2 turn zero when the test bit is shifted out at the right
                while (variant && t1 && t2) {
                        if (!(cpu->rA & t1) != !(cpu->rB & t2)) {
                                // A <> B: we are done
                                cpu->rA = false;
                                return;
                        }
                        --variant;
                        t1 >>= 1;
                        t2 >>= 1;
                        ++cpu->cycleCount; // approximate the shift counts
                }
                cpu->rA = true;
		return;

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


