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
} CPUREGS;

typedef struct cpu {
	CPUREGS		r;	// CPU register set
	ACCESSOR	acc;	// memory accessor
	const char	*id;	// pointer to name of CPU ("A" or "B")
} CPU;

/*
 * shared memory resource names and pointers
 */
#define	SHM_MAIN	(('M'<<24)|('A'<<16)|('I'<<8)|'N')
#define	SHM_CPUA	(('C'<<24)|('P'<<16)|('U'<<8)|'A')
#define	SHM_CPUB	(('C'<<24)|('P'<<16)|('U'<<8)|'B')
#define	MAXMEM		32768

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
#define	MASK_CONTROLW	004000000000000000 // the control bit
#define	SHFT_MANTISSA	0
#define	SHFT_EXPONENT	39
#define	SHFT_SIGNEXPO	45
#define	SHFT_SIGNMANT	46
#define	SHFT_CONTROLW	47

/*
 * special use of host wordsize to aid in arithmetics
 */
typedef unsigned long long WORD49;	// carry + 39 bits mantissa + 9 bit extension
#define	MASK_MANTLJ	007777777777777000 // mantissa left aligned in 48 bit word
#define	MASK_MANTHIGHLJ	007000000000000000 // highest octet of left justified mantissa
#define	MASK_MANTCARRY	010000000000000000 // the carry/not borrow bit
#define	SHFT_MANTISSALJ	9

/* functions available */
extern int b5500_sp_compare(CPU *this);
extern void b5500_sp_addsub(CPU *this, BIT subtract);

extern void signalInterrupt(CPU *this);

#endif /* B5500_COMMON_H */
