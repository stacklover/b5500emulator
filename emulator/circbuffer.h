/*
===============================================================================
 Name        : circbuffer.h
 Author      : Reinhard Meyer, DL5UY
 Version     : 0.00
 Copyright   : Copyright (C) 2018 Reinhard Meyer, DL5UY
 Description : handling of circular buffers
===============================================================================
*/

#ifndef _circbuffer_h
#define _circbuffer_h

typedef struct circbuffer {
	unsigned char	*buf;	// pointer to buffer area
	unsigned char	*rp;	// read pointer
	unsigned char	*wp;	// write pointer
	unsigned char	*ep;	// end pointer
	unsigned	length;	// length of buffer
	unsigned	used;	// bytes used in buffer
} CIRCBUFFER_T;

extern int circ_init(CIRCBUFFER_T *cb, int size);
extern int circ_space(CIRCBUFFER_T *cb);
extern int circ_used(CIRCBUFFER_T *cb);
extern int circ_write(CIRCBUFFER_T *cb, unsigned char v);
extern int circ_read(CIRCBUFFER_T *cb);

#endif
