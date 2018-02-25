/*
 * C CANbus library
 *
 * Copyright 2016-2018 Reinhard Meyer
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/resource.h> 
#include <unistd.h>
#include <net/if.h> 
#include <linux/socket.h> 
#include <linux/serial.h>
#include <linux/fd.h>
#include <linux/types.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <linux/can/error.h> 

#include "canlib.h"

#define min(a, b)	(((a) < (b)) ? (a) : (b))
#define max(a, b)	(((a) < (b)) ? (b) : (a))
#define abs(x)		(((x) > 0) ? (x) : (-x))
#define sign(x)		(((x) > 0) ? 1 : -1)

// open a CANbus
int can_open(const char *busname)
{
	int i, sock;
	struct ifreq ifr;
	struct sockaddr_can sa;	// sockaddr for bind()
	sock = socket(PF_CAN, SOCK_RAW, CAN_RAW);
	if (sock <= 0) {
		perror("cannot create socket");
		return -1;
	}

	// do not set to non blocking - we use thread to read
	// i = fcntl(sock, F_GETFL, 0);
	// fcntl(sock, F_SETFL, i | O_NONBLOCK);

	// find interface number for name
	strncpy(ifr.ifr_name, busname, sizeof(ifr.ifr_name));
	ifr.ifr_name[sizeof(ifr.ifr_name)-1] = 0;
	if (ioctl(sock, SIOCGIFINDEX, &ifr) < 0) {
		perror("cannot ioctl for interface number");
		close(sock);
		return -1;
	}

	// bind
	sa.can_family = AF_CAN;
	sa.can_ifindex = ifr.ifr_ifindex;
	i = bind(sock, (struct sockaddr *)&sa, sizeof(sa));
	if (i < 0) {
		perror("cannot bind");
		close(sock);
		return -1;
	}
	printf("can bus opened, socket=%d, ifindex=%d\n", sock, ifr.ifr_ifindex);

	return sock;
}

// close a CANbus
void can_close(int sock)
{
	if (sock >= 0)
		close(sock);
}

// read a frame
int can_read(int sock, struct can_frame &frame, struct timeval *tv)
{
	memset(&frame, 0, sizeof frame);
	int res = read(sock, &frame, sizeof frame);
	if (res != 0 && tv != 0)
		ioctl(sock, SIOCGSTAMP, tv);
	return res;
}

// write a frame
int can_write(int sock, struct can_frame &frame)
{
	return write(sock, &frame, sizeof frame);
}


