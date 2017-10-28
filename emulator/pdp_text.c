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
* 2017-09-30  R.Meyer
*   overhaul of file names
***********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include "common.h"

void b5500_pdp_text(CPU *cpu)
{
	int i;
	//       * * * * * * * * * * * * * * * * * * * * * * * * * * *
	printf(".............X..............J........Q.........R... %s\n",
		cpu->id);

	printf("'");
	for (i=38; i>=0; i-=3) {
		printf("%u", (int)(cpu->rX >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("'%u %u'", (cpu->rJ >> 3) & 1, (cpu->rJ >> 2) & 1);
	printf("%u %u %u %u %u %u %u",
		cpu->bCWMF, cpu->bMWOF, cpu->bMROF,
		cpu->bQ09F, cpu->bQ06F, cpu->bQ03F,
		cpu->bVARF);
	printf("'");
	for (i=8; i>=0; i-=3) {
		printf("%u", (cpu->rR >> (i+6)) & 1);
		if (i>=3) printf(" ");
	}
	printf("'\n");

	printf(" ");
	for (i=37; i>=0; i-=3) {
		printf("%u", (int)(cpu->rX >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("   %u ", (cpu->rJ >> 1) & 1);
	printf("%u %u %u %u %u %u %u",
		cpu->bSALF, cpu->bEIHF, cpu->bMRAF,
		cpu->bQ08F, cpu->bQ05F, cpu->bQ02F,
		cpu->bCCCF);
	printf(" ");
	for (i=7; i>=0; i-=3) {
		printf("%u", (cpu->rR >> (i+6)) & 1);
		if (i>=3) printf(" ");
	}
	printf("\n");

	printf(" ");
	for (i=36; i>=0; i-=3) {
		printf("%u", (int)(cpu->rX >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("   %u ", (cpu->rJ >> 0) & 1);
	printf("%u %u %u %u %u %u %u",
		cpu->bNCSF, cpu->bHLTF, cpu->bQ12F,
		cpu->bQ07F, cpu->bQ04F, cpu->bQ01F,
		cpu->bzzzF);
	printf(" ");
	for (i=6; i>=0; i-=3) {
		printf("%u", (cpu->rR >> (i+6)) & 1);
		if (i>=3) printf(" ");
	}
	printf("\n");

	//       * * * * * * * * * * * * * * * * * * * * * * * * * * *
	printf("................A................o.G.H. .Y.Z.....M.....\n");

	printf("'");
	for (i=47; i>=0; i-=3) {
		printf("%u", (int)(cpu->rA >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("'%u'%u'%u' '%u'%u'", cpu->bAROF,
		(cpu->rGH >> 5) & 1, (cpu->rGH >> 2) & 1,
		(cpu->rY >> 5) & 1, (cpu->rZ >> 5) & 1);
	for (i=14; i>=0; i-=3) {
		printf("%u", (cpu->rM >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("'\n");

	printf(" ");
	for (i=46; i>=0; i-=3) {
		printf("%u", (int)(cpu->rA >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("   %u %u...%u %u ",
		(cpu->rGH >> 4) & 1, (cpu->rGH >> 1) & 1,
		(cpu->rY >> 4) & 1, (cpu->rZ >> 4) & 1);
	for (i=13; i>=0; i-=3) {
		printf("%u", (cpu->rM >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("\n");

	printf(" ");
	for (i=45; i>=0; i-=3) {
		printf("%u", (int)(cpu->rA >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("   %u %u'N'    ",
		(cpu->rGH >> 3) & 1, (cpu->rGH >> 0) & 1);
	for (i=12; i>=0; i-=3) {
		printf("%u", (cpu->rM >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("\n");

	//       * * * * * * * * * * * * * * * * * * * * * * * * * * *
	printf("................B................o.K.V.%u %u %u.....S.....\n",
		(cpu->rN >> 3) & 1,
		(cpu->rY >> 3) & 1, (cpu->rZ >> 3) & 1);

	printf("'");
	for (i=47; i>=0; i-=3) {
		printf("%u", (int)(cpu->rB >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("'%u'%u'%u'%u %u %u'", cpu->bBROF,
		(cpu->rKV >> 5) & 1, (cpu->rKV >> 2) & 1,
		(cpu->rN >> 2) & 1,
		(cpu->rY >> 2) & 1, (cpu->rZ >> 2) & 1);
	for (i=14; i>=0; i-=3) {
		printf("%u", (cpu->rS >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("'\n");

	printf(" ");
	for (i=46; i>=0; i-=3) {
		printf("%u", (int)(cpu->rB >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("   %u %u %u %u %u ",
		(cpu->rKV >> 4) & 1, (cpu->rKV >> 1) & 1,
		(cpu->rN >> 1) & 1,
		(cpu->rY >> 1) & 1, (cpu->rZ >> 1) & 1);
	for (i=13; i>=0; i-=3) {
		printf("%u", (cpu->rS >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("\n");

	printf(" ");
	for (i=45; i>=0; i-=3) {
		printf("%u", (int)(cpu->rB >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("   %u %u %u %u %u ",
		(cpu->rKV >> 3) & 1, (cpu->rKV >> 0) & 1,
		(cpu->rN >> 0) & 1,
		(cpu->rY >> 0) & 1, (cpu->rZ >> 0) & 1);
	for (i=12; i>=0; i-=3) {
		printf("%u", (cpu->rS >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("\n");

	//       * * * * * * * * * * * * * * * * * * * * * * * * * * *
	printf("................P................o....T....o.....C.....\n");

	printf("'");
	for (i=47; i>=0; i-=3) {
		printf("%u", (int)(cpu->rP >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("'%u'", cpu->bPROF);
	for (i=11; i>=0; i-=3) {
		printf("%u", (cpu->rT >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("'%u'", cpu->bTROF);
	for (i=14; i>=0; i-=3) {
		printf("%u", (cpu->rC >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("'\n");

	printf(" ");
	for (i=46; i>=0; i-=3) {
		printf("%u", (int)(cpu->rP >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf(" %u ", (cpu->rL >> 1) & 1);
	for (i=10; i>=0; i-=3) {
		printf("%u", (cpu->rT >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("   ");
	for (i=13; i>=0; i-=3) {
		printf("%u", (cpu->rC >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("\n");

	printf(" ");
	for (i=45; i>=0; i-=3) {
		printf("%u", (int)(cpu->rP >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf(" %u ", (cpu->rL >> 0) & 1);
	for (i=9; i>=0; i-=3) {
		printf("%u", (cpu->rT >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("   ");
	for (i=12; i>=0; i-=3) {
		printf("%u", (cpu->rC >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("\n");

	//       * * * * * * * * * * * * * * * * * * * * * * * * * * *
	printf("    ........I........           ......E..........F.....\n");
	printf("    '");
	for (i=7; i>=0; i-=1) {
		printf("%u", (cpu->rI >> i) & 1);
		if (i>=1) printf(" ");
	}
	printf("'           '");
	for (i=5; i>=0; i-=1) {
		printf("%u", (cpu->rE >> i) & 1);
		if (i>=1) printf(" ");
	}
	printf("'");
	for (i=14; i>=0; i-=3) {
		printf("%u", (cpu->rF >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("'\n");

	printf("                                             ");
	for (i=13; i>=0; i-=3) {
		printf("%u", (cpu->rF >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("\n");

	printf("                                             ");
	for (i=12; i>=0; i-=3) {
		printf("%u", (cpu->rF >> i) & 1);
		if (i>=3) printf(" ");
	}
	printf("\n");
#if 0
	printf("Cyc%06u/%06u N%06u C%06u c%06u T%09u",
		cpu->runCycles, cpu->cycleLimit,
		cpu->normalCycles, cpu->controlCycles,
		cpu->cycleCount, cpu->totalCycles);
#endif
}

