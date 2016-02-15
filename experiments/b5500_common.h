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

typedef unsigned char bit;
typedef unsigned long long word48;
typedef unsigned long long word49;	// extended mantissa with carry

/*
 * B5500 integer/real format:
 * 0 <sign mantissa> <sign exponent> <6 bits exponent> <39 bits mantissa>
 * octet numbers         FEDCBA9876543210
 */
#define	MASK_MANTISSA	000007777777777777 // 13 octets unsigned mantissa
#define	MASK_MANTLJ	007777777777777000 // mantissa left aligned in 48 bit word
#define	MASK_MANTHIGHLJ	007000000000000000 // highest octet of left justified mantissa
#define	MASK_EXPONENT	000770000000000000 // 2 octets unsigned exponent
#define	MASK_SIGNEXPO	001000000000000000 // exponent sign bit
#define	MASK_SIGNMANT	002000000000000000 // mantissa sign bit
#define	MASK_NUMBER	003777777777777777 // the number
#define	MASK_CONTROLW	004000000000000000 // the control bit
#define	MASK_MANTCARRY	010000000000000000 // the carry/not borrow bit
#define	SHFT_MANTISSA	0
#define	SHFT_MANTISSALJ	9
#define	SHFT_EXPONENT	39
#define	SHFT_SIGNEXPO	45
#define	SHFT_SIGNMANT	46
#define	SHFT_CONTROLW	47

/* functions available */
extern int b5500_sp_compare(word48 A, word48 B);
extern word48 b5500_sp_addsub(word48 A, word48 B, bit subtract);

#endif /* B5500_COMMON_H */
