/***********************************************************************
* b5500emulator
************************************************************************
* Copyright (c) 2017, Reinhard Meyer, DL5UY
* Licensed under the MIT License,
*       see LICENSE
************************************************************************
* b5500 card reader emulation (CRA, CRB)
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
#include "common.h"
#include "io.h"

#define READERS 2
#define NAMELEN 100
#define	CBUFLEN	164

/***********************************************************************
* for each supported card reader
***********************************************************************/
static struct cr {
	char	filename[NAMELEN];
	FILE	*fp;
	BIT	ready;
	char	cbuf[CBUFLEN];
	char	*cbufp;
} cr[READERS];

/***********************************************************************
* optional open file to write debugging traces into
***********************************************************************/
static FILE *trace = NULL;
static struct cr *crx = NULL;

/***********************************************************************
* set to cra/crb
***********************************************************************/
static int set_cr(const char *v, void *data) {crx = cr+(int)data; return 0; }

/***********************************************************************
* specify or close the trace file
***********************************************************************/
static int set_crtrace(const char *v, void *) {
	// if open, close existing trace file
	if (trace) {
		fclose(trace);
		trace = NULL;
	}
	// if a name is given, open new trace
	if (strlen(v) > 0) {
		trace = fopen(v, "w");
		if (!trace)
			return 2; // FATAL
	}
	return 0; // OK
}

/***********************************************************************
* specify or close the file for emulation
***********************************************************************/
static int set_crfile(const char *v, void *) {
	if (!crx) {
		printf("cr not specified\n");
		return 2; // FATAL
	}
	strncpy(crx->filename, v, NAMELEN);
	crx->filename[NAMELEN-1] = 0;

	// if we are ready, close current file
	if (crx->ready) {
		fclose(crx->fp);
		crx->fp = NULL;
		crx->ready = false;
	}

	// now open the new file, if any name was given
	// if none given, the drive just stays unready
	if (crx->filename[0]) {
		crx->fp = fopen(crx->filename, "r"); // cards are always read only
		if (crx->fp) {
			crx->ready = true;
			return 0; // OK
		} else {
			// cannot open
			perror(crx->filename);
			return 2; // FATAL
		}
	}
	return 0; // OK
}

/***********************************************************************
* command table
***********************************************************************/
static const command_t cr_commands[] = {
	{"cra",		set_cr,	(void *) 0},
	{"crb", 	set_cr, (void *) 1},
	{"trace",	set_crtrace},
	{"file",	set_crfile},
	{NULL,		NULL},
};

/***********************************************************************
* Initialize command from argv scanner or special SPO input
***********************************************************************/
int cr_init(const char *option) {
	crx = NULL; // require specification of a drive
	return command_parser(cr_commands, option);
}

/***********************************************************************
* query ready status
***********************************************************************/
BIT cr_ready(unsigned index) {
	if (index < READERS)
		return cr[index].ready;
	return false;
}

/***********************************************************************
* read a single card
***********************************************************************/
void cr_read(IOCU *u) {
        BIT mi;
	struct cr *crx;
	int i;

        ACCESSOR acc;
        int chars;

        mi = u->d_control & CD_30_MI ? true : false;
        acc.addr = u->d_addr;

        if (u->d_control & CD_27_BINARY)
                chars = 160;
        else
                chars = 80;

        u->d_result = 0;

        acc.id = unit[u->d_unit][1].name;
        acc.MAIL = false;
	crx = cr + unit[u->d_unit][1].index;

        if (!crx->ready) {
notready:
                u->d_result = RD_18_NRDY;
                goto retresult;
        }

	// read one ACSII line from file
        if (fgets(crx->cbuf, sizeof crx->cbuf, crx->fp) == NULL) {
                crx->ready = false;
		fclose(crx->fp);
		crx->fp = NULL;
		goto notready;
        }

        // remove trailing control codes
        crx->cbufp = crx->cbuf + strlen(crx->cbuf);
        while (crx->cbufp >= crx->cbuf && *crx->cbufp <= ' ')
                *crx->cbufp-- = 0;
        crx->cbufp = crx->cbuf;

        if ((u->d_control & CD_27_BINARY) && strlen(crx->cbuf) != (unsigned)chars) {
                printf("*\tERROR: binary card incorrect length(%u). abort\n", strlen(crx->cbuf));
                exit(0);
        }

        if (crx->cbuf[0] == '?' && !(u->d_control & CD_27_BINARY))
                u->d_result |= RD_19_PAR;

        while (chars > 0) {
		acc.word = 0LL;
		for (i=0; i<8; i++) {
		        if (*crx->cbufp >= ' ') {
		                acc.word = (acc.word << 6) | translatetable_ascii2bic[*crx->cbufp & 0x7f];
		                crx->cbufp++;
		        } else {
		                acc.word = (acc.word << 6) | 060; // blank
		        }
		}
                chars -= 8;
		if (!mi) {
	                store(&acc);
        	        acc.addr++;
		}
        }

retresult:
	u->d_wc = 0;
	u->d_addr = acc.addr;
}


