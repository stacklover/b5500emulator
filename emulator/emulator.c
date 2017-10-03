/***********************************************************************
* b5500emulator
************************************************************************
* Copyright (c) 2017, Reinhard Meyer, DL5UY
* Licensed under the MIT License,
*       see LICENSE
************************************************************************
* b5500 emulator main rountine
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

FILEHANDLE    listfile;         /* file with listing */
FILEHANDLE    spiofile;         /* file with special instruction and I/O trace */

/* input/output buffer */
char    linebuf[MAXLINELENGTH];
char    *linep;

/* NAME entries */
#define MAXNAME 1000
char name[MAXNAME][29];

/* instruction execution counter */
unsigned instr_count;

CPU *cpu;


int openfile(FILEHANDLE *f, const char *mode) {
        if (f->name != NULL) {
                f->fp = fopen(f->name, mode);
                if (f->fp == NULL) {
                        perror(f->name);
                        return 0;
                }
                f->eof = false;
                f->line = 0;
		printf("opened %s\n", f->name);
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
		printf("traces to %s\n", f->tracename);
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
	//printf("runn: C=%05o L=%o T=%04o\n", cpu->r.C, cpu->r.L, cpu->r.T);
        while (cpu->busy) {
                instr_count++;
#if 0
                // check for instruction count
                if (instr_count > 800000) {
                        dotrcmem = dodmpins = dotrcins = false;
                        memdump();
                        closefile(&diskfile);
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

int handle_option(const char *option) {
	if (strncmp(option, "spo", 3) == 0) {
                return spo_init(option); /* console emulation options */
	} else if (strncmp(option, "cr", 2) == 0) {
                return cr_init(option);  /* card reader emulation options */
	} else if (strncmp(option, "mt", 2) == 0) {
                return mt_init(option); /* tape emulation options */
	} else if (strncmp(option, "dk", 2) == 0) {
                return dk_init(option); /* disk file emulation options */
	} else if (strncmp(option, "lp", 2) == 0) {
                return lp_init(option); /* printer emulation options */
	}
	return 1; // WARNING
}

int main(int argc, char *argv[])
{
        int opt;
        ADDR15 addr;

        printf("B5500 Emulator Main Thread\n");

        b5500_init_shares();

        memset(MAIN, 0, MAXMEM*sizeof(WORD48));
        cpu = P[0];
        memset(cpu, 0, sizeof(CPU));
        cpu->id = "P1";
        cpu->acc.id = cpu->id;
        cpu->isP1 = true;

        while ((opt = getopt(argc, argv, "imsezSl:I:")) != -1) {
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
                default: /* '?' */
                        fprintf(stderr,
                                "Usage: %s\n"
                                "\t-i\t\ttrace I/O\n"
                                "\t-m\t\tshow memory accesses\n"
                                "\t-s\t\tlist source cards\n"
                                "\t-e\t\ttrace execution\n"
                                "\t-z\t\tstop at ZPI instruction\n"
                                "\t-S\t\tcard load select\n"
                                "\t-l <file>\tspecify listing file name\n"
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

        while (argc > 1) {
		argv++;
		argc--;
		//printf("argc=%d *argv=%s\n", argc, *argv);
		opt = handle_option(*argv);
		if (opt)
			exit (opt);
        }

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

        opt = openfile(&spiofile, "r");
        if (opt)
                exit(2);

        start(cpu);

        addr = 020; // boot addr
        if (cardload) {
                // binary read first CRA card to <addr>
                cr_read(0240000540000000LL | addr);
        } else {
                // load DKA disk segments 1..63 to <addr>
		MAIN[addr-1] = 1LL;
                dk_access(0140000047700000LL | (addr-1));
        }

        execute(addr);

        return 0;
}
