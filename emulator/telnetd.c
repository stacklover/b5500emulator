/***********************************************************************
* telnet server
************************************************************************
* Copyright (c) 2018, Reinhard Meyer, DL5UY
* Licensed under the MIT License,
*       see LICENSE
************************************************************************
* TELNET server
*
* This file is linkable to a program.
* It will handle the typical TELNET handshake and put the connection into
* remote ECHO
*
************************************************************************
* 2018-03-21  R.Meyer
*   extracted from b5500emulator/dev_dcc.c
* 2019-01-29  R.Meyer
*   clear telnet structure "type" when new connection arrives
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

#include "telnetd.h"

#define	TN_VERBOSE 1

/***********************************************************************
* Initial Negotiations
***********************************************************************/
static char negotiate[] = {
	TN_IAC, TN_WILL, TN_ECHO,	// server echo on
	TN_IAC, TN_DONT, TN_LINEMODE,	// client linemode off
	TN_IAC, TN_DO, TN_WINDOWSIZE,	// client window size - expect answer here
	TN_IAC, TN_DO, TN_TERMTYPE,	// terminal type - expect answer here
};

static char request_termtype[] = {
	TN_IAC, TN_SUB, TN_TERMTYPE, 1, TN_IAC, TN_END,
};

/***********************************************************************
* TELNET Escape Interpret
***********************************************************************/
static void telnet_escape_interpret(TELNET_SESSION_T *t) {
#if TN_VERBOSE
	unsigned i;

	for (i = 0; i < t->subidx; i++)
		printf("_%u", t->subbuf[i]);
	printf("\n");
#endif
	switch (t->subbuf[1]) { // which OPTION
	case TN_ECHO:
		switch (t->subbuf[0]) { // which response
		case TN_DO:
			t->is_fullduplex = true;
#if TN_VERBOSE
			printf("+FULLDUPLEX=%u\n", t->is_fullduplex);
#endif
			t->success_mask |= (1u << TN_ECHO);
			break;
		case TN_DONT:
			t->is_fullduplex = false;
#if TN_VERBOSE
			printf("+FULLDUPLEX=%u\n", t->is_fullduplex);
#endif
			t->success_mask |= 1u;	// failure
			break;
		default:
			return;
		}
		break;
	case TN_TERMTYPE:
		switch (t->subbuf[0]) { // which response
		case TN_WILL:
			if (write(t->socket, request_termtype, sizeof request_termtype) != sizeof request_termtype) {
				perror("telnet_session_write(request_termtype)");
				close(t->socket);
				t->socket = -1;
				return;
			}
			break;
		case TN_WONT:
#if TN_VERBOSE
			printf("+TERMINAL TYPE DENIED\n");
#endif
			t->success_mask |= 1u;	// failure
			break;
		case TN_SUB:
			t->subbuf[t->subidx] = 0;
			strncpy(t->type, t->subbuf+3, TN_TYPE_BUFLEN);
			t->type[TN_TYPE_BUFLEN-1] = 0;
#if TN_VERBOSE
			printf("+TERMINAL TYPE=%s\n", t->type);
#endif
			t->success_mask |= (1u << TN_TERMTYPE);
			break;
		default:
			return;
		}
		break;
	case TN_WINDOWSIZE:
		switch (t->subbuf[0]) { // which response
		case TN_SUB:
			if (t->subidx == 6) {
				t->cols = (t->subbuf[2] << 8) | t->subbuf[3];
				t->rows = (t->subbuf[4] << 8) | t->subbuf[5];
				t->success_mask |= (1u << TN_WINDOWSIZE);
			} else return;
			break;
		default:
			return;
		}
#if TN_VERBOSE
		printf("+WINDOW SIZE=%ux%u\n", t->cols, t->rows);
#endif
		break;
	default:
		return;
	}
}

/***********************************************************************
* TELNET Escape Check
***********************************************************************/
static int telnet_escape_check(TELNET_SESSION_T *t, char *buf, int len) {
	char *p, *q;
	int i;
	// test each character
	p = q = buf;
	for (i=0; i<len; i++) {
		// first detect IAC
		if (*p == TN_IAC) {
			if (t->lastchar == TN_IAC) {
				// double IAC means IAC in data
				t->lastchar = 0;
			} else {
				// first IAC, remmeber that
				t->lastchar = TN_IAC;
				p++;
				continue;
			}
		}

		// handle chars
		if (t->lastchar == TN_IAC) {
			// last char was IAC
			t->lastchar = 0;
			if (*p == TN_WILL || *p == TN_WONT || *p == TN_DO || *p == TN_DONT) {
				t->escape = had_cmd;
			} else if (*p == TN_SUB) {
				t->escape = had_sub;
			} else if (*p == TN_END) {
				t->escape = none;
				telnet_escape_interpret(t);
			} else {
				t->escape = none;
			}
			t->subbuf[0] = *p++;
			t->subidx = 1;
			continue;
		}

		// handle chars 
		switch (t->escape) {
		case none:
			// copy this char
			*q++ = *p++;
			break;
		case had_cmd:
			t->subbuf[1] = *p++;
			t->subidx = 2;
			t->escape = none;
			telnet_escape_interpret(t);
			break;
		case had_sub:
			// assemble into subbuf - prevent buffer overflow
			if (t->subidx < sizeof t->subbuf)
				t->subbuf[t->subidx++] = *p;
			p++;
			break;
		} // switch
	} // for
	return q - buf;
}

/***********************************************************************
* TELNET Session Open
***********************************************************************/
int telnet_session_open(TELNET_SESSION_T *t, int sock) {
	t->socket = sock;
	t->escape = none;
	t->is_fullduplex = true;
	t->cols = t->rows = 0;
	t->success_mask = 0;
	// try to negotiate ECHO ON, LINEMODE OFF and get WINDOWSIZE and get TERMINALTYPE
	if (write(t->socket, negotiate, sizeof negotiate) != sizeof negotiate) {
		perror("telnet_session_write(negotiate)");
		close(t->socket);
		t->socket = -1;
		return -1;
	}
	return 0;
}

/***********************************************************************
* TELNET Session Close
***********************************************************************/
void telnet_session_close(TELNET_SESSION_T *t) {
	int so = t->socket;
	t->socket = -1;		// prevent recursion
	if (so > 2)		// prevent accidential closing of std files
		close(so);
	t->escape = none;
	t->is_fullduplex = false;
	t->cols = t->rows = 0;
}

/***********************************************************************
* TELNET Session Clear
***********************************************************************/
void telnet_session_clear(TELNET_SESSION_T *t) {
	memset(t, 0, sizeof *t);
	strncpy(t->type, "unknown", sizeof(t->type));
	t->socket = -1;
}

/***********************************************************************
* TELNET Session Read
* Returns number of bytes read or -1 on non-recoverable error
***********************************************************************/
int telnet_session_read(TELNET_SESSION_T *t, char *buf, int len) {
	if (t->socket > 2) {	// prevent accidential use of std files
		int cnt = read(t->socket, buf, len);
		if (cnt < 0) {	// cnt < 0 : error occured
			if (errno == EAGAIN)
				return 0;	// recoverable
			telnet_session_close(t);
			return -1;
		}
		if (cnt == 0) {	// cnt = 0 : connection was closed by peer
			telnet_session_close(t);
			errno = ECONNRESET;
			return -1;
		}
		// check for TELNET inband escape sequences
		cnt = telnet_escape_check(t, buf, cnt);
		return cnt;
	}
	// no connection
	errno = ECONNRESET;
	return -1;
}

/***********************************************************************
* TELNET Session Write
* Returns number of bytes written or -1 on non-recoverable error
***********************************************************************/
int telnet_session_write(TELNET_SESSION_T *t, const char *buf, int len) {
	if (t->socket > 2) {	// prevent accidential use of std files
		int cnt = write(t->socket, buf, len);
		if (cnt < 0) {	// cnt < 0 : error occured
			if (errno == EAGAIN) {
				printf("telnet_session_write: EAGAIN\n");
				return 0;	// recoverable
			}
			telnet_session_close(t);
			return -1;
		}
		if (cnt != len) {
			printf("telnet_session_write: not all written: %d of %d\n",
				cnt, len);
		}
		return cnt;
	}
	// no connection
	errno = ECONNRESET;
	return -1;
}

/***********************************************************************
* TELNET Server Start
***********************************************************************/
int telnet_server_start(TELNET_SERVER_T *ts, unsigned port) {
	struct sockaddr_in addr;
	int optval;
	socklen_t optlen;

	// create socket for listening
	ts->socket = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
	if (ts->socket < 0) {
		perror("telnet socket");
		return -1;
	}
	// set REUSEADDR option
	optval = 1;
	optlen = sizeof(optval);
	if (setsockopt(ts->socket, SOL_SOCKET, SO_REUSEADDR, &optval, optlen) < 0) {
		perror("setsockopt(SO_REUSEADDR)");
		return -1;
	}
	// bind it to port
	bzero((char*)&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(port);
	if (bind(ts->socket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
		perror("telnet bind");
		close(ts->socket);
		ts->socket = -1;
		return -1;
	}
	// start the listening
	if (listen(ts->socket, 4)) {
		perror("telnet listen");
		close(ts->socket);
		ts->socket = -1;
		return -1;
	}
	printf("TELNET server listen socket %d for port %u\n",
		ts->socket, port);
	// all good
	return ts->socket;
}

/***********************************************************************
* TELNET Server Stop
***********************************************************************/
void telnet_server_stop(TELNET_SERVER_T *ts) {
	close(ts->socket);
	ts->socket = -1;
}

/***********************************************************************
* TELNET Server Poll for New Incoming Connection
***********************************************************************/
int telnet_server_poll(TELNET_SERVER_T *ts, struct sockaddr_in *addr) {
	int newsocket;
	socklen_t addrlen = sizeof(*addr);
	int optval;
	socklen_t optlen = sizeof(optval);

	if (ts->socket > 2) {
		newsocket = accept4(ts->socket, (struct sockaddr*)addr, &addrlen,
					SOCK_NONBLOCK | SOCK_CLOEXEC);
		if (newsocket > 2) {
			// set KEEPALIVE option
			optval = 1;
			optlen = sizeof(optval);
			if (setsockopt(newsocket, SOL_SOCKET, SO_KEEPALIVE, &optval, optlen) < 0) {
				perror("setsockopt(SO_KEEPALIVE)");
				close(newsocket);
				return -1;
			}
			return newsocket;
		}
		// no new connection
		return 0;
	}
	// no listen socket
	errno = ECONNRESET;
	return -1;
}

/***********************************************************************
* TELNET Server Clear
***********************************************************************/
void telnet_server_clear(TELNET_SERVER_T *ts) {
	ts->socket = -1;
}


