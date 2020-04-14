/***********************************************************************
* b5500emulator
************************************************************************
* Copyright (c) 2018, Reinhard Meyer, DL5UY
* Licensed under the MIT License,
*       see LICENSE
************************************************************************
* b5500 data communication emulation (DCC)
*
* LINE DISCIPLINE CONTENTION
*
************************************************************************
* 2018-04-01  R.Meyer
*   Frame from dev_dcc.c
* 2018-04-21  R.Meyer
*   factored out all physcial connection (PC), all line discipline(LD)
*   and all emulation (EM) functionality to spearate files
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
#include "circbuffer.h"
#include "dcc.h"

/***********************************************************************
* Convert sysbuf from 6 Bit BIC(in ASCII) to outbuf 8 Bit ASCII
* release sysbuf
***********************************************************************/
static BIT convert6to8(TERMINAL_T *t) {
	int iptr;
	char ch;
	BIT disc = false;

	t->outidx = 0;
	for (iptr=0; iptr<t->sysidx; iptr++) {
		ch = t->sysbuf[iptr];
		// MODE char toggles mode
		if (ch == MODE) {
			t->outmode = !t->outmode;
			// if the last char was MODE too, enter the MODE char
			if (t->outlastwasmode) {
				t->outbuf[t->outidx++] = MODE;
				t->outlastwasmode = false;
			} else {
				t->outlastwasmode = true;
			}
		} else if (t->outmode) {
			t->outbuf[t->outidx++] = ch;
			t->outlastwasmode = false;
		} else {
			ch &= 0x1f;
			t->outbuf[t->outidx++] = ch;
			t->outlastwasmode = false;
			// count EOTs
			if (ch == EOT) {
				if (++t->eotcount >= 3)
					disc = true;
			} else {
				t->eotcount = 0;
			}
		}
	} // for

	// if not a full sysbuf - assume mode is back to control
	if (!t->fullbuffer) {
		t->outmode = false;
		t->outlastwasmode = false;
	}

	// make sysbuf available again
	t->sysidx = 0;
	t->bufstate = t->fullbuffer ? writeready : idle;
	t->interrupt = true;

	return disc;
}

/***********************************************************************
* Convert inbuf from 8 Bit ASCII to sysbuf 6 Bit BIC(in ASCII)
***********************************************************************/
static void convert8to6(TERMINAL_T *t) {
	int ptr;
	char ch;

	// clear and reserve sysbuf now
	t->bufstate = inputbusy;
	t->sysidx = 0;
	t->inmode = false;

	ptr = 0;
	while (ptr < t->inidx) {
		ch = t->inbuf[ptr++];
		if (ch >= 0x20) {
			// printable char
			// change mode if current mode is control...
			if (!t->inmode) {
				if (t->sysidx < SYSBUFSIZE) {
					// ... change it
					t->sysbuf[t->sysidx++] = MODE;
				}
				t->inmode = true;
			}
			// enter printable char as is, double it when it is "!"
			if (t->sysidx+1 < SYSBUFSIZE) {
				t->sysbuf[t->sysidx++] = ch;
				if (ch == '!')
					t->sysbuf[t->sysidx++] = ch;
			}
		} else {
			// control char
			// change mod if current mode is printable...
			if (t->inmode) {
				if (t->sysidx < SYSBUFSIZE) {
					// ... change it
					t->sysbuf[t->sysidx++] = MODE;
				}
				t->inmode = false;
			}
			// convert control char to ASCII printable
			if (t->sysidx < SYSBUFSIZE)
				t->sysbuf[t->sysidx++] = ch + 0x20;
		}
	} // while
	// make sure we finish in control mode
	if (t->inmode) {
		if (t->sysidx < SYSBUFSIZE) {
			// ... change it
			t->sysbuf[t->sysidx++] = MODE;
		}
		t->inmode = false;
	}

	// buffer is ready to be ready by the system
	t->abnormal = false;
	t->bufstate = readready;
	t->interrupt = true;
}

/***********************************************************************
* line discipline contention write (data in sysbuf)
* Returns number of bytes used or -1 on non-recoverable error
***********************************************************************/
void ld_write_contention(TERMINAL_T *t) {
	int ptr;
	char ch;
	BIT error = false;
	//char obuf[30];

	// convert sysbuf to outbuf, release sysbuf
	BIT disc = convert6to8(t);

	// reset inbuf index
	t->inidx = 0;

	// here we run the terminal end of the line discipline
	if (etrace)
		printf("+DATA %s ", t->name);
	ptr = 0;
	while (ptr < t->outidx && !error) {
		ch = t->outbuf[ptr++];

		// some characters are "terminal" characters
		if (ch == ACK || ch == NAK || ch == ENQ || ch == EOT || ch == ETX) {
			// and hence the sysbuf should be exhausted at this point
			if (ptr != t->outidx)
				printf("{outbuf not exhausted}");

			// now handle those terminal characters
			if (ch == ACK) {
				// received ACK
				if (etrace)
					printf("<ACK>");

				// we may send now, if we did ask to do so
				if (t->lds == lds_sentenq) {
					// ACK for our REQ
					t->inbuf[t->inidx++] = STX;
					memcpy(t->inbuf+t->inidx, t->keybuf, t->keyidx);
					t->inidx += t->keyidx;
					t->inbuf[t->inidx++] = ETX;

					if (etrace) {
						int i;
						char chx;
						printf("[STX]");
						for (i = 1; i < t->inidx-1; i++) {
							chx = t->inbuf[i];
							if (isprint(chx))
								printf("%c", chx);
							else
								printf("<%02x>", chx);
						}
						printf("[ETX]");
					}
					// mark keybuf empty
					t->keyidx = 0;

					// next state is datasent, waiting for ACK
					t->lds = lds_sentdata;

					// redisplay();
				} else {
					if (t->lds != lds_sentdata)
						printf("{unexpected ACK}");

					// ACK for our data or unexpected
					if (etrace)
						printf("[EOT]");

					// prepare response
					t->inbuf[t->inidx++] = EOT;

					// next state is idle
					t->lds = lds_idle;
				}
			} else if (ch == NAK) {
				// system negative acked - what do we do??
				if (etrace)
					printf("<NAK>");
				t->lds = lds_idle;
			} else if (ch == EOT) {
				// system finished sending, turn over to display
				if (etrace)
					printf("<EOT>");

				// EOT and we got nothing to send
				if (etrace)
					printf("[EOT]");

				// prepare response
				t->inbuf[t->inidx++] = EOT;

				// next state is idle
				t->lds = lds_idle;
			} else if (ch == ENQ) {
				// system queries status
				if (etrace)
					printf("<ENQ>[ACK]");

				// prepare response (we can always receive, right?)
				t->inbuf[t->inidx++] = ACK;

				// next state is waiting for data
				t->lds = lds_recvenq;
			} else if (ch == ETX) {
				// data ends
				if (etrace)
					printf("<ETX>[ACK]");

				// prepare response
				t->inbuf[t->inidx++] = ACK;

				// next state is waiting for more data or EOT
				t->lds = lds_recvenq;

				// in B9352 emulation, we need to store the ETX
				error = b9352_output(t, ETX);
			}

			// if we prepared a response, mark sysbuf ready to send response
			if (t->inidx > 0) {
				// copy and convert 8 inbuf to 6 sysbuf and hand over to system
				convert8to6(t);
			}
			goto finish;
		}

		// all other chars handled here
		if (ch == STX) {
			// data begins
			if (etrace)
				printf("<STX>");
			t->lds = lds_recvdata;
		} else {
			if (t->lds == lds_recvdata) {
				// send to terminal
				if (etrace) {
					if (isprint(ch))
						printf("%c", ch);
					else
						printf("<%02x>", ch);
				}
				error = b9352_output(t, ch);
			} else {
				// chars outside STX/ETX
				printf("{%02x}", ch);
			}
		}
	} // while

finish:
	if (etrace)
		printf("\n");

	// reason to disconnect?
	if (disc || error) {
		if (disc && dtrace) {
			sprintf(t->outbuf, "+DREQ %s\r\n", t->name);
			spo_print(t->outbuf);
		}
		t->pcs = pcs_failed;
	}
}

/***********************************************************************
* line discipline contention poll
* check for status changes or for data from client
***********************************************************************/
int ld_poll_contention(TERMINAL_T *t) {
	char ibuf[10], ch;
	int cnt = 0, idx;

	// simple case with no emulation, assemble into keybuf
	// until terminal char found
	if (t->em == em_none) {
		// only try to receive if sysbuf is idle or inputbusy
		if (t->bufstate != idle && t->bufstate != inputbusy)
			return 0;

		// check for data available
		switch (t->pc) {
		case pc_telnet: cnt = pc_telnet_read(t, ibuf, sizeof ibuf); break;
		case pc_serial: cnt = pc_serial_read(t, ibuf, sizeof ibuf); break;
		case pc_canopen: cnt = pc_canopen_read(t, ibuf, sizeof ibuf); break;
		default: return -1;
		}

		if (cnt <= 0) // non recoverable error or no data
			return cnt;

		// mark sysbuf as busy and prepare it
		if (t->bufstate == idle) {
			t->bufstate = inputbusy;
			t->sysidx = 0;
		}

		// loop over all what we received
		for (idx=0; idx<cnt; idx++) {
			ch = ibuf[idx];
			if (t->keyidx < KEYBUFSIZE-1)
				t->keybuf[t->keyidx++] = ch;
			// stop when received terminal char
			if (ch == ACK || ch == NAK || ch == ENQ || ch == EOT || ch == ETX) {
				convert8to6(t);
				t->keyidx = 0;
				t->abnormal = false;
				t->bufstate = readready;
				t->interrupt = true;
			}
		}
		return idx;	
	}

	// emulations handled here

	// keybuf ready for sending and sysbuf idle?
	if (t->lds == lds_sendrdy && t->bufstate == idle) {
		if (etrace)
			printf("+DATA %s [ENQ]\n", t->name);
		t->sysbuf[0] = ENQ + 0x20;
		t->sysidx = 1;
		t->abnormal = false;
		t->bufstate = readready;
		t->interrupt = true;
		t->lds = lds_sentenq;
		return 0;
	}

	// check for data available
	cnt = telnet_session_read(&t->tsession, ibuf, sizeof ibuf);
	if (cnt <= 0) // non recoverable error or no data
		return cnt;

	// B9352 terminal emulation with external ANSI terminal
	// loop over all what we received
	for (idx=0; idx<cnt; idx++) {
		ch = ibuf[idx];
		if (b9352_input(t, ch))
			break;
	}
	return idx;	
}


