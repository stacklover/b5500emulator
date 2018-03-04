/***********************************************************************
* b5500emulator
************************************************************************
* Copyright (c) 2018, Reinhard Meyer, DL5UY
* Licensed under the MIT License,
*       see LICENSE
************************************************************************
* b5500 data communication emulation (DC)
*
* This emulates:
* 1x B249 Data Transmission Control Unit (DTCU) with
*   15x B487 Data Transmission Terminal Unit (DTTU) each with
*      16x 980 Teletype adapters
*
* Internally we treat this as 240 Terminals, indexed from 16 to 255
*
* Originally a B487 has a total of 448 characters buffer, meaning that,
* if you use 16 adapters, each adapter can only habe 28 characters of buffer.
*
* However, it seems to cause no issues, when we give each adapter 112
* chars of buffer.
*
* This SYSDISK/MAKER input works:
*
?EXECUTE SYSDISK/MAKER
?DATA CARD
LINE,0,0,112,0,0,7,0,
LINE,1,0,112,0,0,0,0,
STA,0,0,0,0,"0","0",0,0,
LINE,1,1,112,0,0,0,0,
STA,0,0,0,0,"0","0",0,0,
LINE,1,2,112,0,0,0,0,
STA,0,0,0,0,"0","0",0,0,
LINE,1,3,112,0,0,0,0,
STA,0,0,0,0,"0","0",0,0,
LINE,1,4,112,0,0,0,0,
STA,0,0,0,0,"0","0",0,0,
LINE,1,5,112,0,0,0,0,
STA,0,0,0,0,"0","0",0,0,
LINE,1,6,112,0,0,0,0,
STA,0,0,0,0,"0","0",0,0,
LINE,1,7,112,0,0,0,0,
STA,0,0,0,0,"0","0",0,0,
LINE,1,8,112,0,0,0,0,
STA,0,0,0,0,"0","0",0,0,
LINE,1,9,112,0,0,0,0,
STA,0,0,0,0,"0","0",0,0,
LINE,1,10,112,0,0,0,0,
STA,0,0,0,0,"0","0",0,0,
LINE,1,11,112,0,0,0,0,
STA,0,0,0,0,"0","0",0,0,
LINE,1,12,112,0,0,0,0,
STA,0,0,0,0,"0","0",0,0,
LINE,1,13,112,0,0,0,0,
STA,0,0,0,0,"0","0",0,0,
LINE,1,14,112,0,0,0,0,
STA,0,0,0,0,"0","0",0,0,
LINE,1,15,112,0,0,0,0,
STA,0,0,0,0,"0","0",0,0,
?END
*
************************************************************************
* 2018-02-14  R.Meyer
*   Frame from dev_spo.c
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

#define NUMTERM 256
#define	BUFLEN 112
#define	BUFLEN2 200

#define TRACE_DCC 0

#define TIMEOUT 1000

/***********************************************************************
* the DCC
***********************************************************************/
enum bufstate {
	notready=0,
	idle,
	inputbusy,
	readready,
	outputbusy,
	writeready};

typedef struct terminal {
	int socket;
	enum bufstate bufstate;
	BIT connected;
	BIT interrupt;
	BIT abnormal;
	BIT fullbuffer;
	unsigned timer;
	unsigned bufidx;
	char buffer[BUFLEN2+1];
} TERMINAL_T;

static BIT ready;
static TERMINAL_T terminal[NUMTERM];	// we waste indices 0..15 here

// telnet
static int listen_socket;
static BIT dtrace = false;
static BIT ctrace = true;

/***********************************************************************
* Socket Close
***********************************************************************/
static void socket_close(TERMINAL_T *t) {
	int so = t->socket;
	t->socket = -1;		// prevent recursion
	if (so > 2) {
		close(so);
		if (ctrace) {
			sprintf(t->buffer, "+TELNET(%d) CLOSED\r\n", so);
			spo_print(t->buffer);
		}
	}
	t->interrupt = true;
	t->abnormal = true;
	t->fullbuffer = false;
	t->bufstate = notready;
	t->connected = false;
	t->buffer[0] = 0;
	t->bufidx = 0;
}

/***********************************************************************
* Socket Read
***********************************************************************/
static int socket_read(TERMINAL_T *t, char *buf, int len) {
	if (t->socket > 2) {
		int cnt = read(t->socket, buf, len);
		if (cnt < 0) {
			if (errno == EAGAIN)
				return 0;
			if (dtrace)
				perror("read failed - closing");
			socket_close(t);
		}
		return cnt;
	}
	errno = ECONNRESET;
	return -1;
}

/***********************************************************************
* Socket Write
***********************************************************************/
static int socket_write(TERMINAL_T *t, const char *buf, int len) {
	if (t->socket > 2) {
		int cnt = write(t->socket, buf, len);
		if (cnt < 0) {
			if (errno == EAGAIN)
				return 0;
			if (dtrace)
				perror("write failed - closing");
			socket_close(t);
		}
		return cnt;
	}
	errno = ECONNRESET;
	return -1;
}

/***********************************************************************
* specify connect trace on/off
***********************************************************************/
static int set_ctrace(const char *v, void *) {
	if (strcasecmp(v, "ON") == 0) {
		ctrace = true;
	} else if (strcasecmp(v, "OFF") == 0) {
		ctrace = false;
	} else {
		spo_print("$SPECIFY ON OR OFF\r\n");
		return 2; // FATAL
	}
	return 0; // OK
}

/***********************************************************************
* specify data trace on/off
***********************************************************************/
static int set_dtrace(const char *v, void *) {
	if (strcasecmp(v, "ON") == 0) {
		dtrace = true;
	} else if (strcasecmp(v, "OFF") == 0) {
		dtrace = false;
	} else {
		spo_print("$SPECIFY ON OR OFF\r\n");
		return 2; // FATAL
	}
	return 0; // OK
}

/***********************************************************************
* get status
***********************************************************************/
static int get_status(const char *v, void *) {
	char buf[BUFLEN2];
	TERMINAL_T *t;
	unsigned tun, bnr;

	// list all connected terminals
	for (tun = 1; tun < 16; tun++) {
		for (bnr = 0; bnr < 16; bnr++) {
			t = &terminal[tun*16 + bnr];
			if (t->connected) {
				sprintf(buf,"%u/%u S=%d B=%d I=%u A=%u F=%u\r\n",
					tun, bnr,
					t->socket, (int)t->bufstate,
					t->interrupt, t->abnormal, t->fullbuffer);
				spo_print(buf);
			}
		}
	}

	return 0; // OK
}

/***********************************************************************
* command table
***********************************************************************/
static const command_t dcc_commands[] = {
	{"DCC", NULL},
	{"CTRACE", set_ctrace},
	{"DTRACE", set_dtrace},
	{"STATUS", get_status},
	{NULL, NULL},
};

/***********************************************************************
* Find a buffer that needs service
***********************************************************************/
static BIT terminal_search(unsigned *ptun, unsigned *pbnr) {
	unsigned tun, bnr;
	TERMINAL_T *t;

	for (tun = 1; tun < 16; tun++) {
		for (bnr = 0; bnr < 16; bnr++) {
			t = &terminal[tun*16 + bnr];
			// trigger late output IRQ
			if (t->bufstate == outputbusy) {
				if (++t->timer > TIMEOUT) {
					t->bufstate = t->fullbuffer ? writeready : idle;
					t->interrupt = true;
					//printf("IRQ %u/%u\n", tun, bnr);
				}
			}
			if (t->interrupt)  {
				// something found
				if (ptun) *ptun = tun;
				if (pbnr) *pbnr = bnr;
				return true;
			}
		}
	}
	// nothing found
	if (ptun) *ptun = 0;
	if (pbnr) *pbnr = 0;
	return false;
}

/***********************************************************************
* Initialize command from argv scanner or special SPO input
***********************************************************************/
int dcc_init(const char *option) {
	if (!ready) {
		// ignore all SIGPIPEs
		signal(SIGPIPE, SIG_IGN);
		// start telnet server
		struct sockaddr_in addr;
		listen_socket = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
		if (listen_socket < 0) {
			perror("telnet socket");
			return errno;
		}
		bzero((char*)&addr, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = INADDR_ANY;
		addr.sin_port = htons(23);
		if (bind(listen_socket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
			// perror("telnet bind");
			close(listen_socket);
			listen_socket = -1;
			return errno;
		}
		if (listen(listen_socket, 16)) {
			perror("telnet listen");
			close(listen_socket);
			listen_socket = -1;
			return errno;
		}
		printf("TELNET server listen socket %d\n", listen_socket);
	}
	ready = true;
	return command_parser(dcc_commands, option);
}

/***********************************************************************
* handle new incoming TELNET session
***********************************************************************/
static const char *msg = "\r\nB5500 TIME SHARING - BUSY\r\nPLEASE CALL BACK LATER\r\n";
static void dcc_test_incoming(void) {
	int socket, cnt, i;
	struct sockaddr_in addr;
	socklen_t addrlen = sizeof(addr);
	unsigned port = 0;
	unsigned tun, bnr;
	char host[50];
	char buf[BUFLEN2];
	TERMINAL_T *t;

	if (listen_socket > 2) {
		socket = accept4(listen_socket, (struct sockaddr*)&addr, &addrlen, SOCK_NONBLOCK | SOCK_CLOEXEC);
		if (socket > 2) {
			if (ctrace) {
				getnameinfo((struct sockaddr*)&addr, addrlen, host, sizeof(host), NULL, 0, 0);
				port = ntohs(addr.sin_port);
			}
			// find a free terminal to handle this
			for (tun = 1; tun < 2; tun++) {
				for (bnr = 0; bnr < 4; bnr++) {
					t = &terminal[tun*16 + bnr];
					if (!t->connected) {
						// free terminal found
						if (ctrace) {
							sprintf(buf, "+TELNET(%d) %s:%u LINE %u/%u\r\n",
								socket, host, port, tun, bnr);
							spo_print(buf);
						}
						t->bufidx = 0;
						t->socket = socket;
						t->abnormal = true;
						t->bufstate = writeready;
						t->connected = true;
						t->interrupt = true;
						return;
					}
				}
			}
			// nothing found
			if (ctrace) {
				sprintf(buf, "+TELNET#%d %s:%u NO LINE. CLOSING\r\n",
					socket, host, port);
				spo_print(buf);
			}
			write(socket, msg, strlen(msg));
			sleep(10);
			close(socket);
		} else if (errno != EAGAIN) {
			perror("accept");
		}
	}

	// check for input from any terminal
	for (tun = 1; tun < 16; tun++) {
		for (bnr = 0; bnr < 16; bnr++) {
			t = &terminal[tun*16 + bnr];
			if (t->connected && t->socket > 2 && t->bufstate != readready) {
				cnt = socket_read(t, buf, sizeof buf);
				for (i=0; i<cnt; i++) {
					if (buf[i] == 0x0d) {
						// end of line
						t->buffer[t->bufidx] = 0;
						t->bufidx = 0;
						if (dtrace)
							printf("$DCC received %u/%u:%s\n", tun, bnr, t->buffer);
						t->abnormal = false;
						t->bufstate = readready;
						t->interrupt = true;
					} else if (buf[i] == 0x08) {
						// backspace
						if (t->bufidx > 0)
							t->bufidx--;
						socket_write(t, " \010", 2);
					} else if (buf[i] == 0x1b) {
						// cancel line
						t->bufidx = 0;
						socket_write(t, "X\015\012", 3);
					} else if (buf[i] >= ' ' && buf[i] <= 0x7e) {
						// printable, add to buffer
						if (t->bufidx < BUFLEN2)
							t->buffer[t->bufidx++] = buf[i];
					}
				}
			}
		}
	}
}

/***********************************************************************
* query DCC ready status
***********************************************************************/
BIT dcc_ready(unsigned index) {

	// initialize SPO if not ready
	if (!ready)
		dcc_init("");

	// test if incoming connection or data
	dcc_test_incoming();

	// check for a signal ready buffer
	if (terminal_search(NULL, NULL)) {
		// a terminal requesting service has been found - set Datacomm IRQ
		CC->CCI13F = true;
	}

	// finally return always ready
	return ready;
}

/***********************************************************************
* access the DCC
***********************************************************************/
void dcc_access(IOCU *u) {
	// terminal unit and buffer number are win word count
	unsigned tun = (u->d_wc >> 5) & 0xf;
	unsigned bnr = (u->d_wc >> 0) & 0xf;
	TERMINAL_T *t;

	// interrogate command
	BIT iro = u->d_control & CD_30_MI ? true : false;

	// reading
	BIT reading = u->d_control & CD_24_READ ? true : false;

#if TRACE_DCC
	print_iocw(stdout, u);
	printf("\n");
#endif

	// default good result
	u->d_result = 0;

	// type of access
	if (iro) {
		// interrogate

		// tun == 0: general query
		if (tun == 0) {
			// search if any terminal needs service
			terminal_search(&tun, &bnr);
		}
		// any found or specific query?
		if (tun > 0) {
			t = &terminal[tun*16 + bnr];
			switch (t->bufstate) {
			case readready:
				u->d_result = RD_24_READ;
				break;
			case writeready:
				u->d_result = RD_21_END;
				break;
			case inputbusy:
			case outputbusy:
				u->d_result = RD_20_ERR;
				break;
			case idle:
				break;
			default:
				u->d_result = RD_20_ERR | RD_18_NRDY;
			}
			// abnornal flag?
			if (t->abnormal)
				u->d_result |= RD_25_ABNORMAL;
			// clear pending IRQ
			t->interrupt = false;
		}
	} else if (reading) {
		// read buffer
		t = &terminal[tun*16 + bnr];
		if (t->connected) {
			int count;
			char *p = t->buffer;
			BIT gmset = false;
			p[BUFLEN] = 0; // fake max

			// connected - action depends on buffer state
			switch (t->bufstate) {
			case readready:
				if (dtrace)
					printf("$DCC READ(%u/%u)=\"", tun, bnr);

				// do whole words (8 characters) while we have more data
				while (*p >= ' ' || !gmset) {
					for (count=0; count<8; count++) {
						// note that p stays on the invalid char
						// causing the rest of the word to be filled with
						// GM
						if (*p >= ' ') {
							// printable char
							if (dtrace)
								printf("%c", *p);
							if (*p == '?')
								t->abnormal = true;
							u->ib = translatetable_ascii2bic[*p++ & 0x7f];
						} else {
							// EOL or other char, fill word with GM
							u->ib = 037;
							gmset = true;
						}
						u->w <<= 6;
						u->w |= u->ib;
					}
					// store the complete word
					main_write_inc(u);
				}
				if (dtrace)
					printf("\"\n");

				// abnormal flag?
				if (t->abnormal)
					u->d_result |= RD_25_ABNORMAL;
				// reset IRQ
				t->bufstate = idle;
				t->abnormal = false;
				t->interrupt = false;
				break;
			case writeready:
				u->d_result = RD_21_END;
				break;
			case inputbusy:
			case outputbusy:
				u->d_result = RD_21_END | RD_20_ERR;
				break;
			default:
				u->d_result = RD_21_END | RD_20_ERR | RD_18_NRDY;
			}
		} else {
			// not connected
			u->d_result = RD_21_END | RD_20_ERR | RD_18_NRDY;
		}
	} else {
		// write buffer
		t = &terminal[tun*16 + bnr];
		if (t->connected) {
			char buffer[BUFLEN];
			char *p = buffer;
			int chars = 0;
			BIT disc = false;
			// connected - action depends on buffer state
			switch (t->bufstate) {
			case idle:
			case writeready:
				if (dtrace)
					printf("$DCC WRITE(%u/%u)=\"", tun, bnr);

			loop:	main_read_inc(u);
				for (int count=0; count<8 && chars<BUFLEN; count++) {
					u->ob = (u->w >> 42) & 077;
					u->w <<= 6;
					if (u->ob == 037) {
						t->fullbuffer = false;
						goto done;
					}
					char c = translatetable_bic2ascii[u->ob];
					switch (c) {
					case 0x21: if (dtrace) printf("="); *p++ = '\n'; break;
					case 0x7b: if (dtrace) printf("<"); *p++ = '\r';  break;
					case 0x3c: if (dtrace) printf("_"); break;
					case 0x3e: if (dtrace) printf("|"); break;
					case 0x7d: if (dtrace) printf("~"); disc = true; break;
					default:   if (dtrace) printf("%c", c); *p++ = c;
					}
					chars++;
				}
				if (chars < BUFLEN)
					goto loop;
				t->fullbuffer = true;

			done:	if (dtrace)
					printf("\"\n");
				if (socket_write(t, buffer, p - buffer) < 0)
					goto closing;
				if (disc) {
					if (ctrace)
						printf("$DCC cande requests disconnect\n");
					sleep(10);
			closing:	socket_close(t);
					goto wrapup;
				}
				// set IRQ
				t->bufstate = outputbusy;
				t->timer = 0;
				t->abnormal = false;
			wrapup:	// abnormal flag?
				if (t->abnormal)
					u->d_result = RD_25_ABNORMAL;
				// buffer full flag?
				if (t->fullbuffer)
					u->d_result |= RD_23_ETYPE;
				//t->interrupt = true;
				break;
			case readready:
				u->d_result = RD_21_END;
				break;
			case inputbusy:
			case outputbusy:
				u->d_result = RD_21_END | RD_20_ERR;
				break;
			default:
				u->d_result = RD_21_END | RD_20_ERR | RD_18_NRDY;
			}
		} else {
			// not connected
			u->d_result = RD_21_END | RD_20_ERR | RD_18_NRDY;
		}
	}

	// return actual terminal and buffer numbers in word count
	u->d_wc = (tun << 5) | bnr;

#if TRACE_DCC
	print_ior(stdout, u);
	printf("\n");
#endif
}


