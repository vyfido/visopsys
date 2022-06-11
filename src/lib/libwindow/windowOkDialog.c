// 
//  Visopsys
//  Copyright (C) 1998-2006 J. Andrew McLaughlin
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
//  windowOkDialog.c
//

// This contains functions for user programs to operate GUI components.

#include <string.h>
#include <sys/window.h>
#include <sys/api.h>
#include <sys/errors.h>


typedef enum {
  infoDialog, errorDialog
} dialogType;

static volatile image infoImage;
static volatile image errorImage;


static int okDialog(dialogType type, objectKey parentWindow, const char *title,
		    const char *message)
{
  // This will make a simple "OK" dialog message, and wait until the button
  // has been pressed.

  int status = 0;
  objectKey dialogWindow = NULL;
  char *imageName = NULL;
  image *myImage = NULL;
  objectKey imageComp = NULL;
  objectKey mainLabel = NULL;
  objectKey okButton = NULL;
  componentParameters params;
  windowEvent event;
  
  // Check params.  It's okay for parentWindow to be NULL.
  if ((title == NULL) || (message == NULL))
    return (status = ERR_NULLPARAMETER);

  bzero(&params, sizeof(componentParameters));
  params.gridWidth = 1;
  params.gridHeight = 1;
  params.padLeft = 5;
  params.padRight = 5;
  params.padTop = 5;
  params.orientationX = orient_center;
  params.orientationY = orient_middle;
  params.useDefaultForeground = 1;
  params.useDefaultBackground = 1;

  // Create the dialog.  Arbitrary size and coordinates
  if (parentWindow)
    dialogWindow = windowNewDialog(parentWindow, title);
  else
    dialogWindow = windowNew(multitaskerGetCurrentProcessId(), title);
  if (dialogWindow == NULL)
    return (status = ERR_NOCREATE);

  // If our 'info' image hasn't been loaded, try to load it
  if (type == infoDialog)
    {
      imageName = INFOIMAGE_NAME;
      myImage = (image *) &infoImage;
    }
  else if (type == errorDialog)
    {
      imageName = ERRORIMAGE_NAME;
      myImage = (image *) &errorImage;
    }

  if (myImage->data == NULL)
    status = imageLoad(imageName, 0, 0, myImage);

  if (status == 0)
    {
      myImage->translucentColor.red = 0;
      myImage->translucentColor.green = 255;
      myImage->translucentColor.blue = 0;
      params.padRight = 0;
      imageComp = windowNewImage(dialogWindow, myImage, draw_translucent,
				 &params);
    }

  // Create the label
  params.gridX = 1;
  params.padRight = 5;
  mainLabel = windowNewTextLabel(dialogWindow, message, &params);
  if (mainLabel == NULL)
    return (status = ERR_NOCREATE);

  // Create the button
  params.gridX = 0;
  params.gridY = 1;
  params.gridWidth = 2;
  params.padBottom = 5;
  params.fixedWidth = 1;
  okButton = windowNewButton(dialogWindow, "OK", NULL, &params);
  if (okButton == NULL)
    return (status = ERR_NOCREATE);

  if (parentWindow)
    windowCenterDialog(parentWindow, dialogWindow);
  windowSetVisible(dialogWindow, 1);

  while(1)
    {
      // Check for our OK button
      status = windowComponentEventGet(okButton, &event);
      if ((status < 0) || ((status > 0) && (event.type == EVENT_MOUSE_LEFTUP)))
	break;

      // Check for window close events
      status = windowComponentEventGet(dialogWindow, &event);
      if ((status < 0) || ((status > 0) && (event.type == EVENT_WINDOW_CLOSE)))
	break;

      // Done
      multitaskerYield();
    }
      
  windowDestroy(dialogWindow);
  return (status = 0);
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


_X_ int windowNewInfoDialog(objectKey parentWindow, const char *title, const char *message)
{
  // Desc: Create an 'info' dialog box, with the parent window 'parentWindow', and the given titlebar text and main message.  The dialog will have a single 'OK' button for the user to acknowledge.  If 'parentWindow' is NULL, the dialog box is actually created as an independent window that looks the same as a dialog.  This is a blocking call that returns when the user closes the dialog window (i.e. the dialog is 'modal').
  return(okDialog(infoDialog, parentWindow, title, message));
}


_X_ int windowNewErrorDialog(objectKey parentWindow, const char *title, const char *message)
{
  // Desc: Create an 'error' dialog box, with the parent window 'parentWindow', and the given titlebar text and main message.  The dialog will have a single 'OK' button for the user to acknowledge.  If 'parentWindow' is NULL, the dialog box is actually created as an independent window that looks the same as a dialog.  This is a blocking call that returns when the user closes the dialog window (i.e. the dialog is 'modal').
  return(okDialog(errorDialog, parentWindow, title, message));
}
