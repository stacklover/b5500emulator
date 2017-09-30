/***********************************************************************
* b5500emulator
************************************************************************
* Copyright (c) 2016, Reinhard Meyer, DL5UY
* Licensed under the MIT License,
*       see LICENSE
* based on work by Nigel Williams and Paul Kimpel
* see: https://github.com/pkimpel/retro-b5500
************************************************************************
* main program for tests
************************************************************************
* 2016-02-13  R.Meyer
*   from thin air.
***********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <memory.h>
#include <math.h>
#include <errno.h>
#include <unistd.h>
#include "b5500_common.h"

void b5500_sp_print(WORD48 A)
{
	int	e;	// exponent
	WORD48	m;	// mantissa
	BIT	se;	// mantissa sign
	BIT	sm;	// exponent sign
	BIT	cw;	// control word
	double	d;	// number in host format

	m = (A & MASK_MANTISSA) >> SHFT_MANTISSA;
	e = (A & MASK_EXPONENT) >> SHFT_EXPONENT;
	sm = (A & MASK_SIGNMANT) >> SHFT_SIGNMANT;
	se = (A & MASK_SIGNEXPO) >> SHFT_SIGNEXPO;
	cw = (A & MASK_FLAG) >> SHFT_FLAG;

	d = (sm ? -1.0 : 1.0) * (double)m * pow(8.0, (se ? -1.0 : 1.0) * (double)e);

	printf("%c%llu&%c%u --> %16.3f",
		sm ? '-' : '+',
		m,
		se ? '-' : '+',
		e,
		d);
	if (cw)
		printf(" CONTROL WORD!");
}

void signalInterrupt(CPU *this)
{
	printf("\nIRQ=$%02x\n", this->r.I);
}

CPU *this;
int dotrcmem = 0;
int dotrcins = 0;

int main(int argc, char *argv[])
{
	int	i;
	int	sub = 0;

	b5500_init_shares();

	this = CPUA;
	memset(this, 0, sizeof(CPU));
	this->id = "CPUA";
	start(this);
	preset(this, 020);
	this->r.US14X = true;
	while (this->busy) {
		this->cycleLimit = 1;
		run(this);
		sleep(10);
	}
	return 0;
}
