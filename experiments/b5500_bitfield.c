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

#include "b5500_common.h"

extern void fieldTransfer(
	WORD48 *dest,		// word to insert into
	unsigned wstart,	// starting bit in that word
	unsigned width,		// number of bits
	WORD48 value,		// value to insert
	unsigned vstart)	// starting bit in the value
{
}


extern WORD48 fieldIsolate(
	WORD48 word,		// value to isolate from
	unsigned start,		// starting bit in the value
	unsigned width)		// number of bits
{
	return 0;
}


extern WORD48 fieldInsert(
	WORD48 word,
	unsigned start,
	unsigned width,
	WORD48 value)
{
	return 0;
}


extern void bitSet(
	WORD48 *dest,
	unsigned bit)
{
}


extern void bitReset(
	WORD48 *dest,
	unsigned bit)
{
}

