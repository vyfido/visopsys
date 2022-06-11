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
//  kernelLog.c
//
	
// This file contains the routines designed to facilitate a variety
// of kernel logging features.

#include "kernelLog.h"
#include "kernelParameters.h"
#include "kernelText.h"
#include "kernelFileStream.h"
#include "kernelMultitasker.h"
#include "kernelMiscAsmFunctions.h"
#include "kernelRtcFunctions.h"
#include "kernelDebug.h"
#include "kernelError.h"
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <sys/file.h>
#include <sys/errors.h>
#include <string.h>


static volatile int logToConsole = 0;
static volatile int logToFile = 0;
static volatile int loggingInitialized = 0;
static int updaterPID = 0;

static stream * volatile logStream;
static fileStream * volatile logFileStream;


static int flushLogStream(void)
{
  // This routine is internal, and will take whatever is currently in the
  // log stream and output it to the log file, if applicable.  Returns
  // 0 on success, negative otherwise

  int status = 0;
  int bufferSize = 0;
  char buffer[LOG_STREAM_SIZE];


  // If there's no log stream, there's no point in going any further
  // (there will be nothing to flush)
  if (logStream == NULL)
    return (status = 0);

  // kernelTextPrintUnsigned(logFileStream->s->count);
  // kernelTextPrint(" -> log file stream count ");
  // kernelTextPrintLine(__FUNCTION__);

  // How much stuff is in the log stream?
  bufferSize = logStream->count;

  if (bufferSize > 0)
    {
      // Take the contents of the log stream...
      status = ((streamFunctions *) logStream->functions)->popN(logStream,
						bufferSize, buffer);

      if (status < 0)
	{
	  // Oops, couldn't get anything
	  kernelError(kernel_warn, "Unable to read the log stream");
	  return (status);
	}

      // ...and write them to the log file
      status = kernelFileStreamWriteStr(logFileStream, buffer);

      if (status < 0)
	{
	  // Oops, couldn't write to the log file.  Make a warning
	  kernelError(kernel_warn, "Unable to write to the log stream");
	  return (status);
	}

      // Flush the file stream
      status = kernelFileStreamFlush(logFileStream);

      if (status < 0)
	{
	  // Oops, couldn't write to the log file.  Make a warning
	  kernelError(kernel_warn, "Unable to flush the log file");
	  return (status);
	}
    }

  // kernelTextPrintUnsigned(logFileStream->s->count);
  // kernelTextPrint(" -> log file stream count ");
  // kernelTextPrintLine(__FUNCTION__);

  // Return success
  return (status = 0);
}


static void kernelLogUpdater(void)
{
  // This function will be a new thread spawned by the kernel which
  // flushes the log file stream as a low-priority process.  

  int status = 0;

  // Wait 5 seconds to let the system get moving
  kernelMultitaskerWait(100);

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

      // Yield the rest of the timeslice and wait for at least 1 second
      kernelMultitaskerWait(20);
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


  kernelDebugEnter();

  // Initially, we will log to the console, and not to a file
  logToConsole = 1;
  logToFile = 0;

  // Initialize the logging stream
  logStream = kernelStreamNew(LOG_STREAM_SIZE, itemsize_char);

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


  kernelDebugEnter();

  // Do not accept this call unless logging has been initialized
  if (!loggingInitialized)
    {
      kernelError(kernel_error, "Kernel logging has not been initialized");
      return (status = ERR_NOTINITIALIZED);
    }

  // Make sure the file name argument isn't NULL
  if (logFileName == NULL)
    {
      kernelError(kernel_error, "Log file name argument is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  // Initialize the fileStream structure that we'll be using for a log file
  status = 
    kernelFileStreamOpen(logFileName, (OPENMODE_WRITE | OPENMODE_CREATE),
			 &theStream);

  if (status < 0)
    {
      // We couldn't open or create a log file, for whatever reason.
      kernelError(kernel_error, "Couldn't open or create kernel log file");
      return (status);
    }

  logFileStream = &theStream;

  // We will be logging to a file, so we don't need to log to the console
  // any more
  logToFile = 1;

  // Flush the file stream of any existing data.  It will all get written
  // to the file now.
  flushLogStream();

  // Make a 'log file updater' thread
  updaterPID = kernelMultitaskerSpawn(kernelLogUpdater, "log file updater",
				      0 , NULL);

  // Make sure we were successful
  if (updaterPID < 0)
    {
      // Make a kernelError
      kernelError(kernel_error, "Couldn't spawn log updater thread");
      return (updaterPID);
    }

  // Re-nice the log file updater
  status = kernelMultitaskerSetProcessPriority(updaterPID, 
					       (PRIORITY_LEVELS - 2));
  
  if (status < 0)
    {
      // Oops, we couldn't make it low-priority.  This is probably
      // bad, but not fatal.  Make a kernelError.
      kernelError(kernel_warn, "Couldn't re-nice the log updater thread");
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


int kernelLogShutdown(void)
{
  // This function stops kernel logging to the log file.

  int status = 0;


  kernelDebugEnter();

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


int kernelLog(const char *format, ...)
{
  // This is the function that does all of the kernel logging.
  // Returns 0 on success, negative otherwise

  int status = 0;
  va_list list;
  char output[MAXSTRINGLENGTH];
  char streamOutput[MAXSTRINGLENGTH];
  struct tm time;


  kernelDebugEnter();

  // Do not accept this call unless logging has been initialized
  if (!loggingInitialized)
    return (status = ERR_NOTINITIALIZED);

  // Make sure the format string isn't NULL
  if (format == NULL)
    return (status = ERR_NULLPARAMETER);

  // Initialize the argument list
  va_start(list, format);

  // Expand the format string into an output string
  _expand_format_string(output, format, list);

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
  status = ((streamFunctions *) logStream->functions)->appendN(logStream,
				       strlen(streamOutput), streamOutput);
  return (status);
}
