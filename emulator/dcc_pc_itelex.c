/***********************************************************************
* b5500emulator
************************************************************************
* Copyright (c) 2018, Reinhard Meyer, DL5UY
* Licensed under the MIT License,
*       see LICENSE
************************************************************************
* b5500 data communication emulation (DCC)
*
* This is the iTELEX server wrapper
*
************************************************************************
* 2020-03-09  R.Meyer
*   copied and modified from dcc_pc_telnet.c
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
#include "itelexd.h"
#include "dcc.h"

/***********************************************************************
* the iTELEX servers
***********************************************************************/
static ITELEX_SERVER_T server[NUMSERV_I];
// server[0]: Port 8024 - LINE type terminals (TELETYPE)
static unsigned portno[NUMSERV_I] = {8024, 8025};
static enum ld ldno[NUMSERV_I] = {ld_teletype, ld_teletype};
static enum em emno[NUMSERV_I] = {em_none, em_none};
static BIT     is_baudot[NUMSERV_I] = {0, 1};

/***********************************************************************
* handle new incoming ITELEX session
***********************************************************************/
static void new_connection(int newsocket, struct sockaddr_in *addr, enum ld ld, enum em em, BIT baudot) {
	static const char *msg = "\r\nB5700 TIME SHARING - BUSY\r\nPLEASE CALL BACK LATER\r\n";
	socklen_t addrlen = sizeof(*addr);
	char host[PEER_INFO_LEN - 6];	// keep space at end for port
	unsigned port;
	TERMINAL_T *t;
	char buf[OUTBUFSIZE];

	// get the peer info
	getnameinfo((struct sockaddr*)addr, addrlen, host, sizeof(host), NULL, 0, 0);
	port = ntohs(addr->sin_port);

	// find a free terminal to handle this line procedure
	t = dcc_find_free_terminal(ld);

	if (t != NULL) {
		// free terminal found
		snprintf(t->peer_info, sizeof t->peer_info, "%s:%u", host, port);
		t->outidx = sprintf(t->outbuf, "+NEWC %s %s (%d)",
			t->name, t->peer_info, newsocket);
		dcc_init_terminal(t);
		itelex_session_clear(&t->isession);
		t->isession.baudot = baudot;
		itelex_session_open(&t->isession, newsocket);
		t->pc = pc_itelex;
		t->ld = ld; t->em = em; t->lds = lds_idle;
		t->pcs = pcs_pending;
	} else {
		// no free entry found
		sprintf(buf, "+BUSY %s:%u (%d)\r\n",
			host, port, newsocket);
		spo_print(buf);
		write(newsocket, msg, strlen(msg));
		close(newsocket);
	}
}

/***********************************************************************
* PC ITELEX: POLL TERMINAL
***********************************************************************/
void pc_itelex_poll_terminal(TERMINAL_T *t) {
	static const char *msg = "\r\nB5700 TIME SHARING - YOUR ITELEX CLIENT IS NOT COMPATIBLE\r\n";
	int cnt = -1;

	switch (t->pcs) {
	case pcs_disconnected:
		// this state is only left when an incoming ITELEX is assigned to this slot
		break;
	case pcs_pending:
		// connection is pending
		// poll receive to make negotiation run
		// (we are not interested in any data yet)
		cnt = itelex_session_read(&t->isession, t->inbuf, sizeof t->inbuf);
		if (cnt < 0) {
			// socket closed by peer
			t->outidx += sprintf(t->outbuf+t->outidx, " CLOSED\r\n");
			if (ctrace)
				spo_print(t->outbuf);
			t->pcs = pcs_aborted;
		} else if (t->isession.success_mask & 1) {
			// negotiations have come up with a bad answer
			// send message
			itelex_session_write(&t->isession, msg, strlen(msg));
			t->outidx += sprintf(t->outbuf+t->outidx, " FAILED %08x\r\n", t->isession.success_mask);
			if (ctrace)
				spo_print(t->outbuf);
			t->pcs = pcs_aborted;
		} else if (t->ld == ld_teletype) {
			// it is a TELETYPE discipline
			// report it to system
			t->outidx += sprintf(t->outbuf+t->outidx, " CONNECTED\r\n");
			if (ctrace)
				spo_print(t->outbuf);
			t->outidx = 0;
			t->pcs = pcs_connected;
			dcc_report_connect(t);
		} else {
			// keep waiting
		}
		break;
	case pcs_connected:
		// depending on line discipline the action is different
		switch (t->ld) {
		case ld_teletype:
			cnt = ld_poll_teletype(t);
			break;
		case ld_contention:
			cnt = ld_poll_contention(t);
			break;
		}
		if (cnt < 0) {
			// socket closed by peer
			t->pcs = pcs_failed;
		}
		break;
	case pcs_aborted:
		itelex_session_close(&t->isession);
		t->pcs = pcs_disconnected;
		t->pc = pc_none;
		break;

	case pcs_failed:
		if (ctrace) {
			t->outidx = sprintf(t->outbuf, "+CLSD %s\r\n",
				t->name);
			spo_print(t->outbuf);
		}
		itelex_session_close(&t->isession);
		dcc_report_disconnect(t);
		t->pcs = pcs_disconnected;
		t->pc = pc_none;
		break;
	}
}

/***********************************************************************
* PC ITELEX: INIT
***********************************************************************/
void pc_itelex_init(void) {
	int index;

	for (index=0; index<NUMSERV_I; index++)
		itelex_server_clear(server+index);
}

/***********************************************************************
* PC ITELEX: POLL
***********************************************************************/
void pc_itelex_poll(BIT itelex) {
	int newsocket = -1;
	struct sockaddr_in addr;
	unsigned index;

	// start/stop servers
	for (index=0; index<NUMSERV_I; index++) {
		if (itelex && server[index].socket <= 2) {
			itelex_server_start(server+index, portno[index]);
		} else if (!itelex && server[index].socket > 2) {
			itelex_server_stop(server+index);
		}
	}

	// poll servers for new connections
	for (index=0; index<NUMSERV_I; index++) {
		if (server[index].socket > 2) {
			newsocket = itelex_server_poll(server+index, &addr);
			if (newsocket > 0)
				new_connection(newsocket, &addr,
					ldno[index], emno[index], is_baudot[index]);
		}
	}
}

/***********************************************************************
* PC ITELEX: READ
***********************************************************************/
int pc_itelex_read(TERMINAL_T *t, char *buf, int len) {
	// check for data available
	return itelex_session_read(&t->isession, buf, len);
}

/***********************************************************************
* PC ITELEX: WRITE
***********************************************************************/
int pc_itelex_write(TERMINAL_T *t, char *buf, int len) {
	// try to write
	return itelex_session_write(&t->isession, buf, len);
}


