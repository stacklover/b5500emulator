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
***********************************************************************/

#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include "b5500_common.h"

extern const INSTRUCTION instr[];

#define MAXLABEL	100	/* total possible labels in intermediate */
#define MAXLINELENGTH	80	/* maximum input line length */

/* debug flags: turn these on for various dumps and traces */
int dodmpins	 = false;	/* dump instructions after assembly */
int dolistsource = false;	/* list source line */
int dotrcins	 = false;	/* trace instruction execution */

typedef enum labelst {entered=0, defined} LABELST;
typedef struct labelrec {
	unsigned val;
	LABELST	st;
} LABELREC;

/* variables for input file reading */
FILE	*prd;		/* for assembly source */
int	iline;		/* line number of intermediate file */
char	linebuf[MAXLINELENGTH];
char	*linep;
int	eof;
int	level;
char	opname[20];
char	regname[20];
unsigned wc;
unsigned sc;

/* variables of code generator */
int	pass2;
LABELREC labeltab[MAXLABEL];

CENTRAL_CONTROL cc;
CPU *this;

void signalInterrupt(CPU *this)
{
	printf("\nIRQ=$%02x\n", this->r.I);
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

void errorl(char *msg)
{
	fputs(linebuf, stdout);
	printf("\n*** Program load error: [%d] %s\n", iline, msg);
	fclose(prd);
	exit(2);
}

void setlabel(int x, int value)
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

int getlabel(int x)	/* search in label table */
{
	int q;
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
		MAIN[wc] = (MAIN[wc] & 0x000fffffffff) | ((WORD48)v << 36);
		sc = 1;
		break;
	case 1:
		MAIN[wc] = (MAIN[wc] & 0xfff000ffffff) | ((WORD48)v << 24);
		sc = 2;
		break;
	case 2:
		MAIN[wc] = (MAIN[wc] & 0xffffff000fff) | ((WORD48)v << 12);
		sc = 3;
		break;
	case 3:
		MAIN[wc] = (MAIN[wc] & 0xfffffffff000) | ((WORD48)v << 0);
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
	if (fgets(linebuf, sizeof linebuf, prd) == NULL) {
		eof = true;
		linebuf[0] = 0;
		return;
	}
}

WORD48 parseint(void)
{
	while(isspace((int)*linep))
		linep++;
	if (isdigit((int)*linep) || (*linep == '-'))
		return strtoll(linep, &linep, 0);
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

void printinstr(ADDR15 wc, WORD2 sc, BIT symbolic)
{
	const INSTRUCTION *ip;
	WORD12 code;

	switch (sc) {
	case 0:	code = (MAIN[wc] >> 36) & 0xfff; break;
	case 1:	code = (MAIN[wc] >> 24) & 0xfff; break;
	case 2:	code = (MAIN[wc] >> 12) & 0xfff; break;
	case 3:	code = (MAIN[wc] >> 0) & 0xfff; break;
	}
	printf("%04o", code);
	if (symbolic) {
		// search instruction table
		ip = instr;
		while (ip->name != 0) {
			switch (ip->outtype) {
			case OP_ASIS:
			case OP_BRAS:
			case OP_BRAW:
				if (ip->code == code) {
					printf (" %s", ip->name);
					return;
				}
				break;
			case OP_TOP4:
				if (ip->code == (code & 0x0ff)) {
					printf (" %s %u", ip->name, code >> 8);
					return;
				}
				break;
			case OP_TOP6:
				if (ip->code == (code & 0x03f)) {
					printf (" %s 0%02o", ip->name, code >> 6);
					return;
				}
				break;
			case OP_TOP10:
				if (ip->code == (code & 0x003)) {
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

int verifyreg(char *regname, long long c)
{
	if (strcmp(regname, "AROF") == 0) {
		if (this->r.AROF == c)
			return true;
	} else
	if (strcmp(regname, "BROF") == 0) {
		if (this->r.BROF == c)
			return true;
	} else
	if (strcmp(regname, "A") == 0) {
		if (this->r.A == c)
			return true;
	} else
	if (strcmp(regname, "B") == 0) {
		if (this->r.B == c)
			return true;
	} else
	if (strcmp(regname, "S") == 0) {
		if (this->r.S == c)
			return true;
	}
	return false;
}

void setreg(char *regname, long long c)
{
	if (strcmp(regname, "AROF") == 0) {
		this->r.AROF = c;
	} else
	if (strcmp(regname, "BROF") == 0) {
		this->r.BROF = c;
	} else
	if (strcmp(regname, "A") == 0) {
		this->r.A = c;
	} else
	if (strcmp(regname, "B") == 0) {
		this->r.B = c;
	} else
	if (strcmp(regname, "S") == 0) {
		this->r.S = c;
	}
}

void assemble(void);

int again;
void generate(void)	/* generate segment of code */
{
	int	x;
	int	labelvalue;

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
	ip = instr;
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
	case OP_RELA:
		c = parseint();
		break;
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
			this->r.C = wc;
			this->r.L = sc;
			loadPviaC(this);	// load the program word to P
			switch (this->r.L) {
			case 0:
				this->r.T = (this->r.P >> 36) & 0xfff;
				this->r.L = 1;
				break;
			case 1:
				this->r.T = (this->r.P >> 24) & 0xfff;
				this->r.L = 2;
				break;
			case 2:
				this->r.T = (this->r.P >> 12) & 0xfff;
				this->r.L = 3;
				break;
			case 3:
				this->r.T = (this->r.P >> 0) & 0xfff;
				this->r.L = 0;
				this->r.C++;
				this->r.PROF = false;
				break;
			}
			this->r.TROF = true;

			this->r.US14X = true;
			start(this);
			while (this->busy) {
				if (dotrcins) {
					ADDR15 c;
					WORD2 l;
					c = this->r.C;
					l = this->r.L;
					if (l == 0) {
						l = 3;
						c--;
					} else {
						l--;
					}
					printf("%05o:%o ", c, l);
					printinstr(c, l, true);
					printf("\n");
				}
				this->cycleLimit = 1;
				run(this);
				if (dotrcins) {
					printf("A=%016llo(%u) GH=%o%o Y=%02o M=%05o\n",
						this->r.A, this->r.AROF,
						this->r.G, this->r.H,
						this->r.Y, this->r.M);
					printf("B=%016llo(%u) KV=%o%o Z=%02o S=%05o N=%d\n",
						this->r.B, this->r.BROF,
						this->r.K, this->r.V,
						this->r.Z, this->r.S,
						this->r.N);
				}
				//sleep(1);
			}
			wc = this->r.C;
			sc = this->r.L;
			if (sc == 0) {
				sc = 3;
				wc--;
			} else {
				sc--;
			}
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
				printf("%05o:%o ", oldwc, oldsc);
				if ((oldwc!=wc) || (oldsc!=sc))
					printinstr(oldwc, oldsc, false);
				else
					printf("    ");
			}
			if (dolistsource)
				fputs(linebuf, stdout);
			else
				printf("\n");
		}
	}
}

void load(void)
{
	init();
	generate();
}

int main(int argc, char *argv[])
{
	int opt;

	printf("B5500 Assembler\n");

	while ((opt = getopt(argc, argv, "bst")) != -1) {
		switch (opt) {
		case 'b':
			dodmpins = true; /* dump instructions after assembly */
			break;
		case 's':
			dolistsource = true; /* list source */
			break;
		case 't':
			dotrcins = true; /* trace execution */
			break;
		default: /* '?' */
			fprintf(stderr,
				"Usage: %s [-b] [-s] input\n",
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

	this = CPUA;
	memset(this, 0, sizeof(CPU));
	this->cc = &cc;
	this->id = "CPUA";
	start(this);

	printf("Pass 1\n");
	pass2 = false;
	load();
	printf("Pass 2\n");
	pass2 = true;
	load();

	fclose(prd);

	return 0;
}