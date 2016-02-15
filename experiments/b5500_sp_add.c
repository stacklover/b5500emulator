/***********************************************************************
* b5500emulator
************************************************************************
* Copyright (c) 2016, Reinhard Meyer, DL5UY
* Licensed under the MIT License,
*       see LICENSE
* based on work by Nigel Williams and Paul Kimpel
* see: https://github.com/pkimpel/retro-b5500
************************************************************************
* single precision add/subtract
************************************************************************
* 2016-02-13  R.Meyer
*   Inspired by Paul's work, rest from thin air.
***********************************************************************/

#include "b5500_common.h"

#if DEBUG
# include <stdio.h>
#endif

// TODO:
//   should really implement the X register to be 100% correct,
//   right now we use a 9 bit extension which should be good enough for
//   rounding purposes :)
word48 b5500_sp_addsub(word48 A, word48 B, bit subtract)
{
	int	ea;	// signed exponent of A
	int	eb;	// signed exponent of B
	word49	ma;	// absolute mantissa of A (left justified in word) + 9 bits of extension
	word49	mb;	// absolute mantissa of B (left justified in word) + 9 bits of extension
	bit	sa;	// mantissa sign of A (0=positive)
	bit	sb;	// mantissa sign of B (ditto)

	// if subtracting, complement sign of A
	// why do it more complicated than this?
	if (subtract)
		A ^= MASK_SIGNMANT;

	ma = (A & MASK_MANTISSA) << SHFT_MANTISSALJ; // extract the A mantissa and shift left
	mb = (B & MASK_MANTISSA) << SHFT_MANTISSALJ; // extract the B mantissa and shift left

	ea = (A & MASK_EXPONENT) >> SHFT_EXPONENT;
	sa = (A & MASK_SIGNMANT) >> SHFT_SIGNMANT;
	if (A & MASK_SIGNEXPO)
		ea = -((A & MASK_EXPONENT) >> SHFT_EXPONENT);
	else
		ea = ((A & MASK_EXPONENT) >> SHFT_EXPONENT);

	eb = (B & MASK_EXPONENT) >> SHFT_EXPONENT;
	sb = (B & MASK_SIGNMANT) >> SHFT_SIGNMANT;
	if (B & MASK_SIGNEXPO)
		eb = -((B & MASK_EXPONENT) >> SHFT_EXPONENT);
	else
		eb = ((B & MASK_EXPONENT) >> SHFT_EXPONENT);

#if DEBUG
	printf("ma='%016llo' sa='%d' ea='%d'\n", ma, sa, ea);
	printf("mb='%016llo' sb='%d' eb='%d'\n", mb, sb, eb);
#endif

	// trivial cases
	if (ma == 0 && mb == 0)
		return 0;
	if (ma == 0)
		return B & MASK_NUMBER;
	if (mb == 0)
		return A & MASK_NUMBER;

        // If the exponents are unequal, normalize the larger and scale the smaller
        // until they are in alignment
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

	// At this point, the exponents are aligned,
	// so do the actual 48-bit additions of mantissas (and extensions)

#if DEBUG
	printf("ma='%016llo' sa='%d' ea='%d'\n", ma, sa, ea);
	printf("mb='%016llo' sb='%d' eb='%d'\n", mb, sb, eb);
#endif

	// A and B have same sign?
	if (sa == sb) {
		// we really add
		mb += ma;
		// carry ?
		if (mb & MASK_MANTCARRY) {
			mb >>= 3;
			++eb;
		}
	} else {
		// we must subtract and will do it unsigned, so:
		if (mb > ma) {
			// regular subtract
			mb -= ma;
		} else {
			// inverse subtract
			mb = ma - mb;
			sb = !sb;
		}
	}

#if DEBUG
	printf("mr='%016llo' sr='%d' er='%d'\n", mb, sb, eb);
#endif
	// Normalize and round as necessary
	// highest bit in extension set?
	// note that here our "extension" is just the 9 bits at the right end of
	// the mantissa and not the full "X" register!
	if (mb & 0400)
		mb += 0400;	// round up

	// Check for exponent overflow
	if (eb > 63) {
		eb = 63;
	} else if (eb < -63) {
		eb = -63;
        }

#if DEBUG
	printf("mr='%016llo' sr='%d' er='%d'\n", mb, sb, eb);
#endif

	B = (mb >> SHFT_MANTISSALJ) & MASK_MANTISSA;
	// if B is zero, no exponent, no signs
	if (B == 0)
		return 0ll;

	// put in mantissa sign
	if (sb)
		B |= MASK_SIGNMANT;

	// put in exponent
	if (eb >= 0)
		B |= ((long long)eb << SHFT_EXPONENT);
	else
		B |= (((long long)-eb << SHFT_EXPONENT) | MASK_SIGNEXPO);
	return B; 
}
