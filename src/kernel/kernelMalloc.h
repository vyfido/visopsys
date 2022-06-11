//
//  Visopsys
//  Copyright (C) 1998-2004 J. Andrew McLaughlin
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
//  kernelMalloc.h
//
	
#if !defined(_KERNELMALLOC_H)

#define MEMORY_HEAP_MULTIPLE (1024 * 1024)  // 1 meg

typedef volatile struct
{
  int used;
  int process;
  void *start;
  void *end;
  void *previous;
  void *next;
  char *function;

} kernelMallocBlock;

// Functions from kernelMalloc.c
#define kernelMalloc(size) _kernelMalloc(__FUNCTION__, size);
void *_kernelMalloc(char *, unsigned);
#define kernelFree(ptr) _kernelFree(__FUNCTION__, ptr);
int _kernelFree(char *, void *);
void kernelMallocDump(void);

#define _KERNELMALLOC_H
#endif
