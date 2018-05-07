/***********************************************************************
* b5500emulator
************************************************************************
* Copyright (c) 2018, Reinhard Meyer, DL5UY
* Licensed under the MIT License,
*       see LICENSE
************************************************************************
* b5500 card punch emulation (CPA, CPB)
************************************************************************
* 2018-05-04  R.Meyer
*   Copied from dev_cr.c
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

#define PUNCHES 1
#define NAMELEN 100
#define	CBUFLEN	84

/***********************************************************************
* for each supported card reader
***********************************************************************/
static struct cp {
	char	filename[NAMELEN];
	FILE	*fp;
	BIT	ready;
} cp[PUNCHES];

/***********************************************************************
* optional open file to write debugging traces into
***********************************************************************/
static FILE *trace = NULL;
static struct cp *cpx = NULL;

/***********************************************************************
* set to cpa
***********************************************************************/
static int set_cp(const char *v, void *data) {cpx = cp+(int)data; return 0; }

/***********************************************************************
* specify or close the trace file
***********************************************************************/
static int set_cptrace(const char *v, void *) {
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
static int set_cpfile(const char *v, void *) {
	if (!cpx) {
		printf("cp not specified\n");
		return 2; // FATAL
	}
	strncpy(cpx->filename, v, NAMELEN);
	cpx->filename[NAMELEN-1] = 0;

	// if we are ready, close current file
	if (cpx->ready) {
		fclose(cpx->fp);
		cpx->fp = NULL;
		cpx->ready = false;
	}

	// now open the new file, if any name was given
	// if none given, the drive just stays unready
	if (cpx->filename[0]) {
		cpx->fp = fopen(cpx->filename, "w"); // card punch is always write only
		if (cpx->fp) {
			cpx->ready = true;
			return 0; // OK
		} else {
			// cannot open
			perror(cpx->filename);
			return 2; // FATAL
		}
	}
	return 0; // OK
}

/***********************************************************************
* command table
***********************************************************************/
static const command_t cp_commands[] = {
	{"cpa",		set_cp,	(void *) 0},
	{"trace",	set_cptrace},
	{"file",	set_cpfile},
	{NULL,		NULL},
};

/***********************************************************************
* Initialize command from argv scanner or special SPO input
***********************************************************************/
int cp_init(const char *option) {
	cpx = NULL; // require specification of a drive
	return command_parser(cp_commands, option);
}

/***********************************************************************
* query ready status
***********************************************************************/
BIT cp_ready(unsigned index) {
	if (index < PUNCHES)
		return cp[index].ready;
	return false;
}

/***********************************************************************
* punch a single card
***********************************************************************/
void cp_write(IOCU *u) {
        BIT mi;
	struct cp *cpx;
	int w, i;

        mi = u->d_control & CD_30_MI ? true : false;

        u->d_result = 0; // no errors so far

	cpx = cp + unit[u->d_unit][1].index;

        if (!cpx->ready) {
                u->d_result = RD_18_NRDY;
                goto retresult;
        }

        if (!mi) {
                // "punch" 10 words of 8 chars each
                for (w=0; w<10; w++) {
                        main_read_inc(u);
                        for (i=0; i<8; i++) {
                                get_ob(u);
                                fputc(translatetable_bic2ascii[u->ob], cpx->fp);
			}
                }
		fputc('\r', cpx->fp);
		fputc('\n', cpx->fp);
		fflush(cpx->fp);
        }

retresult:
	u->d_wc = 0;
}


