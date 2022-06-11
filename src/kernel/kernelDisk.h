//
//  Visopsys
//  Copyright (C) 1998-2005 J. Andrew McLaughlin
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

#include "kernelLock.h"
#include <sys/disk.h>

#define DISK_CACHE              1
#define DISK_CACHE_ALIGN        (64 * 1024) // Convenient for floppies
#define DISK_MAX_CACHE          1048576 // 1 Meg
#define DISK_READAHEAD_SECTORS  32

typedef enum { addr_pchs, addr_lba } kernelAddrMethod;

typedef struct
{
  // This is a structure containing pointers to the
  // generic disk driver routines

  int (*driverDetect) (int, void *);    // Pointer is a kernelPhysicalDisk *
  int (*driverRegisterDevice) (void *); // Pointer is a kernelPhysicalDisk *
  int (*driverReset) (int);
  int (*driverRecalibrate) (int);
  int (*driverSetMotorState) (int, int);
  int (*driverSetLockState) (int, int);
  int (*driverSetDoorState) (int, int);
  int (*driverDiskChanged) (int);
  int (*driverReadSectors) (int, unsigned, unsigned, void *);
  int (*driverWriteSectors) (int, unsigned, unsigned, void *);

} kernelDiskDriver;

#if (DISK_CACHE)
typedef volatile struct
{
  // This is for metadata about one sector of data from a disk cache
  
  unsigned number;
  void *data;
  int dirty;
  unsigned lastAccess;

} kernelDiskCacheSector;

typedef volatile struct
{
  // This is for managing the data cache of a logical disk

  int initialized;
  unsigned numSectors;
  unsigned usedSectors;
  kernelDiskCacheSector **sectors;
  kernelDiskCacheSector *sectorMemory;
  void *dataMemory;
  int dirty;
  lock cacheLock;

} kernelDiskCache;
#endif // DISK_CACHE

typedef volatile struct
{
  // This defines a logical disk, a disk 'volume' (for example, a hard
  // disk partition is a logical disk)

  char name[DISK_MAX_NAMELENGTH];
  partitionType partType;
  char fsType[FSTYPE_MAX_NAMELENGTH];
  unsigned opFlags;
  void *physical;
  unsigned startSector;
  unsigned numSectors;
  int primary;

} kernelDisk;

typedef volatile struct 
{
  // This structure describes a physical disk device, as opposed to a
  // logical disk.
  
  // Generic disk metadata
  char name[DISK_MAX_NAMELENGTH];
  int deviceNumber;
  int dmaChannel;
  char *description;
  int flags;
  int readOnly;

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
  unsigned biosType;     // Needed for floppy detection
  unsigned lastSession;  // Needed for multisession CD-ROM
  void *driverData;
  lock diskLock;
  int motorState;
  int lockState;
  int doorState;
  unsigned idleSince;

  kernelDiskDriver *driver;

#if (DISK_CACHE)
  kernelDiskCache cache;
#endif // DISK_CACHE

} kernelPhysicalDisk;

// The default driver initializations
int kernelFloppyDriverInitialize(void);
int kernelIdeDriverInitialize(void);

// Functions exported by kernelDisk.c
int kernelDiskRegisterDevice(kernelPhysicalDisk *);
int kernelDiskReadPartitions(void);
int kernelDiskInitialize(const char *, unsigned);
int kernelDiskSync(void);
int kernelDiskSyncDisk(const char *);
int kernelDiskInvalidateCache(const char *);
int kernelDiskShutdown(void);
int kernelDiskGetBoot(char *);
int kernelDiskGetCount(void);
int kernelDiskGetPhysicalCount(void);
int kernelDiskGet(const char *, disk *);
int kernelDiskGetAll(disk *, unsigned);
int kernelDiskGetAllPhysical(disk *, unsigned);
int kernelDiskFromLogical(kernelDisk *, disk *);
int kernelDiskGetInfo(disk *);
int kernelDiskFromPhysical(kernelPhysicalDisk *, disk *);
int kernelDiskGetPhysicalInfo(disk *);
int kernelDiskGetPartType(int, partitionType *);
partitionType *kernelDiskGetPartTypes(void);
kernelDisk *kernelGetDiskByName(const char *);
kernelPhysicalDisk *kernelGetPhysicalDiskByName(const char *);
int kernelDiskSetLockState(const char *diskName, int state);
int kernelDiskSetDoorState(const char *, int);
int kernelDiskReadSectors(const char *, unsigned, unsigned, void *);
int kernelDiskWriteSectors(const char *, unsigned, unsigned, void *);

#define _KERNELDISK_H
#endif
