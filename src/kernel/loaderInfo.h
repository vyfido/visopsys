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
//  loaderInfo.h
//

// This header file contains information about the information structure
// passed to the kernel by the OS loader at startup.

#if !defined(_LOADERINFO_H)

#include <sys/image.h>

// The data structure created by the loader (actually, by the BIOS) to
// describe a memory range

// The types of memory ranges described by the memoryInfoBlock structure,
// below
typedef enum
{
  available = 1, 
  reserved  = 2,
  reclaim   = 3,
  nvs = 4
} memoryRangeType;

typedef struct
{
  long long start;
  long long size;
  memoryRangeType type;

} memoryInfoBlock;

// The data structure created by the loader to describe the particulars
// about the current graphics environment to the kernel
typedef struct
{
  unsigned videoMemory;
  void *framebuffer;
  int mode;
  int xRes;
  int yRes;
  int bitsPerPixel;
  int numberModes;
  videoMode supportedModes[MAXVIDEOMODES];

} graphicsInfoBlock;

// The data structure created by the loader to describe the particulars
// about a floppy disk drive to the kernel
typedef struct
{
  int type;
  int heads;
  int tracks;
  int sectors;

} fddInfoBlock;

// The data structure created by the loader to describe the particulars
// about a hard disk drive to the kernel
typedef struct
{
  unsigned heads;
  unsigned cylinders;
  unsigned sectorsPerCylinder;
  unsigned bytesPerSector;
  unsigned totalSectors;

} hddInfoBlock;

// The data structure created by the loader to hold info about the serial
// ports
typedef struct
{
  unsigned port1;
  unsigned port2;
  unsigned port3;
  unsigned port4;

} serialInfoBlock;

// The data structure created by the loader to hold info about the mouse
typedef struct
{
  unsigned port;
  unsigned idByte;

} mouseInfoBlock;

// The data structure created by the loader to describe the system's hardware
// to the kernel
typedef struct
{
  int cpuType;
  char cpuVendor[12];
  int mmxExtensions;
  unsigned extendedMemory;
  memoryInfoBlock memoryMap[50];
  graphicsInfoBlock graphicsInfo;
  int bootDevice;
  unsigned bootSector;
  char bootDisk[4];
  int floppyDisks;
  fddInfoBlock fddInfo[2];
  int hardDisks;
  hddInfoBlock hddInfo[4];
  serialInfoBlock serialPorts;
  mouseInfoBlock mouseInfo;

} loaderInfoStruct;

#define _LOADERINFO_H
#endif
