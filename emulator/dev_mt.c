/***********************************************************************
* b5500emulator
************************************************************************
* Copyright (c) 2018, Reinhard Meyer, DL5UY
* Licensed under the MIT License,
*       see LICENSE
************************************************************************
* b5500 magnetic tape emulation (MTA, MTB until MTT)
************************************************************************
* 2017-10-02  R.Meyer
*   Factored out from emulator.c
* 2018-03-16  R.Meyer
*   Changed old ACCESSOR method to main_*_inc functions
* 2018-05-05  R.Meyer
*   Converted to Input/Output Buffer
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
#define	TBUFLEN	8192

/***********************************************************************
* for each supported tape drive
***********************************************************************/
static struct mt {
	char	filename[NAMELEN];	// external filename
	FILE	*fp;			// file handle
	int	reclen;			// length of record in tbuf
        int	pos;			// position in file
	int	recpos;			// position of record begin
	BIT	ready;			// unit is ready
	BIT	eof;			// unit has encountered an eof
	BIT	writering;		// unit has write ring
	char	tbuf[TBUFLEN];		// tape buffer
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
* specify or close the file for emulation (read/write)
***********************************************************************/
static int set_mtfile(const char *v, void *) {
	if (!mtx) {
		printf("mt not specified\n");
		return 2; // FATAL
	}

	// if open, close current file
	if (mtx->fp) {
		fclose(mtx->fp);
	}

	mtx->fp = NULL;
	mtx->reclen = 0;
	mtx->pos = 0;
	mtx->recpos = 0;
	mtx->ready = false;
	mtx->eof = true;
	mtx->writering = false;

	strncpy(mtx->filename, v, NAMELEN);
	mtx->filename[NAMELEN-1] = 0;

	// now open the new file, if any name was given
	// if none given, the drive just stays unready
	if (mtx->filename[0]) {
		mtx->fp = fopen(mtx->filename, "r+");	// read/write
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
* specify or close the file for emulation (create and read/write)
***********************************************************************/
static int set_mtnewfile(const char *v, void *) {
	if (!mtx) {
		printf("mt not specified\n");
		return 2; // FATAL
	}

	// if open, close current file
	if (mtx->fp) {
		fclose(mtx->fp);
	}

	mtx->fp = NULL;
	mtx->reclen = 0;
	mtx->pos = 0;
	mtx->recpos = 0;
	mtx->ready = false;
	mtx->eof = true;
	mtx->writering = false;

	strncpy(mtx->filename, v, NAMELEN);
	mtx->filename[NAMELEN-1] = 0;

	// now open the new file, if any name was given
	// if none given, the drive just stays unready
	if (mtx->filename[0]) {
		mtx->fp = fopen(mtx->filename, "w+");	// create or truncate, then read/write
		if (mtx->fp) {
			mtx->ready = true;
			mtx->eof = false;
			mtx->writering = true;		// implicitly writeable
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
* set the writering flag
***********************************************************************/
static int set_mtwritering(const char *v, void *) {
	if (!mtx) {
		printf("mt not specified\n");
		return 2; // FATAL
	}

	mtx->writering = true;

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
	{"newfile",	set_mtnewfile},
	{"writering",	set_mtwritering},
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
        BIT mi, binary, reverse, usewc, read;

        int i;
	int cc;		// character counter
        BIT first;
        int lastchar;
        int lp;
	struct mt *mtx;

        mi = (u->d_control & CD_30_MI) ? true : false;
        binary = (u->d_control & CD_27_BINARY) ? true : false;
        reverse = (u->d_control & CD_26_DIR) ? true : false;
        usewc = (u->d_control & CD_25_USEWC) ? true : false;
        read = (u->d_control & CD_24_READ) ? true : false;

        // number of words to do
        words = (usewc) ? u->d_wc : 1024;

	mtx = mt + unit[u->d_unit][0].index;

        u->d_result = 0;

	cc = 0;

        if (!mtx->ready) {
                u->d_result = RD_18_NRDY;
                goto retresult;
        }

	/***************************************************************
	* REWIND
	***************************************************************/
        if (mi && reverse && !read) {
                if (trace) fprintf(trace, " REWIND\n");
                mtx->pos = 0;
                mtx->eof = false;
                goto retresult;
        }

	/***************************************************************
	* READ REVERSE
	***************************************************************/
        if (!mi && reverse && read) {
                if (trace) fprintf(trace, " %s READ REVERSE %u WORDS to %05o\n\t",
					binary ? "BINARY" : "ALPHA", words, u->d_addr);
                if (mtx->recpos >= 0) {
                        mtx->pos = mtx->recpos;
                        mtx->recpos = -1;
                        if (mi || words == 0) { // TODO: mi can never be true here!
                                // no data transfer - we are good
                                goto retresult;
                        }
                }
                // oh crap... - we really ought to read backwards here
                // return read error
                printf("*\tREAD BACKWARDS NOT POSSIBLE\n");
                u->d_result = RD_20_ERR;
                goto retresult;
        }

	/***************************************************************
	* READ (FORWARD)
	***************************************************************/
        if (!mi && !reverse && read) {
                if (trace) fprintf(trace, " %s READ %u WORDS to %05o\n\t",
					binary ? "BINARY" : "ALPHA", words, u->d_addr);
                mtx->recpos = mtx->pos;
                fseek(mtx->fp, mtx->pos, SEEK_SET);
                first = true;
                lastchar = -1;
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
                        mtx->pos++;
                        lastchar = i;

                        first = false;
                        // assemble char into word

                        if (trace) {
                                if (lp >= 80) {
                                        fprintf(trace,"'\n\t'");
                                        lp = 0;
                                }
                                fprintf(trace, "%c", translatetable_bic2ascii[i & 077]);
                                lp++;
                        }

                        if (!binary) {
                                // translate external BCL as ASCII to BIC??
                        }

			u->ib = i & 077;
			put_ib(u);
			cc++;
                        if (cc >= 8) {
                                if (words > 0) {
                                        words--;
					main_write_inc(u);
                                }
				cc = 0;
                        }
                } /* while(1) */

recend:         // record end reached
                if (trace)
                        fprintf(trace, "'\n");
                // store possible partial filled word
                if (words > 0 && cc > 0) {
			u->ib = 0;
			for (i=cc; i < 8; i++)
				put_ib(u);
                        words--;
			main_write_inc(u);
                }
                // return good result
                if (trace)
                        fprintf(trace, "\tWORDS REMAINING=%u LAST CHAR=%u\n", words, cc);
                goto retresult;
        }

	/***************************************************************
	* WRITE
	***************************************************************/
        if (!read) {
                if (trace) fprintf(trace, " %s WRITE %u WORDS to %05o\n\t",
					binary ? "BINARY" : "ALPHA", words, u->d_addr);
		if (!mtx->writering) {
		        // return no ring status
		        if (trace)
		                fprintf(trace, " WRITE BUT NO WRITE RING\n");
		        u->d_result = RD_20_ERR | RD_22_MAE;
		        goto retresult;
		}
        }

	/***************************************************************
	* UNSUPPORTED
	***************************************************************/
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
	u->d_wc = cc;
}


