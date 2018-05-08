/***********************************************************************
* b5500emulator
************************************************************************
* Copyright (c)	2016, Reinhard Meyer, DL5UY
* Licensed under the MIT License,
*	see LICENSE
* based	on work	by Nigel Williams and Paul Kimpel
* see: https://github.com/pkimpel/retro-b5500
************************************************************************
* viewer program for CPU state
************************************************************************
* 2016-02-19  R.Meyer
*   from thin air.
* 2017-09-30  R.Meyer
*   overhaul of file names
***********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <math.h>
#include <errno.h>
#include <unistd.h>
#include "common.h"

int main(int argc, char	*argv[])
{
	int i;
	b5500_init_shares();

	printf("\033[2J");
	while (1) {
		printf("\033[H");
		for (i=0; i<2; i++) {
			b5500_pdp_text2(P[i]);
			printf("\n");
		}

		b5500_ccdp_text2(CC);
		printf("\n");

		for (i=0; i<4; i++) {
			b5500_iodp_text2(IO[i]);
			printf("\n");
		}

		sleep(1);
	}
	return 0;
}
