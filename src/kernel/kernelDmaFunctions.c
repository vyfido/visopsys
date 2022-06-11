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
//  kernelDmaFunctions.c
//

// This file contains boilerplate functions for DMA access, and routines
// for managing the installed DMA driver.

#include "kernelDmaFunctions.h"
#include "kernelError.h"
#include <sys/errors.h>
#include <string.h>


static kernelDmaObject *kernelDma = NULL;


static int checkObjectAndDriver(char *invokedBy)
{
  // This routine consists of a couple of things commonly done by the
  // other driver wrapper routines.  I've done this to minimize the amount
  // of duplicated code.  As its argument, it takes the name of the 
  // routine that called it so that any error messages can better reflect
  // what was really being done when the error occurred.

  int status = 0;

  // Make sure the DMA object isn't NULL (which could indicate that the
  // device has not been properly registered)
  if (kernelDma == NULL)
    {
      // Make the error
      kernelError(kernel_error, "DMA controller is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  // Make sure that the device has a non-NULL driver
  if (kernelDma->deviceDriver == NULL)
    {
      // Make the error
      kernelError(kernel_error, "DMA driver has not been installed");
      return (status = ERR_NOSUCHDRIVER);
    }

  // Return success
  return (status = 0);
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
//  Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


int kernelDmaRegisterDevice(kernelDmaObject *theDma)
{
  // This routine will register a new Dma object.  It takes a 
  // kernelDmaObject structure and returns 0 if successful.  It returns 
  // negative if the device structure is NULL.

  int status = 0;

  if (theDma == NULL)
    {
      // Make the error
      kernelError(kernel_error, "DMA controller is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  // Alright.  We'll save the pointer to the device
  kernelDma = theDma;

  // Return success
  return (status = 0);
}


int kernelDmaFunctionsInstallDriver(kernelDmaDeviceDriver *theDriver)
{
  // This function is used to register a DMA driver with these routines
  // for future use.

  int status = 0;

  // Make sure the DMA controller object is non-NULL
  if (kernelDma == NULL)
    {
      // Ooops.  It's NULL.  Make an error.
      kernelError(kernel_error, "DMA controller is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  // Make sure the driver structure itself is non-NULL
  if (theDriver == NULL)
    {
      // Ooops.  It's NULL.  Make an error.
      kernelError(kernel_error, "DMA driver has not been installed");
      return (status = ERR_NOSUCHDRIVER);
    }

  // OK, install the driver
  kernelDma->deviceDriver = theDriver;

  // Return success
  return (status = 0);
}


int kernelDmaInitialize(void)
{
  // This function is used to initialize the DMA driver.  It is a generic 
  // routine which calls the specific associated device driver function 
  // installed with the kernelDmaInstallDriver function.

  int status = 0;

  // Check the DMA object and device driver before proceeding
  status = checkObjectAndDriver(__FUNCTION__);
  if (status < 0)
    // Something went wrong, so we can't continue
    return (status);

  // Make sure the driver's initialize routine has been initialized :-)
  if (kernelDma->deviceDriver->driverInitialize == NULL)
    {
      // Ooops.  Driver function is NULL.  Make an error.
      kernelError(kernel_error, "Driver function is NULL");
      return (status = ERR_NOSUCHFUNCTION);
    }

  status = kernelDma->deviceDriver->driverInitialize();

  // if status is not equal to zero, we'll assume it's a driver error
  // and register a kernelError

  if (status < 0)
    {
      // Make the error
      kernelError(kernel_error, "DMA driver initialization failed");
    }

  return (status);
}


int kernelDmaFunctionsEnableChannel(int channelNumber)
{
  // This function is used to open a DMA channel beforer the desired
  // "read" or "write" operation has been started.  It is a generic 
  // routine which calls the specific associated device driver function 
  // installed with the kernelDmaInstallDriver function.

  int status = 0;

  // Check the DMA object and device driver before proceeding
  status = checkObjectAndDriver(__FUNCTION__);
  if (status < 0)
    // Something went wrong, so we can't continue
    return (status);

  // Make sure the driver's "enable channel" routine has been initialized
  if (kernelDma->deviceDriver->driverEnableChannel == NULL)
    {
      // Ooops.  Driver function is NULL.  Make an error.
      kernelError(kernel_error, "Driver function is NULL");
      return (status = ERR_NOSUCHFUNCTION);
    }

  status = kernelDma->deviceDriver->driverEnableChannel(channelNumber);

  // if status is not equal to zero, we'll assume it's a driver error
  // and register a kernelError

  if (status < 0)
    {
      // Make the error
      kernelError(kernel_error, "Error while enabling the DMA channel");
    }

  return (status);
}


int kernelDmaFunctionsCloseChannel(int channelNumber)
{
  // This function is used to close a DMA channel after the desired
  // "read" or "write" operation has been completed.  It is a generic 
  // routine which calls the specific associated device driver function 
  // installed with the kernelDmaInstallDriver function.

  int status = 0;

  // Check the DMA object and device driver before proceeding
  status = checkObjectAndDriver(__FUNCTION__);
  if (status < 0)
    // Something went wrong, so we can't continue
    return (status);

  // Make sure the driver's "close channel" routine has been initialized
  if (kernelDma->deviceDriver->driverCloseChannel == NULL)
    {
      // Ooops.  Driver function is NULL.  Make an error.
      kernelError(kernel_error, "Driver function is NULL");
      return (status = ERR_NOSUCHFUNCTION);
    }

  status = kernelDma->deviceDriver->driverCloseChannel(channelNumber);

  // if status is not equal to zero, we'll assume it's a driver error
  // and register a kernelError

  if (status < 0)
    {
      // Make the error
      kernelError(kernel_error, "Error while closing the DMA channel");
    }

  return (status);
}


int kernelDmaFunctionsSetupChannel(int channelNumber, void *address, 
				   int baseAndCurrentCount)
{
  // This function is used to set up a DMA channel and prepare it to 
  // read or write data.  It is a generic routine which calls the 
  // specific associated device driver function installed with the
  // kernelDmaInstallDriver function.

  int status = 0;
  int baseAndCurrentAddress = 0 ;
  int pageRegister = 0;

  // Check the DMA object and device driver before proceeding
  status = checkObjectAndDriver(__FUNCTION__);
  if (status < 0)
    // Something went wrong, so we can't continue
    return (status);

  // Make sure the driver's "setup channel" routine has been initialized
  if (kernelDma->deviceDriver->driverSetupChannel == NULL)
    {
      // Ooops.  Driver function is NULL.  Make an error.
      kernelError(kernel_error, "Driver function is NULL");
      return (status = ERR_NOSUCHFUNCTION);
    }

  // Convert the "address" argument we were passed into a base address
  // and page register
  pageRegister = (int) ((unsigned) address >> 16);
  baseAndCurrentAddress = (int) ((unsigned) address - 
				 ((unsigned) pageRegister << 16));

  status = kernelDma->deviceDriver->driverSetupChannel(channelNumber, 
		baseAndCurrentAddress, baseAndCurrentCount, pageRegister);

  // if status is not equal to zero, we'll assume it's a driver error
  // and register a kernelError

  if (status < 0)
    {
      // Make the error
      kernelError(kernel_error, "Error while setting up DMA channel");
    }

  return (status);
}


int kernelDmaFunctionsReadData(int channelNumber, int mode)
{
  // This function is used to instigate a read operation FROM memory TO
  // the associated device.  It is a generic routine which calls the 
  // specific associated device driver function installed with the
  // kernelDmaInstallDriver function.

  int status = 0;

  // Check the DMA object and device driver before proceeding
  status = checkObjectAndDriver(__FUNCTION__);
  if (status < 0)
    // Something went wrong, so we can't continue
    return (status);

  // Make sure the driver's "set mode" routine has been initialized
  if (kernelDma->deviceDriver->driverSetMode == NULL)
    {
       // Ooops.  Driver function is NULL.  Make an error.
      kernelError(kernel_error, "Driver function is NULL");
      return (status = ERR_NOSUCHFUNCTION);
   }

  // Make the "mode" argument say "Read mode"
  mode = (mode | READMODE);

  status = kernelDma->deviceDriver->driverSetMode(channelNumber, mode);

  // if status is not equal to zero, we'll assume it's a driver error
  // and register a kernelError

  if (status < 0)
    {
      // Make the error
      kernelError(kernel_error, "Error while performing a DMA read");
    }

  return (status);
}


int kernelDmaFunctionsWriteData(int channelNumber, int mode)
{
  // This function is used to instigate a write operation FROM the associated
  // device TO memory.  It is a generic routine which calls the 
  // specific associated device driver function installed with the
  // kernelDmaInstallDriver function.

  int status = 0;

  // Check the DMA object and device driver before proceeding
  status = checkObjectAndDriver(__FUNCTION__);
  if (status < 0)
    // Something went wrong, so we can't continue
    return (status);

  // Make sure the driver's "set mode" routine has been initialized
  if (kernelDma->deviceDriver->driverSetMode == NULL)
    {
      // Ooops.  Driver function is NULL.  Make an error.
      kernelError(kernel_error, "Driver function is NULL");
      return (status = ERR_NOSUCHFUNCTION);
    }

  // Make the "mode" argument say "Write mode"
  mode = (mode | WRITEMODE);

  status = kernelDma->deviceDriver->driverSetMode(channelNumber, mode);

  // if status is not equal to zero, we'll assume it's a driver error
  // and register a kernelError

  if (status < 0)
    {
      // Make the error
      kernelError(kernel_error, "Error while performing a DMA write");
    }

  return (status);
}
