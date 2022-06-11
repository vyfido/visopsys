// 
//  Visopsys
//  Copyright (C) 1998-2005 J. Andrew McLaughlin
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
//  windowChoiceDialog.c
//

// This contains functions for user programs to operate GUI components.

#include <string.h>
#include <sys/window.h>
#include <sys/api.h>
#include <sys/errors.h>


static volatile image questImage;


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


_X_ int windowNewChoiceDialog(objectKey parentWindow, const char *title, const char *message, char *choiceStrings[], int numChoices, int defaultChoice)
{
  // Desc: Create a 'choice' dialog box, with the parent window 'parentWindow', the given titlebar text and main message, and 'numChoices' choices, as specified by the 'choiceStrings'.  'default' is the default focussed selection.  The dialog will have a button for each choice.  If the user chooses one of the choices, the function returns the 0-based index of the choice.  Otherwise it returns negative.  If 'parentWindow' is NULL, the dialog box is actually created as an independent window that looks the same as a dialog.  This is a blocking call that returns when the user closes the dialog window (i.e. the dialog is 'modal').

  int status = 0;
  objectKey dialogWindow = NULL;
  objectKey imageComp = NULL;
  objectKey buttonContainer = NULL;
  objectKey buttons[WINDOW_MAX_COMPONENTS];
  componentParameters params;
  windowEvent event;
  int choice = ERR_INVALID;
  int count;
  
  // Check params.  It's okay for parentWindow to be NULL.
  if ((title == NULL) || (message == NULL) || (choiceStrings == NULL))
    return (status = ERR_NULLPARAMETER);

  // Create the dialog.  Arbitrary size and coordinates
  if (parentWindow)
    dialogWindow = windowNewDialog(parentWindow, title);
  else
    dialogWindow = windowNew(multitaskerGetCurrentProcessId(), title);
  if (dialogWindow == NULL)
    return (status = ERR_NOCREATE);

  bzero(&params, sizeof(componentParameters));
  params.gridWidth = 1;
  params.gridHeight = 1;
  params.padLeft = 5;
  params.padRight = 5;
  params.padTop = 5;
  params.orientationX = orient_right;
  params.orientationY = orient_middle;
  params.useDefaultForeground = 1;
  params.useDefaultBackground = 1;

  if (questImage.data == NULL)
    status = imageLoad(QUESTIMAGE_NAME, 0, 0, (image *) &questImage);

  if (status == 0)
    {
      questImage.translucentColor.red = 0;
      questImage.translucentColor.green = 255;
      questImage.translucentColor.blue = 0;
      imageComp = windowNewImage(dialogWindow, (image *) &questImage,
				 draw_translucent, &params);
    }

  // Create the label
  params.gridX = 1;
  params.orientationX = orient_left;
  if (windowNewTextLabel(dialogWindow, message, &params) == NULL)
    {
      windowDestroy(dialogWindow);
      return (status = ERR_NOCREATE);
    }

  // Create the container for the buttons
  params.gridY = 1;
  params.padBottom = 5;
  params.fixedWidth = 1;
  params.orientationX = orient_center;
  buttonContainer =
    windowNewContainer(dialogWindow, "buttonContainer", &params);
  if (buttonContainer == NULL)
    {
      windowDestroy(dialogWindow);
      return (status = ERR_NOCREATE);
    }
  
  // Create the buttons
  params.gridY = 0;
  params.padTop = 0;
  params.padBottom = 0;
  for (count = 0; count < numChoices; count ++)
    {
      params.gridX = count;
      buttons[count] =
	windowNewButton(buttonContainer, choiceStrings[count], NULL, &params);
      if (buttons[count] == NULL)
	{
	  windowDestroy(dialogWindow);
	  return (status = ERR_NOCREATE);
	}
    }

  if ((defaultChoice >= 0) && (defaultChoice < numChoices))
    windowComponentFocus(buttons[defaultChoice]);

  if (parentWindow)
    windowCenterDialog(parentWindow, dialogWindow);
  windowSetVisible(dialogWindow, 1);
  
  while(1)
    {
      // Check for our buttons
      for (count = 0; count < numChoices; count ++)
	{
	  status = windowComponentEventGet(buttons[count], &event);
	  if ((status > 0) && (event.type == EVENT_MOUSE_LEFTUP))
	    {
	      choice = count;
	      break;
	    }
	}

      // Check for selections or window close events
      if ((choice >= 0) ||
	  ((windowComponentEventGet(dialogWindow, &event) > 0) &&
	   (event.type == EVENT_WINDOW_CLOSE)))
	{
	  windowDestroy(dialogWindow);
	  return (choice);
	}

      // Done
      multitaskerYield();
    }
}
