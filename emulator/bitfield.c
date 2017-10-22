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
* 2017-07-17  R.Meyer
*   Added "long long" qualifier to constants with long long value
* 2017-09-30  R.Meyer
*   overhaul of file names
***********************************************************************/

#include <stdio.h>
#include "common.h"

/***********************************************************************
* Note: Bits are numbered 0..47 from most significant to least significant!
***********************************************************************/

/***********************************************************************
* bitmasks for 0..48 bits
***********************************************************************/
const WORD48 bitmask[49] = {0LL,
	01LL, 03LL, 07LL,
	017LL, 037LL, 077LL,
	0177LL, 0377LL, 0777LL,
	01777LL, 03777LL, 07777LL,
	017777LL, 037777LL, 077777LL,
	0177777LL, 0377777LL, 0777777LL,
	01777777LL, 03777777LL, 07777777LL,
	017777777LL, 037777777LL, 077777777LL,
	0177777777LL, 0377777777LL, 0777777777LL,
	01777777777LL, 03777777777LL, 07777777777LL,
	017777777777LL, 037777777777LL, 077777777777LL,
	0177777777777LL, 0377777777777LL, 0777777777777LL,
	01777777777777LL, 03777777777777LL, 07777777777777LL,
	017777777777777LL, 037777777777777LL, 077777777777777LL,
	0177777777777777LL, 0377777777777777LL, 0777777777777777LL,
	01777777777777777LL, 03777777777777777LL, 07777777777777777LL};

/***********************************************************************
* Inserts a bit field from value.[vstart:width] into word.[wstart:width]
***********************************************************************/
void fieldTransfer(
	WORD48 *dest,           // word to insert into
	unsigned wstart,        // starting bit in that word
	unsigned width,         // number of bits
	WORD48 value,           // value to insert
	unsigned vstart) {       // starting bit in the value
        WORD48 temp = fieldIsolate(value, vstart, width);
	if (wstart + width > 48 || vstart + width > 48) {
		printf("*\tfieldTransfer width=%u wstart=%u vstart=%u\n",
			width, wstart, vstart);
	}
        *dest = fieldInsert(*dest, wstart, width, temp);
}

/***********************************************************************
* Returns a bit field from value.[start:width] right justified
***********************************************************************/
WORD48 fieldIsolate(
	WORD48 word,            // value to isolate from
	unsigned start,         // starting bit in the value
	unsigned width) {         // number of bits
	unsigned rbitpos = 48-start-width;	// rightmost bit position
	if (start + width > 48) {
		printf("*\tfieldIsolate width=%u start=%u\n",
			width, start);
	}
	return (word >> rbitpos) & bitmask[width];
}

/***********************************************************************
* Inserts a bit field into word.[start:width] from value
* and returns the updated word
***********************************************************************/
WORD48 fieldInsert(
	WORD48 word,
	unsigned start,
	unsigned width,
	WORD48 value) {
	unsigned rbitpos = 48-start-width;	// rightmost bit position
	if (start + width > 48) {
		printf("*\tfieldInsert width=%u start=%u\n",
			width, start);
	}
	return (word & ~(bitmask[width] << rbitpos)) |
		((value & bitmask[width]) << rbitpos);
}

/***********************************************************************
* Sets a bit in the word
***********************************************************************/
void bitSet(WORD48 *dest, unsigned bit) {
	if (bit > 48) {
		printf("*\tbitSet bit=%u\n", bit);
	}
        *dest |= 1LL << (48-bit);
}

/***********************************************************************
* Resets a bit in the word
***********************************************************************/
void bitReset(WORD48 *dest, unsigned bit) {
	if (bit > 48) {
		printf("*\tbitSet bit=%u\n", bit);
	}
        *dest &= ~(1LL << (48-bit));
}

/***********************************************************************
* Tests a bit in the word
***********************************************************************/
BIT bitTest(WORD48 src, unsigned bit) {
	if (bit > 48) {
		printf("*\tbitSet bit=%u\n", bit);
	}
        return src & (1LL << (48-bit)) ? true : false;
}


