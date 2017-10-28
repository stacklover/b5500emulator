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
	b5500_init_shares();

	printf("\033[2J");
	while (1) {
		printf("\033[H");
		b5500_pdp_text(P[0]);
		//sleep(1);
	}
	return 0;
}
