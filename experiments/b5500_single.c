/***********************************************************************
* b5500emulator
************************************************************************
* Copyright (c) 2016, Reinhard Meyer, DL5UY
* Licensed under the MIT License,
*       see LICENSE
* based on (C) work by Nigel Williams and Paul Kimpel
* see: https://github.com/pkimpel/retro-b5500
************************************************************************
* single precision arithmetic
************************************************************************
* 2016-02-1921  R.Meyer
*   Converted Paul's work from Javascript to C
***********************************************************************/

#include <stdio.h>
#include "b5500_common.h"
#define DEBUG 1

/*
 * print a NUM in human readable form
 */
void num_printf(NUM *n)
{
	printf("%c%013llo.%013llo**%d\n", n->s ? '-' : '+', n->m, n->x, n->e);
}

/*
 * extract a B5500 word into its numeric components
 */
void num_extract(WORD48 *in, NUM *out)
{
	// extract the mantissa
	out->m = (*in & MASK_MANTISSA);

	if (out->m == 0) {
		// if the mantissa is zero consider the number to be completely zero
		out->e = 0;
		out->s = false;
	} else {
		// Extract the exponents and sign
		out->s = (*in & MASK_SIGNMANT) >> SHFT_SIGNMANT;
		out->e = (*in & MASK_EXPONENT) >> SHFT_EXPONENT;
		if (*in & MASK_SIGNEXPO)
			out->e = -out->e;
	}
	out->x = 0;
}

/*
 * compose a B5500 word from its numeric components
 */
void num_compose(NUM *in, WORD48 *out)
{
	*out = in->m;
	// if n is zero, no exponent, no signs
	if (in->m == 0) {
		*out = 0;
		return;
	}

	// put in mantissa sign
	if (in->s)
		*out |= MASK_SIGNMANT;

	// put in exponent
	if (in->e >= 0)
		*out |= ((long long)(in->e) << SHFT_EXPONENT);
	else
		*out |= (((long long)(-in->e) << SHFT_EXPONENT) | MASK_SIGNEXPO);
}

/*
 * normalize a number using the extension
 */
void num_normalize(NUM *n, int exp)
{
	if (n->e != 0) {
		// exponent underflow ?
		if (n->e < -63) {
			// de-normalize until exponent is >="exp" or digits get lost
			while ((n->e < exp) && ((n->m & 7) == 0) && (n->x == 0))
				num_right_shift_cnt(n, 1);
		} else {
			num_left_shift_exp(n, -63);
		}
	}
}

/*
 * round a number using the mantissa and the extension
 *
 * there are interesting differences in EMODE and B5500 mode:
 *
 * EMODE: checks highest bit of X for non-zero to enable rounding.
 *        if the highest octet of the mantissa is zero:
 *          shift both X and mantissa
 *          one octet to the left (and adjust exponent),
 *          increasing precision of the result by one octet.
 *        else
 *          increase mantissa by one and handle carry
 * B5500: checks highest octet of X for non-zero to enable rounding. 
 *        if the highest octet of the mantissa is zero:
 *          shift both X and mantissa
 *          one octet to the left (and adjust exponent),
 *          increasing precision of the result by one octet.
 *        check the now highest bit of X for non-zero AND
 *        mantissa not being all-ones:
 *          increase mantissa by one (no carry can happen)
 * Note: the highest octet of the mantissa can only happen to be zero
 *       after a subtraction of two same sign numbers or after an addition
 *       of two opposite sign numbers. In both cases the B5500 seems to do
 *       the better rounding.
 */
void num_round(NUM *n)
{
	// round up
	if (emode) {
		// EMODE: check highest bit
		if (n->x & MASK_MANTHBIT) {
			// shift left if highest octet is zero
			if ((n->m & MASK_MANTHIGH) == 0) {
				num_left_shift(n, 0);
			} else {
				// OTHERWISE:
				// increment mantissa and handle possible
				// carry on an all-ones mantissa
				n->m++;
				if (n->m & MASK_MANTCARRY)
					num_right_shift_cnt(n, 1);
			}
		}
	} else {
		// B5500: check highest octet
		if (n->x & MASK_MANTHIGH) {
			// shift left if highest octet is zero
			if ((n->m & MASK_MANTHIGH) == 0) {
				num_left_shift(n, 0);
			}
			// ADDITIONALLY: round if the now topmost bit of X is set
			// increment mantissa ONLY if there will be no carry
			if ((n->x & MASK_MANTHBIT)
						&& ((n->m & MASK_MANTISSA) != MASK_MANTISSA)) {
				n->m++;
			}
		}
	}
}

/*
 * left shift a number of octets towards a given exponent
 * but stop before shifting out on the left
 */
unsigned num_left_shift_exp(NUM *n, int exp)
{
	unsigned cnt = 0;
	while (!(n->m & MASK_MANTHIGH) && (n->e > exp)) {
		n->m = (n->m << 3) & MASK_MANTISSA;
		n->m |= (n->x) >> 36;
		n->x = (n->x << 3) & MASK_MANTISSA;
		n->e--;
		cnt++;
	}
	return cnt;
}

/*
 * left shift one octet to the left and insert
 * data at the right
 */
void num_left_shift(NUM *n, unsigned d)
{
	n->m = (n->m << 3);
	n->m |= (n->x) >> 36;
	n->x = (n->x << 3) & MASK_MANTISSA;
	n->x |= d;
	n->e--;
}

/*
 * right shift a number of octets towards a given exponent
 * the mantissa may become zero
 */
unsigned num_right_shift_exp(NUM *n, int exp)
{
	unsigned cnt = 0;
	while (n->e < exp) {
		cnt++;
		// shift rightmost octet into x
		n->x = (n->x >> 3) | ((n->m & 7) << 36);
		n->m >>= 3;	// shift right
		n->e++;
		// on B5500, stop when mantissa is zero
		// UNLESS the exponents just became the same!
		if (!emode && (n->m == 0) && (n->e != exp)) {
			n->e = exp;
			n->x = 0;
			break;
		}
	}
	return cnt;
}

/*
 * right shift a number of octets
 * the mantissa may become zero
 */
void num_right_shift_cnt(NUM *n, int cnt)
{
	while (cnt > 0) {
		// shift rightmost octet into x
		n->x = (n->x >> 3) | ((n->m & 7) << 36);
		n->m >>= 3;	// shift right
		n->e++;
		cnt--;
	}
}

/*
 * Algebraically compares the B register to the A register.
 *
 * Function returns:
 * -1 if B<A
 *  0 if B=A
 * +1 if B>A
*/
int singlePrecisionCompare(CPU *this)
{
	NUM		A;	// extracted A register
	NUM		B;	// extracted B register

    this->cycleCount += 4;	// estimate some general overhead
    adjustABFull(this);
    this->r.AROF = 0;		// A is unconditionally marked empty

	// extract the numbers
	num_extract(&this->r.A, &A);
	num_extract(&this->r.B, &B);

#if DEBUG
	if (dotrcmat) {
		printf("After extraction:\n");
		num_printf(&A);
		num_printf(&B);
	}
#endif

	// If the exponents are unequal, normalize the larger until the high-order
	// octade is non-zero or the exponents are equal.
	if (A.e > B.e) {
		this->cycleCount += num_left_shift_exp(&A, B.e);
	} else if (A.e < B.e) {
		this->cycleCount += num_left_shift_exp(&B, A.e);
	}

#if DEBUG
	if (dotrcmat) {
		printf("After exponent equalisation try:\n");
		num_printf(&A);
		num_printf(&B);
	}
#endif

	// Compare signs, exponents, and normalized magnitudes,
	// in that order.
	if (B.s == A.s) { // if signs are equal:
		if (B.e == A.e) { // if exponents are equal:
			if (B.m == A.m) { // if magnitudes are equal:
				return 0; // then the operands are equal
			} else
			if (B.m > A.m) { // otherwise, if magnitude of B > A:
				return (B.s ? -1 : 1); // B<A if B negative, B>A if B positive
			} else { // otherwise, if magnitude of B < A:
				return (B.s ? 1 : -1); // B>A if B negative, B<A if B positive
			}
		} else
		if (B.e > A.e) { // otherwise, if exponent of B > A:
			return (B.s ? -1 : 1); // B<A if B negative, B>A if B positive
		} else { // otherwise, if exponent of B < A
			return (B.s ? 1 : -1); // B>A if B negative, B<A if B positive
		}
	} else { // otherwise, if signs are different:
		return (A.s < B.s ? -1 : 1); // B<A if B negative, B>A if B positive
	}
}

// TODO:
//   should really implement the X register to be 100% correct,
//   right now we use a 9 bit extension which should be good enough for
//   rounding purposes :)
void singlePrecisionAdd(CPU *this, BIT add)
{
	NUM		A;	// extracted A register
	NUM		B;	// extracted B register

    this->cycleCount += 2;	// estimate some general overhead
    adjustABFull(this);
    this->r.AROF = 0;		// A is unconditionally marked empty

	// extract the numbers
	num_extract(&this->r.A, &A);
	num_extract(&this->r.B, &B);

#if DEBUG
	if (dotrcmat) {
		printf("After extraction:\n");
		num_printf(&A);
		num_printf(&B);
	}
#endif

	// if subtracting, complement sign of A
	if (!add)
		A.s ^= true;

	// trivial cases
	if (A.m == 0) {
		// result is already in B
		this->r.B &= MASK_NUMBER;
		return;
	}
	if (B.m == 0) {
		// result is in A
		num_compose(&A, &this->r.B);
		return;
	}

	// If the exponents are unequal, normalize the larger and scale the smaller
	// until they are in alignment
	if (A.e > B.e) {
		// Normalize A for 39 bits (13 octades)
		this->cycleCount += num_left_shift_exp(&A, B.e);
		// Scale B until its exponent matches (mantissa may go to zero)
		this->cycleCount += num_right_shift_exp(&B, A.e);
		// for display only
		this->r.X = B.x;
	} else if (A.e < B.e) {
		// Normalize B for 39 bits (13 octades)
		this->cycleCount += num_left_shift_exp(&B, A.e);
		// Scale A until its exponent matches (mantissa may go to zero)
		this->cycleCount += num_right_shift_exp(&A, B.e);
		// for display only
		this->r.X = A.x;
	}

	// At this point, the exponents are aligned,
	// so do the actual 48-bit additions of mantissas (and extensions)
#if DEBUG
	if (dotrcmat) {
		printf("After exponent alignment:\n");
		num_printf(&A);
		num_printf(&B);
	}
#endif

	// A and B have same sign?
	if (A.s == B.s) {
		// we really add!
		// add extensions first
		B.x += A.x;
		// add possible carry from extension add
		B.m += (B.x >> SHFT_MANTCARRY);
		// correct extension
		B.x &= MASK_MANTISSA;
		// now add mantissa
		B.m += A.m;
		// carry in mantissa add?
		if (B.m & MASK_MANTCARRY) {
			num_right_shift_cnt(&B, 1);
			// for display only
			this->r.X = B.x;
		}
	} else {
		// we must subtract and will do it unsigned, so:
		if (B.m > A.m) {
			// regular subtract
			// sub extension first
			B.x -= A.x;
			// borrow in extension sub?
			if (B.x & MASK_MANTCARRY) {
				// decrease mantissa
				B.m--;
				// correct extension
				B.x &= MASK_MANTISSA;
			}
			// now sub mantissa
			B.m -= A.m;
		} else {
			// inverse subtract
			// sub extension first
			B.x = A.x - B.x;
			// borrow in extension sub?
			if (B.x & MASK_MANTCARRY) {
				// decrease mantissa
				A.m--;
				// correct extension
				B.x &= MASK_MANTISSA;
			}
			// now sub mantissa
			B.m = A.m - B.m;
			B.s = !B.s;
		}
	}

#if DEBUG
	if (dotrcmat) {
		printf("After addition:\n");
		num_printf(&B);
	}
#endif

	// Normalize and round as necessary
	if (emode) {
		num_normalize(&B, -62);
	}
	num_round(&B);

#if DEBUG
	if (dotrcmat) {
		printf("After rounding:\n");
		num_printf(&B);
	}
#endif

	// Check for exponent overflow
	if (B.e > 63) {
		B.e &= 63;
		if (this->r.NCSF) {
			// signal overflow here
			// set I05/6/8: exponent-overflow
			this->r.I = (this->r.I & IRQ_MASKL) | IRQ_EXPO;
			signalInterrupt(this);
		}
	}
	// underflow cannot happen here

	num_compose(&B, &this->r.B);
	// for display only
	this->r.X = B.x;
}

/* Multiplies the contents of the A register to the B register, leaving the
 * result in B and invalidating A. A double-precision mantissa is developed and
 * then normalized and rounded
 */
void singlePrecisionMultiply(CPU *this)
{
	NUM			A;	// extracted A register
	NUM			B;	// extracted B register

	unsigned	d;	// current multiplier & shifting digit (octal)
	int			n;	// local copy of N (octade counter)

    this->cycleCount += 2;	// estimate some general overhead
    adjustABFull(this);
    this->r.AROF = 0;		// A is unconditionally marked empty

	// extract the numbers
	num_extract(&this->r.A, &A);
	num_extract(&this->r.B, &B);

#if DEBUG
	if (dotrcmat) {
		printf("After extraction:\n");
		num_printf(&A);
		num_printf(&B);
	}
#endif

	// trivial case, either operand is zero
	if ((A.m == 0) || (B.m == 0)) {
		// if A or B mantissa is zero
		this->r.B = 0;
		return;
	}

	// If the exponents are BOTH zero, perform an integer multiply.
	// Otherwise, normalize both operands
	if ((A.e == 0) && (B.e == 0)) {
		// integer multiply operation: set Q05F
		this->r.Q05F = true;
	} else {
		// Normalize A for 39 bits (13 octades)
		this->cycleCount += num_left_shift_exp(&A, -63);
		// Normalize B for 39 bits (13 octades)
		this->cycleCount += num_left_shift_exp(&B, -63);
	}

#if DEBUG
	if (dotrcmat) {
		printf("After normalization:\n");
		num_printf(&A);
		num_printf(&B);
	}
#endif

	// Determine resulting mantissa sign; initialize the product
	B.s ^= A.s;	// positive if signs are same, negative if different
	B.x = B.m;	// move multiplier to X
	B.m = 0;	// initialize high-order part of product

	// Now we step through the 13 octades of the multiplier, developing the product
	for (n=0; n<13; ++n) {
		// extract the current multiplier digit from X
		d = B.x & 7;
#if DEBUG
		if (dotrcmat) {
			printf("n=%d d=%u\n", n, d);
			num_printf(&B);
		}
#endif
		if (d == 0) {
			// if multiplier digit is zero
			// hardware optimizes this case
			this->cycleCount++;
		} else {
			// just estimate the average number of clocks
			this->cycleCount += 3;
			// develop the partial product
			B.m += A.m * d;
		}

		// Shift B & X together one octade to the right
		num_right_shift_cnt(&B, 1);
	} // for n
#if DEBUG
	if (dotrcmat) {
		printf("after loop\n");
		num_printf(&B);
	}
#endif

	// Normalize the result
	if (this->r.Q05F && (B.m == 0)) {
		// if it's integer multiply (Q05F) with integer result
		// just use the low-order 39 bits
		B.m = B.x;
		B.x = 0;
		// and don't normalize
		B.e = 0;
	} else {
		// compute resulting exponent from multiply
		B.e += A.e;
	}

#if DEBUG
	if (dotrcmat) {
		printf("after exponent calculation\n");
		num_printf(&B);
	}
#endif
	// Round the result
	this->r.Q05F = false;
	// required by specs due to the way rounding addition worked
	this->r.A = 0;

	// Normalize and round as necessary
	if (emode)
		num_normalize(&B, -62);
	num_round(&B);

	if (B.m == 0) {
		// don't see how this could be necessary here, but
		// the TM says to do it anyway
		this->r.B = 0;
	} else {
		// Check for exponent under/overflow
		if (B.e > 63) {
			B.e &= 63;
			if (this->r.NCSF) {
				// set I05/6/8: exponent-overflow
				this->r.I = (this->r.I & IRQ_MASKL) | IRQ_EXPO;
				signalInterrupt(this);
			}
		} else if (B.e < -63) {
			// mod the exponent
			B.e = -((-B.e) & 63);
			if (this->r.NCSF) {
				// set I06/8: exponent-underflow
				this->r.I = (this->r.I & IRQ_MASKL) | IRQ_EXPU;
				signalInterrupt(this);
			}
		}
	}
	num_compose(&B, &this->r.B);
	this->r.X = B.x;
}

/*
 * Divides the contents of the A register into the B register, leaving the
 * result in B and invalidating A. A 14-octade mantissa is developed and
 * then normalized and rounded
 */
void singlePrecisionDivide(CPU *this)
{
	NUM			A;	// extracted A register
	NUM			B;	// extracted B register
	unsigned	q;	// current quotient digit (octal)
	int		n=0;	// local copy of N (octade counter)

    this->cycleCount += 2;	// estimate some general overhead
    adjustABFull(this);
    this->r.AROF = 0;		// A is unconditionally marked empty

	// extract the numbers
	num_extract(&this->r.A, &A);
	num_extract(&this->r.B, &B);

#if DEBUG
	if (dotrcmat) {
		printf("After extraction:\n");
		num_printf(&A);
		num_printf(&B);
	}
#endif

	// divide by zero?
	if (A.m == 0) {
		// if A mantissa is zero
		// and we're in Normal State
		if (this->r.NCSF) {
			// set I05/7/8: divide by zero
			this->r.I = (this->r.I & IRQ_MASKL) | IRQ_DIVZ;
			signalInterrupt(this);
		}
		return;
	}
	// if B is zero...
	if (B.m == 0) {
		// ...result is all zeroes
		this->r.A = this->r.B = 0;
		return;
	}

	// otherwise, may the octades always be in your favor
	// Normalize A for 39 bits (13 octades)
	this->cycleCount += num_left_shift_exp(&A, -63);
	// Normalize B for 39 bits (13 octades)
	this->cycleCount += num_left_shift_exp(&B, -63);

	B.s ^= A.s;	// positive if signs are same, negative if different

#if DEBUG
	if (dotrcmat) {
		printf("After normalization:\n");
		num_printf(&A);
		num_printf(&B);
	}
#endif

	// Now we step through the development of the quotient one octade at a time,
	// tallying the shifts in n until the high-order octade of xx is non-zero (i.e.,
	// normalized). The divisor is in ma and the dividend (which becomes the
	// remainder) is in mb. Since the operands are normalized, this will take
	// either 13 or 14 shifts. We do the xx shift at the top of the loop so that
	// the 14th (rounding) digit will be available in q at the end. The initial
	// shift has no effect, as it operates using zero values for xx and q.
	B.x = 0;
	do {
		q = 0;					// initialize the quotient digit
		while (B.m >= A.m) {
			++q;				// bump the quotient digit
			B.m -= A.m;			// subtract divisor from remainder
		}
		if (B.x & MASK_MANTHIGH) {
			break;				// quotient has become normalized
		} else {
			++n;				// tally the shifts
			// shift the remainder left one octade
			// shift quotient digit into the working quotient
			num_left_shift(&B, q);
		}
#if DEBUG
		if (dotrcmat) {
			printf("q=%u n=%u\n", q, n);
			num_printf(&B);
			if (n > 14) break;
		}
#endif
	} while (true);

	B.m = B.x;

	// just estimate the average number of divide clocks
	this->cycleCount += n*3;

	B.e -= A.e - 1;			// compute the exponent, accounting for the shifts

	// Round the result (it's already normalized)
	this->r.A = 0;	// required by specs due to the way rounding addition worked
	B.x = 0;
	if (q >= 4) {	// if high-order bit of last quotient digit is 1
		this->r.Q01F = true;	// set Q01F (for display purposes only)
		B.x = MASK_MANTHBIT;	// round up the result
	}

	// Normalize and round as necessary
	if (emode) {
#if DEBUG
		if (dotrcmat) {
			printf("before normalization\n");
			num_printf(&B);
		}
#endif
		num_normalize(&B, -63);
	}
#if DEBUG
	if (dotrcmat) {
		printf("before rounding\n");
		num_printf(&B);
	}
#endif
	num_round(&B);

	// Check for exponent under/overflow
	if (B.e > 63) {
		B.e &= 63;
		if (this->r.NCSF) {
			// set I05/6/8: exponent-overflow
			this->r.I = (this->r.I & IRQ_MASKL) | IRQ_EXPO;
			signalInterrupt(this);
		}
	} else if (B.e < -63) {
		// mod the exponent
		B.e = -((-B.e) & 63);
		if (this->r.NCSF) {
			// set I06/8: exponent-underflow
			this->r.I = (this->r.I & IRQ_MASKL) | IRQ_EXPU;
			signalInterrupt(this);
		}
	}

	num_compose(&B, &this->r.B);
	this->r.X = B.x;
}

/*
 * Divides the contents of the A register into the B register, leaving the
 * integerized result in B and invalidating A. If the result cannot be expressed
 * as an integer, the Integer-Overflow interrupt is set
 */
void integerDivide(CPU *this)
{
	NUM			A;	// extracted A register
	NUM			B;	// extracted B register
	unsigned q=0;	// current quotient digit (octal)
	int		n=0;	// local copy of N (octade counter)

    this->cycleCount += 4;	// estimate some general overhead
    adjustABFull(this);
    this->r.AROF = 0;		// A is unconditionally marked empty

	// extract the numbers
	num_extract(&this->r.A, &A);
	num_extract(&this->r.B, &B);

#if DEBUG
	if (dotrcmat) {
		printf("After extraction:\n");
		num_printf(&A);
		num_printf(&B);
	}
#endif

	// divide by zero?
	if (A.m == 0) {
		// if A mantissa is zero
		// and we're in Normal State
		if (this->r.NCSF) {
			// set I05/7/8: divide by zero
			this->r.I = (this->r.I & IRQ_MASKL) | IRQ_DIVZ;
			signalInterrupt(this);
		}
		return;
	}
	// if B is zero...
	if (B.m == 0) {
		// ...result is all zeroes
		this->r.A = this->r.B = 0;
		return;
	}

	// Normalize A for 39 bits (13 octades)
	this->cycleCount += num_left_shift_exp(&A, -63);
	// Normalize B for 39 bits (13 octades)
	this->cycleCount += num_left_shift_exp(&B, -63);

	if (A.e > B.e) {
		// if divisor has greater magnitude
		// quotient is < 1, so set result to zero
		this->r.A = this->r.B = 0;
		return;
	}

	B.s ^= A.s;	// positive if signs are same, negative if different

#if DEBUG
	if (dotrcmat) {
		printf("After normalization:\n");
		num_printf(&A);
		num_printf(&B);
	}
#endif

	// Now we step through the development of the quotient one octade at a time,
	// similar to that for DIV, but in addition to stopping when the high-order
	// octade of xx is non-zero (i.e., normalized), we can stop if the exponents
	// become equal. Since there is no rounding, we do not need to develop an
	// extra quotient digit.
	B.x = 0;
	do {
		// just estimate the average number of clocks
		this->cycleCount += 3;
		// initialize the quotient digit
		q = 0;
		while (B.m >= A.m) {
			// bump the quotient digit
			++q;
			// subtract divisor from remainder
			B.m -= A.m;
		}
		// shift the remainder left one octade
		// shift quotient digit into the working quotient
		num_left_shift(&B, q);
		B.e++;	// correct back
		if (B.x & MASK_MANTHIGH) {
			// quotient has become normalized
			break;
		} else if (A.e < B.e) {
			// decrement the B exponent
			B.e--;
		} else {
			break;
		}
#if DEBUG
		if (dotrcmat) {
			printf("q=%u n=%u\n", q, n);
			num_printf(&B);
			if (n > 14) break;
		}
#endif
	} while (true);

	B.m = B.x;

#if DEBUG
	if (dotrcmat) {
		printf("after loop\n");
		num_printf(&B);
	}
#endif

	if (A.e == B.e) {
		// integer result developed
		B.e = 0;
	} else {
		if (this->r.NCSF) {
			// integer overflow result
			// set I07/8: integer-overflow
			this->r.I = (this->r.I & IRQ_MASKL) | IRQ_INTO;
			signalInterrupt(this);
		}
		B.e = (B.e-A.e) & 63;
	}

	this->r.A = 0;
	num_compose(&B, &this->r.B);
	this->r.X = B.x;
}

/*
 * Divides the contents of the A register into the B register, leaving the
 * remainder result in B and invalidating A. The sign of the result is the sign
 * of the dividend (B register value). If the quotient cannot be expressed as an
 * integer, the Integer-Overflow interrupt is set
 */
void remainderDivide(CPU *this)
{
	NUM			A;	// extracted A register
	NUM			B;	// extracted B register
	unsigned q=0;	// current quotient digit (octal)
	int		n=0;	// local copy of N (octade counter)
	BIT			af;	// A was float number

    this->cycleCount += 4;	// estimate some general overhead
    adjustABFull(this);
    this->r.AROF = 0;		// A is unconditionally marked empty

	// extract the numbers
	num_extract(&this->r.A, &A);
	num_extract(&this->r.B, &B);

#if DEBUG
	if (dotrcmat) {
		printf("After extraction:\n");
		num_printf(&A);
		num_printf(&B);
	}
#endif
	// needed for emode normalization
	af = (A.e != 0) || (B.e != 0);

	// divide by zero?
	if (A.m == 0) {
		// if A mantissa is zero
		// and we're in Normal State
		if (this->r.NCSF) {
			// set I05/7/8: divide by zero
			this->r.I = (this->r.I & IRQ_MASKL) | IRQ_DIVZ;
			signalInterrupt(this);
		}
		return;
	}
	// if B is zero...
	if (B.m == 0) {
		// ...result is all zeroes
		this->r.A = this->r.B = 0;
		return;
	}

	// Normalize A for 39 bits (13 octades)
	this->cycleCount += num_left_shift_exp(&A, -63);
	// Normalize B for 39 bits (13 octades)
	this->cycleCount += num_left_shift_exp(&B, -63);

	if (A.e > B.e) {
		// if divisor has greater magnitude
		// quotient is < 1, so set A to zero and
		this->r.A = 0;
		// result is original B (less the flag bit)
		goto normalize;
	}

	B.s ^= A.s;	// positive if signs are same, negative if different

#if DEBUG
	if (dotrcmat) {
		printf("After normalization:\n");
		num_printf(&A);
		num_printf(&B);
	}
#endif

	// Now we step through the development of the quotient one octade at a time,
	// similar to that for DIV, but in addition to stopping when the high-order
	// octade of xx is non-zero (i.e., normalized), we can stop if the exponents
	// become equal. Since there is no rounding, we do not need to develop an
	// extra quotient digit.
	B.x = 0;
	do {
		// just estimate the average number of clocks
		this->cycleCount += 3;
		// initialize the quotient digit
		q = 0;
		while (B.m >= A.m) {
			// bump the quotient digit
			++q;
			// subtract divisor from remainder
			B.m -= A.m;
		}
		// shift quotient digit into the working quotient
		B.x = (B.x << 3) | q;
		if (B.x & MASK_MANTHIGH) {
			// quotient has become normalized
			break;
		} else if (A.e < B.e) {
			// shift the remainder left one octade
			B.m <<= 3;
			// decrement the B exponent
			B.e--;
		} else {
			break;
		}
#if DEBUG
		if (dotrcmat) {
			printf("q=%u n=%u\n", q, n);
			num_printf(&B);
			if (n > 14) break;
		}
#endif
	} while (true);

#if DEBUG
	if (dotrcmat) {
		printf("after loop\n");
		num_printf(&B);
	}
#endif

	// check for exponent underflow
	if (B.e < -63) {
		// if so, exponent is mod 64
		B.e = -(-B.e & 63);
		if (this->r.NCSF) {
			// set I06/8: exponent-underflow
			this->r.I = (this->r.I & IRQ_MASKL) | IRQ_EXPU;
			signalInterrupt(this);
		}
	} else if (A.e == B.e) {
		// integer result developed
		if (B.m == 0) {
			// if B mantissa is zero, then
			// assure result will be all zeroes
			B.e = 0;
		} else {
			// use remainder exponent mod 64
			B.e %= 64;
		}
	} else {
		if (this->r.NCSF) {
			// integer overflow result
			// set I07/8: integer-overflow
			this->r.I = (this->r.I & IRQ_MASKL) | IRQ_INTO;
			signalInterrupt(this);
		}
		// result in B will be all zeroes
		B.m = 0;
		B.e = 0;
	}

	// Normalize and round as necessary
normalize:
	if (emode) {
		B.x = 0;
#if DEBUG
		if (dotrcmat) {
			printf("before normalization\n");
			num_printf(&B);
		}
#endif
		if (af)
			num_left_shift_exp(&B, -63);
		else
			num_right_shift_exp(&B, 0);
	}

	this->r.A = 0;
	num_compose(&B, &this->r.B);
	this->r.X = B.x;
}
