/***********************************************************************
* b5500emulator
************************************************************************
* Copyright (c) 2016, Reinhard Meyer, DL5UY
* Licensed under the MIT License,
*       see LICENSE
* based on (C) work by Nigel Williams and Paul Kimpel
* see: https://github.com/pkimpel/retro-b5500
************************************************************************
* misc and CPU control
************************************************************************
* 2016-02-1921  R.Meyer
*   Converted Paul's work from Javascript to C
***********************************************************************/

#include <stdio.h>
#include "b5500_common.h"

/*
 * Note: Bits are numbered 0..47 from most significant to least significant!
 */

/*
 * bitmasks for 0..48 bits
 */
const WORD48 bitmask[49] = {0,
	01, 03, 07,
	017, 037, 077,
	0177, 0377, 0777,
	01777, 03777, 07777,
	017777, 037777, 077777,
	0177777, 0377777, 0777777,
	01777777, 03777777, 07777777,
	017777777, 037777777, 077777777,
	0177777777, 0377777777, 0777777777,
	01777777777, 03777777777, 07777777777,
	017777777777, 037777777777, 077777777777,
	0177777777777, 0377777777777, 0777777777777,
	01777777777777, 03777777777777, 07777777777777,
	017777777777777, 037777777777777, 077777777777777,
	0177777777777777, 0377777777777777, 0777777777777777,
	01777777777777777, 03777777777777777, 07777777777777777};

/*
 * Inserts a bit field from value.[vstart:width] into word.[wstart:width]
 * and returns the updated word
 */
void fieldTransfer(
	WORD48 *dest,		// word to insert into
	unsigned wstart,	// starting bit in that word
	unsigned width,		// number of bits
	WORD48 value,		// value to insert
	unsigned vstart)	// starting bit in the value
{
	WORD48	temp;

	temp = fieldIsolate(value, vstart, width);
	*dest = fieldInsert(*dest, wstart, width, temp);
}

WORD48 fieldIsolate(
	WORD48 word,		// value to isolate from
	unsigned start,		// starting bit in the value
	unsigned width)		// number of bits
{
	int rbitpos;		// rightmost bit position
	WORD48	res;

	rbitpos = 48-start-width;
	res = (word >> rbitpos) & bitmask[width];
	printf("fieldIsolate: w=%016llo s=%u w=%u -> %016llo\n",
		word, start, width, res); 
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

	rbitpos = 48-start-width;
	res = (word & ~(bitmask[width] << rbitpos)) |
		((value & bitmask[width]) << rbitpos); 
	printf("fieldInsert: w=%016llo s=%u w=%u v=%016llo -> %016llo\n",
		word, start, width, value, res); 
	return res;
}


void bitSet(
	WORD48 *dest,
	unsigned bit)
{
	*dest |= 1ll << (48-bit);
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
	return src & (1ll << (48-bit)) ? true : false;
}
