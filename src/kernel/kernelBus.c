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
//  kernelBus.c
//

#include "kernelBus.h"
#include "kernelError.h"
#include "kernelMalloc.h"
#include "kernelMisc.h"
#include <string.h>


static kernelBus *buses[BUS_MAX_BUSES];
static int numBuses = 0;


static inline kernelBus *findBus(kernelBusType type)
{
  // Search through our list of buses to find the first one of the correct
  // type

  kernelBus *bus = NULL;
  int count;

  for (count = 0; count < numBuses; count ++)
    {
      if (buses[count]->type == type)
	{
	  bus = buses[count];
	  break;
	}
    }

  return (bus);
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
//  Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


int kernelBusRegister(kernelBusType type, kernelDevice *dev)
{
  int status = 0;
  kernelBus *bus = NULL;

  // Check params
  if (dev == NULL)
    return (status = ERR_NULLPARAMETER);

  if (numBuses >= BUS_MAX_BUSES)
    {
      kernelError(kernel_error, "Max buses (%d) has been reached", numBuses);
      return (status = ERR_NOFREE);
    }

  // Get memory for the bus
  bus = kernelMalloc(sizeof(kernelBus));
  if (bus == NULL)
    return (status = ERR_MEMORY);

  bus->type = type;
  bus->ops = dev->driver->ops;

  // Add the supplied device to our list of buses
  buses[numBuses] = bus;
  numBuses += 1;

  return (status = 0);
}


int kernelBusGetTargets(kernelBusType type, kernelBusTarget **pointer)
{
  // This is a wrapper for the bus-specific driver function

  int status = 0;
  kernelBus *bus = NULL;
  kernelBusTarget *tmpTargets = NULL;
  kernelBusTarget *currentTargets = NULL;
  int numTargets = 0;
  int count;

  // Check params
  if (pointer == NULL)
    return (status = ERR_NULLPARAMETER);

  // Loop through all our buses and count the number of targets for buses
  // of this type
  for (count = 0; count < numBuses; count ++)
    {
      bus = buses[count];
      
      if (bus->type == type)
	{
	  // Operation supported?
	  if (!bus->ops->driverGetTargets)
	    {
	      kernelError(kernel_error, "Bus type %d doesn't support this "
			  "function", type);
	      return (status = ERR_NOSUCHFUNCTION);
	    }
      
	  status = bus->ops->driverGetTargets(&tmpTargets);
	  if (status < 0)
	    return (status);

	  numTargets += status;
	}
    }

  if (numTargets <= 0)
    return (status = ERR_NODATA);

  // Allocate enough memory for all the targets
  *pointer = kernelMalloc(numTargets * sizeof(kernelBusTarget));
  if (*pointer == NULL)
    return (status = ERR_MEMORY);

  // Loop through the buses again and copy the targets into our new memory
  numTargets = 0;
  for (count = 0; count < numBuses; count ++)
    {
      bus = buses[count];
      
      if (bus->type == type)
	{
	  status = bus->ops->driverGetTargets(&tmpTargets);
	  if (status < 0)
	    return (status);

	  currentTargets = (*pointer + (numTargets * sizeof(kernelBusTarget)));
	  
	  kernelMemCopy(tmpTargets, currentTargets,
			(status * sizeof(kernelBusTarget)));
	  numTargets += status;
	}
    }

  return (numTargets);
}


int kernelBusGetTargetInfo(kernelBusType type, int target, void *pointer)
{
  // This is a wrapper for the bus-specific driver function

  int status = 0;
  kernelBus *bus = NULL;

  // Check params
  if (pointer == NULL)
    return (status = ERR_NULLPARAMETER);

  bus = findBus(type);
  if (bus == NULL)
    {
      kernelError(kernel_error, "No such bus type %d for target %d", type,
		  target);
      return (status = ERR_NOSUCHENTRY);
    }

  // Operation supported?
  if (!bus->ops->driverGetTargetInfo)
    {
      kernelError(kernel_error, "Bus type %d doesn't support this function",
		  type);
      return (status = ERR_NOSUCHFUNCTION);
    }

  status = bus->ops->driverGetTargetInfo(target, pointer);
  return (status);
}


unsigned kernelBusReadRegister(kernelBusType type, int target, int reg,
			       int bitWidth)
{
  // This is a wrapper for the bus-specific driver function

  unsigned contents = 0;
  kernelBus *bus = NULL;

  bus = findBus(type);
  if (bus == NULL)
    {
      kernelError(kernel_error, "No such bus type %d", type);
      return (contents = 0);
    }

  // Operation supported?
  if (!bus->ops->driverReadRegister)
    {
      kernelError(kernel_error, "Bus type %d doesn't support this function",
		  type);
      return (contents = 0);
    }

  contents = bus->ops->driverReadRegister(target, reg, bitWidth);
  return (contents);
}


void kernelBusWriteRegister(kernelBusType type, int target, int reg,
			    int bitWidth, unsigned contents)
{
  // This is a wrapper for the bus-specific driver function
  kernelBus *bus = NULL;

  bus = findBus(type);
  if (bus == NULL)
    {
      kernelError(kernel_error, "No such bus type %d", type);
      return;
    }

  // Operation supported?
  if (!bus->ops->driverWriteRegister)
    {
      kernelError(kernel_error, "Bus type %d doesn't support this function",
		  type);
      return;
    }

  bus->ops->driverWriteRegister(target, reg, bitWidth, contents);
  return;
}


int kernelBusDeviceEnable(kernelBusType type, int target, int enable)
{
  // This is a wrapper for the bus-specific driver function

  int status = 0;
  kernelBus *bus = NULL;

  bus = findBus(type);
  if (bus == NULL)
    {
      kernelError(kernel_error, "No such bus type %d", type);
      return (status = ERR_NOSUCHENTRY);
    }

  // Operation supported?
  if (!bus->ops->driverDeviceEnable)
    {
      kernelError(kernel_error, "Bus type %d doesn't support this function",
		  type);
      return (status = ERR_NOSUCHFUNCTION);
    }

  status = bus->ops->driverDeviceEnable(target, enable);
  return (status);
}


int kernelBusSetMaster(kernelBusType type, int target, int master)
{
  // This is a wrapper for the bus-specific driver function

  int status = 0;
  kernelBus *bus = NULL;

  bus = findBus(type);
  if (bus == NULL)
    {
      kernelError(kernel_error, "No such bus type %d", type);
      return (status = ERR_NOSUCHENTRY);
    }

  // Operation supported?
  if (!bus->ops->driverSetMaster)
    {
      kernelError(kernel_error, "Bus type %d doesn't support this function",
		  type);
      return (status = ERR_NOSUCHFUNCTION);
    }

  status = bus->ops->driverSetMaster(target, master);
  return (status);
}


int kernelBusRead(kernelBusType type, int target, void *buffer)
{
  // This is a wrapper for the bus-specific driver function

  int status = 0;
  kernelBus *bus = NULL;

  // Check params
  if (buffer == NULL)
    return (status = ERR_NULLPARAMETER);

  bus = findBus(type);
  if (bus == NULL)
    {
      kernelError(kernel_error, "No such bus type %d", type);
      return (status = ERR_NOSUCHENTRY);
    }

  // Operation supported?
  if (!bus->ops->driverRead)
    {
      kernelError(kernel_error, "Bus type %d doesn't support this function",
		  type);
      return (status = ERR_NOSUCHFUNCTION);
    }

  status = bus->ops->driverRead(target, buffer);
  return (status);
}


int kernelBusWrite(kernelBusType type, int target, void *buffer)
{
  // This is a wrapper for the bus-specific driver function

  int status = 0;
  kernelBus *bus = NULL;

  // Check params
  if (buffer == NULL)
    return (status = ERR_NULLPARAMETER);

  bus = findBus(type);
  if (bus == NULL)
    {
      kernelError(kernel_error, "No such bus type %d", type);
      return (status = ERR_NOSUCHENTRY);
    }

  // Operation supported?
  if (!bus->ops->driverWrite)
    {
      kernelError(kernel_error, "Bus type %d doesn't support this function",
		  type);
      return (status = ERR_NOSUCHFUNCTION);
    }

  status = bus->ops->driverWrite(target, buffer);
  return (status);
}

