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
//  kernelLog.c
//
	
// This file contains the routines designed to facilitate a variety
// of kernel logging features.

#include "kernelLog.h"
#include "kernelParameters.h"
#include "kernelFileStream.h"
#include "kernelMultitasker.h"
#include "kernelRtc.h"
#include "kernelError.h"
#include <stdio.h>
#include <string.h>


static volatile int logToConsole = 0;
static volatile int logToFile = 0;
static volatile int loggingInitialized = 0;
static int updaterPID = 0;

static stream logStream;
static fileStream * volatile logFileStream;


static int flushLogStream(void)
{
  // This routine is internal, and will take whatever is currently in the
  // log stream and output it to the log file, if applicable.  Returns
  // 0 on success, negative otherwise

  int status = 0;
  char buffer[512];

  // Take the contents of the log stream...
  while (logStream.popN(&logStream, 511, buffer) > 0)
    {
      // ...and write them to the log file
      status = kernelFileStreamWriteStr(logFileStream, buffer);
      if (status < 0)
	{
	  // Oops, couldn't write to the log file.  Make a warning
	  kernelError(kernel_warn, "Unable to write to the log stream");
	  logToFile = 0;
	  return (status);
	}
      
      // Flush the file stream
      status = kernelFileStreamFlush(logFileStream);
      if (status < 0)
	{
	  // Oops, couldn't write to the log file.  Make a warning
	  kernelError(kernel_warn, "Unable to flush the log file");
	  logToFile = 0;
	  return (status);
	}
    }
  
  // Return success
  return (status = 0);
}


static void kernelLogUpdater(void)
{
  // This function will be a new thread spawned by the kernel which
  // flushes the log file stream as a low-priority process.  

  int status = 0;

  // Wait a few seconds to let the system get moving
  kernelMultitaskerWait(200);

  while(1)
    {
      // Let "flush" do its magic
      status = flushLogStream();
      if (status < 0)
	{
	  // Eek!  Make logToFile = 0 and try to close it
	  logToFile = 0;
	  kernelFileStreamClose(logFileStream);
	  kernelMultitaskerTerminate(status);
	}

      // Yield the rest of the timeslice and wait
      kernelMultitaskerWait(40);
    }
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
//  Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


int kernelLogInitialize(void)
{
  // This function initializes kernel logging.  When logging is first
  // initiated, log messages are not written to files.

  int status = 0;

  // Initially, we will log to the console, and not to a file
  logToConsole = 1;
  logToFile = 0;

  // Initialize the logging stream
  status = kernelStreamNew(&logStream, LOG_STREAM_SIZE, itemsize_byte);
  if (status < 0)
    return (status);

  // Make a note that we've been initialized
  loggingInitialized = 1;

  // Return success
  return (status = 0);
}


int kernelLogSetFile(const char *logFileName)
{
  // This function will initiate the logging of messages to the log
  // file specified.  Returns 0 on success, negative otherwise.

  int status = 0;
  static fileStream theStream;

  // Do not accept this call unless logging has been initialized
  if (!loggingInitialized)
    {
      kernelError(kernel_error, "Kernel logging has not been initialized");
      return (status = ERR_NOTINITIALIZED);
    }

  if (logFileName == NULL)
    {
      // No more logging to a file
      logToFile = 0;
      return (status = 0);
    }

  // Initialize the fileStream structure that we'll be using for a log file
  status =
    kernelFileStreamOpen(logFileName, (OPENMODE_WRITE | OPENMODE_CREATE),
			 &theStream);
  if (status < 0)
    {
      // We couldn't open or create a log file, for whatever reason.
      kernelError(kernel_error, "Couldn't open or create kernel log file");
      logToFile = 0;
      return (status);
    }

  logFileStream = &theStream;

  // We will be logging to a file, so we don't need to log to the console
  // any more
  logToFile = 1;

  // Flush the file stream of any existing data.  It will all get written
  // to the file now.
  flushLogStream();

  // Make a logging thread
  updaterPID = kernelMultitaskerSpawn(kernelLogUpdater, "logging thread",
				      0 , NULL);
  // Make sure we were successful
  if (updaterPID < 0)
    {
      // Make a kernelError
      kernelError(kernel_error, "Couldn't spawn logging thread");
      return (updaterPID);
    }

  // Re-nice the log file updater
  status = kernelMultitaskerSetProcessPriority(updaterPID, 
					       (PRIORITY_LEVELS - 2));
  if (status < 0)
    {
      // Oops, we couldn't make it low-priority.  This is probably
      // bad, but not fatal.  Make a kernelError.
      kernelError(kernel_warn, "Couldn't re-nice the logging thread");
    }

  // Return success
  return (status = 0);
}


int kernelLogGetToConsole(void)
{
  return (logToConsole);
}


void kernelLogSetToConsole(int onOff)
{
  // Ya want console logging or not?
  logToConsole = onOff;
  return;
}


int kernelLog(const char *format, ...)
{
  // This is the function that does all of the kernel logging.
  // Returns 0 on success, negative otherwise

  int status = 0;
  va_list list;
  char output[MAXSTRINGLENGTH];
  char streamOutput[MAXSTRINGLENGTH];
  struct tm time;

  // Do not accept this call unless logging has been initialized
  if (!loggingInitialized)
    return (status = ERR_NOTINITIALIZED);

  // Make sure the format string isn't NULL
  if (format == NULL)
    return (status = ERR_NULLPARAMETER);

  // Initialize the argument list
  va_start(list, format);

  // Expand the format string into an output string
  _expandFormatString(output, format, list);

  va_end(list);

  // Now take care of the output part

  // Are we logging to the console?  Just print the message itself.
  if (logToConsole)
    kernelTextPrintLine(output);

  // Get the current date/time so we can prepend it to the logging output
  status = kernelRtcDateTime(&time);
  if (status < 0)
    // Before RTC initialization (at boot time) the above will fail.
    sprintf(streamOutput, "%s\n", output);
  else
    // Turn the date/time into a string representation (but skip the
    // first 4 'weekday' characters)
    sprintf(streamOutput, "%s %s\n", (asctime(&time) + 4), output);

  // Put it all into the log stream
  status = logStream.appendN(&logStream, strlen(streamOutput), streamOutput);
  return (status);
}


int kernelLogShutdown(void)
{
  // This function stops kernel logging to the log file.

  int status = 0;

  if (logToFile)
    {
      // Flush the file stream of any remaining data
      flushLogStream();

      // Close the log file
      status = kernelFileStreamClose(logFileStream);
      if (status < 0)
	{
	  kernelError(kernel_error, "Unable to close the kernel log file");
	  return (status);
	}
      
      logToFile = 0;
    }

  // Return success
  return (status = 0);
}
