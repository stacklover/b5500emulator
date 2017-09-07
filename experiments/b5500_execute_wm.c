/***********************************************************************
* b5500emulator
************************************************************************
* Copyright (c)	2016, Reinhard Meyer, DL5UY
* Licensed under the MIT License,
*	see LICENSE
* based	on (C) work by Nigel Williams and Paul Kimpel
* see: https://github.com/pkimpel/retro-b5500
************************************************************************
* execute one word mode	instruction
************************************************************************
* 2016-02-19  R.Meyer
*   Converted Paul's work from Javascript to C
* 2017-07-17  R.Meyer
*   changed "this" to "cpu" to avoid errors when using g++
***********************************************************************/

#include "b5500_common.h"

/***********************************************************
*  Word	Mode Syllables					   *
***********************************************************/
void b5500_execute_wm(CPU *cpu)
{
	WORD12 opcode =	cpu->r.T;
	WORD12 variant;
	WORD48 t1, t2;

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
	cpu->r.M = 0;
	cpu->r.N = 0;
	cpu->r.X = 0;

	// last	2 bits of opcode
	switch (opcode & 3) {
	case 0:	// LITC: Literal Call
		adjustAEmpty(cpu);
		cpu->r.A = opcode >> 2;
		cpu->r.AROF = true;
		break;

	case 2:	// OPDC: Operand Call
		adjustAEmpty(cpu);
		computeRelativeAddr(cpu, opcode	>> 2, true);
		loadAviaM(cpu);
		if (DESCRIPTOR(cpu->r.A)) {
			// if it's a control word, evaluate it
			operandCall(cpu);
		}
		// otherwise, just leave it in A
		break;

	case 3:	// DESC: Descriptor (name) Call
		adjustAEmpty(cpu);
		computeRelativeAddr(cpu, opcode	>> 2, true);
		loadAviaM(cpu);
		descriptorCall(cpu);
		break;

	case 1:	// all other word-mode operators
		variant	= opcode >> 6;
		switch (opcode & 077) {
		case 001: // XX01: single-precision numerics
			switch (variant) {
			case 001: // 0101: ADD=single-precision	add
				singlePrecisionAdd(cpu,	true);
				break;
			case 003: // 0301: SUB=single-precision	subtract
				singlePrecisionAdd(cpu,	false);
				break;
			case 004: // 0401: MUL=single-precision	multiply
				singlePrecisionMultiply(cpu);
				break;
			case 010: // 1001: DIV=single-precision	floating divide
				singlePrecisionDivide(cpu);
				break;
			case 030: // 3001: IDV=integer divide
				integerDivide(cpu);
				break;
			case 070: // 7001: RDV=remainder divide
				remainderDivide(cpu);
				break;
			}
			break;
		case 005: // XX05: double-precision numerics
			switch (variant) {
			case 001: // 0105: DLA=double-precision	add
				doublePrecisionAdd(cpu,	true);
				break;
			case 003: // 0305: DLS=double-precision	subtract
				doublePrecisionAdd(cpu,	false);
				break;
			case 004: // 0405: DLM=double-precision	multiply
				doublePrecisionMultiply(cpu);
				break;
			case 010: // 1005: DLD=double-precision	floating divide
				doublePrecisionDivide(cpu);
				break;
			}
			break;
		case 011: // XX11: Control State and communication ops
			switch (variant) {
			case 001: // 0111: PRL=Program Release
				// TOS should be operand or descriptor
				// t1 =	copy of	A
				// t2 =	presence bit or	value valid
				// get it into A and copy into t1
				adjustAFull(cpu);
				t1 = cpu->r.A;
				if (OPERAND(t1)) {
					// it's	an operand
					computeRelativeAddr(cpu, t1, false);
					t2 = true;
				} else if (presenceTest(cpu, t1)) {
					// present descriptor
					cpu->r.M = t1 &	MASKMEM;
					t2 = true;
				} else {
					// absent descriptor
					t2 = false;
				}
				if (t2)	{
					// fetch IO Descriptor
					loadAviaM(cpu);
					if (cpu->r.NCSF) {
						// not in control state
						// test	continuity bit,	[20:1]
						if (cpu->r.A & MASK_DDCONT) {
							// set I07/6: continuity bit
							cpu->r.I = (cpu->r.I & IRQ_MASKL) | IRQ_CONT;
						} else {
							// set I07/5: program release
							cpu->r.I = (cpu->r.I & IRQ_MASKL) | IRQ_PREL;
						}
						signalInterrupt(cpu);
						cpu->r.A = cpu->r.M;
						// store IOD address in	PRT[9]
						cpu->r.M = (cpu->r.R<<6) + 9;
						storeAviaM(cpu);
					} else {
						// in control state
						// clear presence bit
						cpu->r.A &= ~MASK_PBIT;
						storeAviaM(cpu);
					}
					cpu->r.AROF = false;
				}
				break;
			case 002: // 0211: ITI=Interrogate Interrupt
				// control-state only
				if (CC->IAR && !cpu->r.NCSF) {
					cpu->r.C = CC->IAR;
					cpu->r.L = 0;
					// stack address @100
					cpu->r.S = AA_IRQSTACK;
					clearInterrupt(cpu);
					// require fetch at SECL
					cpu->r.PROF = false;
				}
				break;
			case 004: // 0411: RTR=Read Timer
				// control-state only
				if (!cpu->r.NCSF) {
					adjustAEmpty(cpu);
					cpu->r.A = readTimer(cpu);
					cpu->r.AROF = true;
				}
				break;
			case 010: // 1011: COM=Communicate
				// no-op in Control State
				if (cpu->r.NCSF) {
					// address = R+@11
					cpu->r.M = cpu->r.R*64 + RR_COM;
					if (cpu->r.AROF) {
						storeAviaM(cpu);
						// [M] = A
						cpu->r.AROF = false;
					} else if (cpu->r.BROF)	{
						storeBviaM(cpu);
						// [M] = B
						cpu->r.BROF = false;
					} else {
						adjustBFull(cpu);
						storeBviaM(cpu);
						// [M] = B
						cpu->r.BROF = false;
					}
					// set I07: communicate
					cpu->r.I = (cpu->r.I & IRQ_MASKL) | IRQ_COM;
					signalInterrupt(cpu);
				}
				break;
			case 021: // 2111: IOR=I/O Release
				// no-op in Normal State
				if (!cpu->r.NCSF) {
					adjustAFull(cpu);
					t1 = cpu->r.A;
					if (OPERAND(t1)) {
						// it's	an operand
						computeRelativeAddr(cpu, t1, 0);
						t2 = true;
					} else if (PRESENT(t1))	{
						cpu->r.M = t1 &	MASKMEM;
						// present descriptor
						t2 = true;
					} else {
						// for an absent descriptor, just leave	it on the stack
						t2 = false;
					}
					if (t2)	{
						loadAviaM(cpu);
						cpu->r.A |= MASK_PBIT;
						storeAviaM(cpu);
						cpu->r.AROF = false;
					}
				}
				break;
			case 022: // 2211: HP2=Halt Processor 2
				// control-state only
				if (!(cpu->r.NCSF || CC->HP2F))	{
					haltP2(cpu);
				}
				break;
			case 024: // 2411: ZPI=Conditional Halt
				if (cpu->r.US14X) {
					// STOP	OPERATOR switch	on
					stop(cpu);
				}
				break;
			case 030: // 3011: SFI=Store for Interrupt
				storeForInterrupt(cpu, false, false);
				break;
			case 034: // 3411: SFT=Store for Test
				storeForInterrupt(cpu, false, true);
				break;
			case 041: // 4111: IP1=Initiate	Processor 1
				// control-state only
				if (!cpu->r.NCSF) {
					initiate(cpu, false);
				}
				break;
			case 042: // 4211: IP2=Initiate	Processor 2
				// control-state only
				if (!cpu->r.NCSF) {
					// INCW	is stored in @10
					cpu->r.M = AA_IODESC;
					if (cpu->r.AROF) {
						storeAviaM(cpu);
						// [M] = A
						cpu->r.AROF = false;
					} else if (cpu->r.BROF)	{
						storeBviaM(cpu);
						// [M] = B
						cpu->r.BROF = false;
					} else {
						adjustAFull(cpu);
						storeAviaM(cpu);
						// [M] = A
						cpu->r.AROF = false;
					}
					initiateP2(cpu);
				}
				break;
			case 044: // 4411: IIO=Initiate	I/O
				if (!cpu->r.NCSF) {
					// address of IOD is stored in @10
					cpu->r.M = AA_IODESC;
					if (cpu->r.AROF) {
						storeAviaM(cpu);
						// [M] = A
						cpu->r.AROF = false;
					} else if (cpu->r.BROF)	{
						storeBviaM(cpu);	      // [M] = B
						cpu->r.BROF = false;
					} else {
						adjustAFull(cpu);
						storeAviaM(cpu);
						// [M] = A
						cpu->r.AROF = false;
					}
					// let CentralControl choose the I/O Unit
					initiateIO(cpu);
				}
				break;
			case 051: // 5111: IFT=Initiate	For Test
				if (!cpu->r.NCSF) {
					// control-state only
					initiate(cpu, 1);
				}
				break;
			} // end switch	for XX11 ops
			break;
		case 015: // XX15: logical (bitmask) ops
			switch (variant) {
			case 001: // 0115: LNG=logical negate
				adjustAFull(cpu);
				cpu->r.A ^= MASK_NUMBER;
				break;
			case 002: // 0215: LOR=logical OR
				adjustABFull(cpu);
				cpu->r.A = (cpu->r.A & MASK_NUMBER) | cpu->r.B;
				cpu->r.BROF = false;
				break;
			case 004: // 0415: LND=logical AND
				adjustABFull(cpu);
				cpu->r.A = (cpu->r.A | MASK_FLAG) & cpu->r.B;
				cpu->r.BROF = false;
				break;
			case 010: // 1015: LQV=logical EQV
				adjustABFull(cpu);
				cpu->r.B ^= (~cpu->r.A)	& MASK_NUMBER;
				cpu->r.AROF = false;
				break;

			case 020: // 2015: MOP=reset flag bit (make operand)
				adjustAFull(cpu);
				cpu->r.A &= MASK_NUMBER;
				break;
			case 040: // 4015: MDS=set flag	bit (make descriptor)
				adjustAFull(cpu);
				cpu->r.A |= MASK_FLAG; // set [0:1]
				break;
			}
			break;
		case 021: // XX21: load	& store	ops
			switch (variant) {
			case 001: // 0121: CID=Conditional integer store destructive
				integerStore(cpu, true,	true);
				break;
			case 002: // 0221: CIN=Conditional integer store nondestructive
				integerStore(cpu, true,	false);
				break;
			case 004: // 0421: STD=Store destructive
				adjustABFull(cpu);
				if (OPERAND(cpu->r.A)) {
					// it's	an operand
					computeRelativeAddr(cpu, cpu->r.A, false);
					storeBviaM(cpu);
					cpu->r.AROF = cpu->r.BROF = false;
				} else {
					// it's	a descriptor
					if (presenceTest(cpu, cpu->r.A)) {
						cpu->r.M = cpu->r.A & MASKMEM;
						storeBviaM(cpu);
						cpu->r.AROF = cpu->r.BROF = false;
					}
				}
				break;
			case 010: // 1021: SND=Store nondestructive
				adjustABFull(cpu);
				if (OPERAND(cpu->r.A)) {
					// it's	an operand
					computeRelativeAddr(cpu, cpu->r.A, false);
					storeBviaM(cpu);
					cpu->r.AROF = false;
				} else {
					// it's	a descriptor
					if (presenceTest(cpu, cpu->r.A)) {
						cpu->r.M = cpu->r.A & MASKMEM;
						storeBviaM(cpu);
						cpu->r.AROF = false;
					}
				}
				break;
			case 020: // 2021: LOD=Load operand
				adjustAFull(cpu);
				if (OPERAND(cpu->r.A)) {
					// simple operand
					computeRelativeAddr(cpu, cpu->r.A, true);
					loadAviaM(cpu);
				} else if (presenceTest(cpu, cpu->r.A))	{
					// present descriptor
					cpu->r.M = cpu->r.A & MASKMEM;
					loadAviaM(cpu);
				}
				break;
			case 041: // 4121: ISD=Integer store destructive
				integerStore(cpu, false, true);
				break;
			case 042: // 4221: ISN=Integer store nondestructive
				integerStore(cpu, false, false);
				break;
			}
			break;
		case 025: // XX25: comparison &	misc. stack ops
			switch (variant) {
			case 001: // 0125: GEQ=compare B greater or equal to A
				cpu->r.B = (singlePrecisionCompare(cpu)	>= 0) ?	true : false;
				break;
			case 002: // 0225: GTR=compare B greater to A
				cpu->r.B = (singlePrecisionCompare(cpu)	> 0) ? true : false;
				break;
			case 004: // 0425: NEQ=compare B not equal to A
				cpu->r.B = (singlePrecisionCompare(cpu)	!= 0) ?	true : false;
				break;
			case 041: // 4125: LEQ=compare B less or equal to A
				cpu->r.B = (singlePrecisionCompare(cpu)	<= 0) ?	true : false;
				break;
			case 042: // 4225: LSS=compare B less to A
				cpu->r.B = (singlePrecisionCompare(cpu)	< 0) ? true : false;
				break;
			case 044: // 4425: EQL=compare B equal to A
				cpu->r.B = (singlePrecisionCompare(cpu)	== 0) ?	true : false;
				break;

			case 010: // 1025: XCH=exchange	TOS words
				exchangeTOS(cpu);
				break;
			case 020: // 2025: DUP=Duplicate TOS
				if (cpu->r.AROF) {
					adjustBEmpty(cpu);
					cpu->r.B = cpu->r.A;
					cpu->r.BROF = true;
				} else {
					adjustBFull(cpu);
					cpu->r.A = cpu->r.B;
					cpu->r.AROF = true;
				}
				break;

			case 014: // 1425: FTC=F field to C field
				adjustABFull(cpu);
				t1 = (cpu->r.A & MASK_RCWrF) >>	SHFT_RCWrF;
				cpu->r.B = (cpu->r.B & ~MASK_RCWrC) | (t1 << SHFT_RCWrC);
				cpu->r.AROF = false;
				break;
			case 034: // 3425: FTF=F field to F field
				adjustABFull(cpu);
				t1 = (cpu->r.A & MASK_RCWrF) >>	SHFT_RCWrF;
				cpu->r.B = (cpu->r.B & ~MASK_RCWrF) | (t1 << SHFT_RCWrF);
				break;
			case 054: // 5425: CTC=C field to C field
				adjustABFull(cpu);
				t1 = (cpu->r.A & MASK_RCWrC) >>	SHFT_RCWrC;
				cpu->r.B = (cpu->r.B & ~MASK_RCWrC) | (t1 << SHFT_RCWrC);
				cpu->r.AROF = false;
				break;
			case 074: // 7425: CTF=C field to F field
				adjustABFull(cpu);
				t1 = (cpu->r.A & MASK_RCWrC) >>	SHFT_RCWrC;
				cpu->r.B = (cpu->r.B & ~MASK_RCWrF) | (t1 << SHFT_RCWrF);
				cpu->r.AROF = false;
				break;
			}
			break;
		case 031: // XX31: branch, sign-bit, interrogate ops
			switch (variant) {
			case 001: // 0131: BBC=branch backward conditional
			case 002: // 0231: BFC=branch forward conditional
				adjustABFull(cpu);
				if (cpu->r.B & 1) {
					// true	=> no branch
					cpu->r.AROF = cpu->r.BROF = false;
					break;
				}
				cpu->r.BROF = false;
				goto common_branch;
			case 041: // 4131: BBW=branch backward unconditional
			case 042: // 4231: BFW=branch forward unconditional
				adjustAFull(cpu);
common_branch:
				if (OPERAND(cpu->r.A)) {
					// simple operand
					if (variant == 001 || variant == 041)
						jumpSyllables(cpu, -(cpu->r.A &	0xfff));
					else
						jumpSyllables(cpu, cpu->r.A & 0xfff);
					cpu->r.AROF = false;
				} else {
					// descriptor
					if (cpu->r.L ==	0) {
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
				break;

			case 021: // 2131: LBC=branch backward word conditional
			case 022: // 2231: LFC=branch forward word conditional
				adjustABFull(cpu);
				if (cpu->r.B & 1) {
					// true	=> no branch
					cpu->r.AROF = cpu->r.BROF = false;
					break;
				}
				cpu->r.BROF = false;
				goto common_branch_word;
			case 061: // 6131: LBU=branch backward word unconditional
			case 062: // 6231: LFU=branch forward word unconditional
				adjustAFull(cpu);
common_branch_word:
				if (cpu->r.L ==	0) {
					--cpu->r.C;
					// adjust for Inhibit Fetch
				}
				if (OPERAND(cpu->r.A)) {
					// simple operand
					if (variant == 021 || variant == 061)
						jumpWords(cpu, -(cpu->r.A & 0x03ff));
					else
						jumpWords(cpu, cpu->r.A	& 0x03ff);
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
				break;

			case 004: // 0431: SSN=set sign	bit (set negative)
				adjustAFull(cpu);
				cpu->r.A |= MASK_SIGNMANT;
				break;
			case 010: // 1031: CHS=change sign bit
				adjustAFull(cpu);
				cpu->r.A ^= MASK_SIGNMANT;
				break;
			case 020: // 2031: TOP=test flag bit (test for operand)
				adjustAEmpty(cpu);
				adjustBFull(cpu);
				cpu->r.A = OPERAND(cpu->r.B) ? true : false;
				cpu->r.AROF = true;
				break;
			case 024: // 2431: TUS=interrogate peripheral status
				adjustAEmpty(cpu);
				cpu->r.A = interrogateUnitStatus(cpu);
				cpu->r.AROF = true;
				break;
			case 044: // 4431: SSP=reset sign bit (set positive)
				adjustAFull(cpu);
				cpu->r.A &= ~MASK_SIGNMANT;
				break;
			case 064: // 6431: TIO=interrogate I/O channel
				adjustAEmpty(cpu);
				cpu->r.A = interrogateIOChannel(cpu);
				cpu->r.AROF = true;
				break;
			case 070: // 7031: FBS=stack search for	flag
				adjustAFull(cpu);
				cpu->r.M = cpu->r.A & MASKMEM;
				loadAviaM(cpu);
				while (OPERAND(cpu->r.A)) {
					cpu->r.M = (cpu->r.M+1)	& MASKMEM;
					loadAviaM(cpu);
				}
				// flag	bit found: stop	the search
				cpu->r.A = INIT_DD | MASK_PBIT | cpu->r.M;
				break;
			}
			break;
		case 035: // XX35: exit	& return ops
			switch (variant) {
			case 001: // 0135: BRT=branch return
				adjustAEmpty(cpu);
				if (!cpu->r.BROF) {
					cpu->r.Q03F = true;
					// Q03F: not used, except for display purposes
					adjustBFull(cpu);
				}
				if (presenceTest(cpu, cpu->r.B)) {
					cpu->r.S = (cpu->r.B >>	15) & MASKMEM;
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
				break;
			case 002: // 0235: RTN=return normal
			case 012: // 1235: RTS=return special
				adjustAFull(cpu);
				// If A	is an operand or a present descriptor,
				// proceed with	the return,
				// otherwise throw a p-bit interrupt
				// (cpu	isn't well-documented)
				if (OPERAND(cpu->r.A) || presenceTest(cpu, cpu->r.A)) {
					if (variant == 002) {
						// RTN - reset stack to	F to be	at RCW
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
					case 2:	// flag-bit interrupt occurred,	do nothing
						break;
					}
				}
				break;

			case 004: // 0435: XIT=exit procedure
				cpu->r.AROF = false;
				cpu->r.S = cpu->r.F;
				loadBviaS(cpu);
				// B = [S], fetch the RCW
				exitSubroutine(cpu, false);
				break;
			}
			break;
		case 041: // XX41: index, mark stack, etc.
			switch (variant) {
			case 001: // 0141: INX=index
				adjustABFull(cpu);
				cpu->r.M = (cpu->r.A + cpu->r.B) & MASKMEM;
				cpu->r.A = (cpu->r.A & ~MASKMEM) | cpu->r.M;
				cpu->r.BROF = false;
				break;

			case 002: // 0241: COC=construct operand call
				exchangeTOS(cpu);
				cpu->r.A |= MASK_FLAG;
				// set [0:1]
				operandCall(cpu);
				break;
			case 004: // 0441: MKS=mark stack
				adjustABEmpty(cpu);
				cpu->r.B = buildMSCW(cpu);
				cpu->r.BROF = true;
				adjustBEmpty(cpu);
				cpu->r.F = cpu->r.S;
				if (!cpu->r.MSFF) {
					if (cpu->r.SALF) {
						// store the MSCW at R+7
						cpu->r.M = cpu->r.R*64 + 7;
						storeBviaM(cpu);
						// [M] = B
					}
					cpu->r.MSFF = true;
				}
				break;
			case 012: // 1241: CDC=construct descriptor call
				exchangeTOS(cpu);
				cpu->r.A |= MASK_FLAG;
				// set [0:1]
				descriptorCall(cpu);
				break;
			case 021: // 2141: SSF=F & S register set/store
				adjustABFull(cpu);
				switch (cpu->r.A & 3) {
				case 0:	// store F into	B.[18:15]
					cpu->r.B = (cpu->r.B & MASK_RCWrF) | (cpu->r.F << SHFT_RCWrF);
					break;
				case 1:	// store S into	B.[33:15]
					cpu->r.B = (cpu->r.B & ~MASK_RCWrC) | (cpu->r.S	<< SHFT_RCWrC);
					break;
				case 2:	// set	 F from	B.[18:15]
					cpu->r.F = (cpu->r.B & MASK_RCWrF) >> SHFT_RCWrF;
					cpu->r.SALF = true;
					cpu->r.BROF = false;
					break;
				case 3:	// set	 S from	B.[33:15]
					cpu->r.S = (cpu->r.B & MASK_RCWrC) >> SHFT_RCWrC;
					cpu->r.BROF = false;
					break;
				}
				cpu->r.AROF = false;
				break;
			case 025: // 2541: LLL=link list look-up
				adjustABFull(cpu);
				// get test field
				t1 = cpu->r.A &	MASK_MANTISSA;
				// test	value
				cpu->r.M = cpu->r.B & MASKMEM;
				// starting link address
				do {
					cpu->cycleCount	+= 2;
					// approximate the timing
					loadBviaM(cpu);
					t2 = cpu->r.B &	MASK_MANTISSA;
					if (t2 < t1) {
						cpu->r.M = t2 &	MASKMEM;
					} else {
						cpu->r.A = INIT_DD | MASK_PBIT | cpu->r.M;
						break;
						// B >=	A: stop	look-up
					}
				} while	(true);
				break;
			case 044: // 4441: CMN=enter character mode inline
				enterCharModeInline(cpu);
				break;
			}
			break;
		case 045: // XX45: ISO=Variable	Field Isolate op
			adjustAFull(cpu);
			t2 = variant >>	3; // number of	whole chars
			if (t2)	{
				t1 = cpu->r.G*6	+ cpu->r.H; // starting	source bit position
				t2 = t2*6 - (variant & 7) - cpu->r.H; // number	of bits
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
				cpu->cycleCount	+= (variant >> 3) + (variant & 7) + cpu->r.G + cpu->r.H;
				cpu->r.G = (cpu->r.G + (variant	>> 3)) & 7;
				cpu->r.H = 0;
			}
			break;
		case 051: // XX51: delete & conditional	branch ops
			if (variant < 4) {
				// 0051=DEL: delete TOS	(or field branch with zero-length field)
				if (cpu->r.AROF) {
					cpu->r.AROF = false;
				} else if (cpu->r.BROF)	{
					cpu->r.BROF = false;
				} else {
					--cpu->r.S;
				}
			} else {
				adjustABFull(cpu);
				// field length	(1-15 bits)
				t2 = variant >>	2;
				t1 = fieldIsolate(cpu->r.B, cpu->r.G*6+cpu->r.H, t2);
				// approximate the shift counts
				cpu->cycleCount	+= cpu->r.G + cpu->r.H + (t2 >>	1);
				// A is	unconditionally	empty at end
				cpu->r.AROF = false;
				switch (variant	& 0x03)	{
				case 0x02: // X251/X651: CFD=non-zero field branch forward destructive
					cpu->r.BROF = false;
					// no break: fall through
				case 0x00: // X051/X451: CFN=non-zero field branch forward nondestructive
					if (t1)	{
						if (OPERAND(cpu->r.A)) {
							// simple operand
							jumpSyllables(cpu, cpu->r.A & 0x0fff);
						} else {
							// descriptor
							if (cpu->r.L ==	0) {
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
					break;
				case 0x03: // X351/X751: CBD=non-zero field branch backward destructive
					cpu->r.BROF = false;
					// no break: fall through
				case 0x01: // X151/X551: CBN=non-zero field branch backward nondestructive
					if (t1)	{
						if (OPERAND(cpu->r.A)) {
							// simple operand
							jumpSyllables(cpu, -(cpu->r.A &	0x0fff));
						} else {
							// descriptor
							if (cpu->r.L ==	0) {
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
				break;
				}
			}
			break;
		case 055: // XX55: NOP & DIA=Dial A ops
			if (opcode & 0xFC0) {
				cpu->r.G = variant >> 3;
				cpu->r.H = variant & 7;
			// else	// 0055: NOP=no	operation (the official	one, at	least)
			}
			break;
		case 061: // XX61: XRT & DIB=Dial B ops
			if (opcode & 0xFC0) {
				cpu->r.K = variant >> 3;
				cpu->r.V = variant & 7;
			} else { // 0061=XRT: temporarily set full PRT addressing mode
				cpu->r.VARF = cpu->r.SALF;
				cpu->r.SALF = false;
			}
			break;
		case 065: // XX65: TRB=Transfer	Bits
			adjustABFull(cpu);
			if (variant > 0) {
				t1 = cpu->r.G*6	+ cpu->r.H; // A register starting bit nr
				if (t1+variant > 48) {
					variant	= 48-t1;
				}
				t2 = cpu->r.K*6	+ cpu->r.V; // B register starting bit nr
				if (t2+variant > 48) {
					variant	= 48-t2;
				}
				fieldTransfer(&cpu->r.B, t2, variant, cpu->r.A,	t1);
			}
			cpu->r.AROF = false;
			cpu->cycleCount	+= variant + cpu->r.G +	cpu->r.K;	// approximate the shift counts
			break;
		case 071: // XX71: FCL=Compare Field Low
			adjustABFull(cpu);
			t1 = cpu->r.G*6	+ cpu->r.H;	// A register starting bit nr
			if (t1+variant > 48) {
				variant	= 48-t1;
			}
			t2 = cpu->r.K*6	+ cpu->r.V;	// B register starting bit nr
			if (t2+variant > 48) {
				variant	= 48-t2;
			}
			if (variant == 0) {
				cpu->r.A = true;
			} else if (fieldIsolate(cpu->r.B, t2, variant) < fieldIsolate(cpu->r.A,	t1, variant)) {
				cpu->r.A = true;
			} else {
				cpu->r.A = false;
			}
			cpu->cycleCount	+= variant + cpu->r.G +	cpu->r.K;	// approximate the shift counts
			break;
		case 075: // XX75: FCE=Compare Field Equal
			adjustABFull(cpu);
			t1 = cpu->r.G*6	+ cpu->r.H;	// A register starting bit nr
			if (t1+variant > 48) {
				variant	= 48-t1;
			}
			t2 = cpu->r.K*6	+ cpu->r.V;	// B register starting bit nr
			if (t2+variant > 48) {
				variant	= 48-t2;
			}
			if (variant == 0) {
				cpu->r.A = true;
			} else if (fieldIsolate(cpu->r.B, t2, variant) == fieldIsolate(cpu->r.A, t1, variant)) {
				cpu->r.A = true;
			} else {
				cpu->r.A = false;
			}
			cpu->cycleCount	+= variant + cpu->r.G +	cpu->r.K;	// approximate the shift counts
			break;
		default:
			break;
			// anything else is a no-op
		} // end switch	for non-LITC/OPDC/DESC operators
		break;
	} // end switch	for word-mode operators
}
