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
//  kernelPicFunctions.c
//

#include "kernelPicFunctions.h"
#include "kernelError.h"
#include <sys/errors.h>
#include <string.h>


static kernelPicObject *kernelPic = NULL;


static int checkObjectAndDriver(char *invokedBy)
{
  // This routine consists of a couple of things commonly done by the
  // other driver wrapper routines.  I've done this to minimize the amount
  // of duplicated code.  As its argument, it takes the name of the 
  // routine that called it so that any error messages can better reflect
  // what was really being done when the error occurred.

  int status = 0;

  // Make sure the PIC object isn't NULL (which could indicate that the
  // device has not been properly registered)
  if (kernelPic == NULL)
    {
      // Make the error
      kernelError(kernel_error, "The interrupt controller is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  // Make sure that the device has a non-NULL driver
  if (kernelPic->deviceDriver == NULL)
    {
      // Make the error
      kernelError(kernel_error, "The device driver is NULL");
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


int kernelPicRegisterDevice(kernelPicObject *thePic)
{
  // This routine will register a new PIC object.  It takes a 
  // kernelPicObject structure and returns 0 if successful.  It returns 
  // -1 if the device srtucture is NULL.

  int status = 0;

  if (thePic == NULL)
    {
      // Make the error
      kernelError(kernel_error, "The interrupt controller is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  // Alright.  We'll save the pointer to the device
  kernelPic = thePic;

  // Return success
  return (status = 0);
}


int kernelPicInstallDriver(kernelPicDeviceDriver *theDriver)
{
  // Attaches a driver object to an PIC object.  If the pointer to the
  // driver object is NULL, it returns -1.  Otherwise, returns zero.

  int status = 0;

  // Make sure the Pic object isn't NULL
  if (kernelPic == NULL)
    {
      // Make the error
      kernelError(kernel_error, "The interrupt controller is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  // Make sure the driver isn't NULL
  if (theDriver == NULL)
    {
      // Make the error
      kernelError(kernel_error, "The device driver is NULL");
      return (status = ERR_NOSUCHDRIVER);
    }

  // Install the device driver
  kernelPic->deviceDriver = theDriver;
  
  // Return success
  return (status = 0);
}


int kernelPicInitialize(void)
{
  // This function initializes the PIC.  It pretty much just calls
  // the associated driver routines, but it also does some checks and
  // whatnot to make sure that the device, driver, and driver routines are
  // valid.

  int status = 0;

  // Check the PIC object and device driver before proceeding
  status = checkObjectAndDriver(__FUNCTION__);
  if (status < 0)
    // Something went wrong, so we can't continue
    return (status);

  // Now make sure the device driver initialize routine has been 
  // installed
  if (kernelPic->deviceDriver->driverInitialize == NULL)
    {
      // Make the error
      kernelError(kernel_error, "The device driver routine is NULL");
      return (status = ERR_NOSUCHFUNCTION);
    }

  // Ok, now we can call the routine.
  status = kernelPic->deviceDriver->driverInitialize();

  if (status < 0)
    {
      // Make the error
      kernelError(kernel_error, "The interrupt controller driver "
		  "initialization failed");
      return (status);
    }

  // Return success
  return (status = 0);
}


int kernelPicEndOfInterrupt(int interruptNumber)
{
  // This instructs the PIC to end the current interrupt.  It pretty much 
  // just calls the associated driver routines, but it also does some
  // checks and whatnot to make sure that the device, driver, and driver
  // routines are valid.  Note that the interrupt number parameter is
  // merely so that the driver can determine which controller(s) to send
  // the command to.

  int status = 0;

  // Check the PIC object and device driver before proceeding
  status = checkObjectAndDriver(__FUNCTION__);
  if (status < 0)
    // Something went wrong, so we can't continue
    return (status);

  // Now make sure the device driver disable interrupts routine has been 
  // installed
  if (kernelPic->deviceDriver->driverEndOfInterrupt == NULL)
    {
      // Make the error
      kernelError(kernel_error, "The device driver routine is NULL");
      return (status = ERR_NOSUCHFUNCTION);
    }

  // Ok, now we can call the routine.
  kernelPic->deviceDriver->driverEndOfInterrupt(interruptNumber);

  // (The driver function returns void)

  // Return success
  return (status = 0);
}
