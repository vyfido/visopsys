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
//  kernelSysTimerFunctions.c
//

#include "kernelSysTimerFunctions.h"
#include "kernelError.h"
#include <sys/errors.h>
#include <string.h>


// These are the generic C "wrapper" functions for the routines which
// reside in the system timer driver.  Most of them basically just call
// their associated functions, but there will be extra functionality here
// as well.


static kernelSysTimerObject *kernelTimer = NULL;


static int checkObjectAndDriver(char *invokedBy)
{
  // This routine consists of a couple of things commonly done by the
  // other driver wrapper routines.  I've done this to minimize the amount
  // of duplicated code.  As its argument, it takes the name of the 
  // routine that called it so that any error messages can better reflect
  // what was really being done when the error occurred.

  int status = 0;

  // Make sure the SysTimer object isn't NULL (which could indicate that the
  // device has not been properly registered)
  if (kernelTimer == NULL)
    {
      // Make the error
      kernelError(kernel_error, "The system timer is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  // Make sure that the device has a non-NULL driver
  if (kernelTimer->deviceDriver == NULL)
    {
      // Make the error
      kernelError(kernel_error, "The system timer driver is NULL");
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


int kernelSysTimerRegisterDevice(kernelSysTimerObject *theTimer)
{
  // This routine will register a new system timer object.  It takes a 
  // kernelSysTimerObject structure and returns 0 if successful.  It returns 
  // -1 if the device srtucture is NULL.

  int status = 0;

  if (theTimer == NULL)
    {
      // Make the error
      kernelError(kernel_error, "The system timer is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  // Alright.  We'll save the pointer to the device
  kernelTimer = theTimer;

  // Return success
  return (status = 0);
}


int kernelSysTimerInstallDriver(kernelSysTimerDriver *theDriver)
{
  // Attaches a driver object to a timer object.  If the pointer to the
  // driver object is NULL, it returns -1.  Otherwise, returns zero.

  int status = 0;

  // Make sure the system timer isn't NULL
  if (kernelTimer == NULL)
    {
      // Make the error
      kernelError(kernel_error, "The system timer is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  // Make sure the driver isn't NULL
  if (theDriver == NULL)
    {
      // Make the error
      kernelError(kernel_error, "The system timer driver is NULL");
      return (status = ERR_NOSUCHDRIVER);
    }

  // Install the device driver
  kernelTimer->deviceDriver = theDriver;
  
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

  // Check the timer object and device driver before proceeding
  status = checkObjectAndDriver(__FUNCTION__);
  if (status < 0)
    // Something went wrong, so we can't continue
    return (status);

  // Now make sure the device driver initialize routine has been 
  // installed
  if (kernelTimer->deviceDriver->driverInitialize == NULL)
    {
      // Make the error
      kernelError(kernel_error, "The device driver routine is NULL");
      return (status = ERR_NOSUCHFUNCTION);
    }

  status = kernelTimer->deviceDriver->driverInitialize();

  // Return the code from the driver call
  return (status);
}


unsigned kernelSysTimerRead(void)
{
  // This returns the value of the number of system timer ticks.  It 
  // pretty much just calls the associated driver routines, but it also 
  // does some checks and whatnot to make sure that the device, driver, 
  // and driver routines are

  unsigned timer = 0;
  int status = 0;
  
  // Check the timer object and device driver before proceeding
  status = checkObjectAndDriver(__FUNCTION__);
  if (status < 0)
    // Something went wrong, so we can't continue
    return (status);

  // Now make sure the device driver timer tick routine has been 
  // installed
  if (kernelTimer->deviceDriver->driverReadTicks == NULL)
    {
      // Make the error
      kernelError(kernel_error, "The device driver routine is NULL");
      return (status = ERR_NOSUCHFUNCTION);
    }

  // Ok, now we can call the routine.
  timer = kernelTimer->deviceDriver->driverReadTicks();

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

  // Check the timer object and device driver before proceeding
  status = checkObjectAndDriver(__FUNCTION__);
  if (status < 0)
    // Something went wrong, so we can't continue
    return (status);

  // Now make sure the device driver read value routine has been 
  // installed
  if (kernelTimer->deviceDriver->driverReadTicks == NULL)
    {
      // Make the error
      kernelError(kernel_error, "The device driver routine is NULL");
      return (status = ERR_NOSUCHFUNCTION);
    }

  // Ok, now we can call the routine.
  value = kernelTimer->deviceDriver->driverReadValue(timer);

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

  // Check the timer object and device driver before proceeding
  status = checkObjectAndDriver(__FUNCTION__);
  if (status < 0)
    // Something went wrong, so we can't continue
    return (status);

  // Now make sure the device driver setup timer routine has been 
  // installed
  if (kernelTimer->deviceDriver->driverSetupTimer == NULL)
    {
      // Make the error
      kernelError(kernel_error, "The device driver routine is NULL");
      return (status = ERR_NOSUCHFUNCTION);
    }

  // Ok, now we can call the routine.
  status = 
    kernelTimer->deviceDriver->driverSetupTimer(timer, mode, startCount);

  // Return the result from the driver call
  return (status);
}


void kernelSysTimerWaitTicks(int waitTicks)
{
  // This routine waits for a specified number of timer ticks to occur.  
  // Also does some checks and whatnot to make sure that the device and 
  // driver have been properly initialized.

  int targetTime = 0;
  int status = 0;

  // Check the timer object and device driver before proceeding
  status = checkObjectAndDriver(__FUNCTION__);
  if (status < 0)
    // Something went wrong, so we can't continue
    return;

  // Now make sure the device driver read timer routine has been 
  // installed
  if (kernelTimer->deviceDriver->driverReadTicks == NULL)
    {
      // Make the error
      kernelError(kernel_error, "The device driver routine is NULL");
      return;
    }

  // One more thing: make sure the number to wait is reasonable.
  if (waitTicks < 0)
    {
      // Make the error
      kernelError(kernel_warn, "Timer ticks to wait is negative.  Not "
		  "possible in this dimension");
      // Assume zero
      waitTicks = 0;
    }

  // Ok, now we can call the timer routine safely.

  // Find out the current time
  targetTime = kernelTimer->deviceDriver->driverReadTicks();

  // Add the ticks-to-wait to that number
  targetTime += waitTicks;

  // Now loop until the time reaches the specified mark
  while (targetTime >= kernelTimer->deviceDriver->driverReadTicks());

  return;
}


void kernelSysTimerTick(void)
{
  // This registers a tick of the system timer.  It pretty much just calls
  // the associated driver routines, but it also does some checks and
  // whatnot to make sure that the device, driver, and driver routines are

  int status = 0;

  // Check the timer object and device driver before proceeding
  status = checkObjectAndDriver(__FUNCTION__);
  if (status < 0)
    // Something went wrong, so we can't continue
    return;

  // Now make sure the device driver timer tick routine has been 
  // installed
  if (kernelTimer->deviceDriver->driverTimerTick == NULL)
    {
      // Make the error
      kernelError(kernel_error, "The device driver routine is NULL");
      return;
    }

  // Ok, now we can call the routine.
  kernelTimer->deviceDriver->driverTimerTick();

  return;
}
