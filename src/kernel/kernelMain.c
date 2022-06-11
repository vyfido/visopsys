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
//  kernelMain.c
//
	
#include "kernelMain.h"
#include "kernelParameters.h"
#include "kernelInitialize.h"
#include "kernelText.h"
#include "kernelSysTimerFunctions.h"
#include "kernelMultitasker.h"
#include "kernelMiscFunctions.h"
#include "kernelMiscAsmFunctions.h"
#include "kernelShutdown.h"
#include "kernelError.h"


// This is the global 'errno' variable that will be set on error by
// any standard library functions we use.  We need to declare it here
// because the kernel doesn't use the start.o code used by applications
int errno = 0;

// This is a variable that is checked by the standard library before calling
// any kernel API functions.  This helps to prevent any API functions from
// being called from within the kernel (which is bad).  For example, it is
// permissable to use sprintf() inside the kernel, but not printf().  This
// should help to catch mistakes.
int visopsys_in_kernel = 1;


void kernelMain(int bootDevice, unsigned int kernelMemory,
		loaderInfoStruct *info)
{

  // This is the kernel entry point -- and main routine --
  // which starts the entire show and, of course, never returns.

  int status = 0;
  loaderInfoStruct systemInfo;


  // Copy the loaderHardware structure we were passed into kernel memory
  kernelMemCopy(info, &systemInfo, sizeof(loaderInfoStruct));

  // Call the kernel initialization routine
  status = kernelInitialize(bootDevice, kernelMemory, &systemInfo);
  
  if (status < 0)
    {
      // Kernel initialization failed.  Crap.  We don't exactly know
      // what failed.  That makes it a little bit risky to call the 
      // error routine, but we'll do it anyway.

      // Make the error
      kernelError(kernel_error, "Initialization failed.  Press any key (or the \"reset\" button) to reboot.");

      // Do a loop, manually polling the keyboard input buffer
      // looking for the key press to reboot.
      while (kernelTextInputCount() == 0);

      kernelTextPrint("Rebooting...");
      kernelSysTimerWaitTicks(20); // Wait 2 seconds
      kernelSuddenReboot();
    }

  // Launch a login process 
  kernelConsoleLogin();

  // Finally, we will change the kernel state to 'sleeping'.  This is
  // done because there's nothing that needs to be actively done by
  // the kernel process itself; it just needs to remain resident in
  // memory.  Changing to a 'sleeping' state means that it won't get invoked
  // again by the scheduler.
  status = kernelMultitaskerSetProcessState(KERNELPROCID, sleeping);

  if (status < 0)
    kernelError(kernel_error, "The kernel process could not go to sleep.");

  // Yield the rest of this time slice back to the scheduler
  kernelMultitaskerYield();

  // If we ever get here, something went wrong.  We're rebooting.

  kernelError(kernel_panic, "The kernel process was unexpectedly woken up");

  kernelShutdown(reboot, 1); // try this first (nice)
  kernelShutdown(reboot, 0); // now try this (not nice)
  while(1); // Give up.

  // Just for good form, and so the compiler never complains:
  return;
}
