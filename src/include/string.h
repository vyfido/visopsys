//
//  Visopsys
//  Copyright (C) 1998-2001 J. Andrew McLaughlin
// 
//  This program is free software; you can redistribute it and/or modify it
//  under the terms of the GNU General Public License as published by the Free
//  Software Foundation; either version 2 of the License, or (at your option)
//  any later version.
// 
//  This program is distributed in the hope that it will be useful, but
//  WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
//  or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
//  for more details.
//  
//  You should have received a copy of the GNU General Public License along
//  with this program; if not, write to the Free Software Foundation, Inc.,
//  59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
//
//  string.h
//

// This is the Visopsys version of the standard header file string.h

#if !defined(_STRING_H)

#include <stddef.h>

#define MAXSTRINGLENGTH 512

#ifndef NULL
#define NULL
#endif

// Argh.  We're supposed to define size_t again here?  No, we're including
// stddef.h instead, which is the proper place.
// typedef unsigned int size_t;

// Functions
void *memcpy(void *, const void *, size_t);
void *memmove(void *, const void *, size_t);
int strcasecmp(const char *, const char *);
char *strcat(char *, const char *);
int strcmp(const char *, const char *);
char *strcpy(char *, const char *);
size_t strlen(const char *);
char *strncat(char *, const char *, size_t);
int strncmp(const char *, const char *, size_t);
char *strncpy(char *, const char *, size_t);

#if !defined(__cplusplus)
char *strstr(const char *, const char *);
#endif

// For dealing with unimplemented functions
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

#if !defined(__cplusplus)
// These functions are unimplemented
#define memchr(s, c, n) not_implemented_ptr()
#define strchr(s, c) not_implemented_ptr()
#define strpbrk(s1, s2) not_implemented_ptr()
#define strrchr(s, c) not_implemented_ptr()
#endif

// These functions are unimplemented
#define memcmp(s1, s2, n) not_implemented_int()
#define memset(s, c, n) not_implemented_ptr()
#define strcoll(s1, s2) not_implemented_int()
#define strcspn(s1, s2) not_implemented_uns()
#define strerror(errcode) not_implemented_ptr()
#define strspn(s1, s2) not_implemented_uns()
#define strtok(s1, s2) not_implemented_ptr()
#define strxfrm(s1, s2, n) not_implemented_uns()

#define _STRING_H
#endif
