//
//  Visopsys
//  Copyright (C) 1998-2013 J. Andrew McLaughlin
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
//  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
//
//  kernelSystemDriver.c
//

// This is a place for putting basic, generic driver initializations, including
// the one for the 'system device' itself, and any other abstract things that
// have no real hardware driver, per se.

#include "kernelSystemDriver.h"
#include "kernelBus.h"
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
	 kernelDeviceClass *class, kernelDeviceClass *subClass)
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

	// Register the device
	dev = regDevice(parent, driver, kernelDeviceGetClass(DEVICECLASS_MEMORY),
		NULL);
	if (dev == NULL)
		return (status = ERR_NOCREATE);

	// Initialize the variable list for attributes of the memory
	status = kernelVariableListCreate(&dev->device.attrs);
	if (status < 0)
		return (status);

	sprintf(value, "%u Kb", (1024 + kernelOsLoaderInfo->extendedMemory));
	kernelVariableListSet(&dev->device.attrs, "memory.size", value);

	return (status = 0);
}


static int driverDetectBios32(void *parent, kernelDriver *driver)
{
	// Detect a 32-bit BIOS interface

	int status = 0;
	void *biosArea = NULL;
	char *ptr = NULL;
	kernelBios32Header *dataStruct = NULL;
	char checkSum = 0;
	kernelDevice *dev = NULL;
	int count;

	// Map the designated area for the BIOS into memory so we can scan it.
	status = kernelPageMapToFree(KERNELPROCID, (void *) BIOSAREA_START,
		&biosArea, BIOSAREA_SIZE);
	if (status < 0)
		goto out;

	// Search for our signature
	for (ptr = biosArea; ptr <= (char *) (biosArea + BIOSAREA_SIZE -
		sizeof(kernelBios32Header)); ptr += sizeof(kernelBios32Header))
	{
		if (!strncmp(ptr, BIOSAREA_SIG_32, strlen(BIOSAREA_SIG_32)))
		{
			dataStruct = (kernelBios32Header *) ptr;
			break;
		}
	}

	if (!dataStruct)
		// Not found
		goto out;

	// Check the checksum (signed chars, should sum to zero)
	for (count = 0; count < (int) sizeof(kernelBios32Header); count ++)
		checkSum += ptr[count];
	if (checkSum)
	{
		kernelLog("32-bit BIOS checksum failed (%d)", checkSum);
		goto out;
	}

	kernelLog("32-bit BIOS found at %p, entry point %p",
		(void *)(BIOSAREA_START + ((void *) dataStruct - biosArea)),
		dataStruct->entryPoint);

	// Register the device
	dev = regDevice(parent, driver,
		kernelDeviceGetClass(DEVICESUBCLASS_SYSTEM_BIOS32), NULL);
	if (dev == NULL)
	{
		status = ERR_NOCREATE;
		goto out;
	}

	// Allocate memory for driver data
	dev->data = kernelMalloc(sizeof(kernelBios32Header));
	if (dev->data == NULL)
	{
		status = ERR_MEMORY;
		goto out;
	}
	
	// Copy the data we found into the driver's data structure
	kernelMemCopy(dataStruct, dev->data, sizeof(kernelBios32Header));

	status = 0;

out:
	if (status < 0)
	{
		if (dev && dev->data)
			kernelFree(dev->data);
	}

	if (biosArea)
		kernelPageUnmap(KERNELPROCID, biosArea, BIOSAREA_SIZE);

	return (status);
}


static int driverDetectBiosPnP(void *parent, kernelDriver *driver)
{
	// Detect a Plug and Play BIOS

	int status = 0;
	void *biosArea = NULL;
	char *ptr = NULL;
	kernelBiosPnpHeader *dataStruct = NULL;
	char checkSum = 0;
	kernelDevice *dev = NULL;
	char value[80];
	int count;

	// Map the designated area for the BIOS into memory so we can scan it.
	status = kernelPageMapToFree(KERNELPROCID, (void *) BIOSAREA_START,
		&biosArea, BIOSAREA_SIZE);
	if (status < 0)
		goto out;

	// Search for our signature
	for (ptr = biosArea; ptr <= (char *) (biosArea + BIOSAREA_SIZE -
		sizeof(kernelBiosPnpHeader)); ptr += 16)
	{
		if (!strncmp(ptr, BIOSAREA_SIG_PNP, strlen(BIOSAREA_SIG_PNP)))
		{
			dataStruct = (kernelBiosPnpHeader *) ptr;
			break;
		}
	}

	if (!dataStruct)
		// Not found
		goto out;

	// Check the checksum (signed chars, should sum to zero)
	for (count = 0; count < (int) sizeof(kernelBiosPnpHeader); count ++)
		checkSum += ptr[count];
	if (checkSum)
	{
		kernelLog("Plug and Play BIOS checksum failed (%d)", checkSum);
		goto out;
	}

	kernelLog("Plug and Play BIOS found at %p",
		(void *)(BIOSAREA_START + ((void *) dataStruct - biosArea)));

	// Register the device
	dev = regDevice(parent, driver,
		kernelDeviceGetClass(DEVICESUBCLASS_SYSTEM_BIOSPNP), NULL);
	if (dev == NULL)
	{
		status = ERR_NOCREATE;
		goto out;
	}

	// Initialize the variable list for attributes of the memory
	status = kernelVariableListCreate(&dev->device.attrs);
	if (status < 0)
		goto out;

	sprintf(value, "%d.%d", ((dataStruct->version & 0xF0) >> 4),
		(dataStruct->version & 0x0F));
	kernelVariableListSet(&dev->device.attrs, "pnp.version", value);

	// Allocate memory for driver data
	dev->data = kernelMalloc(sizeof(kernelBiosPnpHeader));
	if (dev->data == NULL)
	{
		status = ERR_MEMORY;
		goto out;
	}
	
	// Copy the data we found into the driver's data structure
	kernelMemCopy(dataStruct, dev->data, sizeof(kernelBiosPnpHeader));

	status = 0;

out:
	if (status < 0)
	{
		if (dev)
		{
			if (dev->data)
				kernelFree(dev->data);

			kernelVariableListDestroy(&dev->device.attrs);
		}
	}

	if (biosArea)
		kernelPageUnmap(KERNELPROCID, biosArea, BIOSAREA_SIZE);

	return (status);
}


static int driverDetectIsaBridge(void *parent __attribute__((unused)),
	kernelDriver *driver)
{
	// Detect an ISA bridge

	int status = 0;
	kernelBusTarget *busTargets = NULL;
	int numBusTargets = 0;
	kernelDevice *dev = NULL;
	int deviceCount;

	// Search the PCI bus(es) for devices
	numBusTargets = kernelBusGetTargets(bus_pci, &busTargets);
	if (numBusTargets <= 0)
		return (status = ERR_NODATA);

	// Search the bus targets for an PCI-to-ISA bridge device.
	for (deviceCount = 0; deviceCount < numBusTargets; deviceCount ++)
	{
		// If it's not a PCI-to-ISA bridge device, skip it
		if ((busTargets[deviceCount].class == NULL) ||
			(busTargets[deviceCount].class->class != DEVICECLASS_BRIDGE) ||
			(busTargets[deviceCount].subClass == NULL) ||
			(busTargets[deviceCount].subClass->class !=
				DEVICESUBCLASS_BRIDGE_ISA))
		{
			continue;
		}

		kernelLog("Found PCI/ISA bridge");

		// After this point, we know we have a supported device.

		dev = regDevice(busTargets[deviceCount].bus->dev, driver,
			kernelDeviceGetClass(DEVICECLASS_BRIDGE),
			kernelDeviceGetClass(DEVICESUBCLASS_BRIDGE_ISA));
		if (dev == NULL)
			return (status = ERR_NOCREATE);
	}

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


void kernelBios32DriverRegister(kernelDriver *driver)
{
	// Device driver registration.
	driver->driverDetect = driverDetectBios32;
	return;
}


void kernelBiosPnpDriverRegister(kernelDriver *driver)
{
	// Device driver registration.
	driver->driverDetect = driverDetectBiosPnP;
	return;
}


void kernelIsaBridgeDriverRegister(kernelDriver *driver)
{
	// Device driver registration.
	driver->driverDetect = driverDetectIsaBridge;
	return;
}
