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
//  kernelMemoryManager.h
//
	
#if !defined(_KERNELMEMORYMANAGER_H)

#include "loaderInfo.h"

// Definitions

#define MAXMEMORYBLOCKS 2048
#define MEMBLOCKSIZE MEMORY_PAGE_SIZE
#define MAX_DESC_LENGTH 20

// Messages
#define FAIL_INTEGRITY_CHECK "The memory manger's internal integrity check failed."
#define USEDBLOCKS_TOOBIG "The number of used blocks exceeds the legal limit"
#define TOTAL_INCONSISTENT "The amounts of free and used memory are inconsistent with the available total"
#define INVALID_USED_MEMORY_BLOCK "One member of the memory manager's used block list was invalid"
#define NULL_MEMORY_POINTER "The memory pointer passed or referenced is NULL"
#define OUT_OF_MEMORY_BLOCKS "The number of available memory allocation blocks has been exhausted"
#define OUT_OF_MEMORY "The computer is out of physical memory"
#define UNMAPPING_ERROR "The memory manager was unable to unmap memory from the virtual address space"
#define UNKNOWN_PROCESS "The memory manager was unable to determine the current process"


typedef volatile struct
{

  int processId;
  char description[MAX_DESC_LENGTH];
  unsigned int startLocation;
  unsigned int endLocation;

} kernelMemoryBlock;


// Functions from kernelMemoryManager.c

int kernelMemoryInitialize(unsigned int, loaderInfoStruct *);
void kernelMemoryPrintUsage(void);
int kernelMemoryReserveBlock(unsigned int, unsigned int, const char *);
void *kernelMemoryRequestBlock(unsigned int, unsigned int, const char *);
void *kernelMemoryRequestSystemBlock(unsigned int, unsigned int,
				     const char *);
void *kernelMemoryRequestPhysicalBlock(unsigned int, unsigned int,
				       const char *);
int kernelMemoryReleaseByPointer(void *);
int kernelMemoryReleaseByPhysicalPointer(void *);
int kernelMemoryReleaseAllByProcId(int);
int kernelMemoryChangeOwner(int, int, int, void *, void **);
int kernelMemoryShare(int, int, void *, void **);

#define _KERNELMEMORYMANAGER_H
#endif
