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
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>

#include "common.h"
#include "io.h"
#include "circbuffer.h"
#include "telnetd.h"
#include "dcc.h"

/***********************************************************************
* string constants
***********************************************************************/
static const char *pc_name[] = {
	"NONE", "SERI", "CANO", "TELN"};
static const char *pcs_name[] = {
	"DISC", "PEND", "ABOR", "CONN", "FAIL"};
static const char *ld_name[] = {
	"TTY ", "CONT"};
static const char *em_name[] = {
	"NONE", "TTY ", "ANSI"};
static const char *bufstate_name[] = {
	"NRDY", "IDLE", "IBSY", "RRDY", "OBSY", "WRDY"};

/***********************************************************************
* the terminals
***********************************************************************/
int shm_dcc;	// DCC shared structures
static TERMINAL_T *terminal;

int main(int argc, char	*argv[])
{
	int i;
	TERMINAL_T *t;

	// establish shared memory
	shm_dcc = shmget(SHM_DCC, sizeof(TERMINAL_T)*NUMTERM, IPC_CREAT|0644);
	if (shm_dcc < 0) {
		perror("shmget DCC");
		exit(2);
	}
	terminal = (TERMINAL_T *)shmat(shm_dcc, NULL, 0);
	if ((int)terminal == -1) {
		perror("shmat DCC");
		exit(2);
	}

	printf("\033[2J");
	while (1) {
		printf("\033[H");
		for (i=0; i<NUMTERM-1; i++) {
			t = &terminal[i];
			printf("%-5.5s %-4.4s I=%u A=%u F=%u %-4.4s %-4.4s %-4.4s %-4.4s",
				t->name,
				bufstate_name[t->bufstate],
				t->interrupt, t->abnormal, t->fullbuffer,
				pc_name[t->pc], pcs_name[t->pcs],
				ld_name[t->ld], em_name[t->em]);
			switch (t->pc) {
			case pc_none:
				break;
			case pc_serial:
				printf(" %d", t->serial_handle);
				break;
			case pc_canopen:
				printf(" %d", t->canid);
				break;
			case pc_telnet:
				printf(" %d %s %ux%u %s",
					t->session.socket,
					t->session.type,
					t->session.cols, t->session.rows,
					t->peer_info);
				break;
			}
			printf("\033[K\n");
		}
		fflush(stdout);
		sleep(1);
	}
	return 0;
}
