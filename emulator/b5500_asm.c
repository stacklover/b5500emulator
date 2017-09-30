/***********************************************************************
* b5500emulator
************************************************************************
* Copyright (c) 2016, Reinhard Meyer, DL5UY
* Licensed under the MIT License,
*       see LICENSE
************************************************************************
* b5500 assembler
************************************************************************
* 2016-02-29  R.Meyer
*   From thin air (based on my Pascal P5 assembler).
* 2017-07-17  R.Meyer
*   Added "long long" qualifier to constants with long long value
*   changed "this" to "cpu" to avoid errors when using g++
***********************************************************************/

#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include "b5500_common.h"

#define MAXLABEL        100     /* total possible labels in intermediate */
#define MAXLINELENGTH   80      /* maximum input line length */

/* debug flags: turn these on for various dumps and traces */
int dodmpins     = false;       /* dump instructions after assembly */
int dotrcmem     = false;       /* trace memory accesses */
int dolistsource = false;       /* list source line */
int dotrcins     = false;       /* trace instruction execution */
int dotrcmat     = false;       /* trace math operations */
int emode        = false;       /* emode math */

typedef enum labelst {entered=0, defined} LABELST;
typedef struct labelrec {
        unsigned val;
        LABELST st;
} LABELREC;

/* variables for input file reading */
FILE    *prd;           /* for assembly source */
int     iline;          /* line number of intermediate file */
char    linebuf[MAXLINELENGTH];
char    *linep;
int     eof;
int     level;
char    opname[20];
char    regname[20];
unsigned wc;
unsigned sc;
unsigned instr_count;

/* variables of code generator */
int     pass2;
LABELREC labeltab[MAXLABEL];

CENTRAL_CONTROL cc;
CPU *cpu;

void signalInterrupt(const char *id, const char *cause)
{
        printf("***** signalInterrupt *****\n");
}

void init(void)
{
        int i;

        wc = 0;
        sc = 0;

        if (pass2 == false) {
                for (i=0; i<MAXLABEL; i++) {
                        labeltab[i].val = -1;
                        labeltab[i].st = entered;
                }
        } else {
                fseek(prd, 0, SEEK_SET);
        }
        eof = false;
}

void errorl(const char *msg)
{
        fputs(linebuf, stdout);
        printf("\n*** Program load error: [%d] %s\n", iline, msg);
        fclose(prd);
        exit(2);
}

void setlabel(int x, unsigned value)
{
        if (pass2) {
                if (labeltab[x].st != defined)
                        errorl("pass2: label not defined");
                /* check whether we get the same value */
                if (labeltab[x].val != value)
                        errorl("pass2: label has different value");
        } else {
                if (labeltab[x].st == defined)
                        errorl("duplicated label");
                labeltab[x].st  = defined;
                labeltab[x].val = value;
        }
}

int getlabel(int x)     /* search in label table */
{
        int q = 0;
        switch (labeltab[x].st) {
        case entered:
                q = labeltab[x].val;
                labeltab[x].val = wc;
                break;
        case defined:
                q = labeltab[x].val;
                break;
        }
        return q;
}

void getname(char *name, int maxlen)
{
        int i=0;
        memset(name, 0, maxlen);
        while (isalnum((int)*linep) || (*linep=='.')) {
                if (i >= maxlen)
                        linep++;
                else
                        name[i++] = *linep++;
        }
}

void storesyllable(WORD12 v)
{
        switch (sc) {
        case 0:
                MAIN[wc] = (MAIN[wc] & 0x000fffffffffll) | ((WORD48)v << 36);
                sc = 1;
                break;
        case 1:
                MAIN[wc] = (MAIN[wc] & 0xfff000ffffffll) | ((WORD48)v << 24);
                sc = 2;
                break;
        case 2:
                MAIN[wc] = (MAIN[wc] & 0xffffff000fffll) | ((WORD48)v << 12);
                sc = 3;
                break;
        case 3:
                MAIN[wc] = (MAIN[wc] & 0xfffffffff000ll) | ((WORD48)v << 0);
                sc = 0;
                wc++;
                break;
        }
}

void storeword(WORD48 v)
{
        if (sc != 0)
                wc++;
        MAIN[wc] = v;
        sc = 0;
        wc++;
}

void getlin(void) /* get next line */
{
        iline++;
        if (fgets(linebuf, sizeof linebuf, prd) == NULL) {
                eof = true;
                linebuf[0] = 0;
                return;
        }
}

const char int2ascii[64+1] =
        "0123456789#@?:>}"
        "+ABCDEFGHI.[&(<~"
        "|JKLMNOPQR$*-);{"
        " /STUVWXYZ,%!=]\"";

WORD48 string(char *lp, char **rp)
{
        WORD48  res = 0;
        char *p;
        lp++;
        while (*lp != '"') {
                p = strchr(int2ascii, *lp);
                if (p)
                        res = (res << 6) | (p - int2ascii);
                else
                        res = (res << 6) | (014);
                lp++;
        }
        *rp = lp;
        return res;
}

char *word2string(WORD48 w)
{
        static char buf[33];
        int i;
        for (i=7; i>=0; i--) {
                buf[3*i  ] = '0'+((w>>3) & 7);
                buf[3*i+1] = '0'+((w   ) & 7);
                buf[3*i+2] = ' ';
                buf[24+i] = int2ascii[w&077];
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

WORD48 parseint(void)
{
        while(isspace((int)*linep))
                linep++;
        if (isdigit((int)*linep) || (*linep == '-'))
                return strtoll(linep, &linep, 0);
        if (*linep == '"')
                return string(linep, &linep);
        errorl("expected integer");
        return -1LL;
}

WORD48 parserel(void)
{
        char ch;
        long long ll;
        while(isspace((int)*linep))
                linep++;
        ch = *linep;
        if (ch == 'R') {
                linep++;
        } else if (ch == 'F') {
                linep++;
        } else if (ch == 'C') {
                linep++;
        }
        if (isdigit((int)*linep) || (*linep == '-') || (*linep == '+')) {
                ll = strtoll(linep, &linep, 0);
                switch (ch) {
                case 'R':
                        return ll;
                case 'F':
                        if (ll < 0)
                                return -ll | 01600;
                        else
                                return ll | 01000;
                case 'C':
                        return ll | 01400;
                default:
                        return ll;
                }
        }
        errorl("expected integer");
        return -1LL;
}

int parselabel(void)
{
        while(isspace((int)*linep))
                linep++;
        if (*linep == 'l' || *linep == 'L')
                return getlabel(strtol(linep+1, &linep, 10));
        errorl("expected label");
        return -1;
}

void printinstr(ADDR15 wc, WORD2 sc, BIT symbolic, BIT cwmf)
{
        const INSTRUCTION *ip;
        WORD12 code = 0;

        switch (sc) {
        case 0: code = (MAIN[wc] >> 36) & 0xfff; break;
        case 1: code = (MAIN[wc] >> 24) & 0xfff; break;
        case 2: code = (MAIN[wc] >> 12) & 0xfff; break;
        case 3: code = (MAIN[wc] >> 0) & 0xfff; break;
        }
        printf("%04o", code);
        if (symbolic) {
                // search instruction table
                ip = instruction_table;
                while (ip->name != 0) {
                        switch (ip->outtype) {
                        case OP_ASIS:
                        case OP_BRAS:
                        case OP_BRAW:
                                if (ip->cwmf == cwmf && ip->code == code) {
                                        printf (" %s", ip->name);
                                        return;
                                }
                                break;
                        case OP_TOP4:
                                if (ip->cwmf == cwmf && ip->code == (code & 0x0ff)) {
                                        printf (" %s %u", ip->name, code >> 8);
                                        return;
                                }
                                break;
                        case OP_TOP6:
                                if (ip->cwmf == cwmf && ip->code == (code & 0x03f)) {
                                        printf (" %s 0%02o", ip->name, code >> 6);
                                        return;
                                }
                                break;
                        case OP_TOP10:
                                if (ip->cwmf == cwmf && ip->code == (code & 0x003)) {
                                        printf (" %s 0%04o", ip->name, code >> 2);
                                        return;
                                }
                                break;
                        default:
                                ;
                        }
                        ip++;
                }
                printf(" unknown instruction!");
        }
}

int verifyreg(char *regname, unsigned long long c)
{
        if (strcmp(regname, "AROF") == 0) { if (cpu->r.AROF == c) return true;  } else
        if (strcmp(regname, "BROF") == 0) { if (cpu->r.BROF == c) return true;  } else
        if (strcmp(regname, "PROF") == 0) { if (cpu->r.PROF == c) return true;  } else
        if (strcmp(regname, "TROF") == 0) { if (cpu->r.TROF == c) return true;  } else
        if (strcmp(regname, "MSFF") == 0) { if (cpu->r.MSFF == c) return true;  } else
        if (strcmp(regname, "SALF") == 0) { if (cpu->r.SALF == c) return true;  } else
        if (strcmp(regname, "NCSF") == 0) { if (cpu->r.NCSF == c) return true;  } else
        if (strcmp(regname, "CWMF") == 0) { if (cpu->r.CWMF == c) return true;  } else
        if (strcmp(regname, "isP1") == 0) { if (cpu->isP1 == c) return true;    } else
        if (strcmp(regname, "A") == 0) { if (cpu->r.A == c) return true; } else
        if (strcmp(regname, "B") == 0) { if (cpu->r.B == c) return true; } else
        if (strcmp(regname, "C") == 0) { if (cpu->r.C == c) return true; } else
        if (strcmp(regname, "E") == 0) { if (cpu->r.E == c) return true; } else
        if (strcmp(regname, "F") == 0) { if (cpu->r.F == c) return true; } else
        if (strcmp(regname, "G") == 0) { if (cpu->r.G == c) return true; } else
        if (strcmp(regname, "H") == 0) { if (cpu->r.H == c) return true; } else
        if (strcmp(regname, "I") == 0) { if (cpu->r.I == c) return true; } else
        if (strcmp(regname, "J") == 0) { if (cpu->r.J == c) return true; } else
        if (strcmp(regname, "K") == 0) { if (cpu->r.K == c) return true; } else
        if (strcmp(regname, "L") == 0) { if (cpu->r.L == c) return true; } else
        if (strcmp(regname, "M") == 0) { if (cpu->r.M == c) return true; } else
        if (strcmp(regname, "N") == 0) { if (cpu->r.N == c) return true; } else
        if (strcmp(regname, "P") == 0) { if (cpu->r.P == c) return true; } else
        if (strcmp(regname, "R") == 0) { if (cpu->r.R == c) return true; } else
        if (strcmp(regname, "S") == 0) { if (cpu->r.S == c) return true; } else
        if (strcmp(regname, "T") == 0) { if (cpu->r.T == c) return true; } else
        if (strcmp(regname, "V") == 0) { if (cpu->r.V == c) return true; } else
        if (strcmp(regname, "X") == 0) { if (cpu->r.X == c) return true; } else
        if (strcmp(regname, "Y") == 0) { if (cpu->r.Y == c) return true; } else
        if (strcmp(regname, "Z") == 0) { if (cpu->r.Z == c) return true; }
        return false;
}

void setreg(char *regname, long long c)
{
        if (strcmp(regname, "AROF") == 0) { cpu->r.AROF = c; } else
        if (strcmp(regname, "BROF") == 0) { cpu->r.BROF = c; } else
        if (strcmp(regname, "PROF") == 0) { cpu->r.PROF = c; } else
        if (strcmp(regname, "TROF") == 0) { cpu->r.TROF = c; } else
        if (strcmp(regname, "MSFF") == 0) { cpu->r.MSFF = c; } else
        if (strcmp(regname, "SALF") == 0) { cpu->r.SALF = c; } else
        if (strcmp(regname, "NCSF") == 0) { cpu->r.NCSF = c; } else
        if (strcmp(regname, "CWMF") == 0) { cpu->r.CWMF = c; } else
        if (strcmp(regname, "isP1") == 0) { cpu->isP1 = c; } else
        if (strcmp(regname, "A") == 0) { cpu->r.A = c;  } else
        if (strcmp(regname, "B") == 0) { cpu->r.B = c;  } else
        if (strcmp(regname, "C") == 0) { cpu->r.C = c;  } else
        if (strcmp(regname, "E") == 0) { cpu->r.E = c;  } else
        if (strcmp(regname, "F") == 0) { cpu->r.F = c;  } else
        if (strcmp(regname, "G") == 0) { cpu->r.G = c;  } else
        if (strcmp(regname, "H") == 0) { cpu->r.H = c;  } else
        if (strcmp(regname, "I") == 0) { cpu->r.I = c;  } else
        if (strcmp(regname, "J") == 0) { cpu->r.J = c;  } else
        if (strcmp(regname, "K") == 0) { cpu->r.K = c;  } else
        if (strcmp(regname, "L") == 0) { cpu->r.L = c;  } else
        if (strcmp(regname, "M") == 0) { cpu->r.M = c;  } else
        if (strcmp(regname, "N") == 0) { cpu->r.N = c;  } else
        if (strcmp(regname, "P") == 0) { cpu->r.P = c;  } else
        if (strcmp(regname, "R") == 0) { cpu->r.R = c;  } else
        if (strcmp(regname, "S") == 0) { cpu->r.S = c;  } else
        if (strcmp(regname, "T") == 0) { cpu->r.T = c;  } else
        if (strcmp(regname, "V") == 0) { cpu->r.V = c;  } else
        if (strcmp(regname, "X") == 0) { cpu->r.X = c;  } else
        if (strcmp(regname, "Y") == 0) { cpu->r.Y = c;  } else
        if (strcmp(regname, "Z") == 0) { cpu->r.Z = c;  }
}

void assemble(void);

int again;
void generate(void)     /* generate segment of code */
{
        int     x;
        int     labelvalue;

        again = true;
        while (again) {
                getlin();
                if (eof) {
                        printf("unexpected eof on input\n");
                        return;
                }
                linep = linebuf+1;
                switch (linebuf[0]) {
                case '#': /* comment */
                        if (pass2 && dolistsource)
                                fputs(linebuf, stdout);
                        break;
                case 'l':
                case 'L': /* label definition */
                        x = strtol(linep, &linep, 10);
                        while(isspace((int)*linep))
                                linep++;
                        if (*linep == '=')
                                labelvalue = strtol(linep+1, &linep, 0);
                        else
                                labelvalue = wc;
                        setlabel(x, labelvalue);
                        if (pass2 && dolistsource)
                                fputs(linebuf, stdout);
                        break;
                case ' ':
                case '\t': /* instruction */
                        while(isspace((int)*linep))
                                linep++;
                        assemble();
                        break;
                }
        }
}

void assemble(void)
{
        const INSTRUCTION *ip;
        WORD12 op = 0;
        long long c;
        unsigned oldwc;
        unsigned oldsc;

        oldwc = wc;
        oldsc = sc;

        getname(opname, sizeof opname);

        /* search for instruction in table */
        ip = instruction_table;
        while (ip->name != 0 && strcmp(opname, ip->name) != 0)
                ip++;
        if (ip->name == 0)
                errorl("illegal instruction");
        op = ip->code;

        /* get parameters */
        c = 0;
        switch (ip->intype) {
        case OP_NONE:
                break;
        case OP_EXPR:
                c = parseint();
                break;
        case OP_RELA:
                c = parserel();
        case OP_BRAS:
        case OP_BRAW:
                break;
        case OP_REGVAL:
                while(isspace((int)*linep))
                        linep++;
                getname(regname, sizeof regname);
                while(isspace((int)*linep))
                        linep++;
                c = parseint();
                break;
        default:
                errorl("unexpected intype");
        }

        /* generate code or handle pseudo instructions */
        switch (ip->outtype) {
        case OP_ORG:
                oldwc = wc = c;
                oldsc = sc = 0;
                if (pass2 && dolistsource)
                        fputs(linebuf, stdout);
                return;
        case OP_RUN:
                if (pass2) {
                        if (dolistsource)
                                fputs(linebuf, stdout);
                        cpu->r.C = wc;
                        cpu->r.L = sc;
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

                        cpu->r.US14X = true;
                        start(cpu);
//printf("runn: C=%05o L=%o T=%04o\n", cpu->r.C, cpu->r.L, cpu->r.T);
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
                                        printf("%05o:%o ", c, l);
                                        printinstr(c, l, true, cpu->r.CWMF);
                                        printf("\n");
                                }
                                cpu->cycleLimit = 1;
                                run(cpu);
                                if (dotrcins) {
                                        if (cpu->r.CWMF) {
                                                printf("  SI=%05o.%o.%o A=%s (%u) Y=%02o\n",
                                                        cpu->r.M, cpu->r.G, cpu->r.H,
                                                        word2string(cpu->r.A), cpu->r.AROF,
                                                        cpu->r.Y);
                                                printf("  DI=%05o.%o.%o B=%s (%u) Z=%02o\n",
                                                        cpu->r.S, cpu->r.K, cpu->r.V,
                                                        word2string(cpu->r.B), cpu->r.BROF,
                                                        cpu->r.Z);
                                                printf("  R=%03o N=%d F=%05o TFFF=%u SALF=%u NCSF=%u %s\n",
                                                        cpu->r.R, cpu->r.N, cpu->r.F,
                                                        cpu->r.TFFF, cpu->r.SALF, cpu->r.NCSF,
                                                        lcw2string(cpu->r.X));
                                        } else {
                                                printf("  A=%016llo(%u) GH=%o%o Y=%02o M=%05o F=%05o N=%d NCSF=%u\n",
                                                        cpu->r.A, cpu->r.AROF,
                                                        cpu->r.G, cpu->r.H,
                                                        cpu->r.Y, cpu->r.M,
                                                        cpu->r.F,
                                                        cpu->r.N, cpu->r.NCSF);
                                                printf("  B=%016llo(%u) KV=%o%o Z=%02o S=%05o R=%03o MSFF=%u SALF=%u\n",
                                                        cpu->r.B, cpu->r.BROF,
                                                        cpu->r.K, cpu->r.V,
                                                        cpu->r.Z, cpu->r.S,
                                                        cpu->r.R,
                                                        cpu->r.MSFF, cpu->r.SALF);
                                        }
                                        for (wc = 01000; wc < 01040; wc++)
                                                if (MAIN[wc] != MAIN[wc+0x4000]) {
                                                        printf("  %05o: %s\n",
                                                                wc, word2string(MAIN[wc]));
                                                        MAIN[wc+0x4000] = MAIN[wc];
                                                }
                                }
//                              sleep(1);
                        }
                        wc = cpu->r.C;
                        sc = cpu->r.L;
                        if (sc == 0) {
                                sc = 3;
                                wc--;
                        } else {
                                sc--;
                        }
                        for (wc = 01000; wc < 01040; wc++)
                                printf("  %05o: %s\n",
                                        wc, word2string(MAIN[wc]));
                }
                return;
        case OP_END:
                again = 0;
                if (pass2 && dolistsource)
                        fputs(linebuf, stdout);
                return;
        case OP_SET:
                if (pass2 && dolistsource)
                        fputs(linebuf, stdout);
                if (pass2)
                        setreg(regname, c);
                return;
        case OP_VFY:
                if (pass2 && dolistsource)
                        fputs(linebuf, stdout);
                if (pass2 && !verifyreg(regname, c)) {
                        printf("*** %s value not as expected\n",
                                regname);
                }
                return;

        case OP_BRAS:
        case OP_BRAW:
        case OP_ASIS:
                storesyllable(op);
                break;
        case OP_TOP4:
                storesyllable(op|c<<8);
                break;
        case OP_TOP6:
                storesyllable(op|c<<6);
                break;
        case OP_TOP10:
                storesyllable(op|c<<2);
                break;
        case OP_WORD:
                storeword(c);
                break;
        case OP_SYLL:
                storesyllable(c);
                break;
        default:
                printf("{%d}", op);
                ;
        }

        if (pass2) {
                if (ip->outtype == OP_WORD) {
                        if (dolistsource)
                                fputs(linebuf, stdout);
                        if (dodmpins)
                                printf("%05o   %016llo\n", wc-1, MAIN[wc-1]);
                } else {
                        if (dodmpins) {
                                printf("%05o:%o (%o) ", oldwc, oldsc, oldwc*4 + oldsc);
                                if ((oldwc!=wc) || (oldsc!=sc))
                                        printinstr(oldwc, oldsc, false, false);
                                else
                                        printf("    ");
                        }
                        if (dolistsource)
                                fputs(linebuf, stdout);
                        if (dodmpins && !dolistsource)
                                printf("\n");
                }
        }
}

void load(void)
{
        init();
        generate();
}

void initiateIO(CPU *cpu){}
void interrogateInterrupt(CPU *cpu){}
WORD48 interrogateUnitStatus(CPU *cpu){return 0;}
WORD48 interrogateIOChannel(CPU *cpu){return 0;}
WORD48 readTimer(CPU *cpu){return 0;}

int main(int argc, char *argv[])
{
        int opt;

        printf("B5500 Assembler\n");

        while ((opt = getopt(argc, argv, "abcmst")) != -1) {
                switch (opt) {
                case 'a':
                        dotrcmat = true; /* trace math */
                        break;
                case 'b':
                        dodmpins = true; /* dump instructions after assembly */
                        break;
                case 'c':
                        emode = true; /* do math the emode way */
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
                default: /* '?' */
                        fprintf(stderr,
                                "Usage: %s [-a] [-b] [-m] [-s] [-t] input\n",
                                argv[0]);
                        exit(2);
                }
        }
        if (argc - optind != 1) {
                fprintf(stderr, "requires 1 filename\n");
                exit (2);
        }
        prd = fopen(argv[optind], "rt");
        if (prd == NULL) {
                fprintf(stderr, "cannot open %s for reading\n", argv[optind]);
                exit (2);
        }

        b5500_init_shares();

        memset(MAIN, 0, MAXMEM*sizeof(WORD48));
        cpu = P[0];
        memset(cpu, 0, sizeof(CPU));
        cpu->id = "P1";
        //cpu->isP1 = true;
        start(cpu);

        printf("Pass 1\n");
        pass2 = false;
        iline = 0;
        load();
        printf("Pass 2\n");
        pass2 = true;
        iline = 0;
        load();

        fclose(prd);

        return 0;
}
