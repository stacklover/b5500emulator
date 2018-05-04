/***********************************************************************
* b5500emulator
************************************************************************
* Copyright (c) 2018, Reinhard Meyer, DL5UY
* Licensed under the MIT License,
*       see LICENSE
************************************************************************
* b5500 data communication emulation (DCC)
*
* LINE DISCIPLINE TELETYPE (not really a line discipline)
*
************************************************************************
* 2018-04-01  R.Meyer
*   Frame from dev_dcc.c
* 2018-04-21  R.Meyer
*   factored out all physcial connection (PC), all line discipline(LD)
*   and all emulation (EM) functionality to spearate files
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
		// some characters have special control functions on output
		switch (ch) {
		case '>': // BIC: greater than
			t->outbuf[t->outidx++] = NUL; // DC1
			break;
		case '}': // BIC: greater or equal
			disc = true;
			break;
		case '<': // BIC: less than
			t->outbuf[t->outidx++] = NUL; // RUBOUT
			break;
		case '{': // BIC: less or equal
			t->outbuf[t->outidx++] = CR;
			break;
		case '!': // BIC: not equal
			t->outbuf[t->outidx++] = LF;
			break;
		default: // all others just copy
			t->outbuf[t->outidx++] = ch;
		} // switch
	} // for

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

	ptr = 0;
	while (ptr < t->inidx) {
		ch = t->inbuf[ptr++];
		if (ch >= 0x20) {
			// printable char
			if (t->sysidx < SYSBUFSIZE) {
				t->sysbuf[t->sysidx++] = ch;
			}
		}
	} // while

	// buffer is ready to be ready by the system
	t->abnormal = false;
	t->bufstate = readready;
	t->interrupt = true;
}

/***********************************************************************
* line discipline teletype (data in sysbuf)
* Returns number of bytes used or -1 on non-recoverable error
***********************************************************************/
void ld_write_teletype(TERMINAL_T *t) {
	int ptr = -1;

	// convert sysbuf to outbuf, release sysbuf
	BIT disc = convert6to8(t);

	// send data to physical connections
	switch (t->pc) {
	case pc_telnet: ptr = pc_telnet_write(t, t->outbuf, t->outidx); break;
	case pc_serial: ptr = pc_serial_write(t, t->outbuf, t->outidx); break;
	case pc_canopen: ptr = pc_canopen_write(t, t->outbuf, t->outidx); break;
	default: t->pcs = pcs_failed;
	}

	// check for write errors
	if (ptr != t->outidx)
		t->pcs = pcs_failed;

	// disconnect requested?
	if (disc) {
		if (dtrace) {
			sprintf(t->outbuf, "+DREQ %s\r\n", t->name);
			spo_print(t->outbuf);
		}
		t->pcs = pcs_failed;
	}
}

/***********************************************************************
* line discipline teletype
* check for status changes or for data from client
***********************************************************************/
int ld_poll_teletype(TERMINAL_T *t) {
	char ibuf[10], obuf[20], ch;
	int cnt = -1, idx, odx;

	// check for data available
	switch (t->pc) {
	case pc_telnet: cnt = pc_telnet_read(t, ibuf, sizeof ibuf); break;
	case pc_serial: cnt = pc_serial_read(t, ibuf, sizeof ibuf); break;
	case pc_canopen: cnt = pc_canopen_read(t, ibuf, sizeof ibuf); break;
	default: return -1;
	}

	if (cnt <= 0) // non recoverable error or no data
		return cnt;

	odx = 0;
	// loop over all what we received
	for (idx = 0; idx < cnt && t->lds != lds_sendrdy; idx++) {
		ch = ibuf[idx];

		// make any sequence of CR,LF codes behave like a single CR
		if (ch == CR || ch == LF) {
			// prevent further line ending chars of that sequence from causing action
			if (t->escaped)
				continue;
			ch = CR;
			t->escaped = true;
		} else {
			t->escaped = false;
		}

		switch (ch) {
		case CR:
			// end of line - mark keybuf as ready to send
			t->lds = lds_sendrdy;
			break;
		case BS:
			// backspace
			if (t->inidx > 0) {
				t->inidx--;
				// echo BS+SPACE+BS
				obuf[odx++] = BS;
				obuf[odx++] = ' ';
				obuf[odx++] = BS;
			}
			break;
		case RUBOUT:
			// rubout
			if (t->inidx > 0) {
				t->inidx--;
				// echo '\'+CHAR+'\'
				obuf[odx++] = '\\';
				obuf[odx++] = t->inbuf[t->inidx];
				obuf[odx++] = '\\';
			}
			break;
		case ESC:
			// cancel line
			t->inidx = 0;
			// echo TILDE+CR+CL
			obuf[odx++] = '~';
			obuf[odx++] = CR;
			obuf[odx++] = LF;
			break;
		default:
			if (ch >= ' ' && ch <= 0x7e && t->keyidx < KEYBUFSIZE) {
				// if printable, add to keybuf
				ch = translatetable_ascii2bic[ch&0x7f];
				ch = translatetable_bic2ascii[ch&0x7f];
				t->inbuf[t->inidx++] = ch;
				// echo
				obuf[odx++] = ch;
			} else {
				obuf[odx++] = BEL;
			}
		}
	}

	// anything to send?
	if (odx > 0) {
		switch (t->pc) {
		case pc_telnet: pc_telnet_write(t, obuf, odx); break;
		case pc_serial: pc_serial_write(t, obuf, odx); break;
		case pc_canopen: pc_canopen_write(t, obuf, odx); break;
		default: return -1;
		}
	}

	// inbuf ready for sending and sysbuf idle?
	if (t->lds == lds_sendrdy && t->bufstate == idle) {
		convert8to6(t);
		t->inidx = 0;
		t->lds = lds_idle;
	}

	// return number of chars processed
	return cnt;
}


