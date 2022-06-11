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
//  kernelBus.c
//

#include "kernelBus.h"
#include "kernelError.h"
#include "kernelLinkedList.h"
#include "kernelMalloc.h"
#include "kernelMisc.h"
#include <string.h>

static kernelLinkedList buses;
static int initialized = 0;


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
//  Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


int kernelBusRegister(kernelBus *bus)
{
  int status = 0;

  // Check params
  if (bus == NULL)
    return (status = ERR_NULLPARAMETER);

  if (!initialized)
    {
      kernelMemClear(&buses, sizeof(kernelLinkedList));
      initialized = 1;
    }

  // Add the supplied device to our list of buses
  status = kernelLinkedListAdd(&buses, (void *) bus);
  if (status < 0)
    return (status);

  return (status = 0);
}


int kernelBusGetTargets(kernelBusType type, kernelBusTarget **pointer)
{
  // This is a wrapper for the bus-specific driver functions, but it will
  // aggregate a list of targets from all buses of the requested type.

  int status = 0;
  kernelLinkedListItem *iter = NULL;
  kernelBus *bus = NULL;
  kernelBusTarget *tmpTargets = NULL;
  int numTargets = 0;

  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check params
  if (pointer == NULL)
    return (status = ERR_NULLPARAMETER);

  *pointer = NULL;

  // Loop through all our buses and collect all the targets for buses
  // of the requested type
  bus = kernelLinkedListIterStart(&buses, &iter);
  while (bus)
    {
      if (bus->type == type)
	{
	  // Operation supported?
	  if (!bus->ops->driverGetTargets)
	    continue;

	  status = bus->ops->driverGetTargets(bus, &tmpTargets);

	  if (status > 0)
	    {
	      *pointer = kernelRealloc(*pointer, ((numTargets + status) *
						  sizeof(kernelBusTarget)));
	      if (*pointer == NULL)
		return (status = ERR_MEMORY);

	      kernelMemCopy(tmpTargets, &((*pointer)[numTargets]),
			    (status * sizeof(kernelBusTarget)));

	      numTargets += status;
	      kernelFree(tmpTargets);
	    }
	}

      bus = kernelLinkedListIterNext(&buses, &iter);
    }

  return (numTargets);
}


kernelBusTarget *kernelBusGetTarget(kernelBusType type, int id)
{
  // Get the target for the specified type and ID

  int numTargets = 0;
  kernelBusTarget *targets = NULL;
  kernelBusTarget *target = NULL;
  int count;

  numTargets = kernelBusGetTargets(type, &targets);
  if (numTargets <= 0)
    return (target = NULL);

  for (count = 0; count < numTargets; count ++)
    {
      if (targets[count].id == id)
	{
	  target = kernelMalloc(sizeof(kernelBusTarget));
	  if (target == NULL)
	    break;

	  kernelMemCopy(&targets[count], target, sizeof(kernelBusTarget));
	  break;
	}
    }

  kernelFree(targets);
  return (target);
}


int kernelBusGetTargetInfo(kernelBusTarget *target, void *pointer)
{
  // This is a wrapper for the bus-specific driver function

  int status = 0;

  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check params
  if ((target == NULL) || (pointer == NULL))
    return (status = ERR_NULLPARAMETER);

  if (!target->bus)
    return (status = ERR_NODATA);

  // Operation supported?
  if (!target->bus->ops->driverGetTargetInfo)
    {
      kernelError(kernel_error, "Bus type %d doesn't support this function",
		  target->bus->type);
      return (status = ERR_NOSUCHFUNCTION);
    }

  status = target->bus->ops->driverGetTargetInfo(target, pointer);
  return (status);
}


unsigned kernelBusReadRegister(kernelBusTarget *target, int reg, int bitWidth)
{
  // This is a wrapper for the bus-specific driver function

  unsigned contents = 0;

  if (!initialized)
    return (0);

  // Check params
  if ((target == NULL) || (!target->bus))
    return (0);

  // Operation supported?
  if (!target->bus->ops->driverReadRegister)
    {
      kernelError(kernel_error, "Bus type %d doesn't support this function",
		  target->bus->type);
      return (contents = 0);
    }

  contents = target->bus->ops->driverReadRegister(target, reg, bitWidth);
  return (contents);
}


int kernelBusWriteRegister(kernelBusTarget *target, int reg, int bitWidth,
			   unsigned contents)
{
  // This is a wrapper for the bus-specific driver function

  int status = 0;

  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check params
  if (target == NULL)
    return (status = ERR_NULLPARAMETER);
  
  if (!target->bus)
    return (status = ERR_NODATA);

  // Operation supported?
  if (!target->bus->ops->driverWriteRegister)
    {
      kernelError(kernel_error, "Bus type %d doesn't support this function",
		  target->bus->type);
      return (status = ERR_NOSUCHFUNCTION);
    }

  status = target->bus->ops->driverWriteRegister(target, reg, bitWidth,
						 contents);
  return (status);
}


void kernelBusDeviceClaim(kernelBusTarget *target, kernelDriver *driver)
{
  // This is a wrapper for the bus-specific driver function, called by a
  // device driver that wants to lay claim to a specific device.  This is
  // advisory-only.

  if (!initialized)
    return;

  // Check params
  if ((target == NULL) || (driver == NULL))
    return;
  
  // Operation supported?
  if (!target->bus->ops->driverDeviceClaim)
    {
      kernelError(kernel_error, "Bus type %d doesn't support this function",
		  target->bus->type);
      return;
    }

  target->bus->ops->driverDeviceClaim(target, driver);
  return;
}


int kernelBusDeviceEnable(kernelBusTarget *target, int enable)
{
  // This is a wrapper for the bus-specific driver function

  int status = 0;

  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check params
  if (target == NULL)
    return (status = ERR_NULLPARAMETER);
  
  if (!target->bus)
    return (status = ERR_NODATA);

  // Operation supported?
  if (!target->bus->ops->driverDeviceEnable)
    {
      kernelError(kernel_error, "Bus type %d doesn't support this function",
		  target->bus->type);
      return (status = ERR_NOSUCHFUNCTION);
    }

  status = target->bus->ops->driverDeviceEnable(target, enable);
  return (status);
}


int kernelBusSetMaster(kernelBusTarget *target, int master)
{
  // This is a wrapper for the bus-specific driver function

  int status = 0;

  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check params
  if (target == NULL)
    return (status = ERR_NULLPARAMETER);
  
  if (!target->bus)
    return (status = ERR_NODATA);

  // Operation supported?
  if (!target->bus->ops->driverSetMaster)
    {
      kernelError(kernel_error, "Bus type %d doesn't support this function",
		  target->bus->type);
      return (status = ERR_NOSUCHFUNCTION);
    }

  status = target->bus->ops->driverSetMaster(target, master);
  return (status);
}


int kernelBusRead(kernelBusTarget *target, unsigned size, void *buffer)
{
  // This is a wrapper for the bus-specific driver function

  int status = 0;

  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check params
  if ((target == NULL) || (buffer == NULL))
    return (status = ERR_NULLPARAMETER);

  if (!target->bus)
    return (status = ERR_NODATA);

  // Operation supported?
  if (!target->bus->ops->driverRead)
    {
      kernelError(kernel_error, "Bus type %d doesn't support this function",
		  target->bus->type);
      return (status = ERR_NOSUCHFUNCTION);
    }

  return (target->bus->ops->driverRead(target, size, buffer));
}


int kernelBusWrite(kernelBusTarget *target, unsigned size, void *buffer)
{
  // This is a wrapper for the bus-specific driver function

  int status = 0;

  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check params
  if ((target == NULL) || (buffer == NULL))
    return (status = ERR_NULLPARAMETER);

  if (!target->bus)
    return (status = ERR_NODATA);

  // Operation supported?
  if (!target->bus->ops->driverWrite)
    {
      kernelError(kernel_error, "Bus type %d doesn't support this function",
		  target->bus->type);
      return (status = ERR_NOSUCHFUNCTION);
    }

  return (target->bus->ops->driverWrite(target, size, buffer));
}

