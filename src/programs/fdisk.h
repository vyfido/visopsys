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
//  fdisk.h
//

// This is the header for the "Disk Manager" program, fdisk.c

#if !defined(_FDISK_H)

#include <sys/disk.h>

#define DISKMANAGER "Visopsys Disk Manager"
#define PARTLOGIC   "Partition Logic"
#define TEMP_DIR    "/temp"
#define BACKUP_DIR  "/system/boot"
#define BACKUP_MBR  BACKUP_DIR"/backup-%s.mbr"
#define PERM        "You must be a privileged user to use this command."
#define PARTTYPES   "Supported partition types"

#define ENTRYOFFSET_DRV_ACTIVE    0
#define ENTRYOFFSET_START_HEAD    1
#define ENTRYOFFSET_START_CYLSECT 2
#define ENTRYOFFSET_START_CYL     3
#define ENTRYOFFSET_TYPE          4
#define ENTRYOFFSET_END_HEAD      5
#define ENTRYOFFSET_END_CYLSECT   6
#define ENTRYOFFSET_END_CYL       7
#define ENTRYOFFSET_START_LBA     8
#define ENTRYOFFSET_SIZE_LBA      12
#define COPYBUFFER_SIZE           1048576 // 1 Meg
#define MAX_SLICES                ((DISK_MAX_PARTITIONS * 2) + 1)
#define MAX_DESCSTRING_LENGTH     128

#define ISLOGICAL(slc) (((slc)->entryType == partition_extended) || \
                        ((slc)->entryType == partition_logical))

typedef enum {
  partition_primary  = 0,
  partition_logical  = 1,
  partition_extended = 2,
  partition_any      = 3
} partEntryType;

// This stucture represents both used partitions and empty spaces.
typedef struct {
  // These fields come directly from the partition table
  int active;
  int drive;
  unsigned startCylinder;
  unsigned startHead;
  unsigned startSector;
  int typeId;
  unsigned endCylinder;
  unsigned endHead;
  unsigned endSector;
  unsigned startLogical;
  unsigned sizeLogical;
  // Below here, the fields are generated internally
  int partition;
  char name[6];
  partEntryType entryType;
  void *extendedTable;
  char string[MAX_DESCSTRING_LENGTH];
  int pixelX;
  int pixelWidth;

} slice;

// This structure represents a partition table
typedef struct {
  unsigned startSector;
  unsigned char sectorData[512];
  slice entries[DISK_MAX_PRIMARY_PARTITIONS];
  int numberEntries;
  int maxEntries;
  // For extended partition tables
  int extended;
  int parentPartition;

} partitionTable;

#define _FDISK_H
#endif
