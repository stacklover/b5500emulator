/***********************************************************************
* b5500emulator
************************************************************************
* Copyright (c) 2017, Reinhard Meyer, DL5UY
* Licensed under the MIT License,
*       see LICENSE
************************************************************************
* b5500 I/O unit
************************************************************************
* 2018-02-27  R.Meyer
*   Factored out from cc2.c
* 2018-03-16  R.Meyer
*   Added MAIN Memory access functions and IB/OB functions
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
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include "common.h"
#include "io.h"

/***********************************************************************
* optional trace files
***********************************************************************/
//static FILE *traceio = NULL;

/***********************************************************************
* the four I/O control units
***********************************************************************/
IOCU IO[4];

/***********************************************************************
* ready/initialized flag
***********************************************************************/
static BIT ready;

/***********************************************************************
* thread handle
***********************************************************************/
static pthread_t io_handler;

/***********************************************************************
* message
***********************************************************************/
struct iomsgbuf {
	long	iocu;	// 1..4
	char	iocw[8];
};

/***********************************************************************
* Main memory accesses for I/O units
* Mask all addresses and words to prevent extra bits from sneaking in
***********************************************************************/
void main_read(IOCU *u) {
	u->w = MAIN[u->d_addr & MASKMEM] & MASK_WORD48;
}

void main_read_inc(IOCU *u) {
	u->w = MAIN[u->d_addr & MASKMEM] & MASK_WORD48;
	u->d_addr = (u->d_addr+1) & MASKMEM;
}

void main_write(IOCU *u) {
	MAIN[u->d_addr & MASKMEM] = u->w & MASK_WORD48;
}

void main_write_inc(IOCU *u) {
	MAIN[u->d_addr & MASKMEM] = u->w & MASK_WORD48;
	u->d_addr = (u->d_addr+1) & MASKMEM;
}

void main_write_dec(IOCU *u) {
	MAIN[u->d_addr & MASKMEM] = u->w & MASK_WORD48;
	u->d_addr = (u->d_addr-1) & MASKMEM;
}

/***********************************************************************
* Sequential character accesses using input/output buffer of a unit
***********************************************************************/
void get_ob(IOCU *u) {
	u->ob = (u->w >> 42) & 077;
	u->w = (u->w << 6) & MASK_WORD48;
}

void put_ib(IOCU *u) {
	u->w = (u->w << 6) & MASK_WORD48;
	u->w |= u->ib & 077;
}

void put_ib_reverse(IOCU *u) {
	u->w = (u->w >> 6) & MASK_WORD48;
	u->w |= (WORD48)(u->ib & 077) << 42;
}

/***********************************************************************
* Print an IO control word
***********************************************************************/
void print_iocw(FILE *fp, IOCU *u) {
	fprintf(fp, "@%p UNIT=%02u. WC=%04o CMD=%03o CMD2=%o ADDR=%05o",
		u, u->d_unit, u->d_wc, u->d_control, u->d_result, u->d_addr);
}

/***********************************************************************
* Print an IO result word
***********************************************************************/
void print_ior(FILE *fp, IOCU *u) {
	fprintf(fp, "@%p UNIT=%02u. WC=%04o CMD=%03o RESULT=%04o ADDR=%05o",
		u, u->d_unit, u->d_wc, u->d_control, u->d_result, u->d_addr);
}

/***********************************************************************
* table of I/O units
* indexed by unit designator and read bit of I/O descriptor
***********************************************************************/
const UNIT unit[32][2] = {
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
        /*10*/ {{"CPA", 47-25, 0, cp_ready, cp_write, NULL},   {"CRA", 47-24, 0, cr_ready, cr_read, NULL}},
        /*11*/ {{"MTF", 47-42, 5, mt_ready, mt_access, NULL},  {"MTF", 47-42, 5, mt_ready, mt_access, NULL}},
        /*12*/ {{"DKB", 47-28, 1, dk_ready, dk_access, NULL},  {"DKB", 47-28, 1, dk_ready, dk_access, NULL}},
        /*13*/ {{"MTH", 47-41, 6, mt_ready, mt_access, NULL},  {"MTH", 47-41, 6, mt_ready, mt_access, NULL}},
        /*14*/ {{NULL, 0, 0},                                  {"CRB", 47-23, 1, cr_ready, cr_read, NULL}},
        /*15*/ {{"MTJ", 47-40, 7, mt_ready, mt_access, NULL},  {"MTJ", 47-40, 7, mt_ready, mt_access, NULL}},
        /*16*/ {{"DCC", 47-17, 0, dcc_ready, dcc_access, NULL},{"DCC", 47-17, 0, dcc_ready, dcc_access, NULL}},
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

/***********************************************************************
* actual I/O is done here
***********************************************************************/
static void perform_io(int cu, WORD48 iocw) {
	IOCU *u = IO + (cu-1);

	// iocw is passed by W register
	u->w = iocw;

	// statistics
	u->calls++;

	// analyze and decompose IOCW
	u->d_unit = (u->w & MASK_IODUNIT) >> SHFT_IODUNIT;
	u->d_wc = (u->w & MASK_IODWCNT) >> SHFT_IODWCNT;
	u->d_control = (u->w & MASK_IODCONTROL) >> SHFT_IODCONTROL;
	u->d_result = (u->w & MASK_IODRESULT) >> SHFT_IODRESULT;
	u->d_addr = (u->w & MASK_IODADDR) >> SHFT_IODADDR;

#if 0
	print_iocw(stdout, u);
	printf(" - ");
#endif

	BIT reading = (u->d_control & CD_24_READ) ? true : false;

	// check for entry in unit table
	if (unit[u->d_unit][reading].ioaccess) {
		// handle I/O
		(*unit[u->d_unit][reading].ioaccess)(u);
	} else {
	        // prepare result with not ready set
	        u->d_result = RD_18_NRDY;
	}

	// compose W register
	u->w = MASK_IORISMOD3;
	u->w |= ((WORD48)u->d_unit) << SHFT_IODUNIT;
	u->w |= ((WORD48)u->d_wc) << SHFT_IODWCNT;
	u->w |= ((WORD48)u->d_control) << SHFT_IODCONTROL;
	u->w |= ((WORD48)u->d_result) << SHFT_IODRESULT;
	u->w |= ((WORD48)u->d_addr) << SHFT_IODADDR;

        // return IO RESULT
	switch (cu) {
	case 1:	u->d_addr = 014;
		main_write(u);
		// set I/O complete IRQ
	        CC->CCI08F = true;
		break;
	case 2:	u->d_addr = 015;
		main_write(u);
		// set I/O complete IRQ
	        CC->CCI09F = true;
		break;
	case 3:	u->d_addr = 016;
		main_write(u);
		// set I/O complete IRQ
	        CC->CCI10F = true;
		break;
	case 4:	u->d_addr = 017;
		main_write(u);
		// set I/O complete IRQ
	        CC->CCI11F = true;
		break;
	}
#if 0
	print_ior(stdout, u);
	printf ("\n");
#endif
}

/***********************************************************************
* the IIO operation is executed here
***********************************************************************/
void initiateIO(CPU *cpu) {
	struct iomsgbuf msg;
	WORD48 w;

	// find first non busy IOCU
	if (!CC->AD1F) {
		msg.iocu = 1;
		CC->AD1F = true;
	} else if (!CC->AD2F) {
		msg.iocu = 2;
		CC->AD2F = true;
	} else if (!CC->AD3F) {
		msg.iocu = 3;
		CC->AD3F = true;
	} else if (!CC->AD4F) {
		msg.iocu = 4;
		CC->AD4F = true;
	} else {
		printf("initiateIO: all channels busy\n");
		CC->CCI04F = true;
		return;
	}

        // first: get address of IOCW
        w = MAIN[AA_IODESC];

        // get IOCW itself
	w = MAIN[w & MASKMEM];

	memcpy(msg.iocw, (char*)&w, sizeof msg.iocw);

	while (msgsnd(msg_iocu, &msg, sizeof msg.iocw, IPC_NOWAIT) < 0) {
		perror("initiateIO");
		if (errno == EINTR)
			continue;
		exit(2);
	}
}

/***********************************************************************
* check which units are ready
***********************************************************************/
WORD48 interrogateUnitStatus(CPU *cpu) {
	int i, j;
	WORD48 unitsready = 0LL;

	// go through all units
	for (i=0; i<32; i++) for (j=0; j<2; j++)
		if (unit[i][j].isready && (*unit[i][j].isready)(unit[i][j].index))
			unitsready |= (1LL << unit[i][j].readybit);

	// return the mask
        return unitsready;
}

/***********************************************************************
* interrogate the next free I/O channel
***********************************************************************/
WORD48 interrogateIOChannel(CPU *cpu) {
        WORD48 result = 0LL;

	// find first not busy IOCU
	if (!CC->AD1F)
		result = 1LL;
	else if (!CC->AD2F)
		result = 2LL;
	else if (!CC->AD3F)
		result = 3LL;
	else if (!CC->AD4F)
		result = 4LL;
	// no channel available ?
	if (result == 0LL) {
		printf("interrogateIOChannel: 0\n");
	}

        return result;
}

/***********************************************************************
* I/O handling thread
***********************************************************************/
static void *io_function(void *p) {
	size_t	len;
	struct iomsgbuf msg;
	WORD48	iocw;
loop:
	len = msgrcv(msg_iocu, &msg, sizeof msg.iocw, 0, 0);
	if (len < 0) {
		perror("IO THREAD");
		if (errno == EINTR)
			goto loop;
		exit(2);
	}
	// now do the I/O
	memcpy((char*)&iocw, msg.iocw, sizeof msg.iocw);
	perform_io(msg.iocu, iocw);
	goto loop;	
}

/***********************************************************************
* Status
***********************************************************************/
static int io_status(const char *v, void *) {
	printf("$CALLS: %u %u %u %u\n",
		IO[0].calls, IO[1].calls, IO[2].calls, IO[3].calls);
	return 0; // OK
}

/***********************************************************************
* command table
***********************************************************************/
static const command_t io_commands[] = {
	{"IO", NULL},
	{"STA", io_status},
	{NULL, NULL},
};

/***********************************************************************
* Initialize command from argv scanner or special SPO input
***********************************************************************/
int io_init(const char *option) {
	if (!ready) {
		// io handler thread
		pthread_create(&io_handler, 0, io_function, 0);
		ready = true;
	}
	return command_parser(io_commands, option);
}

/***********************************************************************
* Initial Program Load (either from CRA or DKA)
***********************************************************************/
int io_ipl(ADDR15 addr) {
        CC->CCI08F = false;
        addr = AA_STARTLOC; // start addr
        if (CC->CLS) {
                // binary read first CRA card to <addr>
		perform_io(1, 0240000540000000LL | addr);
        } else {
                // load DKA disk segments 1..63 to <addr>
		MAIN[addr-1] = 1LL;
                perform_io(1, 0140000047700000LL | (addr-1));
        }
	while (!CC->CCI08F) {
		printf ("I/O finish IRQ not present\n");
		sleep(1);
	}
        CC->CCI08F = false;
	return 1;
}

