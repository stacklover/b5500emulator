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
#include <errno.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include "b5500_common.h"

int	id_main,
	id_cpua,
	id_cpub;

WORD48	*MAIN;
CPU	*CPUA;
CPU	*CPUB;

void b5500_init_shares(void)
{
	id_main = shmget(SHM_MAIN, MAXMEM*sizeof(WORD48), IPC_CREAT|0644);
	if (id_main < 0) {
		perror("shmget MAIN");
		exit(2);
	}
	id_cpua = shmget(SHM_CPUA, sizeof(CPU), IPC_CREAT|0644);
	if (id_cpua < 0) {
		perror("shmget CPUA");
		exit(2);
	}
	id_cpub = shmget(SHM_CPUB, sizeof(CPU), IPC_CREAT|0644);
	if (id_cpub < 0) {
		perror("shmget CPUB");
		exit(2);
	}
	MAIN = shmat(id_main, NULL, 0);
	if ((int)MAIN == -1) {
		perror("shmat MAIN");
		exit(2);
	}
	CPUA = shmat(id_cpua, NULL, 0);
	if ((int)CPUA == -1) {
		perror("shmat CPUA");
		exit(2);
	}
	CPUB = shmat(id_cpub, NULL, 0);
	if ((int)CPUB == -1) {
		perror("shmat CPUB");
		exit(2);
	}
}

void b5500_blinky_lights(CPU *this)
{
	int i;
	//       * * * * * * * * * * * * * * * * * * * * * * * * * * *
	printf(".............X..............J........Q.........R...\n");

	printf("'");
	for (i=38; i>=0; i-=3) {
		printf("%u", (int)(this->r.X >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("'%u %u'", (this->r.J >> 3) & 1, (this->r.J >> 2) & 1);
	for (i=20; i>=0; i-=3) {
		printf("%u", (int)(this->r.Q >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("'");
	for (i=8; i>=0; i-=3) {
		printf("%u", (this->r.R >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("'\n");

	printf(" ");
	for (i=37; i>=0; i-=3) {
		printf("%u", (int)(this->r.X >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("   %u ", (this->r.J >> 1) & 1);
	for (i=19; i>=0; i-=3) {
		printf("%u", (int)(this->r.Q >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf(" ");
	for (i=7; i>=0; i-=3) {
		printf("%u", (this->r.R >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("\n");

	printf(" ");
	for (i=36; i>=0; i-=3) {
		printf("%u", (int)(this->r.X >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("   %u ", (this->r.J >> 0) & 1);
	for (i=18; i>=0; i-=3) {
		printf("%u", (int)(this->r.Q >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf(" ");
	for (i=6; i>=0; i-=3) {
		printf("%u", (this->r.R >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("\n");

	//       * * * * * * * * * * * * * * * * * * * * * * * * * * *
	printf("................A................o.G.H. .Y.Z.....M.....\n");

	printf("'");
	for (i=47; i>=0; i-=3) {
		printf("%u", (int)(this->r.A >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("'%u'%u'%u' '%u'%u'", this->r.AROF,
		(this->r.G >> 2) & 1, (this->r.H >> 2) & 1,
		(this->r.Y >> 5) & 1, (this->r.Z >> 5) & 1);
	for (i=14; i>=0; i-=3) {
		printf("%u", (this->r.M >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("'\n");

	printf(" ");
	for (i=46; i>=0; i-=3) {
		printf("%u", (int)(this->r.A >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("   %u %u...%u %u ",
		(this->r.G >> 1) & 1, (this->r.H >> 1) & 1,
		(this->r.Y >> 4) & 1, (this->r.Z >> 4) & 1);
	for (i=13; i>=0; i-=3) {
		printf("%u", (this->r.M >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("\n");

	printf(" ");
	for (i=45; i>=0; i-=3) {
		printf("%u", (int)(this->r.A >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("   %u %u'N'    ",
		(this->r.G >> 0) & 1, (this->r.H >> 0) & 1);
	for (i=12; i>=0; i-=3) {
		printf("%u", (this->r.M >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("\n");

	//       * * * * * * * * * * * * * * * * * * * * * * * * * * *
	printf("................B................o.K.V.%u %u %u.....S.....\n",
		(this->r.N >> 3) & 1,
		(this->r.Y >> 3) & 1, (this->r.Z >> 3) & 1);

	printf("'");
	for (i=47; i>=0; i-=3) {
		printf("%u", (int)(this->r.B >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("'%u'%u'%u'%u %u %u'", this->r.BROF,
		(this->r.K >> 2) & 1, (this->r.V >> 2) & 1,
		(this->r.N >> 2) & 1,
		(this->r.Y >> 2) & 1, (this->r.Z >> 2) & 1);
	for (i=14; i>=0; i-=3) {
		printf("%u", (this->r.S >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("'\n");

	printf(" ");
	for (i=46; i>=0; i-=3) {
		printf("%u", (int)(this->r.B >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("   %u %u %u %u %u ",
		(this->r.K >> 1) & 1, (this->r.V >> 1) & 1,
		(this->r.N >> 1) & 1,
		(this->r.Y >> 1) & 1, (this->r.Z >> 1) & 1);
	for (i=13; i>=0; i-=3) {
		printf("%u", (this->r.S >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("\n");

	printf(" ");
	for (i=45; i>=0; i-=3) {
		printf("%u", (int)(this->r.B >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("   %u %u %u %u %u ",
		(this->r.K >> 0) & 1, (this->r.V >> 0) & 1,
		(this->r.N >> 0) & 1,
		(this->r.Y >> 0) & 1, (this->r.Z >> 0) & 1);
	for (i=12; i>=0; i-=3) {
		printf("%u", (this->r.S >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("\n");

	//       * * * * * * * * * * * * * * * * * * * * * * * * * * *
	printf("................P................o....T....o.....C.....\n");

	printf("'");
	for (i=47; i>=0; i-=3) {
		printf("%u", (int)(this->r.P >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("'%u'", this->r.PROF);
	for (i=11; i>=0; i-=3) {
		printf("%u", (this->r.T >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("'%u'", this->r.TROF);
	for (i=14; i>=0; i-=3) {
		printf("%u", (this->r.C >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("'\n");

	printf(" ");
	for (i=46; i>=0; i-=3) {
		printf("%u", (int)(this->r.P >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("   ");
	for (i=10; i>=0; i-=3) {
		printf("%u", (this->r.T >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("   ");
	for (i=13; i>=0; i-=3) {
		printf("%u", (this->r.C >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("\n");

	printf(" ");
	for (i=45; i>=0; i-=3) {
		printf("%u", (int)(this->r.P >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("   ");
	for (i=9; i>=0; i-=3) {
		printf("%u", (this->r.T >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("   ");
	for (i=12; i>=0; i-=3) {
		printf("%u", (this->r.C >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("\n");

	//       * * * * * * * * * * * * * * * * * * * * * * * * * * *
	printf("    ........I........           ......E..........F.....\n");
	printf("    '");
	for (i=7; i>=0; i-=1) {
		printf("%u", (this->r.I >> i) & 1);
		if (i>=1) printf(" ");
	}
	printf("'           '");
	for (i=5; i>=0; i-=1) {
		printf("%u", (this->r.E >> i) & 1);
		if (i>=1) printf(" ");
	}
	printf("'");
	for (i=14; i>=0; i-=3) {
		printf("%u", (this->r.F >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("'\n");

	printf("                                             ");
	for (i=13; i>=0; i-=3) {
		printf("%u", (this->r.F >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("\n");

	printf("                                             ");
	for (i=12; i>=0; i-=3) {
		printf("%u", (this->r.F >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("\n");
}

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

	this->r.A = testa;
	this->r.B = testb;
	this->r.AROF = this->r.BROF = 1;
	b5500_sp_addsub(this, sub);

	printf("\nsp_add=");
	b5500_sp_print(this->r.B);
	printf("\n\n");
	
	b5500_blinky_lights(this);
	return 0;
}
