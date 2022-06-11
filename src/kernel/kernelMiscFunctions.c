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
//  kernelMiscFunctions.c
//
	
// A file for miscellaneous things

#include "kernelMiscFunctions.h"
#include "kernelParameters.h"
#include "kernelLoader.h"
#include "kernelMultitasker.h"
#include "kernelError.h"
#include <stdio.h>
#include <string.h>


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


void kernelConsoleLogin(void)
{
  // This routine will launch a login process on the console.  This should
  // normally be called by the kernel's kernelMain() routine, above, but 
  // also possibly by the keyboard driver when some hot-key is pressed.

  static int loginPid = 0;
  kernelProcessState tmp;

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
  kernelMultitaskerTransferStreams(KERNELPROCID, loginPid, 1); // clear

  // Execute the login process.  Don't block.
  kernelLoaderExecProgram(loginPid, 0);

  // Done
  return;
}


kernelVariableList *kernelConfigurationReader(const char *fileName)
{
  int status = 0;
  fileStream configFile;
  kernelVariableList *list = NULL;
  char lineBuffer[256];
  char *variable = NULL;
  char *value = NULL;
  int count;

  if (fileName == NULL)
    return (list = NULL);

  status = kernelFileStreamOpen(fileName, OPENMODE_READ, &configFile);
  
  if (status < 0)
    {
      kernelError(kernel_warn, "Unable to read the configuration file \"%s\"",
		  fileName);
      return (list = NULL);
    }

  // Create the list, based on the size of the file, estimating one variable
  // for each minimum-sized 'line' of the file
  list = kernelVariableListCreate((configFile.f.size / 4),
				  configFile.f.size, "configuration data");

  if (list == NULL)
    {
      kernelError(kernel_warn, "Unable to create a variable list for "
		  "configuration file \"%s\"", fileName);
      kernelFileStreamClose(&configFile);
      return (list);
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

  return (list);
}


int kernelConfigurationWriter(kernelVariableList *list, fileStream *configFile)
{
  // Writes a variable list out to an open file.

  int status = 0;
  char lineBuffer[256];
  char *variable = NULL;
  char *value = NULL;
  int count;

  if ((list == NULL) || (configFile == NULL))
    return (status = ERR_NULLPARAMETER);

  // Write line by line for each variable
  for (count = 0; count < list->numVariables; count ++)
    {
      variable = list->variables[count];
      value = list->values[count];

      sprintf(lineBuffer, "%s=%s", variable, value);

      status = kernelFileStreamWriteLine(configFile, lineBuffer);

      if (status < 0)
	// Eh?  Disk full?  Something?
	break;
    }

  kernelFileStreamFlush(configFile);
  return (status);
}
