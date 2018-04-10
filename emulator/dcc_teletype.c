/***********************************************************************
* b5500emulator
************************************************************************
* Copyright (c) 2018, Reinhard Meyer, DL5UY
* Licensed under the MIT License,
*       see LICENSE
************************************************************************
* b5500 data communication emulation (DCC)
*
* This emulates a Teletype
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
#include "dcc.h"

/***********************************************************************
* line discipline teletype (data in sysbuf)
* Returns number of bytes used or -1 on non-recoverable error
***********************************************************************/
void ld_write_teletype(TERMINAL_T *t) {
	int iptr;
	char ch;
	char buf[SYSBUFSIZE];
	char *p = buf;
	BIT disc = false;

	for (iptr=0; iptr<t->sysidx; iptr++) {
		ch = t->sysbuf[iptr];
		// some characters have special control functions on output
		switch (ch) {
		case '>': // BIC: greater than
			*p++ = DC1;
			break;
		case '}': // BIC: greater or equal
			disc = true;
			break;
		case '<': // BIC: less than
			*p++ = RUBOUT;
			break;
		case '{': // BIC: less or equal
			*p++ = CR;
			break;
		case '!': // BIC: not equal
			*p++ = LF;
			break;
		default: // all others copy
			*p++ = ch;
		} // switch
	} // for

	// make sysbuf available again
	t->bufstate = t->fullbuffer ? writeready : idle;
	t->interrupt = true;

	switch (t->pc) {
	case pc_telnet:
		// try to write
		iptr = telnet_session_write(&t->session, buf, p-buf);

		// reason to disconnect?
		if (disc || iptr != (p-buf)) {
			if (disc && dtrace) {
				sprintf(buf, "+DISC REQ (%d)\n", t->session.socket);
				spo_print(buf);
			}
			telnet_session_close(&t->session);
		}
		break;
	case pc_canopen:
		// write to CANopen
		*p = 0;
		iptr = can_send_string(t->canid, buf);

		// reason to disconnect?
		if (disc) {
			sprintf(buf, "+DISC REQ (%d)\n", t->session.socket);
			spo_print(buf);
			t->interrupt = true;
			t->abnormal = true;
			t->fullbuffer = false;
			t->bufstate = notready;
			t->connected = false;
			t->sysbuf[0] = 0;
			t->sysidx = 0;
		}
		break;
	default:
		;
	}
}

/***********************************************************************
* line discipline teletype
* check for status changes or for data from client
***********************************************************************/
int ld_poll_teletype(TERMINAL_T *t) {
	char obuf[KEYBUFSIZE*2], ch;
	int cnt, idx, odx;

	// teletype: only try to receive if sysbuf is idle or inputbusy
	if (t->bufstate != idle && t->bufstate != inputbusy)
		return 0;

	switch (t->pc) {
	case pc_telnet:
		// check for data available
		cnt = telnet_session_read(&t->session, t->keybuf, KEYBUFSIZE);
		if (cnt <= 0) // non recoverable error or no data
			return cnt;
		break;
	case pc_canopen:
		if (can_receive_string(t->canid, t->keybuf, KEYBUFSIZE) != NULL) {
			// got something
			cnt = strlen(t->keybuf);
			t->keybuf[cnt++] = CR;
		} else {
			// nothing received
			return 0;
		}
		break;
	default:
		return -1;
	}

	// mark sysbuf as busy and prepare it
	if (t->bufstate == idle) {
		t->bufstate = inputbusy;
		t->sysidx = 0;
	}

	// loop over all what we received
	odx = 0;
	for (idx=0; idx<cnt; idx++) {
		ch = t->keybuf[idx];
		switch (ch) {
		case CR:
			// end of line
			t->abnormal = false;
			t->bufstate = readready;
			t->interrupt = true;
			// echo
			obuf[odx++] = CR;
			obuf[odx++] = LF;
			// line complete - leave this function
			switch (t->pc) {
			case pc_telnet:
				if (telnet_session_write(&t->session, obuf, odx) < 0)
					return -1;
				break;
			default:
				;
			}
			return idx;
		case BS:
		case RUBOUT:
			// backspace
			if (t->sysidx > 0) {
				t->sysidx--;
				// echo
				obuf[odx++] = BS;
				obuf[odx++] = ' ';
				obuf[odx++] = BS;
			}
			break;
		case ESC:
			// cancel line
			t->sysidx = 0;
			// echo
			obuf[odx++] = '~';
			obuf[odx++] = CR;
			obuf[odx++] = LF;
			break;
		default:
			if (ch >= ' ' && ch <= 0x7e && t->sysidx < SYSBUFSIZE) {
				// if printable, add to sysbuf
				ch = translatetable_ascii2bic[ch&0x7f];
				ch = translatetable_bic2ascii[ch&0x7f];
				t->sysbuf[t->sysidx++] = ch;
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
		case pc_telnet:
			if (telnet_session_write(&t->session, obuf, odx) < 0)
				return -1;
			break;
		default:
			;
		}
	}

	// release sysbuf when empty again
	if (t->sysidx == 0) {
		t->abnormal = false;
		t->bufstate = idle;
		t->interrupt = true;
	}

	// return number of chars processed
	return cnt;
}


