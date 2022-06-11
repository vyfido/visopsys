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
//  kernelLoader.c
//
	
// This file contains the functions belonging to the kernel's executable
// program loader.


#include "kernelLoader.h"
#include "kernelFile.h"
#include "kernelMemoryManager.h"
#include "kernelMultitasker.h"
#include "kernelMiscAsmFunctions.h"
#include "kernelParameters.h"
#include "kernelError.h"
#include <sys/file.h>
#include <sys/errors.h>
#include <string.h>


int kernelLoaderLoadAndExec(const char *userProgram, int argc, 
			    char *argv[], int block)
{
  // This function is the main routine exported by the kernel's built-in
  // loader.  It takes the name of an executable to load and execute.  
  // If it is unable to load and/or execute the program, it returns a
  // negative error code.  Otherwise, it returns whatever status is 
  // returned by the program, if any.

  int status = 0;
  file theFile;
  unsigned char *loadAddress = NULL;
  int newProcId = 0;
  int argSpaceSize = 0;
  void *argSpace = NULL;
  void *newArgAddress = NULL;
  int count, length; 

  struct
  {
    int argc;
    char **argv;

  } argStruct;


  // We need to make sure neither of the arguments are NULL

  if (userProgram == NULL)
    {
      // Make an error
      kernelError(kernel_error,
		  "The loader received a NULL function parameter");
      return (status = ERR_NULLPARAMETER);
    }

  // kernelTextPrint("Loader looking for user program \"");
  // kernelTextPrint(userProgram);
  // kernelTextPrintLine("\"");

  // Initialize the file structure we're going to use
  kernelMemClear((void *) &theFile, sizeof(file));

  // Now, we need to ask the filesystem driver to find the appropriate
  // file, and return a little information about it
  status = kernelFileFind(userProgram, &theFile);

  if (status < 0)
    {
      // Don't make an official error.  Print a message instead.
      kernelError(kernel_error, "The file '%s' could not be found.",
		  userProgram);
      return (status);
    }

  // If we get here, that means the file was found.  Make sure the size
  // of the program is greater than zero
  if (theFile.size == 0)
    {
      kernelError(kernel_error, "Program to load is empty (size is zero)");
      return (status = ERR_NODATA);
    }

  // Get some memory into which we can load the program
  loadAddress = (unsigned char *) 
    kernelMemoryRequestBlock((theFile.blocks * theFile.blockSize), 
			     0, "process code/data");

  if (loadAddress == NULL)
    {
      // Make an error
      kernelError(kernel_error,
	  "There was not enough memory for the loader to load this program");
      return (status = ERR_MEMORY);
    }

  // We got the memory.  Now we can load the program into memory
  status = kernelFileOpen(userProgram, OPENMODE_READ, &theFile);
  
  if (status < 0)
    {
      // Release the memory we allocated for the program
      kernelMemoryReleaseByPointer(loadAddress);
      // Make an error
      kernelError(kernel_error, "The loader could not load this program");
      return (status);
    }

  status = kernelFileRead(&theFile, 0, theFile.blocks, loadAddress);

  if (status < 0)
    {
      // Release the memory we allocated for the program
      kernelMemoryReleaseByPointer(loadAddress);
      // Make an error
      kernelError(kernel_error, "The loader could not load this program");
      return (status);
    }

  // By default, pass no arguments
  argStruct.argc = 0;
  argStruct.argv = NULL;

  // Set up and run the user program as a process in the multitasker
  newProcId = kernelMultitaskerCreateProcess(loadAddress, theFile.size,
					     userProgram, 2, &argStruct);

  if (newProcId < 0)
    {
      // Release the memory we allocated for the program
      kernelMemoryReleaseByPointer(loadAddress);
      // Make an error
      kernelError(kernel_error,
		  "The loader could not create a process for this program");
      return (newProcId);
    }

  // If there were any arguments, we need to pass them on the new
  // process' stack

  if (argc > 0)
    {
      argSpaceSize = ((argc + 1) * sizeof(char *));
      for (count = 0; count < argc; count ++)
	argSpaceSize += (strlen(argv[count]) + 1);

      argStruct.argc = argc;
      argStruct.argv =
	kernelMemoryRequestBlock(argSpaceSize, 0, "argument space");

      if (argStruct.argv != NULL)
	{
	  // Make the new process own the memory
	  status =
	    kernelMemoryChangeOwner(kernelMultitaskerGetCurrentProcessId(),
			    newProcId, 1, argStruct.argv, &newArgAddress);

	  if (status >= 0)
	    {
	      // Share the memory back with this process
	      status =
		kernelMemoryShare(newProcId,
				  kernelMultitaskerGetCurrentProcessId(),
				  newArgAddress, (void *) &(argStruct.argv));

	      if (status >= 0)
		{
		  // Leave space for pointers to the strings
		  argSpace = (argStruct.argv + ((argc + 1) * sizeof(char *)));
		  
		  for (count = 0; count < argc; count ++)
		    {
		      length = strlen(argv[count]);
		      strncpy(argSpace, argv[count], length);
		      argStruct.argv[count] = argSpace;
		      argSpace += (length + 1);
		      // kernelTextPrintLine(argStruct.argv[count]);
		      // Adjust the pointers in argv so that they refer to the
		      // new process' address space
		      argStruct.argv[count] -= (unsigned int) argStruct.argv;
		      argStruct.argv[count] += (unsigned int) newArgAddress;
		    }

		  // argv[argc] is supposed to be a NULL pointer, according
		  // to the standard
		  argStruct.argv[argc] = NULL;
	      
		  // Adjust argv so that it refers to the new process' address
		  // space
		  argStruct.argv = newArgAddress;

		  // Finally, pass argc and argv to the new process
		  status =
		    kernelMultitaskerPassArgs(newProcId, 2, &argStruct);

		  if (status < 0)
		    kernelError(kernel_warn, "Unable to pass arguments");
		}
	      else
		kernelError(kernel_warn, "Unable to share argument space");
	    }
	  else
	    kernelError(kernel_warn,
			"Unable to make new process own argument space");
	}
      else
	kernelError(kernel_warn, "Unable to allocate argument space");
    }

  // Start user program's new process
  status = kernelMultitaskerSetProcessState(newProcId, ready);

  if (status < 0)
    {
      // Kill the process.  This will release all of the memory it owns.
      kernelMultitaskerKillProcess(newProcId);
      if (argc > 0)
	// Release the arg space memory
	kernelMemoryReleaseByPointer(argStruct.argv);
      // Make an error
      kernelError(kernel_error, "The loader could not execute this program");
      return (status);
    }

  // Now, if we are supposed to block on this program, we should make
  // the appropriate call to the multitasker
  if (block)
    {
      status = kernelMultitaskerBlock(newProcId);

      // Return the status code from the program.
      return (status);
    }
  else
    {
      // Return the process Id of the program
      return (newProcId);
    }
}
