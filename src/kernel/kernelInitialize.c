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
//  kernelInitialize.c
//

#include "kernelInitialize.h"
#include "kernelParameters.h"
#include "kernelMiscFunctions.h"
#include "kernelLog.h"
#include "kernelError.h"
#include "kernelInterruptsInit.h"
#include "kernelHardwareEnumeration.h"
#include "kernelDriverManagement.h"
#include "kernelRandom.h"
#include "kernelMemoryManager.h"
#include "kernelDescriptor.h"
#include "kernelPageManager.h"
#include "kernelMultitasker.h"
#include "kernelExceptionHandler.h"
#include "kernelFilesystem.h"
#include <sys/errors.h>


int kernelInitialize(int bootDevice, unsigned int kernelMemory, 
		     loaderInfoStruct *info)
{
  // Does a bunch of calls involved in initializing the kernel.
  // kernelMain passes all of the kernel's arguments to this function
  // for processing.  Returns 0 if successful, negative on error.

  int status;
  int rootFilesystemId = -1;
  // void *foo = NULL;


  // Initialize the page manager
  status = kernelPageManagerInitialize(kernelMemory);
  if (status < 0)
    return (status);

  // Initialize the memory manager
  status = kernelMemoryInitialize(kernelMemory, info);
  if (status < 0)
    return (status);

  // Initialize the text screen output.  This needs to be done after paging
  // has been initialized so that our screen memory can be mapped to a
  // virtual memory address.
  status = kernelTextInitialize(80, 50);
  if (status < 0)
    return (status);

  // Initialize kernel logging
  status = kernelLogInitialize();
  if (status < 0)
    return (status);

  // Now that text input/output has been initialized, we can print the
  // welcome message.  Then turn off console logging.
  kernelLog(kernelVersion());
  kernelLog("Copyright (C) 1998-2001 J. Andrew McLaughlin");
  kernelTextPrintLine("Starting, one moment please...");

  // Disable console logging after this point, since it fills up the screen
  // with unnecessary details.
  kernelLogSetToConsole(0);

  // Initialize the error output early, for the best diagnostic results.
  status = kernelErrorInitialize();
  if (status < 0)
    return (status);

  // Initialize the descriptor tables (GDT and IDT)
  status = kernelDescriptorInitialize();
  if (status < 0)
    {
      kernelError(kernel_error, "Descriptor table initialization failed");
      return (status);
    }

  // Initialize the interrupt vector tables and default handlers.  Note
  // that interrupts are not enabled here; that is done during hardware
  // enumeration after the Programmable Interrupt Controller has been
  // set up.
  status = kernelInterruptVectorsInstall();
  if (status < 0)
    {
      kernelError(kernel_error, "Interrupt vector initialization failed");
      return (status);
    }

  // Initialize the kernel's driver management routines before we
  // go calling any driver functions.
  status = kernelDriverManagementInitialize();
  if (status < 0)
    {
      kernelError(kernel_error,
		  "Device driver manager initialization failed");
      return (status);
    }

  // Pass the loader's info structure to the hardware enumeration
  // routine
  status = kernelHardwareEnumerate(info);
  if (status < 0)
    {
      kernelError(kernel_error, "Hardware initialization failed");
      return (status);
    }

  // Initialize the kernel's random number generator.
  // has been initialized.
  status = kernelRandomInitialize();
  if (status < 0)
    {
      kernelError(kernel_error, "Random number initialization failed");
      return (status);
    }

  // Initialize the multitasker
  status = kernelMultitaskerInitialize();
  if (status < 0)
    {
      kernelError(kernel_error, "Multitasker initialization failed");
      return (status);
    }

  // Initialize the new exception handler process, now that multitasking
  // is enabled.
  status = kernelExceptionHandlerInitialize();
  if (status < 0)
    {
      kernelError(kernel_error, "Exception handler initialization failed");
      return (status);
    }

  // Initialize the disk functions.  This must be done AFTER the hardware
  // has been enumerated, and AFTER the drivers have been installed.
  status = kernelDiskFunctionsInitialize();
  if (status < 0)
    {
      kernelError(kernel_error, "Disk functions initialization failed");
      return (status);
    }

  // Initialize the filesystem manager
  status = kernelFilesystemInitialize();
  if (status < 0)
    {
      kernelError(kernel_error, "Filesystem functions initialization failed");
      return (status);
    }

  // Mount the root filesystem
  rootFilesystemId = kernelFilesystemMount(bootDevice, "/");
  if (rootFilesystemId < 0)
    {
      kernelError(kernel_error, "Mounting root filesystem failed");
      return (status = ERR_NOTINITIALIZED);
    }

  // Open a kernel log file
  status = kernelLogSetFile(DEFAULT_LOGFILE);
  if (status < 0)
    // Make a warning, but don't return error.  This is not fatal.
    kernelError(kernel_warn, "Unable to open the kernel log file");

  // Done setup.  Return success.
  return (status = 0);
}
