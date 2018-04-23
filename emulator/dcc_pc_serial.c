/***********************************************************************
* b5500emulator
************************************************************************
* Copyright (c) 2018, Reinhard Meyer, DL5UY
* Licensed under the MIT License,
*       see LICENSE
************************************************************************
* b5500 data communication emulation (DCC)
*
* This is the SERIAL server wrapper
*
************************************************************************
* 2018-04-21  R.Meyer
*   Frame from dev_dcc.c
* 2018-04-21  R.Meyer
*   factored out all physcial connection (PC), all line discipline(LD)
*   and all emulation (EM) functionality to spearate files
***********************************************************************/

#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <signal.h>

#include "common.h"
#include "io.h"
#include "telnetd.h"
#include "dcc.h"

/***********************************************************************
* PC SERIAL: POLL TERMINAL
***********************************************************************/
void pc_serial_poll_terminal(TERMINAL_T *t) {
}

/***********************************************************************
* PC SERIAL: INIT
***********************************************************************/
void pc_serial_init(void) {
}

/***********************************************************************
* PC SERIAL: POLL
***********************************************************************/
void pc_serial_poll(void) {
}

/***********************************************************************
* PC SERIAL: READ
***********************************************************************/
int pc_serial_read(TERMINAL_T *t, char *buf, int len) {
	int cnt;
	// try to read
	cnt = read(t->serial_handle, buf, len);
	// reason to disconnect?
	if (cnt != len) {
		t->pcs = pcs_failed;
	}
	return cnt;
}

/***********************************************************************
* PC SERIAL: WRITE
***********************************************************************/
int pc_serial_write(TERMINAL_T *t, char *buf, int len) {
	int cnt;
	// try to write
	cnt = write(t->serial_handle, buf, len);
	// reason to disconnect?
	if (cnt != len) {
		t->pcs = pcs_failed;
	}
	return cnt;
}


