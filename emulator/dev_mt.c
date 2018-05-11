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
*   Converted to Input/Output Buffer and Added Write Capability
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
	//int	recpos;			// position of record begin
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
	//mtx->recpos = 0;
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
	//mtx->recpos = 0;
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
* read a tape record into the tape buffer
* returns 0: all good
* returns 2: EOT encountered
* returns 3: record exceeds tape buffer
* returns 4: tape mark detected
* returns 5: record does not start with bit 7 set
* start condition: pos must point the record begin
* end condition: pos points to next record begin
***********************************************************************/
static int mt_read_record(struct mt *mt) {
	int lp;
	int data;

	mt->reclen = 0;
	//mt->recpos = mt->pos;

	fseek(mt->fp, mt->pos, SEEK_SET);
	lp = 0;

	while (1) {
		data = fgetc(mt->fp);
		if (data < 0) {
			mt->eof = true;
			return 2;
		}
		// the first char of each record must have bit 7 set
		if (mt->reclen == 0 && (data & 0x80) == 0) {
			// error here
			return 5;
		}
		// at non-first char it denotes record end
		if (mt->reclen > 0 && (data & 0x80) != 0) {
			// record complete
			// is it a tape mark?
			if (mt->reclen == 1 && mt->tbuf[0] == 0x0f)
				return 4;
			return 0;
		}
		// trace output
		if (trace) {
			if (lp >= 80) {
				fprintf(trace,"'\n\t'");
				lp = 0;
			}
			fprintf(trace, "%c", translatetable_bic2ascii[data & 077]);
			lp++;
		}
		// store char in buffer
		if (mt->reclen >= TBUFLEN) {
			// record exceeds buffer size
			return 3;
		}
		// now really store it
		mt->tbuf[mt->reclen] = data & 0x7f;
		mt->reclen++;
		mt->pos++;
	}
	// we never come here, but the compiler demands it:
	return 0;
}

/***********************************************************************
* reverse read a tape record into the tape buffer
* returns 0: all good
* returns 1: BOT encountered
* returns 3: record exceeds tape buffer
* returns 4: tape mark detected
* start condition: pos must point the the record end+1
* end condition: pos points to the record begin
***********************************************************************/
static int mt_read_record_reverse(struct mt *mt) {
	int lp;
	int data;

	mt->reclen = 0;
	//mt->recpos = mt->pos;

	lp = 0;

	while (1) {
		mt->pos--;
		if (mt->pos < 0) {
			mt->pos = 0;
			return 1;
		}
		fseek(mt->fp, mt->pos, SEEK_SET);
		data = fgetc(mt->fp);
		if (data < 0) {
			mt->eof = true;
			return 1;
		}
		// store char in buffer
		if (mt->reclen >= TBUFLEN) {
			// record exceeds buffer size
			return 3;
		}
		// trace output
		if (trace) {
			if (lp >= 80) {
				fprintf(trace,"'\n\t'");
				lp = 0;
			}
			fprintf(trace, "%c", translatetable_bic2ascii[data & 077]);
			lp++;
		}
		// now really store it
		mt->tbuf[mt->reclen] = data & 0x7f;
		mt->reclen++;
		// bit 7 set denotes (reverse) record begin
		if ((data & 0x80) != 0) {
			// record complete
			// is it a tape mark?
			if (mt->reclen == 0)
				return 4;
			return 0;
		}
	}
	// we never come here, but the compiler demands it:
	return 0;
}

/***********************************************************************
* read or write a tape record or rewind
***********************************************************************/
void mt_access(IOCU *u) {
        unsigned words;
        BIT mi, binary, reverse, usewc, read;

        int i;
	int cc;		// character counter
	struct mt *mtx;

	// all the flags that distinguish the operations
        mi = (u->d_control & CD_30_MI) ? true : false;
        binary = (u->d_control & CD_27_BINARY) ? true : false;
        reverse = (u->d_control & CD_26_DIR) ? true : false;
        usewc = (u->d_control & CD_25_USEWC) ? true : false;
        read = (u->d_control & CD_24_READ) ? true : false;

        // number of words to do
        words = usewc ? u->d_wc : 1023;

	mtx = mt + unit[u->d_unit][0].index;

        u->d_result = 0;

        if (!mtx->ready) {
                u->d_result = RD_18_NRDY;
                return;
        }

	if (trace) {
		fprintf(trace, unit[u->d_unit][0].name); 
		if (read) fprintf(trace, " READ");
			else fprintf(trace, " WRITE");
		if (usewc) fprintf(trace, " WC=%d", words);
			else fprintf(trace, " GM");
		if (reverse) fprintf(trace, " REVERSE");
		if (binary) fprintf(trace, " BINARY");
			else fprintf(trace, " ALPHA");
		if (mi) fprintf(trace, " MI");
	}

	/***************************************************************
	* READ Section
	***************************************************************/
	if (read) {
	        if (trace) fprintf(trace, " ADDR=%05o\n\t'", u->d_addr);
		// read a record into local buffer
		if (reverse)
			i = mt_read_record_reverse(mtx);
		else
			i = mt_read_record(mtx);

		// analyze result
		switch (i) {
		case 1:	// BOT
			mtx->eof = true;
			if (trace)
				fprintf(trace, "' BOT\n");
			u->d_wc = WD_35_BOT;
			u->d_result = RD_19_PAR;
			return;
		case 2:	// EOT
			mtx->eof = true;
			if (trace)
				fprintf(trace, "' EOT\n");
			u->d_wc = WD_34_EOT;
			u->d_result = RD_19_PAR;
			return;
		case 3:	// record too long
			if (trace)
				fprintf(trace, "' RECORD TOO LONG\n");
			u->d_result = RD_20_ERR;
			return;
		case 4:	// tape mark
			if (trace)
				fprintf(trace, "' TAPE MARK\n");
			u->d_result = RD_21_END;
			return;
		case 5:	// format error
			if (trace)
				fprintf(trace, "' .BCD FORMAT ERROR\n");
			u->d_result = RD_20_ERR;
			return;
		case 0:	// normal record
	                if (!binary) {
	                        // translate external BCL as ASCII to BIC??
	                }
			if (reverse) {
				cc = 7;
				for (i=0; i<mtx->reclen; i++) {
					u->ib = mtx->tbuf[i] & 077;
					put_ib_reverse(u);
					cc--;
			                if (cc < 0) {
			                        if (words > 0) {
			                                words--;
							main_write_dec(u);
			                        }
						cc = 7;
			                }
				}
			} else {
				// for group mark ending, add a group mark to the buffer
				if (!binary && !usewc)
					mtx->tbuf[mtx->reclen++] = 037;
				cc = 0;
				for (i=0; i<mtx->reclen; i++) {
					u->ib = mtx->tbuf[i] & 077;
					put_ib(u);
					cc++;
			                if (cc > 7) {
			                        if (words > 0) {
			                                words--;
							main_write_inc(u);
			                        }
						cc = 0;
			                }
				}
			}
			// record end reached
			if (trace)
				fprintf(trace, "'\n");
			// store possible partial filled word
			if (reverse) {
				if (words > 0 && cc < 7) {
					u->ib = 014;
					for (i=0; i<cc; i++)
						put_ib_reverse(u);
					words--;
					main_write_dec(u);
				}
			} else {
				if (words > 0 && cc > 0) {
					u->ib = 000;
					for (i=cc; i<8; i++)
						put_ib(u);
					words--;
					main_write_inc(u);
				}
			}
			// return good result
			if (usewc) {
				u->d_wc = words;
				if (trace)
					fprintf(trace, "\tWORDS REMAINING=%u\n", words);
			} else {
				u->d_wc = cc;
				if (trace)
					fprintf(trace, "\tLAST CHAR=%u\n", cc);
			}
		default:
			return;
		}
		return;
	}

	/***************************************************************
	* Write Section
	***************************************************************/
	if (!read) {
		/***************************************************************
		* Special WRITE: REWIND
		***************************************************************/
		if (reverse) {
		        if (trace) fprintf(trace, "-> REWIND\n");
			// we should also have MI=1, BINARY=0, USEWC=0
			if (!mi || binary || usewc)
				printf("* WARNING: TAPE REWIND WITH UNEXPECTED OPTIONS IOCW=%016llo\n", u->w);
		        mtx->pos = 0;
		        mtx->eof = false;
		        return;
		}

		/***************************************************************
		* Regular WRITE
		***************************************************************/
                if (trace) fprintf(trace, " ADDR=%05o\n\t'", u->d_addr);
		// we should also have BINARY equal to USEWC
		if (binary != usewc)
			printf("* WARNING: TAPE WRITE WITH UNEXPECTED OPTIONS IOCW=%016llo\n", u->w);
		if (!mtx->writering) {
		        // return no ring status
		        if (trace)
		                fprintf(trace, "' NO WRITE RING\n");
		        u->d_result = RD_20_ERR | RD_22_MAE;
		        return;
		}

		// read data from memory into local buffer
		mtx->reclen = 0;
		while (words > 0) {
			main_read_inc(u);
			words--;
			for (i=0; i<8; i++) {
				get_ob(u);
				// break out of this, if we use GM ending
				if (!binary && u->ob == 037)
					goto end_of_write;
				// if MI is set, we still read memory but store 000 in the tape buffer instead
				mtx->tbuf[mtx->reclen++] = mi ? 000 : u->ob;
			}
                }

end_of_write:	// now write data to tape

		// trace output
		if (trace) {
			int lp = 0;
			int j;
			for (j=0; j<mtx->reclen; j++) {
				if (lp >= 80) {
					fprintf(trace,"'\n\t'");
					lp = 0;
				}
				fprintf(trace, "%c", translatetable_bic2ascii[mtx->tbuf[j] & 077]);
				lp++;
			}
		}

		// in alpha mode, we have to do some checks first:
		if (!binary) {
			// remove "unique marks" at begin of buffer
			for (i=0; i<mtx->reclen && mtx->tbuf[i] == 014; i++)
				;
			if (trace)
				fprintf(trace,"'\n\t(%d unique marks ignored)\n", i);
		} else {
			i = 0;
			if (trace)
				fprintf(trace,"'\n");
		}

		// set bit 7 of first char in buffer
		mtx->tbuf[i] |= 0x80;

		// anything left to write ?
		if (mtx->reclen > i) {
			fseek(mt->fp, mt->pos, SEEK_SET);
			fwrite(mtx->tbuf + i, 1, mtx->reclen - i, mt->fp);
			mtx->pos += mtx->reclen - i;
		}

		// return good result and WC=0
		u->d_wc = 0;
		return;
	}
}


