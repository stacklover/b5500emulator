/***********************************************************************
* b5500emulator
************************************************************************
* Copyright (c) 2018, Reinhard Meyer, DL5UY
* Licensed under the MIT License,
*       see LICENSE
************************************************************************
* b5500 data communication emulation (DCC)
*
* This emulates a B9352 Terminal on an ANSI Terminal
*
* t->em == em_teletype: output is line oriented
* t->em == em_ansi: behaviour is basically true to the original B9352
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
* ANSI terminal control codes
***********************************************************************/
#define _SO_	"\033[22m"	// bold display off
#define _SI_	"\033[1m"	// bold display on
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
#define	_CRSYM_		"~" //"\302\266"
#define	_ETXSYM_	"|" //"\302\244"
#define	_RSSYM_		"^" //
#define	_USSYM_		"^" //
#define	_LE_		"{" //"\302\253"
#define	_GE_		"}" //"\302\273"
#define	_NOT_		"!" //"\302\254"
#define	_MULT_		"x" //"\303\230"
#define	FILLCHAR	' '

/***********************************************************************
* Redisplay memory on ANSI terminal
***********************************************************************/
static void redisplay(TERMINAL_T *t, int row1, int row2) {
	int row, col;
	char ch;
	char buf[COLS+20];
	char *p = buf;

	for (row = row1; row <= row2; row++) {
		// move cursor to begin of line
		p = buf + sprintf(buf, _GOTO_, row+1, 1);
		for (col = 0; col < COLS; col++) {
			ch = t->scrbuf[row*COLS + col];
			if (row == ROWS-1 && col == COLS-1) {
				// cannot use last char of last line
				break;
			}
			if (ch == CR) {
				// end of line
				// just clear the rest of the line (or the whole line)
				//p += sprintf(p, _CRSYM_ _EREOL_);
				//break;
				p += sprintf(p, _CRSYM_);
			} else if (ch == ETX) {
				p += sprintf(p, _ETXSYM_);
			} else if (ch == RS) {
				p += sprintf(p, _RSSYM_);
			} else if (ch == US) {
				p += sprintf(p, _USSYM_);
			} else {
				*p++ = ch;
			}
			if (p-buf > COLS) {
				telnet_session_write(&t->session, buf, p-buf);
				p = buf;
			}
		}
	}
	// move cursor to its last position
	p += sprintf(p, _GOTO_, t->scridy+1, t->scridx+1);
	telnet_session_write(&t->session, buf, p-buf);
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
		if (etrace)
			printf("scroll up!\n");
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
	len = telnet_session_write(&t->session, buf, len);
}

/***********************************************************************
* erase to end of line
***********************************************************************/
static void erasetoeol(TERMINAL_T *t) {
	char buf[20];
	int len;
	memset(t->scrbuf + t->scridy*COLS + t->scridx, FILLCHAR, COLS - t->scridx);
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
	memset(t->scrbuf, FILLCHAR, ROWS*COLS);
	len = sprintf(buf, _HOME_ _CLS_);
	len = telnet_session_write(&t->session, buf, len);
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
	int len;
	if (ch == CR) {
		len = sprintf(buf, _SO_ _CRSYM_ _SI_);
	} else if (ch == ETX) {
		len = sprintf(buf, _SO_ _ETXSYM_ _SI_);
	} else if (ch == RS) {
		len = sprintf(buf, _SO_ _LE_);
	} else if (ch == US) {
		len = sprintf(buf, _GE_ _SI_);
	} else {
		len = sprintf(buf, "%c", ch);
	}
	len = telnet_session_write(&t->session, buf, len);
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
		if (t->em == em_teletype) {
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
		if (t->em == em_teletype) {
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

	// try to write to terminal
	cnt = telnet_session_write(&t->session, obuf, op-obuf);

	// reason to disconnect?
	return cnt != (op-obuf);
}

/***********************************************************************
* ANSI interpret ESC sequence
***********************************************************************/
static void interpret_esc(TERMINAL_T *t) {
	t->keybuf[t->keyidx] = 0; // close the string
	//printf("ESC%s -> ", t->keybuf);
	if (t->keybuf[0] == 'O') {
		if (       t->keybuf[1] == 'P') {
			//printf("F1");
			redisplay(t, 0, ROWS-1);
		} else if (t->keybuf[1] == 'Q') {
			//printf("F2");
		} else if (t->keybuf[1] == 'R') {
			//printf("F3");
		} else if (t->keybuf[1] == 'S') {
			//printf("F4");
		} else {
			//printf("?1?");
		}
	} else if (t->keybuf[0] == '[') {
		if (       t->keybuf[1] == 'A') {
			//printf("^");
			t->scridy--;
			cursorwrap(t);
			cursormove(t);
		} else if (t->keybuf[1] == 'B') {
			//printf("v");
			t->scridy++;
			cursorwrap(t);
			cursormove(t);
		} else if (t->keybuf[1] == 'C') {
			//printf(">");
			t->scridx++;
			cursorwrap(t);
			cursormove(t);
		} else if (t->keybuf[1] == 'D') {
			//printf("<");
			t->scridx--;
			cursorwrap(t);
			cursormove(t);
		} else if (t->keybuf[1] == 'P') {
			//printf("PAUSE");
		} else if (isdigit(t->keybuf[1])) {
			// scan the number
			char *p;
			unsigned number = strtoul(t->keybuf+1, &p, 10);
			if (*p == '~') {
				//printf("KEY%u", number);
				if (number == 2)
					char_insert(t);
			} else {
				//printf("?3?");
			}
		} else {
			//printf("?2?");
		}
	} else {
		//printf("?4?");
	}
	//printf("\n");
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
	} else if (isprint(ch)) {
		// if printable
		ch = translatetable_ascii2bic[ch&0x7f];
		ch = translatetable_bic2ascii[ch&0x7f];
		//printf("printable: %c\n", ch);
		store(t, ch);
	} else if (ch == BS) {
		//printf("<X]\n");
		if (t->scridx > 0) {
			t->scridx--;
			char_delete(t);
		}
	} else if (ch == TAB) {
		//printf("-->\n");
	} else if (ch == RUBOUT) {
		//printf("~\n");
		char_delete(t);
	} else {
		//printf("scrap:%02x\n", ch);
	}

	return false;
}


