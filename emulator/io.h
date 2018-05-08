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
* main memory access
***********************************************************************/
extern void main_read(IOCU *u);
extern void main_read_inc(IOCU *u);
extern void main_write(IOCU *u);
extern void main_write_inc(IOCU *u);
extern void main_write_dec(IOCU *u);
extern void get_ob(IOCU *u);
extern void put_ib(IOCU *u);
extern void put_ib_reverse(IOCU *u);

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

/* Card Punches (CPx) */
extern int cp_init(const char *info);
extern void cp_term(void);
extern BIT cp_ready(unsigned index);
extern void cp_write(IOCU*);

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

/* Magnetic Tapes V2 (MTx) */
extern int mt2_init(const char *info);
extern void mt2_term(void);
extern BIT mt2_ready(unsigned index);
extern void mt2_access(IOCU*);

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
extern int can_write(unsigned id, const char *buf, int len);
extern int can_read(unsigned id, char *buf, int maxlen);
extern int can_ready(unsigned id);
#endif

#endif /* IOCU_H */

