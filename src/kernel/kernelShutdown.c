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
//  kernelShutdown.c
//

// This code is responsible for an orderly shutdown and/or reboot of
// the kernel

#include "kernelShutdown.h"
#include "kernelText.h"
#include "kernelMultitasker.h"
#include "kernelLog.h"
#include "kernelFilesystem.h"
#include "kernelSysTimerFunctions.h"
#include "kernelDiskFunctions.h"
#include "kernelMiscAsmFunctions.h"
#include "kernelError.h"
#include <string.h>


int kernelShutdown(kernelShutdownType shutdownType, int force)
{
  // This function will shut down the kernel, and reboot the computer
  // if the shutdownType argument dictates.  This function must include
  // shutdown invocations for all of the major subsystems that support
  // and/or require such activity

  int status = 0;


  // Echo the appropriate message(s)
  kernelTextPrintLine("\nShutting down Visopsys, please wait...");
  if (shutdownType == halt)
    kernelTextPrintLine("[ Wait for \"OK to power off\" message ]");

  // Kill all the processes, except this one and the kernel.
  kernelLog("Stopping all processes");
  status = multitaskerKillAllProcesses();

  if ((status < 0) && (!force))
    {
      // Eek.  We couldn't kill the processes nicely
      kernelError(kernel_error,
		  "Unable to stop processes nicely.  Shutdown aborted.");
      return (status);
    }


  // Synchronize all filesystems
  kernelLog("Synchronizing filesystems");
  status = kernelFilesystemSync(NULL);

  if ((status < 0) && (!force))
    {
      // Eek.  We couldn't synchronize the filesystems.  We should
      // stop and allow the user to try to save their data
      kernelError(kernel_error, NO_SHUTDOWN_FS);
      return (status);
    }


  // Shut down the multitasker
  status = kernelMultitaskerShutdown(1 /* nice shutdown */);

  if (status < 0)
    {
      if (!force)
	{
	  // Abort the shutdown
	  kernelError(kernel_error,
		      "Unable to stop multitasker.  Shutdown aborted.");
	  return (status);
	}
      else
	{
	  // Attempt to shutdown the multitasker without the 'nice' flag.
	  // We won't bother to check whether this was successful
	  kernelMultitaskerShutdown(0 /* NOT nice shutdown */);
	}
    }

  // After this point, don't abort.  We're running the show.

  // Shut down kernel logging
  kernelLog("Stopping kernel logging");

  status = kernelLogShutdown();
  
  if (status < 0)
    kernelError(kernel_error, "The kernel logger could not be stopped.");

  // Unmount all filesystems.
  kernelLog("Unmounting filesystems");

  status = kernelFilesystemUnmountAll();
    
  if (status < 0)
    kernelError(kernel_error,
		"The filesystems were not all unmounted successfully");


  // Power off any removable disk drives
  kernelDiskFunctionsMotorOff(0);


  // Clear any pending scheduled events
  kernelLog("Dispatching all pending timed events");
  kernelTimedEventDispatchAll();


  // Finally, we either halt or reboot the computer
  if (shutdownType == reboot)
    {
      kernelTextPrint("\nRebooting\n");
      kernelSysTimerWaitTicks(20); // Wait ~2 seconds
      kernelSuddenReboot();
    }
  else
    {
      kernelTextPrint("\nOK to power off now.");
      kernelSuddenStop();
    }

  // Just for good measure
  return (status);
}
