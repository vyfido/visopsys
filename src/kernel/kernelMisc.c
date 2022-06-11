//
//  Visopsys
//  Copyright (C) 1998-2007 J. Andrew McLaughlin
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
//  kernelMisc.c
//
	
// A file for miscellaneous things

#include "kernelMisc.h"
#include "kernelParameters.h"
#include "kernelLoader.h"
#include "kernelMultitasker.h"
#include "kernelMemory.h"
#include "kernelNetwork.h"
#include "kernelProcessorX86.h"
#include "kernelRandom.h"
#include "kernelRtc.h"
#include "kernelLog.h"
#include "kernelFile.h"
#include "kernelError.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

char *kernelVersion[] = {
  "Visopsys",
  _KVERSION_
};

volatile kernelSymbol *kernelSymbols = NULL;
volatile int kernelNumberSymbols = 0;


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
//  Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


void kernelGetVersion(char *buffer, int bufferSize)
{
  // This function creates and returns a pointer to the kernel's version
  // string.

  // Construct the version string
  snprintf(buffer, bufferSize, "%s v%s", kernelVersion[0], kernelVersion[1]);

  return;
}


int kernelSystemInfo(struct utsname *uname)
{
  // This function gathers some info about the system and puts it into
  // a 'utsname' structure, just like the one returned by 'uname' in Unix.

  int status = 0;
  kernelDevice *cpuDevice = NULL;

  // Check params
  if (uname == NULL)
    return (status = ERR_NULLPARAMETER);

  strncpy(uname->sysname, kernelVersion[0], UTSNAME_MAX_SYSNAME_LENGTH);
  kernelNetworkGetHostName(uname->nodename, NETWORK_MAX_HOSTNAMELENGTH);
  strncpy(uname->release, kernelVersion[1], UTSNAME_MAX_RELEASE_LENGTH);
  strncpy(uname->version, __DATE__" "__TIME__, UTSNAME_MAX_VERSION_LENGTH);
  if ((kernelDeviceFindType(kernelDeviceGetClass(DEVICECLASS_CPU), NULL,
			    &cpuDevice, 1) > 0) && cpuDevice->device.subClass)
    strncpy(uname->machine, cpuDevice->device.subClass->name,
	    UTSNAME_MAX_MACHINE_LENGTH);
  kernelNetworkGetDomainName(uname->domainname, NETWORK_MAX_DOMAINNAMELENGTH);

  return (status = 0);
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


void kernelMemSet(void *dest, unsigned char value, unsigned bytes)
{
  unsigned dwords = (bytes >> 2);
  unsigned tmpDword = 0;
  int interrupts = 0;

  if (dwords)
    tmpDword = ((value << 24) | (value << 16) |	(value << 8) | value);

  kernelProcessorSuspendInts(interrupts);

  if (((unsigned) dest % 4) || (bytes % 4))
    kernelProcessorWriteBytes(value, dest, bytes);
  else
    kernelProcessorWriteDwords(tmpDword, dest, dwords);

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
  loginPid = kernelLoaderLoadProgram("/programs/login", PRIVILEGE_SUPERVISOR);
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
  file tmpFile;
  variableList tmpList;
  int count;

  // See if there is a kernelSymbols file.
  status = kernelFileFind(filename, &tmpFile);
  if (status < 0)
    {
      kernelLog("No kernel symbols file \"%s\"", filename);
      return (status);
    }

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
  kernelVariableListDestroy(&tmpList);

  return (status = 0);
}


time_t kernelUnixTime(void)
{
  // Unix time is seconds since 00:00:00 January 1, 1970

  int status = 0;
  time_t returnTime = 0;
  struct tm timeStruct;
  int count;

  static int month_days[] =
  { 31, /* Jan */ 28, /* Feb */ 31, /* Mar */ 30, /* Apr */
    31, /* May */ 30, /* Jun */ 31, /* Jul */ 31, /* Aug */
    30, /* Sep */ 31, /* Aug */ 30 /* Nov */ };

  // Get the date and time according to the kernel
  status = kernelRtcDateTime(&timeStruct);
  if (status < 0)
    return (returnTime = -1);

  if (timeStruct.tm_year < 1970)
    return (returnTime = -1);

  // Calculate seconds for all complete years
  returnTime = ((timeStruct.tm_year - 1970) * SECPERYR);

  // Add 1 days's worth of seconds for every complete leap year.  There
  // is a leap year in every year divisible by 4 except for years which
  // are both divisible by 100 not by 400.  Got it?
  for (count = timeStruct.tm_year; count >= 1972; count--)
    if (((count % 4) == 0) && (((count % 100) != 0) || ((count % 400) == 0)))
      returnTime += SECPERDAY;

  // Add seconds for all complete months this year
  for (count = (timeStruct.tm_mon - 1); count >= 0; count --)
    returnTime += (month_days[count] * SECPERDAY);

  // Add seconds for all complete days in this month
  returnTime += ((timeStruct.tm_mday - 1) * SECPERDAY);

  // Add one day's worth of seconds if THIS is a leap year, and if the
  // current month and day are greater than Feb 28
  if (((timeStruct.tm_year % 4) == 0) &&
      (((timeStruct.tm_year % 100) != 0) ||
       ((timeStruct.tm_year % 400) == 0)))
    {
      if ((timeStruct.tm_mon > 1) ||
	  ((timeStruct.tm_mon == 1) &&
	   (timeStruct.tm_mday > 28)))
	returnTime += SECPERDAY;
    }

  // Add seconds for all complete hours in this day
  returnTime += (timeStruct.tm_hour * SECPERHR);

  // Add seconds for all complete minutes in this hour
  returnTime += (timeStruct.tm_min * SECPERMIN);

  // Add the current seconds
  returnTime += timeStruct.tm_sec;

  return (returnTime);
}


int kernelGuidGenerate(kernelGuid *guid)
{
  // Generates our best approximation of a GUID, which is not to spec but
  // so what, really?  Will generate GUIDs unique enough for us.

  int status = 0;
  unsigned long long longTime = 0;
  static unsigned clockSeq = 0;
  lock globalLock;

  // Check params
  if (guid == NULL)
    {
      kernelError(kernel_error, "GUID parameter is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  // Get the lock
  status = kernelLockGet(&globalLock);
  if (status < 0)
    return (status);

  if (clockSeq == 0)
    clockSeq = kernelRandomUnformatted();

  // Increment the clock
  clockSeq += 1;

  // Get the time as a 60-bit value representing a count of 100-nanosecond
  // intervals since 00:00:00.00, 15 October 1582 (the date of Gregorian
  // reform to the Christian calendar).
  //
  // Umm.  Overkill on the time thing?  Maybe?  Nanoseconds since 1582?
  // Why are we wasting the number of nanoseconds in the 400 years before
  // most of us ever touched computers?

  longTime = ((kernelUnixTime() * 10000000) + 0x01b21dd213814000ULL);

  guid->fields.timeLow = (unsigned) (longTime & 0x00000000FFFFFFFF);
  guid->fields.timeMid = (unsigned) ((longTime >> 32) & 0x0000FFFF);
  guid->fields.timeHighVers = (((unsigned)(longTime >> 48) & 0x0FFF) | 0x1000);
  guid->fields.clockSeqRes = (((clockSeq >> 16) & 0x3FFF) | 0x8000);
  guid->fields.clockSeqLow = (clockSeq & 0xFFFF);
  // Random node ID
  *((unsigned *) guid->fields.node) = kernelRandomUnformatted();
  *((unsigned *) (guid->fields.node + 4)) = (kernelRandomUnformatted() >> 16);

  /* 6ba7b810-9dad-11d1-80b4-00c04fd430c8
  kernelTextPrintLine("GUID %08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
		      guid->fields.timeLow, guid->fields.timeMid,
		      guid->fields.timeHighVers, guid->fields.clockSeqRes,
		      guid->fields.clockSeqLow, guid->fields.node[0],
		      guid->fields.node[1], guid->fields.node[2],
		      guid->fields.node[3], guid->fields.node[4],
		      guid->fields.node[5]);
  */

  kernelLockRelease(&globalLock);

  return (status = 0);
}
