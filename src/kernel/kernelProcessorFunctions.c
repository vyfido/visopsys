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
//  kernelProcessorFunctions.c
//

// These are the generic C "wrapper" functions for the routines which
// reside in the system timer driver.  Most of them basically just call
// their associated functions.

#include "kernelProcessorFunctions.h"
#include "kernelError.h"
#include <sys/errors.h>
#include <string.h>


static kernelProcessorObject *kernelProcessor = NULL;


static int checkObjectAndDriver(char *invokedBy)
{
  // This routine consists of a couple of things commonly done by the
  // other driver wrapper routines.  I've done this to minimize the amount
  // of duplicated code.  As its argument, it takes the name of the 
  // routine that called it so that any error messages can better reflect
  // what was really being done when the error occurred.

  int status = 0;

  // Make sure the processor object isn't NULL (which could indicate that the
  // device has not been properly registered)
  if (kernelProcessor == NULL)
    {
      // Make the error
      kernelError(kernel_error, "The processor is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  // Make sure that the device has a non-NULL driver
  if (kernelProcessor->deviceDriver == NULL)
    {
      // Make the error
      kernelError(kernel_error, "The driver is NULL");
      return (status = ERR_NOSUCHDRIVER);
    }

  return (status = 0);
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
//  Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


int kernelProcessorRegisterDevice(kernelProcessorObject *theProcessor)
{
  // This routine will register a new processor object.  It takes a 
  // kernelProcessorObject structure and returns 0 if successful.  It returns 
  // negative if the device structure is NULL.

  int status = 0;

  if (theProcessor == NULL)
    {
      // Make the error
      kernelError(kernel_error, "The processor is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  // Alright.  We'll save the pointer to the device
  kernelProcessor = theProcessor;

  return (status = 0);
}


int kernelProcessorInstallDriver(kernelProcessorDriver *theDriver)
{
  // Attaches a driver object to a processor object.  If the pointer to the
  // driver object is NULL, it returns negative.  Otherwise, returns zero.

  int status = 0;

  // Make sure the driver isn't NULL
  if (theDriver == NULL)
    {
      // Make the error
      kernelError(kernel_error, "The driver is NULL");
      return (status = ERR_NOSUCHDRIVER);
    }

  // Install the device driver
  kernelProcessor->deviceDriver = theDriver;
  
  return (status = 0);
}


int kernelProcessorInitialize(void)
{
  // This function initializes the processor.  It pretty much just calls
  // the associated driver routines, but it also does some checks and
  // whatnot to make sure that the device, driver, and driver routines are
  // valid.

  int status = 0;

  // Check the processor object and device driver before proceeding
  status = checkObjectAndDriver(__FUNCTION__);
  if (status < 0)
    // Something went wrong, so we can't continue
    return (status);

  // Now make sure the device driver initialize routine has been 
  // installed
  if (kernelProcessor->deviceDriver->driverInitialize == NULL)
    {
      // Make the error
      kernelError(kernel_error, "The driver function is NULL");
      return (status = ERR_NOSUCHFUNCTION);
    }

  // Ok, now we can call the routine.
  status = kernelProcessor->deviceDriver->driverInitialize();

  return (status);
}
