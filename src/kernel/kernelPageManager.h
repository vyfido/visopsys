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
//  kernelPageManager.h
//

#if !defined(_KERNELPAGEMANAGER_H)

// Data structures

typedef volatile struct 
{
  unsigned int table[1024];

} kernelPageDirPhysicalMem;

typedef kernelPageDirPhysicalMem kernelPageDirVirtualMem;

typedef volatile struct 
{
  unsigned int page[1024];

} kernelPageTablePhysicalMem;

typedef kernelPageTablePhysicalMem kernelPageTableVirtualMem;

typedef volatile struct
{
  int tableNumber;
  kernelPageTablePhysicalMem *physical;
  kernelPageTableVirtualMem *virtual;
  void *next;
  void *previous;

} kernelPageTable;

typedef volatile struct
{
  int processId;
  int numberShares;
  int parent;
  int privilege;
  kernelPageDirPhysicalMem *physical;
  kernelPageDirVirtualMem *virtual;
  int lastPageTableNumber;
  kernelPageTable *firstTable;

} kernelPageDirectory;


// Functions exported by kernelPageManager.c
int kernelPageManagerInitialize(unsigned int);
void *kernelPageGetDirectory(int);
void *kernelPageNewDirectory(int, int);
void *kernelPageShareDirectory(int, int);
int kernelPageDeleteDirectory(int);
int kernelPageMapToFree(int, void *, void **, unsigned int);
int kernelPageUnmap(int, void **, void *, unsigned int);
void *kernelPageGetPhysical(int, void *);

// Macros
#define tableNumber(address) ((unsigned int) address >> 22);
#define pageNumber(address) (((unsigned int) address >> 12) & 0x000003FF);

#define _KERNELPAGEMANAGER_H
#endif
