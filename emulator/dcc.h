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
#define	NUMSERV 2
#define TRACE_DCC 0

// Special Codes 
#define	EOM	'~'	// (BIC: left arow) Marks (premature) Buffer End
#define	MODE	'!'	// (BIC: not equal) toggles Control/Printable Mode
			// at Line Discipline "contention"
/***********************************************************************
* defines for screen resolution and storage
***********************************************************************/
#define	COLS	80	// number of characters per row
#define	ROWS	25	// number of rows

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
#define	RUBOUT	0x7f	// rubout - punch all holes on tape

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
* the contention line discipline state
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
	ld_teletype=0,	// teletype discipline, except input buffer editing
	ld_contention};	// burroughs contention (half duplex)

/***********************************************************************
* the terminal emulation (used with ld_contention only)
***********************************************************************/
enum em {
	em_none=0,	// no emulation, send/receive raw data
	em_teletype,	// emulate B9352 for external TELETYPE
	em_ansi};	// emulate B9352 for external ANSI terminal

/***********************************************************************
* the physical connection
***********************************************************************/
enum pc {
	pc_none=0,	// no connection
	pc_serial,	// via tty device
	pc_canopen,	// via CANopen
	pc_telnet};	// via TELNET server

/***********************************************************************
* the physical connection state
***********************************************************************/
enum pcs {
	pcs_disconnected=0,	// not connected
	pcs_pending,		// connected, pending further verification
	pcs_aborted,		// connected, now aborted
	pcs_connected,		// connected and also connected to system
	pcs_failed};		// connection failed and will be closed

/***********************************************************************
* the complete terminal state
***********************************************************************/
typedef struct terminal {
	char name[10];			// printable name
// physical connection
	enum pc pc;			// physical connection
	enum pcs pcs;			// physical connection state
	char peer_info[80];		// identification of peer
	// pc = pc_serial
	int serial_handle;		// handle of open tty device
	// pc = pc_canopen
	unsigned canid;			// canif of terminal
	// pc = pc_telnet
	TELNET_SESSION_T session;	// TELNET state values
// system communication buffer
	char sysbuf[SYSBUFSIZE];	// buffer with raw data from/to system
	int sysidx;			// number of chars in sysbuf
	enum bufstate bufstate;		// current state of sysbuf
	BIT fullbuffer;
// input buffer
	char inbuf[SYSBUFSIZE];		// buffer simulating line from terminal
	int inidx;
// output buffer
	char outbuf[SYSBUFSIZE];	// buffer simulating line to terminal
	int outidx;
// keyboard edit buffer
	char keybuf[KEYBUFSIZE];	// buffer for keyboard editing
	int keyidx;			// number of chars in keybuf
	BIT escaped;
// line discipline/emulation
	enum ld ld;			// line discipline
	enum em em;			// emulation
	enum lds lds;			// line discipline state
// status bits to system
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
// screen buffer
	char scrbuf[ROWS*COLS];		// screen buffer
	int scridx, scridy;		// index into screen (cursor position, zero based)
	BIT lfpending;
	BIT paused;
	BIT utf8mode;
	BIT insertmode;
// tracing file
	FILE *trace;
} TERMINAL_T;

/***********************************************************************
* trace flags
***********************************************************************/
extern BIT etrace;
extern BIT dtrace;
extern BIT ctrace;

/***********************************************************************
* physical connection by TELNET
***********************************************************************/
extern void pc_telnet_init(void);
extern void pc_telnet_poll(BIT telnet);
extern void pc_telnet_poll_terminal(TERMINAL_T *t);
extern int pc_telnet_read(TERMINAL_T *t, char *buf, int len);
extern int pc_telnet_write(TERMINAL_T *t, char *buf, int len);

/***********************************************************************
* physical connection by CANopen
***********************************************************************/
extern void pc_canopen_init(void);
extern void pc_canopen_poll(void);
extern void pc_canopen_poll_terminal(TERMINAL_T *t);
extern int pc_canopen_read(TERMINAL_T *t, char *buf, int len);
extern int pc_canopen_write(TERMINAL_T *t, char *buf, int len);

/***********************************************************************
* physical connection by SERIAL
***********************************************************************/
extern void pc_serial_init(void);
extern void pc_serial_poll(void);
extern void pc_serial_poll_terminal(TERMINAL_T *t);
extern int pc_serial_read(TERMINAL_T *t, char *buf, int len);
extern int pc_serial_write(TERMINAL_T *t, char *buf, int len);

/***********************************************************************
* line discipline write:
* called when the sysbuf has been written to
***********************************************************************/
extern void ld_write_teletype(TERMINAL_T *t);
extern void ld_write_contention(TERMINAL_T *t);

/***********************************************************************
* line discipline poll:
* called cyclically. should check for
* status changes and user input
***********************************************************************/
extern int ld_poll_teletype(TERMINAL_T *t);
extern int ld_poll_contention(TERMINAL_T *t);

/***********************************************************************
* dcc input ready
* called when the sysbuf is input ready
***********************************************************************/
extern void dcc_input_ready(TERMINAL_T *t);

/***********************************************************************
* dcc find free terminal
***********************************************************************/
extern TERMINAL_T *dcc_find_free_terminal(enum ld ld);
extern void dcc_init_terminal(TERMINAL_T *t);
extern void dcc_report_connect(TERMINAL_T *t);
extern void dcc_report_disconnect(TERMINAL_T *t);

/***********************************************************************
* B9352 emulation input/output
***********************************************************************/
extern int b9352_input(TERMINAL_T *t, char ch);
extern int b9352_output(TERMINAL_T *t, char ch);

#endif	//_DCC_H_

