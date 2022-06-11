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
//  kernelSysTimer.c
//

#include "kernelSysTimer.h"
#include "kernelError.h"
#include <sys/errors.h>
#include <string.h>


// These are the generic C "wrapper" functions for the routines which
// reside in the system timer driver.  Most of them basically just call
// their associated functions, but there will be extra functionality here
// as well.


static kernelSysTimer *systemTimer = NULL;
int initialized = 0;


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
//  Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


int kernelSysTimerRegisterDevice(kernelSysTimer *theTimer)
{
  // This routine will register a new system timer.  Returns 0 if successful.

  int status = 0;

  if (theTimer == NULL)
    {
      kernelError(kernel_error, "The system timer is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  // Make sure that the device has a non-NULL driver
  if (theTimer->driver == NULL)
    {
      kernelError(kernel_error, "The system timer driver is NULL");
      return (status = ERR_NOSUCHDRIVER);
    }

  // If the driver has a 'register device' function, call it
  if (theTimer->driver->driverRegisterDevice)
    status = theTimer->driver->driverRegisterDevice(theTimer);

  // Alright.  We'll save the pointer to the device
  systemTimer = theTimer;

  // Return success
  return (status = 0);
}


int kernelSysTimerInitialize(void)
{
  // This function initializes the system timer.  It pretty much just calls
  // the associated driver routines, but it also does some checks and
  // whatnot to make sure that the device, driver, and driver routines are
  // valid.

  int status = 0;

  // Check the timer before proceeding
  if (systemTimer == NULL)
    {
      kernelError(kernel_error, "The system timer is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  initialized = 1;

  return (status = 0);
}


unsigned kernelSysTimerRead(void)
{
  // This returns the value of the number of system timer ticks.  It 
  // pretty much just calls the associated driver routines, but it also 
  // does some checks and whatnot to make sure that the device, driver, 
  // and driver routines are

  unsigned timer = 0;
  int status = 0;
  
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Now make sure the device driver timer tick routine has been 
  // installed
  if (systemTimer->driver->driverReadTicks == NULL)
    {
      kernelError(kernel_error, "The device driver routine is NULL");
      return (status = ERR_NOSUCHFUNCTION);
    }

  // Ok, now we can call the routine.
  timer = systemTimer->driver->driverReadTicks();

  // Return the result from the driver call
  return (timer);
}


int kernelSysTimerReadValue(int timer)
{
  // This returns the current value of the requested timer.  It 
  // pretty much just calls the associated driver routines, but it also 
  // does some checks and whatnot to make sure that the device, driver, 
  // and driver routines are

  int value = 0;
  int status = 0;

  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Now make sure the device driver read value routine has been 
  // installed
  if (systemTimer->driver->driverReadTicks == NULL)
    {
      kernelError(kernel_error, "The device driver routine is NULL");
      return (status = ERR_NOSUCHFUNCTION);
    }

  // Ok, now we can call the routine.
  value = systemTimer->driver->driverReadValue(timer);

  // Return the result from the driver call
  return (value);
}


int kernelSysTimerSetupTimer(int timer, int mode, int startCount)
{
  // This sets up the operation of the requested timer.  It 
  // pretty much just calls the associated driver routines, but it also 
  // does some checks and whatnot to make sure that the device, driver, 
  // and driver routines are

  int status = 0;

  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Now make sure the device driver setup timer routine has been 
  // installed
  if (systemTimer->driver->driverSetupTimer == NULL)
    {
      kernelError(kernel_error, "The device driver routine is NULL");
      return (status = ERR_NOSUCHFUNCTION);
    }

  // Ok, now we can call the routine.
  status = systemTimer->driver->driverSetupTimer(timer, mode, startCount);

  // Return the result from the driver call
  return (status);
}


void kernelSysTimerWaitTicks(int waitTicks)
{
  // This routine waits for a specified number of timer ticks to occur.  
  // Also does some checks and whatnot to make sure that the device and 
  // driver have been properly initialized.

  int targetTime = 0;

  if (!initialized)
    return;

  // Now make sure the device driver read timer routine has been 
  // installed
  if (systemTimer->driver->driverReadTicks == NULL)
    {
      kernelError(kernel_error, "The device driver routine is NULL");
      return;
    }

  // One more thing: make sure the number to wait is reasonable.
  if (waitTicks < 0)
    {
      kernelError(kernel_warn, "Timer ticks to wait is negative.  Not "
		  "possible in this dimension");
      // Assume zero
      waitTicks = 0;
    }

  // Ok, now we can call the timer routine safely.

  // Find out the current time
  targetTime = systemTimer->driver->driverReadTicks();

  // Add the ticks-to-wait to that number
  targetTime += waitTicks;

  // Now loop until the time reaches the specified mark
  while (targetTime >= systemTimer->driver->driverReadTicks());

  return;
}


void kernelSysTimerTick(void)
{
  // This registers a tick of the system timer.  It pretty much just calls
  // the associated driver routines, but it also does some checks and
  // whatnot to make sure that the device, driver, and driver routines are

  if (!initialized)
    return;

  // Now make sure the device driver timer tick routine has been 
  // installed
  if (systemTimer->driver->driverTimerTick == NULL)
    {
      kernelError(kernel_error, "The device driver routine is NULL");
      return;
    }

  // Ok, now we can call the routine.
  systemTimer->driver->driverTimerTick();

  return;
}
