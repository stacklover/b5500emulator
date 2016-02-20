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
	cw = (A & MASK_CONTROLW) >> SHFT_CONTROLW;

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

int main(int argc, char *argv[])
{
	WORD48	testa, testb;
	int	i;
	int	sub = 0;
	CPU	*this;

	if (argc < 3) {
		printf("needs 2 octal B5500 numbers\n");
		exit(2);
	}

	b5500_init_shares();

	testa = strtoull(argv[1], NULL, 8);
	testb = strtoull(argv[2], NULL, 8);
	if (argc > 3)
		sub = atoi(argv[3]);

	printf("testa=");
	b5500_sp_print(testa);
	printf("\n");
	printf("testb=");
	b5500_sp_print(testb);
	printf("\n");

	this = CPUA;

	this->r.NCSF = 1;
	this->r.I = 0;

	this->r.A = testa;
	this->r.B = testb;
	this->r.AROF = this->r.BROF = 1;
	i = b5500_sp_compare(this);
	printf("sp_compare='%d'\n\n", i);

#if 1
	this->r.A = testa;
	this->r.B = testb;
	this->r.AROF = this->r.BROF = 1;
	b5500_sp_addsub(this, sub);
#else
	memset(&(this->r), 0, sizeof(CPUREGS));
	this->r.S = 2;
	while (!b5500_sp_addsub2(this)) {
		sleep(1);
		// fill register request?
		this->r.MROF = 0;
		switch (this->r.E) {
		case 2:
			this->r.A = testa;
			sleep(10);
			this->r.E = 63;
			this->r.MROF = 1;
			break;
		case 3:
			this->r.B = testb;
			sleep(10);
			this->r.E = 63;
			this->r.MROF = 1;
			break;
		}
	}
#endif

	printf("\nsp_add=");
	b5500_sp_print(this->r.B);
	printf("\n\n");
	
	return 0;
}
