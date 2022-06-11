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
//  kernelKeyboardFunctions.c
//
	
// These are the generic C "wrapper" functions for the routines which
// reside in the system timer driver.  Most of them basically just call
// their associated functions.


#include "kernelKeyboardFunctions.h"
#include "kernelText.h"
#include "kernelError.h"
#include <sys/errors.h>
#include <string.h>


static kernelKeyboardObject *kernelKeyboard = NULL;


static int checkObjectAndDriver(char *invokedBy)
{
  // This routine consists of a couple of things commonly done by the
  // other driver wrapper routines.  I've done this to minimize the amount
  // of duplicated code.  As its argument, it takes the name of the 
  // routine that called it so that any error messages can better reflect
  // what was really being done when the error occurred.

  int status = 0;

  // Make sure the keyboard object isn't NULL (which could indicate that the
  // device has not been properly registered)
  if (kernelKeyboard == NULL)
    {
      // Make the error
      kernelError(kernel_error, "The keyboard is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  // Make sure that the device has a non-NULL driver
  if (kernelKeyboard->deviceDriver == NULL)
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


int kernelKeyboardRegisterDevice(kernelKeyboardObject *theKeyboard)
{
  // This routine will register a new keyboard object.  It takes a 
  // kernelKeyboardObject structure and returns 0 if successful.  It returns 
  // negative if the device structure is NULL.

  int status = 0;

  if (theKeyboard == NULL)
    {
      // Make the error
      kernelError(kernel_error, "The keyboard is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  // Alright.  We'll save the pointer to the device
  kernelKeyboard = theKeyboard;

  return (status = 0);
}


int kernelKeyboardInstallDriver(kernelKeyboardDriver *theDriver)
{
  // Attaches a driver object to a keyboard object.  If the pointer to the
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
  kernelKeyboard->deviceDriver = theDriver;
  
  return (status = 0);
}


int kernelKeyboardInitialize(void)
{
  // This function initializes the keyboard.  It pretty much just calls
  // the associated driver routines, but it also does some checks and
  // whatnot to make sure that the device, driver, and driver routines are
  // valid.

  int status = 0;
  kernelTextInputStream *console = NULL;

  // Check the keyboard object and device driver before proceeding
  status = checkObjectAndDriver(__FUNCTION__);
  if (status < 0)
    // Something went wrong, so we can't continue
    return (status);

  // Now make sure the device driver initialize routine has been 
  // installed
  if (kernelKeyboard->deviceDriver->driverInitialize == NULL)
    {
      // Make the error
      kernelError(kernel_error, "The driver function is NULL");
      return (status = ERR_NOSUCHFUNCTION);
    }

  // Ok, find out which kernelTextInputStream represents the console input
  console = kernelTextGetConsoleInput();

  if (console == NULL)
    {
      kernelError(kernel_error, "Unable to determine the console input "
		  "stream");
      return (status = ERR_NOSUCHENTRY);
    }

  // Ok, now we can call the routine.
  status = kernelKeyboard->deviceDriver->driverInitialize();

  return (status);
}


int kernelKeyboardSetStream(stream *newStream)
{
  // Set the current stream used by the keyboard driver
  
  int status = 0;

  // Check the keyboard object and device driver before proceeding
  status = checkObjectAndDriver(__FUNCTION__);
  if (status < 0)
    // Something went wrong, so we can't continue
    return (status);

  // Now make sure the device driver initialize routine has been 
  // installed
  if (kernelKeyboard->deviceDriver->driverSetStream == NULL)
    {
      // Make the error
      kernelError(kernel_error, "The driver function is NULL");
      return (status = ERR_NOSUCHFUNCTION);
    }

  // We need to unscramble some of the goop surrounding this text input
  // stream, and pass two pieces of information to the keyboard driver
  // initialize routine:
  // 1. A stream to append characters to (the console input stream)
  // 2. A function for appending single characters to a stream
  status = kernelKeyboard->deviceDriver
    ->driverSetStream(newStream, (void *) newStream->append);
  
  return (status);
}


int kernelKeyboardReadData(void)
{
  // This function calls the keyboard driver to read data from the
  // device.  It pretty much just calls the associated driver routines, 
  // but it also does some checks and whatnot to make sure that the 
  // device, driver, and driver routines are

  int status = 0;

  // Check the keyboard object and device driver before proceeding
  status = checkObjectAndDriver(__FUNCTION__);
  if (status < 0)
    // Something went wrong, so we can't continue
    return (status);

  // Now make sure the device driver initialize routine has been 
  // installed
  if (kernelKeyboard->deviceDriver->driverReadData == NULL)
    {
      // Make the error
      kernelError(kernel_error, "The driver function is NULL");
      return (status = ERR_NOSUCHFUNCTION);
    }

  // Ok, now we can call the routine.
  kernelKeyboard->deviceDriver->driverReadData();

  return (status = 0);
}
