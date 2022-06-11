// 
//  Visopsys
//  Copyright (C) 1998-2007 J. Andrew McLaughlin
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
//  linux-swap.h
//

// This file contains definitions and structures for using and manipulating
// the Linux swap filesystem in Visopsys.

#if !defined(_LINUXSWAP_H)

#define LINUXSWAP_MAXPAGES  (~0UL << 8)

typedef union  {
    struct {
      char reserved[MEMORY_PAGE_SIZE - 10];
      char magic[10];       // SWAP-SPACE or SWAPSPACE2
      
    } magic;
    
    struct {
      char bootbits[1024];  // Space for disk label etc.
      int version;
      unsigned lastPage;
      unsigned numBadPages;
      unsigned padding[125];
      unsigned badPages[1];

    } info;

} __attribute__((packed)) linuxSwapHeader;

#define _LINUXSWAP_H
#endif
