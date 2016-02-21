/***********************************************************************
* b5500emulator
************************************************************************
* Copyright (c) 2016, Reinhard Meyer, DL5UY
* Licensed under the MIT License,
*       see LICENSE
* based on (C) work by Nigel Williams and Paul Kimpel
* see: https://github.com/pkimpel/retro-b5500
************************************************************************
* IRQ and I/O
************************************************************************
* 2016-02-1921  R.Meyer
*   Converted Paul's work from Javascript to C
***********************************************************************/

#include "b5500_common.h"

int presenceTest(CPU *this, WORD48 value)
{
	return 0;
}

int interrogateUnitStatus(CPU *this)
{
	return 0;
}

int interrogateIOChannel(CPU *this)
{
	return 0;
}

void storeForInterrupt(CPU *this, int x, int y)
{
}

void clearInterrupt(CPU *this)
{
}

void initiateIO(CPU *this)
{
}

