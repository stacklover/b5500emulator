/***********************************************************************
* b5500emulator
************************************************************************
* Copyright (c)	2016, Reinhard Meyer, DL5UY
* Licensed under the MIT License,
*	see LICENSE
* based	on work	by Nigel Williams and Paul Kimpel
* see: https://github.com/pkimpel/retro-b5500
************************************************************************
* initialize shared memory
************************************************************************
* 2016-02-19  R.Meyer
*   from thin air.
* 2016-03-01  R.Meyer
*   added Central Control and message queues
* 2017-07-17  R.Meyer
*   added proper casts to return values	of shmat
* 2017-09-30  R.Meyer
*   overhaul of file names
***********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <errno.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include "common.h"

/*
 * storage declared here
 *
 * we might have different, implementation dependent ways
 * to get storage
 */
static
int	shm_main,   // main memory
	shm_cpu[2], // P1 and P2 registers
	shm_cc,	    // central control registers
	msg_cpu[2], // messages	to P1 and P2
	msg_cc;	    // messages	to CC

WORD48		*MAIN;
CPU2		*P[2];
CENTRAL_CONTROL	*CC;

void b5500_init_shares(void)
{
	shm_main = shmget(SHM_MAIN, MAXMEM*sizeof(WORD48), IPC_CREAT|0644);
	if (shm_main < 0) {
		perror("shmget MAIN");
		exit(2);
	}
	shm_cpu[0] = shmget(SHM_CPUA, sizeof(CPU2), IPC_CREAT|0644);
	if (shm_cpu[0] < 0) {
		perror("shmget P1");
		exit(2);
	}
	shm_cpu[1] = shmget(SHM_CPUB, sizeof(CPU2), IPC_CREAT|0644);
	if (shm_cpu[1] < 0) {
		perror("shmget P2");
		exit(2);
	}
	shm_cc = shmget(SHM_CC,	sizeof(CENTRAL_CONTROL), IPC_CREAT|0644);
	if (shm_cc < 0)	{
		perror("shmget CC");
		exit(2);
	}

	msg_cpu[0] = msgget(MSG_CPUA, IPC_CREAT|0644);
	if (msg_cpu[0] < 0) {
		perror("msgget P1");
		exit(2);
	}
	msg_cpu[1] = msgget(MSG_CPUB, IPC_CREAT|0644);
	if (msg_cpu[1] < 0) {
		perror("msgget P2");
		exit(2);
	}
	msg_cc = msgget(MSG_CC,	IPC_CREAT|0644);
	if (msg_cc < 0)	{
		perror("msgget CC");
		exit(2);
	}

	MAIN = (WORD48*)shmat(shm_main,	NULL, 0);
	if ((int)MAIN == -1) {
		perror("shmat MAIN");
		exit(2);
	}
	P[0] = (CPU2 *)shmat(shm_cpu[0], NULL,	0);
	if ((int)P[0]	== -1) {
		perror("shmat P1");
		exit(2);
	}
	P[1] = (CPU2 *)shmat(shm_cpu[1], NULL,	0);
	if ((int)P[1]	== -1) {
		perror("shmat P2");
		exit(2);
	}
	CC = (CENTRAL_CONTROL*)shmat(shm_cc, NULL, 0);
	if ((int)CC == -1) {
		perror("shmat CC");
		exit(2);
	}
}
