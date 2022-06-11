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
//  kernelPageManager.h
//

#if !defined(_KERNELPAGEMANAGER_H)

#include "kernelLock.h"

// Definitions
#define TABLES_PER_DIR  1024
#define PAGES_PER_TABLE 1024

// Data structures

typedef volatile struct 
{
  unsigned table[TABLES_PER_DIR];

} kernelPageDirPhysicalMem;

typedef kernelPageDirPhysicalMem kernelPageDirVirtualMem;

typedef volatile struct 
{
  unsigned page[PAGES_PER_TABLE];

} kernelPageTablePhysicalMem;

typedef kernelPageTablePhysicalMem kernelPageTableVirtualMem;

typedef volatile struct
{
  int processId;
  int numberShares;
  int parent;
  int privilege;
  kernelPageDirPhysicalMem *physical;
  kernelPageDirVirtualMem *virtual;
  kernelLock dirLock;

} kernelPageDirectory;

typedef volatile struct
{
  kernelPageDirectory *directory;
  int tableNumber;
  int freePages;
  kernelPageTablePhysicalMem *physical;
  kernelPageTableVirtualMem *virtual;

} kernelPageTable;

// Functions exported by kernelPageManager.c
int kernelPageManagerInitialize(unsigned);
void *kernelPageGetDirectory(int);
void *kernelPageNewDirectory(int, int);
void *kernelPageShareDirectory(int, int);
int kernelPageDeleteDirectory(int);
int kernelPageMapToFree(int, void *, void **, unsigned);
int kernelPageUnmap(int, void *, unsigned);
void *kernelPageGetPhysical(int, void *);

// Macros
#define getTableNumber(address) ((((unsigned) address) >> 22) & 0x000003FF)
#define getPageNumber(address) ((((unsigned) address) >> 12) & 0x000003FF)

#define _KERNELPAGEMANAGER_H
#endif
