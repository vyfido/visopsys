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
//  kernelResourceManager.c
//

// This header file contains source code for the kernel's standard 
// resource management facilities (resource locking, etc.).  These facilities
// can be used for locking any desired resource (i.e. it is not specific
// to devices, or anything in particular).

#include "kernelResourceManager.h"
#include "kernelMultitasker.h"
#include "kernelPicFunctions.h"
#include <sys/errors.h>
#include <string.h>


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


int kernelResourceManagerLock(volatile int *lock)
{
  // This function is used to lock a resource for exclusive use by a 
  // particular process.

  // The function scans the list of lock structures looking for an existing
  // lock on the same resource.  If the resource is not being used, a lock
  // structure is filled and the lock is granted.

  // If a lock is already held by another process at the time of the 
  // request, this routine will add the requesting process' id to the list
  // of 'waiters' in the lock structure, and go into a multitasker yield()
  // loop until the lock can be obtained -- on a first come, first served 
  // basis for the time being.  Waiters wait in a queue.

  // As a safeguard, this loop will make sure that the holding process is
  // still viable (i.e. it still exists, and is not stopped or anything
  // like that).

  // This yielding loop serves the dual purpose of maintaining exclusivity
  // and also allows the first process to terminate more quickly, since it
  // will not be contending with other resource-bound processes for processor
  // time.

  // The void* argument passed to the function must be a pointer to some
  // identifiable part of the resource (shared by all requesting
  // processes) such as a pointer to a data structure, or to a 'lock' flag.

  int status = 0;
  int interrupts = 0;
  int currentProcId = 0;

  // These are for priority inversion
  int myPriority = 0;
  volatile int holder = 0;
  volatile int holderPriority = 0;
  volatile int inversion = 0;

  // Make sure the pointer we were given is not NULL
  if (lock == NULL)
    return (status = ERR_NULLPARAMETER);

  // Get the process Id of the current process
  currentProcId = kernelMultitaskerGetCurrentProcessId();

  // Make sure it's a valid process Id
  if (currentProcId < 0)
    return (currentProcId);

  // Check to see whether the process already owns the lock.  This can
  // happen, and it's okay.
  if (*lock == currentProcId)
    // This process already owns the resource
    return (status = 0);

  while(1)
    {
      // This is the loop of death, where the requesting process will live
      // until it is allowed to use the resource

      // Disable interrupts from here, so that we don't get the lock granted
      // or released out from under us.
      kernelPicInterruptStatus(interrupts);
      kernelPicDisableInterrupts();

      if (*lock == 0)
	{
	  // Give the lock to the requesting process
	  *lock = currentProcId;

	  // If we inverted the priority of some process to achieve this lock,
	  // we need to reset it to its old priority
	  if (inversion)
	    {
	      kernelMultitaskerSetProcessPriority(holder, holderPriority);
	      holder = 0;
	      inversion = 0;
	    }
	}
      if (interrupts)
	kernelPicEnableInterrupts();

      // Got it?
      if (*lock == currentProcId)
	return (status = 0);

      // Some other process has locked the resource.  Make sure the process
      // is still alive, and that it is not sleeping, and that it has not
      // become stopped or zombie.  If it has, we will remove the lock given
      // to the process and reassign it to this process
      if (!kernelResourceManagerVerifyLock(lock))
	{
	  // We will give the lock to the requesting process at the exit of
	  // this loop.  Clear the current lock and exit the loop.
	  *lock = 0;
	  break;
	}

      if (!inversion)
	{
	  // Here's where we do priority inversion, if applicable.  
	  // Basically, the algorithm is as follows:
	  // - If the holding process has the same or higher priority than 
	  // the requesting process, do nothing
	  // - If the holding process has a lower priority level than the
	  //   requesting process, temporarily give the holding process the 
	  //   same priority level as the requesting process.
	  // This prevents a lower priority process from delaying a higher
	  // priority process for too long a time.

	  myPriority = kernelMultitaskerGetProcessPriority(currentProcId);
	  holderPriority = kernelMultitaskerGetProcessPriority(*lock);

	  if ((myPriority >= 0) && (holderPriority > myPriority))
	      {
		// Priority inversion.
		holder = *lock;
		kernelMultitaskerSetProcessPriority(holder, myPriority);
		inversion = 1;
	      }
	}

      // This process will now have to continue waiting until the lock has 
      // been released or becomes invalid
      
      // Yield this time slice back to the scheduler while the process
      // waits for the lock
      kernelMultitaskerYield();
	  
      // Loop again
      continue;
    }

  return (status = 0);
}


int kernelResourceManagerUnlock(volatile int *lock)
{
  // This function corresponds to the lock function.  It enables a 
  // process to release a resource that it had previously locked.

  int status = 0;
  int currentProcId = 0;
  
  // Make sure the pointer we were given is not NULL
  if (lock == NULL)
    return (status = ERR_NULLPARAMETER);

  // Get the process Id of the current process
  currentProcId = kernelMultitaskerGetCurrentProcessId();

  // Make sure it's a valid process Id
  if (currentProcId < 0)
    return (currentProcId);
  
  // Make sure that the current lock, if any, really belongs to this process.

  if (*lock == currentProcId)
    {
      *lock = 0;
      return (status = 0);
    }

  else
    // It is not locked by this process
    return (status = ERR_NOLOCK);
}


int kernelResourceManagerVerifyLock(volatile int *lock)
{
  // This function should be used to determine whether a lock is still
  // valid.  This means checking to see whether the locking process still
  // exists, and if so, that it is still viable (i.e. not sleeping, stopped,
  // or zombie.  If the lock is still valid, the function returns 1.  If
  // it is invalid, the function returns 0.

  int status = 0;
  kernelProcessState tmpState;
  
  // Make sure the pointer we were given is not NULL
  if (lock == NULL)
    return (status = ERR_NULLPARAMETER);

  // Make sure there's really a lock here
  if (*lock == 0)
    return (status = 0);

  // Get the current state of the owning process
  status = kernelMultitaskerGetProcessState(*lock, &tmpState);
      
  // Is the process that holds the lock still valid?
  if ((status < 0) || 
      (tmpState == sleeping) || (tmpState == stopped) ||
      (tmpState == finished) || (tmpState == zombie))
    {
      // This process either no longer exists, or else it shouldn't
      // continue holding this lock.
      return (status = 0);
    }
  else
    // It's a valid lock
    return (status = 1);
}
