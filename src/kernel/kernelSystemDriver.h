//
//  Visopsys
//  Copyright (C) 1998-2006 J. Andrew McLaughlin
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
//  kernelSystemDriver.h
//
	
#if !defined(_KERNELSYSTEMDRIVER_H)

#define BIOSAREA_START  0x000E0000
#define BIOSAREA_END    0x000FFFF0
#define BIOSAREA_SIZE   ((BIOSAREA_END - BIOSAREA_START) + 1)

typedef struct {
  char signature[4];
  void *entryPoint;
  unsigned char revision;
  unsigned char structLen;
  unsigned char checksum;
  unsigned char reserved[5];
  
} __attribute__((packed)) kernelBios;

#define _KERNELSYSTEMDRIVER_H
#endif
