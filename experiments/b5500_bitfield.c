/***********************************************************************
* b5500emulator
************************************************************************
* Copyright (c)	2016, Reinhard Meyer, DL5UY
* Licensed under the MIT License,
*	see LICENSE
* based	on (C) work by Nigel Williams and Paul Kimpel
* see: https://github.com/pkimpel/retro-b5500
************************************************************************
* misc and CPU control
************************************************************************
* 2016-02-1921	R.Meyer
*   Converted Paul's work from Javascript to C
* 2017-07-17  R.Meyer
*   Added "long	long" qualifier	to constants with long long value
***********************************************************************/

#include <stdio.h>
#include "b5500_common.h"

/*
 * Note: Bits are numbered 0..47 from most significant to least	significant!
 */

/*
 * bitmasks for	0..48 bits
 */
const WORD48 bitmask[49] = {0ll,
	01ll, 03ll, 07ll,
	017ll, 037ll, 077ll,
	0177ll,	0377ll,	0777ll,
	01777ll, 03777ll, 07777ll,
	017777ll, 037777ll, 077777ll,
	0177777ll, 0377777ll, 0777777ll,
	01777777ll, 03777777ll,	07777777ll,
	017777777ll, 037777777ll, 077777777ll,
	0177777777ll, 0377777777ll, 0777777777ll,
	01777777777ll, 03777777777ll, 07777777777ll,
	017777777777ll,	037777777777ll,	077777777777ll,
	0177777777777ll, 0377777777777ll, 0777777777777ll,
	01777777777777ll, 03777777777777ll, 07777777777777ll,
	017777777777777ll, 037777777777777ll, 077777777777777ll,
	0177777777777777ll, 0377777777777777ll,	0777777777777777ll,
	01777777777777777ll, 03777777777777777ll, 07777777777777777ll};

/*
 * Inserts a bit field from value.[vstart:width] into word.[wstart:width]
 * and returns the updated word
 */
void fieldTransfer(
	WORD48 *dest,		// word	to insert into
	unsigned wstart,	// starting bit	in that	word
	unsigned width,		// number of bits
	WORD48 value,		// value to insert
	unsigned vstart)	// starting bit	in the value
{
	WORD48	temp;

	temp = fieldIsolate(value, vstart, width);
	*dest =	fieldInsert(*dest, wstart, width, temp);
}

WORD48 fieldIsolate(
	WORD48 word,		// value to isolate from
	unsigned start,		// starting bit	in the value
	unsigned width)		// number of bits
{
	int rbitpos;		// rightmost bit position
	WORD48	res;

	rbitpos	= 48-start-width;
	res = (word >> rbitpos)	& bitmask[width];
//	printf("fieldIsolate: w=%016llo	s=%u w=%u -> %016llo\n",
//		word, start, width, res);
	return res;
}


WORD48 fieldInsert(
	WORD48 word,
	unsigned start,
	unsigned width,
	WORD48 value)
{
	int rbitpos;		// rightmost bit position
	WORD48	res;

	rbitpos	= 48-start-width;
	res = (word & ~(bitmask[width] << rbitpos)) |
		((value	& bitmask[width]) << rbitpos);
//	printf("fieldInsert: w=%016llo s=%u w=%u v=%016llo -> %016llo\n",
//		word, start, width, value, res);
	return res;
}


void bitSet(
	WORD48 *dest,
	unsigned bit)
{
	*dest |= 1ll <<	(48-bit);
}


void bitReset(
	WORD48 *dest,
	unsigned bit)
{
	*dest &= ~(1ll << (48-bit));
}

BIT bitTest(
	WORD48 src,
	unsigned bit)
{
	return src & (1ll << (48-bit)) ? true :	false;
}
