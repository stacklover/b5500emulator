/*
===============================================================================
 Name        : circbuffer.c
 Author      : Reinhard Meyer, DL5UY
 Version     : 0.00
 Copyright   : Copyright (C) 2018 Reinhard Meyer, DL5UY
 Description : handling of circular buffers
===============================================================================
*/

#include <stdlib.h>
#include "circbuffer.h"

void circ_clear(CIRCBUFFER_T *cb) {
	cb->rp = cb->wp = cb->buf;
	cb->used = 0;
}

int circ_create(CIRCBUFFER_T *cb, int size) {
	cb->rp = cb->wp = cb->buf = (unsigned char *)malloc(size);
	if (cb->buf == 0)
		return -1;
	cb->length = size;
	cb->used = 0;
	cb->ep = cb->buf + size;
	return 0;
}

void circ_destroy(CIRCBUFFER_T *cb) {
	free(cb->buf);
	cb->ep = cb->rp = cb->wp = cb->buf = NULL;
	cb->length = 0;
	cb->used = 0;
}

int circ_space(CIRCBUFFER_T *cb) {
	return cb->length - cb->used;
}

int circ_used(CIRCBUFFER_T *cb) {
	return cb->used;
}

int circ_write(CIRCBUFFER_T *cb, unsigned char v) {
	if (cb->length - cb->used < 1)
		return -1;
	*cb->wp++ = v;
	if (cb->wp >= cb->ep)
		cb->wp = cb->buf;
	++cb->used;
	return 1;
}

int circ_read(CIRCBUFFER_T *cb) {
	int v;
	if (cb->used < 1)
		return -1;
	v = *cb->rp++;
	if (cb->rp >= cb->ep)
		cb->rp = cb->buf;
	--cb->used;
	return v;
}


