//
//  Visopsys
//  Copyright (C) 1998-2005 J. Andrew McLaughlin
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

#include <sys/memory.h>

// Definitions
#define MAXMEMORYBLOCKS 2048
#define MEMBLOCKSIZE MEMORY_PAGE_SIZE

// Functions from kernelMemoryManager.c
int kernelMemoryInitialize(unsigned);
void *kernelMemoryGet(unsigned, const char *);
void *kernelMemoryGetSystem(unsigned, const char *);
void *kernelMemoryGetPhysical(unsigned, unsigned, const char *);
int kernelMemoryRelease(void *);
int kernelMemoryReleaseSystem(void *);
int kernelMemoryReleasePhysical(void *);
int kernelMemoryReleaseAllByProcId(int);
int kernelMemoryChangeOwner(int, int, int, void *, void **);
int kernelMemoryShare(int, int, void *, void **);
int kernelMemoryGetStats(memoryStats *, int);
int kernelMemoryGetBlocks(memoryBlock *, unsigned, int);

#define _KERNELMEMORYMANAGER_H
#endif
