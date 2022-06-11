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
//  kernelEnvironment.c
//

// This file contains the C functions belonging to the kernel's environment
// manager.  The environment manager is for maintaining list of variables
// associated with each process (for example, the PATH variable).

#include "kernelEnvironment.h"
#include "kernelParameters.h"
#include "kernelMemoryManager.h"
#include "kernelPageManager.h"
#include "kernelMultitasker.h"
#include "kernelVariableList.h"
#include "kernelMiscFunctions.h"
#include "kernelError.h"
#include <string.h>


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
//  Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


kernelEnvironment *kernelEnvironmentCreate(int processId,
					   kernelEnvironment *copy)
{
  // This function will create a new environment structure for a process.

  int status = 0;
  int currentProcessId = 0;
  kernelEnvironment *peekEnvAddr = NULL;
  kernelEnvironment *procEnvAddr = NULL;
  unsigned variablesOffset, valuesOffset;
  char **peekVariables;
  char **peekValues;
  unsigned count;

  // It's OK for the 'copy' pointer to be NULL, but if it is not, it is
  // assumed that it is in the current process' address space.

  peekEnvAddr = kernelVariableListCreate(MAX_ENVIRONMENT_VARIABLES,
					 ENVIRONMENT_BYTES,
					 "process environment");
  if (peekEnvAddr == NULL)
    // Eek.  Couldn't get environment space
    return (peekEnvAddr);

  currentProcessId = kernelMultitaskerGetCurrentProcessId();

  if (processId != KERNELPROCID)
    {
      // Share the memory with the target process
      status =
	kernelMemoryShare(currentProcessId, processId,
			  (void *) peekEnvAddr, (void **) &procEnvAddr);
      
      if (status < 0)
	// Eek.  Couldn't share the memory.
	return (peekEnvAddr = NULL);

      // Change the ownership without remapping
      status = kernelMemoryChangeOwner(currentProcessId, processId, 0,
				       (void *) peekEnvAddr, NULL);

      if (status < 0)
	{
	  // Eek.  Couldn't chown the memory.
	  kernelMemoryRelease(peekEnvAddr);
	  return (peekEnvAddr = NULL);
	}
    }
  else
    procEnvAddr = peekEnvAddr;

  // Now we have access to it again, but we also have the virtual address
  // using which the target process will refer to it.
  
  // Are we supposed to inherit the environment from another process?
  if (copy)
    {
      kernelMemCopy((void *) copy, (void *) peekEnvAddr,
		    peekEnvAddr->totalSize);

      variablesOffset = ((void *) copy->variables - (void *) copy);
      valuesOffset = ((void *) copy->values - (void *) copy);
      
      // Adjust the pointers to the lists of variable names and values, and
      // to the data itself
      peekEnvAddr->variables = ((void *) procEnvAddr + variablesOffset);
      peekEnvAddr->values = ((void *) procEnvAddr + valuesOffset);
      peekEnvAddr->data =
	((void *) procEnvAddr + ((void *) copy->data - (void *) copy));

      // Loop through all of the pointers to the variables, adjusting them so
      // that they now point to the copied strings in the target address
      // space.

      peekVariables = ((void *) peekEnvAddr + variablesOffset);
      peekValues = ((void *) peekEnvAddr + valuesOffset);

      for (count = 0; count < peekEnvAddr->numVariables; count ++)
	{
	  peekVariables[count] = (char *) ((void *) procEnvAddr +
	    ((void *) (copy->variables[count]) - (void *) copy));
	  peekValues[count] = (char *) ((void *) procEnvAddr +
	    ((void *) (copy->values[count]) - (void *) copy));
	}
      
    }

  if (processId != KERNELPROCID)
    // Unmap the new environment from the current process' address space
    kernelPageUnmap(currentProcessId, (void *) peekEnvAddr,
		    peekEnvAddr->totalSize);

  // Return success
  return (procEnvAddr);
}


int kernelEnvironmentGet(const char *variable, char *buffer,
			 unsigned buffSize)
{
  // Get a variable's value from the current process' environment space

  int status = 0;

  // Make sure neither of our pointers are NULL
  if ((variable == NULL) || (buffer == NULL))
    return (status = ERR_NULLPARAMETER);

  status = kernelVariableListGet(kernelCurrentProcess->environment, variable,
				 buffer, buffSize);
  return (status);
}


int kernelEnvironmentSet(const char *variable, const char *value)
{
  // Set a variable's value in the current process' environment space.

  int status = 0;
  
  // Make sure neither our pointers is NULL
  if ((variable == NULL) || (value == NULL))
    return (status = ERR_NULLPARAMETER);

  status = kernelVariableListSet(kernelCurrentProcess->environment, variable,
				 value);
  return (status);
}


int kernelEnvironmentUnset(const char *variable)
{
  // Unset a variable's value from the current process' environment space.

  int status = 0;
  
  // Make sure our pointer isn't NULL
  if (variable == NULL)
    return (status = ERR_NULLPARAMETER);
  
  kernelVariableListUnset(kernelCurrentProcess->environment, variable);

  return (status);
}


void kernelEnvironmentDump(void)
{
  unsigned count;

  kernelTextPrintLine("Diagnostic process environment dump:");

  for (count = 0; count < kernelCurrentProcess->environment->numVariables;
       count ++)
    {
      kernelTextPrint(kernelCurrentProcess->environment->variables[count]);
      kernelTextPutc('=');
      kernelTextPrintLine(kernelCurrentProcess->environment->values[count]);
    }

  return;
}
