// 
//  Visopsys
//  Copyright (C) 1998-2004 J. Andrew McLaughlin
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

#include <stddef.h>
#include <limits.h>

#define EXIT_FAILURE  -1
#define EXIT_SUCCESS  0
#define MB_CUR_MAX    MB_LEN_MAX
#define RAND_MAX      UINT_MAX

#ifndef NULL
#define NULL 0
#endif

typedef struct
{
  int quot;
  int rem;
} div_t;

typedef struct
{
  long quot;
  long rem;
} ldiv_t;


// We're supposed to define size_t here???  Same with wchar_t???  I thought
// they were defined in stddef.h.  Oh well, we're including stddef.h anyway.

// Functions
void abort(void);
int abs(int);
int atoi(const char *);
void *calloc(size_t, size_t);
div_t div(int, int);
void exit(int);
void free(void *);
long int labs(long int);
ldiv_t ldiv(long int, long int);
void *malloc(size_t);
int rand(void);
void *realloc(void *, size_t);
void srand(unsigned int);
int system(const char *);

// For dealing with unimplemented functions
#define not_implemented_void()   \
  {                              \
    errno = ERR_NOTIMPLEMENTED;  \
    return;                      \
  }
#define not_implemented_int()    \
  {                              \
    errno = ERR_NOTIMPLEMENTED;  \
    return (ERR_NOTIMPLEMENTED); \
  }
#define not_implemented_ptr()    \
  {                              \
    errno = ERR_NOTIMPLEMENTED;  \
    return (NULL);               \
  }
#define not_implemented_uns()    \
  {                              \
    errno = ERR_NOTIMPLEMENTED;  \
    return 0;                    \
  }

// These functions are unimplemented
int atexit(void (*)(void));
double atof(const char *);
long atol(const char *);
void *bsearch(const void *, const void *, size_t, size_t size,
	      int (*)(const void *, const void *));
char *getenv(const char *);
#define mblen(s, n) not_implemented_int()
#define mbstowcs(wcs, s, n) not_implemented_uns()
#define mbtowc(pwc, s, n) not_implemented_int()
#define qsort(base, nelem, size, cmp) not_implemented_void()
#define strtod(s, endptr) not_implemented_int()
#define strtol(s, endptr, base) not_implemented_int()
#define strtoul(s, endptr, base) not_implemented_uns()
#define wcstombs(s, wcs, n) not_implemented_uns()
#define wctomb(s, wchar) not_implemented_int()

// Argh.  Isn't there a function that does these?  These are andy-special.
void itoa(int, char *);
void itox(int, char *);
int xtoi(const char *);
void utoa(unsigned int, char *);

#define _STDLIB_H
#endif
