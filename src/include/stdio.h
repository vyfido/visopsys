// 
//  Visopsys
//  Copyright (C) 1998-2005 J. Andrew McLaughlin
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
//  stdio.h

// This is the Visopsys version of the standard header file stdio.h

#if !defined(_STDIO_H)

#include <stdarg.h>
#include <sys/stream.h>

// Make FILE be the same as a Visopsys 'fileStream'
#define FILE fileStream

#ifndef EOF
#define EOF -1
#endif

#define stdout (FILE *) 0
#define stdin  (FILE *) 1
#define stderr (FILE *) 2

// For seeking using fseek()
#define SEEK_SET 0x01
#define SEEK_CUR 0x02
#define SEEK_END 0x03

// fpos_t
typedef unsigned fpos_t;

// Available functions
int fgetpos(FILE *, fpos_t *);
int fprintf(FILE *, const char *, ...);
size_t fread(void *, size_t, size_t, FILE *);
int fseek(FILE *, long, int);
int fsetpos(FILE *, fpos_t *);
long ftell(FILE *);
size_t fwrite(const void *, size_t, size_t, FILE *);
int getc(FILE *);
int getchar(void);
char *gets(char *);
void perror(const char *);
int printf(const char *, ...);
int putc(int, FILE *);
int putchar(int);
int puts(const char *);
int remove(const char *);
int rename(const char *, const char *);
void rewind(FILE *);
int scanf(const char *, ...);
int sprintf(char *, const char *, ...);

// Internal routines, but are exported
int _expandFormatString(char *, const char *, va_list);
int _formatInput(const char *, const char *, va_list);

#define _STDIO_H
#endif
