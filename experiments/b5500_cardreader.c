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

#define MAXLINELENGTH   (256) /* maximum line length for all devices */

/* debug flags: turn these on for various dumps and traces */
int dodmpins     = false;       /* dump instructions after assembly */
int dotrcmem     = false;       /* trace memory accesses */
int dolistsource = false;       /* list source line */
int dotrcins     = false;       /* trace instruction execution */
// never set
int dotrcmat     = false;       /* trace math operations */
int emode        = false;       /* emode math */

/* variables for input file reading */
FILE    *prd;           /* file with cards */
int     iline;          /* line number in card file */
char    linebuf[MAXLINELENGTH];
char    *linep;
BIT     eof;

/* NAME entries */
#define MAXNAME 500
char name[MAXNAME][9];

void errorl(const char *msg) {
        fputs(linebuf, stdout);
        printf("\n*** Card read error at line %d: %s\n", iline, msg);
        fclose(prd);
        exit(2);
}

void signalInterrupt(CPU *cpu)
{
        printf("\nIRQ=$%02x\n", cpu->r.I);
}

void getlin(void) { /* get next line */
        if (eof)
                return;
        iline++;
        if (fgets(linebuf, sizeof linebuf, prd) == NULL) {
                eof = true;
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
        acc.id = "CRA";
        acc.MAIL = false;
        getlin();
        if (eof)
                return false;
        if (dolistsource)
                printf("*\tCRA: %s\n", linebuf);
        if (strlen(linebuf) > (unsigned)chars) {
                printf("*\tNOTE: alpha card line too long\n");
                exit(0);
        }
        if (strlen(linebuf) < (unsigned)chars) {
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
        acc.id = "CRA";
        acc.MAIL = false;
        getlin();
        if (eof)
                return false;
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
        sleep(1);
}

FILE *diskfile = NULL;

WORD48 disk_access(WORD48 iocw) {
        unsigned unit, count, segcnt, words;
        ADDR15 core;
        WORD48 result = (iocw & ~(MASK_IODRESULT)) | MASK_IODRESNRDY;
        ACCESSOR acc;
        WORD48 diskfileaddr;
        unsigned bindiskfileaddr;
        int i;

        if (diskfile == NULL) {
                diskfile = fopen("./diskfile1.txt", "r+");
                if (diskfile == NULL) {
                        perror("cannot open diskfile1.txt");
                        exit(0);
                }
        }

        acc.id = "DF1";
        acc.MAIL = false;
        unit = (iocw & MASK_IODUNIT) >> SHFT_IODUNIT;
        count = (iocw & MASK_IODWC) >> SHFT_IODWC;
        segcnt = (iocw & MASK_IODSEGCNT) >> SHFT_IODSEGCNT;
        core = (iocw & MASK_IODADDR) >> SHFT_IODADDR;
        acc.addr = core++;
        fetch(&acc);
        diskfileaddr = acc.word & MASK_DSKFILADR;
        bindiskfileaddr = 0;
        for (i=6; i>=0; i--) {
                bindiskfileaddr *= 10;
                bindiskfileaddr += ((diskfileaddr >> 6*i) & 017);
        }
        printf("*\txx%014llo -> %u\n", diskfileaddr, bindiskfileaddr);
        // number of words to do
        words = (iocw & MASK_IODUSEWC) ? count : segcnt * 30;
        printf("*\t%u words\n", words);

        if (iocw & MASK_IODREAD) {
                while (words > 0) {
                        fseek(diskfile, bindiskfileaddr*256, SEEK_SET);
                        fread(linebuf, sizeof(char), 256, diskfile);
                        bindiskfileaddr++;
                        linep = linebuf;
                        puts(linebuf);
                        linep += 10;
                        for (i=0; i<30; i++) {
                                if (words > 0) {
                                        acc.addr = core++;
                                        acc.word = getword(0);
                                        words--;
                                        store(&acc);
                                }
                        }
                }
        } else {
                while (words > 0) {
                        linep = linebuf;
                        linep += sprintf(linep, "%08u: ", bindiskfileaddr);
                        for (i=0; i<30; i++) {
                                if (words > 0) {
                                        acc.addr = core++;
                                        words--;
                                        fetch(&acc);
                                        putword(acc.word);
                                } else {
                                        putword(0ll);
                                }
                        }
                        linep += sprintf(linep, "     \n");
                        if (linep != linebuf+256) {
                                printf("linp error\n");
                                exit(0);
                        }
                        fseek(diskfile, bindiskfileaddr*256, SEEK_SET);
                        fwrite(linebuf, sizeof(char), 256, diskfile);
                        puts(linebuf);
                        bindiskfileaddr++;
                }
        }
        fclose(diskfile);
        diskfile = NULL;
        result = (result & ~(MASK_IODADDR | MASK_IODRESULT)) | (words << SHFT_DDWC) | (core << SHFT_IODADDR);
        return result;
}

WORD48 iohandler(WORD48 iocw) {
        unsigned unit, count;
        ADDR15 core;
        // report not ready
        WORD48 result = (iocw & ~(MASK_IODRESULT)) | MASK_IODRESNRDY;
        // zero out fields that are filled later

        unit = (iocw & MASK_IODUNIT) >> SHFT_IODUNIT;
        count = (iocw & MASK_IODWC) >> SHFT_IODWC;
        core = (iocw & MASK_IODADDR) >> SHFT_IODADDR;

        printf("*\tunit=%u count=%u addr=%05o", unit, count, core);
        if (iocw & MASK_IODMI) printf(" mem_inhibit");
        if (iocw & MASK_IODBINARY) printf(" binary"); else printf(" alpha");
        if (iocw & MASK_IODTAPEDIR)  printf(" reverse");
        if (iocw & MASK_IODUSEWC) printf(" use_word_counter");
        if (iocw & MASK_IODREAD) printf(" read"); else printf(" write");
        if (iocw & MASK_IODSEGCNT) printf(" segments=%llu", (iocw & MASK_IODSEGCNT) >> SHFT_IODSEGCNT);
        printf("\n");

        switch (unit) {
        case 6:  // DF1
                result = disk_access(iocw);
                break;
        case 10: // CRA CPA
                if ((iocw & MASK_IODMI) != 0 || (iocw & MASK_IODREAD) == 0)
                        break;
                if (iocw & MASK_IODBINARY) {
                        if (card_read_binary(&core)) {
                                result = (result & ~(MASK_IODADDR | MASK_IODRESULT)) | (core << SHFT_IODADDR);
                                break;
                        }
                } else {
                        if (card_read_alpha(&core)) {
                                result = (result & ~(MASK_IODADDR | MASK_IODRESULT)) | (core << SHFT_IODADDR);
                                break;
                        }
                }
                break;
        case 30: // SPO
                if ((iocw & MASK_IODREAD) != 0)
                        break;
                spo_write(&core);
                result = (result & ~(MASK_IODADDR | MASK_IODRESULT)) | (core << SHFT_IODADDR);
                break;
        }

        // error ?
        if (result & MASK_IODRESNRDY)
                exit(0);
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
                printf("%s+%04o (%05o:%o) ", name[bestmatch], ((c - bestaddr) << 2) + l, c, l);
        else
                printf("(%05o:%o) ", c, l);
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

        cpu->r.US14X = false;
        start(cpu);
printf("runn: C=%05o L=%o T=%04o\n", cpu->r.C, cpu->r.L, cpu->r.T);
        while (cpu->busy) {
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
                                printf("\tR=%03o N=%d F=%05o TFFF=%u SALF=%u NCSF=%u %s\n",
                                        cpu->r.R, cpu->r.N, cpu->r.F,
                                        cpu->r.TFFF, cpu->r.SALF, cpu->r.NCSF,
                                        lcw2string(cpu->r.X));
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
}

int main(int argc, char *argv[])
{
        int opt;
        ADDR15 addr;
        char *listingfile = NULL;

        printf("B5500 Card Reader\n");

        while ((opt = getopt(argc, argv, "bmstl:")) != -1) {
                switch (opt) {
                case 'b':
                        dodmpins = true; /* dump instructions after assembly */
                        break;
                case 'm':
                        dotrcmem = true; /* trace memory accesses */
                        break;
                case 's':
                        dolistsource = true; /* list source */
                        break;
                case 't':
                        dotrcins = true; /* trace execution */
                        break;
                case 'l':
                        listingfile = optarg; /* file with listing */
                        break;
                default: /* '?' */
                        fprintf(stderr,
                                "Usage: %s [-b] [-m] [-s] [-t] input\n",
                                argv[0]);
                        exit(2);
                }
        }
        if (argc - optind != 1) {
                fprintf(stderr, "requires 1 filename\n");
                exit (2);
        }

        // optional listing file parsing
        if (listingfile) {
        prd = fopen(listingfile, "rt");
                if (prd == NULL) {
                        fprintf(stderr, "cannot open %s for reading\n", argv[optind]);
                        exit (2);
                }
                eof = false;
                iline = 0;
                while (!eof) {
                        getlin();
                        //printf("LIST: {%s}\n", linep);
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
                fclose(prd);
        }

        prd = fopen(argv[optind], "rt");
        if (prd == NULL) {
                fprintf(stderr, "cannot open %s for reading\n", argv[optind]);
                exit (2);
        }

        b5500_init_shares();

        eof = false;
        iline = 0;

        memset(MAIN, 0, MAXMEM*sizeof(WORD48));
        cpu = P[0];
        memset(cpu, 0, sizeof(CPU));
        cpu->id = "P1";
        cpu->acc.id = cpu->id;
        //cpu->isP1 = true;
        start(cpu);

        // load first card to 020
        addr = 020;
        card_read_binary(&addr);
        execute(020);

        fclose(prd);

        return 0;
}
