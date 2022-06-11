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
//  kernelPic.c
//

#include "kernelPic.h"
#include "kernelError.h"
#include <sys/errors.h>
#include <string.h>


static kernelPic *systemPic = NULL;
static int initialized = 0;


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
//  Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


int kernelPicRegisterDevice(kernelPic *thePic)
{
  // This routine will register a new PIC.

  int status = 0;

  // Check the PIC and driver
  if (thePic == NULL)
    {
      kernelError(kernel_error, "The PIC device is NULL");
      return (status = ERR_NULLPARAMETER);
    }
  // Make sure that the device has a non-NULL driver
  if (thePic->driver == NULL)
    {
      kernelError(kernel_error, "The PIC driver is NULL");
      return (status = ERR_NOSUCHDRIVER);
    }

  // If the driver has a 'register device' function, call it
  if (thePic->driver->driverRegisterDevice)
    status = thePic->driver->driverRegisterDevice(thePic);

  // Alright.  We'll save the pointer to the device
  systemPic = thePic;

  // Return success
  return (status);
}


int kernelPicInitialize(void)
{
  // This function initializes the PIC.  It pretty much just calls
  // the associated driver routines, but it also does some checks and
  // whatnot to make sure that the device, driver, and driver routines are
  // valid.

  int status = 0;

  if (systemPic == NULL)
    {
      kernelError(kernel_error, "The interrupt controller is NULL");
      return (status = ERR_NOTINITIALIZED);
    }

  initialized = 1;

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

  if (!initialized)
    {
      kernelError(kernel_error, "PIC driver not initialized");
      return (status = ERR_NOTINITIALIZED);
    }

  // Now make sure the device driver 'end of interrupt' routine has been 
  // installed
  if (systemPic->driver->driverEndOfInterrupt == NULL)
    {
      kernelError(kernel_error, "The device driver routine is NULL");
      return (status = ERR_NOSUCHFUNCTION);
    }

  // Ok, now we can call the routine.
  systemPic->driver->driverEndOfInterrupt(interruptNumber);

  // (The driver function returns void)

  // Return success
  return (status = 0);
}
