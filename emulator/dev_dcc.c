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
#include "circbuffer.h"
#include "telnetd.h"

#define NUMTERM 32
#define	BUFLEN 112
#define	BUFLEN2 200

#define TRACE_DCC 0

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

// tnr/bnr to idx and back
#define	IDX(tnr,bnr)	(((tun)-1)*16+(bnr))
#define	TUN(idx)	((idx)/16+1)
#define	BNR(idx)	((idx)%16)

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

enum type {
	line=0,
	block};

typedef struct terminal {
	TELNET_SESSION_T session;
	enum bufstate bufstate;
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
static TERMINAL_T terminal[NUMTERM];

// telnet
static TELNET_SERVER_T server1;		// Port   23 - LINE type terminals
static TELNET_SERVER_T server2;		// Port 8023 - BLOCK type terminals
static BIT etrace = false;
static BIT dtrace = false;
static BIT ctrace = true;
static BIT telnet = false;

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
	unsigned idx;

	// list all connected terminals
	for (idx = 0; idx < NUMTERM; idx++) {
		t = &terminal[idx];
		if (t->connected) {
			sprintf(buf,"%u/%u S=%d T=%d B=%d I=%u A=%u F=%u\r\n",
				TUN(idx), BNR(idx),
				t->session.socket, (int)t->type, (int)t->bufstate,
				t->interrupt, t->abnormal, t->fullbuffer);
			spo_print(buf);
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
	unsigned idx;
	TERMINAL_T *t;

	for (idx = 0; idx < NUMTERM; idx++) {
		t = &terminal[idx];
		// trigger late output IRQ
		if (t->bufstate == outputbusy) {
			if (++t->timer > TIMEOUT) {
				t->bufstate = t->fullbuffer ? writeready : idle;
				t->interrupt = true;
			}
		}
		if (t->interrupt)  {
			// something found
			if (ptun) *ptun = TUN(idx);
			if (pbnr) *pbnr = BNR(idx);
			return true;
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
		int i;
		// ignore all SIGPIPEs
		signal(SIGPIPE, SIG_IGN);
		// init data structures
		telnet_server_clear(&server1);
		telnet_server_clear(&server2);
		for (i=0; i<NUMTERM; i++) {
			memset(&terminal[i], 0, sizeof(TERMINAL_T));
			telnet_session_clear(&terminal[i].session);
		}
	}
	ready = true;
	return command_parser(dcc_commands, option);
}

/***********************************************************************
* handle new incoming TELNET session
***********************************************************************/
static void new_connection(int newsocket, struct sockaddr_in *addr, enum type type) {
	static const char *msg = "\r\nB5500 TIME SHARING - BUSY\r\nPLEASE CALL BACK LATER\r\n";
	socklen_t addrlen = sizeof(*addr);
	char host[40];
	unsigned port;
	int idx, beg, end;
	TERMINAL_T *t;
	char buf[200];

	// get the peer info
	getnameinfo((struct sockaddr*)addr, addrlen, host, sizeof(host), NULL, 0, 0);
	port = ntohs(addr->sin_port);

	// determine terminal range
	if (type == line) {
		beg = 0;
		end = 15;
	} else {
		beg = 16;
		end = 31;
	}

	// find a free terminal to handle this
	for (idx = beg; idx <= end && idx < NUMTERM; idx++) {
		t = &terminal[idx];
		if (!t->connected) {
			// free terminal found
			if (ctrace) {
				sprintf(buf, "+LINE %u/%u TELNET(%d) FROM %s:%u\r\n",
					TUN(idx), BNR(idx), newsocket, host, port);
				spo_print(buf);
			}
			telnet_session_open(&(t->session), newsocket);
			t->bufidx = 0;
			t->abnormal = true;
			t->bufstate = writeready;
			t->type = type;
			t->inmode = false;
			t->outmode = false;
			t->eotcount = 0;
			t->connected = true;
			t->interrupt = true;
			return;
		}
	}
	// nothing found
	if (ctrace) {
		sprintf(buf, "+LINE BUSY - TELNET(%d) FROM %s:%u REJECTED\r\n",
			newsocket, host, port);
		spo_print(buf);
	}
	write(newsocket, msg, strlen(msg));
	sleep(1);
	close(newsocket);
}
 
/***********************************************************************
* handle TELNET server
***********************************************************************/
static void dcc_test_incoming(void) {
	int newsocket = -1, cnt, i, j, k;
	struct sockaddr_in addr;
	unsigned idx;
	char buf[BUFLEN2];
	char buf2[BUFLEN2];
	TERMINAL_T *t;

	if (telnet && server1.socket <= 2) {
		// SERVER1 needs to be started
		telnet_server_start(&server1, 23);
	} else if (!telnet && server1.socket > 2) {
		// SERVER1 needs to be stopped
		telnet_server_stop(&server1);
	}

	if (telnet && server2.socket <= 2) {
		// SERVER2 needs to be started
		telnet_server_start(&server2, 8023);
	} else if (!telnet && server2.socket > 2) {
		// SERVER2 needs to be stopped
		telnet_server_stop(&server2);
	}

	// poll both servers for new connections
	if (server1.socket > 2) {
		newsocket = telnet_server_poll(&server1, &addr);
		if (newsocket > 0)
			new_connection(newsocket, &addr, line);
	}
	if (server2.socket > 2) {
		newsocket = telnet_server_poll(&server2, &addr);
		if (newsocket > 0)
			new_connection(newsocket, &addr, block);
	}
	
	// check for input from any connection
	for (idx = 0; idx < NUMTERM; idx++) {
		t = &terminal[idx];
		if (t->connected) {
			if (t->session.socket > 2) {
				// socket still open
				if (t->bufstate != readready) {
					// check for data available
					cnt = telnet_session_read(&t->session, buf, sizeof buf);
					if (cnt < 0) {
						// non recoverable error - session was closed
						goto isclosed;
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
							if (buf[i] == 0x0d) {
								// end of line
								t->buffer[t->bufidx] = 0;
								t->bufidx = 0;
								if (etrace)
									printf("+LINE %u/%u RECEIVED %s\n",
										TUN(idx), BNR(idx), t->buffer);
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
						} // for
						if (etrace && cnt > 0)
							printf("\n");
						if (j > 0)
							telnet_session_write(&t->session, buf2, j);
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
			} else {
isclosed:			// socket was closed
				if (ctrace) {
					sprintf(buf, "+LINE %u/%u CLOSED\r\n",
						TUN(idx), BNR(idx));
					spo_print(buf);
				}
				t->interrupt = true;
				t->abnormal = true;
				t->fullbuffer = false;
				t->bufstate = notready;
				t->connected = false;
				t->buffer[0] = 0;
				t->bufidx = 0;
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
	unsigned idx;
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
			idx = IDX(tun, bnr);
			if (idx < NUMTERM) {
				t = &terminal[idx];
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
		}
	} else if (reading) {
		// read buffer
		idx = IDX(tun, bnr);
		if (idx < NUMTERM && (t = &terminal[idx])->connected) {
			int count;
			char *p = t->buffer;
			BIT gmset = false;
			p[BUFLEN] = 0; // fake max

			// connected - action depends on buffer state
			switch (t->bufstate) {
			case readready:
				if (dtrace)
					printf("+LINE %u/%u READ \"", tun, bnr);

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
		idx = IDX(tun, bnr);
		if (idx < NUMTERM && (t = &terminal[idx])->connected) {
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
					printf("+LINE %u/%u WRIT \"", tun, bnr);

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
				if (telnet_session_write(&t->session, buffer, p - buffer) < 0)
					goto closing;
				if (disc) {
					if (ctrace) {
						sprintf(t->buffer, "+TELNET(%d) CANDE DISC\n", t->session.socket);
						spo_print(t->buffer);
					}
					sleep(1);
			closing:	telnet_session_close(&t->session);
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


