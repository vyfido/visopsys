//
//  Visopsys
//  Copyright (C) 1998-2007 J. Andrew McLaughlin
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
//  kernelDisk.h
//
	
// These are the generic functions for disk access.  These are above the
// level of the filesystem, and will generally be called by the filesystem
// drivers.

#if !defined(_KERNELDISK_H)

#include "kernelFile.h"
#include "kernelDevice.h"
#include "kernelLock.h"
#include <sys/disk.h>

#define DISK_CACHE              1
#define DISK_CACHE_ALIGN        (64 * 1024)  // Convenient for floppies
#define DISK_READAHEAD_SECTORS  32

typedef enum { addr_pchs, addr_lba } kernelAddrMethod;

// Forward declarations, where necessary
struct _kernelPhysicalDisk;
struct _kernelFilesystemDriver;

// This defines a logical disk, a disk 'volume' (for example, a hard
// disk partition is a logical disk)
typedef volatile struct _kernelDisk {
  char name[DISK_MAX_NAMELENGTH];
  partitionType partType;
  char fsType[FSTYPE_MAX_NAMELENGTH];
  unsigned opFlags;
  volatile struct _kernelPhysicalDisk *physical;
  unsigned startSector;
  unsigned numSectors;
  int primary;

  // This part of the structure defines file systems
  struct {
    struct _kernelFilesystemDriver *driver;

    // These should always be set by the driver upon successful detection
    unsigned blockSize;
    unsigned minSectors;  // for
    unsigned maxSectors;  // resize

    // These are set when mounted.  Should be cleared during unmount.
    int mounted;
    char mountPoint[MAX_PATH_LENGTH];
    kernelFileEntry *filesystemRoot;
    int childMounts;
    void *filesystemData;
    int caseInsensitive;
    int readOnly;

  } filesystem;

} kernelDisk;

typedef struct {
  int (*driverReset) (int);
  int (*driverRecalibrate) (int);
  int (*driverSetMotorState) (int, int);
  int (*driverSetLockState) (int, int);
  int (*driverSetDoorState) (int, int);
  int (*driverDiskChanged) (int);
  int (*driverReadSectors) (int, unsigned, unsigned, void *);
  int (*driverWriteSectors) (int, unsigned, unsigned, const void *);
  int (*driverFlush) (int);

} kernelDiskOps;

#if (DISK_CACHE)
// This is for metadata about a range of data in a disk cache
typedef volatile struct _kernelDiskCacheSector {
  unsigned startSector;
  unsigned numSectors;
  void *data;
  int dirty;
  unsigned lastAccess;
  volatile struct _kernelDiskCacheSector *prev;
  volatile struct _kernelDiskCacheSector *next;

} kernelDiskCacheBuffer;

// This is for managing the data cache of a physical disk
typedef volatile struct {
  kernelDiskCacheBuffer *buffer;
  unsigned size;
  unsigned dirty;

} kernelDiskCache;
#endif // DISK_CACHE

// This structure describes a physical disk device, as opposed to a
// logical disk.
typedef volatile struct _kernelPhysicalDisk {
  // Generic disk metadata
  char name[DISK_MAX_NAMELENGTH];
  int deviceNumber;
  char *description;
  unsigned type;
  unsigned flags;

  // Generic geometry parameters
  unsigned heads;
  unsigned cylinders;
  unsigned sectorsPerCylinder;
  unsigned numSectors;
  unsigned sectorSize;

  // The logical disks residing on this physical disk
  kernelDisk logical[DISK_MAX_PARTITIONS];
  int numLogical;

  // Misc
  unsigned lastSession;  // Needed for multisession CD-ROM
  lock lock;
  unsigned lastAccess;
  int multiSectors;

  // Physical disk driver
  kernelDriver *driver;
  void *driverData;

  diskStats stats;

#if (DISK_CACHE)
  // The cache
  kernelDiskCache cache;
#endif // DISK_CACHE

} kernelPhysicalDisk;

// Functions exported by kernelDisk.c
int kernelDiskRegisterDevice(kernelDevice *);
int kernelDiskRemoveDevice(kernelDevice *);
int kernelDiskInitialize(void);
int kernelDiskInvalidateCache(const char *);
int kernelDiskShutdown(void);
int kernelDiskFromLogical(kernelDisk *, disk *);
kernelDisk *kernelDiskGetByName(const char *);
kernelDisk *kernelDiskGetByPath(const char *);
// More functions, but also exported to user space
int kernelDiskReadPartitions(const char *);
int kernelDiskReadPartitionsAll(void);
int kernelDiskSync(const char *);
int kernelDiskSyncAll(void);
int kernelDiskGetBoot(char *);
int kernelDiskGetCount(void);
int kernelDiskGetPhysicalCount(void);
int kernelDiskGet(const char *, disk *);
int kernelDiskGetAll(disk *, unsigned);
int kernelDiskGetAllPhysical(disk *, unsigned);
int kernelDiskGetFilesystemType(const char *, char *, unsigned);
int kernelDiskGetPartType(int, partitionType *);
partitionType *kernelDiskGetPartTypes(void);
int kernelDiskSetFlags(const char *, unsigned, int);
int kernelDiskSetLockState(const char *, int state);
int kernelDiskSetDoorState(const char *, int);
int kernelDiskGetMediaState(const char *);
int kernelDiskReadSectors(const char *, unsigned, unsigned, void *);
int kernelDiskWriteSectors(const char *, unsigned, unsigned, const void *);
int kernelDiskEraseSectors(const char *, unsigned, unsigned, int);
int kernelDiskGetStats(const char *, diskStats *);

#define _KERNELDISK_H
#endif
