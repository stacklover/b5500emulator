/*
 * C CANbus library
 *
 * Copyright 2016-2017 Reinhard Meyer
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#ifndef _canlib_h
#define _canlib_h

extern	int	can_open(const char *busname);
extern	int	can_read(int busfd, struct can_frame &frame, struct timeval *tv = 0);
extern	int	can_write(int busfd, struct can_frame &frame);
extern	void	can_close(int busfd);

/* CANopen msg ids */
#define	MSGID_NMT			(0x000)
#define	MSGID_SYNC			(0x080)
#define	MSGID_TIME			(0x100)
#define	MSGID_EMERGENCY(x)		(0x080+(x))
#define	MSGID_RES100(x)			(0x100+(x))
#define	MSGID_TPDO1(x)			(0x180+(x))
#define	MSGID_RPDO1(x)			(0x200+(x))
#define	MSGID_TPDO2(x)			(0x280+(x))
#define	MSGID_RPDO2(x)			(0x300+(x))
#define	MSGID_TPDO3(x)			(0x380+(x))
#define	MSGID_RPDO3(x)			(0x400+(x))
#define	MSGID_TPDO4(x)			(0x480+(x))
#define	MSGID_RPDO4(x)			(0x500+(x))
#define	MSGID_TSDO(x)			(0x580+(x))
#define	MSGID_RSDO(x)			(0x600+(x))
#define	MSGID_RES680(x)			(0x680+(x))
#define	MSGID_ERRCONT(x)		(0x700+(x))
#define	MSGID_RES780(x)			(0x780+(x))

/* CANopen NMT module control command */
#define NMT_CMD_START			1
#define NMT_CMD_STOP			2
#define	NMT_CMD_POWEROFF		64
#define NMT_CMD_ENTER_PRE_OP		128
#define NMT_CMD_RESET_NODE		129
#define NMT_CMD_RESET_COMM		130

/* CANopen Node State */
#define NMT_STATE_INTIALIZING		0
#define NMT_STATE_STOPPED		4
#define NMT_STATE_OPERATIONAL		5
#define	NMT_STATE_POWEROFF		64
#define	NMT_STATE_CONFIGMODE		65
#define NMT_STATE_PRE_OPERATIONAL	127

/* CANopen SDO Bits */
#define	CANopen_SDO_FUNCTION_MASK	(7<<5)
// Requests
#define	CANopen_SDO_DOWNLOAD_SEG_REQ	(0<<5)
#define	CANopen_SDO_INIT_DOWNLOAD_REQ	(1<<5)
#define	CANopen_SDO_INIT_UPLOAD_REQ	(2<<5)
#define	CANopen_SDO_UPLOAD_SEG_REQ	(3<<5)
// Responses
#define	CANopen_SDO_UPLOAD_SEG_RSP	(0<<5)
#define	CANopen_SDO_DOWNLOAD_SEG_RSP	(1<<5)
#define	CANopen_SDO_INIT_UPLOAD_RSP	(2<<5)
#define	CANopen_SDO_INIT_DOWNLOAD_RSP	(3<<5)
// Abort
#define	CANopen_SDO_ABORT_RSP		(4<<5)

#define	CANopen_SDO_TOGGLE_BIT		(1<<4)

#define	CANopen_SDO_SIZE2_MASK		(3<<2)
#define	CANopen_SDO_GET_SIZE2(x)	(((x)&CANopen_SDO_SIZE2_MASK)>>2)
#define	CANopen_SDO_SET_SIZE2(x)	(((x)<<2)&CANopen_SDO_SIZE2_MASK)

#define	CANopen_SDO_SIZE3_MASK		(7<<1)
#define	CANopen_SDO_GET_SIZE3(x)	(((x)&CANopen_SDO_SIZE3_MASK)>>1)
#define	CANopen_SDO_SET_SIZE3(x)	(((x)<<1)&CANopen_SDO_SIZE3_MASK)

#define	CANopen_SDO_EXPEDITED		(1<<1)

#define	CANopen_SDO_SIZEIND		(1<<0)
#define	CANopen_SDO_COMPLETE		(1<<0)

/* CANopen SDOC State */
#define CANopen_SDOC_Fail		0
#define CANopen_SDOC_Succes		1
#define CANopen_SDOC_Exp_Read_Busy	2
#define CANopen_SDOC_Exp_Write_Busy	3
#define CANopen_SDOC_Seg_Read_Busy	4
#define CANopen_SDOC_Seg_Write_Busy	5

#define NODEID_MASK			0x07F
#define COB_MASK			0x780

#endif

