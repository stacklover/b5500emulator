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
int	shm_main,	// main memory
	shm_cpu[2],	// P1 and P2 registers
	shm_cc,		// central control registers
	shm_ioc[4];	// I/O control units

int	msg_cpu[2], // messages	to P1 and P2
	msg_iocu;   // messages	to IOCU(s)

volatile	WORD48		*MAIN;
		CPU		*P[2];
volatile	CENTRAL_CONTROL	*CC;
		IOCU		*IO[4];

void b5500_init_shares(void)
{
	shm_main = shmget(SHM_MAIN, MAXMEM*sizeof(WORD48), IPC_CREAT|0644);
	if (shm_main < 0) {
		perror("shmget MAIN");
		exit(2);
	}
	shm_cpu[0] = shmget(SHM_CPUA, sizeof(CPU), IPC_CREAT|0644);
	if (shm_cpu[0] < 0) {
		perror("shmget P1");
		exit(2);
	}
	shm_cpu[1] = shmget(SHM_CPUB, sizeof(CPU), IPC_CREAT|0644);
	if (shm_cpu[1] < 0) {
		perror("shmget P2");
		exit(2);
	}
	shm_cc = shmget(SHM_CC,	sizeof(CENTRAL_CONTROL), IPC_CREAT|0644);
	if (shm_cc < 0)	{
		perror("shmget CC");
		exit(2);
	}
	shm_ioc[0] = shmget(SHM_IOC1, sizeof(IOCU), IPC_CREAT|0644);
	if (shm_ioc[0] < 0) {
		perror("shmget IOC1");
		exit(2);
	}
	shm_ioc[1] = shmget(SHM_IOC2, sizeof(IOCU), IPC_CREAT|0644);
	if (shm_ioc[1] < 0) {
		perror("shmget IOC2");
		exit(2);
	}
	shm_ioc[2] = shmget(SHM_IOC3, sizeof(IOCU), IPC_CREAT|0644);
	if (shm_ioc[2] < 0) {
		perror("shmget IOC3");
		exit(2);
	}
	shm_ioc[3] = shmget(SHM_IOC4, sizeof(IOCU), IPC_CREAT|0644);
	if (shm_ioc[3] < 0) {
		perror("shmget IOC4");
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
	msg_iocu = msgget(MSG_IOCU, IPC_CREAT|0644);
	if (msg_iocu < 0)	{
		perror("msgget IOCU");
		exit(2);
	}

	MAIN = (WORD48*)shmat(shm_main,	NULL, 0);
	if ((int)MAIN == -1) {
		perror("shmat MAIN");
		exit(2);
	}
	P[0] = (CPU *)shmat(shm_cpu[0], NULL,	0);
	if ((int)P[0]	== -1) {
		perror("shmat P1");
		exit(2);
	}
	P[1] = (CPU *)shmat(shm_cpu[1], NULL,	0);
	if ((int)P[1]	== -1) {
		perror("shmat P2");
		exit(2);
	}
	CC = (CENTRAL_CONTROL*)shmat(shm_cc, NULL, 0);
	if ((int)CC == -1) {
		perror("shmat CC");
		exit(2);
	}
	IO[0] = (IOCU *)shmat(shm_ioc[0], NULL,	0);
	if ((int)IO[0]	== -1) {
		perror("shmat IOC1");
		exit(2);
	}
	IO[1] = (IOCU *)shmat(shm_ioc[1], NULL,	0);
	if ((int)IO[1]	== -1) {
		perror("shmat IOC2");
		exit(2);
	}
	IO[2] = (IOCU *)shmat(shm_ioc[2], NULL,	0);
	if ((int)IO[2]	== -1) {
		perror("shmat IOC3");
		exit(2);
	}
	IO[3] = (IOCU *)shmat(shm_ioc[3], NULL,	0);
	if ((int)IO[3]	== -1) {
		perror("shmat IOC4");
		exit(2);
	}
}
