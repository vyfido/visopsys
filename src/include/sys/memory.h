// 
//  Visopsys
//  Copyright (C) 1998-2005 J. Andrew McLaughlin
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

// Definitions
#define MEMORY_PAGE_SIZE        4096
#define MEMORY_MAX_DESC_LENGTH  32

// Struct that describes one memory block
typedef struct
{
  int processId;
  char description[MEMORY_MAX_DESC_LENGTH];
  unsigned startLocation;
  unsigned endLocation;

} memoryBlock;

// Struct that describes overall memory statistics
typedef struct
{
  unsigned totalBlocks;
  unsigned usedBlocks;
  unsigned totalMemory;
  unsigned usedMemory;

} memoryStats;

#define _MEMORY_H
#endif
