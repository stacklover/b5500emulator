/***********************************************************************
* b5500emulator
************************************************************************
* Copyright (c) 2016, Reinhard Meyer, DL5UY
* Licensed under the MIT License,
*       see LICENSE
************************************************************************
* b5500 single precision math tests
************************************************************************
* 2016-03-12  R.Meyer
*   From thin air (based on the assembler).
* 2017-07-17  R.Meyer
*   Added "long long" qualifier to constants with long long value
***********************************************************************/

#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include "b5500_common.h"

#define MAXLINELENGTH	180	/* maximum input line length */
#define	TEST_ADD	1
#define	TEST_SUB	1
#define	TEST_MUL	1
#define	TEST_FDIV	1
#define	TEST_IDIV	1
#define	TEST_MOD	1

/* debug flags: turn these on for various dumps and traces */
int dodmpins	 = false;	/* dump instructions after assembly */
int dotrcmem	 = false;	/* trace memory accesses */
int dolistsource = false;	/* list source line */
int dotrcins	 = false;	/* trace instruction execution */
int	dotrcmat	 = false;	/* trace math operations */
int	emode		 = false;	/* emode math */

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

CENTRAL_CONTROL cc;
CPU *this;

void signalInterrupt(CPU *this)
{
	//printf("\nIRQ=$%02x\n", this->r.I);
}

void errorl(char *msg)
{
	fclose(prd);
	exit(2);
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

void getlin(void) /* get next line */
{
	if (fgets(linebuf, sizeof linebuf, prd) == NULL) {
		eof = true;
		linebuf[0] = 0;
		return;
	}
}

int parseint(void)
{
	while(isspace((int)*linep))
		linep++;
	if (isdigit((int)*linep) || (*linep == '-'))
		return strtol(linep, &linep, 10);
	errorl("expected dezimal");
	return -1;
}

WORD48 parseoct(void)
{
	while(isspace((int)*linep))
		linep++;
	if (isdigit((int)*linep))
		return strtoll(linep, &linep, 8);
	errorl("expected octal");
	return -1ll;
}

WORD48 V[64];

void load_data(void)
{
	int i, j, test;
	WORD48 a, b, r, c, comp;

	// search for "RAW DATA" first
	do {
		getlin();
		if (eof)
			errorl("unexpected EOF");
	}	while (strncmp(linebuf, "RAW DATA", 8));
	printf("found RAW DATA\n");
	for (i=0; i<64; i++) {
		getlin();
		if (eof)
			errorl("unexpected EOF");
		linep = linebuf;
		test = parseint();
		if (test != i)
			errorl("index mismatch");
		V[i] = parseoct();
		//printf("%2d %016llo\n", i, V[i]);
	}

#if TEST_ADD
	do {
		getlin();
		if (eof)
			errorl("unexpected EOF");
	}	while (strncmp(linebuf, "ADD", 3));
	printf("found ADD\n");
	for (i=0; i<64; i++) {
		for (j=0; j<64; j++) {
			if (j % 6 == 0) {
				getlin();
				if (eof)
					errorl("unexpected EOF");
				linep = linebuf;
			}
			if (j==0) {
				test = parseint();
				if (test != i)
					errorl("index mismatch");
				//printf("%02d", test);
			}
			comp = parseoct();
			// now do the math for ++
			a = V[j];
			b = V[i];
			c = comp;
			this->r.A = a;
			this->r.B = b;
			this->r.AROF = true;
			this->r.BROF = true;
			this->r.NCSF = true;
			singlePrecisionAdd(this, true);
			r = this->r.B;
			if (r != c) {
				printf("%02d,%02d: %016llo+%016llo->%016llo s/b %016llo\n",
					i, j, b, a, r, c);
				errorl("stop");
			}
			// now do the math for --
			a = V[j] ^ MASK_SIGNMANT;
			b = V[i] ^ MASK_SIGNMANT;
			c = comp ^ MASK_SIGNMANT;
			this->r.A = a;
			this->r.B = b;
			this->r.AROF = true;
			this->r.BROF = true;
			this->r.NCSF = true;
			singlePrecisionAdd(this, true);
			r = this->r.B;
			if (r != c) {
				printf("%02d,%02d: %016llo+%016llo->%016llo s/b %016llo\n",
					i, j, b, a, r, c);
				errorl("stop");
			}
		}
	}
#endif

#if TEST_SUB
	do {
		getlin();
		if (eof)
			errorl("unexpected EOF");
	}	while (strncmp(linebuf, "SUB", 3));
	printf("found SUB\n");
	for (i=0; i<64; i++) {
		for (j=0; j<64; j++) {
			if (j % 6 == 0) {
				getlin();
				if (eof)
					errorl("unexpected EOF");
				linep = linebuf;
			}
			if (j==0) {
				test = parseint();
				if (test != i)
					errorl("index mismatch");
				//printf("%02d", test);
			}
			comp = parseoct();
			// now do the math for ++
			a = V[j];
			b = V[i];
			c = comp;
			this->r.A = a;
			this->r.B = b;
			this->r.AROF = true;
			this->r.BROF = true;
			this->r.NCSF = true;
			singlePrecisionAdd(this, false);
			r = this->r.B;
			if (r != c) {
				printf("%02d,%02d: %016llo-%016llo->%016llo s/b %016llo\n",
					i, j, b, a, r, c);
				errorl("stop");
			}
			// now do the math for --
			a = V[j] ^ MASK_SIGNMANT;
			b = V[i] ^ MASK_SIGNMANT;
			if (comp & MASK_MANTISSA)
				c = comp ^ MASK_SIGNMANT;
			else
				c = comp;
			this->r.A = a;
			this->r.B = b;
			this->r.AROF = true;
			this->r.BROF = true;
			this->r.NCSF = true;
			singlePrecisionAdd(this, false);
			r = this->r.B;
			if (r != c) {
				printf("%02d,%02d: %016llo-%016llo->%016llo s/b %016llo\n",
					i, j, b, a, r, c);
				errorl("stop");
			}
		}
	}
#endif

#if TEST_MUL
	do {
		getlin();
		if (eof)
			errorl("unexpected EOF");
	}	while (strncmp(linebuf, "MUL", 3));
	printf("found MUL\n");
	for (i=0; i<64; i++) {
		for (j=0; j<64; j++) {
			if (j % 6 == 0) {
				getlin();
				if (eof)
					errorl("unexpected EOF");
				linep = linebuf;
			}
			if (j==0) {
				test = parseint();
				if (test != i)
					errorl("index mismatch");
				//printf("%02d", test);
			}
			comp = parseoct();
			// now do the math for ++
			a = V[j];
			b = V[i];
			c = comp;
			this->r.A = a;
			this->r.B = b;
			this->r.AROF = true;
			this->r.BROF = true;
			this->r.NCSF = true;
			singlePrecisionMultiply(this);
			r = this->r.B;
			if (r != c) {
				printf("%02d,%02d: %016llo*%016llo->%016llo s/b %016llo\n",
					i, j, b, a, r, c);
				errorl("stop");
			}
		}
	}
#endif

#if TEST_FDIV
	do {
		getlin();
		if (eof)
			errorl("unexpected EOF");
	}	while (strncmp(linebuf, "F-DIV", 5));
	printf("found F-DIV\n");
	for (i=0; i<64; i++) {
		for (j=0; j<64; j++) {
			if (j % 6 == 0) {
				getlin();
				if (eof)
					errorl("unexpected EOF");
				linep = linebuf;
			}
			if (j==0) {
				test = parseint();
				if (test != i)
					errorl("index mismatch");
				//printf("%02d", test);
			}
			comp = parseoct();
			// now do the math for ++
			a = V[j];
			b = V[i];
			c = comp;
			this->r.A = a;
			this->r.B = b;
			this->r.AROF = true;
			this->r.BROF = true;
			this->r.NCSF = true;
			singlePrecisionDivide(this);
			if (this->r.I) {
				this->r.I = 0;
				r = 02000000000000000ll;
			} else {
				r = this->r.B;
			}
			if (r != c) {
				printf("%02d,%02d: %016llo/%016llo->%016llo s/b %016llo\n",
					i, j, b, a, r, c);
				errorl("stop");
			}
		}
	}
#endif

#if TEST_IDIV
	do {
		getlin();
		if (eof)
			errorl("unexpected EOF");
	}	while (strncmp(linebuf, "I-DIV", 5));
	printf("found I-DIV\n");
	for (i=0; i<64; i++) {
		for (j=0; j<64; j++) {
			if (j % 6 == 0) {
				getlin();
				if (eof)
					errorl("unexpected EOF");
				linep = linebuf;
			}
			if (j==0) {
				test = parseint();
				if (test != i)
					errorl("index mismatch");
				//printf("%02d", test);
			}
			comp = parseoct();
			// now do the math for ++
			a = V[j];
			b = V[i];
			c = comp;
			this->r.A = a;
			this->r.B = b;
			this->r.AROF = true;
			this->r.BROF = true;
			this->r.NCSF = true;
			integerDivide(this);
			if (this->r.I) {
				this->r.I = 0;
				r = 02000000000000000ll;
			} else {
				r = this->r.B;
			}
			if (r != c) {
				printf("%02d,%02d: %016llo/%016llo->%016llo s/b %016llo\n",
					i, j, b, a, r, c);
				errorl("stop");
			}
		}
	}
#endif

#if TEST_MOD
	do {
		getlin();
		if (eof)
			errorl("unexpected EOF");
	}	while (strncmp(linebuf, "MOD", 3));
	printf("found MOD\n");
	for (i=0; i<64; i++) {
		for (j=0; j<64; j++) {
			if (j % 6 == 0) {
				getlin();
				if (eof)
					errorl("unexpected EOF");
				linep = linebuf;
			}
			if (j==0) {
				test = parseint();
				if (test != i)
					errorl("index mismatch");
				//printf("%02d", test);
			}
			comp = parseoct();
			// now do the math for ++
			a = V[j];
			b = V[i];
			c = comp;
			this->r.A = a;
			this->r.B = b;
			this->r.AROF = true;
			this->r.BROF = true;
			this->r.NCSF = true;
			remainderDivide(this);
			if (this->r.I) {
				this->r.I = 0;
				r = 02000000000000000ll;
			} else {
				r = this->r.B;
			}
			if (r != c) {
				printf("%02d,%02d: %016llo/%016llo->%016llo s/b %016llo\n",
					i, j, b, a, r, c);
				errorl("stop");
			}
		}
	}
#endif
}

int main(int argc, char *argv[])
{
	int opt;

	printf("Single Precision Math Test\n");

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
				"Usage: %s [-a] [-b] [-c] [-m] [-s] [-t] input\n",
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
	this->id = "CPUA";

	if (emode)
		printf("Testing in EMODE\n");
	else
		printf("Testing in B5500 mode\n");

	load_data();

	fclose(prd);

	return 0;
}
