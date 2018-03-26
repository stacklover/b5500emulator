/***********************************************************************
* telnet daemon
************************************************************************
* Copyright (c) 2018, Reinhard Meyer, DL5UY
* Licensed under the MIT License,
*       see LICENSE
************************************************************************
* TELNET server
*
* ff fd 03 DO Suppress Go Ahead
* ff fb 18 WILL Terminal Type
* ff fb 1f WILL Window Size
* ff fb 20 WILL Term Speed
* ff fb 21 WILL Remote Flow
* ff fb 22 WILL Linemode
* ff fb 27 WILL Send Locate
* ff fd 05 DO Opt Status
* ff fb 23
*
* fb (251) WILL
* fc (252) WONT
* fd (253) DO
* fe (254) DONT
* ff (255) IAC
*
************************************************************************
* 2018-03-21  R.Meyer
*   extracted from b5500emulator/dev_dcc.c
***********************************************************************/

#ifndef	_TELNETD_H_
#define	_TELNETD_H_

#define TIMEOUT 1000

// codes
#define	TN_END	240
#define	TN_SUB	250
#define	TN_WILL	251
#define	TN_WONT	252
#define	TN_DO	253
#define	TN_DONT	254
#define	TN_IAC	255

// options
#define	TN_ECHO	1

/***********************************************************************
* the TELNET stuff
***********************************************************************/
enum escape {
	none=0,
	had_iac,
	had_will,
	had_wont,
	had_do,
	had_dont,
	had_sub};

typedef struct telnet_session {
	int		socket;
	enum escape	escape;
	int		is_fullduplex;
} TELNET_SESSION_T;

typedef struct telnet_server {
	int		socket;
} TELNET_SERVER_T;

/***********************************************************************
* the TELNET methods
***********************************************************************/
extern int telnet_session_open(TELNET_SESSION_T *t, int sock);
extern void telnet_session_close(TELNET_SESSION_T *t);
extern void telnet_session_clear(TELNET_SESSION_T *t);
extern int telnet_session_read(TELNET_SESSION_T *t, char *buf, int len);
extern int telnet_session_write(TELNET_SESSION_T *t, const char *buf, int len);
extern int telnet_server_start(TELNET_SERVER_T *ts, unsigned port);
extern void telnet_server_stop(TELNET_SERVER_T *ts);
extern int telnet_server_poll(TELNET_SERVER_T *ts, struct sockaddr_in *addr);
extern void telnet_server_clear(TELNET_SERVER_T *ts);

#endif	/*_TELNETD_H_*/

