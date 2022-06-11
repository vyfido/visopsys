//
//  Visopsys
//  Copyright (C) 1998-2003 J. Andrew McLaughlin
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

#include <sys/disk.h>

#define TRANSFER_AREA_ALIGN (64 * 1024)
#define RETRY_ATTEMPTS 5

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
  int (*driverReadSectors) (int, unsigned, unsigned,
		   unsigned, unsigned, unsigned, void *);
  int (*driverWriteSectors) (int, unsigned, unsigned,
		   unsigned, unsigned, unsigned, void *);
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
  diskType type;
  mediaType fixedRemovable;
  kernelAddrMethod addressingMethod;

  unsigned startHead;
  unsigned startCylinder;
  unsigned startSector;
  unsigned startLogicalSector;

  unsigned heads;
  unsigned cylinders;
  unsigned sectors;
  unsigned logicalSectors;
  unsigned sectorSize;
  unsigned maxSectorsPerOp;

  void *transferArea;
  void *transferAreaPhysical;
  unsigned transferAreaSize;

  int lock;
  int motorStatus;
  unsigned idleSince;

  kernelDiskDeviceDriver *deviceDriver;

} kernelDiskObject;

// Functions exported by kernelDiskFunctions.c
int kernelDiskFunctionsRegisterDevice(kernelDiskObject *);
int kernelDiskFunctionsInstallDriver(kernelDiskObject *,
				     kernelDiskDeviceDriver *);
int kernelDiskFunctionsInitialize(void);
int kernelDiskFunctionsShutdown(void);
int kernelDiskFunctionsGetBoot(void);
int kernelDiskFunctionsGetCount(void);
int kernelDiskFunctionsGetInfo(int, disk *);
kernelDiskObject *kernelFindDiskObjectByNumber(int);
int kernelDiskFunctionsMotorOn(int);
int kernelDiskFunctionsMotorOff(int);
int kernelDiskFunctionsDiskChanged(int);
int kernelDiskFunctionsReadSectors(int, unsigned, unsigned, void *);
int kernelDiskFunctionsWriteSectors(int, unsigned, unsigned, void *);
int kernelDiskFunctionsReadAbsoluteSectors(int, unsigned, unsigned, void *);
int kernelDiskFunctionsWriteAbsoluteSectors(int, unsigned, unsigned, void *);

#define _KERNELDISKFUNCTIONS_H
#endif
