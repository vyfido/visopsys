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
//  stdio.h

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
int rename(const char *, const char *);
void rewind(FILE *);
int scanf(const char *, ...);
int sprintf(char *, const char *, ...);

// Internal routines, but are exported
int _expandFormatString(char *, const char *, va_list);
int _formatInput(const char *, const char *, va_list);

/*
  Unimplemented routines

  void clearerr(FILE *stream);
  int fclose(FILE *stream);
  FILE *fdopen(int fildes, const char *mode);
  int feof(FILE *stream);
  int ferror(FILE *stream);
  int fflush(FILE *stream);
  int fgetc(FILE *stream);
  char *fgets(char *s, int size, FILE *stream);
  int fileno(FILE *stream);
  FILE *fopen(const char *path, const char *mode);
  int fprintf(FILE *stream, const char *format, ...);
  int fputc(int c, FILE *stream);
  int fputs(const char *s, FILE *stream);
  size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
  FILE *freopen(const char *path, const char *mode, FILE *stream);
  int fscanf(FILE *stream, const char *format, ...);
  size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
  void setbuf(FILE *stream, char *buf);
  void setbuffer(FILE *stream, char *buf, size_tsize);
  void setlinebuf(FILE *stream);
  int snprintf(char *str, size_t size, const char *format, ...);
  int setvbuf(FILE *stream, char *buf, int mode, size_t size);
  int sscanf(const char *str, const char *format, ...);
  FILE *tmpfile(void);
  char *tmpnam(char *s);
  int ungetc(int c, FILE *stream);
*/

#define _STDIO_H
#endif
