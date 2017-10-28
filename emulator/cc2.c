/***********************************************************************
* b5500emulator
************************************************************************
* Copyright (c) 2017, Reinhard Meyer, DL5UY
* Licensed under the MIT License,
*       see LICENSE
************************************************************************
* b5500 central control (CC) IRQ handling part
************************************************************************
* 2017-10-02  R.Meyer
*   Factored out from emulator.c
* 2017-10-10  R.Meyer
*   some refactoring in the functions, added documentation
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
#include "common.h"

/*
 * optional trace files
 */
static FILE *traceirq = NULL;
static FILE *traceio = NULL;

/***********************************************************************
* Prepare a debug message
* message must be completed and ended by caller
***********************************************************************/
void prepMessage(CPU *cpu) {
	if (traceirq == NULL)
		return;
	if (cpu->rC != 0)
		fprintf(traceirq, "*\t%s at %05o:%o ",
			cpu->id,
			cpu->rL == 0 ? cpu->rC-1 : cpu->rC,
			(cpu->rL - 1) & 3);
	else
		fprintf(traceirq, "*\t%s at xxxxx:x ", cpu->id);
}

/***********************************************************************
* Cause a memory access based IRQ
***********************************************************************/
void causeMemoryIrq(CPU *cpu, WORD8 irq, const char *reason) {
	cpu->rI |= irq;
	signalInterrupt(cpu->id, reason);
	if (traceirq == NULL)
		return;
	prepMessage(cpu);
	fprintf(traceirq, "IRQ %02x caused reason %s (I now %02x)\n",
		irq, reason, cpu->rI);
}

/***********************************************************************
* Cause a syllable based IRQ
***********************************************************************/
void causeSyllableIrq(CPU *cpu, WORD8 irq, const char *reason) {
	cpu->rI = (cpu->rI & IRQ_MASKL) | irq;
	signalInterrupt(cpu->id, reason);
	if (traceirq == NULL)
		return;
	prepMessage(cpu);
	fprintf(traceirq, "IRQ %02x caused reason %s (I now %02x)\n",
		irq, reason, cpu->rI);
}

/*
 * table of IRQs
 * indexed by cell address - 020
 */
IRQ irq[48] = {
	// general interrupts
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
	// processor 2 interrupts
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
	// processor 1 interrupts
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

/*
 * Called by all modules to signal that an interrupt has occurred and
 * to invoke the interrupt prioritization mechanism. This will result in
 * an updated vector address in the IAR. Can also be called to reprioritize
 * any remaining interrupts after an interrupt is handled. If no interrupt
 * condition exists, CC->IAR is set to zero
 */
void signalInterrupt(const char *id, const char *cause) {
	// use a temporary variable to ensure a monolithic store at the end
	ADDR15 temp = 0;
	// do the tests in priority order
        if      (P[0]->rI & 0x01) temp = 060;	// P1 memory parity error
        else if (P[0]->rI & 0x02) temp = 061;	// P1 invalid address error

        else if (CC->CCI03F) temp = 022;	// Time interval
        else if (CC->CCI04F) temp = 023;	// I/O busy
        else if (CC->CCI05F) temp = 024;	// Keyboard request
        else if (CC->CCI08F) temp = 027;	// I/O 1 finished
        else if (CC->CCI09F) temp = 030;	// I/O 2 finished
        else if (CC->CCI10F) temp = 031;	// I/O 3 finished
        else if (CC->CCI11F) temp = 032;	// I/O 4 finished
        else if (CC->CCI06F) temp = 025;	// Printer 1 finished
        else if (CC->CCI07F) temp = 026;	// Printer 2 finished
        else if (CC->CCI12F) temp = 033;	// P2 busy
        else if (CC->CCI13F) temp = 034;	// Inquiry request
        else if (CC->CCI14F) temp = 035;	// Special interrupt 1
        else if (CC->CCI15F) temp = 036;	// Disk file 1 read check finished
        else if (CC->CCI16F) temp = 037;	// Disk file 2 read check finished

        else if (P[0]->rI & 0x04) temp = 062;	// P1 stack overflow
        else if (P[0]->rI & 0xF0) temp = (P[0]->rI >> 4) + 060;	// P1 syllable-dependent

        else if (P[1]->rI & 0x01) temp = 040;	// P2 memory parity error
        else if (P[1]->rI & 0x02) temp = 041;	// P2 invalid address error
        else if (P[1]->rI & 0x04) temp = 042;	// P2 stack overflow

        else if (P[1]->rI & 0xF0) temp = (P[1]->rI >> 4) + 040;	// P2 syllable-dependent

        else temp = 0;// no interrupt set

	CC->IAR = temp;

	if (traceirq == NULL)
		return;
	fprintf(traceirq, "%08u %s signalInterrupt %s P1.I=%02x P2.I=%02x\n",
		instr_count, id, cause, P[0]->rI, P[1]->rI);
}

/*
 * Resets an interrupt based on the current setting of CC->IAR, then
 * re-prioritices any remaining interrupts, leaving the new vector address
 * in CC->IAR
 */
void clearInterrupt(ADDR15 iar) {
        if (iar) {
                // current active IRQ
                CC->interruptMask &= ~(1ll << iar);
                switch (iar) {
		case 000: // no IRQ
			break;
                case 022: // Time interval
                        CC->CCI03F = false;
                        break;
                case 023: // I/O busy
                        CC->CCI04F = false;
                        break;
                case 024: // Keyboard request
                        CC->CCI05F = false;
                        break;
                case 025: // Printer 1 finished
                        CC->CCI06F = false;
                        break;
                case 026: // Printer 2 finished
                        CC->CCI07F = false;
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
                case 033: // P2 busy
                        CC->CCI12F = false;
                        break;
                case 034: // Inquiry request
                        CC->CCI13F = false;
                        break;
                case 035: // Special interrupt 1
                        CC->CCI14F = false;
                        break;
                case 036: // Disk file 1 read check finished
                        CC->CCI15F = false;
                        break;
                case 037: // Disk file 2 read check finished
                        CC->CCI16F = false;
                        break;

                case 040: // P2 memory parity error
                        if (P[1]) P[1]->rI &= 0xFE;
                        break;
                case 041: // P2 invalid address error
                        if (P[1]) P[1]->rI &= 0xFD;
                        break;
                case 042: // P2 stack overflow
                        if (P[1]) P[1]->rI &= 0xFB;
                        break;
                // 44-55: P2 syllable-dependent
                case 044: case 045: case 046: case 047:
                case 050: case 051: case 052: case 053:
                case 054: case 055:
                        if (P[1]) P[1]->rI &= 0x0F;
                        break;

                case 060: // P1 memory parity error
                        P[0]->rI &= 0xFE;
                        break;
                case 061: // P1 invalid address error
                        P[0]->rI &= 0xFD;
                        break;
                case 062: // P1 stack overflow
                        P[0]->rI &= 0xFB;
                        break;
                // 64-75: P1 syllable-dependent
                case 064: case 065: case 066: case 067:
                case 070: case 071: case 072: case 073:
                case 074: case 075:
                        P[0]->rI &= 0x0F;
                        break;

                default: // no recognized interrupt vector was set
			printf("*\tWARNING: illegal IAR value: %05o\n", iar);
                        break;
                }
        }
        signalInterrupt("CC", "AGAIN");
};

/*
 * handle 60Hz timer
 *
 * warning: this can be called from another thread or even interrupt context
 */
void timer60hz(union sigval sv) {
	WORD6 temp = CC->TM;
	temp = (temp+1) & 077;
	CC->TM = temp;
	// did it overflow to 0 ?
	if (temp == 0) {
		// signal timer IRQ
		CC->CCI03F = true;
		signalInterrupt("CC", "TIMER");
	}
}

/*
 * Called by P1 to initiate P2. Assumes that an INCW has been stored at
 * memory location @10. If P2 is busy or not present, sets the P2 busy
 * interrupt. Otherwise, loads the INCW into P2's A register and initiates
 * the processor
 */
void initiateP2(CPU *cpu)
{
	if (traceirq != NULL) {
		prepMessage(cpu);
		fprintf(traceirq, "initiateP2 - ");
	}
	// always cause P2 busy IRQ (for now)

	if (true /* P2BF || !P2 */) {
		if (traceirq != NULL)
			fprintf(traceirq, "busy or not available\n");
		CC->CCI12F = true;
		signalInterrupt(cpu->id, "initiateP2");
	} else {
		if (traceirq != NULL)
			fprintf(traceirq, "done\n");
		//P2BF = 1;
		//ccLatch |= 0x10;
		//HP2F = 0;
		//initiateAsP2();
	}
}

void haltP2(CPU *cpu)
{
	if (traceirq != NULL) {
		prepMessage(cpu);
		printf("haltP2");
	}
}

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

        // elaborate traceio
        if (traceio) {
                fprintf(traceio, "%08u IOCW=%016llo\n", instr_count, iocw);
                fprintf(traceio, "\tunit=%u(%s) core=%05o", unitdes, unit[unitdes][reading].name, core);
                if (iocw & MASK_IODMI) fprintf(traceio, " inhibit");
                if (iocw & MASK_IODBINARY) fprintf(traceio, " binary"); else fprintf(traceio, " alpha");
                if (iocw & MASK_IODTAPEDIR)  fprintf(traceio, " reverse");
                if (reading) fprintf(traceio, " read"); else fprintf(traceio, " write");
                if (iocw & MASK_IODSEGCNT) fprintf(traceio, " segments=%llu", (iocw & MASK_IODSEGCNT) >> SHFT_IODSEGCNT);
                if (iocw & MASK_IODUSEWC) fprintf(traceio, " wc=%u", wc);
                fprintf(traceio, "\n");
        }

	// check for entry in unit table
	if (unit[unitdes][reading].ioaccess) {
		// handle I/O
		result = (*unit[unitdes][reading].ioaccess)(iocw);
	} else {
	        // prepare result descriptor with not ready set
	        result = iocw | MASK_IORNRDY;
	}

        if (traceio) {
                fprintf(traceio, "\tRSLT=%016llo\n", result);
                fflush(traceio);
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
		union sigval sv;
		extern void timer60hz(sigval);
		timer60hz(sv);
#if 0
                CC->TM++;
                if (CC->TM >= 63) {
                        CC->TM = 0;
                        CC->CCI03F = true;
                        signalInterrupt("CC", "TIMER");
                } else {
                        CC->TM++;
                }
#endif
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

/*
 * Called by a requestor module passing accessor object "acc" to fetch a
 * word from memory.
 */
void fetch(ACCESSOR *acc)
{
        BIT watched = dotrcmem;
        //if (acc->addr == 0200)
        //        watched = true;
        // For now, we assume memory parity can never happen
        if (acc->MAIL) {
                acc->MPED = false;      // no memory parity error
                acc->MAED = true;       // memory address error
                acc->word = 0;
                if (watched)
                        printf("\t[%05o] ERROR MPED=%u MAED=%u (%s)\n",
                                acc->addr, acc->MPED, acc->MAED, acc->id);
        } else {
                acc->MPED = false;      // no parity error
                acc->MAED = false;      // no address error
                acc->word = MAIN[acc->addr & MASKMEM];
                if (watched)
                        printf("\t[%05o]->%016llo OK (%s)\n",
                                acc->addr, acc->word, acc->id);
        }
}

/*
 * Called by requestor module passing accessor object "acc" to store a
 * word into memory.
 */
void store(ACCESSOR *acc)
{
        BIT watched = dotrcmem;
        //if (acc->addr == 0200)
        //        watched = true;
        // For now, we assume memory parity can never happen
        if (acc->MAIL) {
                acc->MPED = false;      // no memory parity error
                acc->MAED = true;       // memory address error
                if (watched)
                        printf("\t[%05o] ERROR MPED=%u MAED=%u (%s)\n",
                        acc->addr, acc->MPED, acc->MAED, acc->id);
        } else {
                acc->MPED = false;      // no parity error
                acc->MAED = false;      // no address error
                MAIN[acc->addr & MASKMEM] = acc->word;
                if (watched)
                        printf("\t[%05o]<-%016llo OK (%s)\n",
                                acc->addr, acc->word, acc->id);
        }
}



