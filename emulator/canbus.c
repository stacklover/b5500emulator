/***********************************************************************
* b5500emulator
************************************************************************
* Copyright (c) 2018, Reinhard Meyer, DL5UY
* Licensed under the MIT License,
*       see LICENSE
************************************************************************
* b5500 emulator canbus functions
************************************************************************
* 2017-09-08  R.Meyer
*   Started
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
#include <time.h>
#include "common.h"
#include "io.h"

#include <linux/can.h>
#include <linux/can/raw.h>
#include <linux/can/error.h> 
#include "canlib.h"

#define CANSTRINGLENGTH 200

/***********************************************************************
* CANbus data
***********************************************************************/
static int canfd = -1;
static pthread_t canbus_reader;
static timer_t timerid;
static struct sigevent sev;
static struct itimerspec its;

typedef struct can {
	// input buffer
	char	buf[CANSTRINGLENGTH];
	int	bufidx;
	// peer ready
	int	ready;
	struct timeval last_ready;
	// peer status
	int	space;
} CAN_T;

static CAN_T can[128];

/***********************************************************************
* CANbus read thread
***********************************************************************/
static void *reader_function(void *p) {
	struct can_frame frame;
	struct timeval tv;
	int i;
loop:
	// read CANbus - this will block until something is there to read or an error occurs
	i = can_read(canfd, frame, &tv);
	if (i >= 0) {
#if CAN_TRACE
		unsigned mask = 0;
		unsigned val;
		int ms_stamp = (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
		printf("@%u: %08X: %X", ms_stamp, frame.can_id, frame.can_dlc);
		for (int j=0; j<frame.can_dlc; j++)
			printf(" %02X", frame.data[j]);
		printf("\n");
#endif
		// analyze the received frame and react upon it
		unsigned id = frame.can_id & NODEID_MASK;
		unsigned cob = frame.can_id & COB_MASK;

		if (cob == MSGID_ERRCONT(0)) {
			// common code for all stations
			if (frame.can_dlc == 1 && frame.data[0] == NMT_STATE_PRE_OPERATIONAL) {
				// station reports pre-operational: start it
				can[id].ready = 0;
				frame.can_id = MSGID_NMT;
				frame.can_dlc = 2;
				frame.data[0] = NMT_CMD_START;
				frame.data[1] = id;
				can_write(canfd, frame);
			} else if (frame.can_dlc == 1 && frame.data[0] == NMT_STATE_OPERATIONAL) {
				can[id].last_ready = tv;
				// station reports operational: actions if not yet seen
				if (can[id].ready == 0) {
					can[id].ready = 1;
					if (id < 32)
						printf("$CAN %s operational\n", unit[id][1].name);
					else
						printf("$CAN %d operational\n", id);
				}
			}
		} else if (cob == MSGID_TPDO4(0)) {
			// TPDO4: received data
			can[id].space = frame.data[0] & 0x7f;
			// process chars into buffer
			for (int j=1; j<frame.can_dlc; j++) {
				char ch = frame.data[j];
				if (can[id].bufidx < CANSTRINGLENGTH-1) {
					can[id].buf[can[id].bufidx++] = ch;
				}
			}
		}
	} else {
		// error?
		printf("$CAN read returned %d \n", i);
	}
	goto loop;
	return NULL;
}

/***********************************************************************
* CANbus timer thread
***********************************************************************/
static void timer_function(sigval) {
	struct timeval tv;
	int id;
	gettimeofday(&tv, NULL);
	for (id=1; id<128; id++) {
		if (can[id].ready && (tv.tv_sec - can[id].last_ready.tv_sec) > 5) {
			can[id].ready = false;
			if (id < 32)
				printf("$CAN %s not operational\n", unit[id][1].name);
			else
				printf("$CAN %d not operational\n", id);
		}
	}
}

/***********************************************************************
* query readyness of a CANBUS unit
***********************************************************************/
int can_ready(unsigned id) {
	return can[id].ready;
}

/***********************************************************************
* WRITE to CANBUS
***********************************************************************/
int can_write(unsigned id, const char *buf, int len)
{
	// possible at all?
	if (canfd >= 0 && can[id].ready > 0) {
		const char *p = buf;
		struct can_frame frame;
		frame.can_id = MSGID_RPDO4(id);
		while (len) {
			frame.can_dlc = 1;
			frame.data[0] = 0x80;
			while (len && frame.can_dlc <= 7) {
				frame.data[frame.can_dlc] = *p++;
				frame.can_dlc++;
				len--;
			}
			can_write(canfd, frame);
			do {
				usleep(300000);
			} while (can[id].space < 100);
		}
		return p - buf;
	}
	return 0;
}

/***********************************************************************
* READ from CANBUS
***********************************************************************/
int can_read(unsigned id, char *buf, int maxlen)
{
	// possible at all?
	if (canfd >= 0 && can[id].ready > 0) {
		int cnt = can[id].bufidx;
		if (cnt < 0 || cnt >= CANSTRINGLENGTH) {
			printf("can_read: invalid buffer index %d - resetting\n", cnt);
			can[id].bufidx = 0;
			return 0;
		}
		if (cnt > maxlen)
			cnt = maxlen;
		if (cnt < 1)
			return cnt;
		memmove(buf, can[id].buf, cnt);
		can[id].bufidx -= cnt;
		memmove(can[id].buf, can[id].buf + cnt, can[id].bufidx);
		return cnt;
	}
	return -1;
}

/***********************************************************************
* initialise CANBUS
***********************************************************************/
void can_init(const char *busname)
{
	int i;
	for (i=0; i<128; i++) {
		can[i].ready = 0;
		can[i].bufidx = 0;
		can[i].last_ready.tv_sec = 0;
		can[i].last_ready.tv_usec = 0;
	}
	canfd = can_open(busname);
	if (canfd >= 0) {
		// reader thread
		pthread_create(&canbus_reader, 0, reader_function, 0);

		// Create the timer
		sev.sigev_notify = SIGEV_THREAD;
		sev.sigev_notify_function = timer_function;
		sev.sigev_value.sival_int = 0;
		if (timer_create(CLOCK_MONOTONIC, &sev, &timerid) == -1) {
			perror("timer_create");
			exit(2);
		}
		//printf("timer ID is 0x%lx\n", (long) timerid);

		// Start the timer
		its.it_value.tv_sec = 1;
		its.it_value.tv_nsec = 0;
		its.it_interval.tv_sec = its.it_value.tv_sec;
		its.it_interval.tv_nsec = its.it_value.tv_nsec;
		if (timer_settime(timerid, 0, &its, NULL) == -1) {
			perror("timer_settime");
			exit(2);
		}
	}
}


