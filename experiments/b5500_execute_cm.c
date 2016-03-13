/***********************************************************************
* b5500emulator
************************************************************************
* Copyright (c) 2016, Reinhard Meyer, DL5UY
* Licensed under the MIT License,
*       see LICENSE
* based on (C) work by Nigel Williams and Paul Kimpel
* see: https://github.com/pkimpel/retro-b5500
************************************************************************
* execute one character mode instruction
************************************************************************
* 2016-02-27  R.Meyer
*   Converted Paul's work from Javascript to C
***********************************************************************/

#include "b5500_common.h"

/***********************************************************
*  Character Mode Syllables                                *
***********************************************************/
void b5500_execute_cm(CPU *this)
{
#if 0
	WORD12 opcode = this->r.T;
	WORD12 variant;
	WORD48 t1, t2;

	// clear some vars 
	this->r.Q01F = 0;
	this->r.Q02F = 0;
	this->r.Q03F = 0;
	this->r.Q04F = 0;
	this->r.Q05F = 0;
	this->r.Q06F = 0;
	this->r.Q07F = 0;
	this->r.Q08F = 0;
	this->r.Q09F = 0;
	this->r.Q12F = 0;
	this->r.Y = 0;
	this->r.Z = 0;
	this->r.M = 0;
	this->r.N = 0;
	this->r.X = 0;
#endif
}
