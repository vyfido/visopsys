//
//  Visopsys
//  Copyright (C) 1998-2006 J. Andrew McLaughlin
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

// These are the generic C "wrapper" functions for the routines which
// reside in the system timer driver.  Most of them basically just call
// their associated functions, but there will be extra functionality here
// as well.

#include "kernelSysTimer.h"
#include "kernelInterrupt.h"
#include "kernelPic.h"
#include "kernelProcessorX86.h"
#include "kernelError.h"
#include <string.h>

static kernelDevice *systemTimer = NULL;
static kernelSysTimerOps *ops = NULL;


static void timerInterrupt(void)
{
  // This is the system timer interrupt handler.  It calls the timer driver
  // to actually read data from the device.

  void *address = NULL;

  kernelProcessorIsrEnter(address);
  kernelProcessingInterrupt = 1;

  // Ok, now we can call the routine.
  if (ops->driverTick)
    ops->driverTick();

  kernelPicEndOfInterrupt(INTERRUPT_NUM_SYSTIMER);

  kernelProcessingInterrupt = 0;
  kernelProcessorIsrExit(address);
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
//  Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


int kernelSysTimerInitialize(kernelDevice *dev)
{
  // This function initializes the system timer.

  int status = 0;

  if (dev == NULL)
    {
      kernelError(kernel_error, "The system timer device is NULL");
      return (status = ERR_NOTINITIALIZED);
    }

  systemTimer = dev;

  if ((systemTimer->driver == NULL) || (systemTimer->driver->ops == NULL))
    {
      kernelError(kernel_error, "The system timer driver or ops are NULL");
      return (status = ERR_NULLPARAMETER);
    }

  ops = systemTimer->driver->ops;

  // Register our interrupt handler
  status = kernelInterruptHook(INTERRUPT_NUM_SYSTIMER, &timerInterrupt);
  if (status < 0)
    return (status);

  // Turn on the interrupt
  kernelPicMask(INTERRUPT_NUM_SYSTIMER, 1);

  // Return success
  return (status = 0);
}


void kernelSysTimerTick(void)
{
  // This registers a tick of the system timer.

  if (systemTimer == NULL)
    return;

  // Ok, now we can call the routine.
  if (ops->driverTick)
    ops->driverTick();
 
  return;
}


unsigned kernelSysTimerRead(void)
{
  // This returns the value of the number of system timer ticks.

  unsigned timer = 0;
  int status = 0;
  
  if (systemTimer == NULL)
    return (status = ERR_NOTINITIALIZED);

  // Now make sure the device driver timer tick routine has been 
  // installed
  if (ops->driverRead == NULL)
    {
      kernelError(kernel_error, "The device driver routine is NULL");
      return (status = ERR_NOSUCHFUNCTION);
    }

  // Ok, now we can call the routine.
  timer = ops->driverRead();

  // Return the result from the driver call
  return (timer);
}


int kernelSysTimerReadValue(int timer)
{
  // This returns the current value of the requested timer.

  int value = 0;
  int status = 0;

  if (systemTimer == NULL)
    return (status = ERR_NOTINITIALIZED);

  // Now make sure the device driver read value routine has been 
  // installed
  if (ops->driverReadValue == NULL)
    {
      kernelError(kernel_error, "The device driver routine is NULL");
      return (status = ERR_NOSUCHFUNCTION);
    }

  // Ok, now we can call the routine.
  value = ops->driverReadValue(timer);

  // Return the result from the driver call
  return (value);
}


int kernelSysTimerSetupTimer(int timer, int mode, int startCount)
{
  // This sets up the operation of the requested timer.

  int status = 0;

  if (systemTimer == NULL)
    return (status = ERR_NOTINITIALIZED);

  // Now make sure the device driver setup timer routine has been 
  // installed
  if (ops->driverSetupTimer == NULL)
    {
      kernelError(kernel_error, "The device driver routine is NULL");
      return (status = ERR_NOSUCHFUNCTION);
    }

  // Ok, now we can call the routine.
  status = ops->driverSetupTimer(timer, mode, startCount);

  // Return the result from the driver call
  return (status);
}


void kernelSysTimerWaitTicks(int waitTicks)
{
  // This routine waits for a specified number of timer ticks to occur.  

  int targetTime = 0;

  if (systemTimer == NULL)
    return;

  // Now make sure the device driver read timer routine has been 
  // installed
  if (ops->driverRead == NULL)
    {
      kernelError(kernel_error, "The device driver routine is NULL");
      return;
    }

  // One more thing: make sure the number to wait is reasonable.
  if (waitTicks < 0)
    // Not possible in this dimension.  Assume zero.
    waitTicks = 0;

  // Ok, now we can call the timer routine safely.

  // Find out the current time
  targetTime = ops->driverRead();

  // Add the ticks-to-wait to that number
  targetTime += waitTicks;

  // Now loop until the time reaches the specified mark
  while (targetTime >= ops->driverRead());

  return;
}
