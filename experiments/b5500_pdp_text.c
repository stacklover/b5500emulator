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
***********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include "b5500_common.h"

void b5500_pdp_text(CPU *this)
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
	printf("%u %u %u %u %u %u %u",
		this->r.CWMF, this->r.MWOF, this->r.MROF,
		this->r.Q09F, this->r.Q06F, this->r.Q03F,
		this->r.VARF);
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
	printf("%u %u %u %u %u %u %u",
		this->r.SALF, this->r.EIHF, this->r.MRAF,
		this->r.Q08F, this->r.Q05F, this->r.Q02F,
		this->r.CCCF);
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
	printf("%u %u %u %u %u %u %u",
		this->r.NCSF, this->r.HLTF, this->r.Q12F,
		this->r.Q07F, this->r.Q04F, this->r.Q01F,
		this->r.zzzF);
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
	printf(" %u ", (this->r.L >> 1) & 1);
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
	printf(" %u ", (this->r.L >> 0) & 1);
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
