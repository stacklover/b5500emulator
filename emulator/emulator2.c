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
#include <signal.h>
#include <time.h>
#include "common.h"
#include "io.h"

#ifdef USECAN
#include <linux/can.h>
#include <linux/can/raw.h>
#include <linux/can/error.h> 
#include "canlib.h"
#endif

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
unsigned iar_count;
FILE *tracefp = stdout;

CPU *cpu;


/***********************************************************************
* open a text and/or trace file
***********************************************************************/
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

/***********************************************************************
* close a text file and a trace file
***********************************************************************/
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

/***********************************************************************
* get next text line from a file
***********************************************************************/
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

/***********************************************************************
* read the current timer value
***********************************************************************/
WORD48 readTimer(CPU *cpu) {
        WORD48 result = 0;

        if (CC->CCI03F)
                result =  CC->TM | 0100;
        else
                result =  CC->TM;
        return result;
}

/***********************************************************************
* convert a relative address into a string
* warning: static buffer, not thread save!
***********************************************************************/
const char *relsym(unsigned offset, BIT cEnabled) {
        static char buf[32];
        if (cpu->bSALF) {
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
                                if (MAIN[(cpu->rR) + offset] & MASK_FLAG)
                                        sprintf(buf, "%s=%05llo", name[offset], MAIN[(cpu->rR) + offset] & MASK_ADDR);
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
                        if (cpu->bMSFF) {
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
                        if (cpu->bMSFF) {
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

/***********************************************************************
* find and print the best matching label for a given code address
***********************************************************************/
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
                fprintf(tracefp, "%08u %s+%04o (%05o:%o) ",
			instr_count, name[bestmatch],
			((c - bestaddr) << 2) + l, c, l);
        else
                fprintf(tracefp, "%08u (%05o:%o) ",
			instr_count, c, l);
}

/***********************************************************************
* disassemble one instruction
***********************************************************************/
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
                                fprintf(tracefp, "%-4.4s        %04o",
					ip->name, code);
                                return;
                        }
                        break;
                case OP_TOP4:
                        if (ip->cwmf == cwmf && ip->code == (code & 0x0ff)) {
                                fprintf(tracefp, "%-4.4s  %4u  %04o",
					ip->name, code >> 8, code);
                                return;
                        }
                        break;
                case OP_TOP6:
                        if (ip->cwmf == cwmf && ip->code == (code & 0x03f)) {
                                fprintf(tracefp, "%-4.4s  %02o    %04o",
					ip->name, code >> 6, code);
                                return;
                        }
                        break;
                case OP_TOP10:
                        if (ip->cwmf == cwmf && ip->code == (code & 0x003)) {
                                fprintf(tracefp, "%-4.4s  %04o  %04o  (%s)",
					ip->name, code >> 2, code,
					relsym(code >> 2, true));
                                return;
                        }
                        break;
                default:
                        ;
                }
                ip++;
        }
        fprintf(tracefp, "unknown instruction %04o", code);
}

/***********************************************************************
* convert a machine word into 8 two digit octal value and into
* 8 character ASCII string: "OO OO OO OO OO OO OO OO AAAAAAAA"
* warning: static buffer, not thread save!
***********************************************************************/
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

/***********************************************************************
* convert a Loop Control Word into a string
* warning: static buffer, not thread save!
***********************************************************************/
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

/***********************************************************************
* do a complete core momory dump in readable text form
***********************************************************************/
void memdump(CPU *cpu) {
        WORD48 w;
        ADDR15 memaddr = 0;
        char buf[132];
        char *bufp;
        FILE *mfp = fopen("coredump.txt", "w");
        if (mfp == NULL) {
                perror("coredump.txt");
                exit(2);
        }

	if (cpu) {
		fprintf(mfp, "\tA=%016llo(%u) GH=%02o Y=%02o M=%05o F=%05o N=%d NCSF=%u T=%04o\n",
			cpu->rA, cpu->bAROF,
			cpu->rGH,
			(WORD6)cpu->rY, cpu->rM,
			cpu->rF,
			cpu->rN, cpu->bNCSF, cpu->rT);
		fprintf(mfp, "\tB=%016llo(%u) KV=%02o Z=%02o S=%05o R=%05o MSFF=%u SALF=%u\n",
			cpu->rB, cpu->bBROF,
			cpu->rKV,
			cpu->rZ, cpu->rS,
			cpu->rR,
			cpu->bMSFF, cpu->bSALF);
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

/***********************************************************************
* Starts the processor
***********************************************************************/
void start(CPU *cpu)
{
	prepMessage(cpu); printf("start\n");
	cpu->bHLTF = false;
}

/***********************************************************************
* Stops running the processor
***********************************************************************/
void stop(CPU *cpu)
{
	prepMessage(cpu); printf("stop\n");
	cpu->rT = 0;
	cpu->bTROF = false;	// idle the processor
	cpu->bPROF = false;
	cpu->bHLTF = true;
}

/***********************************************************************
* Presets the processor registers for a start at C=runAddr
***********************************************************************/
void preset(CPU *cpu, ADDR15 runAddr)
{
	prepMessage(cpu); printf("preset to %05o\n", runAddr);
        cpu->rC = runAddr;
        cpu->rL = 0;
        cpu->rT = 0;
        cpu->bPROF = false;	// cause memory read
        cpu->bTROF = false;	// cause instrction fetch
        cpu->rR = 0;
        cpu->rS = 0;
	cpu->bHLTF = true;
}

/***********************************************************************
* Instruction execution driver for the B5500 processor.
* This routine assumes the C:L registers are set up.
* if TROF is set an instruction must be in T
* and C:L must point to the next instruction.
* if PROF is set, the current program word must be in P.
***********************************************************************/
void run(CPU *cpu)
{
	// execute one instruction
	sim_instr(cpu);

#if 0
	// check for any registers gone wild
#define CHECK(R,M,T) if((cpu->rR) & ~(M))printf("*\tCHECK "T" = %llo\n", (WORD48)(cpu->rR))
	CHECK(A, MASK_WORD48, "A");
	CHECK(B, MASK_WORD48, "B");
	CHECK(C, MASK_ADDR15, "C");
	CHECK(F, MASK_ADDR15, "F");
	CHECK(M, MASK_ADDR15, "M");
	CHECK(P, MASK_WORD48, "P");
	CHECK(R, MASK_ADDR15, "R");
	CHECK(S, MASK_ADDR15, "S");
#endif

	// check for IAR not serviced for nnn instructions
	if (CC->IAR) {
		iar_count++;
		if (iar_count == 100000) {
			// switch on instruction trace
			dotrcins = true;
			tracefp = fopen("instrace.txt", "w");
		}
		if (iar_count == 110000) {
			// do a memory dump and exit
			memdump(cpu);
			stop(cpu);
		}
	} else {
		iar_count = 0;
	}
}

/***********************************************************************
* called back from emulator code when an instruction has been fetched
* into T
***********************************************************************/
void sim_traceinstr(CPU *cpu) {
	if (dotrcins) {
		ADDR15 c;
		WORD2 l;
		c = cpu->rC;
		l = cpu->rL;
		if (l == 0) {
			l = 3;
			c--;
		} else {
			l--;
		}
		// start a new line and print a symbolic name (if available)
		// and the raw C:L values
		fprintf(tracefp, "\n");
		codesym(c, l);
		// print the instruction itself
		printinstr(cpu->rT, cpu->bCWMF);
		// end the line
		fprintf(tracefp, "\n");
	}
}

/***********************************************************************
* print all useful registers
* word mode and char mode are different printouts
***********************************************************************/
void sim_printregs(CPU *cpu) {
	if (cpu->bCWMF) {
		fprintf(tracefp, "\tSI(M:GH)=%05o:%02o A=%s (%u) Y=%02o\n",
			cpu->rM, cpu->rGH,
			word2string(cpu->rA), cpu->bAROF,
			(WORD6)cpu->rY);
		fprintf(tracefp, "\tDI(S:KV)=%05o:%02o B=%s (%u) Z=%02o\n",
			cpu->rS, cpu->rKV,
			word2string(cpu->rB), cpu->bBROF,
			cpu->rZ);
		fprintf(tracefp, "\tR=%05o N=%d F=%05o TFFF=%u SALF=%u NCSF=%u T=%04o\n",
			cpu->rR, cpu->rN, cpu->rF,
			cpu->bTFFF, cpu->bSALF, cpu->bNCSF, cpu->rT);
		fprintf(tracefp, "\tX=__%014llo %s\n", cpu->rX, lcw2string(cpu->rX));
	} else {
		fprintf(tracefp, "\tA=%016llo(%u) GH=%02o Y=%02o M=%05o F=%05o N=%d NCSF=%u T=%04o\n",
			cpu->rA, cpu->bAROF,
			cpu->rGH,
			(WORD6)cpu->rY, cpu->rM,
			cpu->rF,
			cpu->rN, cpu->bNCSF, cpu->rT);
		fprintf(tracefp, "\tB=%016llo(%u) KV=%02o Z=%02o S=%05o R=%05o MSFF=%u SALF=%u\n",
			cpu->rB, cpu->bBROF,
			cpu->rKV,
			cpu->rZ, cpu->rS,
			cpu->rR,
			cpu->bMSFF, cpu->bSALF);
	}
}

/***********************************************************************
* excecute instructions until halted
***********************************************************************/
void execute(ADDR15 addr) {
	preset(cpu, addr);

runagain:
        start(cpu);

        while (!cpu->bHLTF) {
                instr_count++;

                run(cpu);
		if (dotrcins)
			sim_printregs(cpu);
        }

        // CPU halted
        printf("\n\n***** CPU HALT *****\nContinue?  ");
        (void)fgets(linebuf, sizeof linebuf, stdin);
        if (linebuf[0] != 'n')
                goto runagain;
}

/***********************************************************************
* command parser
***********************************************************************/
int command_parser(const command_t *table, const char *op) {
	const char *delim;
	const char *equals;
	int wlen, res;
	char buffer[80];
	const command_t *tp;

next:
	// remove leading blanks
	while (*op == ' ')
		op++;
	while (*op != 0) {
		// partition options into words
		delim = strchrnul(op, ' ');
		equals = strchrnul(op, '=');
		if (equals >= delim) {
			// single command without assignment
			wlen = delim - op;
		} else {
			// with assigment
			wlen = equals - op;
		}
		// search table for command
		for (tp = table; tp->cmd; tp++) {
			if ((int)strlen(tp->cmd) == wlen && strncasecmp(tp->cmd, op, wlen) == 0) {
				// found the command, prepare parameter
				memset(buffer, 0, sizeof buffer);
				if (delim > equals) {
					if (delim - equals >= (int)sizeof buffer) {
						printf("argument too long\n");
						return 2; // FATAL
					}
					memcpy(buffer, equals+1, delim - (equals+1));
				}
				// execute command if it has a function
				if (tp->func)
					res = (tp->func)(buffer, tp->data);
				else
					res = 0;
				if (res)
					return res; // some error
				op = delim;
				goto next;
			}
		}
		// no match
		printf("unknown command\n");
		return 2; // FATAL
	}
	return 0; // OK
}

/***********************************************************************
* handle an option/command by passing it to the proper "unit"
***********************************************************************/
int handle_option(const char *option) {
	if (strncasecmp(option, "spo", 3) == 0) {
                return spo_init(option); /* console emulation options */
	} else if (strncasecmp(option, "cr", 2) == 0) {
                return cr_init(option);  /* card reader emulation options */
	} else if (strncasecmp(option, "cp", 2) == 0) {
                return cp_init(option);  /* card punch emulation options */
	} else if (strncasecmp(option, "mt", 2) == 0) {
                return mt_init(option); /* tape emulation options */
	} else if (strncasecmp(option, "dr", 2) == 0) {
                return dr_init(option); /* magnetic drum emulation options */
	} else if (strncasecmp(option, "dk", 2) == 0) {
                return dk_init(option); /* disk file emulation options */
	} else if (strncasecmp(option, "lp", 2) == 0) {
                return lp_init(option); /* printer emulation options */
	} else if (strncasecmp(option, "dcc", 3) == 0) {
                return dcc_init(option); /* data communications emulation options */
	} else if (strncasecmp(option, "io", 2) == 0) {
                return io_init(option); /* central control options */
	}
	printf("unknown component\n");
	return 1; // WARNING
}

/***********************************************************************
* 60 Hz timer variables
***********************************************************************/
timer_t timerid;
struct sigevent sev;
struct itimerspec its;
long long freq_nanosecs;
sigset_t mask;
struct sigaction sa;
extern void timer60hz(sigval);

/***********************************************************************
* the MAIN program
***********************************************************************/
int main(int argc, char *argv[])
{
	FILE *inifile = NULL;
        int opt;
        ADDR15 addr;

        printf("B5500 Emulator\n");

        b5500_init_shares();

        memset((void*)MAIN, 0, MAXMEM*sizeof(WORD48));

	// P2
        cpu = P[1];
        memset((void*)cpu, 0, sizeof(CPU));
        strcpy((char*)cpu->id, "P2");
        cpu->acc.id = cpu->id;
        cpu->isP1 = false;

	// P1
        cpu = P[0];
        memset((void*)cpu, 0, sizeof(CPU));
        strcpy((char*)cpu->id, "P1");
        cpu->acc.id = cpu->id;
        cpu->isP1 = true;

	// clear CC
	memset((void*)CC, 0, sizeof(*CC));

	// make sure P2 is not used
	CC->P2BF = true;
	CC->HP2F = true;

        // check translate tables bic2ascii and ascii2bic for consistency
        for (addr=0; addr<64; addr++) {
                if (translatetable_ascii2bic[translatetable_bic2ascii[addr]] != addr)
                        printf("tranlatetable error at bic=%02o\n", addr);
        }

        while ((opt = getopt(argc, argv, "i:msezl:I:")) != -1) {
                switch (opt) {
                case 'i':
                        inifile = fopen(optarg, "r"); /* ini file */
                        break;
                case 'm':
                        dotrcmem = true; /* trace memory accesses */
                        break;
                case 's':
                        dolistsource = true; /* list source */
                        break;
                case 'e':
                        dotrcins = true; /* trace execution */
			tracefp = fopen("instrace.txt", "w");
                        break;
                case 'z':
                        cpu->bUS14X = true; /* stop on ZPI */
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
                                "\t-i <file>\tspecify init file\n"
                                "\t-m\t\tshow memory accesses\n"
                                "\t-s\t\tlist source cards\n"
                                "\t-e\t\ttrace execution\n"
                                "\t-z\t\tstop at ZPI instruction\n"
                                "\t-l <file>\tspecify listing file name\n"
                                "\t-I <file>\tspecify I/O and special instruction trace file name\n"
                                , argv[0]);
                        exit(2);
                }
        }

#ifdef USECAN
	// init canbus
	can_init("can1");
#endif
	io_init("");

	// handle init file first
        if (inifile) {
		char buf[81], *p;
                while (fgets(buf, 81, inifile)) {
			// remove trailing control codes
			p = buf + strlen(buf);
			while (p >= buf && *p <= ' ')
				*p-- = 0;
			p = buf;
			// remove leading spaces
			while (*p == ' ')
				p++;
			// ignore empty lines
			if (*p == 0)
				continue;
			printf("=IF %s\n", p);
			if (*p != '#') {
				opt = handle_option(p);
				if (opt)
					exit(opt);
			}
                }
		fclose(inifile);
		inifile = NULL;
        }

	// the command line options
        while (optind < argc) {
		printf("=CL %s\n", argv[optind]);
		opt = handle_option(argv[optind++]);
		if (opt)
			exit(opt);
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

	// Create the timer
	sev.sigev_notify = SIGEV_THREAD;
	sev.sigev_notify_function = timer60hz;
	sev.sigev_value.sival_int = 0;
	if (timer_create(CLOCK_MONOTONIC, &sev, &timerid) == -1) {
		perror("timer_create");
		exit(2);
	}
	//printf("timer ID is 0x%lx\n", (long) timerid);

	// Start the timer
	its.it_value.tv_sec = 0;
	its.it_value.tv_nsec = 16666667; // 16.666667ms ~ 60 Hz
	its.it_interval.tv_sec = its.it_value.tv_sec;
	its.it_interval.tv_nsec = its.it_value.tv_nsec;
	if (timer_settime(timerid, 0, &its, NULL) == -1) {
		perror("timer_settime");
		exit(2);
	}

	addr = 020;

	io_ipl(addr);

        execute(addr);

	printf("end of emulation\n");

        return 0;
}

#if DEBUG305
// this for MCP XIII endless loop debugging
void trap305(CPU *cpu) {
	char buf[200];
	ADDR15 c = cpu->rC;
	WORD2 l = cpu->rL;
        unsigned bestmatch = 0;
        unsigned index;
        ADDR15 addr, bestaddr = 0;

	if (l > 0)
		l--;
	else {
		c--;
		l = 3;
	}

        // search PRT
        for (index = 0200; index<=0725; index++) {
                if ((MAIN[index] & (MASK_FLAG | MASK_CODE | MASK_PBIT)) == (MASK_FLAG | MASK_CODE | MASK_PBIT)) {
                        // present program descriptor
                        addr = MAIN[index] & MASK_ADDR;
                        if (addr <= c && bestaddr < addr) {
                                bestmatch = index;
                                bestaddr = addr;
                        }
                }
        }
        if (bestmatch > 0)
                sprintf(buf, "%05o:%o (PRT[%3o]+%04o) @305 changed: @%016llo",
			c, l, bestmatch, ((c - bestaddr) << 2) + l, MAIN[0305]);
        else
                sprintf(buf, "%05o:%o @305 changed: @%016llo",
			c, l, MAIN[0305]);

	spo_debug_write(buf);
}
#endif

