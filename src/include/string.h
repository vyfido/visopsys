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
//  string.h
//

// This is the Visopsys version of the standard header file string.h

#if !defined(_STRING_H)

#include <stddef.h>

#define MAXSTRINGLENGTH 1024

#ifndef NULL
#define NULL
#endif

// Functions
void bcopy(const void *, void *, size_t n);
void bzero(void *, size_t);
char *index(const char *, int);
int memcmp(const void *, const void *, size_t);
void *memcpy(void *, const void *, size_t);
void *memmove(void *, const void *, size_t);
void *memset(void *, int, size_t);
char *rindex(const char *, int);
char *strcat(char *, const char *);
int strcmp(const char *, const char *);
int strcasecmp(const char *, const char *);
char *strcpy(char *, const char *);
size_t strlen(const char *);
char *strncat(char *, const char *, size_t);
int strncmp(const char *, const char *, size_t);
int strncasecmp(const char *, const char *, size_t);
char *strncpy(char *, const char *, size_t);
size_t strspn(const char *, const char *);

#if !defined(__cplusplus)
char *strstr(const char *, const char *);
#endif

#define _STRING_H
#endif
