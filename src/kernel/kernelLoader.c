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
//  kernelLoader.c
//
	
// This file contains the functions belonging to the kernel's executable
// program loader.


#include "kernelLoader.h"
#include "kernelExecutableFormatElf.h"
#include "kernelFile.h"
#include "kernelMemoryManager.h"
#include "kernelMultitasker.h"
#include "kernelMiscFunctions.h"
#include "kernelError.h"
#include <sys/errors.h>
#include <string.h>


static int setupElfExecutable(unsigned char *loadAddress)
{
  // This function is for preparing an ELF executable image to run.

  int status = 0;
  Elf32Header *header = (Elf32Header *) loadAddress;
  Elf32ProgramHeader *programHeader = NULL;
  Elf32_Off codeOffset, dataOffset;
  Elf32_Addr codeVirtualAddress, dataVirtualAddress;
  Elf32_Word codeSizeInFile, dataSizeInFile, dataSizeInMem;

  // We will assume we this function is not called unless the loader is
  // already sure that this file is ELF.  Thus, we will not check the magic
  // number stuff at the head of the file again.

  // Check to make aure it's an executable file
  if (header->e_type != (Elf32_Half) 2)
    {
      kernelError(kernel_error, "ELF file is not executable");
      return (status = ERR_INVALID);
    }

  // Check the code entry point.  Should be zero
  if (header->e_entry != (Elf32_Addr) 0)
    {
      kernelError(kernel_error, "ELF entry point is not zero");
      return (status = ERR_INVALID);
    }

  // Make sure there are 2 program header entries; 1 for code and 1 for data
  if (header->e_phnum != (Elf32_Half) 2)
    {
      kernelError(kernel_error, "Invalid number of ELF program header "
		  "entries (%d)", (int) header->e_phnum);
      return (status = ERR_INVALID);
    }

  // Get the address of the program header
  programHeader = (Elf32ProgramHeader *) (loadAddress + header->e_phoff);

  // Skip the segment type.

  // Get the code offset and virtual address
  codeOffset = programHeader->p_offset;
  codeVirtualAddress = programHeader->p_vaddr;

  // Skip the physical address.

  codeSizeInFile = programHeader->p_filesz;

  // Make sure that the code size in the file is the same as the size in
  // memory
  if (codeSizeInFile != programHeader->p_memsz)
    {
      kernelError(kernel_error, "Invalid ELF program (code filesz != memsz)");
      return (status = ERR_INVALID);
    }

  // Just check the alignment.  Must be the same as our page size
  if (programHeader->p_align &&
      (programHeader->p_align != MEMORY_PAGE_SIZE))
    {
      kernelError(kernel_error, "Invalid ELF program (code p_align != "
		  "MEMORY_PAGE_SIZE)");
      return (status = ERR_INVALID);
    }

  // Now we skip to the data segment
  programHeader = (Elf32ProgramHeader *) (loadAddress + header->e_phoff +
					  header->e_phentsize);
  
  // Skip the segment type.

  // Get the data offset and virtual address
  dataOffset = programHeader->p_offset;
  dataVirtualAddress = programHeader->p_vaddr;

  // Skip the physical address.

  dataSizeInFile = programHeader->p_filesz;
  dataSizeInMem = programHeader->p_memsz;

  // Check the alignment.  Must be the same as our page size
  if (programHeader->p_align &&
      (programHeader->p_align != MEMORY_PAGE_SIZE))
    {
      kernelError(kernel_error, "Invalid ELF program (data p_align != "
		  "MEMORY_PAGE_SIZE)");
      return (status = ERR_INVALID);
    }

  // We will do layout for two segments; the code and data segments

  // For the code segment, we simply place it at the entry point.  The
  // entry point, in physical memory, should be at the start of the image.
  // Thus, all we do is move all code forward by codeOffset bytes.
  // This will have the side effect of deleting the ELF header and
  // program headers from memory.

  kernelMemCopy((loadAddress + codeOffset), loadAddress, codeSizeInFile);

  // We do the same operation for the data segment, except we have to
  // first make sure that the difference between the code and data's
  // virtual address is the same as the difference between the offsets
  // in the file.
  if ((dataVirtualAddress - codeVirtualAddress) != dataOffset)
    {
      // The  image doesn't look exactly the way we expected, but that
      // can happen depending on which linker is used.  We can adjust
      // it.  Move the initialized data forward from the original offset
      // so that it matches the difference between the code and data's
      // virtual addresses.
      kernelMemCopy((loadAddress + dataOffset),
		    (loadAddress + (dataVirtualAddress - codeVirtualAddress)),
		    dataSizeInFile);
      // The data offset will be different now
      dataOffset = (dataVirtualAddress - codeVirtualAddress);
    }

  // We need to zero out the memory that makes up the difference
  // between the data's file size and its size in memory.
  kernelMemClear((loadAddress + dataOffset + dataSizeInFile),
		 (dataSizeInMem - dataSizeInFile));

  // Success
  return (status = 0);
}


static int processExecutableType(unsigned char *loadAddress)
{
  // This function will attempt to determine the program's executable type
  // and do whatever is necessary to make it ready to go

  int status = 0;

  if ((loadAddress[0] == 0x7F) &&
      !strncmp((loadAddress + 1), "ELF", 3))
    {
      // This program is an ELF binary.  Set up for that
      status = setupElfExecutable(loadAddress);
      return (status);
    }

  // Otherwise, we assume 'binary' format, which we do nothing to
  return (status = 0);
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
  fileData = kernelMemoryGet((theFile->blocks * theFile->blockSize), 
			     "file data");
  if (fileData == NULL)
    {
      // Make an error
      kernelError(kernel_error, "There was not enough memory for the "
		  "loader to load this file");
      return (fileData = NULL);
    }

  // We got the memory.  Now we can load the program into memory
  status = kernelFileOpen(filename, OPENMODE_READ, theFile);
  if (status < 0)
    {
      // Release the memory we allocated for the program
      kernelMemoryRelease(fileData);
      // Make an error
      kernelError(kernel_error, "The loader could not load this file");
      return (fileData = NULL);
    }

  status = kernelFileRead(theFile, 0, theFile->blocks, fileData);
  if (status < 0)
    {
      // Release the memory we allocated for the program
      kernelMemoryRelease(fileData);
      // Make an error
      kernelError(kernel_error, "The loader could not load this file");
      return (fileData = NULL);
    }

  return (fileData);
}


int kernelLoaderLoadProgram(const char *userProgram, int privilege,
			    int argc, char *argv[])
{
  // Thistakes the name of an executable to load and creates a process
  // image based on the contents of the file.  The program is not started
  // by this function.

  int status = 0;
  file theFile;
  unsigned char *loadAddress = NULL;
  char procName[MAX_NAME_LENGTH];
  char tmp[MAX_PATH_NAME_LENGTH];
  int currentProcId = 0;
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

  // Load the program code/data into memory
  loadAddress = (unsigned char *) kernelLoaderLoad(userProgram, &theFile);

  if (loadAddress == NULL)
    {
      // Make an error
      kernelError(kernel_error, "The loader could not load this program");
      return (status = ERR_INVALID);
    }

  // Try to determine what kind of executable format we're dealing with.
  // We may need to do some fixup or relocations
  status = processExecutableType(loadAddress);

  if (status < 0)
    {
      // Make an error
      kernelError(kernel_error, "The loader could not load this program");
      return (status = ERR_INVALID);
    }

  // Get the current process ID
  currentProcId = kernelMultitaskerGetCurrentProcessId();

  // By default, pass no arguments
  argStruct.argc = 0;
  argStruct.argv = NULL;

  // Just get the program name without the path in order to set the process
  // name
  status = kernelFileSeparateLast(userProgram, tmp, procName);
  if (status < 0)
    strncpy(procName, userProgram, MAX_NAME_LENGTH);

  // Set up and run the user program as a process in the multitasker
  newProcId = kernelMultitaskerCreateProcess(loadAddress, theFile.size,
					     procName, privilege,
					     2, &argStruct);

  if (newProcId < 0)
    {
      // Release the memory we allocated for the program
      kernelMemoryRelease(loadAddress);
      // Make an error
      kernelError(kernel_error, "The loader could not create a process "
		  "for this program");
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
      argStruct.argv = kernelMemoryGet(argSpaceSize, "argument space");

      if (argStruct.argv != NULL)
	{
	  // Make the new process own the memory
	  status =
	    kernelMemoryChangeOwner(currentProcId, newProcId,
				    1, argStruct.argv, &newArgAddress);

	  if (status >= 0)
	    {
	      // Share the memory back with this process
	      status =
		kernelMemoryShare(newProcId, currentProcId,
				  newArgAddress, (void *) &(argStruct.argv));

	      if (status >= 0)
		{
		  // Leave space for pointers to the strings
		  argSpace = (argStruct.argv + ((argc + 1) * sizeof(char *)));
		  
		  for (count = 0; count < argc; count ++)
		    {
		      length = strlen(argv[count]);
		      strcpy(argSpace, argv[count]);		      
		      argStruct.argv[count] = argSpace;
		      argSpace += (length + 1);
		      // Adjust the pointers in argv so that they refer to the
		      // new process' address space
		      argStruct.argv[count] -= (unsigned) argStruct.argv;
		      argStruct.argv[count] += (unsigned) newArgAddress;
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
  status = kernelMultitaskerSetProcessState(processId, ready);

  if (status < 0)
    {
      kernelError(kernel_error, "The loader could not execute this program");
      return (status);
    }

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
