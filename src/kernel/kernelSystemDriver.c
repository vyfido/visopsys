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
//  kernelSystemDriver.c
//

// This is a place for putting basic, generic driver initializations, including
// the one for the 'system device' itself, and any other abstract things that
// have no real hardware driver, per se.

#include "kernelDevice.h"
#include "kernelDriver.h"
#include "kernelMalloc.h"
#include <string.h>


static int driverDetect(void *driver, deviceClass *class,
			deviceClass *subClass)
{
  // Just collects some of the common things from the other detect routines

  int status = 0;
  kernelDevice *dev = NULL;

  // Allocate memory for the device
  dev = kernelMalloc(sizeof(kernelDevice));
  if (dev == NULL)
    return (status = 0);

  dev->device.class = class;
  dev->device.subClass = subClass;
  dev->driver = driver;

  return (status = kernelDeviceAdd(NULL, dev));
}


static int driverDetectCpu(void *driver)
{
  return (driverDetect(driver, kernelDeviceGetClass(DEVICECLASS_CPU), 0));
}


static int driverDetectMemory(void *driver)
{
  return (driverDetect(driver, kernelDeviceGetClass(DEVICECLASS_MEMORY), 0));
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


void kernelCpuDriverRegister(void *driverData)
{
   // Device driver registration.
  ((kernelDriver *) driverData)->driverDetect = driverDetectCpu;
  return;
}


void kernelMemoryDriverRegister(void *driverData)
{
   // Device driver registration.
  ((kernelDriver *) driverData)->driverDetect = driverDetectMemory;
  return;
}
