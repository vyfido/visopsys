//
//  Visopsys
//  Copyright (C) 1998-2001 J. Andrew McLaughlin
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
//  kernelDiskFunctions.h
//
	
// These are the generic functions for disk access.  These are above the
// level of the filesystem, and will generally be called by the filesystem
// drivers.

#if !defined(_KERNELDISKFUNCTIONS_H)

#define TRANSFER_AREA_ALIGN (64 * 1024)
#define MOTOROFF_DELAY 40  // System timer ticks -- approx 2 seconds
#define RETRY_ATTEMPTS 5
#define DISK_BUFFER_HASH_SIZE 100

// Error messages
#define NO_DISKS_REGISTERED "Attempt to initialize the disk functions, but no disks have been registered"
#define NULL_DISK_OBJECT "The kernelDiskObject passed or referenced is NULL"
#define INVALID_DISK_NUMBER "The disk number used to refer to a disk object is not valid"
#define INCONSISTENT_DISK_NUMBER "The kernelDiskObject's diskNumber is not consistent with its array index"
#define NULL_DRIVER_OBJECT "The kernelDiskDeviceDriver object passed or referenced is NULL"
#define NULL_DRIVER_ROUTINE "The associated device driver routine is NULL or has not been installed"
#define TOO_MANY_DISKS "kernelDiskObjectArray full.  Max disk objects already registered"
#define XFER_NOT_INITIALIZED "The requested kernelDiskObject has not been assigned a data transfer area"
#define MEMORY_ALLOC_ERROR "Unable to allocate memory for all of the disk objects' transfer areas"
#define INITIALIZE_ERROR "Disk driver initialize routine: "
#define SPECIFY_ERROR "Disk driver specify routine: "
#define SELECT_ERROR "Disk driver select routine: "
#define MOTORON_ERROR "Disk driver motor on routine: "
#define CALIBRATE_ERROR "Disk driver recalibrate routine: "
#define RESET_ERROR "Disk driver reset routine: "
#define SEEK_ERROR "Disk driver seek routine: "
#define READ_ERROR "Disk driver read sectors routine: "
#define WRITE_ERROR "Disk driver write sectors routine: "
#define DISKCHANGED_ERROR "Disk driver media check routine: "
#define NOT_FIXED_DISK "The requested removable disk is incorrectly being used as a fixed disk"
#define NOT_REMOVABLE_DISK "The requested fixed disk is incorrectly being used as a removable disk"

/*
  These will eventually be used for the disk buffer cache

typedef struct
{
  int number;
  int dirty;
  void *buffer;

} kernelDiskSector;

typedef struct
{
  int startSector;
  unsigned int numSectors;
  kernelDiskSector *sectors;
  void *next;

} kernelDiskBuffer;
*/

typedef enum { fixed, removable } kernelDiskRemove;

typedef enum { floppy, idecdrom, scsicdrom,
	       idedisk, scsidisk } kernelDiskType;

typedef enum { readoperation, writeoperation } kernelDiskOp;

typedef enum { addr_pchs, addr_lba } kernelAddrMethod;

typedef struct
{
  // This is a structure containing pointers to the
  // generic disk driver routines

  int (*driverInitialize) (void);
  int (*driverDescribe) (int, ...);
  int (*driverReset) (int);
  int (*driverRecalibrate) (int);
  int (*driverMotorOn) (int);
  int (*driverMotorOff) (int);
  int (*driverDiskChanged) (int);
  int (*driverReadSectors) (int, unsigned int, unsigned int,
		   unsigned int, unsigned int, unsigned int, void *);
  int (*driverWriteSectors) (int, unsigned int, unsigned int,
		   unsigned int, unsigned int, unsigned int, void *);
  int (*driverLastErrorCode) (void);
  void *(*driverLastErrorMessage) (void);

} kernelDiskDeviceDriver;

typedef volatile struct 
{
  // This is a structure which contains information about a generic disk
  // drive device, including a reference to a "disk driver"

  int diskNumber;
  int driverDiskNumber;
  int dmaChannel;
  char *description;
  kernelDiskRemove fixedRemovable;
  kernelDiskType type;
  kernelAddrMethod addressingMethod;

  unsigned int startHead;
  unsigned int startCylinder;
  unsigned int startSector;
  unsigned int startLogicalSector;

  unsigned int heads;
  unsigned int cylinders;
  unsigned int sectors;
  unsigned int logicalSectors;
  unsigned int sectorSize;
  unsigned int maxSectorsPerOp;

  void *transferArea;
  void *transferAreaPhysical;
  unsigned int transferAreaSize;

  int lock;
  int motorStatus;

  kernelDiskDeviceDriver *deviceDriver;

} kernelDiskObject;


// Functions exported by kernelDiskFunctions.c

int kernelDiskFunctionsRegisterDevice(kernelDiskObject *);
int kernelDiskFunctionsRemoveDevice(int);
int kernelDiskFunctionsInstallDriver(kernelDiskObject *, 
				     kernelDiskDeviceDriver *);
int kernelDiskFunctionsInitialize(void);
kernelDiskObject *kernelFindDiskObjectByNumber(int);
int kernelDiskFunctionsLockDisk(int);
int kernelDiskFunctionsUnlockDisk(int);
int kernelDiskFunctionsMotorOn(int);
int kernelDiskFunctionsMotorOff(int);
int kernelDiskFunctionsDiskChanged(int);
int kernelDiskFunctionsReadSectors(int, unsigned int, unsigned int, void *);
int kernelDiskFunctionsWriteSectors(int, unsigned int, unsigned int, void *);

#define _KERNELDISKFUNCTIONS_H
#endif
