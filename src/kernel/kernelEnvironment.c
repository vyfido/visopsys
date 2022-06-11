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
//  kernelEnvironment.c
//

// This file contains the C functions belonging to the kernel's environment
// manager.  The environment manager is for maintaining list of variables
// associated with each process (for example, the PATH variable).

#include "kernelEnvironment.h"
#include "kernelParameters.h"
#include "kernelMemoryManager.h"
#include "kernelMultitasker.h"
#include "kernelMiscAsmFunctions.h"
#include <string.h>
#include <sys/errors.h>


static int findVariable(kernelEnvironment *env, const char *variable)
{
  // This will attempt to locate a variable in the supplied environment.
  // On success, it returns the slot number of the variable.  Otherwise
  // it returns negative.

  int slot = ERR_NOSUCHENTRY;
  int count;

  // Search through the list of environment variables for the one
  // requested by the caller
  for (count = 0; count < env->numVariables; count ++)
    if (!strcmp(env->variables[count], variable))
      slot = count;

  return (slot);
}


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
  // This function will create a new environment structure for process.

  int status = 0;
  kernelEnvironment *peekEnvAddr = NULL;
  kernelEnvironment *procEnvAddr = NULL;
  int count;

  
  // It's OK for the 'copy' pointer to be NULL

  // Reserve environment space for the process
  peekEnvAddr = kernelMemoryRequestBlock(sizeof(kernelEnvironment), 0,
					 "process environment");
  if (peekEnvAddr == NULL)
    // Eek.  Couldn't get environment space
    return (peekEnvAddr);

  if (processId == KERNELPROCID)
    procEnvAddr = peekEnvAddr;

  else
    {
      // Make the target process own the memory
      status = kernelMemoryChangeOwner(currentProcess->processId, processId,
		       1, (void *) peekEnvAddr, (void **) &procEnvAddr);
      if (status < 0)
	{
	  // Eek.  Couldn't change the memory owner.  Free it.
	  kernelMemoryReleaseByPointer((void *) peekEnvAddr);
	  return (peekEnvAddr = NULL);
	}
      
      // Now, we need to access the memory again.  Share it.
      status = kernelMemoryShare(processId, currentProcess->processId,
			 (void *) procEnvAddr, (void **) &peekEnvAddr);
      if (status < 0)
	// Eek.  Couldn't share the memory.
	return (peekEnvAddr = NULL);

      // Now we have access to it again, but we also have the virtual address
      // using which the target process will refer to it.
    }

  // Initialize the number of variables
  peekEnvAddr->numVariables = 0;

  // Set the first variable pointer
  peekEnvAddr->variables[0] = (char *)
    (procEnvAddr + ((void *) peekEnvAddr->envSpace - (void *) peekEnvAddr));

  // Are we supposed to inherit the environment from another process
  if (copy != NULL)
    {
      // Copy the contents of the environment space
      kernelMemCopy((void *) copy->envSpace, (void *) peekEnvAddr->envSpace,
		    ENVIRONMENT_BYTES);

      // Loop through all of the pointers to the variables, adjusting them so
      // that they now point to the copied strings in the target address
      // space.

      for (count = 0; count < copy->numVariables; count ++)
	{
	  peekEnvAddr->variables[count] = (char *) (procEnvAddr->envSpace +
	   ((void *) copy->variables[count] - (void *) copy->envSpace));
	  peekEnvAddr->values[count] = (char *) (procEnvAddr->envSpace +
		((void *) copy->values[count] - (void *) copy->envSpace));
	}

      // Finally, copy the number of variables
      peekEnvAddr->numVariables = copy->numVariables;
    }

  // Return success
  return (procEnvAddr);
}


int kernelEnvironmentGet(const char *variable, char *buffer,
			 unsigned int buffSize)
{
  // Get a variable's value from the current process' environment space

  int status = 0;
  int slot = 0;


  // Make sure neither of our pointers are NULL
  if (variable == NULL)
    return (status = ERR_NULLPARAMETER);
  if (buffer == NULL)
    return (status = ERR_NULLPARAMETER);

  slot = findVariable(currentProcess->environment, variable);

  if (slot < 0)
    // No such variable
    return (status = slot);

  // Copy the variable's value into the buffer supplied
  strncpy(buffer, currentProcess->environment->values[slot], buffSize);
  buffer[buffSize - 1] = '\0';

  // Return success
  return (status = 0);
}


int kernelEnvironmentSet(const char *variable, const char *value)
{
  // Set a variable's value in the current process' environment space.
  // At the moment, this will simply set a variable.  It will not look
  // for variable 'changes', etc.

  int status = 0;
  kernelEnvironment *env = NULL;

  
  // Make sure neither our pointers is NULL
  if (variable == NULL)
    return (status = ERR_NULLPARAMETER);
  if (value == NULL)
    return (status = ERR_NULLPARAMETER);

  // This is for readability while we're working with this environment.
  env = currentProcess->environment;
  
  // Check to see whether the variable currently has a value
  if (findVariable(env, variable) >= 0)
    {
      // The variable already has a value.  We need to unset it first.
      status = kernelEnvironmentUnset(variable);

      if (status < 0)
	// We couldn't unset it.
	return (status);
    }

  // Okay, we're setting the variable

  // Locate a fresh memory location where we can keep this new variable.
  // This will be the first spot after the terminating NULL character of
  // the current last variable value.  Is that confusing?

  if (env->numVariables > 0)
    env->variables[env->numVariables] =
      (env->values[env->numVariables - 1] +
       (strlen(env->values[env->numVariables - 1]) + 1));
  else
    env->variables[env->numVariables] = env->envSpace;

  // Copy the variable name
  strcpy(env->variables[env->numVariables], variable);

  // The variable's value will come after the variable name in memory
  env->values[env->numVariables] =
    (env->variables[env->numVariables] + (strlen(variable) + 1));

  // Copy the variable value
  strcpy(env->values[env->numVariables], value);

  // We now have one more variable
  env->numVariables++;

  // Return success
  return (status = 0);
}


int kernelEnvironmentUnset(const char *variable)
{
  // Unset a variable's value from the current process' environment space.
  // This involves shifting the entire contents of the environment
  // memory starting from where the variable is found.

  int status = 0;
  kernelEnvironment *env = NULL;
  int foundSlot = 0;
  char *found = NULL;
  int subtract = 0;
  int count;

  
  // Make sure our pointer isn't NULL
  if (variable == NULL)
    return (status = ERR_NULLPARAMETER);
  
  // This is for readability
  env = currentProcess->environment;
  
  // Search through the list of environment variables for the requested one.
  for (count = 0; count < env->numVariables; count ++)
    if (!strcmp(env->variables[count], variable))
      {
	found = env->values[count];
	foundSlot = count;
      }

  // Did we find it?
  if (found == NULL)
    return (status = ERR_NOSUCHENTRY);

  // Found it.  The amount of data to subtract from the environment is
  // equal to the sum of the lengths of the variable name and its value
  // (plus one for each NULL character)
  subtract = (strlen(found) + 1);
  subtract += (strlen(found + subtract) + 1);

  // Starting from 'found', shift the whole contents of the environment
  // space forward by 'subtract' bytes
  for (count = foundSlot; count < (ENVIRONMENT_BYTES - subtract); count ++)
    env->envSpace[count] = env->envSpace[count + subtract];

  // NULL those last 'subtract' characters
  for (count = (ENVIRONMENT_BYTES - subtract); count < ENVIRONMENT_BYTES;
       count ++)
    env->envSpace[count] = '\0';

  // Now remove the 'variable' and 'value' pointers, and shift all subsequent
  // pointers in the lists forward by one.
  for (count = foundSlot; count < (env->numVariables - 1); count ++)
    {
      env->variables[count] = env->variables[count + 1];
      env->values[count] = env->values[count + 1];
    }

  // Now we need to adjust the pointers of every variable and value from
  // 'foundSlot' backwards, to compansate for the 'subtract' bytes we
  // removed.
  for (count = foundSlot; count < env->numVariables; count ++)
    {
      env->variables[count] -= subtract;
      env->values[count] -= subtract;
    }

  // We now have one fewer variables
  env->numVariables--;

  // Return success
  return (status = 0);
}


void kernelEnvironmentDump(void)
{
  kernelEnvironment *env = NULL;
  int count;

  kernelTextPrintLine("Diagnostic process environment dump:");

  // This is for readability
  env = currentProcess->environment;

  for (count = 0; count < env->numVariables; count ++)
    {
      kernelTextPrint(env->variables[count]);
      kernelTextPutc('=');
      kernelTextPrintLine(env->values[count]);
    }

  return;
}
