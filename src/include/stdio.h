//
//  Visopsys
//  Copyright (C) 1998-2003 J. Andrew McLaughlin
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
//  stdio.h
//

// This is the Visopsys version of the standard header file stdio.h


#if !defined(_STDIO_H)

#include <stddef.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/stream.h>

#ifndef EOF
#define EOF -1
#endif

// Make FILE be the same as a Visopsys 'fileStream'
#define FILE fileStream

#define stdout (FILE *) 0
#define stdin (FILE *) 1
#define stderr (FILE *) 2

// For seeking using fseek()
#define SEEK_SET 1
#define SEEK_CUR 2
#define SEEK_END 3

// fpos_t
typedef unsigned int fpos_t;

// Functions
int fgetpos(FILE *, fpos_t *);
int fseek(FILE *, long, int);
int fsetpos(FILE *, fpos_t *);
long ftell(FILE *);
int getc(FILE *);
int getchar(void);
char *gets(char *);
void perror(const char *);
int printf(const char *, ...);
int putc(int, FILE *);
int putchar(int);
int puts(const char *);
int remove(const char *);
void rewind(FILE *);
int scanf(const char *, ...);
int sprintf(char *, const char *, ...);

// Internal routines, but are exported
int _expandFormatString(char *, const char *, va_list);
int _formatInput(const char *, const char *, va_list);


// For dealing with unimplemented routines
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

// Unimplemented routines
#define clearerr(stream) not_implemented_void()
#define fclose(stream) not_implemented_int()
#define feof(stream) not_implemented_int()
#define ferror(stream) not_implemented_int()
#define fflush(stream) not_implemented_int()
#define fgetc(stream) not_implemented_int()
#define fgets(charptr, int, stream) not_implemented_ptr()
#define fopen(charptr1, charptr2) not_implemented_ptr()
#define fprintf(stream, charptr, ...) not_implemented_int()
#define fputc(int, stream) not_implemented_int()
#define fputs(charptr, stream) not_implemented_int()
#define fread(ptr, size, nelem, stream) not_implemented_uns()
#define freopen(filename, mode, stream) not_implemented_ptr()
#define fscanf(stream, format, ...) not_implemented_int()
#define fwrite(ptr, size, nelem, stream) not_implemented_uns()
#define setbuf(stream, buf) not_implemented_void()
#define setvbuf(stream, buf, mode, size) not_implemented_int()
#define sscanf(s, format, ...) not_implemented_int()
#define tmpfile() not_implemented_ptr()
#define tmpnam(s) not_implemented_ptr()
#define ungetc(c, stream) not_implemented_int()
#define vfprintf(stream, format, ap) not_implemented_int()
#define vprintf(format, ap) not_implemented_int()
#define vsprintf(s, format, ap) not_implemented_int()

#define _STDIO_H
#endif
