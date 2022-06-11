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
//  kernelKeyboard.c
//
	
// These are the generic C "wrapper" functions for the routines which
// reside in the system timer driver.  Most of them basically just call
// their associated functions.


#include "kernelKeyboard.h"
#include "kernelMultitasker.h"
#include "kernelText.h"
#include "kernelError.h"
#include "kernelWindowManager.h"
#include <sys/errors.h>
#include <string.h>


static kernelKeyboard *systemKeyboard = NULL;
static stream *consoleStream = NULL;
static int initialized = 0;


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
//  Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


int kernelKeyboardRegisterDevice(kernelKeyboard *theKeyboard)
{
  // This routine will register a new keyboard.  It takes a 
  // kernelKeyboard structure and returns 0 if successful.

  int status = 0;

  if (theKeyboard == NULL)
    {
      kernelError(kernel_error, "The keyboard is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  // Make sure that the device has a non-NULL driver
  if (theKeyboard->driver == NULL)
    {
      kernelError(kernel_error, "The driver is NULL");
      return (status = ERR_NOSUCHDRIVER);
    }

  // If the driver has a 'register device' function, call it
  if (theKeyboard->driver->driverRegisterDevice)
    status = theKeyboard->driver->driverRegisterDevice(theKeyboard);

  // Alright.  We'll save the pointer to the device
  systemKeyboard = theKeyboard;

  return (status);
}


int kernelKeyboardInitialize(void)
{
  // This function initializes the keyboard.  It pretty much just calls
  // the associated driver routines, but it also does some checks and
  // whatnot to make sure that the device, driver, and driver routines are
  // valid.

  int status = 0;

  // Check the keyboard and device driver before proceeding
  if (systemKeyboard == NULL)
    {
      kernelError(kernel_error, "The keyboard is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  initialized = 1;

  return (status = 0);
}


int kernelKeyboardSetStream(stream *newStream)
{
  // Set the current stream used by the keyboard driver
  
  int status = 0;

  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Save the address of the kernelStream we were passed to use for
  // keyboard data
  consoleStream = newStream;

  return (status = 0);
}


int kernelKeyboardReadData(void)
{
  // This function calls the keyboard driver to read data from the
  // device.  It pretty much just calls the associated driver routine.

  int status = 0;

  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Now make sure the device driver 'read data' routine has been installed
  if (systemKeyboard->driver->driverReadData == NULL)
    {
      kernelError(kernel_error, "The driver function is NULL");
      return (status = ERR_NOSUCHFUNCTION);
    }

  // Ok, now we can call the routine.
  systemKeyboard->driver->driverReadData();

  return (status = 0);
}


int kernelKeyboardInput(int ascii, int eventType)
{
  // This gets called by the keyboard driver to tell us that a key has been
  // pressed.
  windowEvent event;

  if (consoleStream && (eventType & EVENT_KEY_DOWN))
    consoleStream->append(consoleStream, (char) ascii);

  // Fill out our event
  event.type = eventType;
  event.xPosition = 0;
  event.yPosition = 0;
  event.key = ascii;

  // Notify the window manager of the event
  kernelWindowManagerProcessEvent(&event);

  return (0);
}
