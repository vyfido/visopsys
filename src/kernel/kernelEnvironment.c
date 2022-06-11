//
//  Visopsys
//  Copyright (C) 1998-2021 J. Andrew McLaughlin
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
#include "kernelError.h"
#include "kernelFile.h"
#include "kernelParameters.h"
#include "kernelPage.h"
#include "kernelMemory.h"
#include "kernelMisc.h"
#include "kernelMultitasker.h"
#include <stdio.h>
#include <sys/paths.h>
#include <sys/user.h>
#include <sys/vis.h>

#define environmentSet	variableListSet


static int environmentGet(variableList *envList, const char *variable,
	char *buffer, unsigned buffSize)
{
	// Get a variable's value from the current process' environment space.

	const char *value = NULL;

	// Check params
	if (!envList || !buffer)
	{
		kernelError(kernel_error, "NULL parameter");
		return (ERR_NULLPARAMETER);
	}

	value = variableListGet(envList, variable);

	if (value)
	{
		strncpy(buffer, value, buffSize);
		return (0);
	}
	else
	{
		buffer[0] = '\0';
		return (ERR_NOSUCHENTRY);
	}
}


static int environmentLoad(const char *userName, variableList *envList)
{
	// Given a user name, load variables from the system's environment.conf
	// file into the environment space, then try to load more from the user's
	// home directory, if applicable.

	int status = ERR_NOSUCHFILE;
	char fileName[MAX_PATH_NAME_LENGTH + 1];
	variableList tmpList;
	const char *variable = NULL;
	int count;

	// Check params
	if (!userName || !envList)
	{
		kernelError(kernel_error, "NULL parameter");
		return (status = ERR_NULLPARAMETER);
	}

	memset(&tmpList, 0, sizeof(variableList));

	// Try to load environment variables from the system configuration
	// directory
	strcpy(fileName, PATH_SYSTEM_CONFIG "/environment.conf");
	if (kernelConfigReadSystem(fileName, &tmpList) >= 0)
	{
		for (count = 0; count < tmpList.numVariables; count ++)
		{
			variable = variableListGetVariable(&tmpList, count);
			environmentSet(envList, variable, variableListGet(&tmpList,
				variable));
		}

		variableListDestroy(&tmpList);
		status = 0;
	}

	if (strncmp(userName, USER_ADMIN, USER_MAX_NAMELENGTH))
	{
		// Try to load more environment variables from the user's home
		// directory
		sprintf(fileName, PATH_USERS_CONFIG "/environment.conf", userName);
		if (kernelFileLookup(fileName) &&
			(kernelConfigReadSystem(fileName, &tmpList) >= 0))
		{
			for (count = 0; count < tmpList.numVariables; count ++)
			{
				variable = variableListGetVariable(&tmpList, count);
				environmentSet(envList, variable, variableListGet(&tmpList,
					variable));
			}

			variableListDestroy(&tmpList);
			status = 0;
		}
	}

	return (status);
}


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
	const char *variable = NULL;
	const char *value = NULL;
	int count;

	// Check params.  It's OK for the 'copy' pointer to be NULL.
	if (!env)
	{
		kernelError(kernel_error, "NULL parameter");
		return (status = ERR_NULLPARAMETER);
	}

	status = variableListCreateSystem(env);
	if (status < 0)
		// Eek.  Couldn't get environment space.
		return (status);

	if (processId == KERNELPROCID)
		return (status = 0);

	// Are we supposed to inherit the environment from another process?
	if (copy)
	{
		for (count = 0; count < copy->numVariables; count ++)
		{
			variable = variableListGetVariable(copy, count);
			if (variable)
			{
				value = variableListGet(copy, variable);
				if (value)
					variableListSet(env, variable, value);
			}
		}
	}

	// Return success
	return (status = 0);
}


int kernelEnvironmentLoad(const char *userName, int processId)
{
	// Given a user name and a process ID, get the process environment and
	// call environmentLoad() to load the environment.

	variableList *envList = NULL;

	envList = kernelMultitaskerGetProcessEnvironment(processId);
	if (!envList)
	{
		kernelError(kernel_error, "No such process %d", processId);
		return (ERR_NOSUCHPROCESS);
	}

	return (environmentLoad(userName, envList));
}


int kernelEnvironmentGet(const char *variable, char *buffer,
	unsigned buffSize)
{
	// Get a variable's value from the current process' environment space.

	if (!kernelCurrentProcess)
		return (ERR_NOSUCHPROCESS);

	return (environmentGet(kernelCurrentProcess->environment, variable,
		buffer, buffSize));
}


int kernelEnvironmentProcessGet(int processId, const char *variable,
	char *buffer, unsigned buffSize)
{
	// Get a variable's value from the process' environment space.

	variableList *envList = NULL;

	envList = kernelMultitaskerGetProcessEnvironment(processId);
	if (!envList)
	{
		kernelError(kernel_error, "No such process %d", processId);
		return (ERR_NOSUCHPROCESS);
	}

	return (environmentGet(envList, variable, buffer, buffSize));
}


int kernelEnvironmentSet(const char *variable, const char *value)
{
	// Set a variable's value in the current process' environment space.

	if (!kernelCurrentProcess)
		return (ERR_NOSUCHPROCESS);

	return (environmentSet(kernelCurrentProcess->environment, variable,
		value));
}


int kernelEnvironmentProcessSet(int processId, const char *variable,
	const char *value)
{
	// Set a variable's value in the process' environment space.

	variableList *envList = NULL;

	envList = kernelMultitaskerGetProcessEnvironment(processId);
	if (!envList)
	{
		kernelError(kernel_error, "No such process %d", processId);
		return (ERR_NOSUCHPROCESS);
	}

	return (environmentSet(envList, variable, value));
}


int kernelEnvironmentUnset(const char *variable)
{
	// Unset a variable's value from the current process' environment space.

	if (!kernelCurrentProcess)
		return (ERR_NOSUCHPROCESS);

	return (variableListUnset(kernelCurrentProcess->environment, variable));
}


int kernelEnvironmentClear(void)
{
	// Clear the current process' entire environment space.

	if (!kernelCurrentProcess)
		return (ERR_NOSUCHPROCESS);

	return (variableListClear(kernelCurrentProcess->environment));
}


void kernelEnvironmentDump(void)
{
	variableList *list = NULL;
	const char *variable = NULL;
	int count;

	if (!kernelCurrentProcess)
		return;

	list = kernelCurrentProcess->environment;

	if (!list)
		return;

	for (count = 0; count < list->numVariables; count ++)
	{
		variable = variableListGetVariable(list, count);
		if (variable)
		{
			kernelTextPrintLine("%s=%s", variable, variableListGet(list,
				variable));
		}
	}

	return;
}

