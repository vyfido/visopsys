// 
//  Visopsys
//  Copyright (C) 1998-2007 J. Andrew McLaughlin
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

static objectKey textField = NULL;


static void doFileSelection(file *theFile __attribute__((unused)),
			    char *fullName, loaderFileClass *loaderClass
			    __attribute__((unused)))
{
  windowComponentSetData(textField, fullName, (strlen(fullName) + 1));
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


_X_ int windowNewFileDialog(objectKey parentWindow, const char *title, const char *message, const char *startDir, char *fileName, unsigned maxLength)
{
  // Desc: Create a 'file' dialog box, with the parent window 'parentWindow', and the given titlebar text and main message.  If 'startDir' is a non-NULL directory name, the dialog will initially display the contents of that directory.  The dialog will have a file selection area, an 'OK' button and a 'CANCEL' button.  If the user presses OK or ENTER, the function returns the value 1 and copies the filename into the fileName buffer.  Otherwise it returns 0 and puts a NULL string into fileName.  If 'parentWindow' is NULL, the dialog box is actually created as an independent window that looks the same as a dialog.  This is a blocking call that returns when the user closes the dialog window (i.e. the dialog is 'modal').

  int status = 0;
  objectKey dialogWindow = NULL;
  componentParameters params;
  char cwd[MAX_PATH_LENGTH];
  objectKey textLabel = NULL;
  windowFileList *fileList = NULL;
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
  params.orientationX = orient_left;
  params.orientationY = orient_middle;
  textLabel = windowNewTextLabel(dialogWindow, message, &params);
  if (textLabel == NULL)
    return (status = ERR_NOCREATE);

  if (startDir != NULL)
    strncpy(cwd, startDir, MAX_PATH_LENGTH);
  else
    multitaskerGetCurrentDirectory(cwd, MAX_PATH_LENGTH);

  // Put a text field in the window for the user to type
  params.gridY = 1;
  fileList = windowNewFileList(dialogWindow, windowlist_icononly, 3, 4,
			       cwd, WINFILEBROWSE_CAN_CD, doFileSelection,
			       &params);
  if (fileList == NULL)
    return (status = ERR_NOCREATE);

  // Create the text field
  params.gridY = 2;
  params.flags = WINDOW_COMPFLAG_HASBORDER;
  textField = windowNewTextField(dialogWindow, 30, &params);
  if (textField == NULL)
    return (status = ERR_NOCREATE);
  windowComponentSetData(textField, cwd, MAX_PATH_LENGTH);

  // Create the OK button
  params.gridY = 3;
  params.gridWidth = 1;
  params.padLeft = 0;
  params.padRight = 5;
  params.padBottom = 5;
  params.orientationX = orient_right;
  params.flags = WINDOW_COMPFLAG_FIXEDWIDTH;
  okButton = windowNewButton(dialogWindow, "OK", NULL, &params);
  if (okButton == NULL)
    return (status = ERR_NOCREATE);

  // Create the Cancel button
  params.gridX = 1;
  params.padLeft = 5;
  params.padRight = 0;
  params.orientationX = orient_left;
  cancelButton = windowNewButton(dialogWindow, "Cancel", NULL, &params);
  if (cancelButton == NULL)
    return (status = ERR_NOCREATE);

  if (parentWindow)
    windowCenterDialog(parentWindow, dialogWindow);
  windowComponentFocus(fileList->key);
  windowSetVisible(dialogWindow, 1);
  
  while(1)
    {
      // Check for events to be passed to the file list widget
      if (windowComponentEventGet(fileList->key, &event) > 0)
	fileList->eventHandler(fileList, &event);

      // Check for the OK button, or 'enter' in the text field
      if (((windowComponentEventGet(okButton, &event) > 0) &&
	   (event.type == EVENT_MOUSE_LEFTUP)) ||
	  ((windowComponentEventGet(textField, &event) > 0) &&
	   (event.type == EVENT_KEY_DOWN) && (event.key == 10)))
	{
	  windowComponentGetData(textField, fileName, maxLength);
	  if (fileName[0] == '\0')
	    status = 0;
	  else
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

      // Done
      multitaskerYield();
    }

  fileList->destroy(fileList);
  windowDestroy(dialogWindow);

  return (status);
}
