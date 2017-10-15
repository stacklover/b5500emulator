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

#if TRACEIRQ
/*
 * optional trace of IRQ action
 */
static FILE *trace = NULL;
#endif

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
        if (P[0]->r.I & 0x01) temp = 060;		// P1 memory parity error
        else if (P[0]->r.I & 0x02) temp = 061;	// P1 invalid address error

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

        else if (P[0]->r.I & 0x04) temp = 062;			// P1 stack overflow
        else if (P[0]->r.I & 0xF0) temp = (P[0]->r.I >> 4) + 060;	// P1 syllable-dependent

        else if (P[1]->r.I & 0x01) temp = 040;	// P2 memory parity error
        else if (P[1]->r.I & 0x02) temp = 041;	// P2 invalid address error
        else if (P[1]->r.I & 0x04) temp = 042;	// P2 stack overflow
        else if (P[1]->r.I & 0xF0) temp = (P[1]->r.I >> 4) + 040;	// P2 syllable-dependent
        else temp = 0;// no interrupt set

	CC->IAR = temp;

#if TRACEIRQ
        if (trace) {
                fprintf(trace, "%08u %s signalInterrupt %s P1.I=%02x P2.I=%02x\n",
                        instr_count, id, cause, P[0]->r.I, P[1]->r.I);
        }
#endif
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
                        if (P[1]) P[1]->r.I &= 0xFE;
                        break;
                case 041: // P2 invalid address error
                        if (P[1]) P[1]->r.I &= 0xFD;
                        break;
                case 042: // P2 stack overflow
                        if (P[1]) P[1]->r.I &= 0xFB;
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
                // 64-75: P1 syllable-dependent
                case 064: case 065: case 066: case 067:
                case 070: case 071: case 072: case 073:
                case 074: case 075:
                        P[0]->r.I &= 0x0F;
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
        printf("*\t%s: initiateP2 - ", cpu->id);

	// always cause P2 busy IRQ (for now)

	if (true /* P2BF || !P2 */) {
		printf("busy or not available\n");
		CC->CCI12F = true;
		signalInterrupt(cpu->id, "initiateP2");
	} else {
		printf("done\n");
		//P2BF = 1;
		//ccLatch |= 0x10;
		//HP2F = 0;
		//initiateAsP2();
	}
}

void haltP2(CPU *cpu)
{
        printf("*\t%s: haltP2\n", cpu->id);
}


