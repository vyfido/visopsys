//
//  Visopsys
//  Copyright (C) 1998-2003 J. Andrew McLaughlin
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
#include "kernelMiscAsmFunctions.h"
#include "kernelResourceManager.h"
#include "kernelError.h"
#include <string.h>
#include <sys/errors.h>


static int findVariable(kernelVariableList *list, const char *variable)
{
  // This will attempt to locate a variable in the supplied list.
  // On success, it returns the slot number of the variable.  Otherwise
  // it returns negative.

  int slot = ERR_NOSUCHENTRY;
  int count;

  // Search through the list of variables in the supplied list for the one
  // requested by the caller
  for (count = 0; count < list->numVariables; count ++)
    if (!strcmp(list->variables[count], variable))
      {
	slot = count;
	break;
      }

  return (slot);
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
//  Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


kernelVariableList *kernelVariableListCreate(unsigned maxVariables,
					     unsigned dataSize,
					     const char *description)
{
  // This function will create a new kernelVariableList structure

  unsigned structureSize = 0;
  kernelVariableList *list = NULL;
  
  // numVariables and size must be non-zero
  if (!maxVariables || !dataSize)
    {
      kernelError(kernel_error, "Cannot create an empty variable list");
      return (list = NULL);
    }

  // The description should not be NULL
  if (description == NULL)
    description = "variable list";

  // The total structure size will be the size of the raw data, plus the size
  // of kernelVariableList itself, plus maxVariables pointers for both
  // the variable names and the values, respectively
  structureSize = (sizeof(kernelVariableList) +
		   (sizeof(char *) * maxVariables * 2) + dataSize);
  
  // Get memory for the data structure
  list = kernelMemoryRequestBlock(structureSize, 0, description);

  if (list == NULL)
    return (list);

  // Initialize the number of variables and max variables and max data
  list->numVariables = 0;
  list->maxVariables = maxVariables;
  list->usedData = 0;
  list->maxData = dataSize;
  list->totalSize = structureSize;

  // Set the first variable pointer to be at the end of the control data
  list->variables = ((void *) list + sizeof(kernelVariableList));
  
  // Set the first value pointer to be at the end of the variable pointers
  list->values =
    ((void *) list->variables + (sizeof(char *) * maxVariables));

  // Set the data pointer to be at the end of the value pointers
  list->data = ((void *) list->values + (sizeof(char *) * maxVariables));

  list->listLock = 0;

  // Return success
  return (list);
}


int kernelVariableListGet(kernelVariableList *list, const char *variable,
			  char *buffer, unsigned buffSize)
{
  // Get a variable's value from the variable list

  int status = 0;
  int slot = 0;

  // Make sure none of our pointers are NULL
  if ((list == NULL) || (variable == NULL) || (buffer == NULL) ||
      (buffSize == 0))
    return (status = ERR_NULLPARAMETER);

  // Lock the list while we're working with it
  status = kernelResourceManagerLock(&(list->listLock));
  if (status < 0)
    return (status = ERR_NOLOCK);

  slot = findVariable(list, variable);

  if (slot < 0)
    {
      // No such variable
      buffer[0] = '\0';
      kernelResourceManagerUnlock(&(list->listLock));
      return (status = slot);
    }

  // Copy the variable's value into the buffer supplied
  strncpy(buffer, list->values[slot], buffSize);
  buffer[buffSize - 1] = '\0';

  kernelResourceManagerUnlock(&(list->listLock));

  // Return success
  return (status = 0);
}


int kernelVariableListSet(kernelVariableList *list, const char *variable,
			  const char *value)
{
  // Set a variable's value in the supplied list

  int status = 0;
  
  // Make sure none of our pointers are NULL
  if ((list == NULL) || (variable == NULL) || (value == NULL))
    return (status = ERR_NULLPARAMETER);

  // Make sure we're not exceeding the maximum number of variables
  if (list->numVariables >= list->maxVariables)
    return (status = ERR_BOUNDS);

  // Lock the list while we're working with it
  status = kernelResourceManagerLock(&(list->listLock));
  if (status < 0)
    return (status = ERR_NOLOCK);

  // Check to see whether the variable currently has a value
  if (findVariable(list, variable) >= 0)
    {
      // The variable already has a value.  We need to unset it first.  The
      // only problem with this is that if we later discover we don't have
      // enough room to store the new variable, the old variable gets trashed.
      // Oh well.
      status = kernelVariableListUnset(list, variable);

      if (status < 0)
	{
	  // We couldn't unset it.
	  kernelResourceManagerUnlock(&(list->listLock));
	  return (status);
	}
    }

  // Make sure we now have enough room to store the variable
  if ((list->usedData + (strlen(variable) + 1) + (strlen(value) + 1)) > 
      list->maxData)
    {
      kernelResourceManagerUnlock(&(list->listLock));
      return (status = ERR_BOUNDS);
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

  kernelResourceManagerUnlock(&(list->listLock));

  // Return success
  return (status = 0);
}


int kernelVariableListUnset(kernelVariableList *list, const char *variable)
{
  // Unset a variable's value from the supplied list.  This involves shifting
  // the entire contents of the list data starting from where the variable is
  // found.

  int status = 0;
  int slot = 0;
  int subtract = 0;
  int count;
  
  // Make sure our pointers aren't NULL
  if ((list == NULL) || (variable == NULL))
    return (status = ERR_NULLPARAMETER);
  
  // Lock the list while we're working with it
  status = kernelResourceManagerLock(&(list->listLock));
  if (status < 0)
    return (status = ERR_NOLOCK);

  // Search the list of variables for the requested one.
  slot = findVariable(list, variable);

  // Did we find it?
  if (slot < 0)
    {
      kernelResourceManagerUnlock(&(list->listLock));
      return (status = ERR_NOSUCHENTRY);
    }

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
  list->numVariables--;

  // Adjust the number of bytes used
  list->usedData -= subtract;

  kernelResourceManagerUnlock(&(list->listLock));

  // Return success
  return (status = 0);
}
