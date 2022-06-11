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
//  disk.h
//

// This file contains definitions and structures for using and manipulating
// disk in Visopsys.

#if !defined(_DISK_H)

#include <sys/file.h>

#define DISK_MAXDEVICES               32
#define DISK_MAX_NAMELENGTH           16
#define DISK_MAX_PARTITIONS           16
#define DISK_MAX_PRIMARY_PARTITIONS   4
#define DISK_MAX_CACHE                1048576 // 1 Meg
#define FSTYPE_MAX_NAMELENGTH         32

// Extended partition types
#define PARTITION_TYPEID_EXTD         0x05
#define PARTITION_TYPEID_EXTD_LBA     0x0F
#define PARTITION_TYPEID_EXTD_LINUX   0x85
#define PARTITION_TYPEID_IS_EXTD(x)    \
  ((x == PARTITION_TYPEID_EXTD)     || \
   (x == PARTITION_TYPEID_EXTD_LBA) || \
   (x == PARTITION_TYPEID_EXTD_LINUX))
#define PARTITION_TYPEID_IS_HIDDEN(x)                          \
  ((x == 0x11) || (x == 0x14) || (x == 0x16) || (x == 0x17) || \
   (x == 0x1B) || (x == 0x1C) || (x == 0x1E) || (x == 0x93))
#define PARTITION_TYPEID_IS_HIDEABLE(x)                        \
  ((x == 0x01) || (x == 0x04) || (x == 0x06) || (x == 0x07) || \
   (x == 0x0B) || (x == 0x0C) || (x == 0x0E) || (x == 0x83))

// Flags for supported filesystem operations on a partition
#define FS_OP_FORMAT                  0x01
#define FS_OP_CLOBBER                 0x02
#define FS_OP_CHECK                   0x04
#define FS_OP_DEFRAG                  0x08
#define FS_OP_STAT                    0x10
#define FS_OP_RESIZECONST             0x20
#define FS_OP_RESIZE                  0x40

// Flags to describe what type of disk is described by a disk structure
#define DISKTYPE_LOGICAL              0x20000000
#define DISKTYPE_PHYSICAL             0x10000000
#define DISKTYPE_PRIMARY              0x01000000
#define DISKTYPE_LOGICALPHYSICAL      (DISKTYPE_PHYSICAL | DISKTYPE_LOGICAL)
#define DISKTYPE_FIXED                0x00200000
#define DISKTYPE_REMOVABLE            0x00100000
#define DISKTYPE_FIXEDREMOVABLE       (DISKTYPE_FIXED | DISKTYPE_REMOVABLE)
#define DISKTYPE_FLOPPY               0x00000100
#define DISKTYPE_SCSICDROM            0x00000020
#define DISKTYPE_IDECDROM             0x00000010
#define DISKTYPE_CDROM                (DISKTYPE_SCSICDROM | DISKTYPE_IDECDROM)
#define DISKTYPE_FLASHDISK            0x00000004
#define DISKTYPE_SCSIDISK             0x00000002
#define DISKTYPE_IDEDISK              0x00000001
#define DISKTYPE_HARDDISK             (DISKTYPE_FLASHDISK | \
                                       DISKTYPE_SCSIDISK | DISKTYPE_IDEDISK)

// Flags to describe the current state of the disk
#define DISKFLAG_NOCACHE              0x10
#define DISKFLAG_READONLY             0x08
#define DISKFLAG_MOTORON              0x04
#define DISKFLAG_DOORLOCKED           0x02
#define DISKFLAG_DOOROPEN             0x01
#define DISKFLAG_USERSETTABLE         (DISKFLAG_NOCACHE | DISKFLAG_READONLY)

// This structure is used to describe a known partition type
typedef struct {
  unsigned char code;
  const char description[FSTYPE_MAX_NAMELENGTH];

} partitionType;   

typedef struct {
  char name[DISK_MAX_NAMELENGTH];
  int deviceNumber;
  unsigned type;
  unsigned flags;
  partitionType partType;
  char fsType[FSTYPE_MAX_NAMELENGTH];
  unsigned opFlags;

  unsigned heads;
  unsigned cylinders;
  unsigned sectorsPerCylinder;
  unsigned sectorSize;

  unsigned startSector;
  unsigned numSectors;

  // Filesystem related
  unsigned blockSize;
  unsigned freeBytes;
  unsigned minSectors;  // for
  unsigned maxSectors;  // resize
  int mounted;
  char mountPoint[MAX_PATH_LENGTH];
  int readOnly;

} disk;

typedef struct {
  // Throughput measurement.
  unsigned readTime;
  unsigned readKbytes;
  unsigned writeTime;
  unsigned writeKbytes;

} diskStats;

#define CYLSECTS(d) (d->heads * d->sectorsPerCylinder)

#define _DISK_H
#endif
