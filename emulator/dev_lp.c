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

/*
 * typical labels (all are on a skip to 1 line):
 * C0        1         2         3         4         5         6         7         8         9         10        11        12
 * C1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901
 * W1111111122222222333333334444444455555555...
 *  <label.>0<mfid.>0<fid..><#><dat><title..................................................................................>
 * " LABEL  000000000LINE   00185168?EXECUTE PATCH/MERGE                                                     PATCH  /MERGE"
 * " LABEL  000000000LINE   00185168?EXECUTE PATCH/MERGE                                                     PATCH  /MERGE"
 * " LABEL  0MCP13  0LISTING00185168? EXECUTE ESPOL/DISK                                                     ESPOL  /DISK"
 * " LABEL  0MCP13  0LISTING00185168? EXECUTE ESPOL/DISK                                                     ESPOL  /DISK"
 */

#define PRINTERS 2
#define NAMELEN 100

/*
 * for each supported printer
 */
enum pt	{pt_file=0, pt_lc10};
struct lp {
	char	filename[NAMELEN];
	FILE	*fp;
	enum pt	type;
	int 	lineno;
	BIT	ready;
} lp[PRINTERS];

/*
 * Initialize command from argv scanner or special SPO input
 */
int lp_init(const char *option) {
	struct lp *lpx = NULL; // require specification of a printer
	const char *op = option;
	printf("printer option(s): %s\n", op);
	while (*op != 0) {
		if (strncmp(op, "lpa=", 4) == 0) {
			lpx = lp+0;
			op += 4;
		} else if (strncmp(op, "lpb=", 4) == 0) {
			lpx = lp+1;
			op += 4;
		} else if (lpx != NULL && strncmp(op, "file:", 5) == 0) {
			lpx->type = pt_file;
			op += 5;
		} else if (lpx != NULL && strncmp(op, "lc10:", 5) == 0) {
			lpx->type = pt_lc10;
			op += 5;
		} else if (lpx != NULL) {
			// assume rest is a filename
			strncpy(lpx->filename, op, NAMELEN);
			lpx->filename[NAMELEN-1] = 0;

			// if we are ready, close current file
			if (lpx->ready) {
				fclose(lpx->fp);
				lpx->fp = NULL;
				lpx->ready = false;
			}

			// now open the new file, if any name was given
			// if none given, the printer just stays unready
			if (lpx->filename[0] != '#') {
				lpx->fp = fopen(lpx->filename, "w");
				if (lpx->fp) {
					lpx->ready = true;
					return 0; // OK
				} else {
					// cannot open
					perror(lpx->filename);
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
BIT lp_ready(unsigned index) {
	if (index < PRINTERS)
		return lp[index].ready;
	return false;
}

/*
 * write a single line
 */
WORD48 lp_write(WORD48 iocw) {
        unsigned unitdes, count;
        BIT mi;
        WORD2 space;
        WORD4 skip;
	struct lp *lpx;

        // prepare result with unit and ready flag
        WORD48 result = iocw & (MASK_IODUNIT | MASK_IODREAD);
        ACCESSOR acc;
        int i;

        unitdes = (iocw & MASK_IODUNIT) >> SHFT_IODUNIT;
        mi = (iocw & MASK_IODMI) ? true : false;
        space = (iocw & 06000000) >> 19;
        skip = (iocw & 01700000) >> 15;
        acc.addr = (iocw & MASK_ADDR) >> SHFT_ADDR;
        if (iocw & MASK_IODUSEWC)
                count = (iocw & MASK_WCNT) >> SHFT_WCNT;
        else
                count = 0;
        acc.id = unit[unitdes][0].name;
        acc.MAIL = false;
	lpx = lp + unit[unitdes][0].index;

        if (!lpx->ready) {
                result |= MASK_IORNRDY | (acc.addr << SHFT_ADDR);
                goto retresult;
        }

        if (skip) {
                // skip to stop
		switch (lpx->type) {
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
		case pt_file:
		        switch (space) {
		        case 0: fprintf(lpx->fp, "0"); break;
		        case 1: case 3: fprintf(lpx->fp, "2"); break;
		        case 2: fprintf(lpx->fp, "1"); break;
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
	case pt_file:
	        fprintf(lpx->fp, "\n");
		break;
	case pt_lc10:
	        fprintf(lpx->fp, "\r");
		break;
	}
	fflush(lpx->fp);

	// end of  page reached?
	if (lpx->lineno >= 66) {
		result |= MASK_IORD21;
	}

        result |= (acc.addr << SHFT_ADDR);
retresult:
        // set printer finished IRQ
        switch (unit[unitdes][0].index) {
	case 0: CC->CCI06F = true; break;
	case 1: CC->CCI07F = true; break;
	}
        signalInterrupt(acc.id, "FIN");

        return result;
}


