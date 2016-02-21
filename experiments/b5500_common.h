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
	WORD21		Q;	// Misc. FFs (bits 1-9 only: Q07F=hardware-induced interrupt, Q09F=enable parallel adder for R-relative addressing)
	ADDR9		R;	// High-order 9 bits of PRT base address (TALLY in char mode)
	ADDR15		S;	// S register (stack pointer)
	WORD12		T;	// Current program syllable register
	WORD3		V;	// Bit index register for K (in B)
	WORD39		X;	// Mantissa extension for B (loop control in CM)
	WORD6		Y;	// Serial character register for A
	WORD6		Z;	// Serial character register for B

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
	BIT		TM;	// Temporary maintenance storage register
	BIT		TROF;	// T contents valid
	BIT		VARF;	// Variant-mode FF (enables full PRT indexing)
	BIT		US14X;	// 
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
#define	MASK_MANTISSA	000007777777777777 // 13 octets unsigned mantissa
#define	MASK_EXPONENT	000770000000000000 // 2 octets unsigned exponent
#define	MASK_SIGNEXPO	001000000000000000 // exponent sign bit
#define	MASK_SIGNMANT	002000000000000000 // mantissa sign bit
#define	MASK_NUMBER	003777777777777777 // the number
#define	SHFT_MANTISSA	0
#define	SHFT_EXPONENT	39
#define	SHFT_SIGNEXPO	45
#define	SHFT_SIGNMANT	46

/*
 * B5500 control word formats
 */
#define	MASK_CONTROLW	004000000000000000 // the control bit
#define	MASK_PBIT	001000000000000000 // the presence bit
#define	MASK_FIELD_F	000000007777700000 // the F field
#define	MASK_FIELD_C_S	000000000000077777 // the C field
#define	SHFT_CONTROLW	47
#define	SHFT_PBIT	45
#define	SHFT_FIELD_F	15
#define	SHFT_FIELD_C_S	0

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
extern void storeForInterrupt(CPU *, int, int);
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
