/***********************************************************************
* b5500emulator
************************************************************************
* Copyright (c) 2017, Reinhard Meyer, DL5UY
* Licensed under the MIT License,
*       see LICENSE
************************************************************************
* b5500 head per track disk drive emulation (DKA, DKB)
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

/*
 * note: a DRIVE here is an array of upto 100 disk drives with 48MB each
 * giving a total capacity of 480MB
 * we emulate two DRIVES
 */
#define DRIVES 2
#define NAMELEN 100
#define	DBUFLEN	257

/*
 * for each supported tape drive
 */
struct dk {
	char	filename[NAMELEN];
	FILE	*fp;
	BIT	ready;
	BIT	readcheck;
	char	dbuf[DBUFLEN];
	char	*dbufp;
} dk[DRIVES];

/*
 * optional open file to write debugging traces into
 */
static FILE *trace = NULL;


/*
 * Initialize command from argv scanner or special SPO input
 */
int dk_init(const char *option) {
	struct dk *dkx = NULL; // require specification of a drive
	const char *op = option;
	printf("disk drive option(s): %s\n", op);
	while (*op != 0) {
		if (strncmp(op, "dka=", 4) == 0) {
			dkx = dk+0;
			op += 4;
		} else if (strncmp(op, "dkb=", 4) == 0) {
			dkx = dk+1;
			op += 4;
		} else if (strncmp(op, "trace=", 6) == 0) {
			op += 6;
			// close existing trace file
			if (trace) {
				fclose(trace);
				trace = NULL;
			}
			// open new trace, if name was given
			if (strlen(op) > 0) {
				trace = fopen(op, "w");
			}
			return 0;
		} else if (dkx != NULL) {
			// assume rest is a filename
			strncpy(dkx->filename, op, NAMELEN);
			dkx->filename[NAMELEN-1] = 0;

			// if we are ready, close current file
			if (dkx->ready) {
				fclose(dkx->fp);
				dkx->fp = NULL;
				dkx->ready = false;
			}

			// reset flags
			dkx->readcheck = false;

			// now open the new file, if any name was given
			// if none given, the drive just stays unready
			if (dkx->filename[0] != '#') {
				dkx->fp = fopen(dkx->filename, "r+");
				if (dkx->fp) {
					dkx->ready = true;
					return 0; // OK
				} else {
					// cannot open
					perror(dkx->filename);
					return 2; // FATAL
				}
			}
			return 0;
		} else {
			// bogus information
			return 1; // WARNING
		}
	}
	return 1; // WARNING
}

/*
 * query ready status
 */
BIT dk_ready(unsigned index) {
	if (index < DRIVES)
		return dk[index].ready;
	return false;
}

/*
 * read or write, check or inquire
 */
WORD48 dk_access(WORD48 iocw) {
        unsigned unitdes, count, segcnt, words;
        // prepare result with unit and read flag
        WORD48 result = iocw & (MASK_IODUNIT | MASK_IODREAD);
        ACCESSOR acc;
        int i, j, k;
	int eu, diskfileaddr;
	struct dk *dkx;

        unitdes = (iocw & MASK_IODUNIT) >> SHFT_IODUNIT;
        count = (iocw & MASK_WCNT) >> SHFT_WCNT;
        segcnt = (iocw & MASK_IODSEGCNT) >> SHFT_IODSEGCNT;
        acc.addr = (iocw & MASK_ADDR) >> SHFT_ADDR;
        // number of words to do
        words = (iocw & MASK_IODUSEWC) ? count : segcnt * 30;

        acc.id = unit[unitdes][0].name;
        acc.MAIL = false;
	dkx = dk + unit[unitdes][0].index;

        if (!dkx->ready) {
                result |= MASK_IORNRDY | (acc.addr << SHFT_ADDR);
                goto retresult;
        }

        // fetch first word from core with disk address
        fetch(&acc);
        diskfileaddr = 0;
        eu = (acc.word >> 36) & 0xf;
        for (i=5; i>=0; i--) {
                unsigned char ch = (acc.word >> 6*i) & 0xf;
                if (ch > 9) {
                          printf("*** FATAL: disk file address has value > 9 ***\n");
                          exit(2);
                }
                diskfileaddr *= 10;
                diskfileaddr += ch;
        }

	// proceed to first data word
	acc.addr++;

        if (trace) {
                fprintf(trace, "%08u UNIT=%u IOCW=%016llo, EU:DFA=%d:%06d", instr_count, unitdes, iocw, eu, diskfileaddr);
        }

        if (eu < 0 || eu > 9 || diskfileaddr < 0 || diskfileaddr > 199999) {
                // not supported
                if (trace)
                        fprintf(trace, " NOT SUPPORTED\n");
                result |= MASK_IORNRDY;
        } else if (iocw & MASK_IODMI) {
                // special case when memory inhibit
                dkx->readcheck = true;
                if (trace)
                        fprintf(trace, " READ CHECK SEGMENTS=%02u\n", segcnt);
        } else if (words == 0) {
                // special case when words=0
                if (trace)
                        fprintf(trace, " INTERROGATE\n");
        } else if (iocw & MASK_IODREAD) {
                // regular read
                if (trace)
                        fprintf(trace, " READ WORDS=%02u\n", words);

                // read until word count exhausted
                while (words > 0) {
                        // get the physical record
                        fseek(dkx->fp, (eu*200000+diskfileaddr)*256, SEEK_SET);
                        dkx->dbufp = dkx->dbuf;
                        // on read problems or if the signature is missing or wrong, this record has never been written
                        if (fread(dkx->dbuf, sizeof(char), 256, dkx->fp) != 256 ||
                                  strncmp(dkx->dbuf, "DADDR(", 6) != 0 ||
                                  (dkx->dbuf[6]-'0') != eu ||
                                  dkx->dbuf[7] != ':' ||
                                  strtol(dkx->dbuf+8, NULL, 10) != diskfileaddr ||
                                  dkx->dbuf[14] != ')') {
                                // return bogus data
                                memset(dkx->dbuf, 0, 255);
                                dkx->dbuf[255] = '\n';
                                printf("*** DISKIO READ OF RECORD NEVER WRITTEN DFA=%d:%06d ***\n", eu, diskfileaddr);
                        }
                        dkx->dbufp += 15;

                        // always read 30 words
                        for (i=0; i<3; i++) {
                                if (trace)
                                        fprintf(trace, "\t%05o %d:%06d", acc.addr, eu, diskfileaddr);
                                for (j=0; j<10; j++) {
                                        if (trace)
                                                fprintf(trace, " %-8.8s", dkx->dbufp);
                                        // store until word count exhausted
                                        if (words > 0) {
						acc.word = 0LL;
						for (k=0; k<8; k++) {
							if (*dkx->dbufp >= ' ') {
								acc.word = (acc.word << 6) | translatetable_ascii2bic[*dkx->dbufp & 0x7f];
								dkx->dbufp++;
							} else {
								acc.word = (acc.word << 6); // blank
							}
						}
                                                store(&acc);
                                                acc.addr++;
                                                words--;
                                        }
                                }
                                if (trace)
                                        fprintf(trace, "\n");
                        }
                        // next record address
                        diskfileaddr++;
                }
        } else {
                // regular write
                if (trace)
                        fprintf(trace, " WRITE WORDS=%02u\n", words);

                // keep writing records until word count is exhausted
                while (words > 0) {
                        // prepare buffer pointer and write header
                        dkx->dbufp = dkx->dbuf;
                        dkx->dbufp += sprintf(dkx->dbufp, "DADDR(%d:%06d)", eu, diskfileaddr);

                        // always write 30 words
                        for (i=0; i<3; i++) {
                                if (trace)
                                        fprintf(trace, "\t%05o %d:%06d", acc.addr, eu, diskfileaddr);
                                for (j=0; j<10; j++) {
                                        if (words > 0) {
                                                // if word count NOT exhausted, write next word
                                                fetch(&acc);
                                                acc.addr++;
						for (k=0; k<8; k++) {
							dkx->dbufp[7-k] = translatetable_bic2ascii[acc.word & 0x3f];
							acc.word >>= 6;
						}
                                                words--;
                                        } else {
                                                // if word count exhausted, write zeros
						for (k=0; k<8; k++)
							dkx->dbufp[7-k] = '0';
                                        }
					dkx->dbufp += 8;
                                        if (trace)
                                                fprintf(trace, " %-8.8s", dkx->dbufp-8);
                                }
                                if (trace)
                                        fprintf(trace, "\n");
                        }

                        // mark end of record
                        dkx->dbufp += sprintf(dkx->dbufp, "\n");

                        // sanity check - should never fail
                        if (dkx->dbufp != dkx->dbuf+256) {
                                printf("*** DISKIO WRITE SANITY CHECK FAILED ***\n");
                                exit(2);
                        }

                        // write record to physical file
                        fseek(dkx->fp, (eu*200000+diskfileaddr)*256, SEEK_SET);
                        fwrite(dkx->dbuf, sizeof(char), 256, dkx->fp);
                        fflush(dkx->fp);

                        // next record address
                        diskfileaddr++;
                }

        }
retresult:
        result |= (acc.addr << SHFT_ADDR);
        if (iocw & MASK_IODUSEWC)
                result |= ((WORD48)words << SHFT_WCNT);

	if (trace)
		fflush(trace);

        return result;
}


