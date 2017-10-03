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

#define READERS 2
#define NAMELEN 100
#define	CBUFLEN	164
/*
 * for each supported card reader
 */
struct cr {
	char	filename[NAMELEN];
	FILE	*fp;
	BIT	ready;
	char	cbuf[CBUFLEN];
	char	*cbufp;
} cr[READERS];

/*
 * Initialize command from argv scanner or special SPO input
 */
int cr_init(const char *option) {
	struct cr *crx = NULL; // require specification of a reader
	const char *op = option;
	printf("card reader option(s): %s\n", op);
	while (*op != 0) {
		if (strncmp(op, "cra=", 4) == 0) {
			crx = cr+0;
			op += 4;
		} else if (strncmp(op, "crb=", 4) == 0) {
			crx = cr+1;
			op += 4;
		} else if (crx != NULL) {
			// assume rest is a filename
			strncpy(crx->filename, op, NAMELEN);
			crx->filename[NAMELEN-1] = 0;

			// if we are ready, close current file
			if (crx->ready) {
				fclose(crx->fp);
				crx->fp = NULL;
				crx->ready = false;
			}

			// now open the new file, if any name was given
			// if none given, the reader just stays unready
			if (crx->filename[0] != '#') {
				crx->fp = fopen(crx->filename, "r");
				if (crx->fp) {
					crx->ready = true;
					return 0; // OK
				} else {
					// cannot open - stay unready
					perror(crx->filename);
					return 2; // ERROR
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
BIT cr_ready(unsigned index) {
	if (index < READERS)
		return cr[index].ready;
	return false;
}

/*
 * read a single card
 */
WORD48 cr_read(WORD48 iocw) {
        unsigned unitdes;
        BIT mi;
	struct cr *crx;
	int i;

        WORD48 result = iocw & (MASK_IODUNIT | MASK_IODREAD);
        ACCESSOR acc;
        int chars;

        unitdes = (iocw & MASK_IODUNIT) >> SHFT_IODUNIT;
        mi = (iocw & MASK_IODMI) ? true : false;
        acc.addr = (iocw & MASK_ADDR) >> SHFT_ADDR;
        if (iocw & MASK_IODBINARY)
                chars = 160;
        else
                chars = 80;

        acc.id = unit[unitdes][1].name;
        acc.MAIL = false;
	crx = cr + unit[unitdes][1].index;

        if (!crx->ready) {
notready:
                result |= MASK_IORNRDY | (acc.addr << SHFT_ADDR);
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

        if ((iocw & MASK_IODBINARY) && strlen(crx->cbuf) != (unsigned)chars) {
                printf("*\tERROR: binary card incorrect length(%u). abort\n", strlen(crx->cbuf));
                exit(0);
        }

        if (crx->cbuf[0] == '?' && !(iocw & MASK_IODBINARY))
                result |= MASK_IORD19;

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

        result |= (acc.addr << SHFT_ADDR);
retresult:
        return result;
}


