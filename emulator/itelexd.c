/***********************************************************************
* iTELEX server
************************************************************************
* Copyright (c) 2020, Reinhard Meyer, DL5UY
* Licensed under the MIT License,
*       see LICENSE
************************************************************************
* ITELEX server
*
* This file is linkable to a program.
* It will handle the typical ITELEX handshake and put the connection into
* remote ECHO
*
************************************************************************
* 2020-03-09  R.Meyer
*   copied and modified from telnetd.c
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

#include "itelexd.h"

#define	IT_VERBOSE 0

/***********************************************************************
* Table ASCII to reduced ASCII
***********************************************************************/
const uint8_t ascii2reduced[128] = {
  // ASCII 0D, 0A are allowed
  // other ASCII 00-1F are translated to "NUL"
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // 00-07
  0x00,0x00,0x0a,0x00,0x00,0x0d,0x00,0x00, // 08-0F
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // 10-17
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // 18-1F
  // ASCII 20 is translated to SPACE
  // ASCII 2A is translated to BELL
  // ASCII 21-2F are translated to symbols where possible, otherwise to space
  0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x27, // 20-27  !"#$%&' <->        '
  0x28,0x29,0x3f,0x2b,0x2c,0x2d,0x2e,0x2f, // 28-2F ()*+,-./ <-> () +,-./
  0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37, // 30-37 01234567 <-> 01234567
  0x38,0x39,0x3a,0x20,0x20,0x3d,0x20,0x20, // 38-3F 89:;<=>? <-> 89:  = ?
  // ASCII 41-5A are translated to letters
  0x20,0x41,0x42,0x43,0x44,0x45,0x46,0x47, // 40-47 @ABCDEFG <->  ABCDEFG
  0x48,0x49,0x4a,0x4b,0x4c,0x4d,0x4e,0x4f, // 48-4F HIJKLMNO <-> HIJKLMNO
  0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57, // 50-57 PQRSTUVW <-> PQRSTUVW
  0x58,0x59,0x5a,0x20,0x20,0x20,0x20,0x20, // 58-5F XYZ[\]^_ <-> XYZ
  // ASCII 61-6A are translated to letters
  0x20,0x41,0x42,0x43,0x44,0x45,0x46,0x47, // 40-47 @abcdefg <->  ABCDEFG
  0x48,0x49,0x4a,0x4b,0x4c,0x4d,0x4e,0x4f, // 48-4F hijklmno <-> HIJKLMNO
  0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57, // 50-57 pqrstuvw <-> PQRSTUVW
  0x58,0x59,0x5a,0x20,0x20,0x20,0x20,0x20, // 58-5F xyz{|}~  <-> XYZ
};

/***********************************************************************
* Table ASCII to BAUDOT/ITA2
***********************************************************************/
// translate table ASCII<->ITA2
const uint8_t ascii2ita2[128] = {
  // ASCII 0D is translated to "CR"
  // ASCII 0A is translated to "LF"
  // other ASCII 00-1F are translated to "NUL"
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // 00-07
  0x00,0x00,0x02,0x00,0x00,0x08,0x00,0x00, // 08-0F
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // 10-17
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // 18-1F
  // ASCII 20 is translated to SPACE
  // ASCII 21-2F are translated to symbols where possible
  0x04,0x80,0x80,0x4D,0x5A,0x80,0x80,0x45, // 20-27  !"#$%&' <->    fg  '	f=DIG+f g=DIG+g
  0x4F,0x52,0x4B,0x51,0x4C,0x43,0x5C,0x5D, // 28-2F ()*+,-./ <-> ()§+,-./	§=BELL
  0x56,0x57,0x53,0x41,0x4A,0x50,0x55,0x47, // 30-37 01234567 <-> 01234567
  0x46,0x58,0x4E,0x80,0x80,0x5E,0x80,0x59, // 38-3F 89:;<=>? <-> 89:  = ?
  // ASCII 41-5A are translated to letters
  0x54,0x23,0x39,0x2E,0x29,0x21,0x2D,0x3A, // 40-47 @ABCDEFG <-> hABCDEFG	h=DIG+h
  0x34,0x26,0x2B,0x2F,0x32,0x3C,0x2C,0x38, // 48-4F HIJKLMNO <-> HIJKLMNO
  0x36,0x37,0x2A,0x25,0x30,0x27,0x3E,0x33, // 50-57 PQRSTUVW <-> PQRSTUVW
  0x3D,0x35,0x31,0x80,0x80,0x80,0x80,0x80, // 58-5F XYZ[\]^_ <-> XYZ
  // ASCII 61-6A are translated to letters
  0x80,0x23,0x39,0x2E,0x29,0x21,0x2D,0x3A, // 60-67  ABCDEFG <->  ABCDEFG
  0x34,0x26,0x2B,0x2F,0x32,0x3C,0x2C,0x38, // 68-6F HIJKLMNO <-> HIJKLMNO
  0x36,0x37,0x2A,0x25,0x30,0x27,0x3E,0x33, // 70-77 PQRSTUVW <-> PQRSTUVW
  0x3D,0x35,0x31,0x80,0x80,0x80,0x80,0x80, // 78-7F XYZ{|}~§ <-> XYZ		§=RUBOUT
};

/***********************************************************************
* Version Data
***********************************************************************/
static char greet_baudot[] = {
	IT_VER,	7, 0x01, 'B', '5', '7', '0', '0', 0x00,
};

static char greet_ascii[] = {
	0x06, 0x01, 0x00, 0x0d, 0x0a, 'B', '5', '7', '0', '0',
};

/***********************************************************************
* ITELEX Command Check
***********************************************************************/
static int iscommand(unsigned char ch) {
	if (ch <= 0x04 || (ch >= 0x06 && ch <= 0x09) || ch == 0x0b)
		return 1;
	return 0;
}

/***********************************************************************
* ITELEX SWAP
***********************************************************************/
static uint8_t iswap(uint8_t ch) {
	return	((ch & 0x01) << 4) |
		((ch & 0x02) << 2) |
		((ch & 0x04))      |
		((ch & 0x08) >> 2) |
		((ch & 0x10) >> 4);
}

#if IT_VERBOSE
/***********************************************************************
* ITELEX Packet Dump
***********************************************************************/
static void dumpbuf(const unsigned char *buf, int len, const char *msg) {
	int i;
	printf("    %03d:", len);
	for (i=0; i<len; i++) {
		printf(" %02x", buf[i]);
	} // for
	printf(" (%s)\n", msg);
}
#endif

/***********************************************************************
* ITELEX Send Ack
***********************************************************************/
static void sendack(ITELEX_SESSION_T *t) {
	uint8_t buf[3];
	buf[0] = IT_ACK;
	buf[1] = 1;
	buf[2] = t->rnr;
#if IT_VERBOSE
	dumpbuf(buf, 3, "sendack");
	printf("RNR=%d\n", t->rnr);
#endif
	write(t->socket, buf, 3);
}

/***********************************************************************
* ITELEX Send End
***********************************************************************/
static void sendend(ITELEX_SESSION_T *t) {
	uint8_t buf[2];
	buf[0] = IT_END;
	buf[1] = 0;
#if IT_VERBOSE
	dumpbuf(buf, 2, "sendend");
	printf("END\n");
#endif
	write(t->socket, buf, 2);
}

/***********************************************************************
* ITELEX Session Open
***********************************************************************/
int itelex_session_open(ITELEX_SESSION_T *t, int sock) {
	t->socket = sock;

	t->bidx = 0;
	t->snr = t->sack = t->rnr = 0;
	t->tbuzi = t->rbuzi = -1;

	t->success_mask = 0;

	// t->baudot must have been set before calling this function!
	if (t->baudot) {
		// greet with BAUDOT MODE
		if (write(t->socket, greet_baudot, sizeof greet_baudot) != sizeof greet_baudot) {
			perror("itelex_session_write(greet_baudot)");
			close(t->socket);
			t->socket = -1;
			return -1;
		}
	} else {
		// greet with ASCII MODE
		if (write(t->socket, greet_ascii, sizeof greet_ascii) != sizeof greet_ascii) {
			perror("itelex_session_write(greet_ascii)");
			close(t->socket);
			t->socket = -1;
			return -1;
		}
		t->snr += sizeof greet_ascii;
	}

	return 0;
}

/***********************************************************************
* ITELEX Session Close
***********************************************************************/
void itelex_session_close(ITELEX_SESSION_T *t) {
	int so = t->socket;
	sendend(t);		// send iTELEX END packet
	sleep(1);		// do we perhaps need this?
	t->socket = -1;		// prevent recursion
	if (so > 2)		// prevent accidential closing of std files
		close(so);
}

/***********************************************************************
* ITELEX Session Clear
***********************************************************************/
void itelex_session_clear(ITELEX_SESSION_T *t) {
	memset(t, 0, sizeof *t);
	t->socket = -1;
}

/***********************************************************************
* ITELEX Session Read
* Returns number of bytes read or -1 on non-recoverable error
***********************************************************************/
int itelex_session_read(ITELEX_SESSION_T *t, char *buf, int len) {
	//dumpbuf(t->buf, t->bidx, "@itelex_session_read - rest of buffer");
	// step 1: try to fill buffer
	int i = IT_BUFLEN-t->bidx;
	int k;

	// try to fill buffer with more read data
	if (i > 0 && t->socket > 2) {	// prevent accidential use of std files
		int cnt = read(t->socket, t->buf+t->bidx, i);
		if (cnt < 0) {	// cnt < 0 : error occured
			if (errno == EAGAIN)
				goto step2;	// recoverable
			itelex_session_close(t);
			return -1;
		}
		if (cnt == 0) {	// cnt = 0 : connection was closed by peer
			itelex_session_close(t);
			errno = ECONNRESET;
			return -1;
		}
		t->bidx += cnt;
#if IT_VERBOSE
		dumpbuf(t->buf, t->bidx, "buffer after read(socket)");
#endif
	}
step2:
	// check for and analyse iTELEX packet
	if (t->bidx > 0 && iscommand(t->buf[0])) {
		// iTELEX command byte at buffer start
		if (t->bidx > 1) {
			// we have length
			int plen = t->buf[1];
			if (t->bidx >= plen + 2) {
				// sufficient length - analyse command
#if IT_VERBOSE
				dumpbuf(t->buf, plen+2, "iTELEX packet found");
#endif
				switch(t->buf[0]) {
				case IT_BAU:
					// now convert to ASCII
					for (i = 0, k = 0; i < plen; i++) {
						uint8_t ch = iswap(t->buf[i+2]);
						// handle special codes first
						if (ch == ITA2_UNSHIFT) {
							t->rbuzi = false;
						} else if (ch == ITA2_SHIFT) {
							t->rbuzi = true;
						} else if (ch == ITA2_CR) {
							t->buf[k++] = '\r';
						} else if (ch == ITA2_SPACE) {
							t->buf[k++] = ' ';
						} else if (ch == ITA2_LF) {
							t->buf[k++] = '\n';
						} else if (ch == ITA2_NULL) {
							t->buf[k++] = 0;
						} else {
							// look it up in table
							// we must do a reverse loop, to find non capital letters instead of capital letters
							for (int m=127; m>=0; m--) {
								if (ascii2ita2[m] == (ch | (t->rbuzi ? TAB_SHIFT : TAB_UNSHIFT))) {
									t->buf[k++] = m;
									break;
								}
							}
							// not in table --> no char received
						 }
					}
					// now crunch buf
					t->bidx -= (plen + 2 - k);
					memcpy(t->buf + k, t->buf + (plen + 2), t->bidx);
#if IT_VERBOSE
					dumpbuf(t->buf, t->bidx, "converted to ASCII");
#endif
					// increment our receive byte counter
					t->rnr += plen;
					// let this be picked up next round
					return 0;
				case IT_ACK:
					if (plen != 1)
						break;
					t->sack = t->buf[2];
#if IT_VERBOSE
					printf("    SACK=%d SNR=%d RNR=%d\n", t->sack, t->snr, t->rnr);
#endif
					// send our own ack
					sendack(t);
					break;
				default:
					;
				}
				// remove command from buffer
				t->bidx -= plen+2;
				memcpy(t->buf, t->buf + (plen+2), t->bidx);
			}
		}
	}

	// try to find string of ASCII data and return it
	if (t->bidx > 0) {
		for (i = 0; i < t->bidx; i++) {
			// stop at any iTELEX Packet command
			if (iscommand(t->buf[i]))
				break;
		}
		if (i > len)
			i = len;
		if (i > 0) {
			memcpy(buf, t->buf, i);
			memcpy(t->buf, t->buf + i, t->bidx - i);
			t->bidx -= i;
#if IT_VERBOSE
			dumpbuf((const unsigned char *)buf, i, "returning ASCII");
#endif
			return i;
		}
	}

	// nothing found
	return 0;
}

/***********************************************************************
* ITELEX Session Write
* Returns number of bytes written or -1 on non-recoverable error
***********************************************************************/
int itelex_session_write(ITELEX_SESSION_T *t, const char *buf, int len) {
#if IT_VERBOSE
	dumpbuf((const unsigned char *)buf, len, "@itelex_session_write");
#endif
	if (t->socket > 2) {	// prevent accidential use of std files
		unsigned char buf2[256];
		int cnt, cnt2, len2;
		if (t->baudot) {
			// convert ASCII to BAUDOT/ITA2
			// exit when not all is acked
			uint8_t delta = t->snr - t->sack;
#if IT_VERBOSE
			printf("    SNR=%u SACK=%u delta=%u\n", t->snr, t->sack, delta);
#endif
			if (delta >= 50) {
#if IT_VERBOSE
				printf("    delta >= 50 - not sending \n");
				sleep(1);
#endif
				return 0;
			}
			buf2[0] = 2; // baudot data
			buf2[1] = 0; // length
			t->tbuzi = -1;
			for (len2 = 0, cnt = 0; cnt < len && len2 < 39; cnt++) {
				uint8_t ch = ascii2ita2[buf[cnt] & 0x7f];
				if (ch & TAB_ESCAPE)
					ch = 0x59;
				if ((ch & TAB_UNSHIFT) && t->tbuzi != 0) {
					buf2[len2 + 2] = iswap(ITA2_UNSHIFT);
					len2++;
					t->tbuzi = 0;
				} else if ((ch & TAB_SHIFT) && t->tbuzi != 1) {
					buf2[len2 + 2] = iswap(ITA2_SHIFT);
					len2++;
					t->tbuzi = 1;
				}
				buf2[len2 + 2] = iswap(ch & TAB_MASK);
				len2++;
			}
			if (t->tbuzi != t->rbuzi) {
				if (t->rbuzi == 0) {
					buf2[len2 + 2] = iswap(ITA2_UNSHIFT);
					len2++;
				} else if (t->rbuzi == 1) {
					buf2[len2 + 2] = iswap(ITA2_UNSHIFT);
					len2++;
				}
			}
			if (len2 > 0) {
				buf2[1] = len2;
#if IT_VERBOSE
				dumpbuf(buf2, len2+2, "iTELEX packet to send");
#endif
				t->snr += len2;
				cnt2 = write(t->socket, buf2, len2+2);
				if (cnt2 < 0) {	// cnt2 < 0 : error occured
					if (errno == EAGAIN) {
						printf("itelex_session_write: EAGAIN\n");
						return 0;	// recoverable
					}
					itelex_session_close(t);
					return -1;
				}
			}
		} else {
			// convert ASCII to reduced ASCII
			for (cnt = 0, len2 = 0; cnt < len; cnt++) {
				if ((buf2[len2] = ascii2reduced[buf[cnt] & 0x7f]) > 0)
					len2++;
			}
			if (len2 > 0) {
#if IT_VERBOSE
				dumpbuf(buf2, len2, "ASCII to send");
#endif
				t->snr += len2;
				cnt2 = write(t->socket, buf2, len2);
				if (cnt2 < 0) {	// cnt < 0 : error occured
					if (errno == EAGAIN) {
						printf("itelex_session_write: EAGAIN\n");
						return 0;	// recoverable
					}
					itelex_session_close(t);
					return -1;
				}
			}
		}
		// report how much we used
		return cnt;
	}
	// no connection
	errno = ECONNRESET;
	return -1;
}

/***********************************************************************
* ITELEX Server Start
***********************************************************************/
int itelex_server_start(ITELEX_SERVER_T *ts, unsigned port) {
	struct sockaddr_in addr;
	int optval;
	socklen_t optlen;

	// create socket for listening
	ts->socket = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
	if (ts->socket < 0) {
		perror("itelex socket");
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
		perror("itelex bind");
		close(ts->socket);
		ts->socket = -1;
		return -1;
	}
	// start the listening
	if (listen(ts->socket, 4)) {
		perror("itelex listen");
		close(ts->socket);
		ts->socket = -1;
		return -1;
	}
	printf("ITELEX server listen socket %d for port %u\n",
		ts->socket, port);
	// all good
	return ts->socket;
}

/***********************************************************************
* ITELEX Server Stop
***********************************************************************/
void itelex_server_stop(ITELEX_SERVER_T *ts) {
	close(ts->socket);
	ts->socket = -1;
}

/***********************************************************************
* ITELEX Server Poll for New Incoming Connection
***********************************************************************/
int itelex_server_poll(ITELEX_SERVER_T *ts, struct sockaddr_in *addr) {
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
* ITELEX Server Clear
***********************************************************************/
void itelex_server_clear(ITELEX_SERVER_T *ts) {
	ts->socket = -1;
}


