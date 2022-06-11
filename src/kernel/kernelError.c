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
#include "kernelWindowManager.h"
#include "kernelMultitasker.h"
#include <string.h>
#include <stdio.h>


static void errorDialogThread(int numberArgs, void *args[])
{
  int status = 0;
  const char *title = NULL;
  const char *message = NULL;
  kernelWindow *dialogWindow = NULL;
  image errorImage;
  kernelWindowComponent *okButton = NULL;
  componentParameters params;
  windowEvent event;

  title = args[0];
  message = args[1];

  // Check params.  It's okay for parentWindow to be NULL.
  if ((title == NULL) || (message == NULL))
    {
      status = ERR_NULLPARAMETER;
      goto exit;
    }

  bzero(&errorImage, sizeof(image));
  bzero(&params, sizeof(componentParameters));

  // Create the dialog.
  dialogWindow = kernelWindowNew(kernelCurrentProcess->processId, title);
  if (dialogWindow == NULL)
    {
      status = ERR_NOCREATE;
      goto exit;
    }

  status = kernelImageLoadBmp(ERRORIMAGE_NAME, &errorImage);
  if (status < 0)
    goto exit;

  params.gridWidth = 1;
  params.gridHeight = 1;
  params.padLeft = 5;
  params.padTop = 5;
  params.orientationX = orient_center;
  params.orientationY = orient_middle;
  params.useDefaultForeground = 1;
  params.useDefaultBackground = 1;

  errorImage.translucentColor.red = 0;
  errorImage.translucentColor.green = 255;
  errorImage.translucentColor.blue = 0;
  kernelWindowNewImage(dialogWindow, &errorImage, draw_translucent, &params);

  // Create the label
  params.gridX = 1;
  params.padRight = 5;
  kernelWindowNewTextLabel(dialogWindow, NULL, message, &params);

  // Create the button
  params.gridX = 0;
  params.gridY = 1;
  params.gridWidth = 2;
  params.padBottom = 5;
  okButton = kernelWindowNewButton(dialogWindow, "OK", NULL, &params);
  if (okButton == NULL)
    {
      status = ERR_NOCREATE;
      goto exit;
    }

  kernelWindowSetVisible(dialogWindow, 1);

  while(1)
    {
      // Check for our OK button
      status = kernelWindowComponentEventGet((void *) okButton, &event);
      if ((status < 0) || ((status > 0) && (event.type == EVENT_MOUSE_UP)))
	break;

      // Check for window close events
      status = kernelWindowComponentEventGet((void *) dialogWindow, &event);
      if ((status < 0) || ((status > 0) && (event.type == EVENT_WINDOW_CLOSE)))
	break;

      // Done
      kernelMultitaskerYield();
    }
      
  kernelWindowDestroy(dialogWindow);
  status = 0;

 exit:
  kernelMultitaskerTerminate(status);
}


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


void kernelErrorDialog(const char *title, const char *message)
{
  // This will make a simple error dialog message, and wait until the button
  // has been pressed.

  void *args[] = { (void *) title, (void *) message };

  kernelMultitaskerSpawnKernelThread(&errorDialogThread, "error dialog", 2,
				     args);

  return;
}
