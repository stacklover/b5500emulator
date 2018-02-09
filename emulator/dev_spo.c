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
char	filename[NAMELEN];
FILE	*fp = stdin;
BIT	ready;
char	spoinbuf[BUFLEN];
char	spooutbuf[BUFLEN];
time_t	stamp;
#if AUTOEXEC
unsigned autoexec = false;
const char *auto_cmd = "cra file=cards/DCMCP-PATCH-COMPILE.card";
const char *auto_trigger1 = "ESPOL/DISK= ";
const char *auto_trigger2 = " EOJ";
#endif

/***********************************************************************
* set spo (no function)
***********************************************************************/
int set_spo(const char *, void *) {return 0; }

/***********************************************************************
* specify load type
***********************************************************************/
int set_spoload(const char *v, void *) {
	if (strcmp(v, "disk") == 0) {
		CC->CLS = false;
	} else if (strcmp(v, "card") == 0) {
		CC->CLS = true;
	} else {
		printf("unknown load type\n");
		return 2; // FATAL
	}
	return 0; // OK
}

/***********************************************************************
* specify autoexec on/off
***********************************************************************/
int set_autoexec(const char *v, void *) {
	if (strcmp(v, "on") == 0) {
		autoexec = true;
	} else if (strcmp(v, "off") == 0) {
		autoexec = false;
	} else {
		printf("specify on or off\n");
		return 2; // FATAL
	}
	return 0; // OK
}

/***********************************************************************
* command table
***********************************************************************/
const command_t spo_commands[] = {
	{"spo", set_spo},
	{"load", set_spoload},
#if AUTOEXEC
	{"autoexec", set_autoexec},
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
* emulator, otherwise the "INPUT REQUEST" interuppt is caused
***********************************************************************/
BIT spo_ready(unsigned index) {
        struct timeval tv = {0, 0};

	// initialize SPO if not ready
	if (!ready)
		spo_init("");

	// check for user input ready
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(0, &fds);
        if (select(1, &fds, NULL, NULL, &tv)) {
		char *spoinp = fgets(spoinbuf, sizeof spoinbuf, stdin); // no buffer overrun possible
		// any input ?
		if (spoinp != NULL) {
			// remove trailing control codes
			spoinp = spoinbuf + strlen(spoinbuf);
			while (spoinp >= spoinbuf && *spoinp <= ' ')
				*spoinp-- = 0;
			spoinp = spoinbuf;
			// divert input starting with '#' to our scanner
			if (*spoinp == '#') {
				int res = handle_option(spoinp+1);
				if (res == 0)
					printf("#ok\n");
				else
					printf("#error %d\n", res);
				// mark the input buffer empty again
				spoinbuf[0] = 0;
				// remember when this input was
				time(&stamp);

			} else {
				// signal input request
				CC->CCI05F = true;
				signalInterrupt("SPO", "INPUT REQUEST");
				// the input line is read later, once the IRQ is handled by the MCP
			}
		}
        }

	// finally return always ready
	return ready;
}

/***********************************************************************
* write a single line to SPO
***********************************************************************/
WORD48 spo_write(WORD48 iocw) {
	int count;
	ACCESSOR acc;
	char *spooutp = spooutbuf;
#if TIMESTAMP
	time_t now;
	struct tm tm;
	time(&now);
	// subtract stamp
	now -= stamp;
	gmtime_r(&now, &tm);
	spooutp += sprintf(spooutp, "%02d:%02u:%02u ", tm.tm_hour, tm.tm_min, tm.tm_sec);
#endif
	acc.id = "SPO";
	acc.MAIL = false;
	acc.addr = iocw & MASKMEM;

loop:	fetch(&acc);
	for (count=0; count<8; count++) {
		// prevent buffer overrun
		if (spooutp >= spooutbuf + sizeof spooutbuf - 1)
			goto done;
		if (((acc.word >> 42) & 0x3f) == 037)
			goto done;
		*spooutp++ = translatetable_bic2ascii[(acc.word>>42) & 0x3f];
		acc.word <<= 6;
	}
	acc.addr++;
	goto loop;

done:	*spooutp++ = '\r';
	*spooutp++ = '\n';
	*spooutp++ = 0;
	printf ("%s", spooutbuf);

#ifdef USECAN
	// send message to "real SPO"
	can_send_string(30, spooutbuf);
#endif

#if AUTOEXEC
	if (autoexec > 0 && strstr(spooutbuf, auto_trigger1) && strstr(spooutbuf, auto_trigger2)) {
		printf("***** AUTOEXEC #%d *****\n", autoexec++);
		time(&stamp);
		handle_option(auto_cmd);
	}
#endif
	return (iocw & (MASK_IODUNIT | MASK_IODREAD)) | acc.addr;
}

/***********************************************************************
* read a single line from the SPO input buffer
***********************************************************************/
WORD48 spo_read(WORD48 iocw) {
	int count;
	ACCESSOR acc;
	BIT gmset = false;		// remember if we have stored a GM already
	char *spoinp = spoinbuf;

	acc.id = "SPO";
	acc.MAIL = false;
	acc.addr = iocw & MASKMEM;

	// convert until EOL or any other control char found
	// there should also be a limitation of the number of words
	// unclear how much the MCP allocated, one place its 60 words(?)
	// with a buflen of 80 (chars) we should be safe

	// do while words (8 characters) while we have more data
	while (*spoinp >= ' ') {
		acc.word = 0;
		for (count=0; count<8; count++) {
			acc.word <<= 6;
			// note that spoinp stays on the invalid char
			// causing the rest of the word to be filled with
			// GM
			if (*spoinp >= ' ') {
				// printable char
				acc.word |= translatetable_ascii2bic[*spoinp++ & 0x7f];
			} else {
				// EOL or other char, fill word with GM
				acc.word |= 037;
				gmset = true;
			}
		}
		// store the complete word
		store(&acc);
		acc.addr++;
	}

	// store one word with GM, if no GM was entered
	if (!gmset) {
		acc.word = 03737373737373737LL;
		store(&acc);
		acc.addr++;
	}

	// mark the input buffer empty
	spoinbuf[0] = 0;

	return (iocw & (MASK_IODUNIT | MASK_IODREAD)) | acc.addr;
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
	spooutp += sprintf(spooutp, "%02d:%02u:%02u ", tm.tm_hour, tm.tm_min, tm.tm_sec);
#endif
	*spooutp++ = 0;
	printf ("%s~%s\n", spooutbuf, msg);
}


