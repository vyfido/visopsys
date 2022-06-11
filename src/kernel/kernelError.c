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
//  kernelError.c
//

#include "kernelError.h"
#include "kernelParameters.h"
#include "kernelText.h"
#include "kernelLog.h"
#include <string.h>
#include <stdio.h>


static int errorForeground = 0;  // The colour of error messages
static int initialized = 0;


int kernelErrorInitialize(void)
{
  //  Well, we'll hold off on doing this function.  We can flesh it out
  //  when there's really a such thing as as kernelTextStreams that
  //  output to more than one place.

  int status = 0;

  // We are now initialized
  initialized = 1;

  // By default, we set the colour of error messages to red
  kernelErrorSetForeground(DEFAULTERRORFOREGROUND);

  // Return success
  return (status = 0);
}


int kernelErrorSetForeground(int colour)
{
  // Sets the colour of error message printouts.

  int status = 0;


  // Don't do anything until we're initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Take the colour and save it.
  errorForeground = colour;

  // Return success
  return (status = 0);
}


int kernelErrorGetForeground(void)
{
  // Returns the colour of error message printouts.

  // Don't do anything until we're initialized
  if (!initialized)
    return (ERR_NOTINITIALIZED);

  // Take the colour and return it.
  return (errorForeground);
}


void kernelErrorOutput(const char *module, const char *function, int line,
		       kernelErrorKind kind, const char *message, ...)
{
  // This routine takes a bunch of parameters and outputs a kernel error.
  // Until there's proper error logging, this will simply involve output
  // to the text console.

  va_list list;
  char errorText[MAX_ERRORTEXT_LENGTH];
  int regularForeground;


  // Don't do anything until we're initialized
  if (!initialized)
    return;

  // Copy the kind of the error
  switch(kind)
    {
    case kernel_panic:
      strcpy(errorText, "PANIC:");
      break;
    case kernel_error:
      strcpy(errorText, "Error:");
      break;
    case kernel_warn:
      strcpy(errorText, "Warning:");
      break;
    default:
      strcpy(errorText, "Message:");
      break;
    }


  // Copy the module name
  if (module)
    {
      strcat(errorText, module);
      strcat(errorText, ":");
    }
  else
    strcat(errorText, "(NULL module):");


  // Copy the function name
  if (function)
    strcat(errorText, function);
  else
    strcat(errorText, "(NULL function)");

  // Add the line number
  sprintf((errorText + strlen(errorText)), "(%d):", line);

  // Save the current text foreground colour so we can re-set it.
  regularForeground = kernelTextStreamGetForeground();

  // Now set the foreground colour to the error colour
  kernelTextStreamSetForeground(errorForeground);

  // Output the context of the message
  kernelLog(errorText);

  // If console logging is disabled, output the message to the screen
  // manually
  if (!kernelLogGetToConsole())
    kernelTextPrintLine(errorText);
  
  // Initialize the argument list
  va_start(list, message);

  // Expand the message if there were any parameters
  _expand_format_string(errorText, message, list);

  va_end(list);

  // Output the message
  kernelLog(errorText);

  if (!kernelLogGetToConsole())
    kernelTextPrintLine(errorText);
  
  // Set the foreground colour back to what it was
  kernelTextStreamSetForeground(regularForeground);

  return;
}
