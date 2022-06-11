// 
//  Visopsys
//  Copyright (C) 1998-2004 J. Andrew McLaughlin
//  
//  This library is free software; you can redistribute it and/or modify it
//  under the terms of the GNU Lesser General Public License as published by
//  the Free Software Foundation; either version 2.1 of the License, or (at
//  your option) any later version.
//
//  This library is distributed in the hope that it will be useful, but
//  WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
//  General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public License
//  along with this library; if not, write to the Free Software Foundation,
//  Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
//
//  windowFileDialog.c
//

// This contains functions for user programs to operate GUI components.

#include <string.h>
#include <sys/window.h>
#include <sys/vsh.h>
#include <sys/api.h>
#include <sys/errors.h>


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


_X_ int windowNewFileDialog(objectKey parentWindow, const char *title, const char *message, char *fileName, unsigned maxLength)
{
  // Desc: Create a 'file' dialog box, with the parent window 'parentWindow', and the given titlebar text and main message.  The dialog will have a file selection area, an 'OK' button and a 'CANCEL' button.  If the user presses OK or ENTER, the function returns the value 1 and copies the filename into the fileName buffer.  Otherwise it returns 0 and puts a NULL string into fileName.  If 'parentWindow' is NULL, the dialog box is actually created as an independent window that looks the same as a dialog.  This is a blocking call that returns when the user closes the dialog window (i.e. the dialog is 'modal').

  int status = 0;
  objectKey dialogWindow = NULL;
  componentParameters params;
  objectKey textLabel = NULL;
  objectKey textField = NULL;
  objectKey okButton = NULL;
  objectKey cancelButton = NULL;
  windowEvent event;

  // Check params.  It's okay for parentWindow to be NULL.
  if ((title == NULL) || (message == NULL)|| (fileName == NULL))
    return (status = ERR_NULLPARAMETER);

  // Create the dialog.  Arbitrary size and coordinates
  if (parentWindow)
    dialogWindow = windowNewDialog(parentWindow, title);
  else
    dialogWindow = windowNew(multitaskerGetCurrentProcessId(), title);

  bzero(&params, sizeof(componentParameters));
  params.gridWidth = 2;
  params.gridHeight = 1;
  params.padLeft = 5;
  params.padRight = 5;
  params.padTop = 5;
  params.orientationX = orient_center;
  params.orientationY = orient_top;
  params.useDefaultForeground = 1;
  params.useDefaultBackground = 1;
  textLabel = windowNewTextLabel(dialogWindow, message, &params);
  if (textLabel == NULL)
    return (status = ERR_NOCREATE);

  // Put a text field in the window for the user to type
  params.gridY = 1;
  params.padTop = 5;
  params.hasBorder = 1;
  params.stickyFocus = 1;
  params.useDefaultBackground = 0;
  params.background.red = 255;
  params.background.green = 255;
  params.background.blue = 255;
  textField = windowNewTextField(dialogWindow, 30, &params);
  if (textField == NULL)
    return (status = ERR_NOCREATE);
  
  // Create the OK button
  params.gridY = 2;
  params.gridWidth = 1;
  params.padBottom = 5;
  params.orientationX = orient_right;
  params.stickyFocus = 0;
  params.useDefaultBackground = 1;
  okButton = windowNewButton(dialogWindow, "OK", NULL, &params);
  if (okButton == NULL)
    return (status = ERR_NOCREATE);

  // Create the Cancel button
  params.gridX = 1;
  params.padLeft = 0;
  params.padRight = 5;
  params.orientationX = orient_left;
  cancelButton = windowNewButton(dialogWindow, "Cancel", NULL, &params);
  if (cancelButton == NULL)
    return (status = ERR_NOCREATE);

  if (parentWindow)
    windowCenterDialog(parentWindow, dialogWindow);
  windowSetVisible(dialogWindow, 1);
  
  while(1)
    {
      // Check for the OK button
      status = windowComponentEventGet(okButton, &event);
      if (status < 0)
	{
	  fileName[0] = '\0';
	  status = 0;
	  break;
	}
      else if ((status > 0) && (event.type == EVENT_MOUSE_LEFTUP))
	{
	  windowComponentGetData(textField, fileName, maxLength);
	  status = 1;
	  break;
	}

      // Check for the Cancel button
      status = windowComponentEventGet(cancelButton, &event);
      if ((status < 0) || ((status > 0) && (event.type == EVENT_MOUSE_LEFTUP)))
	{
	  fileName[0] = '\0';
	  status = 0;
	  break;
	}

      // Check for window close events
      status = windowComponentEventGet(dialogWindow, &event);
      if ((status < 0) || ((status > 0) && (event.type == EVENT_WINDOW_CLOSE)))
	{
	  fileName[0] = '\0';
	  status = 0;
	  break;
	}

      // Check for keyboard events 
      status = windowComponentEventGet(textField, &event);
      if (status < 0)
	{
	  fileName[0] = '\0';
	  status = 0;
	  break;
	}
      if (event.type == EVENT_KEY_DOWN)
	{
	  if (event.key == (unsigned char) 9)
	    {
	      // This is the TAB key.  Attempt to complete a filename.
	      windowComponentGetData(textField, fileName, maxLength);
	      vshCompleteFilename(fileName);
	      windowComponentSetData(textField, fileName, maxLength);
	    }
	  
	  else if (event.key == (unsigned char) 10)
	    {
	      windowComponentGetData(textField, fileName, maxLength);
	      status = 1;
	      break;
	    }
	}

      // Done
      multitaskerYield();
    }

  windowDestroy(dialogWindow);

  return (status);
}
