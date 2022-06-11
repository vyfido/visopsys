//
//  Visopsys
//  Copyright (C) 1998-2001 J. Andrew McLaughlin
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
#include "kernelMiscAsmFunctions.h"
#include "kernelError.h"
#include <sys/errors.h>
#include <string.h>


// These are the generic C "wrapper" functions for the routines which
// reside in the system timer driver.  Most of them basically just call
// their associated functions, but there will be extra functionality here
// as well.


static kernelSysTimerObject *kernelTimer = NULL;
static kernelTimedEvent eventsQueue[MAXTIMEDEVENTS];
static int timedEvents = 0;


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
      kernelError(kernel_error, NULL_TIMER_OBJECT);
      return (status = ERR_NULLPARAMETER);
    }

  // Make sure that the device has a non-NULL driver
  if (kernelTimer->deviceDriver == NULL)
    {
      // Make the error
      kernelError(kernel_error, NULL_TIMER_DRIVER_OBJECT);
      return (status = ERR_NOSUCHDRIVER);
    }

  // Return success
  return (status = 0);
}


static void timedEventDispatch(void)
{
  // This is non-exported routine that checks for the presense of a timed
  // event at the current system timer value.  It is called by the
  // kernelSysTimerTick routine (which is invoked by a sytem timer interrupt).
  // If there are such events, this routine will execute them and remove
  // them from the queue.

  int count, count2, currentTimer;
  void (*goCode) (void) = NULL;


  // Get the current timer value
  currentTimer = kernelSysTimerRead();

  // Check for any events that occur at (or before) the current timer value.
  for (count = 0; count < timedEvents; count ++)
    {
      if (eventsQueue[count].targetTime <= currentTimer)
	{
	  // Save the pointer to the code.  We MUST remove the event from
	  // the queue BEFORE we execute it, so that if the event takes 
	  // more than one tick to complete, we don't get stuck in an 
	  // infinite loop trying to execute it.
	  goCode = eventsQueue[count].targetCode;
	  
	  // Remove the event from the queue by COPYING the values 
	  // of each event forward.
	  for (count2 = count; count2 < (timedEvents - 1); count2 ++)
	    {
	      eventsQueue[count2].targetTime = 
		eventsQueue[count2 + 1].targetTime;
	      eventsQueue[count2].targetCode =
		eventsQueue[count2 + 1].targetCode;
	      eventsQueue[count2].eventCode =
		eventsQueue[count2 + 1].eventCode;
	    }
	    
	  // Reduce the count, and reduce the total number
	  timedEvents -= 1;
	  count -= 1;

	  // This event should happen now.
	  goCode();
	}
    }

  return;
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
      kernelError(kernel_error, NULL_TIMER_OBJECT);
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
      kernelError(kernel_error, NULL_TIMER_OBJECT);
      return (status = ERR_NULLPARAMETER);
    }

  // Make sure the driver isn't NULL
  if (theDriver == NULL)
    {
      // Make the error
      kernelError(kernel_error, NULL_TIMER_DRIVER_OBJECT);
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
      kernelError(kernel_error, NULL_TIMER_DRIVER_ROUTINE);
      return (status = ERR_NOSUCHFUNCTION);
    }

  status = kernelTimer->deviceDriver->driverInitialize();

  // Return the code from the driver call
  return (status);
}


unsigned int kernelSysTimerRead(void)
{
  // This returns the value of the number of system timer ticks.  It 
  // pretty much just calls the associated driver routines, but it also 
  // does some checks and whatnot to make sure that the device, driver, 
  // and driver routines are

  unsigned int timer = 0;
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
      kernelError(kernel_error, NULL_TIMER_DRIVER_ROUTINE);
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
      kernelError(kernel_error, NULL_TIMER_DRIVER_ROUTINE);
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
      kernelError(kernel_error, NULL_TIMER_DRIVER_ROUTINE);
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
      kernelError(kernel_error, NULL_TIMER_DRIVER_ROUTINE);
      return;
    }

  // One more thing: make sure the number to wait is reasonable.
  if (waitTicks < 0)
    {
      // Make the error
      kernelError(kernel_warn, BAD_WAIT_TICKS);
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


unsigned int kernelTimedEventScheduler(void *routine, unsigned int time)
{
  // This routine will register a request for a routine to be called after 
  // a specifed number of timer ticks has elapsed.  This is really useful 
  // because it will enable code to retire, pending some time when an event 
  // should occur.  NOTE THAT THE ROUTINE CANNOT EXPECT PARAMETERS, and 
  // must return to this routine.  This function returns a number which
  // represents the event's Id.  This Id number should be used with the
  // kernelTimedEventCancel routine.

  static unsigned int eventCode = 0;


  // We don't really have to check the event time; the dispatch routine
  // will simply execute any events with past times.  Make sure the code
  // pointer isn't NULL however
  if (routine == NULL)
    {
      // Make the error
      kernelError(kernel_error, NULL_EVENT_FUNCTION);
      return (eventCode = 0);
    }

  // Create the kernelTimedEvent object
  kernelMemClear(&eventsQueue[timedEvents], sizeof(kernelTimedEvent));

  // Fill in the requested values
  eventsQueue[timedEvents].targetTime = time;
  eventsQueue[timedEvents].targetCode = routine;
  eventsQueue[timedEvents].eventCode = eventCode;

  // Increment the events counter
  timedEvents++;  eventCode++;

  return (eventsQueue[timedEvents - 1].eventCode);
}


int kernelTimedEventCancel(unsigned int eventCode)
{
  // This function can be used to cancel a pending event, removing it
  // from the timed event queue.  The value passed in as a parameter should
  // be the unsigned int returned by the kernelTimedEventScheduler

  int count, count2;
  int status = 0;


  // Find an event with a matching event id.
  for (count = 0; count < timedEvents; count ++)
    {
      if (eventsQueue[count].eventCode == eventCode)
	{

	  // Remove the event from the queue by COPYING the values 
	  // of each event forward.
	  for (count2 = count; count2 < (timedEvents - 1); count2 ++)
	    {
	      eventsQueue[count2].targetTime = 
		eventsQueue[count2 + 1].targetTime;
	      eventsQueue[count2].targetCode =
		eventsQueue[count2 + 1].targetCode;
	      eventsQueue[count2].eventCode =
		eventsQueue[count2 + 1].eventCode;
	    }
	  
	  // Reduce the total number
	  timedEvents -= 1;

	  // Finish
	  status = 0;
	  break;
	}
      else
	{
	  status = -1;
	}
    }

  return (status);
}


void kernelTimedEventDispatchAll(void)
{
  // This function can be used to expedite any pending scheduled
  // events.  Basically, it will force all pending events to be 
  // executed immediately, in the order that they remain in the
  // queue

  int count, count2;
  void (*goCode) (void) = NULL;


  // We should implement some code here that will sort all of the
  // pending events by execution time, so that we can do them all
  // in order

  // Go through the whole queue of events, and execute each one
  for (count = 0; count < timedEvents; count ++)
    {
      // Save the pointer to the code.  We MUST remove the event from
      // the queue BEFORE we execute it, so that if the event takes 
      // more than one tick to complete, we don't get stuck in an 
      // infinite loop trying to execute it.
      goCode = eventsQueue[count].targetCode;
      
      // Remove the event from the queue by COPYING the values 
      // of each event forward.
      for (count2 = count; count2 < (timedEvents - 1); count2 ++)
	{
	  eventsQueue[count2].targetTime = 
	    eventsQueue[count2 + 1].targetTime;
	  eventsQueue[count2].targetCode =
	    eventsQueue[count2 + 1].targetCode;
	  eventsQueue[count2].eventCode =
	    eventsQueue[count2 + 1].eventCode;
	}
	    
      // Reduce the count, and reduce the total number
      timedEvents -= 1;
      count -= 1;

      // This event should happen now.
      goCode();
    }
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
      kernelError(kernel_error, NULL_TIMER_DRIVER_ROUTINE);
      return;
    }

  // Ok, now we can call the routine.
  kernelTimer->deviceDriver->driverTimerTick();

  // Check for timed events at this timer value
  timedEventDispatch();

  return;
}
