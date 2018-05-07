/***********************************************************************
* b5500emulator
************************************************************************
* Copyright (c) 2018, Reinhard Meyer, DL5UY
* Licensed under the MIT License,
*       see LICENSE
************************************************************************
* b5500 data communication emulation (DCC)
*
* This is the CANopen server wrapper
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
* PC CANopen: POLL TERMINAL
***********************************************************************/
void pc_canopen_poll_terminal(TERMINAL_T *t) {
	int cnt = -1;
	char buf[OUTBUFSIZE];
	BIT ready = (t->canid > 0) && can_ready(t->canid);

	switch (t->pcs) {
	case pcs_disconnected:
		if (ready)
			t->pcs = pcs_pending;
		break;
	case pcs_pending:
		if (!ready) {
			t->pcs = pcs_disconnected;
		} else if (can_read(t->canid, buf, sizeof buf) > 0) {
			// user entered a line to wake up connection
			dcc_init_terminal(t);
			t->ld = ld_teletype;
			t->em = em_none;
			t->lds = lds_idle;
			dcc_report_connect(t);
			t->pcs = pcs_connected;
		}
		break;
	case pcs_connected:
		if (ready) {
			if (t->ld == ld_teletype)
				cnt = ld_poll_teletype(t);
			else
				cnt = ld_poll_contention(t);
			if (cnt < 0)
				t->pcs = pcs_failed;
		} else {
			t->pcs = pcs_failed;
		}
		break;
	case pcs_aborted:
	case pcs_failed:
		if (ctrace) {
			sprintf(buf, "+CLSD %s\r\n",
				t->name);
			spo_print(buf);
		}
		dcc_report_disconnect(t);
		t->pcs = pcs_disconnected;
		break;
	}
}


/***********************************************************************
* PC CANopen: INIT
***********************************************************************/
void pc_canopen_init(void) {
}

/***********************************************************************
* PC CANopen: POLL
***********************************************************************/
void pc_canopen_poll(void) {
}

/***********************************************************************
* PC CANopen: READ
***********************************************************************/
int pc_canopen_read(TERMINAL_T *t, char *buf, int len) {
	return can_read(t->canid, buf, len);
}

/***********************************************************************
* PC CANopen: WRITE
***********************************************************************/
int pc_canopen_write(TERMINAL_T *t, char *buf, int len) {
	int cnt;
	// write to CANopen
	cnt = can_write(t->canid, buf, len);
	// reason to disconnect?
	if (cnt != len) {
		t->pcs = pcs_failed;
	}
	return cnt;
}


