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
//  stream.h
//

// This file contains definitions and structures for using and manipulating
// streams in Visopsys.

#if !defined(_STREAM_H)

#include <sys/file.h>

// An enum for describing the size of each item in the stream
typedef enum { itemsize_byte, itemsize_dword } streamItemSize;

// This data structure is the generic stream
typedef struct
{
  unsigned char *buffer;
  unsigned buffSize;
  unsigned size;
  unsigned first;
  unsigned last;
  unsigned count;

  // Stream functions
  int (*clear) (void *);
  int (*intercept) (void *, ...);
  int (*append) (void *, ...);
  int (*appendN) (void *, unsigned, ...);
  int (*push) (void *, ...);
  int (*pushN) (void *, unsigned, ...);
  int (*pop) (void *, ...);
  int (*popN) (void *, unsigned, ...);

} stream;

// Some specialized kinds of streams

// A file stream, for character-based file IO
typedef struct 
{
  file f;
  unsigned int block;
  int dirty;
  stream s;

} fileStream;

#define _STREAM_H
#endif
