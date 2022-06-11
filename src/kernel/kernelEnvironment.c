//
//  Visopsys
//  Copyright (C) 1998-2014 J. Andrew McLaughlin
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
//  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
//
//  kernelEnvironment.c
//

// This file contains convenience functions for creating/accessing a
// process' list of environment variables (for example, the PATH variable).
//  It's just a standard variableList.

#include "kernelEnvironment.h"
#include "kernelParameters.h"
#include "kernelPage.h"
#include "kernelVariableList.h"
#include "kernelMultitasker.h"
#include "kernelMemory.h"
#include "kernelMisc.h"
#include "kernelError.h"


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
//  Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


int kernelEnvironmentCreate(int processId, variableList *env,
	variableList *copy)
{
	// This function will create a new environment structure for a process.

	int status = 0;
	int currentProcessId = 0;
	void *procMemAddr = NULL;
	int count;

	// Check params
	if (env == NULL)
	{
		kernelError(kernel_error, "Environment structure pointer is NULL");
		return (status = ERR_NULLPARAMETER);
	}

	// It's OK for the 'copy' pointer to be NULL, but if it is not, it is
	// assumed that it is in the current process' address space.

	status = kernelVariableListCreate(env);
	if (status < 0)
		// Eek.  Couldn't get environment space
		return (status);

	if (processId == KERNELPROCID)
		return (status = 0);

	currentProcessId = kernelMultitaskerGetCurrentProcessId();

	// Share the memory with the target process
	status =
		kernelMemoryShare(currentProcessId, processId, env->memory, &procMemAddr);
	if (status < 0)
	{
		// Eek.  Couldn't share the memory.
		kernelMemoryRelease(env->memory);
		return (status);
	}

	// Change the ownership without remapping
	status =
		kernelMemoryChangeOwner(currentProcessId, processId, 0, env->memory, NULL);
	if (status < 0)
	{
		// Eek.  Couldn't chown the memory.
		kernelMemoryRelease(env->memory);
		return (status);
	}

	// Now we have access to it again, but we also have the virtual address
	// using which the target process will refer to it.
	
	// Are we supposed to inherit the environment from another process?
	if (copy)
	{
		// Add all the variables to the new list
		for (count = 0; count < copy->numVariables; count ++)
			kernelVariableListSet(env, copy->variables[count],
				copy->values[count]);

		// Loop through all of the pointers to the variables, adjusting them so
		// that they now point to the target memory address space.
		for (count = 0; count < env->numVariables; count ++)
		{
			env->variables[count] =
				(procMemAddr + ((void *) env->variables[count] - env->memory));
			env->values[count] =
				(procMemAddr + ((void *) env->values[count] - env->memory));
		}
	}

	// Adjust the pointers to the lists of variable names and values, and
	// to the data itself
	env->variables = (procMemAddr + ((void *) env->variables - env->memory));
	env->values = (procMemAddr + ((void *) env->values - env->memory));
	env->data = (procMemAddr + ((void *) env->data - env->memory));

	// Unmap the new environment from the current process' address space
	kernelPageUnmap(currentProcessId, env->memory, env->memorySize);

	env->memory = procMemAddr;

	// Return success
	return (status = 0);
}


int kernelEnvironmentGet(const char *variable, char *buffer,
	unsigned buffSize)
{
	// Get a variable's value from the current process' environment space

	int status = 0;

	// Make sure neither of our pointers are NULL
	if ((variable == NULL) || (buffer == NULL))
		return (status = ERR_NULLPARAMETER);

	status = kernelVariableListGet((variableList *)
		&(kernelCurrentProcess->environment), variable, buffer, buffSize);
	return (status);
}


int kernelEnvironmentSet(const char *variable, const char *value)
{
	// Set a variable's value in the current process' environment space.

	int status = 0;
	
	// Make sure neither our pointers is NULL
	if ((variable == NULL) || (value == NULL))
		return (status = ERR_NULLPARAMETER);

	status = kernelVariableListSet((variableList *)
		&(kernelCurrentProcess->environment), variable, value);
	return (status);
}


int kernelEnvironmentUnset(const char *variable)
{
	// Unset a variable's value from the current process' environment space.

	int status = 0;
	
	// Make sure our pointer isn't NULL
	if (variable == NULL)
		return (status = ERR_NULLPARAMETER);
	
	kernelVariableListUnset((variableList *)
		&(kernelCurrentProcess->environment), variable);
	return (status);
}


void kernelEnvironmentDump(void)
{
	int count;

	kernelTextPrintLine("Diagnostic process environment dump:");

	for (count = 0; count < kernelCurrentProcess->environment.numVariables;
		count ++)
	{
		kernelTextPrint("%s", kernelCurrentProcess->environment.variables[count]);
		kernelTextPutc('=');
		kernelTextPrintLine("%s", kernelCurrentProcess->environment.values[count]);
	}

	return;
}
