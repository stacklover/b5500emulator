/***********************************************************************
* b5500emulator
************************************************************************
* Copyright (c) 2017, Reinhard Meyer, DL5UY
* Licensed under the MIT License,
*       see LICENSE
************************************************************************
* b5500 operator console emulation (SPO)
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

#define NAMELEN 100

/*
 * the SPO
 */
char	filename[NAMELEN];
FILE	*fp = stdin;
BIT	ready;
char	spoinbuf[80];
char	*spoinp;
char	spooutbuf[80];
char	*spooutp;

/*
 * Initialize command from argv scanner or special SPO input
 */
int spo_init(const char *option) {
	const char *op = option;
	printf("spo option(s): %s\n", op);
	ready = true;
	return 0; // OK
}

/*
 * query ready status
 */
BIT spo_ready(unsigned index) {
        struct timeval tv = {0, 0};

	// initialize SPO if not ready
	if (!ready) spo_init("");

	// check for user input ready
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(0, &fds);
        if (select(1, &fds, NULL, NULL, &tv)) {
                spoinp = fgets(spoinbuf, sizeof spoinbuf, stdin);
		// any input ?
		if (spoinp != NULL) {
			// remove trailing control codes
			spoinp = spoinbuf + strlen(spoinbuf);
			while (spoinp >= spoinbuf && *spoinp <= ' ')
				*spoinp-- = 0;
			spoinp = spoinbuf;
			// divert input starting with '#' to our scanner
			if (*spoinp == '#') {
				handle_option(spoinp+1);
			} else {
				// signal input request
				CC->CCI05F = true;
				signalInterrupt("SPO", "INPUT REQUEST");
				// the input line is read later, once the IRQ is handled by the MCP
			}
		}
        }

	// finally return always ready
	return ready;
}

/*
 * write a single line
 */
WORD48 spo_write(WORD48 iocw) {
        int count;
        ACCESSOR acc;

        acc.id = "SPO";
        acc.MAIL = false;
        acc.addr = iocw & MASKMEM;

        spooutp = spooutbuf;

loop:   fetch(&acc);
        for (count=0; count<8; count++) {
                  if (spooutp >= spooutbuf + sizeof spooutbuf - 1)
                          goto done;
                  if (((acc.word >> 42) & 0x3f) == 037)
                          goto done;
                  *spooutp++ = translatetable_bic2ascii[(acc.word>>42) & 0x3f];
                  acc.word <<= 6;
        }
        acc.addr++;
        goto loop;

done:   *spooutp++ = 0;
        printf ("%s\n", spooutbuf);
        return (iocw & (MASK_IODUNIT | MASK_IODREAD)) | acc.addr;
}

/*
 * read a single line
 */
WORD48 spo_read(WORD48 iocw) {
        int count;
        ACCESSOR acc;
	BIT gmset = false;

        acc.id = "SPO";
        acc.MAIL = false;
        acc.addr = iocw & MASKMEM;
        spoinp = spoinbuf;

        // convert until EOL or any other control char found
        while (*spoinp >= ' ') {
                acc.word = 0ll;
                for (count=0; count<8; count++) {
                          acc.word <<= 6;
                          if (*spoinp >= ' ') {
                                  // printable char
                                  acc.word |= translatetable_ascii2bic[*spoinp++ & 0x7f];
                          } else {
                                  // EOL or other char, fill word with GM
                                  acc.word |= 037;
				  gmset = true;
                          }
                }
                // store the complete word
                store(&acc);
                acc.addr++;
        }
	// store one word with GM, if no GM was entered
	if (!gmset) {
		acc.word = 03737373737373737LL;
                store(&acc);
                acc.addr++;
	}
        return (iocw & (MASK_IODUNIT | MASK_IODREAD)) | acc.addr;
}


