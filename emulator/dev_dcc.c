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
* operational notes:
*   there is no word or character count given in the IOCW
*   by default, all 112 chars (14 words) are valid
*   a shorter message is ended with the EOM character, which is never
*   part of a message
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
#include "dcc.h"

/***********************************************************************
* the terminals
***********************************************************************/
static TERMINAL_T terminal[NUMTERM];

/***********************************************************************
* the TELNET servers
***********************************************************************/
static TELNET_SERVER_T server[NUMSERV];
// server[0]: Port   23 - LINE type terminals (B9353 as TELETYPE)
// server[1]: Port 8023 - BLOCK type terminals (B9352 with external ANSI)
// server[2]: Port 9023 - BLOCK type terminals (B9352 with protocol)
static unsigned portno[NUMSERV] = {23, 8023, 9023};
static enum ld ldno[NUMSERV] = {ld_contention, ld_contention, ld_contention};
static enum em emno[NUMSERV] = {em_teletype, em_ansi, em_none};

/***********************************************************************
* misc variables
***********************************************************************/
static BIT ready;
static BIT telnet = false;

BIT etrace = false;
BIT dtrace = false;
BIT ctrace = true;

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
	char buf[OUTBUFSIZE];
	TERMINAL_T *t;
	unsigned index;

	// list all connected terminals
	for (index = 0; index < NUMTERM; index++) {
		t = &terminal[index];
		if (t->connected) {
			sprintf(buf,"%u/%u S=%d DE=%d%d B=%d I=%u A=%u F=%u\r\n",
				TUN(index), BNR(index),
				t->session.socket, (int)t->ld, (int)t->em,
				(int)t->bufstate,
				t->interrupt, t->abnormal, t->fullbuffer);
			spo_print(buf);
		}
	}
	return 0; // OK
}

#ifdef USECAN
/***********************************************************************
* specify canid on/off
***********************************************************************/
static int set_can(const char *v, void *) {
	if (isdigit(v[0])) {
		terminal[0].canid = atoi(v);
		if (terminal[0].canid < 1 || terminal[0].canid > 126) {
			terminal[0].canid = 0;
			goto help;
		}
		// wait for SPO to become ready
		while (!can_ready(terminal[0].canid)) {
			spo_print("$WAITING FOR TERMINAL READY\r\n");
			sleep(1);
		}
	} else if (strcasecmp(v, "OFF") == 0) {
		terminal[0].canid = 0;
	} else {
help:		spo_print("$SPECIFY CANID(1..126) OR OFF\r\n");
		return 2; // FATAL
	}
	return 0; // OK
}
#endif

/***********************************************************************
* command table
***********************************************************************/
static const command_t dcc_commands[] = {
	{"DCC", NULL},
	{"TELNET", set_telnet},
#ifdef USECAN
	{"CAN", set_can},
#endif
	{"CTRACE", set_ctrace},
	{"DTRACE", set_dtrace},
	{"ETRACE", set_etrace},
	{"STATUS", get_status},
	{NULL, NULL},
};

/***********************************************************************
* Find a terminal that needs service
***********************************************************************/
static BIT terminal_search(unsigned *ptun, unsigned *pbnr) {
	unsigned index;
	TERMINAL_T *t;

	for (index = 0; index < NUMTERM; index++) {
		t = &terminal[index];

		// trigger late output IRQ
		if (t->bufstate == outputbusy) {
			// now send/interpret the data
			if (t->ld == ld_teletype)
				ld_write_teletype(t);
			else
				ld_write_contention(t);
		}

		// is this requiring service now?
		if (t->interrupt)  {
			// something found
			if (ptun) *ptun = TUN(index);
			if (pbnr) *pbnr = BNR(index);
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
		int index;

		// ignore all SIGPIPEs
		signal(SIGPIPE, SIG_IGN);

		// init server data structures
		for (index=0; index<NUMSERV; index++) {
			telnet_server_clear(server+index);
		}

		// init terminal data structures
		for (index=0; index<NUMTERM; index++) {
			TERMINAL_T *t = terminal+index;
			memset(t, 0, sizeof(TERMINAL_T));
			telnet_session_clear(&t->session);
			if (index == 0) {
				// index = 0 is typetype via CANopen
				t->pc = pc_canopen;
				t->canid = 0;
			} else {
				// all the rest are via TELNET server
				t->pc = pc_telnet;
			}
		}
	}
	ready = true;
	return command_parser(dcc_commands, option);
}

/***********************************************************************
* handle new incoming TELNET session
***********************************************************************/
static void new_connection(int newsocket, struct sockaddr_in *addr, enum ld ld, enum em em) {
	static const char *msg = "\r\nB5500 TIME SHARING - BUSY\r\nPLEASE CALL BACK LATER\r\n";
	socklen_t addrlen = sizeof(*addr);
	char host[40];
	unsigned port;
	int index, beg, end;
	TERMINAL_T *t;
	char buf[OUTBUFSIZE];

	// get the peer info
	getnameinfo((struct sockaddr*)addr, addrlen, host, sizeof(host), NULL, 0, 0);
	port = ntohs(addr->sin_port);

	// determine terminal range
	if (em == em_teletype) {
		beg = 1;	// 0 reserved for teletype via CANopen (hardwired for now)
		end = 15;
	} else {
		beg = 16;
		end = NUMTERM-1;
	}

	// find a free terminal to handle this
	for (index = beg; index <= end && index < NUMTERM; index++) {
		t = &terminal[index];
		if (t->pc == pc_telnet && !t->connected) {
			// free terminal found
			if (ctrace) {
				sprintf(buf, "+FROM %u/%u (%d) %s:%u\r\n",
					TUN(index), BNR(index), newsocket, host, port);
				spo_print(buf);
			}
			telnet_session_open(&(t->session), newsocket);
			memset(t->scrbuf, ' ', sizeof t->scrbuf);
			t->sysidx = 0;
			t->keyidx = 0;
			t->scridx = 0;
			t->scridy = 0;
			t->lfpending = false;
			t->paused = false;
			t->eotcount = 0;
			t->abnormal = true;
			t->bufstate = writeready;
			t->ld = ld;
			t->em = em;
			t->lds = lds_idle;
			t->inmode = false;
			t->outmode = false;
			t->outlastwasmode = false;
			t->eotcount = 0;
			t->connected = true;
			t->interrupt = true;
			return;
		}
	}
	// nothing found
	if (ctrace) {
		sprintf(buf, "+BUSY %u/%u (%d)\r\n",
			TUN(index), BNR(index), newsocket);
		spo_print(buf);
	}
	write(newsocket, msg, strlen(msg));
	close(newsocket);
}

/***********************************************************************
* handle TELNET server
***********************************************************************/
static void dcc_test_incoming(void) {
	int newsocket = -1, cnt;
	struct sockaddr_in addr;
	unsigned index;
	char buf[OUTBUFSIZE];
	TERMINAL_T *t;

	// start/stop servers
	for (index=0; index<NUMSERV; index++) {
		if (telnet && server[index].socket <= 2) {
			// server needs to be started
			telnet_server_start(server+index, portno[index]);
		} else if (!telnet && server[index].socket > 2) {
			// server needs to be stopped
			telnet_server_stop(server+index);
		}
	}

	// poll servers for new connections
	for (index=0; index<NUMSERV; index++) {
		if (server[index].socket > 2) {
			newsocket = telnet_server_poll(server+index, &addr);
			if (newsocket > 0)
				new_connection(newsocket, &addr, ldno[index], emno[index]);
		}
	}
	
	// check for input from any connection
	for (index = 0; index < NUMTERM; index++) {
		t = &terminal[index];
		// check for CANopen ready/unready
		if (t->pc == pc_canopen) {
			BIT ready = (t->canid > 0) && can_ready(t->canid);
			if (ready && !t->connected && can_receive_string(t->canid, buf, sizeof buf) != NULL) {
				// CAN became ready
				memset(t->scrbuf, ' ', sizeof t->scrbuf);
				t->sysidx = 0;
				t->keyidx = 0;
				t->scridx = 0;
				t->scridy = 0;
				t->lfpending = false;
				t->paused = false;
				t->eotcount = 0;
				t->abnormal = true;
				t->bufstate = writeready;
				t->ld = ld_teletype;
				t->em = em_none;
				t->lds = lds_idle;
				t->inmode = false;
				t->outmode = false;
				t->outlastwasmode = false;
				t->eotcount = 0;
				t->connected = true;
				t->interrupt = true;
			} else if (!ready && t->connected) {
				// CAN became unready
				t->interrupt = true;
				t->abnormal = true;
				t->fullbuffer = false;
				t->bufstate = notready;
				t->connected = false;
				t->sysbuf[0] = 0;
				t->sysidx = 0;
			}
		}
		if (t->connected) {
			if ((t->pc == pc_telnet && t->session.socket > 2) || (t->pc == pc_canopen && t->canid > 0)) {
				// socket is open OR canid given
				// depending on line discipline the action is different
				if (t->ld == ld_teletype)
					cnt = ld_poll_teletype(t);
				else
					cnt = ld_poll_contention(t);
				if (cnt < 0)
					goto isclosed;
			} else {
isclosed:			// socket is closed OR canid not given
				if (ctrace) {
					sprintf(buf, "+CLSD %u/%u\r\n",
						TUN(index), BNR(index));
					spo_print(buf);
				}
				t->interrupt = true;
				t->abnormal = true;
				t->fullbuffer = false;
				t->bufstate = notready;
				t->connected = false;
				t->sysbuf[0] = 0;
				t->sysidx = 0;
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

	// check for a signal ready sysbuf
	if (terminal_search(NULL, NULL)) {
		// a terminal requesting service has been found - set Datacomm IRQ
		CC->CCI13F = true;
	}

	// finally return always ready
	return ready;
}

/***********************************************************************
* read (from inbuf to system)
***********************************************************************/
static void dcc_read(IOCU *u) {
	// terminal unit and buffer number are in "word count" field of IOCW
	unsigned tun = (u->d_wc >> 5) & 0xf;
	unsigned bnr = (u->d_wc >> 0) & 0xf;
	unsigned index = IDX(tun, bnr);

	// resolve critical error cases first
	if (index >= NUMTERM || !terminal[index].connected) {
		// terminal number outside range or not connected
		u->d_result = RD_21_END | RD_20_ERR | RD_18_NRDY;
		return;
	}

	TERMINAL_T *t = terminal+index;
	char c;
	int count;
	int ptr;
	BIT gmset;

	// return for all sysbuf states that do not allow reading
	switch (t->bufstate) {
	case idle:
	case writeready:
		// awaiting write data
		u->d_result = RD_21_END;
		break;
	case readready:
		// sysbuf is filled with read data, continue after switch
		break;
	case inputbusy:
	case outputbusy:
		// sysbuf is currently in use - return with flags
		u->d_result = RD_21_END | RD_20_ERR;
		return;
	case notready:
		// sysbuf is not in any useful state - return with flags
		u->d_result = RD_21_END | RD_20_ERR | RD_18_NRDY;
		return;
	}

	// now do the read

	if (dtrace)
		printf("+READ %u/%u |", tun, bnr);

	ptr = 0;
	gmset = false;

loop:
	u->w = 0LL;
	// do whole words (8 characters) while we have more data
	for (count=0; count<8; count++) {
		if (ptr >= t->sysidx) {
			gmset = true;
			c = EOM;
		} else {
			c = t->sysbuf[ptr++];
			if (dtrace)
				printf("%c", c);
		}
		// note that ptr stays on the current end of sysbuf
		// causing the rest of the word to be filled with
		// EOM characters
		if (t->ld == ld_teletype && c == '?')
			t->abnormal = true;
		u->ib = translatetable_ascii2bic[c & 0x7f];
		put_ib(u);
	}
	// store the complete word
	main_write_inc(u);

	// continue until done
	if (!gmset && ptr < SYSBUFSIZE)
		goto loop;

	if (dtrace)
		printf("|\n");

	// abnormal flag?
	if (t->abnormal)
		u->d_result |= RD_25_ABNORMAL;

	// sysbuf sent to system, is idle now
	t->bufstate = idle;
	t->abnormal = false;
	t->interrupt = false;
}

/***********************************************************************
* write (from system to sysbuf)
***********************************************************************/
static void dcc_write(IOCU *u) {
	// terminal unit and buffer number are in "word count" field of IOCW
	unsigned tun = (u->d_wc >> 5) & 0xf;
	unsigned bnr = (u->d_wc >> 0) & 0xf;
	unsigned index = IDX(tun, bnr);

	// resolve critical error cases first
	if (index >= NUMTERM || !terminal[index].connected) {
		// terminal number outside range or not connected
		u->d_result = RD_21_END | RD_20_ERR | RD_18_NRDY;
		return;
	}

	TERMINAL_T *t = terminal+index;
	char c;

	// return for all sysbuf states that do not allow writing
	switch (t->bufstate) {
	case idle:
	case writeready:
		// we can write, continue after switch
		break;
	case readready:
		// sysbuf is filled with read data - return with flags
		u->d_result = RD_21_END;
		return;
	case inputbusy:
	case outputbusy:
		// sysbuf is currently in use - return with flags
		u->d_result = RD_21_END | RD_20_ERR;
		return;
	case notready:
		// sysbuf is not in any useful state - return with flags
		u->d_result = RD_21_END | RD_20_ERR | RD_18_NRDY;
		return;
	}

	// now do the write
	if (dtrace)
		printf("+WRIT %u/%u |", tun, bnr);

	t->sysidx = 0;	// start of sysbuf

	// we keep copying data to sysbuf until:
	// we reach SYSBUFSIZE chars -> fullbuffer = true
	// we find the EOM char  -> fullbuffer = false

loop:
	// get next word
	main_read_inc(u);
	// handle each char
	for (int count=0; count<8 && t->sysidx<SYSBUFSIZE; count++) {
		// get next char
		get_ob(u);
		// translate it to ASCII
		c = translatetable_bic2ascii[u->ob];
		// premature end ?
		if (c == EOM) {
			// end of message -> partially filled sysbuf
			t->fullbuffer = false;
			goto done;
		}
		// store char in sysbuf
		t->sysbuf[t->sysidx++] = c;
		if (dtrace)
			printf("%c", c);
	}
	// if not reached SYSBUFSIZE chars, we go on writing
	if (t->sysidx < SYSBUFSIZE)
		goto loop;
	// coming here means we got a full sysbuf
	t->fullbuffer = true;

done:	if (dtrace)
		printf("|\n");

	// set flags
	t->abnormal = false;
	if (t->abnormal)
		u->d_result = RD_25_ABNORMAL;
	if (t->fullbuffer)
		u->d_result |= RD_23_ETYPE;

	// data is now going to be sent...
	t->bufstate = outputbusy;

	// the sending is concluded in "terminal_search"
}

/***********************************************************************
* interrogate
***********************************************************************/
static void dcc_interrogate(IOCU *u) {
	// terminal unit and buffer number are in "word count" field of IOCW
	unsigned tun = (u->d_wc >> 5) & 0xf;
	unsigned bnr = (u->d_wc >> 0) & 0xf;

	// tun == 0: general query - find a terminal that needs service
	if (tun == 0) {
		// search if any terminal needs service
		terminal_search(&tun, &bnr);
	}

	// any found or specific query?
	if (tun > 0) {
		TERMINAL_T *t;
		unsigned index = IDX(tun, bnr);

		if (index < NUMTERM) {
			t = &terminal[index];
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
			case notready:
				u->d_result = RD_20_ERR | RD_18_NRDY;
			}
			// abnornal flag?
			if (t->abnormal)
				u->d_result |= RD_25_ABNORMAL;
			// clear pending IRQ
			t->interrupt = false;
		}
	}

	// return actual terminal and buffer numbers in word count
	u->d_wc = (tun << 5) | bnr;
}

/***********************************************************************
* access the DCC
***********************************************************************/
void dcc_access(IOCU *u) {
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
		dcc_interrogate(u);
	} else if (reading) {
		// read sysbuf
		dcc_read(u);
	} else {
		// write sysbuf
		dcc_write(u);
	}

#if TRACE_DCC
	print_ior(stdout, u);
	printf("\n");
#endif
}


