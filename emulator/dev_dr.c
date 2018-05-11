/***********************************************************************
* b5500emulator
************************************************************************
* Copyright (c) 2018, Reinhard Meyer, DL5UY
* Licensed under the MIT License,
*       see LICENSE
************************************************************************
* b5500 magnetic drum emulation (DRA, DRB)
************************************************************************
* 2018-05-04  R.Meyer
*   Copied from dev_cp.c
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

#define DRUMS 2
#define NAMELEN 100

/***********************************************************************
* for each supported magnetic drum
***********************************************************************/
static struct dr {
	char	filename[NAMELEN];
	FILE	*fp;
	BIT	ready;
	WORD48	drum[32768];
} dr[DRUMS];

/***********************************************************************
* optional open file to write debugging traces into
***********************************************************************/
static FILE *trace = NULL;
static struct dr *drx = NULL;

/***********************************************************************
* set to dra
***********************************************************************/
static int set_dr(const char *v, void *data) {drx = dr+(int)data; return 0; }

/***********************************************************************
* specify or close the trace file
***********************************************************************/
static int set_drtrace(const char *v, void *) {
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
static int set_drfile(const char *v, void *) {
	if (!drx) {
		printf("dr not specified\n");
		return 2; // FATAL
	}
	strncpy(drx->filename, v, NAMELEN);
	drx->filename[NAMELEN-1] = 0;

	// if we are ready, close current file
	if (drx->ready) {
		fclose(drx->fp);
		drx->fp = NULL;
		drx->ready = false;
	}

	// now open the new file, if any name was given
	// if none given, the drive just stays unready
	if (drx->filename[0]) {
		drx->fp = fopen(drx->filename, "r+");
		if (drx->fp) {
			drx->ready = true;
			return 0; // OK
		} else {
			// cannot open
			perror(drx->filename);
			return 2; // FATAL
		}
	}
	return 0; // OK
}

/***********************************************************************
* command table
***********************************************************************/
static const command_t dr_commands[] = {
	{"dra",		set_dr,	(void *) 0},
	{"drb",		set_dr,	(void *) 1},
	{"trace",	set_drtrace},
	{"file",	set_drfile},
	{NULL,		NULL},
};

/***********************************************************************
* Initialize command from argv scanner or special SPO input
***********************************************************************/
int dr_init(const char *option) {
	drx = NULL; // require specification of a drive
	return command_parser(dr_commands, option);
}

/***********************************************************************
* query ready status
***********************************************************************/
BIT dr_ready(unsigned index) {
	if (index < DRUMS)
		return dr[index].ready;
	return false;
}

/***********************************************************************
* transfer to/from magnetic drum
***********************************************************************/
void dr_access(IOCU *u) {
        BIT read;
	ADDR15 addr;
	struct dr *drx;

        read = u->w & MASK_IODDRUMOP ? true : false;
	addr = (u->w & MASK_IODDRUMAD) >> SHFT_IODDRUMAD;

        u->d_result = 0; // no errors so far

	drx = dr + unit[u->d_unit][0].index;

        if (!drx->ready) {
                u->d_result = RD_18_NRDY;
                goto retresult;
        }

	if (trace)
		fprintf(trace, unit[u->d_unit][0].name); 

	if (read) {
		if (trace)
			fprintf(trace, " READ %u WORDS FROM %05o TO %05o\n",
				u->d_wc, addr, u->d_addr);
		while (u->d_wc > 0) {
			u->w = drx->drum[addr++];
			main_write_inc(u);
			u->d_wc--;
		}
	} else {
		if (trace)
			fprintf(trace, " WRITE %u WORDS FROM %05o TO %05o\n",
				u->d_wc, u->d_addr, addr);
		while (u->d_wc > 0) {
			main_read_inc(u);
			drx->drum[addr++] = u->w;
			u->d_wc--;
		}
	}

retresult:
	u->d_wc = 0;
}


