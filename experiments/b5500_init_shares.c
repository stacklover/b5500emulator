/***********************************************************************
* b5500emulator
************************************************************************
* Copyright (c) 2016, Reinhard Meyer, DL5UY
* Licensed under the MIT License,
*       see LICENSE
* based on work by Nigel Williams and Paul Kimpel
* see: https://github.com/pkimpel/retro-b5500
************************************************************************
* initialize shared memory
************************************************************************
* 2016-02-19  R.Meyer
*   from thin air.
***********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <errno.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include "b5500_common.h"

/*
 * storage declared here
 *
 * we might have different, implementation dependant ways
 * to get storage
 */
static
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
