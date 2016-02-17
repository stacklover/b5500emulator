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
typedef unsigned char WORD8;		// 8 bits
typedef unsigned short ADDR15;		// 15 bits memory address
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
	WORD8		I;	// I register (interrupts)
	ADDR15		M;	// M register (memory address)
	ADDR15		S;	// S register (stack pointer)
	ADDR15		C;	// C register (program address)
	ADDR15		F;	// F register (frame address)
	BIT		AROF;	// A register occupied flag
	BIT		BROF;	// B register occupied flag
	BIT		NCSF;	// not control state flag
} CPUREGS;

typedef struct cpu {
	CPUREGS		r;	// CPU register set
	ACCESSOR	acc;	// memory accessor
	const char	*id;	// pointer to name of CPU ("A" or "B")
} CPU;

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
