/***********************************************************************
* b5500emulator
************************************************************************
* Copyright (c) 2017, Reinhard Meyer, DL5UY
* Licensed under the MIT License,
*       see LICENSE
************************************************************************
* b5500 magnetic tape emulation (MTA, MTB until MTT)
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

#define TAPES 16
#define NAMELEN 100
#define	TBUFLEN	164

/***********************************************************************
* for each supported tape drive
***********************************************************************/
static struct mt {
	char	filename[NAMELEN];
	FILE	*fp;
	BIT	ready;
	char	tbuf[TBUFLEN];
	char	*tbufp;
        int	line, lastpos;
	BIT	eof;
} mt[TAPES];

/***********************************************************************
* optional open file to write debugging traces into
***********************************************************************/
static FILE *trace = NULL;
static struct mt *mtx = NULL;

/***********************************************************************
* set to mta..mtt
***********************************************************************/
static int set_mt(const char *v, void *data) {mtx = mt+(int)data; return 0; }

/***********************************************************************
* specify or close the trace file
***********************************************************************/
static int set_mttrace(const char *v, void *) {
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
static int set_mtfile(const char *v, void *) {
	if (!mtx) {
		printf("mt not specified\n");
		return 2; // FATAL
	}
	strncpy(mtx->filename, v, NAMELEN);
	mtx->filename[NAMELEN-1] = 0;

	// if we are ready, close current file
	if (mtx->ready) {
		fclose(mtx->fp);
		mtx->fp = NULL;
		mtx->ready = false;
	}

	// now open the new file, if any name was given
	// if none given, the drive just stays unready
	if (mtx->filename[0]) {
		mtx->fp = fopen(mtx->filename, "r"); // currently tapes are always read only
		if (mtx->fp) {
			mtx->ready = true;
			mtx->eof = false;
			return 0; // OK
		} else {
			// cannot open
			perror(mtx->filename);
			return 2; // FATAL
		}
	}
	return 0; // OK
}

/***********************************************************************
* command table
***********************************************************************/
static const command_t mt_commands[] = {
	{"mta",		set_mt,	(void *) 0},
	{"mtb", 	set_mt, (void *) 1},
	{"mtc",		set_mt, (void *) 2},
	{"mtd",		set_mt, (void *) 3},
	{"mte",		set_mt, (void *) 4},
	{"mtf",		set_mt, (void *) 5},
	{"mth",		set_mt, (void *) 6},
	{"mtj",		set_mt, (void *) 7},
	{"mtk",		set_mt, (void *) 8},
	{"mtl",		set_mt, (void *) 9},
	{"mtm",		set_mt, (void *) 10},
	{"mtn",		set_mt, (void *) 11},
	{"mtp",		set_mt, (void *) 12},
	{"mtr",		set_mt, (void *) 13},
	{"mts",		set_mt, (void *) 14},
	{"mtt",		set_mt, (void *) 15},
	{"trace",	set_mttrace},
	{"file",	set_mtfile},
	{NULL,		NULL},
};

/***********************************************************************
* Initialize command from argv scanner or special SPO input
***********************************************************************/
int mt_init(const char *option) {
	mtx = NULL; // require specification of a drive
	return command_parser(mt_commands, option);
}

/***********************************************************************
* query ready status
***********************************************************************/
BIT mt_ready(unsigned index) {
	if (index < TAPES)
		return mt[index].ready;
	return false;
}

/***********************************************************************
* read or write a tape record or rewind
***********************************************************************/
void mt_access(IOCU *u) {
        unsigned words;
        BIT mi, binary, tapedir, usewc, read;

        ACCESSOR acc;
        int i, cp=42;
        BIT first;
        WORD48 w;
        int lastchar;
        int lp;
	struct mt *mtx;

        mi = (u->d_control & CD_30_MI) ? true : false;
        binary = (u->d_control & CD_27_BINARY) ? true : false;
        tapedir = (u->d_control & CD_26_DIR) ? true : false;
        usewc = (u->d_control & CD_25_USEWC) ? true : false;
        read = (u->d_control & CD_24_READ) ? true : false;
        acc.addr = u->d_addr;
        // number of words to do
        words = (usewc) ? u->d_wc : 1024;

        acc.id = unit[u->d_unit][0].name;
        acc.MAIL = false;
	mtx = mt + unit[u->d_unit][0].index;

        u->d_result = 0;

        if (!mtx->ready) {
                u->d_result = RD_18_NRDY;
                goto retresult;
        }

        // now analyze valid combinations
        if (mi && tapedir && !read) {
                // rewind
                mtx->line = 0;
                mtx->eof = false;
                if (trace)
                        fprintf(trace, " REWIND\n");
                goto retresult;
        }
        if (!mi && tapedir && read) {
                // read reverse
                if (trace)
                        fprintf(trace, " FAKE READ REVERSE\n");
                if (mtx->lastpos >= 0) {
                        mtx->line = mtx->lastpos;
                        mtx->lastpos = -1;
                        if (mi || words == 0) {
                                // no data transfer - we are good
                                goto retresult;
                        }
                }
                // oh crap - we really ought to read backwards here
                // return read error
                printf("*\tREAD BACKWARDS NOT POSSIBLE\n");
                u->d_result = RD_20_ERR;
                goto retresult;
        }
        if (!mi && !tapedir && read) {
                // read forward
                mtx->lastpos = mtx->line;
                fseek(mtx->fp, mtx->line, SEEK_SET);
                first = true;
                w = 0ll;
                cp = 42;
                lastchar = -1;
                if (trace)
                        fprintf(trace, " READ %u WORDS to %05o\n\t'", words, acc.addr);
                lp = 0;
                while (1) {
                        i = fgetc(mtx->fp);
                        if (i < 0) {
                                mtx->eof = true;
                                if (trace)
                                        fprintf(trace, " EOT\n");
				u->d_wc = WD_34_EOT;
                                u->d_result = 0;
                                goto retresult2;
                        }
                        // check for record end
                        if (i & 0x80) {
                                // record marker
                                if (lastchar == 0x8f) {
                                        // tape mark
                                        if (trace)
                                                fprintf(trace, "' MARK\n");
                                        u->d_result = RD_21_END;
                                        goto retresult;
                                }
                                // ignore on first char
                                if (!first)
                                        goto recend;
                        } else {
                                // no record marker
                                // the first char must have bit 7 set
                                if (first) {
                                        printf("*\ttape not positioned at record begin\n");
                                        u->d_result = RD_20_ERR;
                                        goto retresult;
                                }
                        }
                        // record position
                        mtx->line++;
                        lastchar = i;

                        first = false;
                        // assemble char into word

                        if (trace) {
                                if (lp >= 80) {
                                        fprintf(trace,"'\n\t'");
                                        lp = 0;
                                }
                                fprintf(trace, "%c", translatetable_bic2ascii[i & 0x3f]);
                                lp++;
                        }

                        if (!binary) {
                                // translate external BSL as ASCII to BIC??
                        }

                        w |= (WORD48)(i & 0x3f) << cp;
                        cp -= 6;
                        if (cp < 0) {
                                if (words > 0) {
                                        acc.word = w;
                                        words--;
                                        store(&acc);
                                        acc.addr++;
                                }
                                w = 0ll;
                                cp = 42;
                        }
                } /* while(1) */

recend:         // record end reached
                if (trace)
                        fprintf(trace, "'\n");
                // store possible partial filled word
                if (words > 0 && cp != 42) {
                        acc.word = w;
                        words--;
                        store(&acc);
                        acc.addr++;
                }
                // return good result
                if (trace)
                        fprintf(trace, "\tWORDS REMAINING=%u LAST CHAR=%u\n", words, (42-cp)/6);
                goto retresult;
        }
        if (!read) {
                // write??
                // return no ring status
                if (trace)
                        fprintf(trace, " WRITE BUT NO WRITE RING\n");
                u->d_result = RD_20_ERR | RD_22_MAE;
                goto retresult;
        }
        // what's left...
        printf("*\ttape operation not implemented %016llo\n", u->w);
        // return good result
        if (trace)
                fprintf(trace, " tape operation not implemented\n");

retresult:
        if (usewc)
                u->d_wc = words;
	else
		u->d_wc = 0;
retresult2:
	u->d_wc = (42-cp)/6;
	u->d_addr = acc.addr;
}


