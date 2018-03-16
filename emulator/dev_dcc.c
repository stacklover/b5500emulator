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
* ff fd 03 DO Suppress Go Ahead
* ff fb 18 WILL Terminal Type
* ff fb 1f WILL Window Size
* ff fb 20 WILL Term Speed
* ff fb 21 WILL Remote Flow
* ff fb 22 WILL Linemode
* ff fb 27 WILL Send Locate
* ff fd 05 DO Opt Status
* ff fb 23
*
* fb (251) WILL
* fc (252) WONT
* fd (253) DO
* fe (254) DONT
* ff (255) IAC
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
#define	TN_END	240
#define	TN_SUB	250
#define	TN_WILL	251
#define	TN_WONT	252
#define	TN_DO	253
#define	TN_DONT	254
#define	TN_IAC	255

// Special Codes 
#define	EOM	'~'	// Marks Buffer End
#define	MODE	'!'	// toggles Control/Printable Mode at Line Discipline

// ASCII Control Codes
#define	STX	0x02
#define	ETX	0x03
#define	EOT	0x04
#define	ENQ	0x05
#define	ACK	0x06
#define	NAK	0x15

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

enum escape {
	none=0,
	had_iac,
	had_will,
	had_wont,
	had_do,
	had_dont,
	had_sub};

enum type {
	line=0,
	block};

typedef struct terminal {
	int socket;
	enum bufstate bufstate;
	enum escape escape;
	enum type type;
	BIT connected;
	BIT interrupt;
	BIT abnormal;
	BIT fullbuffer;
	BIT inmode;
	BIT outmode;
	unsigned eotcount;
	unsigned timer;
	unsigned bufidx;
	char buffer[BUFLEN2+1];
} TERMINAL_T;

static BIT ready;
static TERMINAL_T terminal[NUMTERM];	// we waste indices 0..15 here

// telnet
static int listen_socket1 = -1;		// LINE type terminals
static int listen_socket2 = -1;		// BLOCK type terminals
static BIT etrace = false;
static BIT dtrace = false;
static BIT ctrace = true;
static BIT telnet = false;

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
	t->escape = none;
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
				return -1;
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
				return -1;
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
* specify extra trace on/off
***********************************************************************/
static int set_etrace(const char *v, void *) {
	if (strcasecmp(v, "ON") == 0) {
		etrace = true;
	} else if (strcasecmp(v, "OFF") == 0) {
		etrace = false;
	} else {
		spo_print("$SPECIFY ON OR OFF\r\n");
		return 2; // FATAL
	}
	return 0; // OK
}

/***********************************************************************
* specify telnet on/off
***********************************************************************/
static int set_telnet(const char *v, void *) {
	if (strcasecmp(v, "ON") == 0) {
		telnet = true;
	} else if (strcasecmp(v, "OFF") == 0) {
		telnet = false;
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
	{"TELNET", set_telnet},
	{"CTRACE", set_ctrace},
	{"DTRACE", set_dtrace},
	{"ETRACE", set_etrace},
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
	}
	ready = true;
	return command_parser(dcc_commands, option);
}

/***********************************************************************
* handle new incoming TELNET session
***********************************************************************/
static const char *msg = "\r\nB5500 TIME SHARING - BUSY\r\nPLEASE CALL BACK LATER\r\n";
static void dcc_test_incoming(void) {
	int newsocket, cnt, i, j, k;
	struct sockaddr_in addr;
	socklen_t addrlen = sizeof(addr);
	unsigned port = 0;
	unsigned tun, bnr;
	char host[50];
	char buf[BUFLEN2];
	char buf2[BUFLEN2];
	TERMINAL_T *t;
	int optval;
	socklen_t optlen = sizeof(optval);

	// does TELNET1 need to be started?
	if (telnet && listen_socket1 <= 2) {
		// create socket for listening
		struct sockaddr_in addr;
		listen_socket1 = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
		if (listen_socket1 < 0) {
			perror("telnet socket");
			return;
		}
		// set REUSEADDR option
		optval = 1;
		optlen = sizeof(optval);
		if (setsockopt(listen_socket1, SOL_SOCKET, SO_REUSEADDR, &optval, optlen) < 0) {
			perror("setsockopt(SO_REUSEADDR)");
		}
		// bind it to port 23
		bzero((char*)&addr, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = INADDR_ANY;
		addr.sin_port = htons(23);
		if (bind(listen_socket1, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
			// perror("telnet bind");
			close(listen_socket1);
			listen_socket1 = -1;
			return;
		}
		// start the listening
		if (listen(listen_socket1, 4)) {
			perror("telnet listen");
			close(listen_socket1);
			listen_socket1 = -1;
			return;
		}
		printf("TELNET server1 listen socket %d\n", listen_socket1);
	} else

	// does TELNET1 need to be stopped?
	if (!telnet && listen_socket1 > 2) {
		close(listen_socket1);
		listen_socket1 = -1;
	}

	// does TELNET2 need to be started?
	if (telnet && listen_socket2 <= 2) {
		// create socket for listening
		struct sockaddr_in addr;
		listen_socket2 = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
		if (listen_socket2 < 0) {
			perror("telnet socket");
			return;
		}
		// set REUSEADDR option
		optval = 1;
		optlen = sizeof(optval);
		if (setsockopt(listen_socket2, SOL_SOCKET, SO_REUSEADDR, &optval, optlen) < 0) {
			perror("setsockopt(SO_REUSEADDR)");
		}
		// bind it to port 8023
		bzero((char*)&addr, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = INADDR_ANY;
		addr.sin_port = htons(8023);
		if (bind(listen_socket2, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
			// perror("telnet bind");
			close(listen_socket2);
			listen_socket2 = -1;
			return;
		}
		// start the listening
		if (listen(listen_socket2, 4)) {
			perror("telnet listen");
			close(listen_socket2);
			listen_socket2 = -1;
			return;
		}
		printf("TELNET server2 listen socket %d\n", listen_socket2);
	} else

	// does TELNET2 need to be stopped?
	if (!telnet && listen_socket2 > 2) {
		close(listen_socket2);
		listen_socket2 = -1;
	}

	// check for new connections
	if (listen_socket1 > 2) {
		newsocket = accept4(listen_socket1, (struct sockaddr*)&addr, &addrlen, SOCK_NONBLOCK | SOCK_CLOEXEC);
		if (newsocket > 2) {
			if (ctrace) {
				// get the peer info
				getnameinfo((struct sockaddr*)&addr, addrlen, host, sizeof(host), NULL, 0, 0);
				port = ntohs(addr.sin_port);
			}
			// set KEEPALIVE option
			optval = 1;
			optlen = sizeof(optval);
			if (setsockopt(newsocket, SOL_SOCKET, SO_KEEPALIVE, &optval, optlen) < 0) {
				perror("setsockopt(SO_KEEPALIVE)");
			}
			// find a free terminal to handle this
			for (tun = 1; tun < 2; tun++) {
				for (bnr = 0; bnr < 16; bnr++) {
					t = &terminal[tun*16 + bnr];
					if (!t->connected) {
						// free terminal found
						if (ctrace) {
							sprintf(buf, "+TELNET(%d) %s:%u LINE %u/%u\r\n",
								newsocket, host, port, tun, bnr);
							spo_print(buf);
						}
						t->bufidx = 0;
						t->socket = newsocket;
						t->abnormal = true;
						t->bufstate = writeready;
						t->escape = none;
						t->type = line;
						t->connected = true;
						t->interrupt = true;
						sprintf(buf, "%c%c%c", TN_IAC, TN_WILL, 1);
						socket_write(t, buf, 3);
						return;
					}
				}
			}
			// nothing found
			if (ctrace) {
				sprintf(buf, "+TELNET#%d %s:%u NO LINE. CLOSING\r\n",
					newsocket, host, port);
				spo_print(buf);
			}
			write(newsocket, msg, strlen(msg));
			sleep(1);
			close(newsocket);
		} else if (errno != EAGAIN) {
			perror("accept");
		}
	}

	// check for new connections
	if (listen_socket2 > 2) {
		newsocket = accept4(listen_socket2, (struct sockaddr*)&addr, &addrlen, SOCK_NONBLOCK | SOCK_CLOEXEC);
		if (newsocket > 2) {
			if (ctrace) {
				// get the peer info
				getnameinfo((struct sockaddr*)&addr, addrlen, host, sizeof(host), NULL, 0, 0);
				port = ntohs(addr.sin_port);
			}
			// set KEEPALIVE option
			optval = 1;
			optlen = sizeof(optval);
			if (setsockopt(newsocket, SOL_SOCKET, SO_KEEPALIVE, &optval, optlen) < 0) {
				perror("setsockopt(SO_KEEPALIVE)");
			}
			// find a free terminal to handle this
			for (tun = 2; tun < 3; tun++) {
				for (bnr = 0; bnr < 16; bnr++) {
					t = &terminal[tun*16 + bnr];
					if (!t->connected) {
						// free terminal found
						if (ctrace) {
							sprintf(buf, "+TELNET(%d) %s:%u LINE %u/%u\r\n",
								newsocket, host, port, tun, bnr);
							spo_print(buf);
						}
						t->bufidx = 0;
						t->socket = newsocket;
						t->abnormal = true;
						t->bufstate = writeready;
						t->escape = none;
						t->type = block;
						t->inmode = false;
						t->outmode = false;
						t->eotcount = 0;
						t->connected = true;
						t->interrupt = true;
						//sprintf(buf, "%c%c%c", TN_IAC, TN_WILL, 1);
						//socket_write(t, buf, 3);
						return;
					}
				}
			}
			// nothing found
			if (ctrace) {
				sprintf(buf, "+TELNET#%d %s:%u NO LINE. CLOSING\r\n",
					newsocket, host, port);
				spo_print(buf);
			}
			write(newsocket, msg, strlen(msg));
			sleep(1);
			close(newsocket);
		} else if (errno != EAGAIN) {
			perror("accept");
		}
	}

	// check for input from any connection
	for (tun = 1; tun < 16; tun++) {
		for (bnr = 0; bnr < 16; bnr++) {
			t = &terminal[tun*16 + bnr];
			if (t->connected && t->socket > 2 && t->bufstate != readready) {
				// check for data available
				cnt = socket_read(t, buf, sizeof buf);
				// cnt==0 means EOF (socket not connected)
				if (cnt == 0) {
					socket_close(t);
				}
				if (etrace && cnt > 0) {
					printf("$TELNET RX:");
					for (i=0; i<cnt; i++)
						printf(" %02x", buf[i]);
					printf("\n");
				}
				if (t->type == line) {
					// line mode
					// run each character
					j = 0;
					for (i=0; i<cnt; i++) {
						switch (t->escape) {
						case none:
							if (buf[i] == TN_IAC) {
								t->escape = had_iac;
							} else if (buf[i] == 0x0d) {
								// end of line
								t->buffer[t->bufidx] = 0;
								t->bufidx = 0;
								if (etrace)
									printf("$DCC received %u/%u:%s\n", tun, bnr, t->buffer);
								t->abnormal = false;
								t->bufstate = readready;
								t->interrupt = true;
								// echo
								if (j < BUFLEN2-2) {
									buf2[j++] = 0x0d;
									buf2[j++] = 0x0a;
								}
							} else if (buf[i] == 0x08 || buf[i] == 0x7f) {
								// backspace
								if (t->bufidx > 0) {
									t->bufidx--;
									//socket_write(t, " \010", 2);
									// echo
									if (j < BUFLEN2-3) {
										buf2[j++] = 0x08;
										buf2[j++] = 0x20;
										buf2[j++] = 0x08;
									}
								}
							} else if (buf[i] == 0x1b) {
								// cancel line
								t->bufidx = 0;
								//socket_write(t, "X\015\012", 3);
								if (j < BUFLEN2-3) {
									buf2[j++] = 'x';
									buf2[j++] = 0x0d;
									buf2[j++] = 0x0a;
								}
							} else if (buf[i] >= ' ' && buf[i] <= 0x7e) {
								// if printable, add to buffer
								k = translatetable_ascii2bic[buf[i] & 0x7f];
								k = translatetable_bic2ascii[k];
								if (t->bufidx < BUFLEN2) {
									t->buffer[t->bufidx++] = k;
									if (j < BUFLEN2-1) {
										buf2[j++] = k;
									}
								} else {
									if (j < BUFLEN2-1) {
										buf2[j++] = 0x07;
									}
								}
							}
							break;
						case had_iac:
							if (buf[i] == TN_IAC) {
								// data 0xff - ignore
								t->escape = none;
							} else if (buf[i] == TN_WILL) {
								t->escape = had_will;
							} else if (buf[i] == TN_WONT) {
								t->escape = had_wont;
							} else if (buf[i] == TN_DO) {
								t->escape = had_do;
							} else if (buf[i] == TN_DONT) {
								t->escape = had_dont;
							} else if (buf[i] == TN_SUB) {
								t->escape = had_sub;
							} else {
								t->escape = none;
							}
							break;
						case had_will:
							if (etrace)
								printf("<WILL%u>", buf[i]);
							t->escape = none;
							break;
						case had_wont:
							if (etrace)
								printf("<WONT%u>", buf[i]);
							t->escape = none;
							break;
						case had_do:
							if (etrace)
								printf("<DO%u>", buf[i]);
							t->escape = none;
							break;
						case had_dont:
							if (etrace)
								printf("<DONT%u>", buf[i]);
							t->escape = none;
							break;
						case had_sub:
							// ignore all until TN_END
							if (buf[i] == TN_END) {
								t->escape = none;
								if (etrace)
									printf("<END>");
							} else {
								if (etrace)
									printf("<IGN%u>", buf[i]);
							}
							break;
						} // switch
					} // for
					if (etrace && cnt > 0)
						printf("\n");
					if (j > 0)
						socket_write(t, buf2, j);
				} else {
					// block mode
					// run each character
					for (i=0; i<cnt; i++) {
						k = buf[i] & 0x7f;
						if (k >= 0x20) {
							// printable... if current mode is control...
							if (!t->inmode) {
								if (t->bufidx < BUFLEN2) {
									// ... change it
									t->buffer[t->bufidx++] = MODE;
								}
								t->inmode = true;
							}
							// enter printable char as is
							if (t->bufidx < BUFLEN2) {
								t->buffer[t->bufidx++] = k;
							}
						} else {
							// control... if current mode is printable...
							if (t->inmode) {
								if (t->bufidx < BUFLEN2) {
									// ... change it
									t->buffer[t->bufidx++] = MODE;
								}
								t->inmode = false;
							}
							// enter control char as printable
							if (t->bufidx < BUFLEN2) {
								t->buffer[t->bufidx++] = k + ' ';
							}
						}
						// special codes that close buffer
						if (k==EOT || k==ETX || k==ENQ || k==ACK || k==NAK) {
							// must send to system, mark end of buffer
							t->buffer[t->bufidx] = 0;
							t->bufidx = 0;
							t->abnormal = false;
							t->bufstate = readready;
							t->interrupt = true;
						}
					} // for
				} // mode
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
							if (t->type == line && *p == '?')
								t->abnormal = true;
							u->ib = translatetable_ascii2bic[*p++ & 0x7f];
						} else {
							// EOL or other char, fill word with GM
							u->ib = 037;
							gmset = true;
						}
						put_ib(u);
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
			char c;
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
					get_ob(u);
					c = translatetable_bic2ascii[u->ob];
					if (t->type == line) {
						// some BIC codes have special meanings on output, handle those
						switch (u->ob) {
						case 016: // BIC: greater than
							if (dtrace)
								printf("<xon>");
							*p++ = 0x11;
							break;
						case 017: // BIC: greater or equal
							if (dtrace)
								printf("<dis>");
							disc = true;
							break;
						case 036: // BIC: less than
							if (dtrace)
								printf("<ro>");
							*p++ = 0xff;
							break;
						case 037: // BIC: left arrow
							if (dtrace)
								printf("<eom>");
							t->fullbuffer = false;
							goto done;
						case 057: // BIC: less or equal
							if (dtrace)
								printf("<cr>");
							*p++ = '\r';
							break;
						case 074: // BIC: not equal
							if (dtrace)
								printf("<lf>");
							*p++ = '\n';
							break;
						default:
							if (dtrace)
								printf("%c", c);
							*p++ = c;
						}
					} else {
						// buffer end ?
						if (c == EOM) {
							t->fullbuffer = false;
							t->outmode = false;
							goto done;
						}
						if (dtrace)
							printf("%c", c);
						if (c == MODE) {
							t->outmode = !t->outmode;
						} else if (t->outmode) {
							*p++ = c;
						} else {
							*p++ = c & 0x1f;
							// count EOTs
							if (c == 0x24) {
								if (++t->eotcount >= 3)
									disc = true;
							} else {
								t->eotcount = 0;
							}
						}
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
					if (ctrace) {
						sprintf(t->buffer, "+TELNET(%d) CANDE DISC\n", t->socket);
						spo_print(t->buffer);
					}
					sleep(1);
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


