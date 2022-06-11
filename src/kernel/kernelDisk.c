//
//  Visopsys
//  Copyright (C) 1998-2011 J. Andrew McLaughlin
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
//  kernelDisk.c
//
	
// This file functions for disk access, and routines for managing the array
// of disks in the kernel's data structure for such things.  

#include "kernelDisk.h"
#include "kernelDebug.h"
#include "kernelError.h"
#include "kernelFilesystem.h"
#include "kernelLock.h"
#include "kernelLog.h"
#include "kernelMain.h"
#include "kernelMalloc.h"
#include "kernelMemory.h"
#include "kernelMisc.h"
#include "kernelMultitasker.h"
#include "kernelParameters.h"
#include "kernelRandom.h"
#include "kernelSysTimer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// All the disks
static kernelPhysicalDisk *physicalDisks[DISK_MAXDEVICES];
static volatile int physicalDiskCounter = 0;
static kernelDisk *logicalDisks[DISK_MAXDEVICES];
static volatile int logicalDiskCounter = 0;
static int numFloppyDisks = 0, numCdRoms = 0, numScsiDisks = 0,
  numHardDisks = 0;

// The name of the disk we booted from
static char bootDisk[DISK_MAX_NAMELENGTH];

// Modes for the readWriteSectors routine
#define IOMODE_READ     0x01
#define IOMODE_WRITE    0x02
#define IOMODE_NOCACHE  0x04

// For the disk daemon
static int diskdPID = 0;

// This is a table for keeping known MS-DOS partition type codes and
// descriptions
static msdosPartType msdosPartTypes[] = {
  { 0x01, "FAT12"},
  { 0x02, "XENIX root"},
  { 0x03, "XENIX /usr"},
  { 0x04, "FAT16 (small)"},
  { 0x05, "Extended"},
  { 0x06, "FAT16"},
  { 0x07, "NTFS or HPFS"},
  { 0x08, "OS/2 or AIX boot"},
  { 0x09, "AIX data"},
  { 0x0A, "OS/2 Boot Manager"},
  { 0x0B, "FAT32"},
  { 0x0C, "FAT32 (LBA)"},
  { 0x0E, "FAT16 (LBA)"},
  { 0x0F, "Extended (LBA)"},
  { 0x11, "Hidden FAT12"},
  { 0x12, "FAT diagnostic"},
  { 0x14, "Hidden FAT16 (small)"},
  { 0x16, "Hidden FAT16"},
  { 0x17, "Hidden HPFS or NTFS"},
  { 0x1B, "Hidden FAT32"},
  { 0x1C, "Hidden FAT32 (LBA)"},
  { 0x1E, "Hidden FAT16 (LBA)"},
  { 0x35, "JFS" },
  { 0x39, "Plan 9" },
  { 0x3C, "PartitionMagic" },
  { 0x3D, "Hidden Netware" },
  { 0x41, "PowerPC PReP" },
  { 0x42, "Win2K dynamic extended" },
  { 0x44, "GoBack" },
  { 0x4D, "QNX4.x" },
  { 0x4D, "QNX4.x 2nd" },
  { 0x4D, "QNX4.x 3rd" },
  { 0x50, "Ontrack R/O" },
  { 0x51, "Ontrack R/W or Novell" },
  { 0x52, "CP/M" },
  { 0x63, "GNU HURD or UNIX SysV"},
  { 0x64, "Netware 2"},
  { 0x65, "Netware 3/4"},
  { 0x66, "Netware SMS"},
  { 0x67, "Novell"},
  { 0x68, "Novell"},
  { 0x69, "Netware 5+"},
  { 0x7E, "Veritas VxVM public"},
  { 0x7F, "Veritas VxVM private"},
  { 0x80, "Minix"},
  { 0x81, "Linux or Minix"},
  { 0x82, "Linux swap or Solaris"},
  { 0x83, "Linux"},
  { 0x84, "Hibernation"},
  { 0x85, "Linux extended"},
  { 0x86, "HPFS or NTFS mirrored"},
  { 0x87, "HPFS or NTFS mirrored"},
  { 0x8E, "Linux LVM"},
  { 0x93, "Hidden Linux"},
  { 0x9F, "BSD/OS"},
  { 0xA0, "Laptop hibernation"},
  { 0xA1, "Laptop hibernation"},
  { 0xA5, "BSD, NetBSD, FreeBSD"},
  { 0xA6, "OpenBSD"},
  { 0xA7, "NeXTSTEP"},
  { 0xA8, "OS-X UFS"},
  { 0xA9, "NetBSD"},
  { 0xAB, "OS-X boot"},
  { 0xAF, "OS-X HFS"},
  { 0xB6, "NT corrupt mirror"},
  { 0xB7, "BSDI"},
  { 0xB8, "BSDI swap"},
  { 0xBE, "Solaris 8 boot"},
  { 0xBF, "Solaris x86"},
  { 0xC0, "NTFT"},
  { 0xC1, "DR-DOS FAT12"},
  { 0xC2, "Hidden Linux"},
  { 0xC3, "Hidden Linux swap"},
  { 0xC4, "DR-DOS FAT16 (small)"},
  { 0xC5, "DR-DOS Extended"},
  { 0xC6, "DR-DOS FAT16"},
  { 0xC7, "HPFS mirrored"},
  { 0xCB, "DR-DOS FAT32"},
  { 0xCC, "DR-DOS FAT32 (LBA)"},
  { 0xCE, "DR-DOS FAT16 (LBA)"},
  { 0xD0, "MDOS"},
  { 0xD1, "MDOS FAT12"},
  { 0xD4, "MDOS FAT16 (small)"},
  { 0xD5, "MDOS Extended"},
  { 0xD6, "MDOS FAT16"},
  { 0xD8, "CP/M-86"},
  { 0xEB, "BeOS BFS"},
  { 0xEE, "EFI GPT protective"},
  { 0xEF, "EFI filesystem"},
  { 0xF0, "Linux/PA-RISC boot"},
  { 0xF2, "DOS 3.3+ second"},
  { 0xFA, "Bochs"},
  { 0xFB, "VmWare"},
  { 0xFC, "VmWare swap"},
  { 0xFD, "Linux RAID"},
  { 0xFE, "NT hidden"},
  { 0, "" }
};

// This is a table for keeping known GPT partition type GUIDs and descriptions
static gptPartType gptPartTypes[] = {
  { { 0x024DEE41, 0x33E7, 0x11D3, 0x9D, 0x69,
      { 0x00, 0x08, 0xC7, 0x81, 0xF3, 0x9F } }, "MBR partition scheme" },
  { { 0xC12A7328, 0xF81F, 0x11D2, 0xBA, 0x4B,
      { 0x00, 0xA0, 0xC9, 0x3E, 0xC9, 0x3B } }, "EFI System partition" },
  { { 0xE3C9E316, 0x0B5C, 0x4DB8, 0x81, 0x7D,
      { 0xF9, 0x2D, 0xF0, 0x02, 0x15, 0xAE } }, "Microsoft Reserved" },
  { { 0xEBD0A0A2, 0xB9E5, 0x4433, 0x87, 0xC0,
      { 0x68, 0xB6, 0xB7, 0x26, 0x99, 0xC7 } }, "Windows or Linux data" },
  { { 0x5808C8AA, 0x7E8F, 0x42E0, 0x85, 0xD2,
      { 0xE1, 0xE9, 0x04, 0x34, 0xCF, 0xB3 } }, "Windows LDM metadata" },
  { { 0xAF9B60A0, 0x1431, 0x4F62, 0xBC, 0x68,
      { 0x33, 0x11, 0x71, 0x4A, 0x69, 0xAD } }, "Windows LDM data" },
  { { 0x75894C1E, 0x3AEB, 0x11D3, 0xB7, 0xC1,
      { 0x7B, 0x03, 0xA0, 0x00, 0x00, 0x00 } }, "HP, UX data" },
  { { 0xE2A1E728, 0x32E3, 0x11D6, 0xA6, 0x82,
      { 0x7B, 0x03, 0xA0, 0x00, 0x00, 0x00 } }, "HP, UX service" },
  { { 0xA19D880F, 0x05FC, 0x4D3B, 0xA0, 0x06,
      { 0x74, 0x3F, 0x0F, 0x84, 0x91, 0x1E } }, "Linux RAID" },
  { { 0x0657FD6D, 0xA4AB, 0x43C4, 0x84, 0xE5,
      { 0x09, 0x33, 0xC8, 0x4B, 0x4F, 0x4F } }, "Linux swap" },
  { { 0xE6D6D379, 0xF507, 0x44C2, 0xA2, 0x3C,
      { 0x23, 0x8F, 0x2A, 0x3D, 0xF9, 0x28 } }, "Linux LVM" },
  { { 0x8DA63339, 0x0007, 0x60C0, 0xC4, 0x36,
      { 0x08, 0x3A, 0xC8, 0x23, 0x09, 0x08 } }, "Linux reserved" },
  { { 0x516E7CB4, 0x6ECF, 0x11D6, 0x8F, 0xF8,
      { 0x00, 0x02, 0x2D, 0x09, 0x71, 0x2B } }, "FreeBSD data" },
  { { 0x516E7CB5, 0x6ECF, 0x11D6, 0x8F, 0xF8,
      { 0x00, 0x02, 0x2D, 0x09, 0x71, 0x2B } }, "FreeBSD swap" },
  { { 0x516E7CB6, 0x6ECF, 0x11D6, 0x8F, 0xF8,
      { 0x00, 0x02, 0x2D, 0x09, 0x71, 0x2B } }, "FreeBSD Unix UFS" },
  { { 0x516E7CB8, 0x6ECF, 0x11D6, 0x8F, 0xF8,
      { 0x00, 0x02, 0x2D, 0x09, 0x71, 0x2B } }, "FreeBSD Vinum" },
  { { 0x48465300, 0x0000, 0x11AA, 0xAA, 0x11,
      { 0x00, 0x30, 0x65, 0x43, 0xEC, 0xAC } }, "MacOS X HFS+" },
  { { 0x55465300, 0x0000, 0x11AA, 0xAA, 0x11,
      { 0x00, 0x30, 0x65, 0x43, 0xEC, 0xAC } }, "Apple UFS" },
  { { 0x52414944, 0x0000, 0x11AA, 0xAA, 0x11,
      { 0x00, 0x30, 0x65, 0x43, 0xEC, 0xAC } }, "Apple RAID" },
  { { 0x52414944, 0x5F4F, 0x11AA, 0xAA, 0x11,
      { 0x00, 0x30, 0x65, 0x43, 0xEC, 0xAC } }, "Apple RAID offline" },
  { { 0x426F6F74, 0x0000, 0x11AA, 0xAA, 0x11,
      { 0x00, 0x30, 0x65, 0x43, 0xEC, 0xAC } }, "Apple Boot" },
  { { 0x4C616265, 0x6C00, 0x11AA, 0xAA, 0x11,
      { 0x00, 0x30, 0x65, 0x43, 0xEC, 0xAC } }, "Apple label" },
  { { 0x5265636F, 0x7665, 0x11AA, 0xAA, 0x11,
      { 0x00, 0x30, 0x65, 0x43, 0xEC, 0xAC } }, "Apple TV recovery" },
  { { 0x6A82CB45, 0x1DD2, 0x11B2, 0x99, 0xA6,
      { 0x08, 0x00, 0x20, 0x73, 0x66, 0x31 } }, "Solaris boot" },
  { { 0x6A85CF4D, 0x1DD2, 0x11B2, 0x99, 0xA6,
      { 0x08, 0x00, 0x20, 0x73, 0x66, 0x31 } }, "Solaris root" },
  { { 0x6A87C46F, 0x1DD2, 0x11B2, 0x99, 0xA6,
      { 0x08, 0x00, 0x20, 0x73, 0x66, 0x31 } }, "Solaris swap" },
  { { 0x6A8B642B, 0x1DD2, 0x11B2, 0x99, 0xA6,
      { 0x08, 0x00, 0x20, 0x73, 0x66, 0x31 } }, "Solaris backup" },
  { { 0x6A898CC3, 0x1DD2, 0x11B2, 0x99, 0xA6,
      { 0x08, 0x00, 0x20, 0x73, 0x66, 0x31 } }, "Solaris /usr" },
  { { 0x6A8EF2E9, 0x1DD2, 0x11B2, 0x99, 0xA6,
      { 0x08, 0x00, 0x20, 0x73, 0x66, 0x31 } }, "Solaris /var" },
  { { 0x6A90BA39, 0x1DD2, 0x11B2, 0x99, 0xA6,
      { 0x08, 0x00, 0x20, 0x73, 0x66, 0x31 } }, "Solaris /home" },
  { { 0x6A9283A5, 0x1DD2, 0x11B2, 0x99, 0xA6,
      { 0x08, 0x00, 0x20, 0x73, 0x66, 0x31 } }, "Solaris EFI_ALTSCTR" },
  { { 0x6A945A3B, 0x1DD2, 0x11B2, 0x99, 0xA6,
      { 0x08, 0x00, 0x20, 0x73, 0x66, 0x31 } }, "Solaris reserved" },
  { { 0x6A9630D1, 0x1DD2, 0x11B2, 0x99, 0xA6,
      { 0x08, 0x00, 0x20, 0x73, 0x66, 0x31 } }, "Solaris reserved" },
  { { 0x6A980767, 0x1DD2, 0x11B2, 0x99, 0xA6,
      { 0x08, 0x00, 0x20, 0x73, 0x66, 0x31 } }, "Solaris reserved" },
  { { 0x6A96237F, 0x1DD2, 0x11B2, 0x99, 0xA6,
      { 0x08, 0x00, 0x20, 0x73, 0x66, 0x31 } }, "Solaris reserved" },
  { { 0x6A8D2AC7, 0x1DD2, 0x11B2, 0x99, 0xA6,
      { 0x08, 0x00, 0x20, 0x73, 0x66, 0x31 } }, "Solaris reserved" },
  { GUID_BLANK, "" }
};

static int initialized = 0;


#if defined(DEBUG)
static void debugLockCheck(kernelPhysicalDisk *physicalDisk,
			   const char *function)
{
  if (physicalDisk->lock.processId != kernelMultitaskerGetCurrentProcessId())
    {
      kernelError(kernel_error, "%s is not locked by process %d in function "
		  "%s", physicalDisk->name,
		  kernelMultitaskerGetCurrentProcessId(), function);
      while(1);
    }
}
#else
#define debugLockCheck(physicalDisk, function) do {} while (0)
#endif // DEBUG


static int motorOff(kernelPhysicalDisk *physicalDisk)
{
  // Calls the target disk driver's 'motor off' routine.

  int status = 0;
  kernelDiskOps *ops = (kernelDiskOps *) physicalDisk->driver->ops;

  debugLockCheck(physicalDisk, __FUNCTION__);

  // Reset the 'last access' value.
  physicalDisk->lastAccess = kernelSysTimerRead();
  
  // If it's a fixed disk, we don't turn the motor off, for now
  if (physicalDisk->type & DISKTYPE_FIXED)
    return (status = 0);

  // Make sure the motor isn't already off
  if (!(physicalDisk->flags & DISKFLAG_MOTORON))
    return (status = 0);

  // Make sure the device driver routine is available.
  if (ops->driverSetMotorState == NULL)
    // Don't make this an error.  It's just not available in some drivers.
    return (status = 0);

  // Ok, now turn the motor off
  status = ops->driverSetMotorState(physicalDisk->deviceNumber, 0);
  if (status < 0)
    return (status);

  // Make note of the fact that the motor is off
  physicalDisk->flags &= ~DISKFLAG_MOTORON;

  // Reset the 'last access' value
  physicalDisk->lastAccess = kernelSysTimerRead();
  
  return (status);
}


__attribute__((noreturn))
static void diskd(void)
{
  // This function will be a thread spawned at inititialization time
  // to do any required ongoing operations on disks, such as shutting off
  // floppy and cdrom motors
  
  kernelPhysicalDisk *physicalDisk = NULL;
  int count;

  // Don't try to do anything until we have registered disks
  while (!initialized || (physicalDiskCounter <= 0))
    kernelMultitaskerWait(60);

  while(1)
    {
      // Loop for each physical disk
      for (count = 0; count < physicalDiskCounter; count ++)
	{
	  physicalDisk = physicalDisks[count];

	  // If the disk is a floppy and has been idle for >= 2 seconds,
	  // turn off the motor.
	  if ((physicalDisk->type & DISKTYPE_FLOPPY) &&
	      (kernelSysTimerRead() > (physicalDisk->lastAccess + 40)))
	    {
	      // Lock the disk
	      if (kernelLockGet(&physicalDisk->lock) < 0)
		continue;

	      motorOff(physicalDisk);

	      // Unlock the disk
	      kernelLockRelease(&physicalDisk->lock);
	    }
	}

      // Yield the rest of the timeslice and wait for 1 second
      kernelMultitaskerWait(20);
    }
}


static int spawnDiskd(void)
{
  // Launches the disk daemon

  diskdPID = kernelMultitaskerSpawnKernelThread(diskd, "disk thread", 0, NULL);
  if (diskdPID < 0)
    return (diskdPID);

  // Re-nice the disk daemon
  kernelMultitaskerSetProcessPriority(diskdPID, (PRIORITY_LEVELS - 2));
 
  // Success
  return (diskdPID);
}


static int realReadWrite(kernelPhysicalDisk *physicalDisk,
			 uquad_t startSector, uquad_t numSectors,
			 void *data, unsigned mode)
{
  // This routine does all real, physical disk reads or writes.

  int status = 0;
  kernelDiskOps *ops = (kernelDiskOps *) physicalDisk->driver->ops;
  processState tmpState;

  debugLockCheck(physicalDisk, __FUNCTION__);

  // Make sure the disk daemon is running
  if (kernelMultitaskerGetProcessState(diskdPID, &tmpState) < 0)
    // Re-spawn the disk daemon
    spawnDiskd();

  // Make sure the device driver routine is available.
  if (((mode & IOMODE_READ) && (ops->driverReadSectors == NULL)) ||
      ((mode & IOMODE_WRITE) && (ops->driverWriteSectors == NULL)))
    {
      kernelError(kernel_error, "Disk %s cannot %s", physicalDisk->name,
		  ((mode & IOMODE_READ)? "read" : "write"));
      return (status = ERR_NOSUCHFUNCTION);
    }

  // Do the actual read/write operation

  kernelDebug(debug_io, "%s %s %llu sectors at %llu", physicalDisk->name,
	      ((mode & IOMODE_READ)? "read" : "write"), numSectors,
	      startSector);

  if (mode & IOMODE_READ)
    status = ops->driverReadSectors(physicalDisk->deviceNumber, startSector,
				    numSectors, data);
  else
    status = ops->driverWriteSectors(physicalDisk->deviceNumber, startSector,
				     numSectors, data);

  kernelDebug(debug_io, "%s done %sing %llu sectors at %llu",
	      physicalDisk->name, ((mode & IOMODE_READ)? "read" : "writ"),
	      numSectors, startSector);

  if (status < 0)
    {
      // If it is a write-protect error, mark the disk as read only
      if ((mode & IOMODE_WRITE) && (status == ERR_NOWRITE))
	{
	  kernelError(kernel_error, "Disk %s is write-protected",
		      physicalDisk->name);
	  physicalDisk->flags |= DISKFLAG_READONLY;
	}
      else
	kernelError(kernel_error, "Error %d %sing %llu sectors at %llu, "
		    "disk %s", status, ((mode & IOMODE_READ)? "read" : "writ"),
		    numSectors, startSector, physicalDisk->name);
    }

  return (status);
}


#if (DISK_CACHE)

#define bufferEnd(buffer) (buffer->startSector + buffer->numSectors - 1)

static inline void cacheMarkDirty(kernelPhysicalDisk *physicalDisk,
				  kernelDiskCacheBuffer *buffer)
{
  if (!buffer->dirty)
    {
      buffer->dirty = 1;
      physicalDisk->cache.dirty += 1;
    }
}


static inline void cacheMarkClean(kernelPhysicalDisk *physicalDisk,
				  kernelDiskCacheBuffer *buffer)
{
  if (buffer->dirty)
    {
      buffer->dirty = 0;
      physicalDisk->cache.dirty -= 1;
    }
}


static int cacheSync(kernelPhysicalDisk *physicalDisk)
{
  // Write all dirty cached buffers to the disk

  int status = 0;
  kernelDiskCacheBuffer *buffer = physicalDisk->cache.buffer;
  int errors = 0;

  debugLockCheck(physicalDisk, __FUNCTION__);

  if (!physicalDisk->cache.dirty || (physicalDisk->flags & DISKFLAG_READONLY))
    return (status = 0);

  while (buffer)
    {
      if (buffer->dirty)
	{
	  status = realReadWrite(physicalDisk, buffer->startSector,
				 buffer->numSectors, buffer->data,
				 IOMODE_WRITE);
	  if (status < 0)
	    errors = status;
	  else
	    cacheMarkClean(physicalDisk, buffer);
	}

      buffer = buffer->next;
    }

  return (status = errors);
}


static kernelDiskCacheBuffer *cacheGetBuffer(kernelPhysicalDisk *physicalDisk,
					     uquad_t startSector,
					     uquad_t numSectors)
{
  // Get a new cache buffer for the specified number of sectors.

  kernelDiskCacheBuffer *buffer = NULL;

  debugLockCheck(physicalDisk, __FUNCTION__);

  // Get memory for the structure
  buffer = kernelMalloc(sizeof(kernelDiskCacheBuffer));
  if (buffer == NULL)
    return (buffer);

  buffer->startSector = startSector;
  buffer->numSectors = numSectors;

  // Get memory for the data
  buffer->data = kernelMalloc(numSectors * physicalDisk->sectorSize);
  if (buffer->data == NULL)
    {
      kernelFree((void *) buffer);
      return (buffer = NULL);
    }

  return (buffer);
}


static inline void cachePutBuffer(kernelDiskCacheBuffer *buffer)
{
  // Deallocate a cache buffer.

  if (buffer->data)
    kernelFree(buffer->data);

  kernelMemClear((void *) buffer, sizeof(kernelDiskCacheBuffer));
  kernelFree((void *) buffer);

  return;
}


static int cacheInvalidate(kernelPhysicalDisk *physicalDisk)
{
  // Invalidate the disk cache, syncing dirty sectors first.

  int status = 0;
  kernelDiskCacheBuffer *buffer = physicalDisk->cache.buffer;
  kernelDiskCacheBuffer *next = NULL;

  debugLockCheck(physicalDisk, __FUNCTION__);

  // Try to sync dirty sectors first.
  cacheSync(physicalDisk);
  
  if (physicalDisk->cache.dirty)
    kernelError(kernel_warn, "Invalidating dirty disk cache!");

  while (buffer)
    {
      next = buffer->next;
      cachePutBuffer(buffer);
      buffer = next;
    }

  physicalDisk->cache.buffer = NULL;
  physicalDisk->cache.size = 0;
  physicalDisk->cache.dirty = 0;

  return (status);
}


static kernelDiskCacheBuffer *cacheFind(kernelPhysicalDisk *physicalDisk,
					uquad_t startSector,
					uquad_t numSectors)
{
  // Finds the first buffer that intersects the supplied range of sectors.
  // If not found, return NULL.

  uquad_t endSector = (startSector + numSectors - 1);
  kernelDiskCacheBuffer *buffer = physicalDisk->cache.buffer;

  debugLockCheck(physicalDisk, __FUNCTION__);

  while (buffer)
    {
      // Start sector inside buffer?
      if ((startSector >= buffer->startSector) &&
	  (startSector <= bufferEnd(buffer)))
	return (buffer);

      // End sector inside buffer?
      if ((endSector >= buffer->startSector) &&
	  (endSector <= bufferEnd(buffer)))
	return (buffer);

      // Range overlaps buffer?
      if ((startSector < buffer->startSector) &&
	  (endSector > bufferEnd(buffer)))
	return (buffer);

      buffer = buffer->next;
    }

  // Not found
  return (buffer = NULL);
}


static unsigned cacheQueryRange(kernelPhysicalDisk *physicalDisk,
				uquad_t startSector, uquad_t numSectors,
				uquad_t *firstCached)
{
  // Search the cache for a range of sectors.  If any of the range is cached,
  // return the *first* portion that is cached.

  kernelDiskCacheBuffer *buffer = NULL;
  uquad_t numCached = 0;

  debugLockCheck(physicalDisk, __FUNCTION__);

  buffer = cacheFind(physicalDisk, startSector, numSectors);
  if (buffer)
    {
      *firstCached = max(startSector, buffer->startSector);
      numCached = min((numSectors - (*firstCached - startSector)),
		      (buffer->numSectors -
		       (*firstCached - buffer->startSector)));
      kernelDebug(debug_io, "%s found %llu->%llu in %llu->%llu, first=%llu "
		  "num=%llu", physicalDisk->name, startSector,
		  (startSector + numSectors - 1), buffer->startSector,
		  bufferEnd(buffer), *firstCached, numCached);
    }
  else
    kernelDebug(debug_io, "%s %llu->%llu not found", physicalDisk->name, 
		startSector, (startSector + numSectors - 1));

  return (numCached);
}


#if defined(DEBUG)
static void cachePrint(kernelPhysicalDisk *physicalDisk)
{
  kernelDiskCacheBuffer *buffer = physicalDisk->cache.buffer;

  while (buffer)
    {
      kernelTextPrintLine("%s cache: %llu->%llu (%llu sectors) %s",
			  physicalDisk->name, buffer->startSector,
			  bufferEnd(buffer), buffer->numSectors,
			  (buffer->dirty? "(dirty)" : ""));
      buffer = buffer->next;
    }
}


static void cacheCheck(kernelPhysicalDisk *physicalDisk)
{
  kernelDiskCacheBuffer *buffer = physicalDisk->cache.buffer;
  uquad_t cacheSize = 0;
  uquad_t numDirty = 0;
  
  while (buffer)
    {
      if (buffer->next)
	{
	  if (buffer->startSector >= buffer->next->startSector)
	    {
	      kernelError(kernel_warn, "%s startSector (%llu) >= "
			  "next->startSector (%llu)", physicalDisk->name,
			  buffer->startSector, buffer->next->startSector);
	      cachePrint(physicalDisk); while(1);
	    }

	  if (bufferEnd(buffer) >= buffer->next->startSector)
	    {
	      kernelError(kernel_warn, "%s (startSector(%llu) + "
			  "numSectors(%llu) = %llu) > next->startSector(%llu)",
			  physicalDisk->name, buffer->startSector,
			  buffer->numSectors,
			  (buffer->startSector + buffer->numSectors),
			  buffer->next->startSector);
	      cachePrint(physicalDisk); while(1);
	    }

	  /*
	  if ((bufferEnd(buffer) == (buffer->next->startSector - 1)) &&
	      (buffer->dirty == buffer->next->dirty))
	    {
	      kernelError(kernel_warn, "%s buffer %llu->%llu should be joined "
			  "with %llu->%llu (%s)", physicalDisk->name,
			  buffer->startSector, bufferEnd(buffer),
			  buffer->next->startSector, bufferEnd(buffer->next),
			  (buffer->dirty? "dirty" : "clean"));
	      cachePrint(physicalDisk); while(1);
	    }
	  */

	  if (buffer->next->prev != buffer)
	    {
	      kernelError(kernel_warn, "%s buffer->next->prev != buffer",
			  physicalDisk->name);
	      cachePrint(physicalDisk); while(1);
	    }
	}

      if (buffer->prev)
	{
	  if (buffer->prev->next != buffer)
	    {
	      kernelError(kernel_warn, "%s buffer->prev->next != buffer",
			  physicalDisk->name);
	      cachePrint(physicalDisk); while(1);
	    }
	}

      cacheSize += (buffer->numSectors * physicalDisk->sectorSize);
      if (buffer->dirty)
	numDirty += 1;

      buffer = buffer->next;
    }
  
  if (cacheSize != physicalDisk->cache.size)
    {
      kernelError(kernel_warn, "%s cacheSize(%llu) != physicalDisk->cache.size"
		  "(%llu)", physicalDisk->name, cacheSize,
		  physicalDisk->cache.size);
      cachePrint(physicalDisk); while(1);
    }

  if (numDirty != physicalDisk->cache.dirty)
    {
      kernelError(kernel_warn, "%s numDirty(%llu) != physicalDisk->cache.dirty"
		  "(%llu)", physicalDisk->name, numDirty,
		  physicalDisk->cache.dirty);
      cachePrint(physicalDisk); while(1);
    }
}
#else
#define cacheCheck(physicalDisk) do {} while (0)
#endif // DEBUG


static void cacheRemove(kernelPhysicalDisk *physicalDisk,
			kernelDiskCacheBuffer *buffer)
{
  debugLockCheck(physicalDisk, __FUNCTION__);

  if (buffer == physicalDisk->cache.buffer)
    physicalDisk->cache.buffer = buffer->next;

  if (buffer->prev)
    buffer->prev->next = buffer->next;
  if (buffer->next)
    buffer->next->prev = buffer->prev;

  physicalDisk->cache.size -= (buffer->numSectors * physicalDisk->sectorSize);
  cachePutBuffer(buffer);
  cacheCheck(physicalDisk);
}


static void cachePrune(kernelPhysicalDisk *physicalDisk)
{
  // If the cache has grown larger than the pre-ordained DISK_CACHE_MAX
  // value, uncache some data.  Uncache the least-recently-used buffers
  // until we're under the limit.

  kernelDiskCacheBuffer *currBuffer = NULL;
  unsigned oldestTime = 0;
  kernelDiskCacheBuffer *oldestBuffer = NULL;

  debugLockCheck(physicalDisk, __FUNCTION__);

  while (physicalDisk->cache.size > DISK_MAX_CACHE)
    {
      currBuffer = physicalDisk->cache.buffer;

      // Don't bother uncaching the only buffer
      if (!currBuffer->next)
	break;

      oldestTime = ~0UL;
      oldestBuffer = NULL;

      while (currBuffer)
	{
	  if (currBuffer->lastAccess < oldestTime)
	    {
	      oldestTime = currBuffer->lastAccess;
	      oldestBuffer = currBuffer;
	    }

	  currBuffer = currBuffer->next;
	}

      if (!oldestBuffer)
	{
	  kernelDebug(debug_io, "%s, no oldest buffer!", physicalDisk->name);
	  break;
	}

      kernelDebug(debug_io, "%s uncache buffer %llu->%llu, mem=%p, dirty=%d",
		  physicalDisk->name, oldestBuffer->startSector,
		  bufferEnd(oldestBuffer), oldestBuffer->data,
		  oldestBuffer->dirty);

      if (oldestBuffer->dirty)
	{
	  if (realReadWrite(physicalDisk, oldestBuffer->startSector,
			    oldestBuffer->numSectors, oldestBuffer->data,
			    IOMODE_WRITE) < 0)
	    {
	      kernelDebug(debug_io, "%s error writing dirty buffer",
			  physicalDisk->name);
	      return;
	    }

	  cacheMarkClean(physicalDisk, oldestBuffer);
	}

      cacheRemove(physicalDisk, oldestBuffer);
    }

  return;
}


static kernelDiskCacheBuffer *cacheAdd(kernelPhysicalDisk *physicalDisk,
				       uquad_t startSector,
				       uquad_t numSectors, void *data)
{
  // Add the supplied range of sectors to the cache.

  kernelDiskCacheBuffer *prevBuffer = NULL;
  kernelDiskCacheBuffer *nextBuffer = NULL;
  kernelDiskCacheBuffer *newBuffer = NULL;

  //kernelDebug(debug_io, "%s adding %llu->%llu", physicalDisk->name,
  //	      startSector, (startSector + numSectors - 1));

  debugLockCheck(physicalDisk, __FUNCTION__);

  // Find out where in the order the new buffer would go.
  nextBuffer = physicalDisk->cache.buffer;
  while (nextBuffer)
    {
      if (startSector > nextBuffer->startSector)
	{
	  prevBuffer = nextBuffer;
	  nextBuffer = nextBuffer->next;
	}
      else
	break;
    }

  // Get a new cache buffer.
  newBuffer = cacheGetBuffer(physicalDisk, startSector, numSectors);
  if (newBuffer == NULL)
    {
      kernelError(kernel_error, "Couldn't get a new buffer for %s's disk "
		  "cache", physicalDisk->name);
      return (newBuffer);
    }

  // Copy the data into the cache buffer.
  kernelMemCopy(data, newBuffer->data,
		(numSectors * physicalDisk->sectorSize));

  newBuffer->prev = prevBuffer;
  newBuffer->next = nextBuffer;

  if (newBuffer->prev)
    newBuffer->prev->next = newBuffer;
  else
    // This will be the first cache buffer in the cache.
    physicalDisk->cache.buffer = newBuffer;

  if (newBuffer->next)
    newBuffer->next->prev = newBuffer;

  physicalDisk->cache.size += (numSectors * physicalDisk->sectorSize);

  cacheCheck(physicalDisk);

  return (newBuffer);
}


static int cacheRead(kernelPhysicalDisk *physicalDisk, uquad_t startSector,
		     uquad_t numSectors, void *data)
{
  // For ranges of sectors that are in the cache, copy them into the target
  // data buffer.  For ranges that are not in the cache, read the sectors
  // from disk and put a copy in a new cache buffer.

  int status = 0;
  uquad_t firstCached = 0;
  uquad_t numCached = 0;
  uquad_t notCached = 0;
  kernelDiskCacheBuffer *buffer = NULL;

  debugLockCheck(physicalDisk, __FUNCTION__);

  while (numSectors)
    {
      numCached =
	cacheQueryRange(physicalDisk, startSector, numSectors, &firstCached);

      if (numCached)
	{
	  // At least some of the data is cached.  Any uncached portion that
	  // comes before the cached portion needs to be read from disk and
	  // added to the cache.

	  notCached = (firstCached - startSector);

	  // Read the uncached portion from disk.
	  if (notCached)
	    {
	      status = realReadWrite(physicalDisk, startSector, notCached,
				     data, IOMODE_READ);
	      if (status < 0)
		return (status);

	      // Add the data to the cache.
	      buffer = cacheAdd(physicalDisk, startSector, notCached, data);
	      if (buffer)
		buffer->lastAccess = kernelSysTimerRead();

	      startSector += notCached;
	      numSectors -= notCached;
	      data += (notCached * physicalDisk->sectorSize);
	    }

	  // Get the cached portion
	  buffer = cacheFind(physicalDisk, startSector, numCached);
	  if (buffer)
	    {
	      kernelMemCopy((buffer->data +
			     ((startSector - buffer->startSector) *
			      physicalDisk->sectorSize)),
			    data, (numCached * physicalDisk->sectorSize));
	      buffer->lastAccess = kernelSysTimerRead();
	    }

	  startSector += numCached;
	  numSectors -= numCached;
	  data += (numCached * physicalDisk->sectorSize);
	}
      else
	{
	  // Nothing is cached.  Read everything from disk.
	  status = realReadWrite(physicalDisk, startSector, numSectors, data,
				 IOMODE_READ);
	  if (status < 0)
	    return (status);

	  // Add the data to the cache.
	  buffer = cacheAdd(physicalDisk, startSector, numSectors, data);
	  if (buffer)
	    buffer->lastAccess = kernelSysTimerRead();

	  break;
	}
    }

  // Since we might have added something to the cache above, check whether
  // we should prune it.
  if (physicalDisk->cache.size > DISK_MAX_CACHE)
    cachePrune(physicalDisk);

  return (status = 0);
}


static kernelDiskCacheBuffer *cacheSplit(kernelPhysicalDisk *physicalDisk,
					 uquad_t startSector,
					 uquad_t numSectors, void *data,
					 kernelDiskCacheBuffer *buffer)
{
  // Given a range of sectors, split them from the supplied buffer, resulting
  // in a previous buffer (if applicable), a next buffer(if applicable),
  // and the new split-off buffer which we return.

  uquad_t prevSectors = 0;
  uquad_t nextSectors = 0;
  kernelDiskCacheBuffer *prevBuffer = NULL;
  kernelDiskCacheBuffer *newBuffer = NULL;
  kernelDiskCacheBuffer *nextBuffer = NULL;

  prevSectors = (startSector - buffer->startSector);
  nextSectors = ((buffer->startSector + buffer->numSectors) -
		 (startSector + numSectors));

  if (!prevSectors && !nextSectors)
    {
      kernelError(kernel_error, "Cannot split %llu sectors from a %llu-sector"
		  "buffer", numSectors, buffer->numSectors);
      return (newBuffer = NULL);
    }

  if (prevSectors)
    prevBuffer =
      cacheGetBuffer(physicalDisk, buffer->startSector, prevSectors);

  newBuffer = cacheGetBuffer(physicalDisk, startSector, numSectors);

  if (nextSectors)
    nextBuffer =
      cacheGetBuffer(physicalDisk, (startSector + numSectors), nextSectors);

  if ((prevSectors && (prevBuffer == NULL)) || (newBuffer == NULL) ||
      (nextSectors && (nextBuffer == NULL)))
    {
      kernelError(kernel_error, "Couldn't get a new buffer for %s's disk "
		  "cache", physicalDisk->name);
      return (newBuffer = NULL);
    }

  // Copy data
  if (prevBuffer)
    {
      kernelMemCopy(buffer->data, prevBuffer->data,
		    (prevSectors * physicalDisk->sectorSize));
      if (buffer->dirty)
	cacheMarkDirty(physicalDisk, prevBuffer);
      prevBuffer->lastAccess = buffer->lastAccess;

      prevBuffer->prev = buffer->prev;
      prevBuffer->next = newBuffer;

      if (prevBuffer->prev)
	prevBuffer->prev->next = prevBuffer;
      else
	physicalDisk->cache.buffer = prevBuffer;

      if (prevBuffer->next)
	prevBuffer->next->prev = prevBuffer;
    }
  else
    {
      newBuffer->prev = buffer->prev;

      if (newBuffer->prev)
	newBuffer->prev->next = newBuffer;
      else
	physicalDisk->cache.buffer = newBuffer;
    }

  kernelMemCopy(data, newBuffer->data,
		(numSectors * physicalDisk->sectorSize));
  if (buffer->dirty)
    cacheMarkDirty(physicalDisk, newBuffer);
  newBuffer->lastAccess = buffer->lastAccess;

  if (nextBuffer)
    {
      kernelMemCopy((buffer->data + (prevSectors * physicalDisk->sectorSize) +
		     (numSectors * physicalDisk->sectorSize)),
		    nextBuffer->data,
		    (nextSectors * physicalDisk->sectorSize));
      if (buffer->dirty)
	cacheMarkDirty(physicalDisk, nextBuffer);
      nextBuffer->lastAccess = buffer->lastAccess;

      nextBuffer->prev = newBuffer;
      nextBuffer->next = buffer->next;

      if (nextBuffer->prev)
	nextBuffer->prev->next = nextBuffer;
      if (nextBuffer->next)
	nextBuffer->next->prev = nextBuffer;
    }
  else
    {
      newBuffer->next = buffer->next;
      
      if (newBuffer->next)
	newBuffer->next->prev = newBuffer;
    }

  if (buffer->dirty)
    cacheMarkClean(physicalDisk, buffer);

  cachePutBuffer(buffer);

  cacheCheck(physicalDisk);

  return (newBuffer);
}


static int cacheWrite(kernelPhysicalDisk *physicalDisk, uquad_t startSector,
		      uquad_t numSectors, void *data)
{
  // For ranges of sectors that are in the cache, overwrite the cache buffer
  // with the new data.  For ranges that are not in the cache, allocate a
  // new cache buffer for the new data.

  int status = 0;
  uquad_t firstCached = 0;
  uquad_t numCached = 0;
  uquad_t notCached = 0;
  kernelDiskCacheBuffer *buffer = NULL;

  debugLockCheck(physicalDisk, __FUNCTION__);

  while (numSectors)
    {
      numCached =
	cacheQueryRange(physicalDisk, startSector, numSectors, &firstCached);

      if (numCached)
	{
	  // At least some of the data is cached.  For any uncached portion
	  // that comes before the cached portion, allocate a new cache
	  // buffer.

	  notCached = (firstCached - startSector);

	  if (notCached)
	    {
	      // Add the data to the cache, and mark it dirty.
	      buffer = cacheAdd(physicalDisk, startSector, notCached, data);
	      if (buffer)
		{
		  cacheMarkDirty(physicalDisk, buffer);
		  buffer->lastAccess = kernelSysTimerRead();
		}

	      startSector += notCached;
	      numSectors -= notCached;
	      data += (notCached * physicalDisk->sectorSize);
	    }

	  buffer = cacheFind(physicalDisk, startSector, numCached);

	  // If the buffer is clean, and we're not dirtying the whole thing,
	  // split off the bit we're making dirty.
	  if (!buffer->dirty && (numCached != buffer->numSectors))
	    {
	      buffer =
		cacheSplit(physicalDisk, startSector, numCached, data, buffer);
	    }
	  else
	    {
	      // Overwrite the cached portion.
	      kernelMemCopy(data, (buffer->data +
				   ((startSector - buffer->startSector) *
				    physicalDisk->sectorSize)),
			    (numCached * physicalDisk->sectorSize));
	    }
	  if (buffer)
	    {
	      cacheMarkDirty(physicalDisk, buffer);
	      buffer->lastAccess = kernelSysTimerRead();
	    }

	  startSector += numCached;
	  numSectors -= numCached;
	  data += (numCached * physicalDisk->sectorSize);
	}
      else
	{
	  // Nothing is cached.  Add it all to the cache, and mark it dirty.
	  buffer = cacheAdd(physicalDisk, startSector, numSectors, data);
	  if (buffer)
	    {
	      cacheMarkDirty(physicalDisk, buffer);
	      buffer->lastAccess = kernelSysTimerRead();
	    }
	  break;
	}
    }

  // Since we might have added something to the cache above, check whether
  // we should prune it.
  if (physicalDisk->cache.size > DISK_MAX_CACHE)
    cachePrune(physicalDisk);

  return (status = 0);
}
#endif // DISK_CACHE


static int readWrite(kernelPhysicalDisk *physicalDisk, uquad_t startSector,
		     uquad_t numSectors, void *data, int mode)
{
  // This is the combined "read sectors" and "write sectors" routine.  Uses
  // the cache where available/permitted.

  int status = 0;
  unsigned startTime = kernelSysTimerRead();

  debugLockCheck(physicalDisk, __FUNCTION__);

  // Don't try to write a read-only disk
  if ((mode & IOMODE_WRITE) && (physicalDisk->flags & DISKFLAG_READONLY))
    {
      kernelError(kernel_error, "Disk %s is read-only", physicalDisk->name);
      return (status = ERR_NOWRITE);
    }

#if (DISK_CACHE)
  if (!(physicalDisk->flags & DISKFLAG_NOCACHE) && !(mode & IOMODE_NOCACHE))
    {  
      if (mode & IOMODE_READ)
	status = cacheRead(physicalDisk, startSector, numSectors, data);
      else
	status = cacheWrite(physicalDisk, startSector, numSectors, data);
    }
  else
#endif // DISK_CACHE
    {
      status =
	realReadWrite(physicalDisk, startSector, numSectors, data, mode);
    }

  // Throughput stats collection
  if (mode & IOMODE_READ)
    {
      physicalDisk->stats.readTime += (kernelSysTimerRead() - startTime);
      physicalDisk->stats.readKbytes +=
	((numSectors * physicalDisk->sectorSize) / 1024);
    }
  else
    {
      physicalDisk->stats.writeTime += (kernelSysTimerRead() - startTime);
      physicalDisk->stats.writeKbytes +=
	((numSectors * physicalDisk->sectorSize) / 1024);
    }

  return (status);
}


static kernelPhysicalDisk *getPhysicalByName(const char *name)
{
  // This routine takes the name of a physical disk and finds it in the
  // array, returning a pointer to the disk.  If the disk doesn't exist,
  // the function returns NULL

  kernelPhysicalDisk *physicalDisk = NULL;
  int count;

  for (count = 0; count < physicalDiskCounter; count ++)
    if (!strcmp(name, (char *) physicalDisks[count]->name))
      {
	physicalDisk = physicalDisks[count];
	break;
      }

  return (physicalDisk);
}


static int diskFromPhysical(kernelPhysicalDisk *physicalDisk, disk *userDisk)
{
  // Takes our physical disk kernel structure and turns it into a user space
  // 'disk' object

  int status = 0;

  // Check params
  if ((physicalDisk == NULL) || (userDisk == NULL))
    return (status = ERR_NULLPARAMETER);

  kernelMemClear(userDisk, sizeof(disk));
  strncpy(userDisk->name, (char *) physicalDisk->name, DISK_MAX_NAMELENGTH);
  userDisk->deviceNumber = physicalDisk->deviceNumber;
  userDisk->type = physicalDisk->type;
  strncpy(userDisk->model, (char *) physicalDisk->model, DISK_MAX_MODELLENGTH);
  userDisk->model[DISK_MAX_MODELLENGTH - 1] = '\0';
  userDisk->flags = physicalDisk->flags;
  userDisk->heads = physicalDisk->heads;
  userDisk->cylinders = physicalDisk->cylinders;
  userDisk->sectorsPerCylinder = physicalDisk->sectorsPerCylinder;
  userDisk->startSector = 0;
  userDisk->numSectors = physicalDisk->numSectors;
  userDisk->sectorSize = physicalDisk->sectorSize;

  return (status = 0);
}


static inline int checkDosSignature(unsigned char *sectorData)
{
  // Returns 1 if the buffer contains an MSDOS signature.

  if ((sectorData[510] != (unsigned char) 0x55) ||
      (sectorData[511] != (unsigned char) 0xAA))
    // No signature.  Return 0.
    return (0);
  else
    // We'll say this has an MSDOS signature.
    return (1);
}


static int isDosDisk(kernelPhysicalDisk *physicalDisk)
{
  // Return 1 if the physical disk appears to have an MS-DOS label on it.

  int status = 0;
  unsigned char *sectorData = NULL;

  sectorData = kernelMalloc(physicalDisk->sectorSize);
  if (sectorData == NULL)
    return (status = ERR_MEMORY);

  // Read the first sector of the device
  status =
    kernelDiskReadSectors((char *) physicalDisk->name, 0, 1, sectorData);
  if (status < 0)
    {
      kernelFree(sectorData);
      return (status);
    }

  // Is this a valid partition table?  Make sure the signature is at the end.
  status = checkDosSignature(sectorData);

  kernelFree(sectorData);

  if (status == 1)
    // Call this an MSDOS label.
    return (status);
  else
    // Not an MSDOS label
    return (status = 0);
}


static int isGptDisk(kernelPhysicalDisk *physicalDisk)
{
  // Return 1 if the physical disk appears to have a GPT label on it.

  int status = 0;
  unsigned char *sectorData = NULL;

  // A GPT disk must have a "guard" MS-DOS table, so a call to the MS-DOS
  // detect() function must succeed first.
  if (isDosDisk(physicalDisk) != 1)
    // Not a GPT label
    return (status = 0);

  sectorData = kernelMalloc(physicalDisk->sectorSize);
  if (sectorData == NULL)
    return (status = ERR_MEMORY);

  // Read the header.  The guard MS-DOS table in the first sector.  Read
  // the second sector.
  status =
    kernelDiskReadSectors((char *) physicalDisk->name, 1, 1, sectorData);
  if (status < 0)
    {
      kernelError(kernel_error, "Can't read GPT header");
      kernelFree(sectorData);
      return (status);
    }

  // Check for the GPT signature 
  if (kernelMemCmp(sectorData, "EFI PART", 8))
    {
      // No signature.
      kernelFree(sectorData);
      return (status = 0);
    }

  // Say it's GPT
  kernelFree(sectorData);
  return (status = 1);
}


static unsigned gptHeaderChecksum(unsigned char *sectorData)
{
  // Given a GPT header, compute the checksum

  unsigned *headerBytesField = ((unsigned *)(sectorData + 12));
  unsigned *checksumField = ((unsigned *)(sectorData + 16));
  unsigned oldChecksum = 0;
  unsigned checksum = 0;

  // Zero the checksum field
  oldChecksum = *checksumField;
  *checksumField = 0;

  // Get the checksum
  checksum = kernelCrc32(sectorData, *headerBytesField, NULL);

  *checksumField = oldChecksum;

  return (checksum);
}


static int readGptPartitions(kernelPhysicalDisk *physicalDisk,
			     kernelDisk *newLogicalDisks[],
			     int *newLogicalDiskCounter)
{
  int status = 0;
  unsigned char *sectorData = NULL;
  unsigned checksum = 0;
  uquad_t entriesLogical = 0;
  unsigned numEntries = 0;
  unsigned entrySize = 0;
  unsigned char *entry = NULL;
  kernelDisk *logicalDisk = NULL;
  guid *typeGuid = NULL;
  gptPartType gptType;
  unsigned count;

  sectorData = kernelMalloc(physicalDisk->sectorSize);
  if (sectorData == NULL)
    return (status = ERR_MEMORY);

  // Read the header.  The guard MS-DOS table in the first sector.  Read
  // the second sector.
  status =
    kernelDiskReadSectors((char *) physicalDisk->name, 1, 1, sectorData);
  if (status < 0)
    {
      kernelFree(sectorData);
      return (status);
    }

  checksum = *((unsigned *)(sectorData + 16));
  entriesLogical = *((uquad_t *)(sectorData + 72));
  numEntries = *((unsigned *)(sectorData + 80));
  entrySize = *((unsigned *)(sectorData + 84));

  // Check the checksum
  if (checksum != gptHeaderChecksum(sectorData))
    {
      kernelError(kernel_error, "GPT header bad checksum");
      kernelFree(sectorData);
      return (status = ERR_BADDATA);
    }
  
  // Reallocate the buffer for reading the entries
  kernelFree(sectorData);
  sectorData = kernelMalloc(numEntries * entrySize);
  if (sectorData == NULL)
    return (status = ERR_MEMORY);

  // Read the first sector of the entries.
  status = kernelDiskReadSectors((char *) physicalDisk->name,
				 (unsigned) entriesLogical,
				 ((numEntries * entrySize) /
				  physicalDisk->sectorSize), sectorData);
  if (status < 0)
    {
      kernelFree(sectorData);
      return (status);
    }
  
  for (count = 0; ((count < numEntries) &&
		   (physicalDisk->numLogical < DISK_MAX_PARTITIONS)); count ++)
    {
      entry = (sectorData + (count * entrySize));
      logicalDisk = &(physicalDisk->logical[physicalDisk->numLogical]);

      typeGuid = (guid *) entry;

      if (!kernelMemCmp(typeGuid, &GUID_BLANK, sizeof(guid)))
	// Empty
	continue;

      // We will add a logical disk corresponding to the partition we've
      // discovered
      sprintf((char *) logicalDisk->name, "%s%c", physicalDisk->name,
	      ('a' + physicalDisk->numLogical));

      // Assume UNKNOWN partition type for now.
      strcpy((char *) logicalDisk->partType, physicalDisk->description);

      // Now try to figure out the real one.
      if (kernelDiskGetGptPartType((guid *) entry, &gptType) >= 0)
	strncpy((char *) logicalDisk->partType, gptType.description,
		FSTYPE_MAX_NAMELENGTH);

      strncpy((char *) logicalDisk->fsType, "unknown", FSTYPE_MAX_NAMELENGTH);
      logicalDisk->physical = physicalDisk;
      logicalDisk->startSector = (unsigned) *((uquad_t *)(entry + 32));
      logicalDisk->numSectors =
	((unsigned) *((uquad_t *)(entry + 40)) - logicalDisk->startSector + 1);
      logicalDisk->primary = 1;

      newLogicalDisks[*newLogicalDiskCounter] = logicalDisk;
      *newLogicalDiskCounter += 1;
      physicalDisk->numLogical += 1;
    }

  kernelFree(sectorData);
  return (status = 0);
}


static int readDosPartitions(kernelPhysicalDisk *physicalDisk,
			     kernelDisk *newLogicalDisks[],
			     int *newLogicalDiskCounter)
{
  // Given a disk with an MS-DOS label, read the partitions and construct
  // the logical disks.

  int status = 0;
  unsigned char *sectorData = NULL;
  unsigned char *partitionRecord = NULL;
  unsigned char *extendedRecord = NULL;
  int partition = 0;
  kernelDisk *logicalDisk = NULL;
  unsigned char msdosTag = 0;
  msdosPartType msdosType;
  uquad_t startSector = 0;
  uquad_t extendedStartSector = 0;

  sectorData = kernelMalloc(physicalDisk->sectorSize);
  if (sectorData == NULL)
    return (status = ERR_MEMORY);

  // Read the first sector of the disk
  status =
    kernelDiskReadSectors((char *) physicalDisk->name, 0, 1, sectorData);
  if (status < 0)
    {
      kernelFree(sectorData);
      return (status);
    }

  while (physicalDisk->numLogical < DISK_MAX_PARTITIONS)
    {
      extendedRecord = NULL;

      // Set this pointer to the first partition record in the
      // master boot record
      partitionRecord = (sectorData + 0x01BE);

      // Loop through the partition records, looking for non-zero
      // entries
      for (partition = 0; partition < 4; partition ++)
	{
	  logicalDisk = &(physicalDisk->logical[physicalDisk->numLogical]);

	  msdosTag = partitionRecord[4];
	  if (msdosTag == 0)
	    {
	      // The "rules" say we must be finished with this
	      // physical device.  But that is not the way things
	      // often happen in real life -- empty records often
	      // come before valid ones.
	      partitionRecord += 16;
	      continue;
	    }

	  if (MSDOS_TAG_IS_EXTD(msdosTag))
	    {
	      extendedRecord = partitionRecord;
	      partitionRecord += 16;
	      continue;
	    }

	  // Assume UNKNOWN (code 0) partition type for now.
	  msdosType.tag = 0;
	  strcpy((char *) msdosType.description, physicalDisk->description);

	  // Now try to figure out the real one.
	  kernelDiskGetMsdosPartType(msdosTag, &msdosType);
	  
	  // We will add a logical disk corresponding to the
	  // partition we've discovered
	  sprintf((char *) logicalDisk->name, "%s%c",
		  physicalDisk->name,
		  ('a' + physicalDisk->numLogical));
	  strncpy((char *) logicalDisk->partType,
		  msdosType.description, FSTYPE_MAX_NAMELENGTH);
	  strncpy((char *) logicalDisk->fsType, "unknown",
		  FSTYPE_MAX_NAMELENGTH);
	  logicalDisk->physical = physicalDisk;
	  logicalDisk->startSector =
	    (startSector + *((unsigned *)(partitionRecord + 0x08)));
	  logicalDisk->numSectors =
	    *((unsigned *)(partitionRecord + 0x0C));
	  if (!extendedStartSector)
	    logicalDisk->primary = 1;
		  
	  newLogicalDisks[*newLogicalDiskCounter] = logicalDisk;
	  *newLogicalDiskCounter += 1;
	  physicalDisk->numLogical += 1;

	  // If the partition's ending geometry values (heads and sectors) are
	  // larger from what we've already recorded for the physical disk,
	  // change the values in the physical disk to patch the partitions.
	  if ((partitionRecord[5] >= physicalDisk->heads) ||
	      ((partitionRecord[6] & 0x3F) > physicalDisk->sectorsPerCylinder))
	    {
	      physicalDisk->heads = (partitionRecord[5] + 1);
	      physicalDisk->sectorsPerCylinder = (partitionRecord[6] & 0x3F);
	      physicalDisk->cylinders =
		(physicalDisk->numSectors /
		 (physicalDisk->heads * physicalDisk->sectorsPerCylinder));
	    }

	  // Move to the next partition record
	  partitionRecord += 16;
	}

      if (!extendedRecord)
	break;

      // Make sure the extended entry doesn't loop back on itself.
      // It can happen.
      if (extendedStartSector &&
	  ((*((unsigned *)(extendedRecord + 0x08)) +
	    extendedStartSector) == startSector))
	{
	  kernelError(kernel_error, "Extended partition links to itself");
	  break;
	}

      // We have an extended partition chain.  We need to go through
      // that as well.
      startSector = *((unsigned *)(extendedRecord + 0x08));

      if (!extendedStartSector)
	extendedStartSector = startSector;
      else
	startSector += extendedStartSector;	

      if (kernelDiskReadSectors((char *) physicalDisk->name, startSector,
				1, sectorData) < 0)
	break;
    }

  kernelFree(sectorData);
  return (status = 0);
}


static int unmountAll(void)
{
  // This routine will unmount all mounted filesystems from the disks,
  // including the root filesystem.

  int status = 0;
  kernelDisk *theDisk = NULL;
  int errors = 0;
  int count;

  // We will loop through all of the mounted disks, unmounting each of them
  // (except the root disk) until only root remains.  Finally, we unmount the
  // root also.

  for (count = 0; count < logicalDiskCounter; count ++)
    {
      theDisk = logicalDisks[count];

      if (!theDisk->filesystem.mounted)
	continue;

      if (!strcmp((char *) theDisk->filesystem.mountPoint, "/"))
	continue;

      // Unmount this filesystem
      status =
	kernelFilesystemUnmount((char *) theDisk->filesystem.mountPoint);
      if (status < 0)
	{
	  // Don't quit, just make an error message
	  kernelError(kernel_warn, "Unable to unmount filesystem %s from "
		      "disk %s", theDisk->filesystem.mountPoint,
		      theDisk->name);
	  errors++;
	  continue;
	}
    }

  // Now unmount the root filesystem
  status = kernelFilesystemUnmount("/");
  if (status < 0)
    // Don't quit, just make an error message
    errors++;

  // If there were any errors, we should return an error code of some kind
  if (errors)
    return (status = ERR_INVALID);
  else
    // Return success
    return (status = 0);
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


int kernelDiskRegisterDevice(kernelDevice *dev)
{
  // This routine will receive a new device structure, add the
  // kernelPhysicalDisk to our array, and register all of its logical disks
  // for use by the system.

  int status = 0;
  kernelPhysicalDisk *physicalDisk = NULL;
  int count;

  // Check params
  if (dev == NULL)
    {
      kernelError(kernel_error, "Disk device structure is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  physicalDisk = dev->data;

  if ((physicalDisk == NULL) || (physicalDisk->driver == NULL))
    {
      kernelError(kernel_error, "Physical disk structure or driver is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  // Make sure the arrays of disk structures aren't full
  if ((physicalDiskCounter >= DISK_MAXDEVICES) ||
      (logicalDiskCounter >= DISK_MAXDEVICES))
    {
      kernelError(kernel_error, "Max disk structures already registered");
      return (status = ERR_NOFREE);
    }

  // Compute the name for the disk, depending on what type of device it is
  if (physicalDisk->type & DISKTYPE_FLOPPY)
    sprintf((char *) physicalDisk->name, "fd%d", numFloppyDisks++);
  else if (physicalDisk->type & DISKTYPE_CDROM)
    sprintf((char *) physicalDisk->name, "cd%d", numCdRoms++);
  else if (physicalDisk->type & DISKTYPE_SCSIDISK)
    sprintf((char *) physicalDisk->name, "sd%d", numScsiDisks++);
  else if (physicalDisk->type & DISKTYPE_HARDDISK)
    sprintf((char *) physicalDisk->name, "hd%d", numHardDisks++);

  // Disk cache initialization is deferred until cache use is attempted.
  // Otherwise we waste memory allocating caches for disks that might
  // never be used.
  
  // Lock the disk
  status = kernelLockGet(&physicalDisk->lock);
  if (status < 0)
    return (status = ERR_NOLOCK);

  // Add the physical disk to our list
  physicalDisks[physicalDiskCounter++] = physicalDisk;

  // Loop through the physical device's logical disks
  for (count = 0; count < physicalDisk->numLogical; count ++)
    // Put the device at the end of the list and increment the counter
    logicalDisks[logicalDiskCounter++] = &physicalDisk->logical[count];

  // If it's a floppy, make sure the motor is off
  if (physicalDisk->type & DISKTYPE_FLOPPY)
    motorOff(physicalDisk);

  // Reset the 'last access' and 'last sync' values
  physicalDisk->lastAccess = kernelSysTimerRead();
  
  // Unlock the disk
  kernelLockRelease(&physicalDisk->lock);

  // Success
  return (status = 0);
}


int kernelDiskRemoveDevice(kernelDevice *dev)
{
  // This routine will receive a new device structure, remove the
  // kernelPhysicalDisk from our array, and remove all of its logical disks

  int status = 0;
  kernelPhysicalDisk *physicalDisk = NULL;
  kernelDisk *newLogicalDisks[DISK_MAXDEVICES];
  int newLogicalDiskCounter = 0;
  int position = -1;
  int count;

  // Check params
  if (dev == NULL)
    {
      kernelError(kernel_error, "Disk device structure is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  physicalDisk = dev->data;

  if ((physicalDisk == NULL) || (physicalDisk->driver == NULL))
    {
      kernelError(kernel_error, "Physical disk structure or driver is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  // Add all the logical disks that don't belong to this physical disk
  for (count = 0; count < logicalDiskCounter; count ++)
    if (logicalDisks[count]->physical != physicalDisk)
      newLogicalDisks[newLogicalDiskCounter++] = logicalDisks[count];

  // Now copy our new array of logical disks
  for (logicalDiskCounter = 0; logicalDiskCounter < newLogicalDiskCounter;
       logicalDiskCounter ++)
    logicalDisks[logicalDiskCounter] = newLogicalDisks[logicalDiskCounter];

  // Remove this physical disk from our array.  Find its position
  for (count = 0; count < physicalDiskCounter; count ++)
    {
      if (physicalDisks[count] == physicalDisk)
	{
	  position = count;
	  break;
	}
    }

  if (position >= 0)
    {
      if ((physicalDiskCounter > 1) && (position < (physicalDiskCounter - 1)))
	{
	  for (count = position; count < (physicalDiskCounter - 1); count ++)
	    physicalDisks[count] = physicalDisks[count + 1];
	}

      physicalDiskCounter -= 1;
    }

  return (status = 0);
}


int kernelDiskInitialize(void)
{
  // This is the "initialize" routine which invokes  the driver routine 
  // designed for that function.  Normally it returns zero, unless there
  // is an error.  If there's an error it returns negative.
  
  int status = 0;
  kernelPhysicalDisk *physicalDisk = NULL;
  kernelDisk *logicalDisk = NULL;
  int count1, count2;

  // Check whether any disks have been registered.  If not, that's 
  // an indication that the hardware enumeration has not been done
  // properly.  We'll issue an error in this case
  if (physicalDiskCounter <= 0)
    {
      kernelError(kernel_error, "No disks have been registered");
      return (status = ERR_NOTINITIALIZED);
    }

  // Spawn the disk daemon
  status = spawnDiskd();
  if (status < 0)
    kernelError(kernel_warn, "Unable to start disk thread");

  // We're initialized
  initialized = 1;

  // Read the partition tables
  status = kernelDiskReadPartitionsAll();
  if (status < 0)
    kernelError(kernel_error, "Unable to read disk partitions");

  // Copy the name of the physical boot disk
  strcpy(bootDisk, kernelOsLoaderInfo->bootDisk);

  // If we booted from a hard disk, we need to find out which partition
  // (logical disk) it was.
  if (!strncmp(bootDisk, "hd", 2) || !strncmp(bootDisk, "sd", 2))
    {
      // Loop through the physical disks and find the one with this name
      for (count1 = 0; count1 < physicalDiskCounter; count1 ++)
	{
	  physicalDisk = physicalDisks[count1];
	  if (!strcmp((char *) physicalDisk->name, bootDisk))
	    {
	      // This is the physical disk we booted from.  Find the
	      // partition
	      for (count2 = 0; count2 < physicalDisk->numLogical; count2 ++)
		{
		  logicalDisk = &(physicalDisk->logical[count2]);
		  // If the boot sector we booted from is in this partition,
		  // save its name as our boot disk.
		  if (logicalDisk->startSector ==
		      kernelOsLoaderInfo->bootSector)
		    {
		      strcpy(bootDisk, (char *) logicalDisk->name);
		      break;
		    }
		}
	      break;
	    }
	}
    }

  return (status = 0);
}


void kernelDiskAutoMount(kernelDisk *theDisk)
{
  // Given a disk, see if it is listed in the mount.conf file, whether it
  // is supposed to be automounted, and if so, mount it.

  int status = 0;
  variableList mountConfig;
  char variable[128];
  char value[128];
  char mountPoint[MAX_PATH_LENGTH];

  // Already mounted?
  if (theDisk->filesystem.mounted)
    return;

  // Try reading the mount configuration file
  status = kernelConfigRead(DISK_MOUNT_CONFIG, &mountConfig);
  if (status < 0)
    return;

  // See if a mount point is specified
  snprintf(variable, 128, "%s.mountpoint", theDisk->name);
  status = kernelVariableListGet(&mountConfig, variable, value, 128);
  if (status < 0)
    goto out;

  status = kernelFileFixupPath(value, mountPoint);
  if (status < 0)
    goto out;

  // See if we're supposed to automount it.
  snprintf(variable, 128, "%s.automount", theDisk->name);
  status = kernelVariableListGet(&mountConfig, variable, value, 128);
  if (status < 0)
    goto out;

  if (strcasecmp(value, "yes"))
    goto out;

  if ((theDisk->physical->type & DISKTYPE_REMOVABLE) &&
      // See if there's any media there
      !kernelDiskGetMediaState((const char *) theDisk->name))
    {
      kernelError(kernel_error, "Can't automount %s on disk %s - no media",
		  mountPoint, theDisk->name);
      goto out;
    }

  kernelFilesystemMount((const char *) theDisk->name, mountPoint);

 out:
  kernelVariableListDestroy(&mountConfig);
  return;
}


void kernelDiskAutoMountAll(void)
{
  int count;

  // Loop through the logical disks and see whether they should be
  // automounted.
  for (count = 0; count < logicalDiskCounter; count ++)
    kernelDiskAutoMount(logicalDisks[count]);

  return;
}


int kernelDiskInvalidateCache(const char *diskName)
{
  // Invalidate the cache of the named disk

  int status = 0;

  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check params
  if (diskName == NULL)
    return (status = ERR_NULLPARAMETER);

#if (DISK_CACHE)
  kernelPhysicalDisk *physicalDisk = NULL;

  physicalDisk = getPhysicalByName(diskName);
  if (physicalDisk == NULL)
    {
      kernelError(kernel_error, "No such disk \"%s\"", diskName);
      return (status = ERR_NOSUCHENTRY);
    }

  // Lock the physical disk
  status = kernelLockGet(&physicalDisk->lock);
  if (status < 0)
    {
      kernelError(kernel_error, "Unable to lock disk \"%s\" for cache "
		  "invalidation", physicalDisk->name);
      return (status);
    }

  status = cacheInvalidate(physicalDisk);
  
  kernelLockRelease(&physicalDisk->lock);  
  
  if (status < 0)
    kernelError(kernel_warn, "Error invalidating disk \"%s\" cache",
		physicalDisk->name);
#endif // DISK_CACHE

  return (status);
}


int kernelDiskShutdown(void)
{
  // Shut down.

  int status = 0;
  kernelPhysicalDisk *physicalDisk = NULL;
  int count;
  
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Unmount all the disks
  unmountAll();

  // Synchronize all the disks
  status = kernelDiskSyncAll();

  for (count = 0; count < physicalDiskCounter; count ++)
    {
      physicalDisk = physicalDisks[count];

      // Lock the disk
      status = kernelLockGet(&physicalDisk->lock);
      if (status < 0)
	return (status = ERR_NOLOCK);
      
      if ((physicalDisk->type & DISKTYPE_REMOVABLE) &&
	  (physicalDisk->flags & DISKFLAG_MOTORON))
	motorOff(physicalDisk);

      // Unlock the disk
      kernelLockRelease(&physicalDisk->lock);
    }

  return (status);
}


int kernelDiskFromLogical(kernelDisk *logical, disk *userDisk)
{
  // Takes our logical disk kernel structure and turns it into a user space
  // 'disk' object

  int status = 0;

  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check params
  if ((logical == NULL) || (userDisk == NULL))
    return (status = ERR_NULLPARAMETER);

  kernelMemClear(userDisk, sizeof(disk));

  // Get the physical disk info
  status = diskFromPhysical(logical->physical, userDisk);
  if (status < 0)
    return (status);

  // Add/override some things specific to logical disks
  strncpy(userDisk->name, (char *) logical->name, DISK_MAX_NAMELENGTH);
  userDisk->type = ((logical->physical->type & ~DISKTYPE_LOGICALPHYSICAL) |
		    DISKTYPE_LOGICAL);
  if (logical->primary)
    userDisk->type |= DISKTYPE_PRIMARY;
  userDisk->flags = logical->physical->flags;
  strncpy(userDisk->partType, (char *) logical->partType,
	  FSTYPE_MAX_NAMELENGTH);
  strncpy(userDisk->fsType, (char *) logical->fsType, FSTYPE_MAX_NAMELENGTH);
  userDisk->opFlags = logical->opFlags;
  userDisk->startSector = logical->startSector;
  userDisk->numSectors = logical->numSectors;

  // Filesystem-related
  strncpy(userDisk->label, (char *) logical->filesystem.label,
	  MAX_NAME_LENGTH);
  userDisk->blockSize = logical->filesystem.blockSize;
  userDisk->minSectors = logical->filesystem.minSectors;
  userDisk->maxSectors = logical->filesystem.maxSectors;
  userDisk->mounted = logical->filesystem.mounted;
  if (userDisk->mounted)
    {
      userDisk->freeBytes =
	kernelFilesystemGetFreeBytes((char *) logical->filesystem.mountPoint);
      strncpy(userDisk->mountPoint, (char *) logical->filesystem.mountPoint,
	      MAX_PATH_LENGTH);
    }
  userDisk->readOnly = logical->filesystem.readOnly;

  return (status = 0);
}


kernelDisk *kernelDiskGetByName(const char *name)
{
  // This routine takes the name of a logical disk and finds it in the
  // array, returning a pointer to the disk.  If the disk doesn't exist,
  // the function returns NULL

  kernelDisk *theDisk = NULL;
  int count;

  if (!initialized)
    return (theDisk = NULL);

  // Check params
  if (name == NULL)
    {
      kernelError(kernel_error, "Disk name is NULL");
      return (theDisk = NULL);
    }

  for (count = 0; count < logicalDiskCounter; count ++)
    if (!strcmp(name, (char *) logicalDisks[count]->name))
      {
	theDisk = logicalDisks[count];
	break;
      }

  return (theDisk);
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
//  Below here, the functions are exported outside the kernel to user
//  space.
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


int kernelDiskReadPartitions(const char *diskName)
{
  // Read the partition table for the requested physical disk, and
  // (re)build the list of logical disks.  This will be done initially at
  // startup time, but can be re-called during operation if the partitions
  // have been changed.

  int status = 0;
  kernelPhysicalDisk *physicalDisk = NULL;
  kernelDisk *newLogicalDisks[DISK_MAXDEVICES];
  int newLogicalDiskCounter = 0;
  int mounted = 0;
  kernelDisk *logicalDisk = NULL;
  msdosPartType msdosType;
  int count;

  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check params
  if (diskName == NULL)
    return (status = ERR_NULLPARAMETER);

  // Find the disk structure.
  physicalDisk = getPhysicalByName(diskName);
  if (physicalDisk == NULL)
    // No such disk.
    return (status = ERR_NOSUCHENTRY);

  // Add all the logical disks that don't belong to this physical disk
  for (count = 0; count < logicalDiskCounter; count ++)
    if (logicalDisks[count]->physical != physicalDisk)
      newLogicalDisks[newLogicalDiskCounter++] = logicalDisks[count];

  // Assume UNKNOWN (code 0) partition type for now.
  msdosType.tag = 0;
  strcpy((char *) msdosType.description, physicalDisk->description);

  // If this is a hard disk, get the logical disks from reading the partitions.
  if (physicalDisk->type & DISKTYPE_HARDDISK)
    {
      // It's a hard disk.  We need to read the partition table

      // Make sure it has no mounted partitions.
      mounted = 0;
      for (count = 0; count < physicalDisk->numLogical; count ++)
	if (physicalDisk->logical[count].filesystem.mounted)
	  {
	    kernelError(kernel_warn, "Logical disk %s is mounted.  Will "
			"not rescan %s until reboot.",
			physicalDisk->logical[count].name,
			physicalDisk->name);
	    mounted = 1;
	    break;
	  }

      if (mounted)
	{
	  // It has mounted partitions.  Add the existing logical disks to
	  // our array and continue to the next physical disk.
	  for (count = 0; count < physicalDisk->numLogical; count ++)
	    newLogicalDisks[newLogicalDiskCounter++] =
	      &(physicalDisk->logical[count]);
	  return (status = 1);
	}

      // Clear the logical disks
      physicalDisk->numLogical = 0;
      kernelMemClear(&(physicalDisk->logical),
		     (sizeof(kernelDisk) * DISK_MAX_PARTITIONS));

      // Check to see if it's a GPT disk first, since a GPT disk is also
      // technically an MS-DOS disk.
      if (isGptDisk(physicalDisk) == 1)
	status = readGptPartitions(physicalDisk, newLogicalDisks,
				   &newLogicalDiskCounter);

      // Now check whether it's an MS-DOS disk.
      else if (isDosDisk(physicalDisk) == 1)
	status = readDosPartitions(physicalDisk, newLogicalDisks,
				   &newLogicalDiskCounter);

      if (status < 0)
	return (status);
    }

  else
    {
      // If this is a not a hard disk, make the logical disk be the same
      // as the physical disk
      physicalDisk->numLogical = 1;
      logicalDisk = &(physicalDisk->logical[0]);
      // Logical disk name same as device name
      strcpy((char *) logicalDisk->name, (char *) physicalDisk->name);
      strncpy((char *) logicalDisk->partType, msdosType.description,
	      FSTYPE_MAX_NAMELENGTH);
      if (logicalDisk->fsType[0] == '\0')
	strncpy((char *) logicalDisk->fsType, "unknown",
		FSTYPE_MAX_NAMELENGTH);
      logicalDisk->physical = physicalDisk;
      logicalDisk->startSector = 0;
      logicalDisk->numSectors = physicalDisk->numSectors;

      newLogicalDisks[newLogicalDiskCounter++] = logicalDisk;
    }

  // Now copy our new array of logical disks
  for (logicalDiskCounter = 0; logicalDiskCounter < newLogicalDiskCounter;
       logicalDiskCounter ++)
    logicalDisks[logicalDiskCounter] = newLogicalDisks[logicalDiskCounter];

  // See if we can determine the filesystem types
  for (count = 0; count < logicalDiskCounter; count ++)
    {
      logicalDisk = logicalDisks[count];
      
      if (logicalDisk->physical == physicalDisk)
	{
	  if (physicalDisk->flags & DISKFLAG_MOTORON)
	    kernelFilesystemScan(logicalDisk);

	  kernelLog("Disk %s (hard disk %s, %s): %s",
		    logicalDisk->name, physicalDisk->name,
		    (logicalDisk->primary? "primary" : "logical"),
		    logicalDisk->fsType);
	}
    }

  return (status = 0);
}


int kernelDiskReadPartitionsAll(void)
{
  // Read the partition tables for all the registered physical disks, and
  // (re)build the list of logical disks.  This will be done initially at
  // startup time, but can be re-called during operation if the partitions
  // have been changed.

  int status = 0;
  int mounts = 0;
  int errors = 0;
  int count;

  if (!initialized)
    return (errors = ERR_NOTINITIALIZED);

  // Loop through all of the registered physical disks
  for (count = 0; count < physicalDiskCounter; count ++)
    {
      status = kernelDiskReadPartitions((char *) physicalDisks[count]->name);
      if (status < 0)
	errors = status;
      else
	mounts += status;
    }

  if (errors)
    return (status = errors);
  else
    return (status = mounts);
}


int kernelDiskSync(const char *diskName)
{
  // Synchronize the named physical disk.
  
  int status = 0;
  kernelPhysicalDisk *physicalDisk = NULL;
  kernelDisk *logicalDisk = NULL;
  int errors = 0;

  if (!initialized)
    return (errors = ERR_NOTINITIALIZED);

  // Check params
  if (diskName == NULL)
    return (status = ERR_NULLPARAMETER);

  // Get the disk structure
  physicalDisk = getPhysicalByName(diskName);
  if (physicalDisk == NULL)
    {
      // Try logical
      if ((logicalDisk = kernelDiskGetByName(diskName)))
	physicalDisk = logicalDisk->physical;
      else
	return (status = ERR_NOSUCHENTRY);
    }

  // Lock the physical disk
  status = kernelLockGet(&physicalDisk->lock);
  if (status < 0)
    {
      kernelError(kernel_error, "Unable to lock disk \"%s\" for sync",
		  physicalDisk->name);
      return (status);
    }

  // If disk caching is enabled, write out dirty sectors
#if (DISK_CACHE)
  status = cacheSync(physicalDisk);
  if (status < 0)
    {
      kernelError(kernel_warn, "Error synchronizing disk \"%s\" cache",
		  physicalDisk->name);
      errors = status;
    }
#endif // DISK_CACHE

  // If the disk driver has a flush function, call it now
  if (((kernelDiskOps *) physicalDisk->driver->ops)->driverFlush)
    {
      status = ((kernelDiskOps *) physicalDisk->driver->ops)
	->driverFlush(physicalDisk->deviceNumber);
      if (status < 0)
	{
	  kernelError(kernel_warn, "Error flushing disk \"%s\"",
		      physicalDisk->name);
	  errors = status;
	}
    }  

  kernelLockRelease(&physicalDisk->lock);

  return (status = errors);
}


int kernelDiskSyncAll(void)
{
  // Syncronize all the registered physical disks.

  int status = 0;
  int errors = 0;
  int count;

  if (!initialized)
    return (errors = ERR_NOTINITIALIZED);

  // Loop through all of the registered physical disks
  for (count = 0; count < physicalDiskCounter; count ++)
    {
      status = kernelDiskSync((char *) physicalDisks[count]->name);
      if (status < 0)
	errors = status;
    }

  return (status = errors);
}


int kernelDiskGetBoot(char *boot)
{
  // Returns the disk name of the boot device

  int status = 0;

  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check params
  if (boot == NULL)
    return (status = ERR_NULLPARAMETER);
  
  strncpy(boot, bootDisk, DISK_MAX_NAMELENGTH);
  return (status = 0);
}


int kernelDiskGetCount(void)
{
  // Returns the number of registered logical disk structures.

  if (!initialized)
    return (ERR_NOTINITIALIZED);

  return (logicalDiskCounter);
}


int kernelDiskGetPhysicalCount(void)
{
  // Returns the number of registered physical disk structures.

  if (!initialized)
    return (ERR_NOTINITIALIZED);

  return (physicalDiskCounter);
}


int kernelDiskGet(const char *diskName, disk *userDisk)
{
  // Given a disk name, return the corresponding user space disk structure

  int status = 0;
  kernelPhysicalDisk *physicalDisk = NULL;
  kernelDisk *logicalDisk = NULL;

  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check params
  if ((diskName == NULL) || (userDisk == NULL))
    return (status = ERR_NULLPARAMETER);

  // Find the disk structure.

  // Try for a logical disk first.
  if ((logicalDisk = kernelDiskGetByName(diskName)))
    return(kernelDiskFromLogical(logicalDisk, userDisk));

  // Try physical instead
  else if ((physicalDisk = getPhysicalByName(diskName)))
    return(diskFromPhysical(physicalDisk, userDisk));

  else
    // No such disk.
    return (status = ERR_NOSUCHENTRY);
}


int kernelDiskGetAll(disk *userDiskArray, unsigned buffSize)
{
  // Return user space disk structures for each logical disk, up to
  // buffSize bytes

  int status = 0;
  unsigned doDisks = logicalDiskCounter;
  unsigned count;

  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check params
  if (userDiskArray == NULL)
    return (status = ERR_NULLPARAMETER);

  if ((buffSize / sizeof(disk)) < doDisks)
    doDisks = (buffSize / sizeof(disk));

  // Loop through the disks, filling the array supplied
  for (count = 0; count < doDisks; count ++)
    kernelDiskFromLogical(logicalDisks[count], &userDiskArray[count]);

  return (status = 0);
}


int kernelDiskGetAllPhysical(disk *userDiskArray, unsigned buffSize)
{
  // Return user space disk structures for each physical disk, up to
  // buffSize bytes

  int status = 0;
  unsigned doDisks = physicalDiskCounter;
  unsigned count;

  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check params
  if (userDiskArray == NULL)
    return (status = ERR_NULLPARAMETER);
 
  if ((buffSize / sizeof(disk)) < doDisks)
    doDisks = (buffSize / sizeof(disk));
 
  // Loop through the physical disks, filling the array supplied
  for (count = 0; count < doDisks; count ++)
    diskFromPhysical(physicalDisks[count], &userDiskArray[count]);

   return (status = 0);
}


int kernelDiskGetFilesystemType(const char *diskName, char *buffer,
				unsigned buffSize)
{
  // This function takes the supplied disk name and attempts to explicitly
  // detect the filesystem type.  Particularly useful for things like removable
  // media where the correct info may not be automatically provided in the
  // disk structure.

  int status = 0;
  kernelDisk *logicalDisk = NULL;

  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check params
  if ((diskName == NULL) || (buffer == NULL))
    return (status = ERR_NULLPARAMETER);

  // There must exist a logical disk with this name.
  logicalDisk = kernelDiskGetByName(diskName);
  if (logicalDisk == NULL)
    {
      // No such disk.
      kernelError(kernel_error, "No such disk \"%s\"", diskName);
      return (status = ERR_NOSUCHENTRY);
    }

  // See if we can determine the filesystem type
  status = kernelFilesystemScan(logicalDisk);
  if (status < 0)
    return (status);

  strncpy(buffer, (char *) logicalDisk->fsType, buffSize);
  return (status = 0);
}


int kernelDiskGetMsdosPartType(int tag, msdosPartType *type)
{
  // This function takes the supplied code and returns a corresponding
  // MS-DOS partition type structure in the memory provided.

  int status = 0;
  int count;

  // We don't check for initialization; the table is static.

  if (type == NULL)
    return (status = ERR_NULLPARAMETER);

  for (count = 0; (msdosPartTypes[count].tag != 0); count ++)
    if (msdosPartTypes[count].tag == tag)
      {
	kernelMemCopy(&msdosPartTypes[count], type, sizeof(msdosPartType));
	return (status = 0);
      }

  // Not found
  return (status = ERR_NOSUCHENTRY);
}


msdosPartType *kernelDiskGetMsdosPartTypes(void)
{
  // Allocate and return a copy of our table of known MS-DOS partition types
  // We don't check for initialization; the table is static.

  msdosPartType *types =
    kernelMemoryGet(sizeof(msdosPartTypes), "partition types");
  if (types == NULL)
    return (types);

  kernelMemCopy(msdosPartTypes, types, sizeof(msdosPartTypes));
  return (types);
}


int kernelDiskGetGptPartType(guid *g, gptPartType *type)
{
  // This function takes the supplied GUID and returns a corresponding
  // GPT partition type structure in the memory provided.

  int status = 0;
  int count;

  // We don't check for initialization; the table is static.

  if (type == NULL)
    return (status = ERR_NULLPARAMETER);

  for (count = 0;
       kernelMemCmp(&gptPartTypes[count].typeGuid, &GUID_BLANK, sizeof(guid));
       count ++)
    if (!kernelMemCmp(&gptPartTypes[count].typeGuid, g, sizeof(guid)))
      {
	kernelMemCopy(&gptPartTypes[count], type, sizeof(gptPartType));
	return (status = 0);
      }

  // Not found
  return (status = ERR_NOSUCHENTRY);
}


gptPartType *kernelDiskGetGptPartTypes(void)
{
  // Allocate and return a copy of our table of known GPT partition types
  // We don't check for initialization; the table is static.

  gptPartType *types =
    kernelMemoryGet(sizeof(gptPartTypes), "partition types");
  if (types == NULL)
    return (types);

  kernelMemCopy(gptPartTypes, types, sizeof(gptPartTypes));
  return (types);
}


int kernelDiskSetFlags(const char *diskName, unsigned flags, int set)
{
  // This routine is the user-accessible interface for setting or clearing
  // (user-settable) disk flags.

  int status = 0;
  kernelDisk *logicalDisk = NULL;
  kernelPhysicalDisk *physicalDisk = NULL;

  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check params
  if (diskName == NULL)
    return (status = ERR_NULLPARAMETER);

  // Only allow the user-settable flags
  flags &= DISKFLAG_USERSETTABLE;

  // Get the disk structure
  physicalDisk = getPhysicalByName(diskName);
  if (physicalDisk == NULL)
    {
      // Try logical
      if ((logicalDisk = kernelDiskGetByName(diskName)))
	physicalDisk = logicalDisk->physical;
      else
	return (status = ERR_NOSUCHENTRY);
    }

  // Reset the 'last access' value
  physicalDisk->lastAccess = kernelSysTimerRead();

  // Lock the disk
  status = kernelLockGet(&physicalDisk->lock);
  if (status < 0)
    goto out;

#if (DISK_CACHE)
  if ((set && (flags & DISKFLAG_READONLY)) || (flags & DISKFLAG_NOCACHE))
    {
      status = cacheSync(physicalDisk);
      if (status < 0)
	goto out;
    }
  if (flags & DISKFLAG_NOCACHE)
    {
      status = cacheInvalidate(physicalDisk);
      if (status < 0)
	goto out;
    }
#endif

  if (set)
    physicalDisk->flags |= flags;
  else
    physicalDisk->flags &= ~flags;

  status = 0;

 out:
  // Reset the 'last access' value
  physicalDisk->lastAccess = kernelSysTimerRead();

  // Unlock the disk
  kernelLockRelease(&physicalDisk->lock);

  return (status);
}

  
int kernelDiskSetLockState(const char *diskName, int state)
{
  // This routine is the user-accessible interface for locking or unlocking
  // a removable disk device.

  int status = 0;
  kernelDisk *logicalDisk = NULL;
  kernelPhysicalDisk *physicalDisk = NULL;

  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check params
  if (diskName == NULL)
    return (status = ERR_NULLPARAMETER);

  // Get the disk structure
  physicalDisk = getPhysicalByName(diskName);
  if (physicalDisk == NULL)
    {
      // Try logical
      if ((logicalDisk = kernelDiskGetByName(diskName)))
	physicalDisk = logicalDisk->physical;
      else
	return (status = ERR_NOSUCHENTRY);
    }

  // Reset the 'last access' value
  physicalDisk->lastAccess = kernelSysTimerRead();
  
  // Make sure the operation is supported
  if (((kernelDiskOps *) physicalDisk->driver->ops)
      ->driverSetLockState == NULL)
    {
      kernelError(kernel_error, "Driver routine is NULL");
      return (status = ERR_NOSUCHFUNCTION);
    }
  
  // Lock the disk
  status = kernelLockGet(&physicalDisk->lock);
  if (status < 0)
    return (status = ERR_NOLOCK);

  // Call the door lock operation
  status = ((kernelDiskOps *) physicalDisk->driver->ops)
    ->driverSetLockState(physicalDisk->deviceNumber, state);

  // Reset the 'last access' value
  physicalDisk->lastAccess = kernelSysTimerRead();

  // Unlock the disk
  kernelLockRelease(&physicalDisk->lock);

  return (status);
}


int kernelDiskSetDoorState(const char *diskName, int state)
{
  // This routine is the user-accessible interface for opening or closing
  // a removable disk device.

  int status = 0;
  kernelDisk *logicalDisk = NULL;
  kernelPhysicalDisk *physicalDisk = NULL;

  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check params
  if (diskName == NULL)
    return (status = ERR_NULLPARAMETER);

  // Get the disk structure
  physicalDisk = getPhysicalByName(diskName);
  if (physicalDisk == NULL)
    {
      // Try logical
      if ((logicalDisk = kernelDiskGetByName(diskName)))
	physicalDisk = logicalDisk->physical;
      else
	return (status = ERR_NOSUCHENTRY);
    }

  // Make sure it's a removable disk
  if (physicalDisk->type & DISKTYPE_FIXED)
    {
      kernelError(kernel_error, "Cannot open/close a non-removable disk");
      return (status = ERR_INVALID);
    }

  // Reset the 'last access' value
  physicalDisk->lastAccess = kernelSysTimerRead();
  
  // Make sure the operation is supported
  if (((kernelDiskOps *) physicalDisk->driver->ops)
      ->driverSetDoorState == NULL)
    {
      kernelError(kernel_error, "Driver routine is NULL");
      return (status = ERR_NOSUCHFUNCTION);
    }
  
  // Lock the disk
  status = kernelLockGet(&physicalDisk->lock);
  if (status < 0)
    return (status = ERR_NOLOCK);

#if (DISK_CACHE)
  // Make sure the cache is invalidated
  cacheInvalidate(physicalDisk);
#endif

  // Call the door control operation
  status = ((kernelDiskOps *) physicalDisk->driver->ops)
    ->driverSetDoorState(physicalDisk->deviceNumber, state);

  // Reset the 'last access' value
  physicalDisk->lastAccess = kernelSysTimerRead();

  // Unlock the disk
  kernelLockRelease(&physicalDisk->lock);

  return (status);
}


int kernelDiskGetMediaState(const char *diskName)
{
  // This routine returns 1 if the requested disk has media present,
  // 0 otherwise

  int status = 0;
  kernelDisk *logicalDisk = NULL;
  kernelPhysicalDisk *physicalDisk = NULL;
  void *buffer = NULL;

  if (!initialized)
    return (status = 0);

  // Check params
  if (diskName == NULL)
    return (status = 0);

  // Get the disk structure
  physicalDisk = getPhysicalByName(diskName);
  if (physicalDisk == NULL)
    {
      // Try logical
      if ((logicalDisk = kernelDiskGetByName(diskName)))
	physicalDisk = logicalDisk->physical;
      else
	return (status = 0);
    }

  // Reset the 'last access' value
  physicalDisk->lastAccess = kernelSysTimerRead();

  // Make sure it's a removable disk
  if (!(physicalDisk->type & DISKTYPE_REMOVABLE))
    return (status = 1);

  buffer = kernelMalloc(physicalDisk->sectorSize);
  if (buffer == NULL)
    return (status = 0);

  // Lock the disk
  status = kernelLockGet(&physicalDisk->lock);
  if (status < 0)
    {
      kernelFree(buffer);
      return (status = 0);
    }

  // Try to read one sector
  status =
    readWrite(physicalDisk, 0, 1, buffer, (IOMODE_READ | IOMODE_NOCACHE));

  // Unlock the disk
  kernelLockRelease(&physicalDisk->lock);

  kernelFree(buffer);

  if (status < 0)
    return (0);
  else
    return (1);  
}


int kernelDiskChanged(const char *diskName)
{
  // Returns 1 if the device a) is a removable type; and b) supports the
  // driverDiskChanged() function; and c) has had the disk changed.

  int changed = 0;
  kernelPhysicalDisk *physicalDisk = NULL;
  kernelDisk *logicalDisk = NULL;

  if (!initialized)
    return (changed = 0);

  // Check params
  if (diskName == NULL)
    return (changed = 0);

  // Get the disk structure
  physicalDisk = getPhysicalByName(diskName);
  if (physicalDisk == NULL)
    {
      // Try logical
      if ((logicalDisk = kernelDiskGetByName(diskName)))
	physicalDisk = logicalDisk->physical;
      else
	return (changed = 0);
    }

  // Reset the 'last access' value
  physicalDisk->lastAccess = kernelSysTimerRead();

  // Make sure it's a removable disk
  if (!(physicalDisk->type & DISKTYPE_REMOVABLE))
    return (changed = 0);

  // Make sure the the 'disk changed' function is implemented
  if (((kernelDiskOps *) physicalDisk->driver->ops)->driverDiskChanged == NULL)
    return (changed = 0);

  // Lock the disk
  if (kernelLockGet(&physicalDisk->lock) < 0)
    return (changed = 0);

  changed = ((kernelDiskOps *) physicalDisk->driver->ops)
    ->driverDiskChanged(physicalDisk->deviceNumber);

  if (changed)
    {
#if (DISK_CACHE)
  // Make sure the cache is invalidated
  cacheInvalidate(physicalDisk);
#endif
    }

  // Reset the 'last access' value
  physicalDisk->lastAccess = kernelSysTimerRead();

  // Unlock the disk
  kernelLockRelease(&physicalDisk->lock);

  return (changed);
}


int kernelDiskReadSectors(const char *diskName, uquad_t logicalSector,
			  uquad_t numSectors, void *dataPointer)
{
  // This routine is the user-accessible interface to reading data using
  // the various disk routines in this file.  Basically, it is a gatekeeper
  // that helps ensure correct use of the "read-write" method.  

  int status = 0;
  kernelPhysicalDisk *physicalDisk = NULL;
  kernelDisk *theDisk = NULL;

  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check params
  if ((diskName == NULL) || (dataPointer == NULL))
    return (status = ERR_NULLPARAMETER);

  // Get the disk structure.  Try a physical disk first.
  physicalDisk = getPhysicalByName(diskName);
  if (physicalDisk == NULL)
    {
      // Try logical
      theDisk = kernelDiskGetByName(diskName);
      if (theDisk == NULL)
	// No such disk.
	return (status = ERR_NOSUCHENTRY);

      // Start at the beginning of the logical volume.
      logicalSector += theDisk->startSector;
      
      // Make sure the logical sector number does not exceed the number
      // of logical sectors on this volume
      if ((logicalSector >= (theDisk->startSector + theDisk->numSectors)) ||
	  ((logicalSector + numSectors) >
	   (theDisk->startSector + theDisk->numSectors)))
	{
	  // Make a kernelError.
	  kernelError(kernel_error, "Sector range %llu-%llu exceeds volume "
		      "boundary of %llu", logicalSector,
		      (logicalSector + numSectors - 1),
		      (theDisk->startSector + theDisk->numSectors));
	  return (status = ERR_BOUNDS);
	}

      physicalDisk = theDisk->physical;

      if (physicalDisk == NULL)
	{
	  kernelError(kernel_error, "Logical disk's physical disk is NULL");
	  return (status = ERR_NOSUCHENTRY);
	}
    }

  // Reset the 'last access' value
  physicalDisk->lastAccess = kernelSysTimerRead();
  
  // Lock the disk
  status = kernelLockGet(&physicalDisk->lock);
  if (status < 0)
    return (status = ERR_NOLOCK);

  // Call the read-write routine for a read operation
  status = readWrite(physicalDisk, logicalSector, numSectors, dataPointer,
		     IOMODE_READ);

  // Reset the 'last access' value
  physicalDisk->lastAccess = kernelSysTimerRead();
  
  // Unlock the disk
  kernelLockRelease(&physicalDisk->lock);
  
  return (status);
}


int kernelDiskWriteSectors(const char *diskName, uquad_t logicalSector, 
			   uquad_t numSectors, const void *data)
{
  // This routine is the user-accessible interface to writing data using
  // the various disk routines in this file.  Basically, it is a gatekeeper
  // that helps ensure correct use of the "read-write" method.
  
  int status = 0;
  kernelPhysicalDisk *physicalDisk = NULL;
  kernelDisk *theDisk = NULL;

  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check params
  if ((diskName == NULL) || (data == NULL))
    return (status = ERR_NULLPARAMETER);

  // Get the disk structure.  Try a physical disk first.
  physicalDisk = getPhysicalByName(diskName);
  if (physicalDisk == NULL)
    {
      // Try logical
      theDisk = kernelDiskGetByName(diskName);
      if (theDisk == NULL)
	// No such disk.
	return (status = ERR_NOSUCHENTRY);

      // Start at the beginning of the logical volume.
      logicalSector += theDisk->startSector;
      
      // Make sure the logical sector number does not exceed the number
      // of logical sectors on this volume
      if ((logicalSector >= (theDisk->startSector + theDisk->numSectors)) ||
	  ((logicalSector + numSectors) >
	   (theDisk->startSector + theDisk->numSectors)))
	{
	  // Make a kernelError.
	  kernelError(kernel_error, "Exceeding volume boundary");
	  return (status = ERR_BOUNDS);
	}
      
      physicalDisk = theDisk->physical;

      if (physicalDisk == NULL)
	{
	  kernelError(kernel_error, "Logical disk's physical disk is NULL");
	  return (status = ERR_NOSUCHENTRY);
	}
    }

  // Reset the 'last access' value
  physicalDisk->lastAccess = kernelSysTimerRead();

  // Lock the disk
  status = kernelLockGet(&physicalDisk->lock);
  if (status < 0)
    return (status = ERR_NOLOCK);

  // Call the read-write routine for a write operation
  status = readWrite(physicalDisk, logicalSector, numSectors, (void *) data,
		     IOMODE_WRITE);

  // Reset the 'last access' value
  physicalDisk->lastAccess = kernelSysTimerRead();

  // Unlock the disk
  kernelLockRelease(&physicalDisk->lock);

  return (status);
}


int kernelDiskEraseSectors(const char *diskName, uquad_t logicalSector, 
			   uquad_t numSectors, int passes)
{
  // This routine synchronously and securely erases disk sectors.  It writes
  // (passes - 1) successive passes of random data followed by a final pass
  // of NULLs.
  
  int status = 0;
  kernelPhysicalDisk *physicalDisk = NULL;
  kernelDisk *theDisk = NULL;
  unsigned bufferSize = 0;
  unsigned char *buffer = NULL;
  int count1;
  unsigned count2;

  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check params
  if (diskName == NULL)
    return (status = ERR_NULLPARAMETER);

  // Get the disk structure.  Try a physical disk first.
  physicalDisk = getPhysicalByName(diskName);
  if (physicalDisk == NULL)
    {
      // Try logical
      theDisk = kernelDiskGetByName(diskName);
      if (theDisk == NULL)
	// No such disk.
	return (status = ERR_NOSUCHENTRY);

      // Start at the beginning of the logical volume.
      logicalSector += theDisk->startSector;
      
      // Make sure the logical sector number does not exceed the number
      // of logical sectors on this volume
      if ((logicalSector >= (theDisk->startSector + theDisk->numSectors)) ||
	  ((logicalSector + numSectors) >
	   (theDisk->startSector + theDisk->numSectors)))
	{
	  // Make a kernelError.
	  kernelError(kernel_error, "Exceeding volume boundary");
	  return (status = ERR_BOUNDS);
	}
      
      physicalDisk = theDisk->physical;
    }

  // Reset the 'last access' value
  physicalDisk->lastAccess = kernelSysTimerRead();

  // Get a buffer for the data
  bufferSize = (numSectors * physicalDisk->sectorSize);
  buffer = kernelMalloc(bufferSize);
  if (buffer == NULL)
    return (status = ERR_MEMORY);

  // Lock the disk
  status = kernelLockGet(&physicalDisk->lock);
  if (status < 0)
    return (status = ERR_NOLOCK);

  for (count1 = 0; count1 < passes; count1 ++)
    {
      if (count1 < (passes - 1))
	{
	  // Fill the buffer with semi-random data
	  for (count2 = 0; count2 < physicalDisk->sectorSize; count2 ++)
	    buffer[count2] = kernelRandomFormatted(0, 255);

	  for (count2 = 1; count2 < numSectors; count2 ++)
	    kernelMemCopy(buffer,
			  (buffer + (count2 * physicalDisk->sectorSize)),
			  physicalDisk->sectorSize);
	}
      else
	// Clear the buffer with NULLs
	kernelMemClear(buffer, bufferSize);

      // Call the read-write routine for a write operation
      status = readWrite(physicalDisk, logicalSector, numSectors, buffer,
			 IOMODE_WRITE);
      if (status < 0)
	break;

#if (DISK_CACHE)
      // Flush the data
      status = cacheSync(physicalDisk);
      if (status < 0)
	break;
#endif // DISK_CACHE
    }

  kernelFree(buffer);

  // Reset the 'last access' value
  physicalDisk->lastAccess = kernelSysTimerRead();
  
  // Unlock the disk
  kernelLockRelease(&physicalDisk->lock);

  return (status);
}


int kernelDiskGetStats(const char *diskName, diskStats *stats)
{
  // Return performance stats about the supplied disk name (if non-NULL,
  // otherwise about all the disks combined).

  int status = 0;
  kernelPhysicalDisk *physicalDisk = NULL;
  kernelDisk *logicalDisk = NULL;
  int count;

  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check params.  It's okay for diskName to be NULL.
  if (stats == NULL)
    return (status = ERR_NULLPARAMETER);

  kernelMemClear(stats, sizeof(diskStats));

  if (diskName)
    {
      // Get the disk structure
      physicalDisk = getPhysicalByName(diskName);
      if (physicalDisk == NULL)
	{
	  // Try logical
	  if ((logicalDisk = kernelDiskGetByName(diskName)))
	    physicalDisk = logicalDisk->physical;
	  else
	    return (status = ERR_NOSUCHENTRY);
	}

      kernelMemCopy((void *) &physicalDisk->stats, stats, sizeof(diskStats));
    }
  else
    {
      for (count = 0; count < physicalDiskCounter; count ++)
	{
	  physicalDisk = physicalDisks[count];
	  stats->readTime += physicalDisk->stats.readTime;
	  stats->readKbytes += physicalDisk->stats.readKbytes;
	  stats->writeTime += physicalDisk->stats.writeTime;
	  stats->writeKbytes += physicalDisk->stats.writeKbytes;
	}
    }

  return (status = 0);
}

