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
#include <math.h>
#include "b5500_common.h"

void b5500_sp_print(word48 A)
{
	int	e;	// exponent
	word48	m;	// mantissa
	bit	se;	// mantissa sign
	bit	sm;	// exponent sign
	bit	cw;	// control word
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

int main(int argc, char *argv[])
{
	word48	testa, testb, testc;
	int	i;
	int	sub = 0;

	if (argc < 3) {
		printf("needs 2 octal B5500 numbers\n");
		exit(2);
	}

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

	i = b5500_sp_compare(testa, testb);
	printf("sp_compare='%d'\n\n", i);

	testc = b5500_sp_addsub(testa, testb, sub);

	printf("\nsp_add=");
	b5500_sp_print(testc);
	printf("\n\n");
	
	return 0;
}
