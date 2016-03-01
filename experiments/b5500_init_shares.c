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
* 2016-03-01  R.Meyer
*   added Central Control and message queues
***********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <errno.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include "b5500_common.h"

/*
 * storage declared here
 *
 * we might have different, implementation dependant ways
 * to get storage
 */
static
int	shm_main,
	shm_cpua,
	shm_cpub,
	shm_cc,
	msg_cpua,
	msg_cpub,
	msg_cc;

WORD48		*MAIN;
CPU		*CPUA;
CPU		*CPUB;
CENTRAL_CONTROL	*CC;

void b5500_init_shares(void)
{
	shm_main = shmget(SHM_MAIN, MAXMEM*sizeof(WORD48), IPC_CREAT|0644);
	if (shm_main < 0) {
		perror("shmget MAIN");
		exit(2);
	}
	shm_cpua = shmget(SHM_CPUA, sizeof(CPU), IPC_CREAT|0644);
	if (shm_cpua < 0) {
		perror("shmget CPUA");
		exit(2);
	}
	shm_cpub = shmget(SHM_CPUB, sizeof(CPU), IPC_CREAT|0644);
	if (shm_cpub < 0) {
		perror("shmget CPUB");
		exit(2);
	}
	shm_cc = shmget(SHM_CC, sizeof(CENTRAL_CONTROL), IPC_CREAT|0644);
	if (shm_cc < 0) {
		perror("shmget CENTRAL_CONTROL");
		exit(2);
	}

	msg_cpua = msgget(MSG_CPUA, IPC_CREAT|0644);
	if (msg_cpua < 0) {
		perror("msgget CPUA");
		exit(2);
	}
	msg_cpub = msgget(MSG_CPUB, IPC_CREAT|0644);
	if (msg_cpub < 0) {
		perror("msgget CPUB");
		exit(2);
	}
	msg_cc = msgget(MSG_CC, IPC_CREAT|0644);
	if (msg_cc < 0) {
		perror("msgget CC");
		exit(2);
	}

	MAIN = shmat(shm_main, NULL, 0);
	if ((int)MAIN == -1) {
		perror("shmat MAIN");
		exit(2);
	}
	CPUA = shmat(shm_cpua, NULL, 0);
	if ((int)CPUA == -1) {
		perror("shmat CPUA");
		exit(2);
	}
	CPUB = shmat(shm_cpub, NULL, 0);
	if ((int)CPUB == -1) {
		perror("shmat CPUB");
		exit(2);
	}
	CC = shmat(shm_cc, NULL, 0);
	if ((int)CC == -1) {
		perror("shmat CC");
		exit(2);
	}
}
