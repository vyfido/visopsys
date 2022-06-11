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
//  disk.h
//

// This file contains definitions and structures for using and manipulating
// disk in Visopsys.

#if !defined(_DISK_H)

#include <sys/file.h>

#define DISK_MAXDEVICES                 32
#define DISK_MAX_NAMELENGTH             16
#define DISK_MAX_PARTITIONS             16
#define DISK_MAX_PRIMARY_PARTITIONS     4
#define FSTYPE_MAX_NAMELENGTH           32

// Extended partition types
#define PARTITION_TYPEID_EXTENDED       0x05
#define PARTITION_TYPEID_EXTENDED_LBA   0x0F
#define PARTITION_TYPEID_EXTENDED_LINUX 0x85
#define PARTITION_TYPEID_IS_EXTENDED(x)    \
  ((x == PARTITION_TYPEID_EXTENDED)     || \
   (x == PARTITION_TYPEID_EXTENDED_LBA) || \
   (x == PARTITION_TYPEID_EXTENDED_LINUX))
#define PARTITION_TYPEID_IS_HIDDEN(x)                          \
  ((x == 0x11) || (x == 0x14) || (x == 0x16) || (x == 0x17) || \
   (x == 0x1B) || (x == 0x1C) || (x == 0x1E) || (x == 0x93))
#define PARTITION_TYPEID_IS_HIDEABLE(x)                        \
  ((x == 0x01) || (x == 0x04) || (x == 0x06) || (x == 0x07) || \
   (x == 0x0B) || (x == 0x0C) || (x == 0x0E) || (x == 0x83))

// Flags for supported filesystem operations on a partition
#define FS_OP_FORMAT                    0x01
#define FS_OP_CHECK                     0x02
#define FS_OP_DEFRAG                    0x04

typedef enum { floppy, idecdrom, scsicdrom, idedisk, scsidisk } diskType;
typedef enum { fixed, removable } mediaType;

// This structure is used to describe a known partition type
typedef struct
{
  unsigned char code;
  const char description[FSTYPE_MAX_NAMELENGTH];

} partitionType;   

typedef struct
{
  char name[DISK_MAX_NAMELENGTH];
  int deviceNumber;
  diskType type;
  mediaType fixedRemovable;
  int readOnly;
  partitionType partType;
  char fsType[FSTYPE_MAX_NAMELENGTH];
  unsigned opFlags;

  unsigned heads;
  unsigned cylinders;
  unsigned sectorsPerCylinder;

  unsigned startSector;
  unsigned numSectors;
  unsigned sectorSize;

} disk;

#define _DISK_H
#endif
