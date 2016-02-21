/***********************************************************************
* b5500emulator
************************************************************************
* Copyright (c) 2016, Reinhard Meyer, DL5UY
* Licensed under the MIT License,
*       see LICENSE
* based on (C) work by Nigel Williams and Paul Kimpel
* see: https://github.com/pkimpel/retro-b5500
************************************************************************
* memory related functions
************************************************************************
* 2016-02-1921  R.Meyer
*   Converted Paul's work from Javascript to C
***********************************************************************/

#include "b5500_common.h"

void fetch(ACCESSOR *acc)
{
}

void store(ACCESSOR *acc)
{
}

void accessError(CPU *this)
{
}

void computeRelativeAddr(CPU *this, int x, int y)
{
}

void loadAviaS(CPU *this)
{
	this->r.E = 2;		// Just to show the world what's happening
	this->acc.addr = this->r.S;
	this->acc.MAIL = (this->r.S < 0x0200) && this->r.NCSF;
	fetch(&this->acc);
	//this->cycleCount += B5500CentralControl.memReadCycles;
	if (this->acc.MAED || this->acc.MPED) {
		accessError(this);
	} else {
		this->r.A = this->acc.word;
		this->r.AROF = 1;
	}
}

void loadBviaS(CPU *this)
{
	this->r.E = 3;		// Just to show the world what's happening
	this->acc.addr = this->r.S;
	this->acc.MAIL = (this->r.S < 0x0200) && this->r.NCSF;
	fetch(&this->acc);
	//this->cycleCount += B5500CentralControl.memReadCycles;
	if (this->acc.MAED || this->acc.MPED) {
		accessError(this);
	} else {
		this->r.B = this->acc.word;
		this->r.BROF = 1;
	}
}

void loadAviaM(CPU *this)
{
	this->r.E = 4;		// Just to show the world what's happening
	this->acc.addr = this->r.M;
	this->acc.MAIL = (this->r.M < 0x0200) && this->r.NCSF;
	fetch(&this->acc);
	//this->cycleCount += B5500CentralControl.memReadCycles;
	if (this->acc.MAED || this->acc.MPED) {
		accessError(this);
	} else {
		this->r.A = this->acc.word;
		this->r.AROF = 1;
	}
}

void loadBviaM(CPU *this)
{
	this->r.E = 5;		// Just to show the world what's happening
	this->acc.addr = this->r.M;
	this->acc.MAIL = (this->r.M < 0x0200) && this->r.NCSF;
	fetch(&this->acc);
	//this->cycleCount += B5500CentralControl.memReadCycles;
	if (this->acc.MAED || this->acc.MPED) {
		accessError(this);
	} else {
		this->r.B = this->acc.word;
		this->r.BROF = 1;
	}
}

void loadMviaM(CPU *this)
{
	this->r.E = 6;		// Just to show the world what's happening
	this->acc.addr = this->r.M;
	this->acc.MAIL = (this->r.M < 0x0200) && this->r.NCSF;
	fetch(&this->acc);
	//this->cycleCount += B5500CentralControl.memReadCycles;
	if (this->acc.MAED || this->acc.MPED) {
		accessError(this);
	} else {
		this->r.M = this->acc.word;
	}
}

void loadPviaC(CPU *this)
{
	this->r.E = 48;		// Just to show the world what's happening
	this->acc.addr = this->r.C;
	this->acc.MAIL = (this->r.C < 0x0200) && this->r.NCSF;
	fetch(&this->acc);
	this->r.PROF = 1;
	//this->cycleCount += B5500CentralControl.memReadCycles;
	if (this->acc.MAED || this->acc.MPED) {
		accessError(this);
	} else {
		this->r.P = this->acc.word;
	}
}

void storeAviaS(CPU *this)
{
	this->r.E = 10;		// Just to show the world what's happening
	this->acc.addr = this->r.S;
	this->acc.MAIL = (this->r.S < 0x0200) && this->r.NCSF;
	this->acc.word = this->r.A;
	store(&this->acc);
	this->r.PROF = 1;
	//this->cycleCount += B5500CentralControl.memReadCycles;
	if (this->acc.MAED || this->acc.MPED) {
		accessError(this);
	}
}

void storeBviaS(CPU *this)
{
	this->r.E = 11;		// Just to show the world what's happening
	this->acc.addr = this->r.S;
	this->acc.MAIL = (this->r.S < 0x0200) && this->r.NCSF;
	this->acc.word = this->r.B;
	store(&this->acc);
	this->r.PROF = 1;
	//this->cycleCount += B5500CentralControl.memReadCycles;
	if (this->acc.MAED || this->acc.MPED) {
		accessError(this);
	}
}

void storeAviaM(CPU *this)
{
	this->r.E = 12;		// Just to show the world what's happening
	this->acc.addr = this->r.M;
	this->acc.MAIL = (this->r.M < 0x0200) && this->r.NCSF;
	this->acc.word = this->r.A;
	store(&this->acc);
	this->r.PROF = 1;
	//this->cycleCount += B5500CentralControl.memReadCycles;
	if (this->acc.MAED || this->acc.MPED) {
		accessError(this);
	}
}

void storeBviaM(CPU *this)
{
	this->r.E = 12;		// Just to show the world what's happening
	this->acc.addr = this->r.M;
	this->acc.MAIL = (this->r.M < 0x0200) && this->r.NCSF;
	this->acc.word = this->r.B;
	store(&this->acc);
	this->r.PROF = 1;
	//this->cycleCount += B5500CentralControl.memReadCycles;
	if (this->acc.MAED || this->acc.MPED) {
		accessError(this);
	}
}

void integerStore(CPU *this, int conditional, int destructive)
{
}

