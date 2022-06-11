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
#include <stdio.h>
#include <string.h>


static struct {
  char *string;
  char *vendor;
} cpuVendorIds[] = {
  { "GenuineIntel", "Intel" },
  { "UMC UMC UMC ", "United Microelectronics" },
  { "AuthenticAMD", "AMD" },
  { "AMD ISBETTER", "AMD" },
  { "CyrixInstead", "Cyrix" },
  { "NexGenDriven", "NexGen" },
  { "CentaurHauls", "IDT/Centaur/VIA" },
  { "RiseRiseRise", "Rise" },
  { "GenuineTMx86", "Transmeta" },
  { NULL, NULL }
};


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


static int driverDetectCpu(void *parent, kernelDriver *driver)
{
  int status = 0;
  unsigned cpuIdLimit = 0;
  char vendorString[13];
  unsigned rega = 0, regb = 0, regc = 0, regd = 0;
  kernelDevice *dev = NULL;
  char variable[80];
  char value[80];
  unsigned count;

  dev = regDevice(parent, driver, kernelDeviceGetClass(DEVICECLASS_CPU),
		  kernelDeviceGetClass(DEVICESUBCLASS_CPU_X86));
  if (dev == NULL)
    return (status = ERR_NOCREATE);

  // Initialize the variable list for attributes of the CPU
  status = kernelVariableListCreate(&dev->device.attrs);
  if (status < 0)
    return (status);

  // Try to identify the CPU

  // The initial call gives us the vendor string and tells us how many other
  // functions are supported

  kernelProcessorId(0, rega, regb, regc, regd);

  cpuIdLimit = (rega & 0x7FFFFFFF);
  ((unsigned *) vendorString)[0] = regb;
  ((unsigned *) vendorString)[1] = regd;
  ((unsigned *) vendorString)[2] = regc;
  vendorString[12] = '\0';

  // Try to identify the chip vendor by name
  kernelVariableListSet(&dev->device.attrs, DEVICEATTRNAME_VENDOR,
			"unknown");
  for (count = 0; cpuVendorIds[count].string; count ++)
    {
      if (!strncmp(vendorString, cpuVendorIds[count].string, 12))
	{
	  kernelVariableListSet(&dev->device.attrs, DEVICEATTRNAME_VENDOR,
				cpuVendorIds[count].vendor);
	  break;
	}
    }
  kernelVariableListSet(&dev->device.attrs, "vendor.string", vendorString);

  // Do additional supported functions

  // If supported, the second call gives us a bunch of binary flags telling
  // us about the capabilities of the chip
  if (cpuIdLimit >= 1)
    {
      kernelProcessorId(1, rega, regb, regc, regd);

      // CPU type
      sprintf(variable, "%s.%s", "cpu", "type");
      sprintf(value, "%02x", ((rega & 0xF000) >> 12));
      kernelVariableListSet(&dev->device.attrs, variable, value);

      // CPU family
      sprintf(variable, "%s.%s", "cpu", "family");
      sprintf(value, "%02x", ((rega & 0xF00) >> 8));
      kernelVariableListSet(&dev->device.attrs, variable, value);

      // CPU model
      sprintf(variable, "%s.%s", "cpu", "model");
      sprintf(value, "%02x", ((rega & 0xF0) >> 4));
      kernelVariableListSet(&dev->device.attrs, variable, value);

      // CPU revision
      sprintf(variable, "%s.%s", "cpu", "rev");
      sprintf(value, "%02x", (rega & 0xF));
      kernelVariableListSet(&dev->device.attrs, variable, value);

      // CPU features
      sprintf(variable, "%s.%s", "cpu", "features");
      sprintf(value, "%08x", regd);
      kernelVariableListSet(&dev->device.attrs, variable, value);
    }

  // See if there's extended CPUID info
  kernelProcessorId(0x80000000, cpuIdLimit, regb, regc, regd);

  if (cpuIdLimit & 0x80000000)
    {     
      if (cpuIdLimit >= 0x80000004)
	{
	  // Get the product string
	  kernelProcessorId(0x80000002, rega, regb, regc, regd);
	  ((unsigned *) value)[0] = rega;
	  ((unsigned *) value)[1] = regb;
	  ((unsigned *) value)[2] = regc;
	  ((unsigned *) value)[3] = regd;
	  kernelProcessorId(0x80000003, rega, regb, regc, regd);
	  ((unsigned *) value)[4] = rega;
	  ((unsigned *) value)[5] = regb;
	  ((unsigned *) value)[6] = regc;
	  ((unsigned *) value)[7] = regd;
	  kernelProcessorId(0x80000004, rega, regb, regc, regd);
	  ((unsigned *) value)[8] = rega;
	  ((unsigned *) value)[9] = regb;
	  ((unsigned *) value)[10] = regc;
	  ((unsigned *) value)[11] = regd;
	  value[48] = '\0';
	  kernelVariableListSet(&dev->device.attrs, DEVICEATTRNAME_MODEL,
				value);
	}
    }

  return (status = 0);
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
  kernelBios *dataStruct = NULL;
  char checkSum = 0;
  kernelDevice *dev = NULL;
  int count;

  // Map the designated area for the BIOS into memory so we can scan it.
  status = kernelPageMapToFree(KERNELPROCID, (void *) BIOSAREA_START,
			       &biosArea, BIOSAREA_SIZE);
  if (status < 0)
    return (status = 0);

  for (ptr = biosArea ; ptr < (char *) (biosArea + BIOSAREA_SIZE);
       ptr += sizeof(kernelBios))
    {
      if (!strncmp(ptr, BIOSAREA_SIG, 4))
	{
	  dataStruct = (kernelBios *) ptr;
	  break;
	}
    }

  if (!dataStruct)
    goto out;

  // Check the checksum
  for (count = 0; count < (int) sizeof(kernelBios); count ++)
    checkSum += ptr[count];
  if (checkSum)
    kernelLog("32-bit BIOS checksum failed (%d)", checkSum);

  kernelLog("32-bit BIOS found at %08x, entry point %08x",
	    (unsigned) (BIOSAREA_START + ((void *) dataStruct - biosArea)),
	    (unsigned) dataStruct->entryPoint);

  // Allocate memory for the device
  dev = kernelMalloc(sizeof(kernelDevice));
  if (dev == NULL)
    goto out;

  dev->device.class = kernelDeviceGetClass(DEVICESUBCLASS_SYSTEM_BIOS32);
  dev->driver = driver;

  // Allocate memory for driver data
  dev->data = kernelMalloc(sizeof(kernelBios));
  if (dev->data == NULL)
    goto out;

  // Copy the data we found into the driver's data structure
  kernelMemCopy(dataStruct, dev->data, sizeof(kernelBios));

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


void kernelCpuDriverRegister(kernelDriver *driver)
{
  // Device driver registration.
  driver->driverDetect = driverDetectCpu;
  return;
}


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
