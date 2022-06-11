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
//  kernelError.c
//

#include "kernelError.h"
#include "kernelParameters.h"
#include "kernelText.h"
#include "kernelLog.h"
#include <string.h>
#include <stdio.h>


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
//  Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


void kernelErrorOutput(const char *module, const char *function, int line,
		       kernelErrorKind kind, const char *message, ...)
{
  // This routine takes a bunch of parameters and outputs a kernel error.
  // Until there's proper error logging, this will simply involve output
  // to the text console.

  va_list list;
  char errorType[32];
  char errorText[MAX_ERRORTEXT_LENGTH];
  //int regularForeground;

  // Copy the kind of the error
  switch(kind)
    {
    case kernel_panic:
      strcpy(errorType, "PANIC");
      break;
    case kernel_error:
      strcpy(errorType, "Error");
      break;
    case kernel_warn:
      strcpy(errorType, "Warning");
      break;
    default:
      strcpy(errorType, "Message");
      break;
    }

  sprintf(errorText, "%s:%s:%s(%d):", errorType, module, function, line);

  // Save the current text foreground color so we can re-set it.
  //regularForeground = kernelTextGetForeground();

  // Now set the foreground color to the error color
  //kernelTextSetForeground(DEFAULTERRORFOREGROUND);

  // Output the context of the message
  kernelLog(errorText);

  // If console logging is disabled, output the message to the screen
  // manually
  if (!kernelLogGetToConsole())
    kernelTextPrintLine(errorText);
  
  // Initialize the argument list
  va_start(list, message);

  // Expand the message if there were any parameters
  _expandFormatString(errorText, message, list);

  va_end(list);

  // Output the message
  kernelLog(errorText);

  if (!kernelLogGetToConsole())
    kernelTextPrintLine(errorText);
  
  // Set the foreground color back to what it was
  //kernelTextSetForeground(regularForeground);

  return;
}
