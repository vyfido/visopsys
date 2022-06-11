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
//  kernelSystemDriver.c
//

// This is a place for putting basic, generic driver initializations, including
// the one for the 'system device' itself, and any other abstract things that
// have no real hardware driver, per se.

#include "kernelSystemDriver.h"
#include "kernelDevice.h"
#include "kernelDriver.h"
#include "kernelLog.h"
#include "kernelMain.h"
#include "kernelMalloc.h"
#include "kernelMisc.h"
#include "kernelPage.h"
#include "kernelParameters.h"
#include "kernelProcessorX86.h"
#include "kernelVariableList.h"
#include <stdio.h>
#include <string.h>


static kernelDevice *regDevice(void *parent, void *driver,
			       kernelDeviceClass *class,
			       kernelDeviceClass *subClass)
{
  // Just collects some of the common things from the other detect routines

  int status = 0;
  kernelDevice *dev = NULL;

  // Allocate memory for the device
  dev = kernelMalloc(sizeof(kernelDevice));
  if (dev == NULL)
    return (dev);

  dev->device.class = class;
  dev->device.subClass = subClass;
  dev->driver = driver;

  status = kernelDeviceAdd(parent, dev);
  if (status < 0)
    {
      kernelFree(dev);
      return (dev = NULL);
    }

  return (dev);
}


static int driverDetectMemory(void *parent, kernelDriver *driver)
{
  int status = 0;
  kernelDevice *dev = NULL;
  char value[80];

  dev = regDevice(parent, driver, kernelDeviceGetClass(DEVICECLASS_MEMORY), 0);
  if (dev == NULL)
    return (status = ERR_NOCREATE);

  // Initialize the variable list for attributes of the CPU
  status = kernelVariableListCreate(&dev->device.attrs);
  if (status < 0)
    return (status);

  sprintf(value, "%u Kb", (1024 + kernelOsLoaderInfo->extendedMemory));
  kernelVariableListSet(&dev->device.attrs, "memory.size", value);

  return (status = 0);
}


static int driverDetectBios(void *parent, kernelDriver *driver)
{
  int status = 0;
  void *biosArea = NULL;
  char *ptr = NULL;
  kernelBiosHeader *dataStruct = NULL;
  char checkSum = 0;
  kernelDevice *dev = NULL;
  int count;

  // Map the designated area for the BIOS into memory so we can scan it.
  status = kernelPageMapToFree(KERNELPROCID, (void *) BIOSAREA_START,
			       &biosArea, BIOSAREA_SIZE);
  if (status < 0)
    return (status = 0);

  for (ptr = biosArea ;
       ptr <= (char *) (biosArea + BIOSAREA_SIZE - sizeof(kernelBiosHeader));
       ptr += sizeof(kernelBiosHeader))
    {
      if (!strncmp(ptr, BIOSAREA_SIG32, strlen(BIOSAREA_SIG32)))
	{
	  dataStruct = (kernelBiosHeader *) ptr;
	  break;
	}
    }

  if (!dataStruct)
    goto out;

  // Check the checksum (signed chars, should sum to zero)
  for (count = 0; count < (int) sizeof(kernelBiosHeader); count ++)
    checkSum += ptr[count];
  if (checkSum)
    {
      kernelLog("32-bit BIOS checksum failed (%d)", checkSum);
      goto out;
    }

  kernelLog("32-bit BIOS found at %p, entry point %p",
	    (void *)(BIOSAREA_START + ((void *) dataStruct - biosArea)),
	    dataStruct->entryPoint);

  // Allocate memory for the device
  dev = kernelMalloc(sizeof(kernelDevice));
  if (dev == NULL)
    goto out;

  dev->device.class = kernelDeviceGetClass(DEVICESUBCLASS_SYSTEM_BIOS32);
  dev->driver = driver;

  // Allocate memory for driver data
  dev->data = kernelMalloc(sizeof(kernelBiosHeader));
  if (dev->data == NULL)
    goto out;

  // Copy the data we found into the driver's data structure
  kernelMemCopy(dataStruct, dev->data, sizeof(kernelBiosHeader));

  // Register the device
  status = kernelDeviceAdd(parent, dev);
  if (status < 0)
    goto out;

 out:
  kernelPageUnmap(KERNELPROCID, biosArea, BIOSAREA_SIZE);
  return (status = 0);
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


void kernelMemoryDriverRegister(kernelDriver *driver)
{
  // Device driver registration.
  driver->driverDetect = driverDetectMemory;
  return;
}


void kernelBiosDriverRegister(kernelDriver *driver)
{
  // Device driver registration.
  driver->driverDetect = driverDetectBios;
  return;
}
