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
//  windowPromptDialog.c
//

// This contains functions for user programs to operate GUI components.

#include <string.h>
#include <sys/window.h>
#include <sys/api.h>
#include <sys/errors.h>


typedef enum {
  promptDialog, passwordDialog
} dialogType;


static int dialog(dialogType type, objectKey parentWindow, const char *title,
		  const char *message, int rows, int columns, char *buffer)
{
  // This will make a simple dialog with either a text field, password field,
  // or text area depending on the requested type and the number of rows
  // requested

  int status = 0;
  objectKey dialogWindow = NULL;
  componentParameters params;
  objectKey textLabel = NULL;
  objectKey field = NULL;
  objectKey okButton = NULL;
  objectKey cancelButton = NULL;
  windowEvent event;

  // Check params.  It's okay for parentWindow to be NULL.
  if ((title == NULL) || (message == NULL) || (buffer == NULL))
    return (status = ERR_NULLPARAMETER);

  bzero(&params, sizeof(componentParameters));
  buffer[0] = '\0';

  // Create the dialog.
  if (parentWindow)
    dialogWindow = windowNewDialog(parentWindow, title);
  else
    dialogWindow = windowNew(multitaskerGetCurrentProcessId(), title);
  if (dialogWindow == NULL)
    return (status = ERR_NOCREATE);

  params.gridWidth = 1;
  params.gridHeight = 1;
  params.padLeft = 5;
  params.padRight = 5;
  params.padTop = 5;
  params.orientationX = orient_center;
  params.orientationY = orient_top;
  params.useDefaultForeground = 1;
  params.useDefaultBackground = 1;

  params.gridWidth = 2;
  textLabel = windowNewTextLabel(dialogWindow, NULL, message, &params);

  params.gridY = 1;
  params.padTop = 5;
  params.hasBorder = 1;
  params.useDefaultBackground = 0;
  params.background.red = 255;
  params.background.green = 255;
  params.background.blue = 255;

  if (type == passwordDialog)
    field = windowNewPasswordField(dialogWindow, columns,
				   NULL /* default font*/, &params);
  else
    {
      if (rows <= 1)
	field = windowNewTextField(dialogWindow, columns,
				   NULL /* default font*/, &params);
      else
	field = windowNewTextArea(dialogWindow, columns, rows,
				  NULL /* default font*/, &params);
    }

  // Create the OK button
  params.gridY = 2;
  params.gridWidth = 1;
  params.padBottom = 5;
  params.orientationX = orient_right;
  params.useDefaultBackground = 1;
  okButton = windowNewButton(dialogWindow, "OK", NULL, &params);

  // Create the Cancel button
  params.gridX = 1;
  params.padLeft = 0;
  params.padRight = 5;
  params.orientationX = orient_left;
  cancelButton = windowNewButton(dialogWindow, "Cancel", NULL, &params);

  windowCenterDialog(parentWindow, dialogWindow);
  windowSetVisible(dialogWindow, 1);
  
  while(1)
    {
      // Check for the OK button
      status = windowComponentEventGet(okButton, &event);
      if (status < 0)
	break;
      else if ((status > 0) && (event.type == EVENT_MOUSE_UP))
	{
	  status = windowComponentGetData(field, buffer, (rows * columns));
	  if (status < 0)
	    break;
	  status = strlen(buffer);
	  break;
	}

      // Check for the Cancel button
      status = windowComponentEventGet(cancelButton, &event);
      if ((status < 0) || ((status > 0) && (event.type == EVENT_MOUSE_UP)))
	{
	  status = 0;
	  break;
	}
      
      // Check for window close events
      status = windowComponentEventGet(dialogWindow, &event);
      if ((status < 0) || ((status > 0) && (event.type == EVENT_WINDOW_CLOSE)))
	{
	  status = 0;
	  break;
	}

      // Check for keyboard events
      status = windowComponentEventGet(field, &event);
      if (status < 0)
	break;
      else if ((event.type == EVENT_KEY_DOWN) &&
	       (event.key == (unsigned char) 10))
	{
	  status = windowComponentGetData(field, buffer, (rows * columns));
	  if (status < 0)
	    break;
	  status = strlen(buffer);
	  break;
	}

      // Done
      multitaskerYield();
    }

  windowDestroy(dialogWindow);
  return (status);
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


_X_ int windowNewPromptDialog(objectKey parentWindow, const char *title, const char *message, int rows, int columns, char *buffer)
{
  // Desc: Create a 'prompt' dialog box, with the parent window 'parentWindow', and the given titlebar text and main message.  The dialog will have a single text field for the user to enter data.  If 'parentWindow' is NULL, the dialog box is actually created as an independent window that looks the same as a dialog.  This is a blocking call that returns when the user closes the dialog window (i.e. the dialog is 'modal').
  return(dialog(promptDialog, parentWindow, title, message, rows, columns,
		buffer));
}


_X_ int windowNewPasswordDialog(objectKey parentWindow, const char *title, const char *message, int columns, char *buffer)
{
  // Desc: Create an 'password' dialog box, with the parent window 'parentWindow', and the given titlebar text and main message.  The dialog will have a single password field.  If 'parentWindow' is NULL, the dialog box is actually created as an independent window that looks the same as a dialog.  This is a blocking call that returns when the user closes the dialog window (i.e. the dialog is 'modal').
  return(dialog(passwordDialog, parentWindow, title, message, 1, columns,
		buffer));
}