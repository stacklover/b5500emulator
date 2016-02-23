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

typedef unsigned char BIT;		// a single bit
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
typedef unsigned long long WORD39;	// 39 bits mantissa extension
typedef unsigned long long WORD48;	// 48 bits machine word

typedef struct central_control {
	BIT		IAR;
	BIT		HP2F;
	BIT		P2BF;	// CPU #2 busy flag
} CENTRAL_CONTROL;

typedef struct accessor {
	const char	*id;	// pointer to name of requestor
	ADDR15		addr;	// requested address
	WORD48		word;	// data
	BIT		MAIL;	// true if access to 00000..00777
	BIT		MPED;	// parity error detected
	BIT		MAED;	// memory access error detected
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
	BIT		AROF;	// A register occupied flag
	BIT		BROF;	// B register occupied flag
	BIT		CCCF;	// Clock-count control FF (maintenance only)
	BIT		CWMF;	// Character/word mode FF (1=CM)
	BIT		EIHF;	// E-register Inhibit Address FF
	BIT		HLTF;	// Processor halt FF
	BIT		MRAF;	// Memory read access FF
	BIT		MROF;	// Memory read obtained FF
	BIT		MSFF;	// Mark-stack FF (word mode: MSCW is pending RCW, physically also TFFF & Q12F)
	BIT		MWOF;	// Memory write obtained FF
	BIT		NCSF;	// Normal/Control State FF (1=normal)
	BIT		PROF;	// P contents valid
	BIT		SALF;	// Program/subroutine state FF (1=subroutine)
	BIT		TROF;	// T contents valid
	BIT		VARF;	// Variant-mode FF (enables full PRT indexing)
	BIT		US14X;	//
	BIT		zzzF;	// one lamp in display right of Q1 has no label 
	BIT		isP1;	// we are CPU #1
} CPUREGS;

typedef struct cpu {
	CPUREGS		r;	// CPU register set
	ACCESSOR	acc;	// memory accessor
	CENTRAL_CONTROL	*cc;	// central control
	const char	*id;	// pointer to name of CPU ("A" or "B")
	unsigned	cycleCount;	// approx of CPU cycles needed
} CPU;

/*
 * shared memory resource names and pointers
 */
#define	SHM_MAIN	(('M'<<24)|('A'<<16)|('I'<<8)|'N')
#define	SHM_CPUA	(('C'<<24)|('P'<<16)|('U'<<8)|'A')
#define	SHM_CPUB	(('C'<<24)|('P'<<16)|('U'<<8)|'B')
#define	MAXMEM		0x8000
#define	MASKMEM		0x7fff

extern WORD48	*MAIN;
extern CPU	*CPUA;
extern CPU	*CPUB;

/*
 * B5500 integer/real format:
 * 0 <sign mantissa> <sign exponent> <6 bits exponent> <39 bits mantissa>
 * octet numbers         FEDCBA9876543210
 */
#define	MASK_MANTISSA	00007777777777777 // (007f'ffff'ffff) 13 octets unsigned mantissa
#define	MASK_EXPONENT	00770000000000000 // (1f80'0000'0000) 2 octets unsigned exponent
#define	MASK_SIGNEXPO	01000000000000000 // (2000'0000'0000) exponent sign bit
#define	MASK_SIGNMANT	02000000000000000 // (4000'0000'0000) mantissa sign bit
#define	MASK_NUMBER	03777777777777777 // (7fff'ffff'ffff) the number without control bit
#define	SHFT_MANTISSA	0
#define	SHFT_EXPONENT	39
#define	SHFT_SIGNEXPO	45
#define	SHFT_SIGNMANT	46

/*
 * B5500 control word formats:
 * 1 <code> <present> ...
 * common for all control words
 * octet numbers         FEDCBA9876543210
 */
#define	MASK_CONTROLW	04000000000000000 // (8000'0000'0000) the control bit
#define	MASK_CODE	02000000000000000 // (4000'0000'0000) the code bit
#define	MASK_PBIT	01000000000000000 // (2000'0000'0000) the presence bit
#define	SHFT_CONTROLW	47
#define	SHFT_CODE	46
#define	SHFT_PBIT	45

/*
 * data descriptor:
 * 1 0 <present> <5 unused> <10 word count> <1 unused>
 *                           <integer> <continuity> <12 unused> <15 address>
 * octet numbers         FEDCBA9876543210
 */
#define	INIT_DD		04000000000000000 // (8000'0000'0000) fixed bits that are set
#define	MASK_DDWC	00017770000000000 // (00ff'c000'0000) word count
#define	MASK_DDINT	00000002000000000 // (0000'1000'0000) integer bit
#define	MASK_DDCONT	00000001000000000 // (0000'0800'0000) continuity bit
#define	MASK_DDADDR	00000000000077777 // (0000'0000'7fff) core or disk address
#define	MASK_DDUNUSED	00760004777700000 // (1f00'27ff'8000) usused bits
#define	SHFT_DDWC	30
#define	SHFT_DINT	28
#define	SHFT_DDCONT	27
#define	SHFT_DDADDR	0

/*
 * mark stack control word:
 * 1 1 <1 unused> 1 <2 unused> <9 rR> <1 unused> <MSFF> <SAIF> <15 rF> <15 unused>
 * octet numbers         FEDCBA9876543210
 */
#define	INIT_MSCW	06400000000000000 // (d000'0000'0000) fixed bits that are set
#define	MASK_MSCWrR	00077700000000000 // (03fe'0000'0000) saved R register
#define	MASK_MSCWMSFF	00000020000000000 // (0000'8000'0000) saved MSFF bit
#define	MASK_MSCWSALF	00000010000000000 // (0000'4000'0000) saved SAIF bit
#define	MASK_MSCWrF	00000007777700000 // (0000'3FFF'8000) saved F register
#define	MASK_MSCWUNUSED	01300040000077777 // (2c01'0000'7fff) unused bits
#define	SHFT_MSCWrR	33
#define	SHFT_MSCWMSFF	31
#define	SHFT_MSCWSALF	30
#define	SHFT_MSCWrF	15

/*
 * program descriptor word:
 * 1 1 <present> 1 <mode> <args> <12 unsued> <15 rF> <15 address>
 * octet numbers         FEDCBA9876543210
 */
#define	INIT_PCW	06400000000000000 // (d000'0000'0000) fixed bits that are set
#define	MASK_PCWMODE	00200000000000000 // (0800'0000'0000) word/char mode bit
#define	MASK_PCWARGS	00100000000000000 // (0400'0000'0000) arguments required
#define	MASK_PCWrF	00000007777700000 // (0000'3fff'8000) F register when ARGS=0
#define	MASK_PCWADDR	00000000000077777 // (0000'0000'7fff) core or disk address
#define	MASK_PCWUNUSED	00077770000000000 // (03ff'c000'0000) unused bits
#define	SHFT_PCWMODE	43
#define	SHFT_PCWARGS	42
#define	SHFT_PCWrF	15
#define	SHFT_PCWADDR	0

/*
 * return control word:
 * 1 1 <type> 0 <3 rH> <3 rV> <2 rL> <3 rG> <3 rK> <15 rF> <15 rC>
 * interrupt return control word:
 * 1 1 <BROF> 0 <3 rH> <3 rV> <2 rL> <3 rG> <3 rK> <15 rF> <15 rC>
 * octet numbers         FEDCBA9876543210
 */
#define	INIT_RCW	06000000000000000 // (c000'0000'0000) fixed bits that are set
#define	MASK_RCWTYPE	01000000000000000 // (2000'0000'0000) type (OPDC/DESC) bit OR
#define	MASK_RCWBROF	01000000000000000 // (2000'0000'0000) saved BROF bit
#define MASK_RCWrH	00340000000000000 // (0e00'0000'0000) saved H register
#define MASK_RCWrV	00034000000000000 // (01c0'0000'0000) saved V register
#define MASK_RCWrL	00003000000000000 // (0030'0000'0000) saved L register
#define MASK_RCWrG	00000700000000000 // (000e'0000'0000) saved G register
#define MASK_RCWrK	00000070000000000 // (0001'c000'0000) saved L register
#define	MASK_RCWrF	00000007777700000 // (0000'3fff'8000) saved F register
#define	MASK_RCWrC	00000000000077777 // (0000'0000'7fff) saved C register
#define	MASK_RCWUNUSED	00000000000000000 // (0000'0000'0000) unused bits
#define	SHFT_RCWTYPE	45
#define	SHFT_RCWBROF	45
#define	SHFT_RCWrH	41
#define	SHFT_RCWrV	38
#define	SHFT_RCWrL	36
#define	SHFT_RCWrG	33
#define	SHFT_RCWrK	30
#define	SHFT_RCWrF	15
#define	SHFT_RCWrC	0

/*
 * interrupt control word:
 * 1 1 <1 unused> 0 <2 unused> <9 rR> <1 unused> <MSFF> <SALF> <5 unused>
 *		<VARF> <5 unused> <4 rN> <15 rM>
 * octet numbers         FEDCBA9876543210
 */
#define	INIT_ICW	06000000000000000 // (c000'0000'0000) fixed bits that are set
#define	MASK_ICWrR	00077700000000000 // (03fe'0000'0000) saved R register
#define	MASK_ICWMSFF	00000020000000000 // (0000'8000'0000) saved MSFF bit
#define	MASK_ICWSALF	00000010000000000 // (0000'4000'0000) saved SAIF bit
#define	MASK_ICWVARF	00000000100000000 // (0000'0100'0000) saved SAIF bit
#define	MASK_ICSrN	00000000001700000 // (0000'0007'8000) saved N register
#define	MASK_ICWrM	00000000000077777 // (0000'0000'7fff) saved M register (0 in word mode)
#define	MASK_ICWUNUSED	01300047676000000 // (2c01'3ef8'0000) unused bits
#define	SHFT_ICWrR	33
#define	SHFT_ICWMSFF	31
#define	SHFT_ICWSALF	30
#define	SHFT_ICWVARF	24
#define	SHFT_ICWrN	15
#define	SHFT_ICWrM	0

/*
 * interrupt loop control word:
 * 1 1 <AROF> 0 <5 unused> <39 rX>
 * octet numbers         FEDCBA9876543210
 */
#define	INIT_ILCW	06000000000000000 // (c000'0000'0000) fixed bits that are set
#define	MASK_ILCWAROF	01000000000000000 // (2000'0000'0000) saved AROF bit
#define	MASK_ILCWrX	00007777777777777 // (007f'ffff'ffff) saved X register (0 in word mode)
#define	MASK_ILCWUNUSED	00370000000000000 // (0f80'0000'0000) unused bits
#define	SHFT_ILCWAROF	45
#define	SHFT_ILCWrX	0

/*
 * initiate control word:
 * 1 1 <1 unused> 0 <1 unused> <9 rQ> <6 rY> <6 rZ> <1 unused> <5 TM bits> <MODE> <15 rS> 
 * octet numbers         FEDCBA9876543210
 */
#define	INIT_INCW	06000000000000000 // (c000'0000'0000) fixed bits that are set
#define	MASK_INCWQ09F	00100000000000000 // (0400'0000'0000) saved Q09F bit
#define	MASK_INCWQ08F	00040000000000000 // (0200'0000'0000) saved Q08F bit
#define	MASK_INCWQ07F	00020000000000000 // (0100'0000'0000) saved Q07F bit
#define	MASK_INCWQ06F	00010000000000000 // (0080'0000'0000) saved Q06F bit
#define	MASK_INCWQ05F	00004000000000000 // (0040'0000'0000) saved Q05F bit
#define	MASK_INCWQ04F	00002000000000000 // (0020'0000'0000) saved Q04F bit
#define	MASK_INCWQ03F	00001000000000000 // (0010'0000'0000) saved Q03F bit
#define	MASK_INCWQ02F	00000400000000000 // (0008'0000'0000) saved Q02F bit
#define	MASK_INCWQ01F	00000200000000000 // (0004'0000'0000) saved Q01F bit
#define	MASK_INCWrY	00000176000000000 // (0003'f000'0000) saved Y register
#define	MASK_INCWrZ	00000001760000000 // (0000'0fc0'0000) saved Z register
#define	MASK_INCWrTM	00000000007600000 // (0000'001f'0000) saved TM bits 1-5
#define	MASK_INCWMODE	00000000000100000 // (0000'0000'8000) word/char mode bit
#define	MASK_INCWrS	00000000000077777 // (0000'0000'7fff) saved S register
#define	MASK_INCWUNUSED	01200000017000000 // (2800'0020'0000) unused bits
#define	SHFT_INCWQ09F	42
#define	SHFT_INCWQ08F	41
#define	SHFT_INCWQ07F	40
#define	SHFT_INCWQ06F	39
#define	SHFT_INCWQ05F	38
#define	SHFT_INCWQ04F	37
#define	SHFT_INCWQ03F	36
#define	SHFT_INCWQ02F	35
#define	SHFT_INCWQ01F	34
#define	SHFT_INCWrY	28
#define	SHFT_INCWrZ	22
#define	SHFT_INCWrTM	16
#define	SHFT_INCWMODE	15
#define	SHFT_INCWrS	0

/*
 * special use of host wordsize to aid in arithmetics
 */
typedef unsigned long long WORD49;	// carry + 39 bits mantissa + 9 bit extension
#define	MASK_MANTLJ	007777777777777000 // mantissa left aligned in 48 bit word
#define	MASK_MANTHIGHLJ	007000000000000000 // highest octet of left justified mantissa
#define	MASK_MANTCARRY	010000000000000000 // the carry/not borrow bit
#define	SHFT_MANTISSALJ	9

/* functions available */
extern int b5500_sp_compare(CPU *);
extern void b5500_sp_addsub(CPU *, BIT subtract);
extern int b5500_sp_addsub2(CPU *);

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
extern void computeRelativeAddr(CPU *, int, int);
extern void loadAviaM(CPU *);
extern void loadBviaM(CPU *);
extern void loadAviaS(CPU *);
extern void loadBviaS(CPU *);
extern void loadPviaC(CPU *);
extern void storeAviaM(CPU *);
extern void storeBviaM(CPU *);
extern void storeAviaS(CPU *);
extern void storeBviaS(CPU *);
extern void integerStore(CPU *, int conditional, int destructive);

/* jumps & calls */
extern void jumpSyllables(CPU *, int count);
extern void jumpWords(CPU *, int count);
extern void jumpOutOfLoop(CPU *, int count);
extern WORD48 buildMSCW(CPU *);
extern void applyMSCW(CPU *, WORD48 mscw);
extern void operandCall(CPU *);
extern void descriptorCall(CPU *);
extern int exitSubroutine(CPU *, int how);

/* interrupts & IO */
extern int presenceTest(CPU *, WORD48 value);
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
extern void initiate(CPU *, int);
extern void initiateP2(CPU *);
extern void stop(CPU *);
extern void haltP2(CPU *);
extern WORD48 readTimer(CPU *);

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

#endif /* B5500_COMMON_H */
