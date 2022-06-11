// 
//  Visopsys
//  Copyright (C) 1998-2007 J. Andrew McLaughlin
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
//  Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
//
//  stdlib.h
//

// This is the Visopsys version of the standard header file stdlib.h

#if !defined(_STDLIB_H)

#include <limits.h>
#include <stddef.h>
#include <sys/cdefs.h>

#define EXIT_FAILURE  -1
#define EXIT_SUCCESS  0
#define MB_CUR_MAX    MB_LEN_MAX
#define RAND_MAX      UINT_MAX

#ifndef NULL
#define NULL 0
#endif

// We're supposed to define size_t here???  Same with wchar_t???  I thought
// they were defined in stddef.h.  Oh well, we're including stddef.h anyway.

// Functions
void abort(void) __attribute__((noreturn));
int abs(int);
#define atoi(string) ((int) _str2num(string, 10, 1))
#define atoll(string) ((long long) _str2num(string, 10, 1))
void *_calloc(size_t, size_t, const char *);
#define calloc(num, size) _calloc(num, size, __FUNCTION__)
void exit(int) __attribute__((noreturn));
void _free(void *, const char *);
#define free(ptr) _free(ptr, __FUNCTION__)
long int labs(long int);
void *_malloc(size_t, const char *);
#define malloc(size) _malloc(size, __FUNCTION__)
int mbtowc(wchar_t *, const char *, size_t);
size_t mbstowcs(wchar_t *dest, const char *src, size_t n);
int rand(void);
void *_realloc(void *, size_t, const char *);
#define realloc(ptr, size) _realloc(ptr, size, __FUNCTION__)
char *realpath(const char *, char *);
void srand(unsigned int);
int system(const char *);
int wctomb(char *, wchar_t);

// Not sure where else to put these
#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))
#define offsetof(type, field) ((unsigned long) &(((type *)0L)->field))

// These are unofficial, Andy-special extensions of the atoi() and atoll()
// paradigm.
#define atou(string) ((unsigned) _str2num(string, 10, 0))
#define atoull(string) _str2num(string, 10, 0)
#define itoa(num, string) _num2str(num, string, 10, 1)
#define itoux(num, string) _num2str(num, string, 16, 0)
#define itox(num, string) _num2str(num, string, 16, 1)
#define lltoa(num, string) _lnum2str(num, string, 10, 1)
#define lltoux(num, string) _lnum2str(num, string, 16, 0)
#define lltox(num, string) _lnum2str(num, string, 16, 1)
#define xtoi(string) ((int) _str2num(string, 16, 1))
#define xtoll(string) ((long long) _str2num(string, 16, 1))
#define ulltoa(num, string) _lnum2str(num, string, 10, 0)
#define utoa(num, string) _num2str(num, string, 10, 0)

#define _STDLIB_H
#endif
