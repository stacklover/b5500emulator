/***********************************************************************
* b5500emulator
************************************************************************
* Copyright (c) 2017, Reinhard Meyer, DL5UY
* Licensed under the MIT License,
*       see LICENSE
************************************************************************
* b5500 assembler
************************************************************************
* 2017-09-08  R.Meyer
*   Started from b5500_asm.c
* 2017-09-30  R.Meyer
*   overhaul of file names
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

#define MAXLINELENGTH   (264)   /* maximum line length for all devices - must be multiple of 8 */

/* debug flags: turn these on for various dumps and traces */
int dodmpins     = false;       /* dump instructions after assembly */
int dotrcmem     = false;       /* trace memory accesses */
int dolistsource = false;       /* list source line */
int dotrcins     = false;       /* trace instruction execution */
int realspo      = false;       /* print to real SPO */
int cardload     = false;       /* card load select switch */

// never set
int dotrcmat     = false;       /* trace math operations */
int emode        = false;       /* emode math */

/* variables for file access */
typedef struct filehandle {
        FILE    *fp;            /* handle */
        FILE    *trace;         /* handle for trace */
        char    *name;          /* name */
        char    *tracename;     /* trace file name */
        int     line;           /* line number */
        BIT     eof;            /* EOF encountered */
} FILEHANDLE;

FILEHANDLE    cardfile;         /* file with cards */
FILEHANDLE    tapefile;         /* file with tape */
FILEHANDLE    diskfile;         /* file with disk */
FILEHANDLE    listfile;         /* file with listing */
FILEHANDLE    spiofile;         /* file with special instruction and I/O trace */

/* input/output buffer */
char    linebuf[MAXLINELENGTH];
char    *linep;

/* file for CANbus */
int     canfile = -1;

/* NAME entries */
#define MAXNAME 1000
char name[MAXNAME][29];

/* instruction execution counter */
unsigned instr_count;

CPU *cpu;


WORD48 unitsready;

/*
 * table of I/O units
 * indexed by unit designator and read bit of I/O descriptor
 */
UNIT unit[32][2] = {
        /*00*/ {{NULL, 0},      {NULL, 0}},
        /*01*/ {{"MTA", 47-47}, {"MTA", 47-47}},
        /*02*/ {{NULL, 0},      {NULL, 0}},
        /*03*/ {{"MTB", 47-46}, {"MTB", 47-46}},
        /*04*/ {{"DRA", 47-31}, {"DRA", 47-31}},
        /*05*/ {{"MTC", 47-45}, {"MTC", 47-45}},
        /*06*/ {{"DKA", 47-29}, {"DKA", 47-29}},
        /*07*/ {{"MTD", 47-44}, {"MTD", 47-44}},
        /*08*/ {{"DRB", 47-30}, {"DRB", 47-30}},
        /*09*/ {{"MTE", 47-43}, {"MTE", 47-43}},
        /*10*/ {{"CPA", 47-25}, {"CRA", 47-24}},
        /*11*/ {{"MTF", 47-42}, {"MTF", 47-42}},
        /*12*/ {{"DKB", 47-28}, {"DKB", 47-28}},
        /*13*/ {{"MTH", 47-41}, {"MTH", 47-41}},
        /*14*/ {{NULL, 0},      {"CRB", 47-23}},
        /*15*/ {{"MTJ", 47-40}, {"MTJ", 47-40}},
        /*16*/ {{"DCC", 47-17}, {"DCC", 47-17}},
        /*17*/ {{"MTK", 47-39}, {"MTK", 47-39}},
        /*18*/ {{"PPA", 47-21}, {"PRA", 47-20}},
        /*19*/ {{"MTL", 47-38}, {"MTL", 47-38}},
        /*20*/ {{"PPA", 47-19}, {"PRA", 47-18}},
        /*21*/ {{"MTM", 47-37}, {"MTM", 47-37}},
        /*22*/ {{"LPA", 47-27}, {NULL, 0}},
        /*23*/ {{"MTN", 47-36}, {"MTN", 47-36}},
        /*24*/ {{NULL, 0},      {NULL, 0}},
        /*25*/ {{"MTP", 47-35}, {"MTP", 47-35}},
        /*26*/ {{"LPB", 47-27}, {NULL, 0}},
        /*27*/ {{"MTR", 47-34}, {"MTR", 47-34}},
        /*28*/ {{NULL, 0},      {NULL, 0}},
        /*29*/ {{"MTS", 47-33}, {"MTS", 47-33}},
        /*30*/ {{"SPO", 47-22}, {"SPO", 47-22}},
        /*31*/ {{"MTT", 47-32}, {"MTT", 47-32}},
};

/*
 * table of IRQs
 * indexed by cell address - 020
 */
IRQ irq[48] = {
        /*20*/ {"NA20"},
        /*21*/ {"NA21"},
        /*22*/ {"TIMER"},
        /*23*/ {"I/O BUSY"},
        /*24*/ {"KBD REQ"},
        /*25*/ {"PTR1 FIN"},
        /*26*/ {"PTR2 FIN"},
        /*27*/ {"I/O1 FIN"},
        /*30*/ {"I/O2 FIN"},
        /*31*/ {"I/O3 FIN"},
        /*32*/ {"I/O4 FIN"},
        /*33*/ {"P2 BUSY"},
        /*34*/ {"DCT IRQ"},
        /*35*/ {"NA35"},
        /*36*/ {"DF1 RCHK FIN"},
        /*37*/ {"DF2 RCHK FIN"},
        /*40*/ {"P2 MPE"},
        /*41*/ {"P2 INVA"},
        /*42*/ {"P2 SOVF"},
        /*43*/ {"NA43"},
        /*44*/ {"P2 COM"},
        /*45*/ {"P2 PRL"},
        /*46*/ {"P2 CONT"},
        /*47*/ {"P2 PBIT"},
        /*50*/ {"P2 FLAG"},
        /*51*/ {"P2 INVIDX"},
        /*52*/ {"P2 EXPO"},
        /*53*/ {"P2 EXPU"},
        /*54*/ {"P2 INTO"},
        /*55*/ {"P2 DIV0"},
        /*56*/ {"NA56"},
        /*57*/ {"NA57"},
        /*60*/ {"P1 MPE"},
        /*61*/ {"P1 INVA"},
        /*62*/ {"P1 SOVF"},
        /*63*/ {"NA63"},
        /*64*/ {"P1 COM"},
        /*65*/ {"P1 PRL"},
        /*66*/ {"P1 CONT"},
        /*67*/ {"P1 PBIT"},
        /*70*/ {"P1 FLAG"},
        /*71*/ {"P1 INVIDX"},
        /*72*/ {"P1 EXPO"},
        /*73*/ {"P1 EXPU"},
        /*74*/ {"P1 INTO"},
        /*75*/ {"P1 DIV0"},
        /*76*/ {"NA76"},
        /*77*/ {"NA77"},
};

int openfile(FILEHANDLE *f, const char *mode) {
        if (f->name != NULL) {
                f->fp = fopen(f->name, mode);
                if (f->fp == NULL) {
                        perror(f->name);
                        return 0;
                }
                f->eof = false;
                f->line = 0;
        } else {
                f->fp = NULL;
                f->eof = true;
                f->line = 0;
        }

        if (f->tracename != NULL) {
                f->trace = fopen(f->tracename, "w");
                if (f->trace == NULL) {
                        perror(f->tracename);
                        return 0;
                }
        } else
                f->trace = NULL;
        return f->fp ? 1 : 0;
}

int closefile(FILEHANDLE *f) {
        if (f->fp != NULL)
                fclose(f->fp);
        if (f->trace != NULL) {
                fprintf(f->trace, "\n***** END of TRACE *****\n");
                fclose(f->trace);
        }
        f->fp = NULL;
        f->trace = NULL;
        f->eof = true;
        return 0;
}

void errorl(const char *msg) {
        fputs(linebuf, stdout);
        printf("\n*** Card read error at line %d: %s\n", cardfile.line, msg);
        fclose(cardfile.fp);
        exit(2);
}

void signalInterrupt(const char *id, const char *cause) {
        // Called by all modules to signal that an interrupt has occurred and
        // to invoke the interrupt prioritization mechanism. This will result in
        // an updated vector address in the IAR. Can also be called to reprioritize
        // any remaining interrupts after an interrupt is handled. If no interrupt
        // condition exists, CC->IAR is set to zero
        if (P[0]->r.I & 0x01) CC->IAR = 060; // P1 memory parity error
        else if (P[0]->r.I & 0x02) CC->IAR = 061; // P1 invalid address error

        else if (CC->CCI03F) CC->IAR = 022; // Time interval
        else if (CC->CCI04F) CC->IAR = 023; // I/O busy
        else if (CC->CCI05F) CC->IAR = 024; // Keyboard request
        else if (CC->CCI08F) CC->IAR = 027; // I/O 1 finished
        else if (CC->CCI09F) CC->IAR = 030; // I/O 2 finished
        else if (CC->CCI10F) CC->IAR = 031; // I/O 3 finished
        else if (CC->CCI11F) CC->IAR = 032; // I/O 4 finished
        else if (CC->CCI06F) CC->IAR = 025; // Printer 1 finished
        else if (CC->CCI07F) CC->IAR = 026; // Printer 2 finished
        else if (CC->CCI12F) CC->IAR = 033; // P2 busy
        else if (CC->CCI13F) CC->IAR = 034; // Inquiry request
        else if (CC->CCI14F) CC->IAR = 035; // Special interrupt 1
        else if (CC->CCI15F) CC->IAR = 036; // Disk file 1 read check finished
        else if (CC->CCI16F) CC->IAR = 037; // Disk file 2 read check finished

        else if (P[0]->r.I & 0x04) CC->IAR = 062; // P1 stack overflow
        else if (P[0]->r.I & 0xF0) CC->IAR = (P[0]->r.I >> 4) + 060; // P1 syllable-dependent

        else if (P[1]->r.I & 0x01) CC->IAR = 040; // P2 memory parity error
        else if (P[1]->r.I & 0x02) CC->IAR = 041; // P2 invalid address error
        else if (P[1]->r.I & 0x04) CC->IAR = 042; // P2 stack overflow
        else if (P[1]->r.I & 0xF0) CC->IAR = (P[1]->r.I >> 4) + 040; // P2 syllable-dependent
        else CC->IAR = 0; // no interrupt set

        if (CC->IAR) {
                CC->interruptMask |= (1ll << CC->IAR);
                CC->interruptLatch |= (1ll << CC->IAR);
        }
#if 1
        if (spiofile.trace) {
                fprintf(spiofile.trace, "%08u %s signalInterrupt %s P1.I=%02x P2.I=%02x MASK=%012llx LATCH=%012llx\n",
                        instr_count, id, cause,
                        P[0]->r.I, P[1]->r.I,
                        CC->interruptMask,
                        CC->interruptLatch);
        }
#endif
}

void getlin(FILEHANDLE *f) { /* get next line */
        if (f->eof)
                return;
        f->line++;
        if (fgets(linebuf, sizeof linebuf, f->fp) == NULL) {
                f->eof = true;
                linebuf[0] = 0;
                return;
        }
        // remove trailing control codes
        linep = linebuf + strlen(linebuf);
        while (linep >= linebuf && *linep <= ' ')
                *linep-- = 0;
        linep = linebuf;
}

WORD48 getword(WORD6 def) {
        WORD48  res = 0;
        int i;
        for (i=0; i<8; i++) {
                if (*linep >= ' ') {
                        res = (res << 6) | translatetable_ascii2bic[*linep & 0x7f];
                        linep++;
                } else {
                        res = (res << 6) | def; // blank
                }
        }
        return res;
}

void putword(WORD48 w) {
        int i;
        for (i=7; i>=0; i--) {
                *linep++ = translatetable_bic2ascii[(w >> (6*i)) & 0x3f];
        }
}

void fileheaderanalyze(FILEHANDLE *f, WORD48 *hdr) {
        int i;
        if (!f->trace) return;
        fprintf(f->trace, "\tFH[00]=%016llo RECLEN=%llu BLKLEN=%llu RECSPERBLK=%llu SEGSPERBLK=%llu\n",
                hdr[0], (hdr[0]>>33)&077777, (hdr[0]>>18)&077777, (hdr[0]>>6)&07777, hdr[0]&077);
        fprintf(f->trace, "\tFH[01]=%016llo DATE=%llu TIME=%llu\n",
                hdr[1], (hdr[1]>>24)&0777777, hdr[1]&037777777);
        fprintf(f->trace, "\tFH[07]=%016llo RECORDS=%llu\n",
                hdr[7], hdr[7]);
        fprintf(f->trace, "\tFH[08]=%016llo SEGSPERROW=%llu\n",
                hdr[8], hdr[8]);
        fprintf(f->trace, "\tFH[09]=%016llo MAXROWS=%llu\n",
                hdr[9], hdr[9]&037);
        for (i=10; i<29; i++) {
                if (hdr[i] > 0) {
                        fprintf(f->trace, "\tFH[%02d]=%016llo DFA=%llu\n", i, hdr[i], hdr[i]);
                }
        }
}

WORD48 card_read(WORD48 iocw) {
        WORD48 result = iocw & (MASK_IODUNIT | MASK_IODREAD);
        ACCESSOR acc;
        int chars;

        acc.id = "CRA";
        acc.MAIL = false;
        acc.addr = iocw & MASK_ADDR;
        if (cardfile.fp == NULL) {
notready:       unitsready &= ~(1ll << unit[10][1].readybit);
                return result | MASK_IORNRDY | (acc.addr << SHFT_ADDR);
        }
        if (iocw & MASK_IODMI)
                return result | (acc.addr << SHFT_ADDR);
        if (iocw & MASK_IODBINARY)
                chars = 160;
        else
                chars = 80;

        getlin(&cardfile);
        if (cardfile.eof) {
                goto notready;
                //return result | MASK_IORD21 | (acc.addr << SHFT_ADDR);
        }
        if (cardfile.trace) {
                fprintf(cardfile.trace, "%08u %s -> %05o\n", instr_count, linebuf, acc.addr);
                fflush(cardfile.trace);
        }
        if (dolistsource)
                printf("*\tCRA: %s\n", linebuf);
        if ((iocw & MASK_IODBINARY) && strlen(linebuf) != (unsigned)chars) {
                printf("*\tERROR: binary card incorrect length(%u). abort\n", strlen(linebuf));
                exit(0);
        }
        if (linebuf[0] == '?' && !(iocw & MASK_IODBINARY))
                result |= MASK_IORD19;
        while (chars > 0) {
                acc.word = getword(060);
                chars -= 8;
                store(&acc);
                acc.addr++;
        }
        return result | (acc.addr << SHFT_ADDR);
}

WORD48 spo_write(WORD48 iocw) {
        int count;
        ACCESSOR acc;

        acc.id = "SPO";
        acc.MAIL = false;
        acc.addr = iocw & MASKMEM;

        linep = linebuf;
        *linep++ = '!';

loop:   fetch(&acc);
        for (count=0; count<8; count++) {
                  if (linep >= linebuf + sizeof linebuf - 1)
                          goto done;
                  if (((acc.word >> 42) & 0x3f) == 037)
                          goto done;
                  *linep++ = translatetable_bic2ascii[(acc.word>>42) & 0x3f];
                  acc.word <<= 6;
        }
        acc.addr++;
        goto loop;

done:   *linep++ = 0;
        printf ("SPO: %s\n", linebuf+1);
        // also write this to all open trace files
        if (cardfile.trace) {
                fprintf(cardfile.trace, "*** SPO: %s\n", linebuf+1);
                fflush(cardfile.trace);
        }
        if (tapefile.trace) {
                fprintf(tapefile.trace, "*** SPO: %s\n", linebuf+1);
                fflush(tapefile.trace);
        }
        if (diskfile.trace) {
                fprintf(diskfile.trace, "*** SPO: %s\n", linebuf+1);
                fflush(diskfile.trace);
        }
        if (spiofile.trace) {
                fprintf(spiofile.trace, "*** SPO: %s\n", linebuf+1);
                fflush(spiofile.trace);
        }
        // print to REAL SPO
        if (realspo) {
                if (canfile >= 0) {
                        linep--; *linep++ = 0x0d; *linep++ = 0;
                        (void)write(canfile, linebuf, linep-linebuf);
                        sleep(3);
                }
        }
        return (iocw & (MASK_IODUNIT | MASK_IODREAD)) | acc.addr;
}

char spoinbuf[80];
char *spoinp;

WORD48 spo_read(WORD48 iocw) {
        int count;
        ACCESSOR acc;

        acc.id = "SPO";
        acc.MAIL = false;
        acc.addr = iocw & MASKMEM;
        spoinp = spoinbuf;

        // convert until EOL or any other control char found
        // checking at end of loop ensures at least one word with GM is written to memory
        do {
                acc.word = 0ll;
                for (count=0; count<8; count++) {
                          acc.word <<= 6;
                          if (*spoinp >= ' ') {
                                  // printable char
                                  acc.word |= translatetable_ascii2bic[*spoinp++ & 0x7f];
                          } else {
                                  // EOL or other char, fill word with GM
                                  acc.word |= 037;
                          }
                }
                // store the complete word
                store(&acc);
                acc.addr++;
        } while (*spoinp >= ' ');
        return (iocw & (MASK_IODUNIT | MASK_IODREAD)) | acc.addr;
}

void spo_test(void) {
        // check, if a user has entered a line on stdin
        struct timeval tv = {0, 0};
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(0, &fds);
        if (select(1, &fds, NULL, NULL, &tv)) {
                spoinp = fgets(spoinbuf, sizeof spoinbuf, stdin);
                // signal input request
                CC->CCI05F = true;
                signalInterrupt("SPO", "INPUT REQUEST");
                // the input line is read later, once the IRQ is handled by the MCP
        }
}

BIT readcheck = false;
WORD48 disk_access(WORD48 iocw, int eu, int diskfileaddr) {
        unsigned unit, count, segcnt, words;
        ADDR15 core;
        // prepare result with unit and read flag
        WORD48 result = iocw & (MASK_IODUNIT | MASK_IODREAD);
        ACCESSOR acc;
        int i, j;
        ADDR15 startaddr;

        acc.id = "DKA";
        acc.MAIL = false;
        unit = (iocw & MASK_IODUNIT) >> SHFT_IODUNIT;
        count = (iocw & MASK_WCNT) >> SHFT_WCNT;
        segcnt = (iocw & MASK_IODSEGCNT) >> SHFT_IODSEGCNT;
        core = (iocw & MASK_ADDR) >> SHFT_ADDR;
        // number of words to do
        words = (iocw & MASK_IODUSEWC) ? count : segcnt * 30;

        core++;

        if (diskfile.trace) {
                fprintf(diskfile.trace, "%08u UNIT=%u IOCW=%016llo, EU:DFA=%d:%06d", instr_count, unit, iocw, eu, diskfileaddr);
        }

        if (unit != 6 || diskfile.fp == NULL ||
                eu < 0 || eu > 9 ||
                diskfileaddr < 0 || diskfileaddr > 199999) {
                // not supported
                if (diskfile.trace)
                        fprintf(diskfile.trace, " NOT SUPPORTED\n");
                result |= MASK_IORNRDY;
        } else if (iocw & MASK_IODMI) {
                // special case when memory inhibit
                readcheck = true;
                if (diskfile.trace)
                        fprintf(diskfile.trace, " READ CHECK SEGMENTS=%02u\n", segcnt);
        } else if (words == 0) {
                // special case when words=0
                if (diskfile.trace)
                        fprintf(diskfile.trace, " INTERROGATE\n");
        } else if (iocw & MASK_IODREAD) {
                // regular read
                if (diskfile.trace)
                        fprintf(diskfile.trace, " READ WORDS=%02u\n", words);
                diskfile.eof = false;

                // read until word count exhausted
                while (words > 0) {
                        // get the physical record
                        fseek(diskfile.fp, (eu*200000+diskfileaddr)*256, SEEK_SET);
                        linep = linebuf;
                        // on read problems or if the signature is missing or wrong, this record has never been written
                        if (fread(linebuf, sizeof(char), 256, diskfile.fp) != 256 ||
                                  strncmp(linebuf, "DADDR(", 6) != 0 ||
                                  (linebuf[6]-'0') != eu ||
                                  linebuf[7] != ':' ||
                                  strtol(linebuf+8, NULL, 10) != diskfileaddr ||
                                  linebuf[14] != ')') {
                                // return bogus data
                                memset(linebuf, 0, 255);
                                linebuf[255] = '\n';
                                diskfile.eof = true;
                                printf("*** DISKIO READ OF RECORD NEVER WRITTEN DFA=%d:%06d ***\n", eu, diskfileaddr);
                        }
                        linep += 15;
                        // always read 30 words
                        startaddr = core;
                        for (i=0; i<3; i++) {
                                if (diskfile.trace)
                                        fprintf(diskfile.trace, "\t%05o %d:%06d", core, eu, diskfileaddr);
                                for (j=0; j<10; j++) {
                                        if (diskfile.trace)
                                                fprintf(diskfile.trace, " %-8.8s", linep);
                                        // store until word count exhausted
                                        if (words > 0) {
                                                acc.addr = core++;
                                                acc.word = getword(0);
                                                words--;
                                                store(&acc);
                                        }
                                }
                                if (diskfile.trace)
                                        fprintf(diskfile.trace, "\n");
                        }
                        // dump file header if in directory
                        if (diskfile.trace && eu==0 && diskfileaddr>=2004 && diskfileaddr<=2018)
                                fileheaderanalyze(&diskfile, MAIN+startaddr);
                        // next record address
                        diskfileaddr++;
                }
                if (diskfile.eof) {
                        if (diskfile.trace)
                                fprintf(diskfile.trace, "\t*** never written segments in this read ***\n");
                }
        } else {
                // regular write
                if (diskfile.trace)
                        fprintf(diskfile.trace, " WRITE WORDS=%02u\n", words);
                diskfile.eof = false;

                // keep writing records until word count is exhausted
                while (words > 0) {
                        // prepare buffer pointer and write header
                        linep = linebuf;
                        linep += sprintf(linep, "DADDR(%d:%06d)", eu, diskfileaddr);

                        // always write 30 words
                        startaddr = core;
                        for (i=0; i<3; i++) {
                                if (diskfile.trace)
                                        fprintf(diskfile.trace, "\t%05o %d:%06d", core, eu, diskfileaddr);
                                for (j=0; j<10; j++) {
                                        if (words > 0) {
                                                // if word count NOT exhausted, write next word
                                                acc.addr = core++;
                                                words--;
                                                fetch(&acc);
                                                putword(acc.word);
                                        } else {
                                                // if word count exhausted, write zeros
                                                putword(0ll);
                                        }
                                        if (diskfile.trace)
                                                fprintf(diskfile.trace, " %-8.8s", linep-8);
                                }
                                if (diskfile.trace)
                                        fprintf(diskfile.trace, "\n");
                        }

                        // mark end of record
                        linep += sprintf(linep, "\n");

                        // sanity check - should never fail
                        if (linep != linebuf+256) {
                                printf("*** DISKIO WRITE SANITY CHECK FAILED ***\n");
                                exit(2);
                        }

                        // write record to physical file
                        fseek(diskfile.fp, (eu*200000+diskfileaddr)*256, SEEK_SET);
                        fwrite(linebuf, sizeof(char), 256, diskfile.fp);
                        fflush(diskfile.fp);

                        // dump file header if in directory
                        if (diskfile.trace && eu==0 && diskfileaddr>=2004 && diskfileaddr<=2018)
                                fileheaderanalyze(&diskfile, MAIN+startaddr);
                        // next record address
                        diskfileaddr++;
                }

        }

        result |= (core << SHFT_ADDR);
        if (iocw & MASK_IODUSEWC)
                result |= ((WORD48)words << SHFT_WCNT);

        if (diskfile.trace) {
                fprintf(diskfile.trace, "\tIO RESULT=%016llo\n\n", result);
                fflush(diskfile.trace);
                fclose(diskfile.trace);
                diskfile.trace = fopen(diskfile.tracename, "a");
        }
        return result;
}

WORD48 tape_access(WORD48 iocw) {
        static int lastpos = -1;
        unsigned unitdes, count, words;
        BIT mi, binary, tapedir, usewc, read;
        ADDR15 core;
        // prepare result with unit and read flag and MOD III DESC bit
        WORD48 result = (iocw & (MASK_IODUNIT | MASK_IODREAD)) | MASK_IORISMOD3;
        ACCESSOR acc;
        int i, cp=42;
        BIT first;
        WORD48 w;
        int lastchar;
        int lp;

        acc.id = "MTA";
        acc.MAIL = false;

        unitdes = (iocw & MASK_IODUNIT) >> SHFT_IODUNIT;
        count = (iocw & MASK_WCNT) >> SHFT_WCNT;
        mi = (iocw & MASK_IODMI) ? true : false;
        binary = (iocw & MASK_IODBINARY) ? true : false;
        tapedir = (iocw & MASK_IODTAPEDIR) ? true : false;
        usewc = (iocw & MASK_IODUSEWC) ? true : false;
        read = (iocw & MASK_IODREAD) ? true : false;
        core = (iocw & MASK_ADDR) >> SHFT_ADDR;
        // number of words to do
        words = (usewc) ? count : 1024;

        if (unitdes != 1 || tapefile.fp == NULL) {
                result |= MASK_IORNRDY | (core << SHFT_ADDR);
                return  result;
        }

        if (tapefile.trace) {
                fprintf(tapefile.trace, "%08u %s IOCW=%016llo WC=%u MI=%u BIN=%u REV=%u USEWC=%u READ=%u CORE=%05o",
                instr_count, unit[unitdes][read].name, iocw,
                count, mi, binary, tapedir, usewc, read, core);
        }

        // now analyze valid combinations
        if (mi && tapedir && !read) {
                // rewind
                tapefile.line = 0;
                tapefile.eof = 0;
                if (tapefile.trace)
                        fprintf(tapefile.trace, " REWIND\n");
                goto retresult;
        }
        if (!mi && tapedir && read) {
                // read reverse
                if (tapefile.trace)
                        fprintf(tapefile.trace, " FAKE READ REVERSE\n");
                if (lastpos >= 0) {
                        tapefile.line = lastpos;
                        lastpos = -1;
                        if (mi || words == 0) {
                                // no data transfer - we are good
                                goto retresult;
                        }
                }
                // oh crap - we really ought to read backwards here
                // return read error
                if (tapefile.trace)
                        fprintf(tapefile.trace, " READ BACKWARDS NOT POSSIBLE\n");
                result |= MASK_IORD20;
                goto retresult;
        }
        if (!mi && !tapedir && read) {
                // read forward
                lastpos = tapefile.line;
                fseek(tapefile.fp, tapefile.line, SEEK_SET);
                first = true;
                w = 0ll;
                cp = 42;
                lastchar = -1;
                if (tapefile.trace)
                        fprintf(tapefile.trace, " READ %u WORDS to %05o\n\t'", words, core);
                lp = 0;
                while (1) {
                        i = fgetc(tapefile.fp);
                        if (i < 0) {
                                tapefile.eof = true;
                                if (tapefile.trace)
                                        fprintf(tapefile.trace, " EOT\n");
                                result |= (core << SHFT_ADDR) | MASK_IOREOT;
                                goto retresult;
                        }
                        // check for record end
                        if (i & 0x80) {
                                // record marker
                                if (lastchar == 0x8f) {
                                        // tape mark
                                        if (tapefile.trace)
                                                fprintf(tapefile.trace, "' MARK\n");
                                        result |= (core << SHFT_ADDR) | MASK_IORD21;
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
                                        result |= (core << SHFT_ADDR) | MASK_IORD20;
                                        goto retresult;
                                }
                        }
                        // record position
                        tapefile.line++;
                        lastchar = i;

                        first = false;
                        // assemble char into word

                        if (tapefile.trace) {
                                if (lp >= 80) {
                                        fprintf(tapefile.trace,"'\n\t'");
                                        lp = 0;
                                }
                                fprintf(tapefile.trace, "%c", translatetable_bic2ascii[i & 0x3f]);
                                lp++;
                        }

                        if (!binary) {
                                // translate external BSL as ASCII to BIC??
                        }

                        w |= (WORD48)(i & 0x3f) << cp;
                        cp -= 6;
                        if (cp < 0) {
                                if (words > 0) {
                                        acc.addr = core++;
                                        acc.word = w;
                                        words--;
                                        store(&acc);
                                }
                                w = 0ll;
                                cp = 42;
                        }
                } /* while(1) */

recend:         // record end reached
                if (tapefile.trace)
                        fprintf(tapefile.trace, "'\n");
                // store possible partial filled word
                if (words > 0 && cp != 42) {
                        acc.addr = core++;
                        acc.word = w;
                        words--;
                        store(&acc);
                }
                // return good result
                if (tapefile.trace)
                        fprintf(tapefile.trace, "\tWORDS REMAINING=%u LAST CHAR=%u\n", words, (42-cp)/6);
                goto retresult;
        }
        if (!read) {
                // write??
                // return no ring status
                if (tapefile.trace)
                        fprintf(tapefile.trace, " WRITE BUT NO WRITE RING\n");
                result |= MASK_IORD20 | MASK_IORMAE;
                goto retresult;
        }
        // what's left...
        printf("*\ttape operation not implemented %016llo\n", iocw);
        // return good result
        if (tapefile.trace)
                fprintf(tapefile.trace, " tape operation not implemented\n");

retresult:
        if (iocw & MASK_IODUSEWC)
                result |= (WORD48)words << SHFT_WCNT;

        result |= (core << SHFT_ADDR) | ((WORD48)((42-cp)/6) << SHFT_IORCHARS);

        if (tapefile.trace) {
                fprintf(tapefile.trace, "\tIO RESULT=%016llo\n\n", result);
                fflush(tapefile.trace);
        }
        return result;
}

WORD48 lp_write(WORD48 iocw) {
        unsigned unitdes, count;
        BIT mi;
        WORD2 space;
        WORD4 skip;
        // prepare result with unit and read flag and MOD III DESC bit
        WORD48 result = iocw & (MASK_IODUNIT | MASK_IODREAD);
        ACCESSOR acc;
        int i;

        acc.id = "LPA";
        acc.MAIL = false;

        unitdes = (iocw & MASK_IODUNIT) >> SHFT_IODUNIT;
        mi = (iocw & MASK_IODMI) ? true : false;
        space = (iocw & 06000000) >> 19;
        skip = (iocw & 01700000) >> 15;
        acc.addr = (iocw & MASK_ADDR) >> SHFT_ADDR;
        if (iocw & MASK_IODUSEWC)
                count = (iocw & MASK_WCNT) >> SHFT_WCNT;
        else
                count = 0;

        if (unitdes != 22) {
                result |= MASK_IORNRDY | (acc.addr << SHFT_ADDR);
                return  result;
        }

        printf("LPA: ");
        if (skip) {
                // skip to stop
                printf("S%X ", skip);
        } else {
                // space
                switch (space) {
                case 0: printf("F0 "); break;
                case 1: case 3: printf("F2 "); break;
                case 2: printf("F1 "); break;
                }
        }
        if (!mi) {
                // print
                while (count > 0) {
                        fetch(&acc);
                        for (i=42; i>=0; i-=6)
                                putc(translatetable_bic2ascii[(acc.word>>i)&077], stdout);
                        acc.addr++;
                        count--;
                }
        }
        printf ("\n");

        result |= (acc.addr << SHFT_ADDR);
        // set printer 1 finished IRQ
        CC->CCI06F = true;
        signalInterrupt(acc.id, "FIN");

        return result;
}

void clearInterrupt(void) {
        // Resets an interrupt based on the current setting of CC->IAR, then
        // re-prioritices any remaining interrupts, leaving the new vector address
        // in CC->IAR
        if (CC->IAR) {
                // current active IRQ
                CC->interruptMask &= ~(1ll << CC->IAR);
                switch (CC->IAR) {
                case 022: // Time interval
                        CC->CCI03F = false;
                        break;
                case 027: // I/O 1 finished
                        CC->CCI08F = false;
                        CC->AD1F = false; // make unit non-busy
                        CC->iouMask &= ~1;
                        break;
                case 030: // I/O 2 finished
                        CC->CCI09F = false;
                        CC->AD2F = false; // make unit non-busy
                        CC->iouMask &= ~2;
                        break;
                case 031: // I/O 3 finished
                        CC->CCI10F = false;
                        CC->AD3F = false; // make unit non-busy
                        CC->iouMask &= ~4;
                        break;
                case 032: // I/O 4 finished
                        CC->CCI11F = false;
                        CC->AD4F = false; // make unit non-busy
                        CC->iouMask &= ~8;
                        break;
                case 025: // Printer 1 finished
                        CC->CCI06F = false;
                        break;
                case 026: // Printer 2 finished
                        CC->CCI07F = false;
                        break;
                // 64-75: P1 syllable-dependent
                case 064: case 065: case 066: case 067:
                case 070: case 071: case 072: case 073:
                case 074: case 075:
                        P[0]->r.I &= 0x0F;
                        break;

                case 034: // Inquiry request
                        CC->CCI13F = false;
                        break;
                case 024: // Keyboard request
                        CC->CCI05F = false;
                        break;
                // 44-55: P2 syllable-dependent
                case 044: case 045: case 046: case 047:
                case 050: case 051: case 052: case 053:
                case 054: case 055:
                        if (P[1]) P[1]->r.I &= 0x0F;
                        break;

                case 060: // P1 memory parity error
                        P[0]->r.I &= 0xFE;
                        break;
                case 061: // P1 invalid address error
                        P[0]->r.I &= 0xFD;
                        break;
                case 062: // P1 stack overflow
                        P[0]->r.I &= 0xFB;
                        break;

                case 040: // P2 memory parity error
                        if (P[1]) P[1]->r.I &= 0xFE;
                        break;
                case 041: // P2 invalid address error
                        if (P[1]) P[1]->r.I &= 0xFD;
                        break;
                case 042: // P2 stack overflow
                        if (P[1]) P[1]->r.I &= 0xFB;
                        break;

                case 036: // Disk file 1 read check finished
                        CC->CCI15F = false;
                        break;
                case 037: // Disk file 2 read check finished
                        CC->CCI16F = false;
                        break;
                case 023: // I/O busy
                        CC->CCI04F = false;
                        break;
                case 033: // P2 busy
                        CC->CCI12F = false;
                        break;
                case 035: // Special interrupt 1
                        CC->CCI14F = false;
                        break;
                default: // no interrupt vector was set
                        break;
                }
        }
#if 0
        if (spiofile.trace) {
                fprintf(spiofile.trace, "%08u clearInterrupt P1.I=%02x P2.I=%02x MASK=%012llx LATCH=%012llx\n",
                        instr_count, P[0]->r.I, P[1]->r.I, CC->interruptMask, CC->interruptLatch);
        }
#endif
        signalInterrupt("CC", "AGAIN");
};


void interrogateInterrupt(CPU *cpu) {
        // control-state only
        if (CC->IAR && !cpu->r.NCSF) {
                if (spiofile.trace) {
                        fprintf(spiofile.trace, "%08u ITI IAR=%03o(%s)\n",
                                instr_count, CC->IAR, irq[CC->IAR-020].name);
                }
                cpu->r.C = CC->IAR;
                cpu->r.L = 0;
                // stack address @100
                cpu->r.S = AA_IRQSTACK;
                // clear IRQ
                clearInterrupt();
                // require fetch at SECL
                cpu->r.PROF = false;
                return;
        }
#if 0
        // elaborate trace
        if (spiofile.trace) {
                fprintf(spiofile.trace, "%08u ITI NO IRQ PENDING\n",
                        instr_count);
        }
#endif
}

void initiateIO(CPU *cpu) {
        ACCESSOR acc;
        WORD48 iocw;
        WORD48 result;
        unsigned unitdes, wc;
        ADDR15 core;
        BIT reading;
        int eu = -1, diskfileaddr = -1;
        int i;

        acc.id = "IO";

        // get address of IOCW
        acc.addr = 010;
        acc.MAIL = false;
        fetch(&acc);
        // get IOCW itself
        acc.addr = acc.word;
        acc.MAIL = false;
        fetch(&acc);
        iocw = acc.word;

        // prepare result descriptor with not ready set
        result = iocw | MASK_IORNRDY;

        // analyze IOCW
        unitdes = (iocw & MASK_IODUNIT) >> SHFT_IODUNIT;
        wc = (iocw & MASK_WCNT) >> SHFT_WCNT;
        reading = (iocw & MASK_IODREAD) ? true : false;
        core = (iocw & MASK_ADDR) >> SHFT_ADDR;

        if (unitdes == 6 || unitdes == 12) {
                // disk file units: fetch first word from core with disk address
                acc.addr = core++;
                fetch(&acc);
                acc.word &= MASK_DSKFILADR;
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
        }

        // elaborate trace
        if (spiofile.trace) {
                fprintf(spiofile.trace, "%08u IOCW=%016llo\n", instr_count, iocw);
                fprintf(spiofile.trace, "\tunit=%u(%s) core=%05o", unitdes, unit[unitdes][reading].name, core);
                if (iocw & MASK_IODMI) fprintf(spiofile.trace, " inhibit");
                if (iocw & MASK_IODBINARY) fprintf(spiofile.trace, " binary"); else fprintf(spiofile.trace, " alpha");
                if (iocw & MASK_IODTAPEDIR)  fprintf(spiofile.trace, " reverse");
                if (reading) fprintf(spiofile.trace, " read"); else fprintf(spiofile.trace, " write");
                if (iocw & MASK_IODSEGCNT) fprintf(spiofile.trace, " segments=%llu", (iocw & MASK_IODSEGCNT) >> SHFT_IODSEGCNT);
                if (iocw & MASK_IODUSEWC) fprintf(spiofile.trace, " wc=%u", wc);
                if (diskfileaddr >= 0) fprintf(spiofile.trace, " dfa=%d:%06d", eu, diskfileaddr);
                fprintf(spiofile.trace, "\n");
        }
        switch (unitdes) {
        case 1:  // MTA
        case 3:  // MTB
        case 5:  // MTC
        case 7:  // MTD
        case 9:  // MTE
        case 11:  // MTF
        case 13:  // MTH
        case 15:  // MTJ
        case 17:  // MTK
        case 19:  // MTL
        case 21:  // MTM
        case 23:  // MTN
        case 25:  // MTP
        case 27:  // MTR
        case 29:  // MTS
        case 31:  // MTT
                result = tape_access(iocw);
                break;
        case 6:  // DF1
                result = disk_access(iocw, eu, diskfileaddr);
                break;
        case 10: // CRA CPA
                if (reading)
                        result = card_read(iocw);
                break;
        case 22: // LPA
                if (!(iocw & MASK_IODREAD))
                        result = lp_write(iocw);
                break;
        case 30: // SPO
                if (iocw & MASK_IODREAD)
                        result = spo_read(iocw);
                else
                        result = spo_write(iocw);
                break;
        }

        if (spiofile.trace) {
                fprintf(spiofile.trace, "\tRSLT=%016llo\n", result);
                fflush(spiofile.trace);
        }

        // return IO RESULT
        acc.addr = 014;
        acc.word = result;
        store(&acc);
        CC->CCI08F = true;
        signalInterrupt("IO", "COMPLETE");
}

WORD48 interrogateUnitStatus(CPU *cpu) {
        static int td = 0;
#if 0
        // elaborate trace
        if (spiofile.trace)
                fprintf(spiofile.trace, "%08u TUS=%016llo\n", instr_count, unitsready);
#endif
        // check console input
        spo_test();
        // simulate timer
        if (++td > 200) {
                CC->TM++;
                if (CC->TM >= 63) {
                        CC->TM = 0;
                        CC->CCI03F = true;
                        signalInterrupt("CC", "TIMER");
                } else {
                        CC->TM++;
                }
                td = 0;
        }
        return unitsready;
}

WORD48 interrogateIOChannel(CPU *cpu) {
        WORD48 result = 0;
        // report I/O control unit 1
        result = 1ll;

#if 0
        // elaborate trace
        if (spiofile.trace)
                fprintf(spiofile.trace, "%08u TIO=%llu\n", instr_count, result);
#endif
        return result;
}

WORD48 readTimer(CPU *cpu) {
        WORD48 result = 0;

        if (CC->CCI03F)
                result =  CC->TM | 0100;
        else
                result =  CC->TM;

#if 0
        // elaborate trace
        if (spiofile.trace)
                fprintf(spiofile.trace, "%08u RTR=%03llo\n", instr_count, result);
#endif
        return result;
}

const char *relsym(unsigned offset, BIT cEnabled) {
        static char buf[32];
        if (cpu->r.SALF) {
                // subroutine level - check upper 3 bits of the 10 bit offset
                switch ((offset >> 7) & 7) {
                case 0:
                case 1:
                case 2:
                case 3:
                        // pattern 0xx xxxxxxx - R+ relative
                        // reach 0..511
                        offset &= 0x1ff;
prtuse:
                        if (offset < MAXNAME && name[offset][0] != 0) {
                                // do we have a memory address?
                                if (MAIN[(cpu->r.R<<6) + offset] & MASK_FLAG)
                                        sprintf(buf, "%s=%05llo", name[offset], MAIN[(cpu->r.R<<6) + offset] & MASK_ADDR);
                                else
                                        sprintf(buf, "%s", name[offset]);
                                return buf;
                        }
                        sprintf(buf, "PRT[%03o]", offset);
                        return buf;
                case 4:
                case 5:
                        // pattern 10x xxxxxxx - F+ or (R+7)+ relative
                        // reach 0..255
                        offset &= 0xff;
                        if (cpu->r.MSFF) {
                                // during function parameter loading its (R+7)+
                                sprintf(buf, "PARAMETERLOADING %u", offset);
                                return buf;
                        } else {
                                // inside function its F+
                                sprintf(buf, "LOCAL %u", offset);
                                return buf;
                        }
                case 6:
                        // pattern 110 xxxxxxx - C+ relative on read, R+ on write
                        // reach 0..127
                        offset &= 0x7f;
                        if (cEnabled) {
                                sprintf(buf, "CODE+%u", offset);
                                return buf;
                        } else {
                                goto prtuse;
                        }
                case 7:
                        // pattern 111 xxxxxxx - F- or (R+7)- relative
                        // reach 0..127 (negative direction)
                        offset &= 0x7f;
                        if (cpu->r.MSFF) {
                                sprintf(buf, "PARAMETER %u", offset);
                                return buf;
                        } else {
                                sprintf(buf, "PARAMETER %u", offset);
                                return buf;
                        }
                } // switch
        } else {
                // program level - all 10 bits are offset
                offset &= 0x3ff;
                goto prtuse;
        }
        return "should not happen";
}

void codesym(ADDR15 c, WORD2 l) {
        unsigned bestmatch = 0;
        unsigned index;
        ADDR15 addr, bestaddr = 0;
        // search for symbol
        for (index = 0; index<MAXNAME; index++) {
                if (name[index][0] != 0) {
                        // do we have a memory address?
                        if (MAIN[index] & MASK_FLAG)
                                addr = MAIN[index] & MASK_ADDR;
                        else
                                addr = 0;
                        if (addr <= c && bestaddr < addr) {
                                bestmatch = index;
                                bestaddr = addr;
                        }
                }
        }
        if (bestmatch > 0)
                printf("%08u %s+%04o (%05o:%o) ", instr_count, name[bestmatch], ((c - bestaddr) << 2) + l, c, l);
        else
                printf("%08u (%05o:%o) ", instr_count, c, l);
}

void printinstr(WORD12 code, BIT cwmf)
{
        const INSTRUCTION *ip;
        // search instruction table
        ip = instruction_table;
        while (ip->name != 0) {
                switch (ip->outtype) {
                case OP_ASIS:
                case OP_BRAS:
                case OP_BRAW:
                        if (ip->cwmf == cwmf && ip->code == code) {
                                printf ("%-4.4s        %04o", ip->name, code);
                                return;
                        }
                        break;
                case OP_TOP4:
                        if (ip->cwmf == cwmf && ip->code == (code & 0x0ff)) {
                                printf ("%-4.4s  %4u  %04o", ip->name, code >> 8, code);
                                return;
                        }
                        break;
                case OP_TOP6:
                        if (ip->cwmf == cwmf && ip->code == (code & 0x03f)) {
                                printf ("%-4.4s  %02o    %04o", ip->name, code >> 6, code);
                                return;
                        }
                        break;
                case OP_TOP10:
                        if (ip->cwmf == cwmf && ip->code == (code & 0x003)) {
                                printf ("%-4.4s  %04o  %04o  (%s)", ip->name, code >> 2, code, relsym(code >> 2, true));
                                return;
                        }
                        break;
                default:
                        ;
                }
                ip++;
        }
        printf("unknown instruction %04o", code);
}

char *word2string(WORD48 w)
{
        static char buf[33];
        int i;
        for (i=7; i>=0; i--) {
                buf[3*i  ] = '0'+((w>>3) & 7);
                buf[3*i+1] = '0'+((w   ) & 7);
                buf[3*i+2] = ' ';
                buf[24+i] = translatetable_bic2ascii[w&077];
                w>>=6;
        }
        buf[32]=0;
        return buf;
}

char *lcw2string(WORD48 w)
{
        static char buf[33];
        if (w & MASK_CREG) {
                sprintf(buf, "Loop(%05llo:%llo Rpt=%03llo Prev=%05llo)",
                        (w & MASK_CREG) >> SHFT_CREG,
                        (w & MASK_LREG) >> SHFT_LREG,
                        (w & MASK_LCWrpt) >> SHFT_LCWrpt,
                        (w & MASK_FREG) >> SHFT_FREG);
                buf[32]=0;
        } else {
                buf[0]=0;
        }
        return buf;
}

void memdump(void) {
        WORD48 w;
        ADDR15 memaddr = 0;
        char buf[132];
        char *bufp;
        FILE *mfp = fopen("coredump.txt", "w");
        if (mfp == NULL) {
                perror("coredump.txt");
                exit(2);
        }
        do {
                int i, j;
                bufp = buf;
                bufp += sprintf(bufp, "%05o ", memaddr);
                for (i=0; i<4; i++) {
                        w = MAIN[memaddr+i];
                        for (j=0; j<8; j++) {
                                bufp += sprintf(bufp, "%02llo", (w>>42)&077);
                                w<<=6;
                        }
                        *bufp++ = ' ';
                }
                for (i=0; i<4; i++) {
                        w = MAIN[memaddr+i];
                        for (j=0; j<8; j++) {
                                *bufp++ = translatetable_bic2ascii[(w>>42)&077];
                                w<<=6;
                        }
                        *bufp++ = ' ';
                }
                *bufp++ = '\n';
                *bufp++ = 0;
                fputs(buf, mfp);
                memaddr = (memaddr + 4) & 077777;
        } while (memaddr != 0);
        fclose (mfp);
}

void execute(ADDR15 addr) {
        cpu->r.C = addr;
        cpu->r.L = 0;
        loadPviaC(cpu); // load the program word to P
        switch (cpu->r.L) {
        case 0:
                cpu->r.T = (cpu->r.P >> 36) & 0xfff;
                cpu->r.L = 1;
                break;
        case 1:
                cpu->r.T = (cpu->r.P >> 24) & 0xfff;
                cpu->r.L = 2;
                break;
        case 2:
                cpu->r.T = (cpu->r.P >> 12) & 0xfff;
                cpu->r.L = 3;
                break;
        case 3:
                cpu->r.T = (cpu->r.P >> 0) & 0xfff;
                cpu->r.L = 0;
                cpu->r.C++;
                cpu->r.PROF = false;
                break;
        }
        cpu->r.TROF = true;

runagain:
        start(cpu);
printf("runn: C=%05o L=%o T=%04o\n", cpu->r.C, cpu->r.L, cpu->r.T);
        while (cpu->busy) {
                instr_count++;
#if 0
                // check for instruction count
                if (instr_count > 800000) {
                        dotrcmem = dodmpins = dotrcins = false;
                        memdump();
                        closefile(&tapefile);
                        closefile(&diskfile);
                        closefile(&cardfile);
                        closefile(&spiofile);
                        exit (0);
                } else if (instr_count >= 680000) {
                        dotrcmem = dodmpins = dotrcins = true;
                }
#endif
                if (dotrcins) {
                        ADDR15 c;
                        WORD2 l;
                        c = cpu->r.C;
                        l = cpu->r.L;
                        if (l == 0) {
                                l = 3;
                                c--;
                        } else {
                                l--;
                        }
                        printf("\n");
                        codesym(c, l);
                        printinstr(cpu->r.T, cpu->r.CWMF);
                        printf("\n");
                }

                // end check for instruction count
                cpu->cycleLimit = 1;
                run(cpu);
                if (cpu->r.T == 03011)
                        printf("\tgotcha 3011!\n");
                if (dotrcins) {
                        if (cpu->r.CWMF) {
                                printf("\tSI(M:G:H)=%05o:%o:%o A=%s (%u) Y=%02o\n",
                                        cpu->r.M, cpu->r.G, cpu->r.H,
                                        word2string(cpu->r.A), cpu->r.AROF,
                                        cpu->r.Y);
                                printf("\tDI(S:K:V)=%05o:%o:%o B=%s (%u) Z=%02o\n",
                                        cpu->r.S, cpu->r.K, cpu->r.V,
                                        word2string(cpu->r.B), cpu->r.BROF,
                                        cpu->r.Z);
                                printf("\tR=%03o N=%d F=%05o TFFF=%u SALF=%u NCSF=%u T=%04o\n",
                                        cpu->r.R, cpu->r.N, cpu->r.F,
                                        cpu->r.TFFF, cpu->r.SALF, cpu->r.NCSF, cpu->r.T);
                                printf("\tX=__%014llo %s\n", cpu->r.X, lcw2string(cpu->r.X));
                        } else {
                                printf("\tA=%016llo(%u) GH=%o%o Y=%02o M=%05o F=%05o N=%d NCSF=%u T=%04o\n",
                                        cpu->r.A, cpu->r.AROF,
                                        cpu->r.G, cpu->r.H,
                                        cpu->r.Y, cpu->r.M,
                                        cpu->r.F,
                                        cpu->r.N, cpu->r.NCSF, cpu->r.T);
                                printf("\tB=%016llo(%u) KV=%o%o Z=%02o S=%05o R=%03o MSFF=%u SALF=%u\n",
                                        cpu->r.B, cpu->r.BROF,
                                        cpu->r.K, cpu->r.V,
                                        cpu->r.Z, cpu->r.S,
                                        cpu->r.R,
                                        cpu->r.MSFF, cpu->r.SALF);
                        }
                }
        }
        // CPU stopped
        printf("\n\n***** CPU stopped *****\nContinue?  ");
        (void)fgets(linebuf, sizeof linebuf, stdin);
        if (linebuf[0] != 'n')
                goto runagain;
}

int main(int argc, char *argv[])
{
        int opt;
        ADDR15 addr;

        printf("B5500 Card Reader\n");
        diskfile.name = (char*)"./disk/dka.dat";

        b5500_init_shares();

        memset(MAIN, 0, MAXMEM*sizeof(WORD48));
        cpu = P[0];
        memset(cpu, 0, sizeof(CPU));
        cpu->id = "P1";
        cpu->acc.id = cpu->id;
        cpu->isP1 = true;

        while ((opt = getopt(argc, argv, "imsezrSl:I:c:C:t:T:d:D:")) != -1) {
                switch (opt) {
                case 'i':
                        dodmpins = true; /* I/O trace */
                        break;
                case 'm':
                        dotrcmem = true; /* trace memory accesses */
                        break;
                case 's':
                        dolistsource = true; /* list source */
                        break;
                case 'e':
                        dotrcins = true; /* trace execution */
                        break;
                case 'z':
                        cpu->r.US14X = true; /* stop on ZPI */
                        break;
                case 'r':
                        realspo = true; /* print to real SPO */
                        break;
                case 'S':
                        cardload = true; /* load from CARD else from DISK */
                        break;
                case 'l':
                        listfile.name = optarg; /* file with listing */
                        break;
                // I/O and special instruction trace
                case 'I':
                        spiofile.tracename = optarg; /* trace file for special instructions and I/O */
                        break;
                // CARD
                case 'c':
                        cardfile.name = optarg; /* file with cards */
                        break;
                case 'C':
                        cardfile.tracename = optarg; /* trace file for cards */
                        break;
                // TAPE
                case 't':
                        tapefile.name = optarg; /* file with tape */
                        break;
                case 'T':
                        tapefile.tracename = optarg; /* trace file for tapes */
                        break;
                // DISK
                case 'd':
                        diskfile.name = optarg; /* file with disk image */
                        break;
                case 'D':
                        diskfile.tracename = optarg; /* trace file for disk image */
                        break;
                default: /* '?' */
                        fprintf(stderr,
                                "Usage: %s [-b] [-m] [-s] [-e] [-l <file>] [-c <file>] [-t <file>] [-d <file>]\n"
                                "\t-i\t\ttrace I/O\n"
                                "\t-m\t\tshow memory accesses\n"
                                "\t-s\t\tlist source cards\n"
                                "\t-e\t\ttrace execution\n"
                                "\t-z\t\tstop at ZPI instruction\n"
                                "\t-r\t\tuse real SPO\n"
                                "\t-S\t\tcard load select\n"
                                "\t-l <file>\tspecify listing file name\n"
                                "\t-c <file>\tspecify card file name\n"
                                "\t-t <file>\tspecify tape file name\n"
                                "\t-d <file>\tspecify disk file name\n"
                                "\t-C <file>\tspecify trace card file name\n"
                                "\t-T <file>\tspecify trace tape file name\n"
                                "\t-D <file>\tspecify trace disk file name\n"
                                "\t-I <file>\tspecify I/O and special instruction trace file name\n"
                                , argv[0]);
                        exit(2);
                }
        }

        // check translate tables bic2ascii and ascii2bic for consistency
        for (addr=0; addr<64; addr++) {
                if (translatetable_ascii2bic[translatetable_bic2ascii[addr]] != addr)
                        printf("tranlatetable error at bic=%02o\n", addr);
        }

        if (argc - optind != 0) {
                fprintf(stderr, "extra parameters on command line\n");
                exit (2);
        }

        unitsready = 0ll;

        // report SPO as ready
        unitsready |= 1ll << unit[30][1].readybit;
        if (realspo)
               canfile = open("/dev/ttyS4", O_RDWR);

        opt = openfile(&listfile, "r");

        // optional listing file parsing
        if (listfile.fp) {
                while (!listfile.eof) {
                        getlin(&listfile);
                        if (strncmp(linep, "PRT(", 4) == 0) {
                                unsigned index;
                                linep += 4;
                                index = strtoul(linep, &linep, 8);
                                if (index < MAXNAME && strncmp(linep, ") = ", 4) == 0) {
                                        linep += 4;
                                        strncpy(name[index], linep, sizeof *name);
                                        name[index][sizeof *name-1] = 0;
                                        printf("%03o %s\n", index, name[index]);
                                }
                        }
                }
        }
        closefile(&listfile);

        opt = openfile(&cardfile, "r");
        if (opt)
                unitsready |= 1ll << unit[10][1].readybit;

        opt = openfile(&tapefile, "r");
        if (opt)
                unitsready |= 1ll << unit[ 1][1].readybit;

        opt = openfile(&diskfile, "r+");
        if (opt)
                unitsready |= 1ll << unit[ 6][1].readybit;

        opt = openfile(&spiofile, "r");
        if (opt)
                exit(2);

        // printer
        unitsready |= 1ll << unit[22][0].readybit;

        start(cpu);

        if (cardload) {
                // binary read first card to 00020
                addr = 020;
                card_read  (0240000540000020LL);
        } else {
                // load disk segment 1..63 to 00020
                disk_access(0140000047700017LL, 0, 1);
        }
        execute(00020);

        fclose(cardfile.fp);

        return 0;
}
