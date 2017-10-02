/***********************************************************************
* b5500emulator
************************************************************************
* Copyright (c) 2017, Reinhard Meyer, DL5UY
* Licensed under the MIT License,
*       see LICENSE
************************************************************************
* b5500 central control (CC) I/O handling part
************************************************************************
* 2017-10-02  R.Meyer
*   Factored out from emulator.c
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
 * optional trace if I/O at this level
 */
static FILE *trace = NULL;

/*
 * the IIO operation is executed here
 */
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
        if (trace) {
                fprintf(trace, "%08u IOCW=%016llo\n", instr_count, iocw);
                fprintf(trace, "\tunit=%u(%s) core=%05o", unitdes, unit[unitdes][reading].name, core);
                if (iocw & MASK_IODMI) fprintf(trace, " inhibit");
                if (iocw & MASK_IODBINARY) fprintf(trace, " binary"); else fprintf(trace, " alpha");
                if (iocw & MASK_IODTAPEDIR)  fprintf(trace, " reverse");
                if (reading) fprintf(trace, " read"); else fprintf(trace, " write");
                if (iocw & MASK_IODSEGCNT) fprintf(trace, " segments=%llu", (iocw & MASK_IODSEGCNT) >> SHFT_IODSEGCNT);
                if (iocw & MASK_IODUSEWC) fprintf(trace, " wc=%u", wc);
                fprintf(trace, "\n");
        }

	// check for entry in unit table
	if (unit[unitdes][reading].ioaccess) {
		// handle I/O
		result = (*unit[unitdes][reading].ioaccess)(iocw);
	} else {
	        // prepare result descriptor with not ready set
	        result = iocw | MASK_IORNRDY;
	}

        if (trace) {
                fprintf(trace, "\tRSLT=%016llo\n", result);
                fflush(trace);
        }

        // return IO RESULT
        acc.addr = 014;
        acc.word = result;
        store(&acc);
        CC->CCI08F = true;
        signalInterrupt("IO", "COMPLETE");
}

/*
 * check which units are ready
 */
WORD48 interrogateUnitStatus(CPU *cpu) {
	int i, j;
	WORD48 unitsready = 0LL;

        // TODO: simulate timer - this should NOT be done this way - fix it
        static int td = 0;
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
	for (i=0; i<32; i++) for (j=0; j<2; j++)
		if (unit[i][j].isready && (*unit[i][j].isready)(unit[i][j].index))
			unitsready |= (1LL << unit[i][j].readybit);

	// return the mask
        return unitsready;
}

/*
 * interrogate the next free I/O channel
 */
WORD48 interrogateIOChannel(CPU *cpu) {
        WORD48 result = 0;

        // report I/O control unit 1
        result = 1ll;

        return result;
}


