/***********************************************************************
* telnet daemon
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

/***********************************************************************
* TELNET Escape Check
***********************************************************************/
static int telnet_escape_check(TELNET_SESSION_T *t, char *buf, int len) {
	char *p, *q;
	int i;
	// test each character
	p = q = buf;
	for (i=0; i<len; i++) {
		switch (t->escape) {
		case none:
			// we are not in an escape sequence
			if (*p == TN_IAC) {
				// start escape sequence handling
				t->escape = had_iac;
				// skip this char
				p++;
			} else {
				// copy this char
				*q++ = *p++;
			}
			break;
		case had_iac:
			// last char was IAC
			if (*p == TN_IAC) {
				// data char
				// copy this char
				*q++ = *p++;
				t->escape = none;
			} else if (*p == TN_WILL) {
				p++;
				t->escape = had_will;
			} else if (*p == TN_WONT) {
				p++;
				t->escape = had_wont;
			} else if (*p == TN_DO) {
				p++;
				t->escape = had_do;
			} else if (*p == TN_DONT) {
				p++;
				t->escape = had_dont;
			} else if (*p == TN_SUB) {
				p++;
				t->escape = had_sub;
			} else {
				p++;
				t->escape = none;
			}
			break;
		case had_will:
			p++;
			t->escape = none;
			break;
		case had_wont:
			p++;
			t->escape = none;
			break;
		case had_do:
			if (*p == TN_ECHO)
				t->is_fullduplex = true;
			p++;
			t->escape = none;
			break;
		case had_dont:
			if (*p == TN_ECHO)
				t->is_fullduplex = false;
			p++;
			t->escape = none;
			break;
		case had_sub:
			// ignore all until TN_END
			if (*p == TN_END) {
				p++;
				t->escape = none;
			} else {
				p++;
			}
			break;
		} // switch
	} // for
	return q - buf;
}

/***********************************************************************
* TELNET Session Open
***********************************************************************/
int telnet_session_open(TELNET_SESSION_T *t, int sock) {
	static char willecho[3] = {TN_IAC, TN_WILL, 1};

	t->socket = sock;
	t->escape = none;
	t->is_fullduplex = false;
	// try to negotiate ECHO ON
	if (write(t->socket, willecho, sizeof willecho) != sizeof willecho) {
		perror("telnet_session_write(willecho)");
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
}

/***********************************************************************
* TELNET Session Clear
***********************************************************************/
void telnet_session_clear(TELNET_SESSION_T *t) {
	t->socket = -1;
	t->escape = none;
	t->is_fullduplex = false;
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
			if (errno == EAGAIN)
				return 0;	// recoverable
			telnet_session_close(t);
			return -1;
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


