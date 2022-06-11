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
//  kernelVariableList.c
//

// This file contains the C functions belonging to the kernel's implementation
// of variable lists.  Variable lists will be used to store environment
// variables, as well as the contents of configuration files, for example

#include "kernelVariableList.h"
#include "kernelMemoryManager.h"
#include "kernelMiscFunctions.h"
#include "kernelLock.h"
#include "kernelError.h"
#include <string.h>


static int expandList(variableList *list)
{
  // Takes the variable list and expands it.

  int status = 0;
  void *memory = NULL;
  char **variables = NULL;
  char **values = NULL;
  void *data = NULL;
  int count;

  list->maxVariables *= 2;
  list->maxData *= 2;
  list->memorySize =
    ((list->maxVariables * sizeof(char *) * 2) + list->maxData);

  memory = kernelMemoryGet(list->memorySize, "variable list");
  if (memory == NULL)
    return (status = ERR_MEMORY);

  // Set the pointers
  variables = memory;
  values = ((void *) variables + (list->maxVariables * sizeof(char *)));
  data = ((void *) values + (list->maxVariables * sizeof(char *)));

  for (count = 0; count < (int) list->numVariables; count ++)
    {
      variables[count] = (data + (list->variables[count] - list->data));
      values[count] = (data + (list->values[count] - list->data));
    }

  // Copy the data
  kernelMemCopy(list->data, data, list->usedData);

  list->variables = variables;
  list->values = values;
  list->data = data;

  kernelMemoryRelease(list->memory);
  list->memory = memory;

  return (status = 0);
}


static int findVariable(variableList *list, const char *variable)
{
  // This will attempt to locate a variable in the supplied list.
  // On success, it returns the slot number of the variable.  Otherwise
  // it returns negative.

  int slot = ERR_NOSUCHENTRY;
  int count;

  if ((list == NULL) || (variable == NULL))
    kernelError(kernel_error, "NULL PARAMETER");

  // Search through the list of variables in the supplied list for the one
  // requested by the caller
  for (count = 0; count < list->numVariables; count ++)
    {
      if (!strcmp(list->variables[count], variable))
	{
	  slot = count;
	  break;
	}
    }

  return (slot);
}


static int unsetVariable(variableList *list, const char *variable)
{
  // Unset a variable's value from the supplied list.  This involves shifting
  // the entire contents of the list data starting from where the variable is
  // found.

  int status = 0;
  int slot = 0;
  int subtract = 0;
  int count;

  // Search the list of variables for the requested one.
  slot = findVariable(list, variable);
  if (slot < 0)
    // Not found
    return (status = ERR_NOSUCHENTRY);

  // Found it.  The amount of data to subtract from the data is equal to the
  // sum of the lengths of the variable name and its value (plus one for
  // each NULL character)
  subtract =
    ((strlen(list->variables[slot]) + 1) + (strlen(list->values[slot]) + 1));

  // Any more data after this?
  if (list->numVariables > 1)
    {
      // Starting from where the variable name starts, shift the whole
      // contents of the data forward by 'subtract' bytes
      kernelMemCopy((void *) (list->variables[slot] + subtract),
		    (void *) list->variables[slot],
		    (list->usedData - (list->variables[slot] - list->data) -
		     subtract));

      // Now remove the 'variable' and 'value' pointers, and shift all
      // subsequent pointers in the lists forward by one, adjusting each
      // by the number of bytes we subtracted from the data
      for (count = slot; count < (list->numVariables - 1); count ++)
	{
	  list->variables[count] = list->variables[count + 1];
	  list->variables[count] -= subtract;
	  list->values[count] = list->values[count + 1];
	  list->values[count] -= subtract;
	}
    }

  // We now have one fewer variables
  list->numVariables -= 1;

  // Adjust the number of bytes used
  list->usedData -= subtract;

  // Return success
  return (status = 0);
}


static int setVariable(variableList *list, const char *variable,
		       const char *value)
{
  // Does the work of setting a variable

  int status = 0;
  
  // Check to see whether the variable currently has a value
  if (findVariable(list, variable) >= 0)
    {
      // The variable already has a value.  We need to unset it first.
      status = unsetVariable(list, variable);
      if (status < 0)
	// We couldn't unset it.
	return (status);
    }

  // Make sure we're not exceeding the maximum number of variables, and
  // make sure we now have enough room to store the variable name and value
  if ((list->numVariables >= list->maxVariables) ||
      ((list->usedData + (strlen(variable) + 1) + (strlen(value) + 1)) > 
       (unsigned) list->maxData))
    {
      status = expandList(list);
      if (status < 0)
	return (status);
    }

  // Okay, we're setting the variable

  // The new variable goes at the end of the usedData
  list->variables[list->numVariables] = (list->data + list->usedData);

  // Copy the variable name
  strcpy(list->variables[list->numVariables], variable);

  // The variable's value will come after the variable name in memory
  list->values[list->numVariables] =
    (list->variables[list->numVariables] + (strlen(variable) + 1));

  // Copy the variable value
  strcpy(list->values[list->numVariables], value);

  // We now have one more variable
  list->numVariables++;

  // Indicate the new used data total
  list->usedData += (strlen(variable) + 1) + (strlen(value) + 1);

  // Return success
  return (status = 0);
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
//  Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


int kernelVariableListCreate(variableList *list)
{
  // This function will create a new variableList structure

  int status = 0;

  if (list == NULL)
    {
      kernelError(kernel_error, "NULL variable list parameter");
      return (status = ERR_NULLPARAMETER);
    }

  // Initialize the number of variables and max variables and max data
  kernelMemClear(list, sizeof(variableList));
  list->maxVariables = VARIABLE_INITIAL_NUMBER;
  list->maxData = VARIABLE_INITIAL_DATASIZE;
  list->memorySize = VARIABLE_INITIAL_MEMORY;

  // The memory will be the size of pointers for both the variable names and
  // the values, plus additional memory for raw data
  list->memory = kernelMemoryGet(list->memorySize, "variable list");
  if (list->memory == NULL)
    return (status = ERR_MEMORY);

  list->variables = list->memory;

  // Set the first value pointer to be at the end of the variable pointers
  list->values =
    ((void *) list->variables + (sizeof(char *) * list->maxVariables));

  // Set the data pointer to be at the end of the value pointers
  list->data = ((void *) list->values + (sizeof(char *) * list->maxVariables));

  // Return success
  return (status = 0);
}


int kernelVariableListGet(variableList *list, const char *variable,
			  char *buffer, unsigned buffSize)
{
  // Get a variable's value from the variable list

  int status = 0;
  int slot = 0;

  if (buffer != NULL)
    buffer[0] = '\0';

  // Make sure none of our pointers are NULL
  if ((list == NULL) || (variable == NULL) || (buffer == NULL) ||
      (buffSize == 0))
    return (status = ERR_NULLPARAMETER);

  // Lock the list while we're working with it
  status = kernelLockGet(&(list->listLock));
  if (status < 0)
    return (status = ERR_NOLOCK);

  slot = findVariable(list, variable);

  if (slot < 0)
    {
      // No such variable
      kernelLockRelease(&(list->listLock));
      return (status = slot);
    }

  // Copy the variable's value into the buffer supplied
  strncpy(buffer, list->values[slot], buffSize);
  buffer[buffSize - 1] = '\0';

  kernelLockRelease(&(list->listLock));

  // Return success
  return (status = 0);
}


int kernelVariableListSet(variableList *list, const char *variable,
			  const char *value)
{
  // A wrapper function for setVariable

  int status = 0;
  
  // Make sure none of our pointers are NULL
  if ((list == NULL) || (variable == NULL) || (value == NULL))
    return (status = ERR_NULLPARAMETER);

  // Lock the list while we're working with it
  status = kernelLockGet(&(list->listLock));
  if (status < 0)
    return (status = ERR_NOLOCK);

  status = setVariable(list, variable, value);

  kernelLockRelease(&(list->listLock));

  return (status);
}


int kernelVariableListUnset(variableList *list, const char *variable)
{
  // A wrapper function for unsetVariable

  int status = 0;
  
  // Make sure our pointers aren't NULL
  if ((list == NULL) || (variable == NULL))
    return (status = ERR_NULLPARAMETER);
  
  // Lock the list while we're working with it
  status = kernelLockGet(&(list->listLock));
  if (status < 0)
    return (status = ERR_NOLOCK);

  status = unsetVariable(list, variable);

  kernelLockRelease(&(list->listLock));

  return (status);
}
