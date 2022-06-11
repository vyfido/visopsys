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
//  kernelParameters.h
//

#if !defined(_KERNELPARAMETERS_H)

// Definitions

// The start of the virtual memory space where the kernel will operate
#define KERNEL_VIRTUAL_ADDRESS    0xC0000000

// Reserved memory blocks at boot time
#define VIDEO_MEMORY              0x000A0000
#define VIDEO_MEMORY_SIZE         0x00020000
#define KERNEL_LOAD_ADDRESS       0x00100000
#define KERNEL_PAGING_DATA_SIZE   0x00004000

// The privilege levels
#define PRIVILEGE_SUPERVISOR 0
#define PRIVILEGE_USER 3

// The kernel's process Id
#define KERNELPROCID 1

// The kernel's symbol file
#define KERNEL_SYMBOLS_FILE "/system/kernelSymbols.txt"

// Disks
#define MAXFLOPPIES 4
#define MAXPARTITIONS 4
#define MAXHARDDISKS 4

// Other
#define MAX_SERIAL_PORTS 4

#define _KERNELPARAMETERS_H
#endif
