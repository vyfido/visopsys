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
//  kernelExceptionHandler.c
//

// This file contains the C functions belonging to the kernel's 
// expection handler

#include "kernelExceptionHandler.h"
#include "kernelParameters.h"
#include "kernelMultitasker.h"
#include "kernelShutdown.h"
#include "kernelMiscAsmFunctions.h"
#include "kernelText.h"
#include "kernelError.h"
#include <string.h>


static kernelException exceptionArray[] = 
{
  { "divide by zero", faultException, 1 },
  { "debug", trapException, 1 },
  { "non-maskable interrupt", trapException, 1 },
  { "breakpoint", trapException, 1 },
  { "overflow", trapException, 1 },
  { "bound range exceeded", faultException, 1 },
  { "invalid opcode", faultException, 1 },
  { "device not available", faultException, 1 },
  { "double fault", abortException, 1 },
  { "coprocessor segment overrun", abortException, 1 },
  { "invalid TSS", faultException, 1 },
  { "segment not present", faultException, 1 },
  { "stack", faultException, 1 },
  { "general protection", faultException, 1 },
  { "page", faultException, 1 },
  { "reserved", unknownException, 1 },
  { "floating point error", faultException, 1 },
  { "alignment check", faultException, 1 },
  { "machine check", abortException, 1 }
};

static kernelException oopsException = { "UNKNOWN", unknownException, 1 };


int kernelExceptionHandlerInitialize(void)
{
  // This function will initialize the kernel's exception handler thread.  
  // It should be called after multitasking has been initialized.  

  int status = 0;


  // One of the first things the kernel does at startup time is to install 
  // a simple set of interrupt handlers, including ones for handling 
  // processor exceptions.  We want to replace those with a set of task
  // gates, so that a context switch will occur -- giving control to the
  // exception handler thread.

  // OK, we will now create the kernel's exception handler thread.

  // Return success
  return (status = 0);
}


void kernelExceptionHandler(int exceptionNumber)
{
  // This function will be called to handle all software exceptions
  // generated by the processor.  It must know about each type that
  // can be generated (this is processor-specific).

  // This will not be implemented right away, but it must be possible
  // for the kernel or applications to "trap" exceptions and handle them
  // themselves.  Otherwise, if an exception occurs and there is no
  // "trap" in place, the process that generated it will generally need
  // to be terminated.  In this case, we do a callback to the multitasker
  // to terminate the process.

  int status = 0;
  kernelException *currentException = NULL;
  int processId = 0;
  const char *processName = NULL;
  char messageString[EXMAXMESLEN];
  int count;

  
  // Initialize the error message string
  for (count = 0; count < EXMAXMESLEN; count ++)
    messageString[count] = '\0';

  // We need to make sure that the exception number we were passed
  // makes sense for this architecture
  if ((exceptionNumber >= 0) && (exceptionNumber < NUM_EXCEPTIONS))
    {
      // Make our currentException pointer point to the appropriate
      // exception information
      currentException = &exceptionArray[exceptionNumber];
    }
  else
    {
      // Oops.  The exception number is messed up.  This is serious.
      currentException = &oopsException;
    }

  // Get some information about the current process
  processId = kernelMultitaskerGetCurrentProcessId();

  if (processId < 0)
    {
      // We have to make an error here.  We can't return to the program
      // that caused the exception, and we can't tell the multitasker
      // to kill it.  We'd better make a kernel panic.
      kernelError(kernel_panic, INVALIDPROCESS_MESS);
      goto quick_stop;
    }

  // Use the process Id to get the process name
  processName = kernelMultitaskerGetProcessName(processId);

  // We need to make a kernelError to notify the user that the
  // exception has occurred.

  // First the name of the exception
  strcpy(messageString, currentException->name);

  // common middle stuff
  if (currentException->fatal)
    strcat(messageString, " exception (fatal) occurred in process \"");
  else
    strcat(messageString, " exception occurred in process \"");

  // The process name
  strcat(messageString, processName);

  strcat(messageString, "\"");

  // If the exception occurred in the kernel, we have to stop 
  // everything
  if (processId == KERNELPROCID)
    {
      // Send the error
      kernelError(kernel_panic, messageString);
      goto quick_stop;
    }

  else
    // Send the error
    kernelError((currentException->fatal? kernel_error: kernel_warn), 
		messageString);
    
  if (currentException->fatal)
    {
      // Make sure the process can be stopped.
      status = kernelMultitaskerSetProcessState(processId, stopped);

      if (status < 0)
	{
	  // This means we cannot stop the process.  This should be a kernel 
	  // panic, because we cannot be sure that this process won't be run 
	  // again.
	  kernelError(kernel_panic, CANNOTKILL_MESS);
	  goto nice_stop;
	}

      // We have to tell the multitasker to kill this process now.  We should 
      // never return from this call.
      kernelMultitaskerKillProcess(processId);
      while(1);
    }

  // It's not a fatal exception.  Return.
  return;

 nice_stop:
  // Try to do a graceful shutdown
  kernelMultitaskerShutdown(0 /* non-nice shutdown */);
  kernelShutdown(halt, 1 /* force */);
 quick_stop:
  // Just in case
  kernelTextPrintLine("stopped.");
  kernelSuddenStop();
}
