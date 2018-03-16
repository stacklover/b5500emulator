/***********************************************************************
* b5500emulator
************************************************************************
* Copyright (c) 2016-2018, Reinhard Meyer, DL5UY
* Licensed under the MIT License,
*       see LICENSE
* based on work by Nigel Williams and Paul Kimpel
* see: https://github.com/pkimpel/retro-b5500
************************************************************************
* common declarations
************************************************************************
* 2016-02-13  R.Meyer
*   Inspired by Paul's work, otherwise from thin air.
* 2017-07-17  R.Meyer
*   Added "long long" qualifier to constants with long long value
* 2017-09-22  R.Meyer
*   Added comments and some cosmetic changes
* 2017-09-30  R.Meyer
*   overhaul of file names
* 2018-03-01  R.Meyer
*   factored out iocu.h
***********************************************************************/

#ifndef IOCU_H
#define IOCU_H

/***********************************************************************
* I/O descriptor or IO-Unit "D" register:
* --- UUU UUW WWW WWW WWW CCC CCC CRR RRR RRR AAA AAA AAA AAA AAA (General)
* octet numbers         FEDCBA9876543210
***********************************************************************/
#define MASK_IODUNIT	00760000000000000LL
#define MASK_IODWCNT	00017770000000000LL
#define MASK_IODCONTROL	00000007740000000LL
#define MASK_IODRESULT	00000000037700000LL
#define MASK_IODADDR	00000000000077777LL
#define SHFT_IODUNIT	40
#define SHFT_IODWCNT	30
#define SHFT_IODCONTROL	23
#define SHFT_IODRESULT	15
#define SHFT_IODADDR	0
#define MASK_DSKFILADR	00077777777777777LL // disk file address

/***********************************************************************
* special I/O result bits
* --- UUU UU- --p baz ccc m-- bdw reA FEP NDB AAA AAA AAA AAA AAA
* octet numbers         FEDCBA9876543210
***********************************************************************/
#define MASK_IORISMOD3  01000000000000000LL // m: TAPE - Model III

/***********************************************************************
* structure defining I/O control units
***********************************************************************/
typedef struct iocu {
	WORD48		w;		// W register
	WORD5		d_unit;		// D register, UNIT
	WORD10		d_wc;		// D register, WORD COUNT
#define WD_37_MEMPAR	0100		// TAPE - Mem. Parity
#define WD_36_BLANK	0040		// TAPE - Blank Tape
#define WD_35_BOT	0020		// TAPE - Begin of Tape
#define WD_34_EOT	0010		// TAPE - End of Tape
#define WD_333231_CHARS	0007		// TAPE - Chars in last word
	WORD7		d_control;	// D register, CONTROL
#define CD_30_MI	0100		// memory inhibit/interrogate
#define CD_29		0040
#define CD_28		0020
#define CD_27_BINARY	0010		// binary mode (0=alpha)
#define CD_26_DIR	0004		// tape reverse
#define CD_25_USEWC	0002		// use word counter
#define CD_24_READ	0001		// read mode (0=write)
	WORD10		d_result;	// D register, RESULT
#define RD_25_ABNORMAL	01000		// DCC abnormal condition
#define RD_24_READ	00400		// read mode (0=write)
#define RD_23_ETYPE	00200		// ending type
#define RD_22_MAE	00100		// Memory Access Error
#define RD_21_END	00040		// unit specific END
#define RD_20_ERR	00020		// unit specific ERR
#define RD_19_PAR	00010		// unit specific PAR
#define RD_18_NRDY	00004		// not ready
#define RD_17_PE	00002		// descriptor parity error
#define RD_16_BUSY	00001		// busy
	ADDR15		d_addr;		// D register, MEMORY ADDRESS
	// buffer registers
	WORD6		ib;		// input buffer
	WORD6		ob;		// output buffer
	// statistics
	unsigned	calls;
} IOCU;

/***********************************************************************
* structure defining physical units (emulated or real)
***********************************************************************/
typedef struct unit {
	const char	*name;		// printable unit name
	const unsigned	readybit;	// B5500 bit number of ready status bit in TUS
	const unsigned	index;		// enumeration of several units of same type
	BIT	(*isready)(unsigned);	// function to check for ready
	void	(*ioaccess)(IOCU*);	// function to actually perform IO
	// handling functions (not yet used, may need reconsideration)
	BIT	(*load)(void);		// function to load from this unit
} UNIT;

/***********************************************************************
* global (IPC) memory areas
***********************************************************************/
extern int	msg_iocu;	// messages to IOCU(s)
extern IOCU	IO[4];
extern const UNIT unit[32][2];

/***********************************************************************
* main memory access
***********************************************************************/
extern void main_read(IOCU *u);
extern void main_read_inc(IOCU *u);
extern void main_write(IOCU *u);
extern void main_write_inc(IOCU *u);
extern void get_ob(IOCU *u);
extern void put_ib(IOCU *u);

/* Supervisory Console (SPO) */
extern void spo_print(const char *buf);
extern int spo_init(const char *info);
extern void spo_term(void);
extern BIT spo_ready(unsigned index);
extern void spo_write(IOCU*);
extern void spo_read(IOCU*);
extern void spo_debug_write(const char *msg);

/* Card Readers (CRx) */
extern int cr_init(const char *info);
extern void cr_term(void);
extern BIT cr_ready(unsigned index);
extern void cr_read(IOCU*);

/* Line Printers (LPx) */
extern int lp_init(const char *info);
extern void lp_term(void);
extern BIT lp_ready(unsigned index);
extern void lp_write(IOCU*);

/* Magnetic Tapes (MTx) */
extern int mt_init(const char *info);
extern void mt_term(void);
extern BIT mt_ready(unsigned index);
extern void mt_access(IOCU*);

/* Disk Control Units (DKx) */
extern int dk_init(const char *info);
extern void dk_term(void);
extern BIT dk_ready(unsigned index);
extern void dk_access(IOCU*);

/* Data Communication (DC) */
extern int dcc_init(const char *info);
extern void dcc_term(void);
extern BIT dcc_ready(unsigned index);
extern void dcc_access(IOCU*);

/* IO Units (IO) */
extern int io_init(const char *info);
extern int io_ipl(ADDR15 addr);

/* debug formatting functions */
extern void print_iocw(FILE *fp, IOCU*);
extern void print_ior(FILE *fp, IOCU*);

#ifdef USECAN
/* CAN bus devices */
extern void can_init(const char *busname);
extern int can_send_string(unsigned id, const char *data);
extern char *can_receive_string(unsigned id, char *data, int maxlen);
extern int can_ready(unsigned id);
#endif

#endif /* IOCU_H */

