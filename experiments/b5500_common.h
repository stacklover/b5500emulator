/***********************************************************************
* b5500emulator
************************************************************************
* Copyright (c) 2016, Reinhard Meyer, DL5UY
* Licensed under the MIT License,
*       see LICENSE
* based on work by Nigel Williams and Paul Kimpel
* see: https://github.com/pkimpel/retro-b5500
************************************************************************
* common declarations
************************************************************************
* 2016-02-13  R.Meyer
*   Inspired by Paul's work, otherwise from thin air.
***********************************************************************/

#ifndef B5500_COMMON_H
#define B5500_COMMON_H

#define	DEBUG	1

typedef unsigned char BIT;			// a single bit
#define true 1
#define false 0
typedef unsigned char WORD2;		// 2 bits
typedef unsigned char WORD3;		// 3 bits
typedef unsigned char WORD4;		// 4 bits
typedef unsigned char WORD6;		// 6 bits
typedef unsigned char WORD8;		// 8 bits
typedef unsigned short ADDR9;		// 9 bits higher part of memory address
typedef unsigned short WORD12;		// 12 bits instruction register
typedef unsigned short ADDR15;		// 15 bits memory address
typedef unsigned long WORD21;		// 21 bits
typedef /*unsigned*/ long long WORD39;	// 39 bits mantissa extension
typedef /*unsigned*/ long long WORD48;	// 48 bits machine word

typedef struct central_control {
	BIT		IAR;
	BIT		HP2F;
	BIT		P2BF;	// CPU #2 busy flag
} CENTRAL_CONTROL;

typedef struct accessor {
	const char	*id;	// pointer to name of requestor
	ADDR15		addr;	// requested address
	WORD48		word;	// data
	BIT			MAIL;	// true if access to 00000..00777
	BIT			MPED;	// parity error detected
	BIT			MAED;	// memory access error detected
} ACCESSOR;

typedef struct cpuregs {
	WORD48		A;	// A register
	WORD48		B;	// B register
	ADDR15		C;	// C register (program address)
	WORD6		E;	// E Memory access control register
	ADDR15		F;	// F register (frame address)
	WORD3		G;	// Character index register for A
	WORD3		H;	// Bit index register for G (in A)
	WORD8		I;	// I register (interrupts)
	WORD4		J;	// J state machine register
	WORD3		K;	// Character index register for B
	WORD2		L;	// Instruction syllable index in P
	ADDR15		M;	// M register (memory address)
	WORD4		N;	// Octal shift counter for B
	WORD48		P;	// Current program instruction word register
	// Q register is handled as BITs, see below
	ADDR9		R;	// High-order 9 bits of PRT base address (TALLY in char mode)
	ADDR15		S;	// S register (stack pointer)
	WORD12		T;	// Current program syllable register
	WORD3		V;	// Bit index register for K (in B)
	WORD39		X;	// Mantissa extension for B (loop control in CM)
	WORD6		Y;	// Serial character register for A
	WORD6		Z;	// Serial character register for B
	WORD8		TM;	// Temporary maintenance storage register

	BIT		Q01F;	// Q register Bit 01
	BIT		Q02F;	// Q register Bit 02
	BIT		Q03F;	// Q register Bit 03
	BIT		Q04F;	// Q register Bit 04
	BIT		Q05F;	// Q register Bit 05
	BIT		Q06F;	// Q register Bit 06
	BIT		Q07F;	// Q register Bit 07
	BIT		Q08F;	// Q register Bit 08
	BIT		Q09F;	// Q register Bit 09
	BIT		Q12F;	// Q register Bit 12 ???
// Q12F: MSFF (word mode: MSCW is pending RCW)
// Q12F: TFFF (char mode: True-False Flip-Flop)
#define	MSFF	Q12F
#define	TFFF	Q12F
	BIT		AROF;	// A register occupied flag
	BIT		BROF;	// B register occupied flag
	BIT		CCCF;	// Clock-count control FF (maintenance only)
	BIT		CWMF;	// Character/word mode FF (1=CM)
	BIT		EIHF;	// E-register Inhibit Address FF
	BIT		HLTF;	// Processor halt FF
	BIT		MRAF;	// Memory read access FF
	BIT		MROF;	// Memory read obtained FF
	BIT		MWOF;	// Memory write obtained FF
	BIT		NCSF;	// Normal/Control State FF (1=normal)
	BIT		PROF;	// P contents valid
	BIT		SALF;	// Program/subroutine state FF (1=subroutine)
	BIT		TROF;	// T contents valid
	BIT		VARF;	// Variant-mode FF (enables full PRT indexing)
	BIT		US14X;	// Operator Halt Switch
	BIT		zzzF;	// one lamp in display right of Q1 has no label 
} CPUREGS;

typedef struct cpu {
	CPUREGS		r;				// CPU register set
	ACCESSOR	acc;			// memory accessor
	const char	*id;			// pointer to name of CPU ("A" or "B")
	unsigned	cycleCount;		// approx of CPU cycles needed
	unsigned	cycleLimit;		// Cycle limit for this.run()
	unsigned	normalCycles;	// Current normal-state cycle count (for UI display)
	unsigned	controlCycles;	// Current control-state cycle count (for UI display)
	unsigned	runCycles;		// Current cycle cound for this.run()
	unsigned	totalCycles;	// Total cycles executed on this processor
	BIT			isP1;			// we are CPU #1
	BIT			busy;			// CPU is busy
} CPU;

/*
 * shared memory resource names and pointers
 */
#define	SHM_MAIN	(('M'<<24)|('A'<<16)|('I'<<8)|'N')
#define	SHM_CPUA	(('C'<<24)|('P'<<16)|('U'<<8)|'A')
#define	SHM_CPUB	(('C'<<24)|('P'<<16)|('U'<<8)|'B')
#define	SHM_CC		(('C'<<24)|('C'<<16)|('_'<<8)|'_')
#define	MSG_CPUA	(('C'<<24)|('P'<<16)|('U'<<8)|'A')
#define	MSG_CPUB	(('C'<<24)|('P'<<16)|('U'<<8)|'B')
#define	MSG_CC		(('C'<<24)|('C'<<16)|('_'<<8)|'_')
#define	MAXMEM		0x8000
#define	MASKMEM		0x7fff

/*
 * Message types
 */
#define	MSG_SIGINT	1	// CPU to CC: signalInterrupt()

#define	MSG_CLEAR	100	// CC to CPU: clear()
#define	MSG_INIT_AS_P2	101	// CC to CPU: initiateAsP2()
#define	MSG_START	102	// CC to CPU: start()
#define	MSG_STOP	103	// CC to CPU: stop()
#define	MSG_PRESET	104	// CC to CPU: preset()

/*
 * global (IPC) memory areas
 */
extern WORD48			*MAIN;
extern CPU				*CPUA;
extern CPU				*CPUB;
extern CENTRAL_CONTROL	*CC;

/*
 * special memory locations (absolute addresses)
 */
#define	AA_IODESC	010		// (0x08) IOCW is stored here by IIO operator
							// also used to store IP2 value
#define	AA_IRQSTACK	0100	// (0x40) stack is set here for IRQ processing
#define	AA_USERMEM	01000	// (0x200) user memory starts here

/*
 * special memory locations (R relative)
 */
#define	RR_MSCW		007		// MSCW is stored here for nested calls
#define	RR_INCW		010		// INCW is stored here on interrupt
#define	RR_COM		011		// COM word is stored here on COM operator

/*
 * interrupt codes
 */
#define	IRQ_MPE		0x01	// memory parity error
#define	IRQ_INVA	0x02	// invalid address
#define	IRQ_STKO	0x04	// stack overflow
#define	IRQ_MASKL	0x0f	// mask for lower 4 IRQ bits
#define	IRQ_COM		0x40	// COM operator
#define	IRQ_PREL	0x50	// program release
#define	IRQ_CONT	0x60	// continuity bit
#define	IRQ_PBIT	0x70	// presence bit
#define	IRQ_FLAG	0x80	// flag bit
#define	IRQ_INDEX	0x90	// invalid index
#define	IRQ_EXPU	0xa0	// expoenent underflow
#define	IRQ_EXPO	0xb0	// exponent overflow
#define	IRQ_INTO	0xc0	// integer overflow
#define	IRQ_DIVZ	0xd0	// divide by zero

/*
 * B5500 integer/real format:
 * 0 <sign mantissa> <sign exponent> <6 bits exponent> <39 bits mantissa>
 * octet numbers         FEDCBA9876543210
 */
#define	MASK_MANTISSA	00007777777777777 // (007f'ffff'ffff) 13 octets unsigned mantissa
#define	MASK_EXPONENT	00770000000000000 // (1f80'0000'0000) 2 octets unsigned exponent
#define	MASK_SIGNEXPO	01000000000000000 // (2000'0000'0000) exponent sign bit
#define	MASK_SIGNMANT	02000000000000000 // (4000'0000'0000) mantissa sign bit
#define	MASK_NUMBER		03777777777777777 // (7fff'ffff'ffff) the number without control bit
#define	MASK_MANTHIGH	00007000000000000 // highest octet of mantissa
#define	MASK_MANTHBIT	00004000000000000 // highest bit of mantissa
#define	MASK_MANTCARRY	00010000000000000 // the carry bit
#define	SHFT_MANTCARRY	39
#define	SHFT_EXPONENT	39
#define	SHFT_SIGNEXPO	45
#define	SHFT_SIGNMANT	46

/*
 * B5500 control word formats:
 * 1<code><present> ...
 * common for all control words
 * octet numbers         FEDCBA9876543210
 */
#define	MASK_FLAG		04000000000000000 // (8000'0000'0000) the control bit
#define	MASK_CODE		02000000000000000 // (4000'0000'0000) the code bit (0=data)
#define	MASK_PBIT		01000000000000000 // (2000'0000'0000) the presence bit
#define	MASK_XBIT		00400000000000000 // (1000'0000'0000) the execute bit (1=PD, 0=CW)
#define	MASK_TYPE		03400000000000000 // (f000'0000'0000) the type bits
#define	SHFT_FLAG		47
#define	SHFT_CODE		46
#define	SHFT_PBIT		45
#define	SHFT_XBIT		44
#define	SHFT_TYPE		44
#define	DESCRIPTOR(x)	((x)&MASK_FLAG)
#define	OPERAND(x)		(!DESCRIPTOR(x))
#define	PRESENT(x)		((x)&MASK_PBIT)
#define	ABSENT(x)		(!PRESENT(x))

/*
 * data descriptor:
 * 10P 000 00 <10 word count> 0<integer><continuity> 000 000 000 000 <15 address>
 * octet numbers         FEDCBA9876543210
 */
#define	INIT_DD			04000000000000000 // (8000'0000'0000) fixed bits that are set
#define	MASK_DDWC		00017770000000000 // (00ff'c000'0000) word count
#define	MASK_DDINT		00000002000000000 // (0000'1000'0000) integer bit
#define	MASK_DDCONT		00000001000000000 // (0000'0800'0000) continuity bit
#define	MASK_DDADDR		00000000000077777 // (0000'0000'7fff) core or disk address
#define	SHFT_DDWC		30
#define	SHFT_DINT		28
#define	SHFT_DDCONT		27
#define	SHFT_DDADDR		0

/*
 * mark stack control word:
 * 110 000 <9 rR> 0<MSFF><SAIF> <15 rF> 000 000 000 000 000
 * octet numbers         FEDCBA9876543210
 */
#define	INIT_MSCW		06000000000000000 // (c000'0000'0000) fixed bits that are set
#define	MASK_MSCWrR		00077700000000000 // (03fe'0000'0000) saved R register
#define	MASK_MSCWMSFF	00000020000000000 // (0000'8000'0000) saved MSFF bit
#define	MASK_MSCWSALF	00000010000000000 // (0000'4000'0000) saved SAIF bit
#define	MASK_MSCWrF		00000007777700000 // (0000'3FFF'8000) saved F register
#define	SHFT_MSCWrR		33
#define	SHFT_MSCWMSFF	31
#define	SHFT_MSCWSALF	30
#define	SHFT_MSCWrF		15

/*
 * program descriptor word:
 * 11P 1<mode><args> 000 000 000 000 <15 rF> <15 address>
 * octet numbers         FEDCBA9876543210
 */
#define	INIT_PCW		06400000000000000 // (d000'0000'0000) fixed bits that are set
#define	MASK_PCWMODE	00200000000000000 // (0800'0000'0000) word/char mode bit
#define	MASK_PCWARGS	00100000000000000 // (0400'0000'0000) arguments required
#define	MASK_PCWrF		00000007777700000 // (0000'3fff'8000) F register when ARGS=0
#define	MASK_PCWADDR	00000000000077777 // (0000'0000'7fff) core or disk address
#define	SHFT_PCWMODE	43
#define	SHFT_PCWARGS	42
#define	SHFT_PCWrF		15
#define	SHFT_PCWADDR	0

/*
 * return control word:
 * 11<type> 0 <3 rH> <3 rV> <2 rL> <3 rG> <3 rK> <15 rF> <15 rC>
 * interrupt return control word:
 * 11<BROF> 0 <3 rH> <3 rV> <2 rL> <3 rG> <3 rK> <15 rF> <15 rC>
 * octet numbers         FEDCBA9876543210
 */
#define	INIT_RCW		06000000000000000 // (c000'0000'0000) fixed bits that are set
#define	MASK_RCWTYPE	01000000000000000 // (2000'0000'0000) type (OPDC/DESC) bit OR
#define	MASK_RCWBROF	01000000000000000 // (2000'0000'0000) saved BROF bit
#define MASK_RCWrH		00340000000000000 // (0e00'0000'0000) saved H register
#define MASK_RCWrV		00034000000000000 // (01c0'0000'0000) saved V register
#define MASK_RCWrL		00003000000000000 // (0030'0000'0000) saved L register
#define MASK_RCWrG		00000700000000000 // (000e'0000'0000) saved G register
#define MASK_RCWrK		00000070000000000 // (0001'c000'0000) saved L register
#define	MASK_RCWrF		00000007777700000 // (0000'3fff'8000) saved F register
#define	MASK_RCWrC		00000000000077777 // (0000'0000'7fff) saved C register
#define	SHFT_RCWTYPE	45
#define	SHFT_RCWBROF	45
#define	SHFT_RCWrH		41
#define	SHFT_RCWrV		38
#define	SHFT_RCWrL		36
#define	SHFT_RCWrG		33
#define	SHFT_RCWrK		30
#define	SHFT_RCWrF		15
#define	SHFT_RCWrC		0

/*
 * interrupt control word:
 * 110 000 <9 rR> 0<MSFF><SALF> 000 00<VARF> 000 00<4 rN> <15 rM>
 * octet numbers         FEDCBA9876543210
 */
#define	INIT_ICW		06000000000000000 // (c000'0000'0000) fixed bits that are set
#define	MASK_ICWrR		00077700000000000 // (03fe'0000'0000) saved R register
#define	MASK_ICWMSFF	00000020000000000 // (0000'8000'0000) saved MSFF bit
#define	MASK_ICWSALF	00000010000000000 // (0000'4000'0000) saved SAIF bit
#define	MASK_ICWVARF	00000000100000000 // (0000'0100'0000) saved SAIF bit
#define	MASK_ICSrN		00000000001700000 // (0000'0007'8000) saved N register
#define	MASK_ICWrM		00000000000077777 // (0000'0000'7fff) saved M register (0 in word mode)
#define	SHFT_ICWrR		33
#define	SHFT_ICWMSFF	31
#define	SHFT_ICWSALF	30
#define	SHFT_ICWVARF	24
#define	SHFT_ICWrN		15
#define	SHFT_ICWrM		0

/*
 * interrupt loop control word:
 * 11<AROF> 000 000 <39 rX>
 * octet numbers         FEDCBA9876543210
 */
#define	INIT_ILCW		06000000000000000 // (c000'0000'0000) fixed bits that are set
#define	MASK_ILCWAROF	01000000000000000 // (2000'0000'0000) saved AROF bit
#define	MASK_ILCWrX		00007777777777777 // (007f'ffff'ffff) saved X register (0 in word mode)
#define	MASK_ILCWrX_S	00000007777700000 // (0000'3fff'8000) saved S part in X
#define	SHFT_ILCWAROF	45
#define	SHFT_ILCWrX		0
#define	SHFT_ILCWrX_S	15

/*
 * initiate control word:
 * 110 00 <9 rQ> <6 rY> <6 rZ> 0 <5 TM bits><MODE> <15 rS> 
 * octet numbers         FEDCBA9876543210
 */
#define	INIT_INCW		06000000000000000 // (c000'0000'0000) fixed bits that are set
#define	MASK_INCWQ09F	00100000000000000 // (0400'0000'0000) saved Q09F bit
#define	MASK_INCWQ08F	00040000000000000 // (0200'0000'0000) saved Q08F bit
#define	MASK_INCWQ07F	00020000000000000 // (0100'0000'0000) saved Q07F bit
#define	MASK_INCWQ06F	00010000000000000 // (0080'0000'0000) saved Q06F bit
#define	MASK_INCWQ05F	00004000000000000 // (0040'0000'0000) saved Q05F bit
#define	MASK_INCWQ04F	00002000000000000 // (0020'0000'0000) saved Q04F bit
#define	MASK_INCWQ03F	00001000000000000 // (0010'0000'0000) saved Q03F bit
#define	MASK_INCWQ02F	00000400000000000 // (0008'0000'0000) saved Q02F bit
#define	MASK_INCWQ01F	00000200000000000 // (0004'0000'0000) saved Q01F bit
#define	MASK_INCWrY		00000176000000000 // (0003'f000'0000) saved Y register
#define	MASK_INCWrZ		00000001760000000 // (0000'0fc0'0000) saved Z register
#define	MASK_INCWrTM	00000000007600000 // (0000'001f'0000) saved TM bits 1-5
#define	MASK_INCWMODE	00000000000100000 // (0000'0000'8000) word/char mode bit
#define	MASK_INCWrS		00000000000077777 // (0000'0000'7fff) saved S register
#define	SHFT_INCWQ09F	42
#define	SHFT_INCWQ08F	41
#define	SHFT_INCWQ07F	40
#define	SHFT_INCWQ06F	39
#define	SHFT_INCWQ05F	38
#define	SHFT_INCWQ04F	37
#define	SHFT_INCWQ03F	36
#define	SHFT_INCWQ02F	35
#define	SHFT_INCWQ01F	34
#define	SHFT_INCWrY		28
#define	SHFT_INCWrZ		22
#define	SHFT_INCWrTM	16
#define	SHFT_INCWMODE	15
#define	SHFT_INCWrS		0

/*
 * I/O descriptor or IO-Unit "D" register:
 * xxx <5 unit> <10 word count> <memory inhibit> xx <binary/alpha>
 * <tape direction> <word/char> <in/out> x <7 result> <15 memory address>
 * octet numbers         FEDCBA9876543210
 */
#define	MASK_IODUNIT	00760000000000000 // (1f00'0000'0000) unit designation
#define	MASK_IODWC		00017770000000000 // (00ff'c000'0000) word/character count
#define	MASK_IODMI		00000004000000000 // (0000'2000'0000) memory inhibit
#define	MASK_IODBINARY	00000000800000000 // (0000'0400'0000) binary mode (0=alpha)
#define	MASK_IODTAPEDIR	00000000400000000 // (0000'0200'0000) tape direction (1=reverse)
#define	MASK_IODWORD	00000000200000000 // (0000'0100'0000) word mode (0=char)
#define	MASK_IODINPUT	00000000100000000 // (0000'0040'0000) input mode (0=output)
#define	MASK_IODRESULT	00000000017700000 // (0000'003f'8000) result
#define	MASK_IODADDR	00000000000077777 // (0000'0000'7fff) memory address
#define	SHFT_IODUNIT	40
#define	SHFT_IODWC		30
#define	SHFT_IODMI		29
#define	SHFT_IODBINARY	26
#define	SHFT_IODTAPEDIR	25
#define	SHFT_IODWORD	24
#define	SHFT_IODINPUT	23
#define	SHFT_IODRESULT	15
#define	SHFT_IODADDR	0

#if 0
/*
 * special use of host wordsize to aid in arithmetics
 */
typedef unsigned long long WORD64;		// carry + 39 bits mantissa + 24 bit extension
#define	MASK_MANTLJ		00777777777777700000000	// mantissa left aligned in 64 bit word
#define	MASK_MANTROUND	00000000000000077777777	// right shifted rounding part
#define	MASK_MANTHIGHLJ	00700000000000000000000	// highest octet of left justified mantissa
#define	MASK_MANTCARRY	01000000000000000000000	// the carry/not borrow bit
#define	SHFT_MANTISSALJ	24
#define	SHFT_EXTTOXREG	(39-24)
#define	VALU_ROUNDUP	00000000000000040000000	// value that causes rounding up
#endif

/*
 * For all single precision operations we use the 64 bits of the host
 * machine's "unsigned long long" (typedef WORD48) to hold the
 * mantissa as follows:
 *
 * Bit 39 holds the carry bit (checked after addition),
 * Bits 38..0 hold the 39 bits of the B5500 mantissa,
 *
 * The exponent, including its sign, is kept in non-B5500 typical two's
 * complement in an integer.
 */

typedef struct num {
	WORD48	m;	// absolute mantissa in above format
	WORD48	x;	// extension of m for right shifts
	int		e;	// signed exponent
	BIT		s;	// sign of mantissa
} NUM;

/* functions available to hande such extracted numbers */
extern void num_extract(WORD48 *, NUM *);
extern void num_compose(NUM *, WORD48 *);
extern void num_left_shift(NUM *, unsigned);
extern unsigned num_left_shift_exp(NUM *, int);
extern unsigned num_right_shift_exp(NUM *, int);
extern void num_right_shift_cnt(NUM *, int);
extern void num_normalize(NUM *, int);
extern void num_round(NUM *);

extern void signalInterrupt(CPU *);
extern void b5500_pdp_text(CPU *);
extern void b5500_init_shares(void);

/* A & B adjustments, stack operations */
extern void adjustABFull(CPU *);
extern void adjustAFull(CPU *);
extern void adjustBFull(CPU *);
extern void adjustABEmpty(CPU *);
extern void adjustAEmpty(CPU *);
extern void adjustBEmpty(CPU *);
extern void exchangeTOS(CPU *);

/* memory accesses */
extern void computeRelativeAddr(CPU *, unsigned offset, BIT cEnabled);
extern void loadAviaM(CPU *);
extern void loadBviaM(CPU *);
extern void loadMviaM(CPU *);
extern void loadAviaS(CPU *);
extern void loadBviaS(CPU *);
extern void loadPviaC(CPU *);
extern void storeAviaM(CPU *);
extern void storeBviaM(CPU *);
extern void storeAviaS(CPU *);
extern void storeBviaS(CPU *);
extern void integerStore(CPU *this, BIT conditional, BIT destructive);
extern BIT indexDescriptor(CPU *this);

/* jumps & calls */
extern void jumpSyllables(CPU *, int count);
extern void jumpWords(CPU *, int count);
extern void jumpOutOfLoop(CPU *, int count);
extern WORD48 buildMSCW(CPU *);
extern void applyMSCW(CPU *, WORD48 mscw);
extern WORD48 buildRCW(CPU *this, BIT descriptorCall);
extern BIT applyRCW(CPU *this, WORD48 word, BIT in_line);
extern void operandCall(CPU *);
extern void descriptorCall(CPU *);
extern void enterSubroutine(CPU *, BIT descriptorCall);
extern int exitSubroutine(CPU *, int how);

/* interrupts & IO */
extern BIT presenceTest(CPU *, WORD48 word);
extern int interrogateUnitStatus(CPU *);
extern int interrogateIOChannel(CPU *);
extern void storeForInterrupt(CPU *, BIT forced, BIT forTest);
extern void clearInterrupt(CPU *);
extern void initiateIO(CPU *);

/* single precision */
extern int singlePrecisionCompare(CPU *);
extern void singlePrecisionAdd(CPU *, BIT add);
extern void singlePrecisionMultiply(CPU *);
extern void singlePrecisionDivide(CPU *);
extern void integerDivide(CPU *);
extern void remainderDivide(CPU *);

/* double precision */
extern void doublePrecisionAdd(CPU *, BIT add);
extern void doublePrecisionMultiply(CPU *);
extern void doublePrecisionDivide(CPU *);

/* stream operations */
extern void streamAdjustSourceChar(CPU *);
extern void streamAdjustDestChar(CPU *);
extern void compareSourceWithDest(CPU *, unsigned count, BIT numeric);
extern void fieldArithmetic(CPU *, unsigned count, BIT adding);
extern void streamBitsToDest(CPU *, unsigned count, WORD48 mask);
extern void streamProgramToDest(CPU *, unsigned count);
extern void streamCharacterToDest(CPU *, unsigned count);
extern void streamNumericToDest(CPU *, unsigned count, unsigned zones);
extern void streamBlankForNonNumeric(CPU *, unsigned count);
extern void streamInputConvert(CPU *, unsigned count);
extern void streamOutputConvert(CPU *, unsigned count);

/* misc & CPU control */
extern void enterCharModeInline(CPU *);
extern void initiate(CPU *, BIT forTest);
extern void initiateP2(CPU *);
extern void start(CPU *);
extern void stop(CPU *);
extern void haltP2(CPU *);
extern WORD48 readTimer(CPU *);
extern void preset(CPU *, ADDR15 runAddr);
extern void b5500_execute_cm(CPU *);
extern void b5500_execute_wm(CPU *);
extern void run(CPU *);

/*
 * bit and field manipulations
 *
 * observe bit numbering!
 */
extern void fieldTransfer(
	WORD48 *dest,		// word to insert into
	unsigned wstart,	// starting bit in that word
	unsigned width,		// number of bits
	WORD48 value,		// value to insert
	unsigned vstart);	// starting bit in the value

extern WORD48 fieldIsolate(
	WORD48 word,		// value to isolate from
	unsigned start,		// starting bit in the value
	unsigned width);	// number of bits

extern WORD48 fieldInsert(
	WORD48 word,
	unsigned start,
	unsigned width,
	WORD48 value);

extern void bitSet(
	WORD48 *dest,
	unsigned bit);

extern void bitReset(
	WORD48 *dest,
	unsigned bit);

/*
 * for assembler/disassembler
 */
typedef enum optype {
	OP_NONE=0,	// no operand
// parsing
	OP_EXPR,	// operand is expression
	OP_RELA,	// operand is relative address
	OP_BRAS,	// optional operand for branch syllables
	OP_BRAW,	// optional operand for branch words
	OP_REGVAL,	// register and value
// output
	OP_ORG,		// set address
	OP_RUN,		// run program from address and wait for halt
	OP_END,		// end
	OP_SET,		// set a register
	OP_VFY,		// verify a register
	OP_ASIS,	// emit code "as is"
	OP_TOP4,	// emit code | (operand << 8)
	OP_TOP6,	// emit code | (operand << 6)
	OP_TOP10,	// emit code | (operand << 2)
	OP_WORD,	// emit one word
} OPTYPE;

typedef struct instruction {
	const char *name;	// symbolical name
	WORD12	code;		// coding
	OPTYPE	intype;		// operand combination in input
	OPTYPE	outtype;	// operand combination in output
} INSTRUCTION;

extern int dotrcmem;		// trace memory accesses
extern int dotrcins;		// trace instruction and IRQs
extern int dotrcmat;		// trace math operations
extern int emode;			// emode math

#endif /* B5500_COMMON_H */
