//
//  Visopsys
//  Copyright (C) 1998-2005 J. Andrew McLaughlin
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
#include "kernelMalloc.h"
#include "kernelMultitasker.h"
#include "kernelMiscFunctions.h"
#include "kernelPageManager.h"
#include "kernelError.h"
#include <string.h>
#include <stdio.h>

// This is the static list of file class registration functions.  If you
// add any to this, remember to update the LOADER_NUM_FILECLASSES value
// in the header file.
static kernelFileClass *(*classRegFns[LOADER_NUM_FILECLASSES]) (void) = {
  kernelFileClassElf,
  kernelFileClassBmp
};
static kernelFileClass *fileClassList[LOADER_NUM_FILECLASSES];
static int numFileClasses = 0;


static void *load(const char *filename, file *theFile, int kernel)
{
  // This function merely loads the named file into memory (kernel memory
  // if 'kernel' is non-NULL, otherwise user memory) and returns a pointer
  // to the memory.  The caller must deallocate the memory when finished
  // with the data

  int status = 0;
  void *fileData = NULL;
  
  // Make sure the filename and theFile isn't NULL
  if ((filename == NULL) || (theFile == NULL))
    {
      kernelError(kernel_error, "NULL filename or file structure");
      return (fileData = NULL);
    }

  // Initialize the file structure we're going to use
  kernelMemClear((void *) theFile, sizeof(file));

  // Now, we need to ask the filesystem driver to find the appropriate
  // file, and return a little information about it
  status = kernelFileFind(filename, theFile);
  if (status < 0)
    {
      // Don't make an official error.  Print a message instead.
      kernelError(kernel_error, "The file '%s' could not be found.",
		  filename);
      return (fileData = NULL);
    }

  // If we get here, that means the file was found.  Make sure the size
  // of the program is greater than zero
  if (theFile->size == 0)
    {
      kernelError(kernel_error, "File to load is empty (size is zero)");
      return (fileData = NULL);
    }

  // Get some memory into which we can load the program
  if (kernel)
    fileData = kernelMalloc(theFile->blocks * theFile->blockSize);
  else
    fileData = kernelMemoryGet((theFile->blocks * theFile->blockSize),
			       "file data");
  if (fileData == NULL)
    return (fileData);

  // We got the memory.  Now we can load the program into memory
  status = kernelFileOpen(filename, OPENMODE_READ, theFile);
  if (status < 0)
    {
      // Release the memory we allocated for the program
      if (kernel)
	kernelFree(fileData);
      else
	kernelMemoryRelease(fileData);
      return (fileData = NULL);
    }

  status = kernelFileRead(theFile, 0, theFile->blocks, fileData);
  if (status < 0)
    {
      // Release the memory we allocated for the program
      if (kernel)
	kernelFree(fileData);
      else
	kernelMemoryRelease(fileData);
      return (fileData = NULL);
    }

  return (fileData);
}


static void populateFileClassList(void)
{
  // Populate our list of file classes

  int count;
  
  for (count = 0; count < LOADER_NUM_FILECLASSES; count ++)
    fileClassList[numFileClasses++] = classRegFns[count]();
}

  
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
//  Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


void *kernelLoaderLoad(const char *filename, file *theFile)
{
  // This function merely loads the named file into memory and returns
  // a pointer to the memory.  The caller must deallocate the memory when
  // finished with the data

  // Make sure the filename and theFile isn't NULL
  if ((filename == NULL) || (theFile == NULL))
    {
      kernelError(kernel_error, "NULL filename or file structure");
      return (NULL);
    }

  return (load(filename, theFile, 0 /* not kernel */));
}


kernelFileClass *kernelLoaderGetFileClass(const char *className)
{
  // Given a file class name, try to find it in our list.  This is internal
  // for kernel use only.

  int count;

  // Has our list of file classes been initialized?
  if (!numFileClasses)
    populateFileClassList();

  // Determine the file's class
  for (count = 0; count < numFileClasses; count ++)
    {
      if (!strcmp(fileClassList[count]->className, className))
	return (fileClassList[count]);
    }
  
  // Not found
  return (NULL);
}


kernelFileClass *kernelLoaderClassify(void *fileData, loaderFileClass *class)
{
  // Given some file data, try to determine whether it is one of our known
  // file classes.

  int count;

  // Has our list of file classes been initialized?
  if (!numFileClasses)
    populateFileClassList();
  
  // Determine the file's class
  for (count = 0; count < numFileClasses; count ++)
    {
      if (fileClassList[count]->detect(fileData, class) == 1)
	return (fileClassList[count]);
    }

  // Not found
  return (NULL);
}


loaderSymbolTable *kernelLoaderGetSymbols(const char *fileName, int dynamic)
{
  // Given a file name, get symbols.

  loaderSymbolTable *symTable = NULL;
  void *loadAddress = NULL;
  file theFile;
  kernelFileClass *fileClassDriver = NULL;
  loaderFileClass class;

  // Check params
  if (fileName == NULL)
    {
      kernelError(kernel_error, "File name is NULL");
      return (symTable = NULL);
    }

  // Load the file data into memory
  loadAddress =
    (unsigned char *) load(fileName, &theFile, 1 /* kernel memory */);
  if (loadAddress == NULL)
    return (symTable = NULL);

  // Try to determine what kind of executable format we're dealing with.
  fileClassDriver = kernelLoaderClassify(loadAddress, &class);
  if (fileClassDriver == NULL)
    {
      kernelFree(loadAddress);
      return (symTable = NULL);
    }

  if (fileClassDriver->executable.getSymbols)
    // Get the symbols
    symTable = fileClassDriver->executable
      .getSymbols(loadAddress, dynamic, 0 /* not kernel */);

  kernelFree(loadAddress);
  return (symTable);
}


int kernelLoaderLoadProgram(const char *userProgram, int privilege,
			    int argc, char *argv[])
{
  // This takes the name of an executable to load and creates a process
  // image based on the contents of the file.  The program is not started
  // by this function.

  int status = 0;
  file theFile;
  void *loadAddress = NULL;
  kernelFileClass *fileClassDriver = NULL;
  loaderFileClass class;
  char procName[MAX_NAME_LENGTH];
  char tmp[MAX_PATH_NAME_LENGTH];
  int newProcId = 0;
  processImage execImage;
  int count;

  // Check params
  if (userProgram == NULL)
    {
      kernelError(kernel_error, "Program name to load is NULL");
      return (status = ERR_NULLPARAMETER);
    }
  if (argc && !argv)
    {
      kernelError(kernel_error, "Parameter list pointer is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  kernelMemClear(&execImage, sizeof(processImage));

  // Load the program code/data into memory
  loadAddress =
    (unsigned char *) load(userProgram, &theFile, 0 /* not kernel */);
  if (loadAddress == NULL)
    return (status = ERR_INVALID);

  // Try to determine what kind of executable format we're dealing with.
  fileClassDriver = kernelLoaderClassify(loadAddress, &class);
  if (fileClassDriver == NULL)
    {
      kernelMemoryRelease(loadAddress);
      return (status = ERR_INVALID);
    }

  // Make sure it's an executable
  if (!(class.flags & LOADERFILECLASS_EXEC))
    {
      kernelError(kernel_error, "File \"%s\" is not an executable program",
		  userProgram);
      kernelMemoryRelease(loadAddress);
      return (status = ERR_PERMISSION);
    }

  // We may need to do some fixup or relocations
  if (fileClassDriver->executable.layoutExecutable)
    {
      status = fileClassDriver->executable
	.layoutExecutable(loadAddress, &execImage);
      if (status < 0)
	{
	  kernelMemoryRelease(loadAddress);
	  return (status);
	}
    }

  // Just get the program name without the path in order to set the process
  // name
  status = kernelFileSeparateLast(userProgram, tmp, procName);
  if (status < 0)
    strncpy(procName, userProgram, MAX_NAME_LENGTH);

  // Set up arguments
  execImage.argc = argc;
  for (count = 0; count < argc; count ++)
    execImage.argv[count] = argv[count];

  // Set up and run the user program as a process in the multitasker
  newProcId = kernelMultitaskerCreateProcess(procName, privilege, &execImage);
  if (newProcId < 0)
    {
      // Release the memory we allocated for the program
      kernelMemoryRelease(loadAddress);
      kernelMemoryRelease(execImage.code);
      return (newProcId);
    }

  // Unmap the new process' image memory from this process' address space.
  status = kernelPageUnmap(kernelCurrentProcess->processId, execImage.code,
			   execImage.imageSize);
  if (status < 0)
    kernelError(kernel_warn, "Unable to unmap new process memory from current "
		"process");

  // Get rid of the old file memory
  kernelMemoryRelease(loadAddress);

  // All set.  Return the process id.
  return (newProcId);
}


int kernelLoaderExecProgram(int processId, int block)
{
  // This is a convenience function for executing a program loaded by
  // the kernelLoaderLoadProgram function.  The calling function could
  // easily accomplish this stuff by talking to the multitasker.  If
  // blocking is requested, the exit code of the program is returned to
  // the caller.
  
  int status = 0;
  
  // Start user program's process
  status = kernelMultitaskerSetProcessState(processId, proc_ready);
  if (status < 0)
    return (status);

  // Now, if we are supposed to block on this program, we should make
  // the appropriate call to the multitasker
  if (block)
    {
      status = kernelMultitaskerBlock(processId);

      // ...Now we're waiting for the program to terminate...

      // Return the exit code from the program
      return (status);
    }
  else
    // Return successfully
    return (status = 0);
}


int kernelLoaderLoadAndExec(const char *progName, int privilege,
			    int argc, char *argv[], int block)
{
  // This is a convenience function that just calls the
  // kernelLoaderLoadProgram and kernelLoaderExecProgram functions for the
  // caller.

  int processId = 0;
  int status = 0;

  processId = kernelLoaderLoadProgram(progName, privilege, argc, argv);

  if (processId < 0)
    return (processId);

  status = kernelLoaderExecProgram(processId, block);

  // All set
  return (status);
}
