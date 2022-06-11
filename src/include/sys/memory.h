// 
//  Visopsys
//  Copyright (C) 1998-2006 J. Andrew McLaughlin
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
//  memory.h
//

// This file contains definitions and structures for using and manipulating
// memory in Visopsys.

#if !defined(_MEMORY_H)

#include <sys/lock.h>
#include <sys/errors.h>

// Definitions
#define MEMORY_PAGE_SIZE             4096
#define MEMORY_BLOCK_SIZE            MEMORY_PAGE_SIZE
#define MEMORY_MAX_DESC_LENGTH       32

#define USER_MEMORY_HEAP_MULTIPLE    (64 * 1024)    // 64 Kb
#define KERNEL_MEMORY_HEAP_MULTIPLE  (1024 * 1024)  // 1 meg

typedef struct {
  int used;
  int process;
  void *start;
  void *end;
  void *previous;
  void *next;
  const char *function;

} mallocBlock;

// Struct that describes one memory block
typedef struct {
  int processId;
  char description[MEMORY_MAX_DESC_LENGTH];
  unsigned startLocation;
  unsigned endLocation;

} memoryBlock;

// Struct that describes overall memory statistics
typedef struct {
  unsigned totalBlocks;
  unsigned usedBlocks;
  unsigned totalMemory;
  unsigned usedMemory;

} memoryStats;

typedef struct {
  int (*multitaskerGetCurrentProcessId) (void);
  void * (*memoryGetSystem) (unsigned, const char *);
  int (*lockGet) (lock *);
  int (*lockRelease) (lock *);
  void (*error) (const char *, const char *, int, kernelErrorKind, 
		 const char *, ...);

} mallocKernelOps;

// For using malloc() in kernel space
extern unsigned mallocHeapMultiple;
extern mallocKernelOps mallocKernOps;

// Extras for malloc debugging
void *_doMalloc(unsigned, const char *);
void _doFree(void *, const char *);
int _mallocBlockInfo(void *, memoryBlock *);
int _mallocGetStats(memoryStats *);
int _mallocGetBlocks(memoryBlock *, int);

#define _MEMORY_H
#endif
