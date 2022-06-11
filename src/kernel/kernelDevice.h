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
//  kernelDevice.h
//

// Describes the generic description/classification mechanism for hardware
// devices.

#if !defined(_KERNELDEVICE_H)

#include "kernelDriver.h"
#include <sys/device.h>

// The generic hardware device structure
typedef struct {
  device device;
  // Driver
  kernelDriver *driver;
  // Device class-specific structure
  void *data;

} kernelDevice;

// Functions exported from kernelDevice.c
int kernelDeviceInitialize(void);
deviceClass *kernelDeviceGetClass(int);
int kernelDeviceFind(deviceClass *, deviceClass *, kernelDevice *[], int);
int kernelDeviceAdd(kernelDevice *, kernelDevice *);
// These ones are exported outside the kernel
int kernelDeviceTreeGetCount(void);
int kernelDeviceTreeGetRoot(device *);
int kernelDeviceTreeGetChild(device *, device *);
int kernelDeviceTreeGetNext(device *);

#define _KERNELDEVICE_H
#endif
