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
//  kernelMiscFunctions.c
//
	
// A file for miscellaneous things

#include "kernelMiscFunctions.h"
#include "kernelParameters.h"
#include "kernelLoader.h"
#include "kernelMultitasker.h"
#include "kernelMemoryManager.h"
#include "kernelProcessorX86.h"
#include "kernelLog.h"
#include "kernelFile.h"
#include "kernelError.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

volatile kernelSymbol *kernelSymbols = NULL;
volatile int kernelNumberSymbols = 0;


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
//  Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


const char *kernelVersion(void)
{
  // This function creates and returns a pointer to the kernel's version
  // string.

  static char *kernelInfo[] =
    {
      "Visopsys",
      _KVERSION_
    } ;
  
  static char versionString[64];

  // Construct the version string
  sprintf(versionString, "%s v%s", kernelInfo[0], kernelInfo[1]);

  return (versionString);
}


void kernelMemCopy(const void *src, void *dest, unsigned bytes)
{
  unsigned dwords = (bytes >> 2);
  int interrupts = 0;

  kernelProcessorSuspendInts(interrupts);

  if (((unsigned) src % 4) || ((unsigned) dest % 4) || (bytes % 4))
    kernelProcessorCopyBytes(src, dest, bytes);
  else
    kernelProcessorCopyDwords(src, dest, dwords);

  kernelProcessorRestoreInts(interrupts);
}


void kernelMemClear(void *dest, unsigned bytes)
{
  unsigned dwords = (bytes >> 2);
  int interrupts = 0;

  kernelProcessorSuspendInts(interrupts);

  if (((unsigned) dest % 4) || (bytes % 4))
    kernelProcessorWriteBytes(0, dest, bytes);
  else
    kernelProcessorWriteDwords(0, dest, dwords);

  kernelProcessorRestoreInts(interrupts);
}


int kernelMemCmp(const void *src, const void *dest, unsigned bytes)
{
  // Return 1 if the memory area is different, 0 otherwise.

  unsigned dwords = (bytes >> 2);
  unsigned count;

  if (((unsigned) dest % 4) || (bytes % 4))
    {
      for (count = 0; count < bytes; count++)
	if (((char *) src)[count] != ((char *) dest)[count])
	  return (1);
    }
  else
    {
      for (count = 0; count < dwords; count++)
	if (((int *) src)[count] != ((int *) dest)[count])
	  return (1);
    }

  return (0);
}


void kernelStackTrace(void *ESP, void *stackBase)
{
  // Will try to do a stack trace of the memory between ESP and stackBase
  // The stack memory in question must already be in the current address space.

  unsigned stackData;
  char *lastSymbol = NULL;
  int count;

  if (kernelSymbols == NULL)
    {
      kernelError(kernel_error, "No kernel symbols to do stack trace");
      return;
    }

  kernelTextPrintLine("--> kernel stack trace:");

  for ( ; ESP <= stackBase; ESP += sizeof(void *))
    {
      // If the current thing on the stack looks as if it might be a kernel
      // memory address, try to find the symbol it most closely matches
      
      stackData = *((unsigned *) ESP);

      if (stackData >= KERNEL_VIRTUAL_ADDRESS)
	// Find roughly the kernel function that this number corresponds to
	for (count = 0; count < kernelNumberSymbols; count ++)
	  {
	    if ((stackData >= kernelSymbols[count].address) &&
		(stackData < kernelSymbols[count + 1].address))
	      {
		if (lastSymbol != kernelSymbols[count].symbol)
		  kernelTextPrintLine("  %s",
				      (char *) kernelSymbols[count].symbol);
		lastSymbol = (char *) kernelSymbols[count].symbol;
	      }
	  }
    }

  kernelTextPrintLine("<--");

  return;
}


void kernelConsoleLogin(void)
{
  // This routine will launch a login process on the console.  This should
  // normally be called by the kernel's kernelMain() routine, above, but 
  // also possibly by the keyboard driver when some hot-key is pressed.

  static int loginPid = 0;
  processState tmp;

  // Try to make sure we don't start multiple logins at once
  if (loginPid)
    {
      // Try to kill the old one, but don't mind the success or failure of
      // the operation
      if (kernelMultitaskerGetProcessState(loginPid, &tmp) >= 0)
	kernelMultitaskerKillProcess(loginPid, 1);
    }

  // Try to load the login process
  loginPid = kernelLoaderLoadProgram("/programs/login",
				     PRIVILEGE_SUPERVISOR,
				     0,     // no args
				     NULL); // no args
  
  if (loginPid < 0)
    {
      // Don't fail, but make a warning message
      kernelError(kernel_warn, "Couldn't start a login process");
      return;
    }
 
  // Attach the login process to the console text streams
  kernelMultitaskerDuplicateIO(KERNELPROCID, loginPid, 1); // clear

  // Execute the login process.  Don't block.
  kernelLoaderExecProgram(loginPid, 0);

  // Done
  return;
}


int kernelConfigurationReader(const char *fileName, variableList *list)
{
  int status = 0;
  fileStream configFile;
  char lineBuffer[256];
  char *variable = NULL;
  char *value = NULL;
  int count;

  // Check params
  if ((fileName == NULL) || (list == NULL))
    {
      kernelError(kernel_error, "File name or list parameter is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  status = kernelFileStreamOpen(fileName, OPENMODE_READ, &configFile);
  if (status < 0)
    {
      kernelError(kernel_warn, "Unable to read the configuration file \"%s\"",
		  fileName);
      return (status);
    }

  // Create the list, based on the size of the file, estimating one variable
  // for each minimum-sized 'line' of the file
  status = kernelVariableListCreate(list);
  if (status < 0)
    {
      kernelError(kernel_warn, "Unable to create a variable list for "
		  "configuration file \"%s\"", fileName);
      kernelFileStreamClose(&configFile);
      return (status);
    }

  // Read line by line
  while(1)
    {
      status = kernelFileStreamReadLine(&configFile, 256, lineBuffer);

      if (status <= 0)
	// End of file?
	break;

      // Just a newline or comment?
      if ((lineBuffer[0] == '\n') || (lineBuffer[0] == '#'))
	continue;

      variable = lineBuffer;
      for (count = 0; lineBuffer[count] != '='; count ++);
      lineBuffer[count] = '\0';

      // Everything after the '=' is the value
      value = (lineBuffer + count + 1);
      // Get rid of the newline, if there
      if (value[strlen(value) - 1] == '\n')
	value[strlen(value) - 1] = '\0';

      // Store it
      kernelVariableListSet(list, variable, value);
    }

  kernelFileStreamClose(&configFile);

  return (status = 0);
}


int kernelConfigurationWriter(const char *fileName, variableList *list)
{
  // Writes a variable list out to a config file, with a little bit of
  // extra sophistication so that if the file already exists, comments and
  // blank lines are (hopefully) preserved

  int status = 0;
  file tmpFile;
  int oldFile = 0;
  char tmpName[MAX_PATH_NAME_LENGTH];
  fileStream oldFileStream;
  fileStream newFileStream;
  char lineBuffer[256];
  char *variable = NULL;
  char *value = NULL;
  int count;

  // Check params
  if ((fileName == NULL) || (list == NULL))
    {
      kernelError(kernel_error, "File name or list parameter is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  kernelMemClear(&tmpFile, sizeof(file));
  kernelMemClear(&oldFileStream, sizeof(fileStream));
  kernelMemClear(&newFileStream, sizeof(fileStream));

  // Is there already an old version of the config file?
  if (!kernelFileFind(fileName, &tmpFile))
    {
      // Yup.  Open it for reading, and make our temporary filename different
      // so that we don't overwrite anything until we've been successful.

      oldFile = 1;

      status = kernelFileStreamOpen(fileName, OPENMODE_READ, &oldFileStream);
      if (status < 0)
	return (status);

      sprintf(tmpName, "%s.TMP", fileName);
    }
  else
    strcpy(tmpName, fileName);

  // Create the new config file for writing
  status = kernelFileStreamOpen(tmpName, (OPENMODE_CREATE | OPENMODE_WRITE |
					  OPENMODE_TRUNCATE), &newFileStream);
  if (status < 0)
    {
      if (oldFile)
	kernelFileStreamClose(&oldFileStream);
      return (status);
    }

  // Write line by line for each variable
  for (count = 0; count < list->numVariables; count ++)
    {
      // If we successfully opened an old file, first try to to stuff in sync
      // with the line numbers
      if (oldFile)
	{
	  strcpy(lineBuffer, "#");
	  while ((lineBuffer[0] == '#') || (lineBuffer[0] == '\n'))
	    {
	      status =
		kernelFileStreamReadLine(&oldFileStream, 256, lineBuffer);
	      if (status < 0)
		break;

	      if ((lineBuffer[0] == '#') || (lineBuffer[0] == '\n'))
		{
		  status =
		    kernelFileStreamWriteLine(&newFileStream, lineBuffer);
		  if (status < 0)
		    return (status);
		}
	    }
	}

      variable = list->variables[count];
      value = list->values[count];

      sprintf(lineBuffer, "%s=%s", variable, value);

      status = kernelFileStreamWriteLine(&newFileStream, lineBuffer);
      if (status < 0)
	return (status);
    }

  // Close things up
  kernelFileStreamClose(&oldFileStream);
  status = kernelFileStreamClose(&newFileStream);
  if (status < 0)
    return (status);

  if (oldFile)
    {
      // Move the temporary file to the destination
      status = kernelFileMove(tmpName, fileName);
      if (status < 0)
	return (status);
    }

  return (status = 0);
}


int kernelReadSymbols(const char *filename)
{
  // This will attempt to read the supplied properties file of kernel
  // symbols into a variable list, then turn that into a data structure
  // through which we can search for addresses.
  
  int status = 0;
  variableList tmpList;
  int count;

  // Make a log message
  kernelLog("Reading kernel symbols from \"%s\"", filename);

  // Try to read the supplied file name
  status = kernelConfigurationReader(filename, &tmpList);
  if (status < 0)
    return (status);

  if (tmpList.numVariables == 0)
    // No symbols were properly read
    return (status = ERR_NOSUCHENTRY);

  // Get some memory to hold our list of symbols
  kernelSymbols =
    kernelMemoryGet((tmpList.numVariables * sizeof(kernelSymbol)),
		    "kernel symbols");
  if (kernelSymbols == NULL)
    // Couldn't get the memory
    return (status = ERR_MEMORY);

  // Loop through all of the variables, setting the symbols in our table
  for (count = 0; count < tmpList.numVariables; count ++)
    {
      kernelSymbols[count].address = xtoi(tmpList.variables[count]);
      strncpy((char *) kernelSymbols[count].symbol, tmpList.values[count],
	      MAX_SYMBOL_LENGTH);
    }

  kernelNumberSymbols = tmpList.numVariables;

  // Release our variable list
  kernelMemoryRelease(tmpList.memory);

  return (status = 0);
}
