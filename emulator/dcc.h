/***********************************************************************
* b5500emulator
************************************************************************
* Copyright (c) 2018, Reinhard Meyer, DL5UY
* Licensed under the MIT License,
*       see LICENSE
************************************************************************
* b5500 data communication emulation (DCC)
************************************************************************
* 2018-02-14  R.Meyer
*   Frame from dev_spo.c
***********************************************************************/

#ifndef	_DCC_H_
#define	_DCC_H_

#define NUMTERM 32
#define	NUMSERV 3
#define TRACE_DCC 0

// Special Codes 
#define	EOM	'~'	// (BIC: left arow) Marks (premature) Buffer End
#define	MODE	'!'	// (BIC: not equal) toggles Control/Printable Mode
			// at Line Discipline "contention"

/***********************************************************************
* ASCII control codes
* the codes for LF, RS, US are also used in the display memory
***********************************************************************/
#define	NUL	0x00	// time fill - ignored
#define	STX	0x02	// start of text during transmission
#define	ETX	0x03	// end of text during transmission
#define	EOT	0x04	// end of transmission, turn over
#define	ENQ	0x05	// status request
#define	ACK	0x06	// positive acknowledge
#define	BEL	0x07	// ring the bell
#define	BS	0x08	// cursor left
#define	TAB	0x09	// forward to tab stop
#define	LF	0x0a	// cursor down
#define	FF	0x0c	// clear screen
#define	CR	0x0d	// newline - move to start of next line
#define	DC1	0x11	// line erase
#define	DC2	0x12
#define	DC3	0x13	// cursor up
#define	DC4	0x14	// cursor home
#define	NAK	0x15	// negative acknowledge
#define	ESC	0x1b	// escape
#define	RS	0x1e	// "shift out" - protected area start
#define	US	0x1f	// "shift in" - protected area end
#define	RUBOUT	0xff	// rubout - punch all holes on tape

// tnr/bnr to index and back
#define	IDX(tnr,bnr)	(((tun)-1)*16+(bnr))
#define	TUN(index)	((index)/16+1)
#define	BNR(index)	((index)%16)

// buffer sizes
#define	SYSBUFSIZE	112
#define	INBUFSIZE	200
#define	OUTBUFSIZE	200
#define	KEYBUFSIZE	100

/***********************************************************************
* the sysbuf states
*
* Note: each line has a single sysbuf that is used for
* reading or writing, not both at the same time
* 112 chars is the maximum that SYSDISK/MAKER accepts, half of that if
* ping-pong buffering is used.
* It seems more efficient that we do not use ping-pong buffering.
* The sysbuf state is used as follows:
* notready:	the terminal is not connected/ready
* idle:		the sysbuf can be used for reading or writing
* inputbusy:	this state is not used. It would normally signal that
*		the sysbuf is currently being filled with input data
* readready:	the sysbuf has been filled with read data, waiting for
*		the system to collect it. After collection it will be
*		set to idle
* outputbusy:	the sysbuf has been filled with write data, waiting for
*		the line to send it. After sending it will be set to
*		idle, if the last sysbuf was not a full sysbuf, or set
*		to writeready to receive more data to send
* writeready:	the sysbuf is empty, but the system is expected to send
*		more data
***********************************************************************/
enum bufstate {
	notready=0,	// sysbuf cannot be used
	idle,		// sysbuf can be used for read OR write
	inputbusy,	// sysbuf is blocked because data arrives
	readready,	// sysbuf can be read by system
	outputbusy,	// sysbuf is filled but has not been sent
	writeready};	// sysbuf awaits further data

/***********************************************************************
* the line discipline state
***********************************************************************/
enum lds {
	lds_idle=0,	// idle (after receiving EOT
	lds_recvenq,	// ENQ has been received and responded
	lds_recvdata,	// receiving user data
	lds_recvetx,	// ETX has been received
	lds_sendrdy,	// data ready for sending
	lds_sentenq,	// ENQ has been sent
	lds_sentdata};	// userdata has been sent

/***********************************************************************
* the line discipline used
***********************************************************************/
enum ld {
	ld_teletype=0,	// no protocol, except input buffer editing
	ld_contention};	// burroughs contention (half duplex)

/***********************************************************************
* the terminal emulation
***********************************************************************/
enum em {
	em_none=0,	// no emulation
	em_teletype,	// emulate B9352 for line oriented (TELETYPE)
	em_ansi};	// emulate B9352 for external ANSI terminal

/***********************************************************************
* the complete terminal state
***********************************************************************/
typedef struct terminal {
	TELNET_SESSION_T session;	// TELNET state values
	// system communication buffer
	char sysbuf[SYSBUFSIZE];	// buffer with raw data from/to system
	int sysidx;			// number of chars in sysbuf
	enum bufstate bufstate;		// current state of sysbuf
	BIT fullbuffer;
	// input ring buffer (to system)
	CIRCBUFFER_T inbuf;
	// output ring buffer (from system)
	CIRCBUFFER_T outbuf;
	// keyboard edit buffer
	char keybuf[KEYBUFSIZE];	// buffer for keyboard editing
	int keyidx;			// number of chars in keybuf
	// line discipline/emulation
	enum ld ld;			// line discipline
	enum em em;			// emulation
	enum lds lds;			// line discipline state
	BIT lfpending;
	BIT paused;
	// status bits
	BIT connected;
	BIT interrupt;
	BIT abnormal;
	// control escape mode
	BIT inmode;
	BIT outmode;
	BIT outlastwasmode;
	// supervisory values
	int eotcount;
	int timer;
} TERMINAL_T;

/***********************************************************************
* trace flags
***********************************************************************/
extern BIT etrace;
extern BIT dtrace;
extern BIT ctrace;

/***********************************************************************
* xxx emulation write:
* called when the sysbuf has been written to
***********************************************************************/
extern void teletype_emulation_write(TERMINAL_T *t);
extern void b9352_emulation_write(TERMINAL_T *t);

/***********************************************************************
* xxx emulation poll:
* called cyclically. should check for
* - TELNET reception
***********************************************************************/
extern int teletype_emulation_poll(TERMINAL_T *t);
extern int b9352_emulation_poll(TERMINAL_T *t);

/***********************************************************************
* dcc input ready
* called when the sysbuf is input ready
***********************************************************************/
extern void dcc_input_ready(TERMINAL_T *t);

#endif	//_DCC_H_

