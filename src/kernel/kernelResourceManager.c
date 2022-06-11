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
//  kernelResourceManager.c
//

// This header file contains source code for the kernel's standard 
// resource management facilities (resource locking, etc.).  These facilities
// can be used for locking any desired resource (i.e. it is not specific
// to devices, or anything in particular).

#include "kernelResourceManager.h"
#include "kernelMultitasker.h"
#include <sys/errors.h>
#include <string.h>


int kernelResourceManagerLock(volatile int *lock)
{
  // This function is used to lock a resource for exclusive use by a 
  // particular process.  If the resource is not being used, the lock is
  // granted, and the requesting process can proceed to use the resource.  

  // If a lock is already in place by another process at the time of the 
  // request, this routine will go into a multitasker yield() loop until 
  // the lock can be obtained (up to a maximum of RESOURCE_MAX_SPINS) times.  

  // As a safeguard, this loop will make sure that the other process is
  // still viable (i.e. it still exists, and is not stopped or zombie).

  // This yielding loop serves the dual purpose of maintaining exclusivity
  // and also allows the first process to terminate more quickly, since it
  // will not be contending with other resource-bound processes for processor
  // time.

  // The int* argument passed to the function must be a pointer to a
  // single "lock" flag for the resource (shared by all requesting
  // processes).  The lock flag should be zero when the resource is not
  // locked.

  // This lock flag is read to check for existing locks, and when a lock
  // is granted it is set to the process Id of the locking function.  The
  // function returns 0 if successful, and negative on error.

  int status = 0;
  // volatile int spins = 0;
  int currentProcess = 0;

  // These are for priority inversion
  int myPriority = 0;
  volatile int holder = 0;
  volatile int holderPriority = 0;
  volatile int inversion = 0;


  // Make sure the pointer we were given is not NULL
  if (lock == NULL)
    return (status = ERR_NULLPARAMETER);

  // Get the process Id of the current process
  currentProcess = kernelMultitaskerGetCurrentProcessId();

  // Make sure it's a valid process Id
  if (currentProcess < 0)
    return (currentProcess);

  // Check to see whether another lock is already in place, and whether
  // that process is actually the current one
  if (*lock == currentProcess)
    // This process already owns the resource
    return (status = 0);

  // This label allows us to jump back in case the loop below exits
  // prematurely.  This is necessary to prevent "simultaneous" granting
  // of a resource to multiple processes -- see the comments at the bottom
  // of the loop near the "goto" statement.

 waitForLock:

  while(*lock != 0)
    {
      // This is the loop of death, where the requesting process will live
      // until it is allowed to use the resource

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

      // If priority inversion was done in a previous loop, it is possible
      // that a new, DIFFERENT process has locked the resource in the
      // meantime.  If that's the case, we need to reset the priority
      // of the process we previously inverted.
      if ((inversion) && (*lock != holder))
	{
	  kernelMultitaskerSetProcessPriority(holder, holderPriority);
	  holder = 0;
	  inversion = 0;
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

	  myPriority = kernelMultitaskerGetProcessPriority(currentProcess);
	  holderPriority = kernelMultitaskerGetProcessPriority(*lock);

	  if ((myPriority >= 0) && (holderPriority >= 0))
	    if (holderPriority < myPriority)
	      {
		holder = *lock;
		kernelMultitaskerSetProcessPriority(*lock, myPriority);
		inversion = 1;

		// kernelTextNewline();
		// kernelTextPrintLine(
		// "Priority inversion invoked");
	      }
	}

      // This process will now have to continue waiting until the lock has 
      // been released or becomes invalid
      
      /*
      // Increase our counter on the number of spins
      spins++;

      // Have we reached the maximum number of spins?
      if (spins >= RESOURCE_MAX_SPINS)
	{
	  // We've been spinning for too long.  Deny the lock to help
	  // avoid deadlock conditions.

	  // Undo any priority inversions first
	  if (inversion)
	    {
	      kernelMultitaskerSetProcessPriority(holder, holderPriority);
	      holder = 0;
	      inversion = 0;
	    }

	  kernelError(kernel_error, "Deadlock prevention");
	  return (status = ERR_DEADLOCK);
	}
      */

      // yield this time slice back to the scheduler while the process
      // waits for the lock
      kernelMultitaskerYield();
	  
      // Loop again
      continue;
    }

  // If we ever fall through to here, that means we can *ALMOST* give the 
  // lock to the requesting process.  But wait: it is technically possible
  // for two processes to get here at the "same time" (because of the
  // possibility that the scheduler will interrupt us before the next
  // instruction completes).  What we will do here is attempt to check
  // for a preexisting lock one more time.  Hopefully, because the 
  // granularity of the scheduler's timeslices should be greater than one 
  // or two instructions, we will then be able to avoid simultaneous locks.

  if (*lock == 0)
    {
      // Give the lock to the requesting process
      *lock = currentProcess;

      // If we inverted the priority of some process to achieve this lock,
      // we need to reset it to its old priority
      if (inversion)
	{
	  kernelMultitaskerSetProcessPriority(holder, holderPriority);
	  holder = 0;
	  inversion = 0;
	}
    }

  else
    // Crap.  Someone else grabbed it first.  We will enter the wait
    // loop again
    goto waitForLock;

  // kernelTextPrint("Lock granted to process ");
  // kernelTextPrintInteger(currentProcess);
  // kernelTextNewline();

  return (status = 0);
}


int kernelResourceManagerUnlock(volatile int *lock)
{
  // This function corresponds to the lock function.  It enables a 
  // process to release a resource that it had previously locked.

  int status = 0;
  int currentProcess = 0;

  
  // Make sure the pointer we were given is not NULL
  if (lock == NULL)
    return (status = ERR_NULLPARAMETER);

  // Get the process Id of the current process
  currentProcess = kernelMultitaskerGetCurrentProcessId();

  // Make sure it's a valid process Id
  if (currentProcess < 0)
    return (currentProcess);
  
  // Make sure that the current disk lock, if any, really belongs
  // to this process.  This prevents any trickery that might be possible
  // if one process could unlock a disk belonging to another process

  if (*lock == currentProcess)
    {
      *lock = 0;

      // kernelTextPrint("Lock released by process ");
      // kernelTextPrintInteger(currentProcess);
      // kernelTextNewline();

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
      (tmpState == sleeping) || 
      (tmpState == stopped) || 
      (tmpState == zombie))
    {
      // This process either no longer exists, or it has no right to
      // continue holding this lock.
      return (status = 0);
    }
  else
    // It's a valid lock
    return (status = 1);
}
