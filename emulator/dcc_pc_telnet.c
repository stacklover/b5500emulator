/***********************************************************************
* b5500emulator
************************************************************************
* Copyright (c) 2018, Reinhard Meyer, DL5UY
* Licensed under the MIT License,
*       see LICENSE
************************************************************************
* b5500 data communication emulation (DCC)
*
* This is the TELNET server wrapper
*
************************************************************************
* 2018-04-21  R.Meyer
*   Frame from dev_dcc.c
* 2018-04-21  R.Meyer
*   factored out all physcial connection (PC), all line discipline(LD)
*   and all emulation (EM) functionality to spearate files
* 2019-01-29  R.Meyer
*   clear telnet structure "type" when new connection arrives
* 2019-02-08  R.Meyer
*   do not insist on TELNET negotiation for TELETYPE lines
* 2020-03-09  R.Meyer
*   added iTELEX functionality
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
* the TELNET servers
***********************************************************************/
static TELNET_SERVER_T server[NUMSERV_T];
// server[0]: Port 23 - BLOCK type terminals (B9352 with external ANSI emulation)
// server[1]: Port 8023 - LINE type terminals (TELETYPE)
static unsigned portno[NUMSERV_T] = {23, 8023};
static enum ld ldno[NUMSERV_T] = {ld_contention, ld_teletype};
static enum em emno[NUMSERV_T] = {em_ansi, em_none};

/***********************************************************************
* handle new incoming TELNET session
***********************************************************************/
static void new_connection(int newsocket, struct sockaddr_in *addr, enum ld ld, enum em em) {
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
		telnet_session_clear(&t->tsession);
		telnet_session_open(&t->tsession, newsocket);
		t->pc = pc_telnet;
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
* PC TELNET: POLL TERMINAL
***********************************************************************/
void pc_telnet_poll_terminal(TERMINAL_T *t) {
	static const char *msg = "\r\nB5700 TIME SHARING - YOUR TELNET CLIENT IS NOT COMPATIBLE\r\n";
	int cnt = -1;

	switch (t->pcs) {
	case pcs_disconnected:
		// this state is only left when an incoming TELNET is assigned to this slot
		break;
	case pcs_pending:
		// connection is pending
		// poll receive to make negotiation run
		// (we are not interested in any data yet)
		cnt = telnet_session_read(&t->tsession, t->inbuf, sizeof t->inbuf);
		if (cnt < 0) {
			// socket closed by peer
			t->outidx += sprintf(t->outbuf+t->outidx, " CLOSED\r\n");
			if (ctrace)
				spo_print(t->outbuf);
			t->pcs = pcs_aborted;
		} else if (t->tsession.success_mask & 1) {
			// negotiations have come up with a bad answer
			// send message
			telnet_session_write(&t->tsession, msg, strlen(msg));
			t->outidx += sprintf(t->outbuf+t->outidx, " FAILED %08x\r\n", t->tsession.success_mask);
			if (ctrace)
				spo_print(t->outbuf);
			t->pcs = pcs_aborted;
		} else if (((t->tsession.success_mask & (1u << TN_TERMTYPE)) &&
			    (t->tsession.success_mask & (1u << TN_WINDOWSIZE))) ||
				(t->ld == ld_teletype)) {
			// negotiations succeeded or it is a TELETYPE discipline
			// report it to system
			t->outidx += sprintf(t->outbuf+t->outidx, " CONNECTED %s %ux%u\r\n",
				t->tsession.type, t->tsession.cols, t->tsession.rows);
			if (ctrace)
				spo_print(t->outbuf);
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
		telnet_session_close(&t->tsession);
		t->pcs = pcs_disconnected;
		t->pc = pc_none;
		break;

	case pcs_failed:
		if (ctrace) {
			t->outidx = sprintf(t->outbuf, "+CLSD %s\r\n",
				t->name);
			spo_print(t->outbuf);
		}
		telnet_session_close(&t->tsession);
		dcc_report_disconnect(t);
		t->pcs = pcs_disconnected;
		t->pc = pc_none;
		break;
	}
}

/***********************************************************************
* PC TELNET: INIT
***********************************************************************/
void pc_telnet_init(void) {
	int index;

	for (index=0; index<NUMSERV_T; index++)
		telnet_server_clear(server+index);
}

/***********************************************************************
* PC TELNET: POLL
***********************************************************************/
void pc_telnet_poll(BIT telnet) {
	int newsocket = -1;
	struct sockaddr_in addr;
	unsigned index;

	// start/stop servers
	for (index=0; index<NUMSERV_T; index++) {
		if (telnet && server[index].socket <= 2) {
			telnet_server_start(server+index, portno[index]);
		} else if (!telnet && server[index].socket > 2) {
			telnet_server_stop(server+index);
		}
	}

	// poll servers for new connections
	for (index=0; index<NUMSERV_T; index++) {
		if (server[index].socket > 2) {
			newsocket = telnet_server_poll(server+index, &addr);
			if (newsocket > 0)
				new_connection(newsocket, &addr, ldno[index], emno[index]);
		}
	}
}

/***********************************************************************
* PC TELNET: READ
***********************************************************************/
int pc_telnet_read(TERMINAL_T *t, char *buf, int len) {
	// check for data available
	return telnet_session_read(&t->tsession, buf, len);
}

/***********************************************************************
* PC TELNET: WRITE
***********************************************************************/
int pc_telnet_write(TERMINAL_T *t, char *buf, int len) {
	int cnt;
	// try to write
	cnt = telnet_session_write(&t->tsession, buf, len);
	// reason to disconnect?
	if (cnt != len) {
		telnet_session_close(&t->tsession);
		t->pcs = pcs_failed;
	}
	return cnt;
}


