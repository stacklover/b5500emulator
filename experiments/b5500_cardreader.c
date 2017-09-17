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
***********************************************************************/

#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include "b5500_common.h"

#define MAXLINELENGTH   (264)   /* maximum line length for all devices - must be multiple of 8 */

/* debug flags: turn these on for various dumps and traces */
int dodmpins     = false;       /* dump instructions after assembly */
int dotrcmem     = false;       /* trace memory accesses */
int dolistsource = false;       /* list source line */
int dotrcins     = false;       /* trace instruction execution */
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

/* input/output buffer */
char    linebuf[MAXLINELENGTH];
char    *linep;

/* NAME entries */
#define MAXNAME 500
char name[MAXNAME][9];

/* instruction execution counter */
unsigned instr_count;

int openfile(FILEHANDLE *f, const char *mode) {
        if (f->name != NULL) {
                f->fp = fopen(f->name, mode);
                if (f->fp == NULL) {
                        perror(f->name);
                        return errno;
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
                        return errno;
                }
        } else
                f->trace = NULL;
        return 0;
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

void signalInterrupt(CPU *cpu)
{
        printf("\nIRQ=$%02x\n", cpu->r.I);
        sleep(10);
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

BIT card_read_alpha(ADDR15 *addr) {
        int chars = 80;
        ACCESSOR acc;
        if (cardfile.fp == NULL)
                return false;
        acc.id = "CRA";
        acc.MAIL = false;
        getlin(&cardfile);
        if (cardfile.eof)
                return false;
        if (cardfile.trace) {
                fprintf(cardfile.trace, "%08u %s -> %05o\n", instr_count, linebuf, *addr);
                fflush(cardfile.trace);
        }
        if (dolistsource)
                printf("*\tCRA: %s\n", linebuf);
        if (strlen(linebuf) > (unsigned)chars) {
                printf("*\tNOTE: alpha card line too long\n");
                exit(0);
        }
        if (dolistsource && strlen(linebuf) < (unsigned)chars) {
                printf("*\tNOTE: alpha card line too short. padded with blanks\n");
        }
        while (chars > 0) {
                acc.addr = (*addr)++;
                acc.word = getword(060);
                chars -= 8;
                store(&acc);
        }
        return true;
}

BIT card_read_binary(ADDR15 *addr) {
        int chars = 160;
        ACCESSOR acc;
        if (cardfile.fp == NULL)
                return false;
        acc.id = "CRA";
        acc.MAIL = false;
        getlin(&cardfile);
        if (cardfile.eof)
                return false;
        if (cardfile.trace) {
                fprintf(cardfile.trace, "%08u %s -> %05o\n", instr_count, linebuf, *addr);
                fflush(cardfile.trace);
        }
        if (dolistsource)
                printf("*\tCRA: %s\n", linebuf);
        if (strlen(linebuf) != (unsigned)chars) {
                printf("*\tERROR: binary card incorrect length(%u). abort\n", strlen(linebuf));
                exit(0);
        }
        while (chars > 0) {
                acc.addr = (*addr)++;
                acc.word = getword(0);
                chars -= 8;
                store(&acc);
        }
        return true;
}

void spo_write(ADDR15 *addr) {
        int count;
        ACCESSOR acc;
        acc.id = "SPO";
        acc.MAIL = false;
        linep = linebuf;
loop:   acc.addr = (*addr)++;
        acc.MAIL = false;
        fetch(&acc);
        for (count=0; count<8; count++) {
                  if (linep >= linebuf + sizeof linebuf - 1)
                          goto done;
                  if (((acc.word >> 42) & 0x3f) == 037)
                          goto done;
                  *linep++ = translatetable_bic2ascii[(acc.word>>42) & 0x3f];
                  acc.word <<= 6;
        }
        goto loop;
done:   *linep++ = 0;
        printf ("*\tSPO: %s\n", linebuf);
        // also write this to all open trace files
        if (cardfile.trace) {
                fprintf(cardfile.trace, "*** SPO: %s\n\n", linebuf);
                fflush(cardfile.trace);
        }
        if (tapefile.trace) {
                fprintf(tapefile.trace, "*** SPO: %s\n\n", linebuf);
                fflush(tapefile.trace);
        }
        if (diskfile.trace) {
                fprintf(diskfile.trace, "*** SPO: %s\n\n", linebuf);
                fflush(diskfile.trace);
        }
        sleep(1);
}

WORD48 disk_access(WORD48 iocw) {
        unsigned unit, count, segcnt, words;
        ADDR15 core;
        // prepare result with unit and read flag
        WORD48 result = iocw & (MASK_IODUNIT | MASK_IODREAD);
        ACCESSOR acc;
        WORD48 diskfileaddr;
        unsigned bindiskfileaddr;
        int i;
        int lp;

        if (diskfile.fp == NULL) {
                diskfile.fp = fopen(diskfile.name, "r+");
                if (diskfile.fp == NULL) {
                        perror(diskfile.name);
                        exit(0);
                }
        }

        acc.id = "DF1";
        acc.MAIL = false;
        unit = (iocw & MASK_IODUNIT) >> SHFT_IODUNIT;
        count = (iocw & MASK_IODWC) >> SHFT_IODWC;
        segcnt = (iocw & MASK_IODSEGCNT) >> SHFT_IODSEGCNT;
        core = (iocw & MASK_IODADDR) >> SHFT_IODADDR;
        // number of words to do
        words = (iocw & MASK_IODUSEWC) ? count : segcnt * 30;

        if (unit != 6 || diskfile.fp == NULL) {
                result |= MASK_IORNRDY | (core << SHFT_IODADDR);
                return  result;
        }

        if (diskfile.trace) {
                fprintf(diskfile.trace, "%08u IOCW=%016llo", instr_count, iocw);
        }

        // fetch first word from core with disk address
        acc.addr = core++;
        fetch(&acc);
        diskfileaddr = acc.word & MASK_DSKFILADR;
        bindiskfileaddr = 0;
        for (i=6; i>=0; i--) {
                bindiskfileaddr *= 10;
                bindiskfileaddr += ((diskfileaddr >> 6*i) & 017);
        }
        if (diskfile.trace)
                fprintf(diskfile.trace, " [%05o]=%014llo DISKADDR=%05u", acc.addr, acc.word, bindiskfileaddr);

        if (iocw & MASK_IODREAD) {
                if (diskfile.trace)
                        fprintf(diskfile.trace, " READ WORDS=%02u\n\t%05o %05u '", words, core, bindiskfileaddr);
                lp = 0;
                diskfile.eof = false;
                while (words > 0) {
                        fseek(diskfile.fp, bindiskfileaddr*256, SEEK_SET);
                        fread(linebuf, sizeof(char), 256, diskfile.fp);
                        linep = linebuf;
                        if (strncmp(linebuf, "DADDR(", 6) != 0) {
                                memset(linebuf, '^', 255);
                                linebuf[255] = '\n';
                                diskfile.eof = true;
                        }
                        linep += 15;
                        for (i=0; i<30; i++) {
                                if (diskfile.trace) {
                                        if (lp >= 10) {
                                                fprintf(diskfile.trace, "'\n\t%05o %05u '", core, bindiskfileaddr);
                                                lp = 0;
                                        }
                                        fprintf(diskfile.trace, "%-8.8s", linep);
                                        lp++;
                                }
                                if (words > 0) {
                                        acc.addr = core++;
                                        acc.word = getword(0);
                                        words--;
                                        store(&acc);
                                }
                        }
                        bindiskfileaddr++;
                }
                if (diskfile.trace)
                        fprintf(diskfile.trace, "'\n");
                if (diskfile.eof) {
                        if (diskfile.trace)
                                fprintf(diskfile.trace, "\t*** never written segments in this read ***\n");
                        //exit(0);
                }
        } else {
                if (diskfile.trace)
                        fprintf(diskfile.trace, " WRITE WORDS=%02u\n\t%05o %05u '", words, core, bindiskfileaddr);
                lp = 0;
                diskfile.eof = false;
                while (words > 0) {
                        linep = linebuf;
                        linep += sprintf(linep, "DADDR(%08u)", bindiskfileaddr);
                        for (i=0; i<30; i++) {
                                if (words > 0) {
                                        acc.addr = core++;
                                        words--;
                                        fetch(&acc);
                                        putword(acc.word);
                                } else {
                                        putword(0ll);
                                }
                                if (diskfile.trace) {
                                        if (lp >= 10) {
                                                fprintf(diskfile.trace, "'\n\t%05o %05u '", core, bindiskfileaddr);
                                                lp = 0;
                                        }
                                        fprintf(diskfile.trace, "%-8.8s", linep-8);
                                        lp++;
                                }
                        }
                        linep += sprintf(linep, "\n");
                        if (linep != linebuf+256) {
                                printf("linep error\n");
                                exit(0);
                        }
                        fseek(diskfile.fp, bindiskfileaddr*256, SEEK_SET);
                        fwrite(linebuf, sizeof(char), 256, diskfile.fp);
                        fflush(diskfile.fp);
                        bindiskfileaddr++;
                }
                if (diskfile.trace)
                        fprintf(diskfile.trace, "'\n");
        }

        result |= (core << SHFT_IODADDR);
        if (iocw & MASK_IODUSEWC)
                result |= ((WORD48)words << SHFT_DDWC);

        if (diskfile.trace) {
                fprintf(diskfile.trace, "\tIO RESULT=%016llo\n\n", result);
                fflush(diskfile.trace);
                fclose(diskfile.trace);
                diskfile.trace = fopen(diskfile.tracename, "a");
        }
        return result;
}

WORD48 tape_access(WORD48 iocw) {
        unsigned unit, count, words;
        BIT mi, tapedir, binary, read;
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

        unit = (iocw & MASK_IODUNIT) >> SHFT_IODUNIT;
        count = (iocw & MASK_IODWC) >> SHFT_IODWC;
        mi = (iocw & MASK_IODMI) >> SHFT_IODMI;
        binary = (iocw & MASK_IODBINARY) >> SHFT_IODBINARY;
        tapedir = (iocw & MASK_IODTAPEDIR) >> SHFT_IODTAPEDIR;
        read = (iocw & MASK_IODREAD) >> SHFT_IODREAD;
        core = (iocw & MASK_IODADDR) >> SHFT_IODADDR;
        // number of words to do
        words = (iocw & MASK_IODUSEWC) ? count : 1024;


        if (unit != 1 || tapefile.fp == NULL) {
                result |= MASK_IORNRDY | (core << SHFT_IODADDR);
                return  result;
        }

        if (tapefile.trace) {
                fprintf(tapefile.trace, "%08u IOCW=%016llo", instr_count, iocw);
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
        if (!mi && !tapedir && read) {
                // read forward
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
                                result |= (core << SHFT_IODADDR) | MASK_IOREOT;
                                goto retresult;
                        }
                        // check for record end
                        if (i & 0x80) {
                                // record marker
                                if (lastchar == 0x8f) {
                                        // tape mark
                                        if (tapefile.trace)
                                                fprintf(tapefile.trace, "' MARK\n");
                                        result |= (core << SHFT_IODADDR) | MASK_IORD21;
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
                                        result |= (core << SHFT_IODADDR) | MASK_IORD20;
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
          // what's left...
        printf("*\ttape operation not implemented");

retresult:
        if (iocw & MASK_IODUSEWC)
                result |= (WORD48)words << SHFT_DDWC;

        result |= (core << SHFT_IODADDR) | ((WORD48)((42-cp)/6) << SHFT_IORCHARS);

        if (tapefile.trace) {
                fprintf(tapefile.trace, "\tIO RESULT=%016llo\n\n", result);
                fflush(tapefile.trace);
        }
        return result;
}

WORD48 iohandler(WORD48 iocw) {
        unsigned unitdes, count;
        ADDR15 core;
        BIT reading;
        // prepare result descriptor
        WORD48 result = iocw & MASK_IODUNIT;

        unitdes = (iocw & MASK_IODUNIT) >> SHFT_IODUNIT;
        count = (iocw & MASK_IODWC) >> SHFT_IODWC;
        reading = (iocw & MASK_IODREAD) >> SHFT_IODREAD;
        core = (iocw & MASK_IODADDR) >> SHFT_IODADDR;

        if (dodmpins) {
                printf("*\tIOCW=%016llo\n", iocw);
                printf("*\tunit=%u(%s) count=%u addr=%05o", unitdes, unit[unitdes][reading].name, count, core);
                if (iocw & MASK_IODMI) printf(" mem_inhibit");
                if (iocw & MASK_IODBINARY) printf(" binary"); else printf(" alpha");
                if (iocw & MASK_IODTAPEDIR)  printf(" reverse");
                if (iocw & MASK_IODUSEWC) printf(" use_word_counter");
                if (reading) printf(" read"); else printf(" write");
                if (iocw & MASK_IODSEGCNT) printf(" segments=%llu", (iocw & MASK_IODSEGCNT) >> SHFT_IODSEGCNT);
                printf("\n");
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
                result = disk_access(iocw);
                break;
        case 10: // CRA CPA
                if ((iocw & MASK_IODMI) != 0 || (iocw & MASK_IODREAD) == 0) {
                        result |= (core << SHFT_IODADDR);
                        break;
                }
                if (iocw & MASK_IODBINARY) {
                        if (card_read_binary(&core)) {
                                result |= (core << SHFT_IODADDR);
                                break;
                        }
                } else {
                        if (card_read_alpha(&core)) {
                                result |= (core << SHFT_IODADDR);
                                break;
                        }
                }
                break;
        case 30: // SPO
                if ((iocw & MASK_IODREAD) != 0)
                        break;
                spo_write(&core);
                result |= (core << SHFT_IODADDR);
                break;
        }

        if (dodmpins) {
                printf("*\tRSLT=%016llo\n", result);
        }
        return result;
}

CPU *cpu;

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
                                        sprintf(buf, "%s=%05llo", name[offset], MAIN[(cpu->r.R<<6) + offset] & MASK_PCWADDR);
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
                                addr = MAIN[index] & MASK_PCWADDR;
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

void printinstr(ADDR15 wc, WORD2 sc, BIT cwmf)
{
        const INSTRUCTION *ip;
        WORD12 code = 0;

        switch (sc) {
        case 0: code = (MAIN[wc] >> 36) & 0xfff; break;
        case 1: code = (MAIN[wc] >> 24) & 0xfff; break;
        case 2: code = (MAIN[wc] >> 12) & 0xfff; break;
        case 3: code = (MAIN[wc] >> 0) & 0xfff; break;
        }
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
        if (w & MASK_LCWrC) {
                sprintf(buf, "Loop(%05llo:%llo Rpt=%03llo Prev=%05llo)",
                        (w & MASK_LCWrC) >> SHFT_LCWrC,
                        (w & MASK_LCWrL) >> SHFT_LCWrL,
                        (w & MASK_LCWrpt) >> SHFT_LCWrpt,
                        (w & MASK_LCWrF) >> SHFT_LCWrF);
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
                        printinstr(c, l, cpu->r.CWMF);
                        printf("\n");
                }
                // check for instruction count
                if (instr_count > 95000) {
                        dotrcmem = dodmpins = dotrcins = false;
                        memdump();
                        closefile(&tapefile);
                        closefile(&diskfile);
                        closefile(&cardfile);
                        exit (0);
                } else if (instr_count >= 84356) {
                        dotrcmem = dodmpins = dotrcins = true;
                }
                // end check for instruction count
                cpu->cycleLimit = 1;
                run(cpu);
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
                                printf("\tR=%03o N=%d F=%05o TFFF=%u SALF=%u NCSF=%u\n",
                                        cpu->r.R, cpu->r.N, cpu->r.F,
                                        cpu->r.TFFF, cpu->r.SALF, cpu->r.NCSF);
                                printf("\tX=__%014llo %s\n", cpu->r.X, lcw2string(cpu->r.X));
                        } else {
                                printf("\tA=%016llo(%u) GH=%o%o Y=%02o M=%05o F=%05o N=%d NCSF=%u\n",
                                        cpu->r.A, cpu->r.AROF,
                                        cpu->r.G, cpu->r.H,
                                        cpu->r.Y, cpu->r.M,
                                        cpu->r.F,
                                        cpu->r.N, cpu->r.NCSF);
                                printf("\tB=%016llo(%u) KV=%o%o Z=%02o S=%05o R=%03o MSFF=%u SALF=%u\n",
                                        cpu->r.B, cpu->r.BROF,
                                        cpu->r.K, cpu->r.V,
                                        cpu->r.Z, cpu->r.S,
                                        cpu->r.R,
                                        cpu->r.MSFF, cpu->r.SALF);
                        }
                }
                //sleep(1);
        }
        // CPU stopped
        printf("\n\n***** CPU stopped *****\nContinue?  ");
        fgets(linebuf, sizeof linebuf, stdin);
        if (linebuf[0] != 'n')
                goto runagain;
}

int main(int argc, char *argv[])
{
        int opt;
        ADDR15 addr;

        printf("B5500 Card Reader\n");
        diskfile.name = (char*)"./diskfile1.txt";

        while ((opt = getopt(argc, argv, "imsel:c:C:t:T:d:D:")) != -1) {
                switch (opt) {
                case 'i':
                        dodmpins = true; /* dump instructions after assembly */
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
                case 'l':
                        listfile.name = optarg; /* file with listing */
                        break;
                case 'c':
                        cardfile.name = optarg; /* file with cards */
                        break;
                case 'C':
                        cardfile.tracename = optarg; /* trace file for cards */
                        break;
                case 't':
                        tapefile.name = optarg; /* file with tape */
                        break;
                case 'T':
                        tapefile.tracename = optarg; /* trace file for tapes */
                        break;
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
                                "\t-l <file>\tspecify listing file name\n"
                                "\t-c <file>\tspecify card file name\n"
                                "\t-t <file>\tspecify tape file name\n"
                                "\t-d <file>\tspecify disk file name\n"
                                "\t-C <file>\tspecify trace card file name\n"
                                "\t-T <file>\tspecify trace tape file name\n"
                                "\t-D <file>\tspecify trace disk file name\n"
                                , argv[0]);
                        exit(2);
                }
        }
        if (argc - optind != 0) {
                fprintf(stderr, "extra parameters on command line\n");
                exit (2);
        }

        opt = openfile(&listfile, "r");
        if (opt)
                exit(2);

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
                exit(2);

        opt = openfile(&tapefile, "r");
        if (opt)
                exit(2);

        opt = openfile(&diskfile, "r+");
        if (opt)
                exit(2);

        b5500_init_shares();

        memset(MAIN, 0, MAXMEM*sizeof(WORD48));
        cpu = P[0];
        memset(cpu, 0, sizeof(CPU));
        cpu->id = "P1";
        cpu->acc.id = cpu->id;
        cpu->r.US14X = true;
        //cpu->isP1 = true;
        start(cpu);

        // load first card to 020
        addr = 020;
        card_read_binary(&addr);
        execute(020);

        fclose(cardfile.fp);

        return 0;
}
