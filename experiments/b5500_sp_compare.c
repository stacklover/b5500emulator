/***********************************************************************
* b5500emulator
************************************************************************
* Copyright (c) 2016, Reinhard Meyer, DL5UY
* Licensed under the MIT License,
*       see LICENSE
* based on work by Nigel Williams and Paul Kimpel
* see: https://github.com/pkimpel/retro-b5500
************************************************************************
* single precision compare
************************************************************************
* 2016-02-13  R.Meyer
*   Inspired by Paul's work, rest from thin air.
***********************************************************************/

#include "b5500_common.h"

#if DEBUG
# include <stdio.h>
#endif

/*
Algebraically compares the B register to the A register.

Function returns:
 -1 if B<A
  0 if B=A
 +1 if B>A
*/
int b5500_sp_compare(word48 A, word48 B)
{
	int	ea;	// signed exponent of A
	int	eb;	// signed exponent of B
	word49	ma;	// absolute mantissa of A (left justified in word)
	word49	mb;	// absolute mantissa of B (left justified in word)
	bit	sa;	// mantissa sign of A (0=positive)
	bit	sb;	// mantissa sign of B (ditto)

	ma = (A & MASK_MANTISSA) << SHFT_MANTISSALJ; // extract the A mantissa and shift left
	mb = (B & MASK_MANTISSA) << SHFT_MANTISSALJ; // extract the B mantissa and shift left

	// Extract the exponents and signs.

	if (ma == 0) { 	// if A mantissa is zero
		ea = 0;
		sa = 0; // consider A to be completely zero
	} else {
		ea = (A & MASK_EXPONENT) >> SHFT_EXPONENT;
		sa = (A & MASK_SIGNMANT) >> SHFT_SIGNMANT;
		if (A & MASK_SIGNEXPO)
			ea = -((A & MASK_EXPONENT) >> SHFT_EXPONENT);
		else
			ea = ((A & MASK_EXPONENT) >> SHFT_EXPONENT);
	}

	if (mb == 0) { // if B mantissa is zero
		eb = 0;
		sb = 0; // consider B to be completely zero
	} else {
		eb = (B & MASK_EXPONENT) >> SHFT_EXPONENT;
		sb = (B & MASK_SIGNMANT) >> SHFT_SIGNMANT;
		if (B & MASK_SIGNEXPO)
			eb = -((B & MASK_EXPONENT) >> SHFT_EXPONENT);
		else
			eb = ((B & MASK_EXPONENT) >> SHFT_EXPONENT);
	}

#if DEBUG
	printf("ma='%016llo' sa='%d' ea='%d'\n", ma, sa, ea);
	printf("mb='%016llo' sb='%d' eb='%d'\n", mb, sb, eb);
#endif

        // If the exponents are unequal, normalize the larger and scale the smaller
        // until they are in alignment, or one of the mantissas (mantissae?) becomes zero
	if (ea > eb) {
		// Normalize A for 39 bits (13 octades)
		while (!(ma & MASK_MANTHIGHLJ) && (ea != eb)) {
			ma <<= 3; // shift left
			--ea;
		}
		// Scale B until its exponent matches (mantissa may go to zero)
		while (ea != eb) {
			mb >>= 3;	// shift right
			++eb;
		}
	} else if (ea < eb) {
		// Normalize B for 39 bits (13 octades)
		while (!(mb & MASK_MANTHIGHLJ) && (eb != ea)) {
			mb <<= 3; // shift left
			--eb;
		}
		// Scale A until its exponent matches (mantissa may go to zero)
		while (eb != ea) {
			ma >>= 3;	// shift right
			++ea;
		}
	}

	// detect zero mantissa
	if (!(ma & MASK_MANTLJ)) {
		ma = 0;
		sa = 0;
		ea = 0;
	}
	if (!(mb & MASK_MANTLJ)) {
		mb = 0;
		sb = 0;
		eb = 0;
	}
		
#if DEBUG
	printf("ma='%016llo' sa='%d' ea='%d'\n", ma, sa, ea);
	printf("mb='%016llo' sb='%d' eb='%d'\n", mb, sb, eb);
#endif

	// Compare signs, exponents, and normalized magnitudes,
	// in that order.
	if (sb == sa) { // if signs are equal:
		if (eb == ea) { // if exponents are equal:
			if (mb == ma) { // if magnitudes are equal:
				return 0; // then the operands are equal
			} else
			if (mb > ma) { // otherwise, if magnitude of B > A:
				return (sb ? -1 : 1); // B<A if B negative, B>A if B positive
			} else { // otherwise, if magnitude of B < A:
				return (sb ? 1 : -1); // B>A if B negative, B<A if B positive
			}
		} else
		if (eb > ea) { // otherwise, if exponent of B > A:
			return (sb ? -1 : 1); // B<A if B negative, B>A if B positive
		} else { // otherwise, if exponent of B < A
			return (sb ? 1 : -1); // B>A if B negative, B<A if B positive
		}
	} else { // otherwise, if signs are different:
		return (sa < sb ? -1 : 1); // B<A if B negative, B>A if B positive
	}
};
