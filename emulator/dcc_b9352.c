/***********************************************************************
* b5500emulator
************************************************************************
* Copyright (c) 2018, Reinhard Meyer, DL5UY
* Licensed under the MIT License,
*       see LICENSE
************************************************************************
* b5500 data communication emulation (DCC)
*
* This emulates a B9352 Terminal
*
************************************************************************
* 2018-04-01  R.Meyer
*   Frame from dev_dcc.c
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
#include "circbuffer.h"
#include "dcc.h"

/***********************************************************************
* Convert sysbuf from 6 Bit BIC(in ASCII) to 8 Bit ASCII
***********************************************************************/
static BIT convert6to8(TERMINAL_T *t) {
	int iptr, optr;
	char ch;
	BIT disc = false;

	printf("converting sysbuf 6 to 8 (%d)", t->sysidx);
	for (iptr=0, optr=0; iptr<t->sysidx; iptr++) {
		ch = t->sysbuf[iptr];
		// MODE char toggles mode
		if (ch == MODE) {
			t->outmode = !t->outmode;
			// if the last char was MODE too, enter the MODE char
			if (t->outlastwasmode) {
				t->sysbuf[optr++] = MODE;
				t->outlastwasmode = false;
			} else {
				t->outlastwasmode = true;
			}
		} else if (t->outmode) {
			t->sysbuf[optr++] = ch;
			t->outlastwasmode = false;
		} else {
			ch &= 0x1f;
			t->sysbuf[optr++] = ch;
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

	t->sysidx = optr;
	printf("=>(%d)\n", t->sysidx);
	return disc;
}

/***********************************************************************
* Convert sysbuf from 8 Bit ASCII to 6 Bit BIC(in ASCII)
***********************************************************************/
static void convert8to6(TERMINAL_T *t) {
	int iptr;
	char ch;

	printf("converting keybuf 8 to sysbuf 6 (%d)", t->keyidx);
	iptr = 0;
	t->sysidx = 0;

	while (iptr < t->keyidx) {
		ch = t->keybuf[iptr++];
		printf("[%02x]", ch);
		if (ch >= ' ') {
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
				t->sysbuf[t->sysidx++] = ch + ' ';
		}
	} // while
	printf("=>(%d)\n", t->sysidx);
}

/***********************************************************************
* B9352 Emulation Write (data in sysbuf)
* Returns number of bytes used or -1 on non-recoverable error
***********************************************************************/
void b9352_emulation_write(TERMINAL_T *t) {
	int iptr;
	char ch;
	char buf[OUTBUFSIZE];
	char *p = buf;

	// convert
	BIT disc = convert6to8(t);

	// if no emulation, just send via TELNET
	if (t->em == em_none) {
		// try to write
		iptr = telnet_session_write(&t->session, t->sysbuf, t->sysidx);

		// reason to disconnect?
		if (disc || iptr != t->sysidx) {
			if (disc && dtrace) {
				sprintf(buf, "+DISC REQ (%d)\n", t->session.socket);
				spo_print(buf);
			}
			telnet_session_close(&t->session);
		}

		// make sysbuf available again
		t->bufstate = t->fullbuffer ? writeready : idle;
		t->sysidx = 0;
		t->interrupt = true;
		return;
	}

	// here we run the terminal end of the line discipline
	printf("{interpreting sysbuf(%d)}", t->sysidx);
	iptr=0;
	while (iptr < t->sysidx) {
		ch = t->sysbuf[iptr++];

		// some characters are "terminal" characters
		// and hence the sysbuf should be exhausted at this point
		if (ch == ACK || ch == NAK || ch == ENQ || ch == EOT || ch == ETX) {
			if (iptr != t->sysidx)
				printf("{sysbuf not exhausted}");
			// all those characters require a response, so
			// clear and reserve sysbuf now
			t->bufstate = inputbusy;
			t->sysidx = 0;

			// now handle those terminal characters
			if (ch == ACK) {
				// received ACK
				printf("<ACK>");

				// we may send now, if we did ask to do so
				if (t->lds == lds_sentenq) {
					// ACK for our REQ
					// copy and convert 8 to 6 keybuf to sysbuf
					convert8to6(t);

					// mark keybuf empty
					t->keyidx = 0;

					// next state is datasent, waiting for ACK
					t->lds = lds_sentdata;

					// redisplay();
				} else {
					if (t->lds != lds_sentdata)
						printf("{unexpected ACK}");

					// ACK for our data or unexpected
					printf("[EOT]\n");

					// prepare response
					t->sysbuf[t->sysidx++] = EOT + ' ';

					// next state is idle
					t->lds = lds_idle;
				}
			} else if (ch == NAK) {
				// system negative acked - what do we do??
				printf("<NAK>");
				t->lds = lds_idle;
			} else if (ch == EOT) {
				// system finished sending, turn over to display
				printf("<EOT>");

				// EOT and we got nothing to send
				printf("[EOT]\n");

				// prepare response
				t->sysbuf[t->sysidx++] = EOT + ' ';

				// next state is idle
				t->lds = lds_idle;
			} else if (ch == ENQ) {
				// system queries status
				printf("<ENQ>");
				printf("[ACK]");

				// prepare response (we can always receive, right?)
				t->sysbuf[t->sysidx++] = ACK + ' ';

				// next state is waiting for data
				t->lds = lds_recvenq;
			} else if (ch == ETX) {
				// data ends
				printf("<ETX>");
				printf("[ACK]\n");

				// prepare response
				t->sysbuf[t->sysidx++] = ACK + ' ';

				// next state is waiting for more data or EOT
				t->lds = lds_recvenq;
			}

			// if we prepared a response, mark sysbuf ready to send response
			if (t->sysidx > 0) {
				t->abnormal = false;
				t->bufstate = readready;
				t->interrupt = true;
			} else {
				t->abnormal = false;
				t->bufstate = idle;
				t->interrupt = true;
			}
			goto finish;
		}

		// all other chars handled here
		if (ch == STX) {
			// data begins
			printf("<STX>");
			t->lds = lds_recvdata;
		} else {
			if (t->lds == lds_recvdata) {
				// text to display or allowed control chars
				switch (ch) {
				case CR:
					*p++ = CR;
					t->lfpending = true;
					break;
				case LF:
					*p++ = CR;
					*p++ = LF;
					t->lfpending = false;
					break;
				case DC3:
					t->lfpending = false;
					break;
				case DC4:
					t->paused = true;
					*p++ = BEL;
					break;
				default:
					if (ch >= ' ') {
						if (t->lfpending) {
							*p++ = LF;
							t->lfpending = false;
						}
						*p++ = ch;
					}
				}
			} else {
				// chars outside STX/ETX
				printf("{%02x}", ch);
			}
		}
	} // while

	// make sysbuf available again
	t->bufstate = t->fullbuffer ? writeready : idle;
	t->sysidx = 0;
	t->interrupt = true;

finish:
	// try to write to terminal
	iptr = telnet_session_write(&t->session, buf, p-buf);

	// reason to disconnect?
	if (disc || iptr != (p-buf)) {
		if (disc && dtrace) {
			sprintf(buf, "+DISC REQ (%d)\n", t->session.socket);
			spo_print(buf);
		}
		telnet_session_close(&t->session);
	}

	// conclude debug and leave
	printf("\n");
}

/***********************************************************************
* B9352 Emulation Poll
* Check for data from TELNET client
***********************************************************************/
int b9352_emulation_poll(TERMINAL_T *t) {
	char ibuf[10], obuf[30], ch;
	int cnt, idx, odx;

	// simple case with no emulation, assemble into keybuf
	// until terminal char found
	if (t->em == em_none) {
		// only try to receive if sysbuf is idle or inputbusy
		if (t->bufstate != idle && t->bufstate != inputbusy)
			return 0;

		// check for data available
		cnt = telnet_session_read(&t->session, ibuf, sizeof ibuf);
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
		printf("[ENQ]");
		t->sysbuf[0] = ENQ + ' ';
		t->sysidx = 1;
		t->abnormal = false;
		t->bufstate = readready;
		t->interrupt = true;
		t->lds = lds_sentenq;
		return 0;
	}

	// check for data available
	cnt = telnet_session_read(&t->session, ibuf, sizeof ibuf);
	if (cnt <= 0) // non recoverable error or no data
		return cnt;

	// teletype emulation, simple line editing in keybuf
	if (t->em == em_teletype) {
		odx = 0;
		// add STX at front of keybuf
		if (t->keyidx == 0)
			t->keybuf[t->keyidx++] = STX;
		// loop over all what we received
		for (idx=0; idx<cnt; idx++) {
			ch = ibuf[idx];
			switch (ch) {
			case CR:
				// end of line
				// add ETX
				t->keybuf[t->keyidx++] = ETX;
				// mark send ready
				t->lds = lds_sendrdy;
				// echo CR/LF when not paued
				if (t->paused) {
					t->paused = false;
				} else {
					obuf[odx++] = CR;
					obuf[odx++] = LF;
				}
				// line complete - leave this loop
				goto finish;
			case BS:
			case RUBOUT:
				// backspace
				if (t->keyidx > 1) {
					t->keyidx--;
					// echo
					obuf[odx++] = BS;
					obuf[odx++] = ' ';
					obuf[odx++] = BS;
				}
				break;
			case ESC:
				// cancel line
				t->keyidx = 0;
				t->keybuf[t->keyidx++] = STX;
				// echo
				obuf[odx++] = '~';
				obuf[odx++] = CR;
				obuf[odx++] = LF;
				break;
			default:
				if (ch >= ' ' && ch <= 0x7e && t->keyidx < KEYBUFSIZE) {
					// if printable, add to keybuf
					ch = translatetable_ascii2bic[ch&0x7f];
					ch = translatetable_bic2ascii[ch&0x7f];
					t->keybuf[t->keyidx++] = ch;
					// echo
					obuf[odx++] = ch;
				} else {
					obuf[odx++] = BEL;
				}
			}
		}

finish:
		if (telnet_session_write(&t->session, obuf, odx) < 0)
			return -1;

		return idx;	
	}

	// default exit
	return 0;
}


