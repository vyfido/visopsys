//
//  Visopsys
//  Copyright (C) 1998-2021 J. Andrew McLaughlin
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
//  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
//
//  kernelPage.h
//

#ifndef _KERNELPAGE_H
#define _KERNELPAGE_H

#include "kernelLock.h"
#include <sys/processor.h>

// Definitions

// Page mapping schemes
#define PAGE_MAP_ANY			0x01
#define PAGE_MAP_EXACT			0x02

// Page caching types
typedef enum {
	pageattr_readonly,
	pageattr_privileged,
	pageattr_writecombine,
	pageattr_uncacheable

} kernelPageAttribute;

// Data structures

typedef volatile struct {
	unsigned table[PROCESSOR_PAGE_TABLES_PER_DIR];

} kernelPageDirPhysicalMem;

typedef kernelPageDirPhysicalMem kernelPageDirVirtualMem;

typedef volatile struct {
	unsigned page[PROCESSOR_PAGES_PER_TABLE];

} kernelPageTablePhysicalMem;

typedef kernelPageTablePhysicalMem kernelPageTableVirtualMem;

typedef volatile struct {
	int processId;
	int numberShares;
	int privilege;
	kernelPageDirPhysicalMem *physical;
	kernelPageDirVirtualMem *virtual;
	spinLock lock;

} kernelPageDirectory;

extern kernelPageDirectory *kernelPageDir;

typedef volatile struct {
	kernelPageDirectory *directory;
	int tableNumber;
	int freePages;
	kernelPageTablePhysicalMem *physical;
	kernelPageTableVirtualMem *virtual;

} kernelPageTable;

// Little macros for rounding up or down to the nearest page size
#define kernelPageRoundDown(val) \
	((unsigned)(val) & ~(MEMORY_PAGE_SIZE - 1))
#define kernelPageRoundUp(val) \
	kernelPageRoundDown((unsigned)(val) + (MEMORY_PAGE_SIZE - 1))

// Functions exported by kernelPage.c
int kernelPageInitialize(unsigned);
kernelPageDirectory *kernelPageGetDirectory(int);
kernelPageDirectory *kernelPageNewDirectory(int);
kernelPageDirectory *kernelPageShareDirectory(int, int);
int kernelPageDeleteDirectory(int);
int kernelPageMap(int, unsigned, void *, unsigned);
int kernelPageMapToFree(int, unsigned, void **, unsigned);
int kernelPageUnmap(int, void *, unsigned);
int kernelPageMapped(int, void *, unsigned);
unsigned kernelPageGetPhysical(int, void *);
void *kernelPageFindFree(int, unsigned);
int kernelPageSetAttrs(int, kernelPageAttribute, void *, unsigned);

#ifdef PAGE_DEBUG
void kernelPageTableDebug(int);
#endif

#endif

