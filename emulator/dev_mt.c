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

#define TAPES 16
#define NAMELEN 100
#define	TBUFLEN	164

/*
 * for each supported tape drive
 */
struct mt {
	char	filename[NAMELEN];
	FILE	*fp;
	BIT	ready;
	char	tbuf[TBUFLEN];
	char	*tbufp;
        int	line, lastpos;
	BIT	eof;
} mt[TAPES];

/*
 * optional open file to write debugging traces into
 */
static FILE *trace = NULL;


/*
 * Initialize command from argv scanner or special SPO input
 */
int mt_init(const char *option) {
	struct mt *mtx = mt;
	const char *op = option;
	printf("magnetic tape option(s): %s\n", op);
	while (*op != 0) {
		if (strncmp(op, "mta:", 4) == 0) {
			mtx = mt+0;
			op += 4;
		} else if (strncmp(op, "mtb:", 4) == 0) {
			mtx = mt+1;
			op += 4;
		} else if (strncmp(op, "mtc:", 4) == 0) {
			mtx = mt+2;
			op += 4;
		} else if (strncmp(op, "mtd:", 4) == 0) {
			mtx = mt+3;
			op += 4;
		} else if (strncmp(op, "mte:", 4) == 0) {
			mtx = mt+4;
			op += 4;
		} else if (strncmp(op, "mtf:", 4) == 0) {
			mtx = mt+5;
			op += 4;
		} else if (strncmp(op, "mth:", 4) == 0) {
			mtx = mt+6;
			op += 4;
		} else if (strncmp(op, "mtj:", 4) == 0) {
			mtx = mt+7;
			op += 4;
		} else if (strncmp(op, "mtk:", 4) == 0) {
			mtx = mt+8;
			op += 4;
		} else if (strncmp(op, "mtl:", 4) == 0) {
			mtx = mt+9;
			op += 4;
		} else if (strncmp(op, "mtm:", 4) == 0) {
			mtx = mt+10;
			op += 4;
		} else if (strncmp(op, "mtn:", 4) == 0) {
			mtx = mt+11;
			op += 4;
		} else if (strncmp(op, "mtp:", 4) == 0) {
			mtx = mt+12;
			op += 4;
		} else if (strncmp(op, "mtr:", 4) == 0) {
			mtx = mt+13;
			op += 4;
		} else if (strncmp(op, "mts:", 4) == 0) {
			mtx = mt+14;
			op += 4;
		} else if (strncmp(op, "mtt:", 4) == 0) {
			mtx = mt+15;
			op += 4;
		} else {
			// assume rest is a filename
			strncpy(mtx->filename, op, NAMELEN);
			mtx->filename[NAMELEN-1] = 0;
			mtx->fp = fopen(mtx->filename, "r");
			if (mtx->fp) {
				mtx->line = 0;
				mtx->lastpos = -1;
				mtx->eof = false;
				mtx->ready = true;
				break;
			} else {
				// cannot open
				perror(mtx->filename);
				return 2; // fatal
			}
		}
	}
	return 0; // OK
}

/*
 * query ready status
 */
BIT mt_ready(unsigned index) {
	if (index < TAPES)
		return mt[index].ready;
	return false;
}

/*
 * read or write a tape record or rewind
 */
WORD48 mt_access(WORD48 iocw) {
        unsigned unitdes, count, words;
        BIT mi, binary, tapedir, usewc, read;
        // prepare result with unit and read flag and MOD III DESC bit
        WORD48 result = (iocw & (MASK_IODUNIT | MASK_IODREAD)) | MASK_IORISMOD3;
        ACCESSOR acc;
        int i, cp=42;
        BIT first;
        WORD48 w;
        int lastchar;
        int lp;
	struct mt *mtx;

        unitdes = (iocw & MASK_IODUNIT) >> SHFT_IODUNIT;
        count = (iocw & MASK_WCNT) >> SHFT_WCNT;
        mi = (iocw & MASK_IODMI) ? true : false;
        binary = (iocw & MASK_IODBINARY) ? true : false;
        tapedir = (iocw & MASK_IODTAPEDIR) ? true : false;
        usewc = (iocw & MASK_IODUSEWC) ? true : false;
        read = (iocw & MASK_IODREAD) ? true : false;
        acc.addr = (iocw & MASK_ADDR) >> SHFT_ADDR;
        // number of words to do
        words = (usewc) ? count : 1024;

        acc.id = unit[unitdes][0].name;
        acc.MAIL = false;
	mtx = mt + unit[unitdes][0].index;

        if (!mtx->ready) {
                result |= MASK_IORNRDY | (acc.addr << SHFT_ADDR);
                goto retresult;
        }

        if (trace) {
                fprintf(trace, "%08u %s IOCW=%016llo WC=%u MI=%u BIN=%u REV=%u USEWC=%u READ=%u CORE=%05o",
                instr_count, unit[unitdes][read].name, iocw,
                count, mi, binary, tapedir, usewc, read, acc.addr);
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
                result |= MASK_IORD20;
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
                                result |= (acc.addr << SHFT_ADDR) | MASK_IOREOT;
                                goto retresult;
                        }
                        // check for record end
                        if (i & 0x80) {
                                // record marker
                                if (lastchar == 0x8f) {
                                        // tape mark
                                        if (trace)
                                                fprintf(trace, "' MARK\n");
                                        result |= (acc.addr << SHFT_ADDR) | MASK_IORD21;
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
                                        result |= (acc.addr << SHFT_ADDR) | MASK_IORD20;
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
                result |= MASK_IORD20 | MASK_IORMAE;
                goto retresult;
        }
        // what's left...
        printf("*\ttape operation not implemented %016llo\n", iocw);
        // return good result
        if (trace)
                fprintf(trace, " tape operation not implemented\n");

retresult:
        if (iocw & MASK_IODUSEWC)
                result |= (WORD48)words << SHFT_WCNT;

        result |= (acc.addr << SHFT_ADDR) | ((WORD48)((42-cp)/6) << SHFT_IORCHARS);

        if (trace) {
                fprintf(trace, "\tIO RESULT=%016llo\n\n", result);
                fflush(trace);
        }

        return result;
}


