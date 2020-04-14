/***********************************************************************
* b5500emulator
************************************************************************
* Copyright (c) 2018, Reinhard Meyer, DL5UY
* Licensed under the MIT License,
*       see LICENSE
************************************************************************
* b5500 data communication emulation (DCC)
*
* This emulates a B9352 Terminal on a TELETYPE or an ANSI Terminal
*
* t->em == em_teletype: output is line oriented
* t->em == em_ansi: behaviour is basically true to the original B9352
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

#define	USE_UTF8	1

/***********************************************************************
* ANSI terminal control codes
***********************************************************************/
#define _SI_	"\033[22m"	// bold display off
#define _SO_	"\033[1m"	// bold display on
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
#define	_CRSYM_		_SO_ "~" _SI_ _EREOL_
#define	_ETXSYM_	_SO_ "|" _SI_ _EREOL_
#define	_RSSYM_		_SO_ "<" _SI_
#define	_USSYM_		_SO_ ">" _SI_

#define	_CRSYM8_	"○" _EREOL_
#define	_ETXSYM8_	"◊" _EREOL_
#define	_RSSYM8_	"◄"
#define	_USSYM8_	"►"

#define	_LE8_		"≤"
#define	_GE8_		"≥"
#define	_NOT8_		"≠"
#define	_MULT8_		"×"
#define	_LEFTARROW8_	"←"

#define	FILLCHAR	' '

/***********************************************************************
* Redisplay memory on ANSI terminal
***********************************************************************/
#define BUFLEN 40	// make sure it can hold the escape sequences!
static void redisplay(TERMINAL_T *t, int row1, int row2) {
	char buf[BUFLEN];
	char *p = buf;

	for (int row = row1; row <= row2; row++) {
		// move cursor to begin of line
		p = buf + sprintf(buf, _GOTO_, row+1, 1);
		char *q = t->scrbuf + row * COLS;
		for (int col = 0; col < COLS; col++) {
			char ch = *q++;
			if (row == ROWS-1 && col == COLS-1) {
				// cannot use last char of last line
				continue;
			}

			switch (ch) {
			case CR:
				if (t->utf8mode) p += sprintf(p, _CRSYM8_); else p += sprintf(p, _CRSYM_);
				break;
			case ETX:
				if (t->utf8mode) p += sprintf(p, _ETXSYM8_); else p += sprintf(p, _ETXSYM_);
				break;
			case RS:
				if (t->utf8mode) p += sprintf(p, _RSSYM8_); else p += sprintf(p, _RSSYM_);
				break;
			case US:
				if (t->utf8mode) p += sprintf(p, _USSYM8_); else p += sprintf(p, _USSYM_);
				break;
			case '{':
				if (t->utf8mode) p += sprintf(p, _LE8_); else *p++ = ch;
				break;
			case '}':
				if (t->utf8mode) p += sprintf(p, _GE8_); else *p++ = ch;
				break;
			case '!':
				if (t->utf8mode) p += sprintf(p, _NOT8_); else *p++ = ch;
				break;
			case '|':
				if (t->utf8mode) p += sprintf(p, _MULT8_); else *p++ = ch;
				break;
			case '~':
				if (t->utf8mode) p += sprintf(p, _LEFTARROW8_); else *p++ = ch;
				break;
			default:
				*p++ = ch;
			}

			if (p > buf) {
				telnet_session_write(&t->tsession, buf, p-buf);
				p = buf;
			}
		}
	}
	// move cursor to its last position
	p += sprintf(p, _GOTO_, t->scridy+1, t->scridx+1);
	telnet_session_write(&t->tsession, buf, p-buf);
}

/***********************************************************************
* wrap cursor to be in bounds
***********************************************************************/
static void cursorwrap(TERMINAL_T *t) {
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
		t->scridy = ROWS-1;
		// copy screen up one line
		memmove(t->scrbuf, t->scrbuf + COLS, t->scridy*COLS);
		// clear last line
		memset(t->scrbuf + t->scridy*COLS, FILLCHAR, COLS);
		// re-paint full screen
		redisplay(t, 0, t->scridy);
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
	len = telnet_session_write(&t->tsession, buf, len);
}

/***********************************************************************
* erase to end of line
***********************************************************************/
static void erasetoeol(TERMINAL_T *t) {
	char buf[20];
	int len;
	memset(t->scrbuf + t->scridy*COLS + t->scridx, FILLCHAR, COLS - t->scridx);
	len = sprintf(buf, _EREOL_);
	len = telnet_session_write(&t->tsession, buf, len);
}

/***********************************************************************
* erase screen and home
***********************************************************************/
static void erasescreen(TERMINAL_T *t) {
	char buf[20];
	int len;
	t->scridx = t->scridy = 0;
	memset(t->scrbuf, FILLCHAR, ROWS*COLS);
	len = sprintf(buf, _HOME_ _CLS_);
	len = telnet_session_write(&t->tsession, buf, len);
}

/***********************************************************************
* insert blank at cursor position, move rest to right
***********************************************************************/
static void char_insert(TERMINAL_T *t) {
	memmove(t->scrbuf + t->scridy*COLS + t->scridx + 1,
		t->scrbuf + t->scridy*COLS + t->scridx,
		COLS-t->scridx-1);
	t->scrbuf[t->scridy*COLS + t->scridx] = FILLCHAR;
	redisplay(t, t->scridy, t->scridy);
}

/***********************************************************************
* delete character at cursor position, move rest to left
***********************************************************************/
static void char_delete(TERMINAL_T *t) {
	memmove(t->scrbuf + t->scridy*COLS + t->scridx,
		t->scrbuf + t->scridy*COLS + t->scridx + 1,
		COLS-t->scridx-1);
	t->scrbuf[t->scridy*COLS + COLS - 1] = FILLCHAR;
	redisplay(t, t->scridy, t->scridy);
}

/***********************************************************************
* store char and move cursor with wrap
***********************************************************************/
static void store(TERMINAL_T *t, char ch) {
	char buf[20];
	char *p = buf;

	switch (ch) {
	case CR:
		if (t->utf8mode) p += sprintf(p, _CRSYM8_); else p += sprintf(p, _CRSYM_);
		break;
	case ETX:
		if (t->utf8mode) p += sprintf(p, _ETXSYM8_); else p += sprintf(p, _ETXSYM_);
		break;
	case RS:
		if (t->utf8mode) p += sprintf(p, _RSSYM8_); else p += sprintf(p, _RSSYM_);
		break;
	case US:
		if (t->utf8mode) p += sprintf(p, _USSYM8_); else p += sprintf(p, _USSYM_);
		break;
	case '{':
		if (t->utf8mode) p += sprintf(p, _LE8_); else *p++ = ch;
		break;
	case '}':
		if (t->utf8mode) p += sprintf(p, _GE8_); else *p++ = ch;
		break;
	case '!':
		if (t->utf8mode) p += sprintf(p, _NOT8_); else *p++ = ch;
		break;
	case '|':
		if (t->utf8mode) p += sprintf(p, _MULT8_); else *p++ = ch;
		break;
	case '~':
		if (t->utf8mode) p += sprintf(p, _LEFTARROW8_); else *p++ = ch;
		break;
	default:
		*p++ = ch;
	}

	telnet_session_write(&t->tsession, buf, p - buf);
	t->scrbuf[t->scridy*COLS+t->scridx] = ch;
	t->scridx++;
	cursorwrap(t);
}

/***********************************************************************
* emulation b9352 output (data in ch)
***********************************************************************/
int b9352_output(TERMINAL_T *t, char ch) {
	char obuf[OUTBUFSIZE];
	char *op = obuf;
	int cnt = 0;

	if (t->em == em_teletype) {
		if (isprint(ch) || ch == CR || ch == LF)
			*op++ = ch;
	} else {
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
			if (true) {
				// teletype emulation
				t->paused = true;
				*op++ = BEL;
				t->scridx = 0;
				cursormove(t);
			} else {
				t->scridx = t->scridy = 0;
				cursormove(t);
			}
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
			if (true) {
				// teletype emulation
				t->scridx = 0;
				t->scridy++;
				cursorwrap(t);
				t->scridy++;
				cursorwrap(t);
				cursormove(t);
			} else {
				t->scridx = t->scridy = 0;
				cursormove(t);
				erasescreen(t);
			}
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

	// try to write to terminal
	switch (t->pc) {
	case pc_serial: cnt = write(t->serial_handle, obuf, op-obuf); break;
	case pc_canopen: cnt = can_write(t->canid, obuf, op-obuf); break;
	case pc_telnet: cnt = telnet_session_write(&t->tsession, obuf, op-obuf); break;
	default: cnt = 0;
	}

	// reason to disconnect?
	return cnt != (op-obuf);
}

/***********************************************************************
* ANSI interpret ESC sequence
***********************************************************************/
static void interpret_esc(TERMINAL_T *t) {
	char *p;
	unsigned number;

	t->keybuf[t->keyidx] = 0; // close the string
	switch (t->keybuf[0]) {
	case 'O': // DEC specific?
		switch (t->keybuf[1]) {
		case 'P': goto F1;
		case 'Q': goto F2;
		case 'R': goto F3;
		case 'S': goto F4;
		default:
			;
		}
		break;
	case '[': // ANSI
		switch (t->keybuf[1]) {
		case 'A': // CURSOR UP
			t->scridy--;
			cursorwrap(t);
			cursormove(t);
			return;
		case 'B': // CURSOR DOWN
			t->scridy++;
			cursorwrap(t);
			cursormove(t);
			return;
		case 'C': // CURSOR RIGHT
			t->scridx++;
			cursorwrap(t);
			cursormove(t);
			return;
		case 'D': // CURSOR LEFT
			t->scridx--;
			cursorwrap(t);
			cursormove(t);
			return;
		case 'P': // PAUSE
			return;
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9': // NUMBER
			// scan the number
			number = strtoul(t->keybuf+1, &p, 10);
			if (*p == '~') {
				switch (number) {
				case 2: // INSERT
					t->insertmode = !t->insertmode;
					return;
				case 3: // DELETE
					char_delete(t);
					return;
				case 11: F1: // F1
					redisplay(t, 0, ROWS-1);
					return;
				case 12: F2: // F2
					t->utf8mode = !t->utf8mode;
					redisplay(t, 0, ROWS-1);
					return;
				case 13: F3: // F3
					break;
				case 14: F4: // F4
					break;
				default:
					;
				}
			}
			break;
		default:
			;
		}
	default:
		;
	}

	printf("ESC%s unknown\n", t->keybuf);
}

/***********************************************************************
* emulation b9352 input (data in ch)
***********************************************************************/
int b9352_input(TERMINAL_T *t, char ch) {
	if (ch == CR) {
		int linestart = t->scridy * COLS;
		int lineend = linestart + COLS;
		int cursor = linestart + t->scridx;
		int startpos, endpos;

		// abort any escape sequence
		t->escaped = false;

		// transmit from last control to next control

		// search for next control char left of cursor (or begin of line)
		startpos = cursor;
		while (startpos > linestart && t->scrbuf[startpos-1] >= 0x20)
			startpos--; 

		// search for next control char right of startpos (or end of line)
		endpos = startpos;
		while (endpos < lineend && t->scrbuf[endpos] >= 0x20)
			endpos++; 

		// remove trailing blanks
		while (endpos > startpos && t->scrbuf[endpos-1] == 0x20)
			endpos--; 
		t->keyidx = endpos - startpos;
		memcpy(t->keybuf, t->scrbuf + startpos, t->keyidx);

		// mark send ready
		t->lds = lds_sendrdy;

		// move cursor to start of next line
		t->scridx = 0;
		t->scridy++;
		cursorwrap(t);
#if 1
		// skip protected fields
		linestart = t->scridy * COLS;
		lineend = linestart + COLS;
		cursor = linestart;
		// are we on protected field right now?
		if (t->scrbuf[cursor] == RS) {
			// search for US
			while (t->scrbuf[cursor] != US && cursor < lineend - 2)
				cursor++;
			cursor++;
			t->scridx = cursor - linestart;
		}
#endif
		cursormove(t);

		// leave this loop
		return true;
	} else if (ch == ESC) {
		// Escape sequence (re)start
		t->escaped = true;
		t->keyidx = 0;
	} else if (t->escaped) {
		// char to keybuf
		t->keybuf[t->keyidx++] = ch;
		if (ch < 0x20 || ch >= 0x7f) {
			// abort escape sequence, if its another ESC, restart
			if (ch == ESC)
				t->keyidx = 0;
			else
				t->escaped = false;
		} else if (t->keyidx == 1) {
			// after ESC must be in this range:
			if (ch >= 0x40 && ch <= 0x5f) {
				if (ch != '[' && ch != 'O')
					goto esc_complete;
			} else {
				// illegal, abort it
				t->escaped = false;
			}
		} else if (ch >= 0x40 && ch <= 0x7e) {
esc_complete:				// escape sequence is complete
			// interpret escape sequence here
			interpret_esc(t);
			t->keyidx = 0;
			t->escaped = false;
		}
		// all other chars are assumed part of the sequence
		// and collection will continue
	} else if (ch == BS || ch == RUBOUT) {
		//printf("<X]\n");
		if (t->scridx > 0) {
			t->scridx--;
			char_delete(t);
		}
	} else if (ch == TAB) {
		//printf("-->\n");
	} else {
		// convert to printable
		ch = translatetable_ascii2bic[ch&0x7f];
		ch = translatetable_bic2ascii[ch&0x7f];
		//printf("printable: %c\n", ch);
		if (t->insertmode)
			char_insert(t);
		store(t, ch);
	}

	return false;
}


