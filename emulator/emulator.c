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


WORD48 unitsready;

/*
 * table of I/O units
 * indexed by unit designator and read bit of I/O descriptor
 */
UNIT unit[32][2] = {
	/*NO     NAME RDYBIT INDEX READYF    WRITEF    NULL     NAME RDYBIT INDEX READYF    READF     BOOTF */
        /*00*/ {{NULL, 0, 0},                                  {NULL, 0, 0}},
        /*01*/ {{"MTA", 47-47, 0, mt_ready, mt_access, NULL},  {"MTA", 47-47, 0, mt_ready, mt_access, NULL}},
        /*02*/ {{NULL, 0, 0},                                  {NULL, 0, 0}},
        /*03*/ {{"MTB", 47-46, 1, mt_ready, mt_access, NULL},  {"MTB", 47-46, 1, mt_ready, mt_access, NULL}},
        /*04*/ {{"DRA", 47-31, 0},                             {"DRA", 47-31, 0}},         
        /*05*/ {{"MTC", 47-45, 2, mt_ready, mt_access, NULL},  {"MTC", 47-45, 2, mt_ready, mt_access, NULL}},
        /*06*/ {{"DKA", 47-29, 0, dk_ready, dk_access, NULL},  {"DKA", 47-29, 0, dk_ready, dk_access, NULL}},
        /*07*/ {{"MTD", 47-44, 3, mt_ready, mt_access, NULL},  {"MTD", 47-44, 3, mt_ready, mt_access, NULL}},
        /*08*/ {{"DRB", 47-30, 1},                             {"DRB", 47-30, 1}},
        /*09*/ {{"MTE", 47-43, 4, mt_ready, mt_access, NULL},  {"MTE", 47-43, 4, mt_ready, mt_access, NULL}},
        /*10*/ {{"CPA", 47-25, 0},                             {"CRA", 47-24, 0, cr_ready, cr_read, NULL}},
        /*11*/ {{"MTF", 47-42, 5, mt_ready, mt_access, NULL},  {"MTF", 47-42, 5, mt_ready, mt_access, NULL}},
        /*12*/ {{"DKB", 47-28, 1, dk_ready, dk_access, NULL},  {"DKB", 47-28, 1, dk_ready, dk_access, NULL}},
        /*13*/ {{"MTH", 47-41, 6, mt_ready, mt_access, NULL},  {"MTH", 47-41, 6, mt_ready, mt_access, NULL}},
        /*14*/ {{NULL, 0, 0},                                  {"CRB", 47-23, 1, cr_ready, cr_read, NULL}},
        /*15*/ {{"MTJ", 47-40, 7, mt_ready, mt_access, NULL},  {"MTJ", 47-40, 7, mt_ready, mt_access, NULL}},
        /*16*/ {{"DCC", 47-17, 0},                             {"DCC", 47-17, 0}},
        /*17*/ {{"MTK", 47-39, 8, mt_ready, mt_access, NULL},  {"MTK", 47-39, 8, mt_ready, mt_access, NULL}},
        /*18*/ {{"PPA", 47-21, 0},                             {"PRA", 47-20, 0}},
        /*19*/ {{"MTL", 47-38, 9, mt_ready, mt_access, NULL},  {"MTL", 47-38, 9, mt_ready, mt_access, NULL}},
        /*20*/ {{"PPB", 47-19, 1},                             {"PRB", 47-18, 1}},
        /*21*/ {{"MTM", 47-37, 10, mt_ready, mt_access, NULL}, {"MTM", 47-37, 10, mt_ready, mt_access, NULL}},
        /*22*/ {{"LPA", 47-27, 0, lp_ready, lp_write, NULL},   {NULL, 0, 0}},
        /*23*/ {{"MTN", 47-36, 11, mt_ready, mt_access, NULL}, {"MTN", 47-36, 11, mt_ready, mt_access, NULL}},
        /*24*/ {{NULL, 0, 0},                                  {NULL, 0, 0}},
        /*25*/ {{"MTP", 47-35, 12, mt_ready, mt_access, NULL}, {"MTP", 47-35, 12, mt_ready, mt_access, NULL}},
        /*26*/ {{"LPB", 47-26, 1, lp_ready, lp_write, NULL},   {NULL, 0, 0}},
        /*27*/ {{"MTR", 47-34, 13, mt_ready, mt_access, NULL}, {"MTR", 47-34, 13, mt_ready, mt_access, NULL}},
        /*28*/ {{NULL, 0, 0},                                  {NULL, 0, 0}},
        /*29*/ {{"MTS", 47-33, 14, mt_ready, mt_access, NULL}, {"MTS", 47-33, 14, mt_ready, mt_access, NULL}},
        /*30*/ {{"SPO", 47-22, 0, spo_ready, spo_write, NULL}, {"SPO", 47-22, 0, NULL, spo_read, NULL}},
        /*31*/ {{"MTT", 47-32, 15, mt_ready, mt_access, NULL}, {"MTT", 47-32, 15, mt_ready, mt_access, NULL}},
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

void signalInterrupt(const char *id, const char *cause) {
        // Called by all modules to signal that an interrupt has occurred and
        // to invoke the interrupt prioritization mechanism. This will result in
        // an updated vector address in the IAR. Can also be called to reprioritize
        // any remaining interrupts after an interrupt is handled. If no interrupt
        // condition exists, CC->IAR is set to zero
        if (P[0]->r.I & 0x01) CC->IAR = 060;		// P1 memory parity error
        else if (P[0]->r.I & 0x02) CC->IAR = 061;	// P1 invalid address error

        else if (CC->CCI03F) CC->IAR = 022;	// Time interval
        else if (CC->CCI04F) CC->IAR = 023;	// I/O busy
        else if (CC->CCI05F) CC->IAR = 024;	// Keyboard request
        else if (CC->CCI08F) CC->IAR = 027;	// I/O 1 finished
        else if (CC->CCI09F) CC->IAR = 030;	// I/O 2 finished
        else if (CC->CCI10F) CC->IAR = 031;	// I/O 3 finished
        else if (CC->CCI11F) CC->IAR = 032;	// I/O 4 finished
        else if (CC->CCI06F) CC->IAR = 025;	// Printer 1 finished
        else if (CC->CCI07F) CC->IAR = 026;	// Printer 2 finished
        else if (CC->CCI12F) CC->IAR = 033;	// P2 busy
        else if (CC->CCI13F) CC->IAR = 034;	// Inquiry request
        else if (CC->CCI14F) CC->IAR = 035;	// Special interrupt 1
        else if (CC->CCI15F) CC->IAR = 036;	// Disk file 1 read check finished
        else if (CC->CCI16F) CC->IAR = 037;	// Disk file 2 read check finished

        else if (P[0]->r.I & 0x04) CC->IAR = 062;			// P1 stack overflow
        else if (P[0]->r.I & 0xF0) CC->IAR = (P[0]->r.I >> 4) + 060;	// P1 syllable-dependent

        else if (P[1]->r.I & 0x01) CC->IAR = 040;	// P2 memory parity error
        else if (P[1]->r.I & 0x02) CC->IAR = 041;	// P2 invalid address error
        else if (P[1]->r.I & 0x04) CC->IAR = 042;	// P2 stack overflow
        else if (P[1]->r.I & 0xF0) CC->IAR = (P[1]->r.I >> 4) + 040;	// P2 syllable-dependent
        else CC->IAR = 0;// no interrupt set

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

        // get address of IOCW
        acc.id = "IO";
        acc.addr = 010;
        acc.MAIL = false;
        fetch(&acc);
        // get IOCW itself
        acc.addr = acc.word;
        acc.MAIL = false;
        fetch(&acc);
        iocw = acc.word;

        // analyze IOCW
        unitdes = (iocw & MASK_IODUNIT) >> SHFT_IODUNIT;
        reading = (iocw & MASK_IODREAD) ? true : false;
        wc = (iocw & MASK_WCNT) >> SHFT_WCNT;
        core = (iocw & MASK_ADDR) >> SHFT_ADDR;

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
                fprintf(spiofile.trace, "\n");
        }

	// check for entry in unit table
	if (unit[unitdes][reading].ioaccess) {
		// handle I/O
		result = (*unit[unitdes][reading].ioaccess)(iocw);
	} else {
	        // prepare result descriptor with not ready set
	        result = iocw | MASK_IORNRDY;
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
	int i, j;
	BIT b;
        static int td = 0;
#if 0
        // elaborate trace
        if (spiofile.trace)
                fprintf(spiofile.trace, "%08u TUS=%016llo\n", instr_count, unitsready);
#endif
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

	// go through all units
	for (i=0; i<32; i++) for (j=0; j<2; j++) {
		if (unit[i][j].isready) {
			b = (*unit[i][j].isready)(unit[i][j].index);
			if (b)
				unitsready |= (1LL << unit[i][j].readybit);
			else
				unitsready &= ~(1LL << unit[i][j].readybit);
		}
	}
	// return the mask
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

        while ((opt = getopt(argc, argv, "imsezSl:I:r:c:p:t:d:")) != -1) {
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
                // CARD READER
                case 'c':
                        if ((opt = cr_init(optarg))) exit(opt);  /* card reader emulation options */
                        break;
                // TAPE DRIVE
                case 't':
                        if ((opt = mt_init(optarg))) exit(opt); /* tape emulation options */
                        break;
                // DISK FILE
                case 'd':
                        if ((opt = dk_init(optarg))) exit(opt); /* disk file emulation options */
                        break;
		// OPERATOR CONSOLE
                case 'r':
                        if ((opt = spo_init(optarg))) exit(opt); /* console emulation options */
                        break;
                // LINE PRINTER
                case 'p':
                        if ((opt = lp_init(optarg))) exit(opt); /* printer emulation options */
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
                                "\t-c <file>\tspecify CRA/CRB options\n"
                                "\t-t <file>\tspecify tape file name\n"
                                "\t-d <file>\tspecify DKA/DKB options\n"
                                "\t-r <opts>\tspecify SPO options\n"
                                "\t-p <opts>\tspecify LPA/LPB options\n"
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

	// nothing is ready now
        unitsready = 0LL;

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
