/***********************************************************************
* b5500emulator
************************************************************************
* Copyright (c) 2016-2017, Reinhard Meyer, DL5UY
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
***********************************************************************/

#ifndef COMMON_H
#define COMMON_H

/*
 * first, we define some types representing the typical register
 * widths of the B5500.
 *
 * Notes:
 *   this will not prevent more bits to be set in the actual variable
 *   not all compilers will warn if shifting or assigning may cause loss
 *   of information
 *   by principle all types are compatible with each other, so no warnings
 *   will be issued by compilers on wrong types in function calls
 *   (I wish we had some of the type strictness of pascal here)
 */
typedef unsigned char BIT;              // a single bit
#define true 1
#define false 0
typedef unsigned char WORD2;            // 2 bits
typedef unsigned char WORD3;            // 3 bits
typedef unsigned char WORD4;            // 4 bits
typedef unsigned char WORD6;            // 6 bits
typedef unsigned char WORD8;            // 8 bits
typedef unsigned short ADDR9;           // 9 bits higher part of memory address
typedef unsigned short WORD12;          // 12 bits instruction register
typedef unsigned short ADDR15;          // 15 bits memory address
typedef unsigned long WORD21;           // 21 bits
typedef unsigned long long WORD39;      // 39 bits mantissa extension
typedef unsigned long long WORD48;      // 48 bits machine word

/*
 * masks that should be used when assigning to above types when overflow is possible
 */
#define MASK_BIT    01

#define MASK_WORD2  03
#define MASK_WORD3  07
#define MASK_WORD4  017
#define MASK_WORD6  077
#define MASK_WORD8  0377
#define MASK_WORD12 07777
#define MASK_WORD21 07777777
#define MASK_WORD39 07777777777777LL
#define MASK_WORD48 07777777777777777LL

#define MASK_ADDR9  000777
#define MASK_ADDR10 001777
#define MASK_ADDR12 007777
#define MASK_ADDR15 077777

/*
 * define all the registers of the Central Control instance
 * NOTE: this will be in shared memory and MUST NOT contain any pointers!
 */
typedef struct central_control {
        ADDR15          IAR;    // IRQ "vector"
        WORD6           TM;     // real time clock register
// interrupt flags (note: each processor has 8 additional interrupt flags)
        BIT             CCI03F; // time interval
        BIT             CCI04F; // I/O busy
        BIT             CCI05F; // keyboard request
        BIT             CCI06F; // printer 1 finished
        BIT             CCI07F; // printer 2 finished
        BIT             CCI08F; // I/O control unit 1 finished
        BIT             CCI09F; // I/O control unit 2 finished
        BIT             CCI10F; // I/O control unit 3 finished
        BIT             CCI11F; // I/O control unit 4 finished
        BIT             CCI12F; // processor 2 busy
        BIT             CCI13F; // datacomm
        BIT             CCI14F; // not assigned
        BIT             CCI15F; // disk file 1 read check finished
        BIT             CCI16F; // disk file 2 read check finished
// I/O control flags
        BIT             AD1F;   // I/O control 1 admitted
        BIT             AD2F;   // I/O control 2 admitted
        BIT             AD3F;   // I/O control 3 admitted
        BIT             AD4F;   // I/O control 4 admitted
// flags from processor 2
        BIT             HP2F;   // HALT CPU #2 flag
        BIT             P2BF;   // CPU #2 busy flag
// operator panel
	BIT		CLS;	// CARD LOAD SELECT
// some helper variables
        WORD48          interruptMask;
        WORD48          interruptLatch;
        WORD4           iouMask;
} CENTRAL_CONTROL;

/*
 * define the structure for the memory access function "fetch" and "store"
 * Note: ALL memory access must use those functions, and not access memory directly
 * this will help implementing alternative memory access methods and also make sure
 * contention and stale data can be prevented in multiple CPU systems
 */
typedef struct accessor {
        const char      *id;    // pointer to name of requester (for debug only)
        ADDR15          addr;   // requested address
        WORD48          word;   // data in or out
        BIT             MAIL;   // true if access to 00000..00777
        BIT             MPED;   // parity error detected
        BIT             MAED;   // memory access error detected
} ACCESSOR;

/*
 * all the data defining one processor
 * NOTE: this will be in shared memory and MUST NOT contain any pointers!
 */
typedef struct cpu {
        WORD48          rA;      // A register
        WORD48          rB;      // B register
        ADDR15          rC;      // C register (program address)
        WORD6           rE;      // E Memory access control register
        ADDR15          rF;      // F register (frame address)
        WORD6           rGH;     // Character/Bit index register for A
        WORD8           rI;      // I register (interrupts)
        WORD4           rJ;      // J state machine register
        WORD6           rKV;     // Character/Bit index register for B
        WORD2           rL;      // Instruction syllable index in P
        ADDR15          rM;      // M register (memory address)
        WORD4           rN;      // Octal shift counter for B
        WORD48          rP;      // Current program instruction word register
// Q register is handled as BITs, see below
        ADDR15          rR;      // High-order 9 bits of PRT base address (TALLY in char mode)
				 // lower 6 bits MUST be kept clear
        ADDR15          rS;      // S register (stack pointer)
        WORD12          rT;      // Current program syllable register
        WORD39          rX;      // Mantissa extension for B (loop control in CM)
        WORD6           rY;      // Serial character register for A
        WORD6           rZ;      // Serial character register for B
        WORD8           rTM;     // Temporary maintenance storage register
// Q register as BITs (not all are used in accordance with the real B5500)
        BIT             bQ01F;   // Q register Bit 01
        BIT             bQ02F;   // Q register Bit 02
        BIT             bQ03F;   // Q register Bit 03
        BIT             bQ04F;   // Q register Bit 04
        BIT             bQ05F;   // Q register Bit 05
        BIT             bQ06F;   // Q register Bit 06
        BIT             bQ07F;   // Q register Bit 07
        BIT             bQ08F;   // Q register Bit 08
        BIT             bQ09F;   // Q register Bit 09
        BIT             bQ12F;   // Q register Bit 12
// Q12F: MSFF (word mode: MSCW is pending RCW)
// Q12F: TFFF (char mode: True-False Flip-Flop)
#define bMSFF           bQ12F
#define bTFFF           bQ12F
// other status and flag registers (not all are currently used)
        BIT             bAROF;   // A register occupied flag
        BIT             bBROF;   // B register occupied flag
        BIT             bCCCF;   // Clock-count control FF (maintenance only)
        BIT             bCWMF;   // Character/word mode FF (1=CM)
        BIT             bEIHF;   // E-register Inhibit Address FF
        BIT             bHLTF;   // Processor halt FF
        BIT             bMRAF;   // Memory read access FF
        BIT             bMROF;   // Memory read obtained FF
        BIT             bMWOF;   // Memory write obtained FF
        BIT             bNCSF;   // Normal/Control State FF (1=normal)
        BIT             bPROF;   // P contents valid
        BIT             bSALF;   // Program/subroutine state FF (1=subroutine)
        BIT             bTROF;   // T contents valid
        BIT             bVARF;   // Variant-mode FF (enables full PRT indexing)
        BIT             bUS14X;  // Operator Halt Switch
        BIT             bzzzF;   // one lamp in display right of Q1 has no label

        ACCESSOR        acc;            // memory access
        char            id[4];          // name of CPU (for display/debug only)
        unsigned        cycleCount;     // approx of CPU cycles needed
        unsigned        cycleLimit;     // Cycle limit for this.run()
        unsigned        normalCycles;   // Current normal-state cycle count (for UI display)
        unsigned        controlCycles;  // Current control-state cycle count (for UI display)
        unsigned        runCycles;      // Current cycle count for this.run()
        unsigned        totalCycles;    // Total cycles executed on this processor
        BIT             isP1;           // we are CPU #1
        BIT             busy;           // CPU is busy
} CPU;

/*
 * structure defining physical units (emulated or real)
 */
typedef struct unit {
        const char      *name;          // printable unit name
        const unsigned  readybit;       // B5500 bit number of ready status bit in TUS
	const unsigned	index;		// enumeration of several units of same type
        // handling functions (not yet used, may need reconsideration)
        BIT             (*isready)(unsigned);     // function to check for ready
        WORD48          (*ioaccess)(WORD48);      // function to actually perform IO
        BIT             (*load)(void);            // function to load from this unit
} UNIT;

/*
 * structure defining interrupts
 */
typedef struct irq {
        const char      *name;          // printable IRQ name
} IRQ;

/*
 * shared memory and message names
 * those are used with IPC based implementations
 */
#define SHM_MAIN        (('M'<<24)|('A'<<16)|('I'<<8)|'N')  // shared memory of WORD48[32768]
#define SHM_CPUA        (('C'<<24)|('P'<<16)|('U'<<8)|'A')  // shared data of cpu A
#define SHM_CPUB        (('C'<<24)|('P'<<16)|('U'<<8)|'B')  // shared data of cpu B
#define SHM_CC          (('C'<<24)|('C'<<16)|('_'<<8)|'_')  // shared data of Central Control
#define MSG_CPUA        (('C'<<24)|('P'<<16)|('U'<<8)|'A')  // messages to cpu A
#define MSG_CPUB        (('C'<<24)|('P'<<16)|('U'<<8)|'B')  // messages to cpu B
#define MSG_CC          (('C'<<24)|('C'<<16)|('_'<<8)|'_')  // messages to central control

/*
 * macros for memory handling
 */
#define MAXMEM          32768
#define MASKMEM         077777

/*
 * Message types
 */
#define MSG_SIGINT      1       // CPU to CC: signalInterrupt()
#define MSG_CLEAR       100     // CC to CPU: clear()
#define MSG_INIT_AS_P2  101     // CC to CPU: initiateAsP2()
#define MSG_START       102     // CC to CPU: start()
#define MSG_STOP        103     // CC to CPU: stop()
#define MSG_PRESET      104     // CC to CPU: preset()

/*
 * global (IPC) memory areas
 */
extern WORD48 *MAIN;
extern CPU *P[2];
extern CENTRAL_CONTROL *CC;

/*
 * special memory locations (absolute addresses)
 */
#define AA_IODESC       00010   // IOCW is stored here by IIO operator
                                // also used to store IP2 value
#define AA_IRQSTACK     00100   // stack is set here for IRQ processing
#define AA_USERMEM      01000   // user memory starts here

/*
 * special memory locations (R relative)
 */
#define RR_MSCW         00007   // MSCW is stored here for nested calls
#define RR_INCW         00010   // INCW is stored here on interrupt
#define RR_COM          00011   // COM word is stored here on COM operator
#define	RSHIFT		6	// missing bits in R register

/*
 * interrupt codes of a cpu
 */
// multiple bits can be set here
#define IRQ_MPE         0x01    // memory parity error
#define IRQ_INVA        0x02    // invalid address
#define IRQ_STKO        0x04    // stack overflow
#define IRQ_MASKL       0x0f    // mask for lower 4 IRQ bits
// only one value can be set here
#define IRQ_COM         0x40    // COM operator
#define IRQ_PREL        0x50    // program release
#define IRQ_CONT        0x60    // continuity bit
#define IRQ_PBIT        0x70    // presence bit
#define IRQ_FLAG        0x80    // flag bit
#define IRQ_INDEX       0x90    // invalid index
#define IRQ_EXPU        0xa0    // exponent underflow
#define IRQ_EXPO        0xb0    // exponent overflow
#define IRQ_INTO        0xc0    // integer overflow
#define IRQ_DIVZ        0xd0    // divide by zero

/*
 * B5500 integer/real format:
 * 0me EEE EEE MMM MMM MMM MMM MMM MMM MMM MMM MMM MMM MMM MMM MMM
 * 444 444 443 333 333 333 222 222 222 211 111 111 110 000 000 000
 * 765 432 109 876 543 210 987 654 321 098 765 432 109 876 543 210
 * octet numbers         FEDCBA9876543210
 */
#define MASK_MANTISSA   00007777777777777LL // M: 13 octets unsigned mantissa
#define MASK_EXPONENT   00770000000000000LL // E: 2 octets unsigned exponent
#define MASK_SIGNEXPO   01000000000000000LL // e: exponent sign bit
#define MASK_SIGNMANT   02000000000000000LL // m: mantissa sign bit
#define MASK_NUMBER     03777777777777777LL // meEM: the number without control bit
#define MASK_MANTHIGH   00007000000000000LL // highest octet of mantissa
#define MASK_MANTHBIT   00004000000000000LL // highest bit of mantissa
#define MASK_MANTCARRY  00010000000000000LL // a carry bit (internal use only)
#define SHFT_MANTCARRY  39
#define SHFT_EXPONENT   39
#define SHFT_SIGNEXPO   45
#define SHFT_SIGNMANT   46

/*
 * B5500 control word formats:
 * (common for all control words)
 * 1cp xma --W WWW WWW WWW FFF FFF FFF FFF FFF AAA AAA AAA AAA AAA
 * --- --- RRR RRR RRR -ms -jk --v --- --- --- CCC CCC CCC CCC CCC
 * --- -HH HVV VLL GGG KKK --- --- --- --N NNN MMM MMM MMM MMM MMM
 * 444 444 443 333 333 333 222 222 222 211 111 111 110 000 000 000
 * 765 432 109 876 543 210 987 654 321 098 765 432 109 876 543 210
 * octet numbers         FEDCBA9876543210
 */
#define MASK_FLAG       04000000000000000LL // 1: the control bit [0:1]
#define MASK_CODE       02000000000000000LL // c: the code bit (0=data) [1:1]
#define MASK_PBIT       01000000000000000LL // p: the presence bit [2:1]
#define MASK_XBIT       00400000000000000LL // x: the execute bit (1=PD, 0=CW) [3:1]
#define MASK_MODE       00200000000000000LL // m: word/char mode bit [4:1]
#define MASK_ARGS       00100000000000000LL // a: arguments required [5:1]
#define MASK_TYPE       03400000000000000LL // the type bits [1:3]
#define MASK_WCNT       00017770000000000LL // W: word count [8:10]
#define MASK_HREG       00340000000000000LL // H: H register [4:3]
#define MASK_VREG       00034000000000000LL // V: V register [7:3]
#define MASK_LREG       00003000000000000LL // L: L register [10:2]
#define MASK_GREG       00000700000000000LL // G: G register [12:3]
#define MASK_KREG       00000070000000000LL // K: K register [15:3]
#define MASK_MSFF       00000020000000000LL // m: MSFF bit [16:1]
#define MASK_SALF       00000010000000000LL // s: SALF bit [17:1]
#define MASK_INTG       00000002000000000LL // j: integer bit [19:1]
#define MASK_CONT       00000001000000000LL // k: continuity bit [20:1]
#define MASK_VARF       00000000100000000LL // v: VARF bit [23:1]
#define MASK_RREG       00077700000000000LL // R: R register [6:9]
#define MASK_FREG       00000007777700000LL // F: F register [18:15]
#define MASK_NREG       00000000001700000LL // N: N register [29:4]
#define MASK_MREG       00000000000077777LL // M: M register [33:15]
#define MASK_CREG       00000000000077777LL // C: C register [33:15]
#define MASK_ADDR       00000000000077777LL // A: memory or disk address [33:15]
#define SHFT_TYPE       44
#define SHFT_HREG       41
#define SHFT_VREG       38
#define SHFT_LREG       36
#define SHFT_GREG       33
#define SHFT_RREG       33
#define SHFT_KREG       30
#define SHFT_WCNT       30
#define SHFT_FREG       15
#define SHFT_NREG       15
#define SHFT_MREG       0
#define SHFT_CREG       0
#define SHFT_ADDR       0

// some helpful macros
#define DESCRIPTOR(x)   ((x)&MASK_FLAG)
#define OPERAND(x)      (!DESCRIPTOR(x))
#define PRESENT(x)      ((x)&MASK_PBIT)
#define ABSENT(x)       (!PRESENT(x))

/*
 * data descriptor:
 * 10P 000 00W WWW WWW WWW 0IC 000 000 000 000 AAA AAA AAA AAA AAA
 */
#define INIT_DD         04000000000000000LL // 10: fixed bits

/*
 * mark stack control word:
 * 110 000 RRR RRR RRR 0ms FFF FFF FFF FFF FFF 000 000 000 000 000
 */
#define INIT_MSCW       06000000000000000LL // 11: fixed bits

/*
 * program descriptor word:
 * 11P 1ma 000 000 000 000 FFF FFF FFF FFF FFF AAA AAA AAA AAA AAA
 */
#define INIT_PCW        06400000000000000LL // 11- 1: fixed bits

/*
 * return control word:
 * 11t 0HH HVV VLL GGG KKK FFF FFF FFF FFF FFF CCC CCC CCC CCC CCC
 * interrupt return control word:
 * 11b 0HH HVV VLL GGG KKK FFF FFF FFF FFF FFF CCC CCC CCC CCC CCC
 * octet numbers         FEDCBA9876543210
 */
#define INIT_RCW        06000000000000000LL // 11: fixed bits
#define MASK_RCWTYPE    01000000000000000LL // t: type (OPDC/DESC) bit OR
#define MASK_RCWBROF    01000000000000000LL // b: BROF bit

/*
 * interrupt control word:
 * 110 000 RRR RRR RRR 0ms 000 00v 000 00N NNN MMM MMM MMM MMM MMM
 * 444 444 443 333 333 333 222 222 222 211 111 111 110 000 000 000
 * 765 432 109 876 543 210 987 654 321 098 765 432 109 876 543 210
 * octet numbers         FEDCBA9876543210
 */
#define INIT_ICW        06000000000000000LL // 110: fixed bits

/*
 * loop control word (only 39 bits while in X):
 * 110 000 000 0LL rrr rrr FFF FFF FFF FFF FFF CCC CCC CCC CCC CCC
 * octet numbers         FEDCBA9876543210
 */
#define INIT_LCW        06000000000000000LL // 110: fixed bits
#define MASK_LCWrpt     00000770000000000LL // r: repeat count
#define SHFT_LCWrpt     30

/*
 * interrupt loop control word:
 * 11a 000 000 XXX XXX XXX XXX XXX XXX XXX XXX XXX XXX XXX XXX XXX
 * octet numbers         FEDCBA9876543210
 */
#define INIT_ILCW       06000000000000000LL // 11: fixed bits
#define MASK_ILCWAROF   01000000000000000LL // a: saved AROF bit

/*
 * initiate control word:
 * 110 00Q QQQ QQQ QQY YYY YYZ ZZZ ZZ0 ttt ttm SSS SSS SSS SSS SSS
 * octet numbers         FEDCBA9876543210
 */
#define INIT_INCW       06000000000000000LL // 110: fixed bits
#define MASK_INCWQ09F   00100000000000000LL // Q: Q09F bit
#define MASK_INCWQ08F   00040000000000000LL // Q: Q08F bit
#define MASK_INCWQ07F   00020000000000000LL // Q: Q07F bit
#define MASK_INCWQ06F   00010000000000000LL // Q: Q06F bit
#define MASK_INCWQ05F   00004000000000000LL // Q: Q05F bit
#define MASK_INCWQ04F   00002000000000000LL // Q: Q04F bit
#define MASK_INCWQ03F   00001000000000000LL // Q: Q03F bit
#define MASK_INCWQ02F   00000400000000000LL // Q: Q02F bit
#define MASK_INCWQ01F   00000200000000000LL // Q: Q01F bit
#define MASK_INCWrY     00000176000000000LL // Y: Y register
#define MASK_INCWrZ     00000001760000000LL // Z: Z register
#define MASK_INCWrTM    00000000007600000LL // t: TM bits 1-5
#define MASK_INCWMODE   00000000000100000LL // m: word/char mode bit
#define MASK_INCWrS     00000000000077777LL // S: S register
#define SHFT_INCWrY     28
#define SHFT_INCWrZ     22
#define SHFT_INCWrTM    16
#define SHFT_INCWrS     0

/*
 * I/O descriptor or IO-Unit "D" register:
 * --- UUU UUW WWW WWW WWW m-- bdw r-- sss sss AAA AAA AAA AAA AAA
 * octet numbers         FEDCBA9876543210
 */
#define MASK_IODUNIT    00760000000000000LL // U: unit designation
#define MASK_IODMI      00000004000000000LL // m: memory inhibit
#define MASK_IODBINARY  00000000400000000LL // b: binary mode (0=alpha)
#define MASK_IODTAPEDIR 00000000200000000LL // d: tape direction (1=reverse)
#define MASK_IODUSEWC   00000000100000000LL // w: use word counter
#define MASK_IODREAD    00000000040000000LL // r: read mode (0=write)
#define MASK_IODSEGCNT  00000000007700000LL // s: segment count
#define MASK_DSKFILADR  00077777777777777LL // disk file address
#define SHFT_IODUNIT    40
//#define SHFT_IODRESULT  15
#define SHFT_IODSEGCNT  15

/*
 * I/O result bits
 * --m --- --- --p baz ccc --- --- --A FEP NDB AAA AAA AAA AAA AAA
 * octet numbers         FEDCBA9876543210
 */
#define MASK_IORISMOD3  01000000000000000LL // m: TAPE - Model III
#define MASK_IORMEMPAR  00001000000000000LL // p: TAPE - Mem. Parity
#define MASK_IORBLANK   00000400000000000LL // b: TAPE - Blank Tape
#define MASK_IORBOT     00000200000000000LL // a: TAPE - Begin of Tape
#define MASK_IOREOT     00000100000000000LL // z: TAPE - End of Tape
#define MASK_IORCHARS   00000070000000000LL // ccc: TAPE - Chars in last word
#define MASK_IORMAE     00000000010000000LL // A: Memory Access Error
#define MASK_IORD21     00000000004000000LL // F: unit specific END
#define MASK_IORD20     00000000002000000LL // E: unit specific ERR
#define MASK_IORD19     00000000001000000LL // P: unit specific PAR
#define MASK_IORNRDY    00000000000400000LL // N: not ready
#define MASK_IORDPE     00000000000200000LL // D: descriptor parity error
#define MASK_IORBUSY    00000000000100000LL // B: busy
#define SHFT_IORCHARS   30

/*
 * For all single precision operations we use the 64 bits of the host
 * machine's "unsigned long long" (typedef WORD48) to hold the
 * mantissa as follows:
 *
 * Bit 39 holds the carry bit (checked after addition),
 * Bits 38..0 hold the 39 bits of the B5500 mantissa,
 *
 * The exponent, including its sign, is kept in non-B5500 typical two's
 * complement in an integer.
 */
typedef struct num {
        WORD48  m;      // absolute mantissa in above format
        WORD48  x;      // extension of m for right shifts
        int     e;      // signed exponent
        BIT     s;      // sign of mantissa
} NUM;

/* functions available to handle such extracted numbers */
extern void num_extract(WORD48 *, NUM *);
extern void num_compose(NUM *, WORD48 *);
extern void num_left_shift(NUM *, unsigned);
extern unsigned num_left_shift_exp(NUM *, int);
extern unsigned num_right_shift_exp(NUM *, int);
extern void num_right_shift_cnt(NUM *, int);
extern void num_normalize(NUM *, int);
extern void num_round(NUM *);

extern void b5500_pdp_text(CPU *);
extern void b5500_init_shares(void);

/* A & B adjustments, stack operations */
extern BIT incrementS(CPU *);
extern BIT decrementS(CPU *);
extern void adjustABFull(CPU *);
extern void adjustAFull(CPU *);
extern void adjustBFull(CPU *);
extern void adjustABEmpty(CPU *);
extern void adjustAEmpty(CPU *);
extern void adjustBEmpty(CPU *);
extern void exchangeTOS(CPU *);

/* memory accesses */
extern void fetch(ACCESSOR *);
extern void store(ACCESSOR *);
extern void computeRelativeAddr(CPU *, unsigned offset, BIT cEnabled);
extern void loadAviaM(CPU *);
extern void loadBviaM(CPU *);
extern void loadMviaM(CPU *);
extern void loadAviaS(CPU *);
extern void loadBviaS(CPU *);
extern void loadPviaC(CPU *);
extern void storeAviaM(CPU *);
extern void storeBviaM(CPU *);
extern void storeAviaS(CPU *);
extern void storeBviaS(CPU *);
extern void integerStore(CPU *, BIT conditional, BIT destructive);
extern BIT indexDescriptor(CPU *);

/* jumps & calls */
extern void jumpSyllables(CPU *, int count);
extern void jumpWords(CPU *, int count);
extern void jumpOutOfLoop(CPU *, int count);
extern WORD48 buildMSCW(CPU *);
extern void applyMSCW(CPU *, WORD48 mscw);
extern WORD48 buildRCW(CPU *, BIT descriptorCall);
extern BIT applyRCW(CPU *cpu, WORD48 word, BIT no_set_lc, BIT no_bits);
extern void operandCall(CPU *);
extern void descriptorCall(CPU *);
extern void enterSubroutine(CPU *, BIT descriptorCall);
extern int exitSubroutine(CPU *, int how);

/* interrupts & IO */
extern void prepMessage(CPU *);
extern void causeMemoryIrq(CPU *, WORD8, const char *cause);
extern void causeSyllableIrq(CPU *, WORD8, const char *cause);
extern BIT presenceTest(CPU *, WORD48 word);
extern WORD48 interrogateUnitStatus(CPU *);
extern WORD48 interrogateIOChannel(CPU *);
extern void storeForInterrupt(CPU *, BIT forced, BIT forTest, const char *);
extern void clearInterrupt(ADDR15);
extern void initiateIO(CPU *);
extern void signalInterrupt(const char *id, const char *cause);

/* single precision */
extern int singlePrecisionCompare(CPU *);
extern void singlePrecisionAdd(CPU *, BIT add);
extern void singlePrecisionMultiply(CPU *);
extern void singlePrecisionDivide(CPU *);
extern void integerDivide(CPU *);
extern void remainderDivide(CPU *);

/* double precision */
extern void doublePrecisionAdd(CPU *, BIT add);
extern void doublePrecisionMultiply(CPU *);
extern void doublePrecisionDivide(CPU *);

/* stream operations */
extern const WORD6 collation[64];
extern void streamAdjustSourceChar(CPU *);
extern void streamAdjustDestChar(CPU *);
extern void compareSourceWithDest(CPU *, unsigned count, BIT numeric);
extern void fieldArithmetic(CPU *, unsigned count, BIT adding);
extern void streamBitsToDest(CPU *, unsigned count, WORD48 mask);
extern void streamProgramToDest(CPU *, unsigned count);
extern void streamCharacterToDest(CPU *, unsigned count);
extern void streamNumericToDest(CPU *, unsigned count, unsigned zones);
extern void streamBlankForNonNumeric(CPU *, unsigned count);
extern void streamInputConvert(CPU *, unsigned count);
extern void streamOutputConvert(CPU *, unsigned count);

/* misc & CPU control */
extern void enterCharModeInline(CPU *);
extern void initiate(CPU *, BIT forTest);
extern void initiateP2(CPU *);
extern void start(CPU *);
extern void stop(CPU *);
extern void haltP2(CPU *);
extern WORD48 readTimer(CPU *);
extern void preset(CPU *, ADDR15 runAddr);
extern void b5500_execute_cm(CPU *);
extern void b5500_execute_wm(CPU *);
extern void run(CPU *);

/* Richards simulator code */
extern int sim_instr(void);
/* and callbacks */
extern void sim_traceinstr(void);

/* devices */
extern int spo_init(const char *info);
extern void spo_term(void);
extern BIT spo_ready(unsigned index);
extern WORD48 spo_write(WORD48 iocw);
extern WORD48 spo_read(WORD48 iocw);
extern int cr_init(const char *info);
extern void cr_term(void);
extern BIT cr_ready(unsigned index);
extern WORD48 cr_read(WORD48 iocw);
extern int lp_init(const char *info);
extern void lp_term(void);
extern BIT lp_ready(unsigned index);
extern WORD48 lp_write(WORD48 iocw);
extern int mt_init(const char *info);
extern void mt_term(void);
extern BIT mt_ready(unsigned index);
extern WORD48 mt_access(WORD48 iocw);
extern int dk_init(const char *info);
extern void dk_term(void);
extern BIT dk_ready(unsigned index);
extern WORD48 dk_access(WORD48 iocw);

/* console and command line options */
/*
 * command analyzing structure
 */
typedef struct command {
	const char	*cmd;
	int		(*func)(const char *value, void *data);
	void		*data;
} command_t;

extern int command_parser(const command_t *table, const char *op);
extern int handle_option(const char *option);

/* translate tables */
extern const WORD6 translatetable_ascii2bic[128];
extern const WORD8 translatetable_bic2ascii[64];
extern const WORD6 translatetable_bcl2bic[64];

/* other tables */
extern UNIT unit[32][2];

/*
 * bit and field manipulations
 *
 * observe bit numbering (0..47)!
 */
extern void fieldTransfer(
        WORD48 *dest,           // word to insert into
        unsigned wstart,        // starting bit in that word
        unsigned width,         // number of bits
        WORD48 value,           // value to insert
        unsigned vstart);       // starting bit in the value

extern WORD48 fieldIsolate(
        WORD48 word,            // value to isolate from
        unsigned start,         // starting bit in the value
        unsigned width);        // number of bits

extern WORD48 fieldInsert(
        WORD48 word,
        unsigned start,
        unsigned width,
        WORD48 value);

extern void bitSet(WORD48 *, unsigned);
extern void bitReset(WORD48 *, unsigned);
extern BIT bitTest(WORD48, unsigned);

/*
 * for assembler/disassembler
 */
typedef enum optype {
        OP_NONE=0,      // no operand
// parsing
        OP_EXPR,        // operand is expression
        OP_RELA,        // operand is relative address
        OP_BRAS,        // optional operand for branch syllables
        OP_BRAW,        // optional operand for branch words
        OP_REGVAL,      // register and value
// output
        OP_ORG,         // set address
        OP_RUN,         // run program from address and wait for halt
        OP_END,         // end
        OP_SET,         // set a register
        OP_VFY,         // verify a register
        OP_ASIS,        // emit code "as is"
        OP_TOP4,        // emit code | (operand << 8)
        OP_TOP6,        // emit code | (operand << 6)
        OP_TOP10,       // emit code | (operand << 2)
        OP_WORD,        // emit one word
        OP_SYLL,        // emit one syllable
} OPTYPE;

typedef struct instruction {
        const char *name;       // symbolical name
        WORD12  code;           // coding
        OPTYPE  intype;         // operand combination in input
        OPTYPE  outtype;        // operand combination in output
        BIT     cwmf;           // character mode instruction
} INSTRUCTION;

extern int dotrcmem;    // trace memory accesses
extern int dotrcins;    // trace instruction and IRQs
extern int dotrcmat;    // trace math operations
extern int emode;       // emode math
extern const INSTRUCTION instruction_table[];
extern unsigned instr_count;

#endif /* COMMON_H */
