/***********************************************************************
* b5500emulator
************************************************************************
* Copyright (c) 2017, Reinhard Meyer, DL5UY
* Licensed under the MIT License,
*       see LICENSE
************************************************************************
* b5500 operator console emulation (SPO)
************************************************************************
* 2017-10-02  R.Meyer
*   Factored out from emulator.c
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
#include "common.h"
#include "io.h"

/***********************************************************************
* analysy of possible buffer overrun situations
************************************************************************
* char *fgets(char *s, int size, FILE *stream);
*
* fgets() reads in at most one less than size characters from stream and
* stores them into the buffer pointed to by s. Reading stops after an
* EOF or a newline. If a newline is read, it is stored into the buffer.
* A '\0' is stored after the last character in the buffer.
*
* risk asessment:
* 1. if called with a static buffer and sizeof of the buffer: none
* 2. it is also guaranteed, that a '\0' is at the end of data and within
*    the buffer limits
***********************************************************************/

#define NAMELEN 100
#define	BUFLEN 80
#define TIMESTAMP 1
#define	AUTOEXEC 1

/***********************************************************************
* the SPO
***********************************************************************/
static BIT	ready;
static char	spoinbuf[BUFLEN];
static char	spooutbuf[BUFLEN];
static time_t	stamp;
#if AUTOEXEC
static unsigned autoexec = false;
static const char *auto_cmd = "CRA FILE=CARDS/DCMCP-PATCH-COMPILE.CARD";
static const char *auto_trigger1 = "ESPOL/DISK= ";
static const char *auto_trigger2 = " EOJ";
#endif
#ifdef USECAN
static unsigned canspo = 0;	// 0 for OFF, CANid for ON
#endif
#ifdef TIMESTAMP
static unsigned timestamp = false;
#endif

/***********************************************************************
* output function
***********************************************************************/
void spo_print(const char *buf) {
	fputs(buf, stdout);

#ifdef USECAN
	// send message to "real SPO"
	if (canspo)
		can_send_string(canspo, buf);
#endif
}

/***********************************************************************
* specify load type
***********************************************************************/
static int set_spoload(const char *v, void *) {
	if (strcasecmp(v, "DISK") == 0) {
		CC->CLS = false;
	} else if (strcasecmp(v, "CARD") == 0) {
		CC->CLS = true;
	} else {
		spo_print("$UNKNOWN LOAD TYPE\r\n");
		return 2; // FATAL
	}
	return 0; // OK
}

/***********************************************************************
* specify autoexec on/off
***********************************************************************/
static int set_autoexec(const char *v, void *) {
	if (strcasecmp(v, "ON") == 0) {
		autoexec = true;
	} else if (strcasecmp(v, "OFF") == 0) {
		autoexec = false;
	} else {
		spo_print("$SPECIFY ON OR OFF\r\n");
		return 2; // FATAL
	}
	return 0; // OK
}

#ifdef USECAN
/***********************************************************************
* specify canspo on/off
***********************************************************************/
static int set_canspo(const char *v, void *) {
	if (isdigit(v[0])) {
		canspo = atoi(v);
		if (canspo < 1 || canspo > 126) {
			canspo = 0;
			goto help;
		}
		// wait for SPO to become ready
		while (!can_ready(canspo)) {
			spo_print("$WAITING FOR SPO READY\r\n");
			sleep(1);
		}
	} else if (strcasecmp(v, "OFF") == 0) {
		canspo = 0;
	} else {
help:		spo_print("$SPECIFY CANID(1..126) OR OFF\r\n");
		return 2; // FATAL
	}
	return 0; // OK
}
#endif

#ifdef TIMESTAMP
/***********************************************************************
* specify timestamp on/off
***********************************************************************/
static int set_timestamp(const char *v, void *) {
	if (strcasecmp(v, "ON") == 0) {
		timestamp = true;
	} else if (strcasecmp(v, "OFF") == 0) {
		timestamp = false;
	} else {
		spo_print("$SPECIFY ON OR OFF\r\n");
		return 2; // FATAL
	}
	return 0; // OK
}
#endif

/***********************************************************************
* command table
***********************************************************************/
static const command_t spo_commands[] = {
	{"SPO", NULL},
	{"LOAD", set_spoload},
#if AUTOEXEC
	{"AUTOEXEC", set_autoexec},
#endif
#ifdef USECAN
	{"CAN", set_canspo},
#endif
#ifdef TIMESTAMP
	{"TIMESTAMP", set_timestamp},
#endif
	{NULL, NULL},
};

/***********************************************************************
* Initialize command from argv scanner or special SPO input
***********************************************************************/
int spo_init(const char *option) {
	ready = true;
	return command_parser(spo_commands, option);
}

/***********************************************************************
* query SPO ready status
*
* this function has a few "side effects":
* on first call, the SPO is initialized
* on every call, the operating system is polled for input (select)
* if input is present, it is read into a buffer
* if the buffer contains the "#" escape, the line is handled in the
* emulator, otherwise the "INPUT REQUEST" interupt is caused
***********************************************************************/
BIT spo_ready(unsigned index) {
        struct timeval tv = {0, 0};
	char *spoinp = NULL;

	// initialize SPO if not ready
	if (!ready)
		spo_init("");

	// check for user input ready
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(0, &fds);
        if (select(1, &fds, NULL, NULL, &tv)) {
		spoinp = fgets(spoinbuf, sizeof spoinbuf, stdin); // no buffer overrun possible
	}
#ifdef USECAN
	else {
		// check whether a complete line has been received from the CANbus SPO
		spoinp = can_receive_string(canspo, spoinbuf, sizeof spoinbuf);
	}
#endif

	// any input ?
	if (spoinp != NULL) {
		// remove trailing control codes
		spoinp = spoinbuf + strlen(spoinbuf);
		while (spoinp >= spoinbuf && *spoinp <= ' ')
			*spoinp-- = 0;
		spoinp = spoinbuf;
		// divert input starting with '$' to our scanner
		if (*spoinp == '$') {
			int res = handle_option(spoinp+1);
			if (res == 0)
				sprintf(spoinbuf, "$OK\r\n");
			else
				sprintf(spoinbuf, "$ERROR %d\r\n", res);
			spo_print(spoinbuf);
			// mark the input buffer empty again
			spoinbuf[0] = 0;
			// remember when this input was
			time(&stamp);

		} else {
			// add EOL to the end
			strcat(spoinbuf, "\r");
			// signal input request
			CC->CCI05F = true;
			// the input line is read later, once the IRQ is handled by the MCP
		}
	}

	// finally return always ready
	return ready;
}

/***********************************************************************
* write a single line to SPO
***********************************************************************/
void spo_write(IOCU *u) {
	int i;
	char *spooutp = spooutbuf;
#if TIMESTAMP
	time_t now;
	struct tm tm;

	time(&now);
	// subtract stamp
	now -= stamp;
	gmtime_r(&now, &tm);
	if (timestamp)
		spooutp += sprintf(spooutp, "%02d:%02u:%02u", tm.tm_hour, tm.tm_min, tm.tm_sec);
#endif

loop:
	// read next word
	main_read_inc(u);
	// handle each char in this word
	for (i=0; i<8; i++) {
		get_ob(u);
		if (u->ob == 037)
			goto done;
		// prevent buffer overrun
		if (spooutp < spooutbuf + sizeof spooutbuf - 1)
			*spooutp++ = translatetable_bic2ascii[u->ob];
	}
	goto loop;

done:
	// add final CRLF, NULL
	*spooutp++ = '\r'; *spooutp++ = '\n'; *spooutp++ = 0;

	// print
	spo_print(spooutbuf);

#if AUTOEXEC
	// check for end of job and reload card deck if so
	if (autoexec > 0 && strstr(spooutbuf, auto_trigger1) && strstr(spooutbuf, auto_trigger2)) {
		sprintf(spooutbuf, "$ ***** AUTOEXEC #%d *****\r\n", autoexec++);
		spo_print(spooutbuf);
		time(&stamp);
		handle_option(auto_cmd);
	}
#endif

	// trivial all good result
	u->d_wc = 0;
	u->d_result = 0;
}

/***********************************************************************
* read a single line from the SPO input buffer
***********************************************************************/
void spo_read(IOCU *u) {
	int i;
	char *spoinp = spoinbuf;
	BIT gmset = false;

	// convert until EOL or any other control char found
	// there should also be a limitation of the number of words
	// unclear how much the MCP allocated, one place its 60 words(?)
	// with a buflen of 80 (chars) we should be safe

	// do while words (8 characters) while we have more data
	while (*spoinp >= ' ' || !gmset) {
		for (i=0; i<8; i++) {
			// note that spoinp stays on the invalid char
			// causing the rest of the word to be filled with
			// GM
			if (*spoinp >= ' ') {
				// printable char
				u->ib = translatetable_ascii2bic[*spoinp++ & 0x7f];
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

	// mark the input buffer empty again
	spoinbuf[0] = 0;

	// trivial all good result
	u->d_wc = 0;
	u->d_result = 0;
}

/***********************************************************************
* write a debug line to SPO
***********************************************************************/
void spo_debug_write(const char *msg) {
	char *spooutp = spooutbuf;
#if TIMESTAMP
	time_t now;
	struct tm tm;
	time(&now);
	// subtract stamp
	now -= stamp;
	gmtime_r(&now, &tm);
	if (timestamp)
		spooutp += sprintf(spooutp, "%02d:%02u:%02u ", tm.tm_hour, tm.tm_min, tm.tm_sec);
#endif
	spooutp += sprintf(spooutp, "%s\r\n", msg);

	// print it
	spo_print(spooutbuf);
}


