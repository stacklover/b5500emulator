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
* ANSI control codes
***********************************************************************/
#define _SO_	"\033[7m"	// inverse display on
#define _SI_	"\033[27m"	// inverse display off
#define	_EREOL_	"\033[K"	// erase to end of line
#define	_EREOS_	"\033[J"	// erase to end of screen
#define	_BS_	"\010"		// backup one char
#define	_CR_	"\015"		// return to begin of SAME line
#define	_LF_	"\012"		// one line down
#define	_HOME_	"\033[H"	// cursor to top left corner
#define	_CLS_	"\033[H\033[2J"	// clear screen and cursor to top left corner
#define	_UP_	"\033[A"	// one line up
#define	_GOTO_	"\033[%u;%uH"	// set cursor to row and column

/***********************************************************************
* Special UTF-8 characters
***********************************************************************/
#define	_STAR_	"~" //"\302\244"
#define	_LE_	"{" //"\302\253"
//#define	_NOT_	"!" //"\302\254"
#define	_PARA_	"^" //"\302\266"
#define	_GE_	"}" //"\302\273"
//#define	_MULT_	"x" //"\303\230"

/***********************************************************************
* Convert sysbuf from 6 Bit BIC(in ASCII) to outbuf 8 Bit ASCII
* release sysbuf
***********************************************************************/
static BIT convert6to8(TERMINAL_T *t) {
	int iptr;
	char ch;
	BIT disc = false;

	if (etrace)
		printf("converting sysbuf 6 to 8 (%d)\n", t->sysidx);
	t->outidx = 0;
	for (iptr=0; iptr<t->sysidx; iptr++) {
		ch = t->sysbuf[iptr];
		if (etrace)
			printf("%02x ", ch);
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

	if (etrace)
		printf("=>(%d)\n", t->outidx);
	return disc;
}

/***********************************************************************
* Convert inbuf from 8 Bit ASCII to sysbuf 6 Bit BIC(in ASCII)
***********************************************************************/
static void convert8to6(TERMINAL_T *t) {
	int ptr;
	char ch;

	if (etrace)
		printf("converting inbuf 8 to sysbuf 6 (%d)", t->inidx);

	// clear and reserve sysbuf now
	t->bufstate = inputbusy;
	t->sysidx = 0;
	t->inmode = false;

	ptr = 0;
	while (ptr < t->inidx) {
		ch = t->inbuf[ptr++];
		if (etrace)
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

	if (etrace)
		printf("=>(%d)\n", t->sysidx);
}

/***********************************************************************
* wrap cursor to be in bounds
***********************************************************************/
static void cursorwrap(TERMINAL_T *t) {
	char buf[20];
	int len;

	while (t->scridx < 0) {
		t->scridx += COLS;
		t->scridy--;
	}
	while (t->scridx >= COLS) {
		t->scridx -= COLS;
		t->scridy++;
	}
	if (t->scridy == ROWS) {
		// scroll up one line
		if (etrace)
			printf("scroll up!\n");
		len = sprintf(buf, _LF_);
		len = telnet_session_write(&t->session, buf, len);
		t->scridy = ROWS-1;
	}
	while (t->scridy < 0) {
		t->scridy += ROWS;
	}
	while (t->scridy >= ROWS) {
		t->scridy -= ROWS;
	}
}

/***********************************************************************
* position cursor on ANSI screen
***********************************************************************/
static void cursormove(TERMINAL_T *t) {
	char buf[20];
	int len;
	len = sprintf(buf, _GOTO_, t->scridy+1, t->scridx+1);
	len = telnet_session_write(&t->session, buf, len);
}

/***********************************************************************
* erase to end of line
***********************************************************************/
static void erasetoeol(TERMINAL_T *t) {
	char buf[20];
	int len;
	//memset(t->scrbuf[t->scridy], ' ', ROWS - t->scridx);
	len = sprintf(buf, _EREOL_);
	len = telnet_session_write(&t->session, buf, len);
}

/***********************************************************************
* erase screen and home
***********************************************************************/
static void erasescreen(TERMINAL_T *t) {
	char buf[20];
	int len;
	t->scridx = t->scridy = 0;
	//memset(t->scrbuf, ' ', ROWS*COLS);
	len = sprintf(buf, _HOME_ _CLS_);
	len = telnet_session_write(&t->session, buf, len);
}

/***********************************************************************
* store char and move cursor with wrap
***********************************************************************/
static void store(TERMINAL_T *t, char ch) {
	char buf[20];
	int len;
	if (ch == CR) {
		len = sprintf(buf, _SO_ _PARA_ _SI_);
	} else if (ch == ETX) {
		len = sprintf(buf, _SO_ _STAR_ _SI_);
	} else if (ch == RS) {
		len = sprintf(buf, _SO_ _LE_);
	} else if (ch == US) {
		len = sprintf(buf, _GE_ _SI_);
	} else {
		len = sprintf(buf, "%c", ch);
	}
	len = telnet_session_write(&t->session, buf, len);
	//t->scrbuf[t->scridy][t->scridx] = ch;
	t->scridx++;
	cursorwrap(t);
}

/***********************************************************************
* B9352 Emulation Write (data in sysbuf)
* Returns number of bytes used or -1 on non-recoverable error
***********************************************************************/
void b9352_emulation_write(TERMINAL_T *t) {
	int ptr;
	char ch;
	char obuf[OUTBUFSIZE];
	char *op = obuf;

	// convert sysbuf to outbuf, release sysbuf
	BIT disc = convert6to8(t);

	// if no emulation, just send via TELNET
	if (t->em == em_none) {
		// try to write
		ptr = telnet_session_write(&t->session, t->outbuf, t->outidx);

		// reason to disconnect?
		if (disc || ptr != t->outidx) {
			if (disc && dtrace) {
				sprintf(obuf, "+DISC REQ (%d)\n", t->session.socket);
				spo_print(obuf);
			}
			telnet_session_close(&t->session);
		}

		return;
	}

	// reset inbuf index
	t->inidx = 0;

	// here we run the terminal end of the line discipline
	if (etrace)
		printf("interpreting outbuf(%d)\n", t->outidx);
	ptr = 0;
	while (ptr < t->outidx) {
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
						printf("[EOT]\n");

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
					printf("[EOT]\n");

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
					printf("<ETX>[ACK]\n");

				// prepare response
				t->inbuf[t->inidx++] = ACK;

				// next state is waiting for more data or EOT
				t->lds = lds_recvenq;

				// in B9352 emulation, we need to store the ETX
				if (t->em == em_ansi)
					store(t, ETX);
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
				// text to display or allowed control chars
				if (t->em == em_teletype) {
					// teletype emulation
					switch (ch) {
					case CR:
						*op++ = CR;
						t->lfpending = true;
						break;
					case LF:
						*op++ = CR;
						*op++ = LF;
						t->lfpending = false;
						break;
					case DC3:
						t->lfpending = false;
						break;
					case DC4:
						t->paused = true;
						*op++ = BEL;
						break;
					default:
						if (ch >= ' ') {
							if (t->lfpending) {
								*op++ = LF;
								t->lfpending = false;
							}
							*op++ = ch;
						}
					}
				} else {
					// B9352 emulation to external ANSI terminal
					switch (ch) {
					case DC1: // DC1 = clear from current to end of line
						erasetoeol(t);
						break;
					case BS: // BS = one position left
						t->scridx--;
						cursorwrap(t);
						cursormove(t);
						break;
					case DC4: // DC4 = home position, no clear
						t->scridx = t->scridy = 0;
						cursormove(t);
						break;
					case DC3: // DC3 = one position up
						t->scridy--;
						cursorwrap(t);
						cursormove(t);
						break;
					case LF: // LF = one position down
						t->scridy++;
						cursorwrap(t);
						cursormove(t);
						break;
					case CR: // CR = move cursor to start of next line
						store(t, CR);
						t->scridx = 0;
						t->scridy++;
						cursorwrap(t);
						cursormove(t);
						break;
					case FF: // FF = clear screen and cursor home
						t->scridx = t->scridy = 0;
						cursormove(t);
						erasescreen(t);
						break;
					case NUL: // NUL = time fill, no effect on screen
						break;
					default:
						if (isprint(ch) || ch == ETX || ch == RS || ch == US) {
							store(t, ch);
						} else {
							printf("<%02x>", ch);
						}
					}
				}
			} else {
				// chars outside STX/ETX
				printf("{%02x}", ch);
			}
		}
	} // while

finish:
	// try to write to terminal
	ptr = telnet_session_write(&t->session, obuf, op-obuf);

	// reason to disconnect?
	if (disc || ptr != (op-obuf)) {
		if (disc && dtrace) {
			sprintf(obuf, "+DISC REQ (%d)\n", t->session.socket);
			spo_print(obuf);
		}
		telnet_session_close(&t->session);
	}

// conclude debug and leave
	if (etrace)
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
		if (etrace)
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
	if (true || t->em == em_teletype) {
		// teletype emulation, simple line editing in keybuf
		odx = 0;
		// loop over all what we received
		for (idx=0; idx<cnt; idx++) {
			ch = ibuf[idx];
			switch (ch) {
			case CR:
				// end of line
				// mark send ready
				t->lds = lds_sendrdy;
				// echo CR/LF when not paused
				if (t->paused) {
					t->paused = false;
				} else {
					obuf[odx++] = CR;
					//obuf[odx++] = LF;
				}
				// line complete - leave this loop
				goto finish;
			case BS:
			case RUBOUT:
				// backspace
				if (t->keyidx > 0) {
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
	} else {
		// B9352 terminal emulation with external ANSI terminal
	}

	// default exit
	return 0;
}


