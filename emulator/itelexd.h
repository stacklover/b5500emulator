/***********************************************************************
* iTELEX server
************************************************************************
* Copyright (c) 2020, Reinhard Meyer, DL5UY
* Licensed under the MIT License,
*       see LICENSE
************************************************************************
* iTELEX server
*
* all packets are TLV:
* 00 00			Heartbeat
* 01 01 <no>		Direct Dial
* 02 nn <data>		ITA2 data (len 1..254)
* 03 00			End
* 04 nn <reason>	in ASCII! (len 0..20)
* 06 01 <count>		Acknowledge (count modulo 256)
* 07 nn <Version>	(len 1..20)
* 08 nn <Pattern>	Selftest (len 2..254)
* 09 nn <config data>	Remote Config (len 3..254)
*
************************************************************************
* 2020-03-09  R.Meyer
*   copied and modified from telnetd.h
***********************************************************************/

#ifndef	_ITELEXD_H_
#define	_ITELEXD_H_

#define TIMEOUT 1000

// iTELEX codes
#define	IT_HBT	0x00
#define	IT_DDL	0x01
#define	IT_BAU	0x02
#define	IT_END	0x03
#define	IT_REJ	0x04
// ENQ reserved
#define	IT_ACK	0x06
#define	IT_VER	0x07
#define	IT_TST	0x08
#define	IT_RCF	0x09
// LF reserved
#define	IT_ASC	0x0b

// translatetable bits and masks
#define TAB_MASK 0x1f
#define TAB_UNSHIFT 0x20
#define TAB_SHIFT 0x40
#define TAB_ESCAPE 0x80

// special ITA2 codes
#define ITA2_UNSHIFT 0x1f
#define ITA2_SHIFT 0x1b
#define ITA2_CR 0x08
#define ITA2_LF 0x02
#define ITA2_SPACE 0x04
#define ITA2_NULL 0x00

// work buffer length
#define	IT_BUFLEN	260

/***********************************************************************
* the iTELEX stuff
***********************************************************************/
typedef struct itelex_session {
	int		socket;
	// flags
	unsigned	success_mask;
	int		baudot;
	// iTELEX packet assembly
	int		bidx;		// buffer index (0 = start of buffer)
	unsigned char	buf[IT_BUFLEN];	// buffer
	unsigned char	snr, sack, rnr;	// character counts each direction
	int		tbuzi, rbuzi;	// letter/figure mode both directions
	// negotiated terminal values
} ITELEX_SESSION_T;

typedef struct itelex_server {
	int		socket;
} ITELEX_SERVER_T;

/***********************************************************************
* the ITELEX methods
***********************************************************************/
extern int itelex_session_open(ITELEX_SESSION_T *t, int sock);
extern void itelex_session_close(ITELEX_SESSION_T *t);
extern void itelex_session_clear(ITELEX_SESSION_T *t);
extern int itelex_session_read(ITELEX_SESSION_T *t, char *buf, int len);
extern int itelex_session_write(ITELEX_SESSION_T *t, const char *buf, int len);
extern int itelex_server_start(ITELEX_SERVER_T *ts, unsigned port);
extern void itelex_server_stop(ITELEX_SERVER_T *ts);
extern int itelex_server_poll(ITELEX_SERVER_T *ts, struct sockaddr_in *addr);
extern void itelex_server_clear(ITELEX_SERVER_T *ts);

#endif	/*_ITELEXD_H_*/

