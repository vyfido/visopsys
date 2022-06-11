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
//  kernelMain.c
//
	
#include "kernelMain.h"
#include "kernelParameters.h"
#include "kernelInitialize.h"
#include "kernelSysTimer.h"
#include "kernelMultitasker.h"
#include "kernelMalloc.h"
#include "kernelLoader.h"
#include "kernelMiscFunctions.h"
#include "kernelProcessorX86.h"
#include "kernelShutdown.h"
#include "kernelError.h"
#include <string.h>


// This is a variable that is checked by the standard library before calling
// any kernel API functions.  This helps to prevent any API functions from
// being called from within the kernel (which is bad).  For example, it is
// permissable to use sprintf() inside the kernel, but not printf().  This
// should help to catch mistakes.
extern int visopsys_in_kernel;

// General kernel configuration variables
variableList *kernelVariables = NULL;


void kernelMain(unsigned kernelMemory, loaderInfoStruct *info)
{

  // This is the kernel entry point -- and main routine --
  // which starts the entire show and, of course, never returns.

  int status = 0;
  int pid = -1;
  loaderInfoStruct systemInfo;
  char startProgram[128];

  visopsys_in_kernel = 1;

  // Copy the loaderHardware structure we were passed into kernel memory
  kernelMemCopy(info, &systemInfo, sizeof(loaderInfoStruct));

  // Call the kernel initialization routine
  status = kernelInitialize(kernelMemory, &systemInfo);
  if (status < 0)
    {
      // Kernel initialization failed.  Crap.  We don't exactly know
      // what failed.  That makes it a little bit risky to call the 
      // error routine, but we'll do it anyway.

      kernelError(kernel_error, "Initialization failed.  Press any key "
		  "(or the \"reset\" button) to reboot.");

      // Do a loop, manually polling the keyboard input buffer
      // looking for the key press to reboot.
      while (kernelTextInputCount() == 0);

      kernelTextPrint("Rebooting...");
      kernelSysTimerWaitTicks(20); // Wait 2 seconds
      kernelProcessorReboot();
    }

  kernelVariables = kernelConfigurationReader(DEFAULT_KERNEL_CONFIG);
  if (kernelVariables != NULL)
    {
      // Find out which initial program to launch
      kernelVariableListGet(kernelVariables, "start.program", startProgram,
			    128);

      // If the start program is our standard login program, use a custom
      // function to launch a login process 
      if (strncmp(startProgram, DEFAULT_KERNEL_STARTPROGRAM, 128))
	{
	  // Try to load the login process
	  pid = kernelLoaderLoadProgram(startProgram, PRIVILEGE_SUPERVISOR,
					0,     // no args
					NULL); // no args
	  if (pid < 0)
	    // Don't fail, but make a warning message
	    kernelError(kernel_warn, "Couldn't load start program \"%s\"",
			startProgram);
	  else
	    {
	      // Attach the start program to the console text streams
	      kernelMultitaskerDuplicateIO(KERNELPROCID, pid, 1); // Clear
	      
	      // Execute the start program.  Don't block.
	      kernelLoaderExecProgram(pid, 0);
	    }
	}
    }

  // If the kernel config file wasn't found, or the start program wasn't
  // specified, or loading the start program failed, assume we're going to
  // use the standard default login program
  if (pid < 0)
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

  // If we ever get here, something went wrong.  We should never get here.
  // We're stopping.
  kernelPanic("The kernel process was unexpectedly woken up");
  return; // Compiler nice
}
