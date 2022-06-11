//
//  Visopsys
//  Copyright (C) 1998-2004 J. Andrew McLaughlin
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
//  kernelDma.c
//

// This file contains functions for DMA access, and routines for managing
// the installed DMA driver.

#include "kernelDma.h"
#include "kernelError.h"
#include <sys/errors.h>
#include <string.h>


static kernelDma *systemDma = NULL;
static int initialized = 0;


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
//  Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


int kernelDmaRegisterDevice(kernelDma *theDma)
{
  // This routine will register a new Dma structure.  It takes a 
  // kernelDma structure and returns 0 if successful

  int status = 0;

  if (theDma == NULL)
    {
      kernelError(kernel_error, "DMA controller is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  if (theDma->driver == NULL)
    {
      kernelError(kernel_error, "DMA driver is NULL");
      return (status = ERR_NOSUCHDRIVER);
    }

  // If the driver has a 'register device' function, call it
  if (theDma->driver->driverRegisterDevice)
    status = theDma->driver->driverRegisterDevice(theDma);

  // Alright.  We'll save the pointer to the device
  systemDma = theDma;

  // Return success
  return (status);
}


int kernelDmaInitialize(void)
{
  // This function is used to initialize the DMA driver.  It is a generic 
  // routine which calls the specific associated device driver function.

  int status = 0;

  // Check the DMA structure and device driver before proceeding

  // Make sure the DMA structure isn't NULL (which could indicate that the
  // device has not been properly registered)
  if (systemDma == NULL)
    {
      kernelError(kernel_error, "DMA controller is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  initialized = 1;

  return (status);
}


int kernelDmaOpenChannel(int channelNumber, void *address, int count, int mode)
{
  // This function is used to set up a DMA channel and prepare it to 
  // read or write data.  It is a generic routine which calls the 
  // specific associated device driver function.

  int status = 0;

  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Make sure the driver's "open channel" routine has been initialized
  if (systemDma->driver->driverOpenChannel == NULL)
    {
      // Ooops.  Driver function is NULL.  Make an error.
      kernelError(kernel_error, "Driver function is NULL");
      return (status = ERR_NOSUCHFUNCTION);
    }

  status = systemDma->driver
    ->driverOpenChannel(channelNumber, address, count, mode);

  // if status is not equal to zero, we'll assume it's a driver error
  // and register a kernelError

  if (status < 0)
    kernelError(kernel_error, "Error while setting up DMA channel %d for %s",
		channelNumber, (mode = DMA_READMODE)? "read" : "write");

  return (status);
}


int kernelDmaCloseChannel(int channelNumber)
{
  // This function is used to close a DMA channel after the desired
  // "read" or "write" operation has been completed.  It is a generic 
  // routine which calls the specific associated device driver function.

  int status = 0;

  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Make sure the driver's "close channel" routine has been initialized
  if (systemDma->driver->driverCloseChannel == NULL)
    {
      // Ooops.  Driver function is NULL.  Make an error.
      kernelError(kernel_error, "Driver function is NULL");
      return (status = ERR_NOSUCHFUNCTION);
    }

  status = systemDma->driver->driverCloseChannel(channelNumber);

  // if status is not equal to zero, we'll assume it's a driver error
  // and register a kernelError

  if (status < 0)
    kernelError(kernel_error, "Error while closing the DMA channel %d",
		channelNumber);

  return (status);
}
