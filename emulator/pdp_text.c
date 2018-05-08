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

void b5500_pdp_text1(CPU *cpu)
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

void b5500_pdp_text2(CPU *cpu)
{
	printf("%s X=%013llo J=%02o  Q=%o%o%o%o%o%o%o%o%o R=%05o  \n",
		cpu->id, cpu->rX, cpu->rJ,
		cpu->bQ09F, cpu->bQ08F, cpu->bQ07F,
		cpu->bQ06F, cpu->bQ05F, cpu->bQ04F,
		cpu->bQ03F, cpu->bQ02F, cpu->bQ01F,
		cpu->rR);

	printf("A=%016llo AROF=%o GH=%02o Y=%02o M=%05o  \n",
		cpu->rA, cpu->bAROF, cpu->rGH, cpu->rY, cpu->rM);

	printf("B=%016llo BROF=%o KV=%02o Z=%02o S=%05o  \n",
		cpu->rB, cpu->bBROF, cpu->rKV, cpu->rZ, cpu->rS);

	printf("P=%016llo PROF=%o T=%04o.%o C:L=%05o:%o  \n",
		cpu->rP, cpu->bPROF, cpu->rT, cpu->bTROF,
		cpu->rC, cpu->rL);

	printf("VARF=%o CWMF=%o MSFF=%o NCSF=%o SALF=%o   F=%05o  \n",
		cpu->bVARF, cpu->bCWMF, cpu->bMSFF, cpu->bNCSF,
		cpu->bSALF, cpu->rF);

	printf("MRAF=%o MWOF=%o MROF=%o HLTF=%o CCCF=%o EIHF=%o  \n",
		cpu->bMRAF, cpu->bMWOF, cpu->bMROF, cpu->bHLTF,
		cpu->bCCCF, cpu->bEIHF);

	printf("I=$%02x E=%02o N=%02o TM=$%02x  \n",
		cpu->rI, cpu->rE, cpu->rN, cpu->rTM);
}

void b5500_ccdp_text2(volatile CENTRAL_CONTROL *cc)
{
	printf("CC IAR=%02o TM=%02o ADxF=%o%o%o%o HP2F=%o P2BF=%o CLS=%o\n",
		cc->IAR, cc->TM,
		cc->AD1F, cc->AD2F, cc->AD3F, cc->AD4F,
		cc->HP2F, cc->P2BF, cc->CLS);
}

void b5500_iodp_text2(IOCU *io)
{
	printf("IO W=%016llo IB=%02o OB=%02o CALLS=%010u  \n",
		io->w, io->ib, io->ob, io->calls);

	printf("   D=(UNIT=%02d WC=%04d CONTROL=%03o RESULT=%04o ADDR=%05o)  \n",
		io->d_unit, io->d_wc, io->d_control, io->d_result, io->d_addr);
}


