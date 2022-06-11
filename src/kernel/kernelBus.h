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
//  kernelBus.h
//

#if !defined(_KERNELBUS_H)

#include "kernelDevice.h"

#define BUS_MAX_BUSES 16

typedef enum {
  bus_pci = 1, bus_usb = 2
} kernelBusType;

typedef struct {
  int target;
  kernelDeviceClass *class;
  kernelDeviceClass *subClass;

} kernelBusTarget;

typedef struct {
  int (*driverGetTargets) (kernelBusTarget **);
  int (*driverGetTargetInfo) (int, void *);
  unsigned (*driverReadRegister) (int, int, int);
  void (*driverWriteRegister) (int, int, int, unsigned);
  int (*driverDeviceEnable) (int, int);
  int (*driverSetMaster) (int, int);
  int (*driverRead) (int, unsigned, void *);
  int (*driverWrite) (int, unsigned, void *);

} kernelBusOps;

typedef struct {
  kernelBusType type;
  kernelBusOps *ops;

} kernelBus;

// Functions exported by kernelBus.c
int kernelBusRegister(kernelBusType, kernelDevice *);
int kernelBusGetTargets(kernelBusType, kernelBusTarget **);
int kernelBusGetTargetInfo(kernelBusType, int, void *);
unsigned kernelBusReadRegister(kernelBusType, int, int, int);
void kernelBusWriteRegister(kernelBusType, int, int, int, unsigned);
int kernelBusDeviceEnable(kernelBusType, int, int);
int kernelBusSetMaster(kernelBusType, int, int);
int kernelBusRead(kernelBusType, int, unsigned, void *);
int kernelBusWrite(kernelBusType, int, unsigned, void *);

#define _KERNELBUS_H
#endif
