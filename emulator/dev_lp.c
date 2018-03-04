/***********************************************************************
* b5500emulator
************************************************************************
* Copyright (c) 2017, Reinhard Meyer, DL5UY
* Licensed under the MIT License,
*       see LICENSE
************************************************************************
* b5500 line printer emulation (LPA, LPB)
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

/***********************************************************************
* typical labels (all are on a skip to 1 line):
* C0        1         2         3         4         5         6         7         8         9         10        11        12
* C1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901
* W1111111122222222333333334444444455555555...
*  <label.>0<mfid.>0<fid..><#><dat><title..................................................................................>
* " LABEL  000000000LINE   00185168?EXECUTE PATCH/MERGE                                                     PATCH  /MERGE"
* " LABEL  000000000LINE   00185168?EXECUTE PATCH/MERGE                                                     PATCH  /MERGE"
* " LABEL  0MCP13  0LISTING00185168? EXECUTE ESPOL/DISK                                                     ESPOL  /DISK"
* " LABEL  0MCP13  0LISTING00185168? EXECUTE ESPOL/DISK                                                     ESPOL  /DISK"
***********************************************************************/

#define PRINTERS 2
#define NAMELEN 100

/***********************************************************************
* for each supported printer
***********************************************************************/
enum pt	{pt_file=0, pt_lc10, pt_text};
static struct lp {
	char	filename[NAMELEN];
	FILE	*fp;
	enum pt	type;
	int 	lineno;
	BIT	ready;
} lp[PRINTERS];

static struct lp *lpx = NULL;

/***********************************************************************
* set to lpa/lpb
***********************************************************************/
static int set_lp(const char *v, void *data) {lpx = lp+(int)data; return 0; }

/***********************************************************************
* specify printer type
***********************************************************************/
static int set_lptype(const char *v, void *) {
	if (!lpx) {
		printf("lp not specified\n");
		return 2; // FATAL
	}
	if (strcmp(v, "file") == 0) {
		lpx->type = pt_file;
	} else if (strcmp(v, "lc10") == 0) {
		lpx->type = pt_lc10;
	} else if (strcmp(v, "text") == 0) {
		lpx->type = pt_text;
	} else {
		printf("unknown type\n");
		return 2; // FATAL
	}
	return 0; // OK
}

/***********************************************************************
* specify or close the file for emulation
***********************************************************************/
static int set_lpfile(const char *v, void *) {
	if (!lpx) {
		printf("lp not specified\n");
		return 2; // FATAL
	}
	strncpy(lpx->filename, v, NAMELEN);
	lpx->filename[NAMELEN-1] = 0;

	// if we are ready, close current file
	if (lpx->ready) {
		fclose(lpx->fp);
		lpx->fp = NULL;
		lpx->ready = false;
	}

	// now open the new file, if any name was given
	// if none given, the drive just stays unready
	if (lpx->filename[0]) {
		lpx->fp = fopen(lpx->filename, "w"); // printers are always write only
		if (lpx->fp) {
			lpx->ready = true;
			lpx->lineno = 1;
			return 0; // OK
		} else {
			// cannot open
			perror(lpx->filename);
			return 2; // FATAL
		}
	}
	return 0; // OK
}

/***********************************************************************
* command table
***********************************************************************/
static const command_t lp_commands[] = {
	{"lpa",		set_lp,	(void *) 0},
	{"lpb", 	set_lp, (void *) 1},
	{"type",	set_lptype},
	{"file",	set_lpfile},
	{NULL,		NULL},
};

/***********************************************************************
* Initialize command from argv scanner or special SPO input
***********************************************************************/
int lp_init(const char *option) {
	lpx = NULL; // require specification of a drive
	return command_parser(lp_commands, option);
}

/***********************************************************************
* query ready status
***********************************************************************/
BIT lp_ready(unsigned index) {
	if (index < PRINTERS)
		return lp[index].ready;
	return false;
}

/***********************************************************************
* write a single line
***********************************************************************/
void lp_write(IOCU *u) {
        unsigned count;
        BIT mi;
        WORD2 space;
        WORD4 skip;
	struct lp *lpx;
        ACCESSOR acc;
        int i;

        mi = (u->d_control & CD_30_MI) ? true : false;
        space = (u->d_result & 060) >> 4;
        skip = (u->d_result & 017) >> 0;
        acc.addr = u->d_addr;
        if (u->d_control & CD_25_USEWC)
                count = u->d_wc;
        else
                count = 0;
        acc.id = unit[u->d_unit][0].name;
        acc.MAIL = false;
	lpx = lp + unit[u->d_unit][0].index;

	u->d_result = 0;

        if (!lpx->ready) {
                u->d_result = RD_18_NRDY;
                goto retresult;
        }

        if (skip) {
                // skip to stop
		switch (lpx->type) {
		case pt_text:
			fprintf(lpx->fp, "****************************** SKIP %d ******************************\n", skip);
			break;
		case pt_file:
			fprintf(lpx->fp, "%c", '@'+skip);
			break;
		case pt_lc10:
			if (skip == 1 && lpx->lineno != 1)
				fprintf(lpx->fp, "\014");
			break;
		}
		lpx->lineno = 1;
        } else {
                // space
		switch (lpx->type) {
		case pt_text:
		        switch (space) {
		        case 1: case 3: fprintf(lpx->fp, "\n"); lpx->lineno += 2; break;
		        case 2: lpx->lineno++; break;
			}
			break;
		case pt_file:
		        switch (space) {
		        case 0: fprintf(lpx->fp, "0"); break;
		        case 1: case 3: fprintf(lpx->fp, "2"); lpx->lineno += 2; break;
		        case 2: fprintf(lpx->fp, "1"); lpx->lineno++; break;
			}
			break;
		case pt_lc10:
		        switch (space) {
		        case 0: fprintf(lpx->fp, "\033P\017"); break;
		        case 1: case 3: fprintf(lpx->fp, "\n\n\033P\017"); lpx->lineno += 2; break;
		        case 2: fprintf(lpx->fp, "\n\033P\017"); lpx->lineno++; break;
			}
			break;
                }
        }
        if (!mi) {
                // print
                while (count > 0) {
                        fetch(&acc);
                        for (i=42; i>=0; i-=6)
                                fputc(translatetable_bic2ascii[(acc.word>>i)&077], lpx->fp);
                        acc.addr++;
                        count--;
                }
        }
	switch (lpx->type) {
	case pt_text:
	case pt_file:
	        fprintf(lpx->fp, "\n");
		break;
	case pt_lc10:
	        fprintf(lpx->fp, "\r");
		break;
	}
	fflush(lpx->fp);

	// end of page reached?
	if (lpx->type != pt_text && lpx->lineno >= 66) {
		u->d_result |= RD_21_END;
	}

retresult:
	u->d_addr = acc.addr;
        // set printer finished IRQ
        switch (unit[u->d_unit][0].index) {
	case 0: CC->CCI06F = true; break;
	case 1: CC->CCI07F = true; break;
	}
}


