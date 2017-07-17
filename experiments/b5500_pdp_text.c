/***********************************************************************
* b5500emulator
************************************************************************
* Copyright (c) 2016, Reinhard Meyer, DL5UY
* Licensed under the MIT License,
*       see LICENSE
* based on work by Nigel Williams and Paul Kimpel
* see: https://github.com/pkimpel/retro-b5500
************************************************************************
* processor display panel in text mode
************************************************************************
* 2016-02-19  R.Meyer
*   from thin air.
* 2017-07-17  R.Meyer
*   changed "this" to "cpu" to avoid errors when using g++
***********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include "b5500_common.h"

void b5500_pdp_text(CPU *cpu)
{
	int i;
	//       * * * * * * * * * * * * * * * * * * * * * * * * * * *
	printf(".............X..............J........Q.........R...\n");

	printf("'");
	for (i=38; i>=0; i-=3) {
		printf("%u", (int)(cpu->r.X >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("'%u %u'", (cpu->r.J >> 3) & 1, (cpu->r.J >> 2) & 1);
	printf("%u %u %u %u %u %u %u",
		cpu->r.CWMF, cpu->r.MWOF, cpu->r.MROF,
		cpu->r.Q09F, cpu->r.Q06F, cpu->r.Q03F,
		cpu->r.VARF);
	printf("'");
	for (i=8; i>=0; i-=3) {
		printf("%u", (cpu->r.R >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("'\n");

	printf(" ");
	for (i=37; i>=0; i-=3) {
		printf("%u", (int)(cpu->r.X >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("   %u ", (cpu->r.J >> 1) & 1);
	printf("%u %u %u %u %u %u %u",
		cpu->r.SALF, cpu->r.EIHF, cpu->r.MRAF,
		cpu->r.Q08F, cpu->r.Q05F, cpu->r.Q02F,
		cpu->r.CCCF);
	printf(" ");
	for (i=7; i>=0; i-=3) {
		printf("%u", (cpu->r.R >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("\n");

	printf(" ");
	for (i=36; i>=0; i-=3) {
		printf("%u", (int)(cpu->r.X >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("   %u ", (cpu->r.J >> 0) & 1);
	printf("%u %u %u %u %u %u %u",
		cpu->r.NCSF, cpu->r.HLTF, cpu->r.Q12F,
		cpu->r.Q07F, cpu->r.Q04F, cpu->r.Q01F,
		cpu->r.zzzF);
	printf(" ");
	for (i=6; i>=0; i-=3) {
		printf("%u", (cpu->r.R >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("\n");

	//       * * * * * * * * * * * * * * * * * * * * * * * * * * *
	printf("................A................o.G.H. .Y.Z.....M.....\n");

	printf("'");
	for (i=47; i>=0; i-=3) {
		printf("%u", (int)(cpu->r.A >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("'%u'%u'%u' '%u'%u'", cpu->r.AROF,
		(cpu->r.G >> 2) & 1, (cpu->r.H >> 2) & 1,
		(cpu->r.Y >> 5) & 1, (cpu->r.Z >> 5) & 1);
	for (i=14; i>=0; i-=3) {
		printf("%u", (cpu->r.M >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("'\n");

	printf(" ");
	for (i=46; i>=0; i-=3) {
		printf("%u", (int)(cpu->r.A >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("   %u %u...%u %u ",
		(cpu->r.G >> 1) & 1, (cpu->r.H >> 1) & 1,
		(cpu->r.Y >> 4) & 1, (cpu->r.Z >> 4) & 1);
	for (i=13; i>=0; i-=3) {
		printf("%u", (cpu->r.M >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("\n");

	printf(" ");
	for (i=45; i>=0; i-=3) {
		printf("%u", (int)(cpu->r.A >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("   %u %u'N'    ",
		(cpu->r.G >> 0) & 1, (cpu->r.H >> 0) & 1);
	for (i=12; i>=0; i-=3) {
		printf("%u", (cpu->r.M >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("\n");

	//       * * * * * * * * * * * * * * * * * * * * * * * * * * *
	printf("................B................o.K.V.%u %u %u.....S.....\n",
		(cpu->r.N >> 3) & 1,
		(cpu->r.Y >> 3) & 1, (cpu->r.Z >> 3) & 1);

	printf("'");
	for (i=47; i>=0; i-=3) {
		printf("%u", (int)(cpu->r.B >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("'%u'%u'%u'%u %u %u'", cpu->r.BROF,
		(cpu->r.K >> 2) & 1, (cpu->r.V >> 2) & 1,
		(cpu->r.N >> 2) & 1,
		(cpu->r.Y >> 2) & 1, (cpu->r.Z >> 2) & 1);
	for (i=14; i>=0; i-=3) {
		printf("%u", (cpu->r.S >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("'\n");

	printf(" ");
	for (i=46; i>=0; i-=3) {
		printf("%u", (int)(cpu->r.B >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("   %u %u %u %u %u ",
		(cpu->r.K >> 1) & 1, (cpu->r.V >> 1) & 1,
		(cpu->r.N >> 1) & 1,
		(cpu->r.Y >> 1) & 1, (cpu->r.Z >> 1) & 1);
	for (i=13; i>=0; i-=3) {
		printf("%u", (cpu->r.S >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("\n");

	printf(" ");
	for (i=45; i>=0; i-=3) {
		printf("%u", (int)(cpu->r.B >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("   %u %u %u %u %u ",
		(cpu->r.K >> 0) & 1, (cpu->r.V >> 0) & 1,
		(cpu->r.N >> 0) & 1,
		(cpu->r.Y >> 0) & 1, (cpu->r.Z >> 0) & 1);
	for (i=12; i>=0; i-=3) {
		printf("%u", (cpu->r.S >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("\n");

	//       * * * * * * * * * * * * * * * * * * * * * * * * * * *
	printf("................P................o....T....o.....C.....\n");

	printf("'");
	for (i=47; i>=0; i-=3) {
		printf("%u", (int)(cpu->r.P >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("'%u'", cpu->r.PROF);
	for (i=11; i>=0; i-=3) {
		printf("%u", (cpu->r.T >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("'%u'", cpu->r.TROF);
	for (i=14; i>=0; i-=3) {
		printf("%u", (cpu->r.C >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("'\n");

	printf(" ");
	for (i=46; i>=0; i-=3) {
		printf("%u", (int)(cpu->r.P >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf(" %u ", (cpu->r.L >> 1) & 1);
	for (i=10; i>=0; i-=3) {
		printf("%u", (cpu->r.T >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("   ");
	for (i=13; i>=0; i-=3) {
		printf("%u", (cpu->r.C >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("\n");

	printf(" ");
	for (i=45; i>=0; i-=3) {
		printf("%u", (int)(cpu->r.P >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf(" %u ", (cpu->r.L >> 0) & 1);
	for (i=9; i>=0; i-=3) {
		printf("%u", (cpu->r.T >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("   ");
	for (i=12; i>=0; i-=3) {
		printf("%u", (cpu->r.C >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("\n");

	//       * * * * * * * * * * * * * * * * * * * * * * * * * * *
	printf("    ........I........           ......E..........F.....\n");
	printf("    '");
	for (i=7; i>=0; i-=1) {
		printf("%u", (cpu->r.I >> i) & 1);
		if (i>=1) printf(" ");
	}
	printf("'           '");
	for (i=5; i>=0; i-=1) {
		printf("%u", (cpu->r.E >> i) & 1);
		if (i>=1) printf(" ");
	}
	printf("'");
	for (i=14; i>=0; i-=3) {
		printf("%u", (cpu->r.F >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("'\n");

	printf("                                             ");
	for (i=13; i>=0; i-=3) {
		printf("%u", (cpu->r.F >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("\n");

	printf("                                             ");
	for (i=12; i>=0; i-=3) {
		printf("%u", (cpu->r.F >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("\n");
	printf("Cyc%06u/%06u N%06u C%06u c%06u T%09u",
		cpu->runCycles, cpu->cycleLimit,
		cpu->normalCycles, cpu->controlCycles,
		cpu->cycleCount, cpu->totalCycles);
}
