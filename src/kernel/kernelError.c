//
//  Visopsys
//  Copyright (C) 1998-2006 J. Andrew McLaughlin
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
#include "kernelLog.h"
#include "kernelInterrupt.h"
#include "kernelPic.h"
#include "kernelMultitasker.h"
#include "kernelWindow.h"
#include "kernelMisc.h"
#include <string.h>
#include <stdio.h>
#include <sys/cdefs.h>

static char *panicConst = "PANIC";
static char *errorConst = "Error";
static char *warningConst = "Warning";
static char *messageConst = "Message";


static void errorDialogThread(int argc, void *argv[])
{
  int status = 0;
  const char *title = NULL;
  const char *message = NULL;
  kernelWindow *dialogWindow = NULL;
  image errorImage;
  kernelWindowComponent *okButton = NULL;
  componentParameters params;
  windowEvent event;

  if (argc < 3)
    goto exit;

  title = argv[1];
  message = argv[2];
 
  kernelMemClear(&errorImage, sizeof(image));
  kernelMemClear(&params, sizeof(componentParameters));

  // Create the dialog.
  dialogWindow = kernelWindowNew(kernelCurrentProcess->processId, title);
  if (dialogWindow == NULL)
    {
      status = ERR_NOCREATE;
      goto exit;
    }

  params.gridWidth = 1;
  params.gridHeight = 1;
  params.padLeft = 5;
  params.padTop = 5;
  params.orientationX = orient_center;
  params.orientationY = orient_middle;
  params.useDefaultForeground = 1;
  params.useDefaultBackground = 1;

  status = kernelImageLoad(ERRORIMAGE_NAME, 0, 0, &errorImage);

  if (status == 0)
    {
      errorImage.translucentColor.red = 0;
      errorImage.translucentColor.green = 255;
      errorImage.translucentColor.blue = 0;
      params.padRight = 0;
      kernelWindowNewImage(dialogWindow, &errorImage, draw_translucent,
			   &params);
    }

  // Create the label
  params.gridX = 1;
  params.padRight = 5;
  kernelWindowNewTextLabel(dialogWindow, message, &params);

  // Create the button
  params.gridX = 0;
  params.gridY = 1;
  params.gridWidth = 2;
  params.padBottom = 5;
  params.fixedWidth = 1;
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
      if ((status > 0) && (event.type == EVENT_MOUSE_LEFTUP))
	break;

      // Check for window close events
      status = kernelWindowComponentEventGet((void *) dialogWindow, &event);
      if ((status > 0) && (event.type == EVENT_WINDOW_CLOSE))
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


void kernelErrorOutput(const char *fileName, const char *function, int line,
		       kernelErrorKind kind, const char *message, ...)
{
  // This routine takes a bunch of parameters and outputs a kernel error.
  // Until there's proper error logging, this will simply involve output
  // to the text console.

  va_list list;
  const char *errorType = NULL;
  char processName[MAX_PROCNAME_LENGTH];
  char errorText[MAX_ERRORTEXT_LENGTH];

  // Copy the kind of the error
  switch(kind)
    {
    case kernel_panic:
      errorType = panicConst;
      break;
    case kernel_error:
      errorType = errorConst;
      break;
    case kernel_warn:
      errorType = warningConst;
      break;
    default:
      errorType= messageConst;
      break;
    }

  processName[0] = '\0';
  if (kernelProcessingInterrupt)
    sprintf(processName, "interrupt %x", kernelPicGetActive());
  else if (kernelCurrentProcess)
    strncpy(processName, (char *) kernelCurrentProcess->processName,
	    MAX_PROCNAME_LENGTH);

  sprintf(errorText, "%s:%s:%s:%s(%d):", errorType, processName, fileName,
	  function, line);

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

  return;
}


void kernelErrorDialog(const char *title, const char *message, ...)
{
  // This will make a simple error dialog message, and wait until the button
  // has been pressed.

  va_list list;
  char errorText[MAX_ERRORTEXT_LENGTH];
  void *args[] = {
    (void *) title,
    (void *) errorText
  };

  // Check params
  if ((title == NULL) || (message == NULL))
    return;

  // Initialize the argument list
  va_start(list, message);

  // Expand the message if there were any parameters
  _expandFormatString(errorText, message, list);

  va_end(list);

  kernelMultitaskerSpawnKernelThread(&errorDialogThread, "error dialog thread",
				     2, args);

  return;
}
