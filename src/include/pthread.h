//
//  Visopsys
//  Copyright (C) 1998-2021 J. Andrew McLaughlin
//
//  This library is free software; you can redistribute it and/or modify it
//  under the terms of the GNU Lesser General Public License as published by
//  the Free Software Foundation; either version 2.1 of the License, or (at
//  your option) any later version.
//
//  This library is distributed in the hope that it will be useful, but
//  WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
//  General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public License
//  along with this library; if not, write to the Free Software Foundation,
//  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
//
//  pthread.h
//

// This file contains definitions and structures for using the POSIX thread
// library in Visopsys.

#ifndef _PTHREAD_H
#define _PTHREAD_H

typedef struct {
	int pad;

} pthread_attr_t;

typedef int pthread_t;

// Functions exported by libpthread
int pthread_attr_destroy(pthread_attr_t *);
int pthread_attr_init(pthread_attr_t *);
int pthread_cancel(pthread_t);
int pthread_create(pthread_t *, const pthread_attr_t *, void *(*)(void *),
	void *);
void pthread_exit(void *);
int pthread_join(pthread_t, void **);
pthread_t pthread_self(void);

#endif

