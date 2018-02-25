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
* 2017-10-14  R.Meyer
*   changed file operations from fread/fwrite/fopen/fseek to
*   read/write/open/lseek
***********************************************************************/

#define COMPLAINABOUTNEVERWRITTEN 1

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

/***********************************************************************
* notes:
* a segment: 30 words of 48 bits or 8 BIC/BCL characters
* smallest entity: B475 Disk File Storage Module
*   4x Disks (8 sides) of 50 Tracks of 24000 characters = 9,600,000
*   characters or 40,000 segments
* next entity: B471 Disk File Electronics Unit
*   controls upto 5x B475 = 48,000,000 characters or 200,000 segments
* top entity: B5470 Disk File Control Unit
*   controls upto 10x B471 = 480,000,000 characters or 2,000,000 segments
* system max: 2x B5470 = 960,000,000 characters or 4,000,000 segments
*
* DKA and DKB therefore each provide access to 2,000,000 segments
*
* we convert each segment into 240 ASCII characters before storing it
* we emulate using 256 bytes per segment
* the maximum (at 10 ESU with 5 disks each) physical disk space required
* is 512,000,000 bytes (each for DKA and DKB)
*
* we use the remaining 16 bytes per segment for trapping access errors
* by placing the EU, DISKFILEADDR, CHECKSUM there (all in ASCII):
* 0123456789ABCDEF
* _ssss_e_dddddd_\n
* 
***********************************************************************/
#define DFCU_PER_SYSTEM	2
#define	DFEU_PER_DFCU	10
#define	DFSM_PER_DFEU	5

#define	SEGS_PER_DFSM	40000
#define	SEGS_PER_DFEU	(DFSM_PER_DFEU*SEGS_PER_DFSM) //   200,000
#define	SEGS_PER_DFCU	(DFEU_PER_DFCU*SEGS_PER_DFEU) // 2,000,000

#define NAMELEN 100
#define	DBUFLEN	260
#define	DATALEN 240

/***********************************************************************
* for each supported disk drive
***********************************************************************/
static struct dk {
	char	filename[NAMELEN];
	int	df;	// we do not use stdio here AND we assume we will never get 0 as the handle
	BIT	ready;
	BIT	readcheck;
	BIT	rwtrace;
	unsigned eus;
	char	dbuf[DBUFLEN];
	char	*dbufp;
} dk[DFCU_PER_SYSTEM];

/***********************************************************************
* optional file to write debugging traces into
***********************************************************************/
static FILE *trace = NULL;

/***********************************************************************
* currently selected unit or NULL in command interpreter
***********************************************************************/
static struct dk *dkx = NULL;

/***********************************************************************
* set to dka/dkb
***********************************************************************/
static int set_dk(const char *v, void *data) {dkx = dk+(int)data; return 0; }

/***********************************************************************
* specify or close the trace file
***********************************************************************/
static int set_dktrace(const char *v, void *) {
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
* specify rwtrace on or off
***********************************************************************/
static int set_dkrwtrace(const char *v, void *) {
	if (!dkx) {
		printf("dk not specified\n");
		return 2; // FATAL
	}
	if (strcmp(v, "on") == 0)
		dkx->rwtrace = true;
	else if (strcmp(v, "off") == 0)
		dkx->rwtrace = false;
	else {
		printf("on or off required\n");
		return 2; // FATAL
	}
	return 0; // OK
}

/***********************************************************************
* set number of simulated eus for a DFCU
***********************************************************************/
static int set_dkeus(const char *v, void *) {
	char *p;
	if (!dkx) {
		printf("dk not specified\n");
		return 2; // FATAL
	}
	dkx->eus = strtoul(v, &p, 10);
	if (*p || dkx->eus > 10) {
		printf("non numeric or illegal data\n");
		return 2; // FATAL
	}
	return 0; // OK
}

/***********************************************************************
* specify or close the file for emulation
***********************************************************************/
static int set_dkfile(const char *v, void *) {
	if (!dkx) {
		printf("dk not specified\n");
		return 2; // FATAL
	}
	strncpy(dkx->filename, v, NAMELEN);
	dkx->filename[NAMELEN-1] = 0;

	// if we are ready, close current file
	if (dkx->ready) {
		close(dkx->df);
		dkx->df = 0;
		dkx->ready = false;
	}

	// reset flags
	dkx->readcheck = false;

	// now open the new file, if any name was given
	// if none given, the drive just stays unready
	if (dkx->filename[0]) {
		dkx->df = open(dkx->filename, O_RDWR);
		if (dkx->df > 0) {
			dkx->ready = true;
			return 0; // OK
		} else {
			// cannot open
			perror(dkx->filename);
			return 2; // FATAL
		}
	}
	return 0; // OK
}

/***********************************************************************
* command table
***********************************************************************/
static const command_t dk_commands[] = {
	{"dka",		set_dk,	(void *) 0},
	{"dkb", 	set_dk, (void *) 1},
	{"rwtrace",	set_dkrwtrace},
	{"trace",	set_dktrace},
	{"eus",		set_dkeus},
	{"file",	set_dkfile},
	{NULL,		NULL},
};

/***********************************************************************
* Initialize command from argv scanner or special SPO input
***********************************************************************/
int dk_init(const char *option) {
	dkx = NULL; // require specification of a drive
	return command_parser(dk_commands, option);
}

/***********************************************************************
* query ready status
***********************************************************************/
BIT dk_ready(unsigned index) {
	if (index < DFCU_PER_SYSTEM)
		return dk[index].ready;
	return false;
}

/***********************************************************************
* read or write, check or inquire
***********************************************************************/
WORD48 dk_access(WORD48 iocw) {
	unsigned unitdes, count, segcnt, words;
	// prepare result with unit and read flag
	WORD48 result = iocw & (MASK_IODUNIT | MASK_IODREAD);
	ACCESSOR acc;
	int i, j, k, retry;
	unsigned eu = 0, diskfileaddr = 0;
	off_t seekval;
	ssize_t cnt;
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
		result |= MASK_IORNRDY;
		goto retresult;
	}

	// fetch first word from core with disk address
	fetch(&acc);
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
	INCADDR(acc.addr);

	if (trace) {
		fprintf(trace, "%08u UNIT=%u IOCW=%016llo, EU:DFA=%u:%06u", instr_count, unitdes, iocw, eu, diskfileaddr);
	}

	// legal access?
	if (eu >= dkx->eus || diskfileaddr >= SEGS_PER_DFEU) {
		// not supported
		if (trace)
			fprintf(trace, " NOT SUPPORTED\n");
		if (dkx->rwtrace)
			putchar('?');
		result |= MASK_IORD21;
		goto retresult;
	}

	// special case when memory inhibit
	if (iocw & MASK_IODMI) {
		dkx->readcheck = true;
		if (trace)
			fprintf(trace, " READ CHECK SEGMENTS=%02u\n", segcnt);
		if (dkx->rwtrace)
			putchar('c');
		goto retresult;
	}

	// special case when words=0
	if (words == 0) {
		if (trace)
			fprintf(trace, " INTERROGATE\n");
		if (dkx->rwtrace)
			putchar('i');
		goto retresult;
	}

	// regular read
	if (iocw & MASK_IODREAD) {
		if (trace)
			fprintf(trace, " READ WORDS=%02u\n", words);
		if (dkx->rwtrace)
			putchar('r');

		// read until word count exhausted
		while (words > 0) {
			unsigned sum;
			unsigned xsum;
			unsigned xeu, xdiskfileaddr;
			BIT check = true;

			// get the physical record
			seekval = (eu*SEGS_PER_DFEU+diskfileaddr)*256;
			// on read problems retry...
			retry = 0;
			readagain:
			if (lseek(dkx->df, seekval, SEEK_SET) != seekval) {
				printf("*** DISKIO READ SEEK ERROR %d DFA=%u:%06u ***\n", errno, eu, diskfileaddr);
				// report not ready
				result |= MASK_IORNRDY;
				goto retresult;
			}
			cnt = read(dkx->df, dkx->dbuf, 256);
			if (cnt == 0) {
				// read past current end
#if COMPLAINABOUTNEVERWRITTEN
				printf("*** DISKIO READ PAST EOF DFA=%u:%06u ***\n", eu, diskfileaddr);
#endif
				goto pasteof;
			}
			if (cnt != 256) {
				printf("*** DISKIO READ ERROR %d DFA=%u:%06u RETRYING... ***\n", errno, eu, diskfileaddr);
				++retry;
				if (retry < 10)
					goto readagain;
				// report not ready
				result |= MASK_IORNRDY;
				goto retresult;
			}
			// put and end of string
			dkx->dbuf[256] = 0;

			// if the signature is missing or wrong, this record has never been written
			dkx->dbufp = dkx->dbuf + DATALEN;
			if (sscanf(dkx->dbufp, "_%04x_%01u_%06u_\n", &xsum, &xeu, &xdiskfileaddr) != 3) {
#if COMPLAINABOUTNEVERWRITTEN
				printf("*** DISKIO READ OF RECORD NEVER WRITTEN DFA=%u:%06u ***\n", eu, diskfileaddr);
				//printf("Segment:'%s'\n", dkx->dbuf);
#endif
		pasteof:
				// return a '0' filled segment
				memset(dkx->dbuf, '0', 256);
				dkx->dbuf[255] = '\n';
				check = false;
			}
			// set pointer back to segment data
			dkx->dbufp = dkx->dbuf;
			sum = 0;

			// always handle chunks of 30 words
			for (i=0; i<3; i++) {
				if (trace)
					fprintf(trace, "\t%05o %u:%06u", acc.addr, eu, diskfileaddr);
				for (j=0; j<10; j++) {
					if (trace)
						fprintf(trace, " %-8.8s", dkx->dbufp);
					// store until word count exhausted
					if (words > 0) {
						acc.word = 0LL;
						for (k=0; k<8; k++) {
							acc.word = (acc.word << 6) | translatetable_ascii2bic[*dkx->dbufp & 0x7f];
							sum += *dkx->dbufp;
							dkx->dbufp++;
						}
						store(&acc);
						INCADDR(acc.addr);
						words--;
					} else {
						for (k=0; k<8; k++) {
							sum += *dkx->dbufp;
							dkx->dbufp++;
						}
					}
				}
				if (trace)
					fprintf(trace, "\n");
			} // chunk of 30 words

			// sanity check - should never fail
			if (dkx->dbufp != dkx->dbuf+DATALEN) {
				printf("*** DISKIO READ SANITY CHECK(1) FAILED ***\n");
				exit(2);
			}

			if (check) {
				// check checksum
				if (sum != xsum) {
					printf("*** DISKIO READ CHECKSUM MISMATCH ***\n");
				}

				// check eu:diskfileaddr
				if (eu != xeu || diskfileaddr != xdiskfileaddr) {
					printf("*** DISKIO READ EU:DISKFILEADDR MISMATCH ***\n");
				}
			}

			// next record address
			diskfileaddr++;
		} // while words
		goto retresult;
	}

	// what remains: regular write
	{
		if (trace)
			fprintf(trace, " WRITE WORDS=%02u\n", words);
		if (dkx->rwtrace)
			putchar('w');

		// keep writing records until word count is exhausted
		while (words > 0) {
			// prepare buffer pointer and checksum
			unsigned sum = 0;
			dkx->dbufp = dkx->dbuf;
			// always handle chunks of 30 words
			for (i=0; i<3; i++) {
				if (trace)
					fprintf(trace, "\t%05o %u:%06u", acc.addr, eu, diskfileaddr);
				for (j=0; j<10; j++) {
					if (words > 0) {
						// if word count NOT exhausted, write next word
						fetch(&acc);
						INCADDR(acc.addr);
						for (k=0; k<8; k++) {
							unsigned ch = translatetable_bic2ascii[acc.word & 0x3f];
							dkx->dbufp[7-k] = ch;
							acc.word >>= 6;
							sum += ch;
						}
						words--;
					} else {
						// if word count exhausted, write zeros
						for (k=0; k<8; k++) {
							dkx->dbufp[7-k] = '0';
							sum += '0';
						}
					}
					dkx->dbufp += 8;
					if (trace)
						fprintf(trace, " %-8.8s", dkx->dbufp-8);
				}
				if (trace)
					fprintf(trace, "\n");
			} // chunk of 30 words

			// sanity check - should never fail
			if (dkx->dbufp != dkx->dbuf+DATALEN) {
				printf("*** DISKIO WRITE SANITY CHECK(1) FAILED ***\n");
				exit(2);
			}

			// write header
			dkx->dbufp = dkx->dbuf + DATALEN;
			dkx->dbufp += sprintf(dkx->dbufp, "_%04x_%01u_%06u_\n",
				sum, eu, diskfileaddr);

			// sanity check - should never fail
			if (dkx->dbufp != dkx->dbuf+256) {
				printf("*** DISKIO WRITE SANITY CHECK(2) FAILED ***\n");
				exit(2);
			}

			// write record to physical file
			seekval = (eu*SEGS_PER_DFEU+diskfileaddr)*256;
			retry = 0;
			writeagain:
			if (lseek(dkx->df, seekval, SEEK_SET) != seekval) {
				printf("*** DISKIO WRITE SEEK ERROR %d DFA=%u:%06u ***\n", errno, eu, diskfileaddr);
				// report not ready
				result |= MASK_IORNRDY;
				goto retresult;
			}
			if (write(dkx->df, dkx->dbuf, 256) != 256) {
				printf("*** DISKIO WRITE ERROR %d DFA=%u:%06u RETRYING... ***\n", errno, eu, diskfileaddr);
				++retry;
				if (retry < 10)
					goto writeagain;
				// report not ready
				result |= MASK_IORNRDY;
				goto retresult;
			}

			// next record address
			diskfileaddr++;
		} // while words
		goto retresult;
	}

retresult:
	result |= (acc.addr << SHFT_ADDR) & MASK_ADDR;
	if (iocw & MASK_IODUSEWC)
		result |= ((WORD48)words << SHFT_WCNT) & MASK_WCNT;

	if (trace)
		fflush(trace);
	if (dkx->rwtrace)
		fflush(stdout);

	// trace/debug when any error has been set
	if (result & 037700000LL) {
		printf("DK "); print_iocw(stdout, iocw);
		printf(" EU:DFA=%u:%06u ", eu, diskfileaddr);
		print_ior(stdout, result);
		printf("\n");
	}

	return result;
}


