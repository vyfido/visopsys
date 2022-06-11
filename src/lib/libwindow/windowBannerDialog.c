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
//  windowBannerDialog.c
//

// This contains functions for user programs to operate GUI components.

#include <string.h>
#include <sys/window.h>
#include <sys/api.h>
#include <sys/errors.h>


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


_X_ objectKey windowNewBannerDialog(objectKey parentWindow, const char *title, const char *message)
{
  // Desc: Create a 'banner' dialog box, with the parent window 'parentWindow', and the given titlebar text and main message.  This is the very simplest kind of dialog; it just contains the supplied message with no acknowledgement mechanism for the user.  If 'parentWindow' is NULL, the dialog box is actually created as an independent window that looks the same as a dialog.  This is a non-blocking call that returns the object key of the dialog window.  The caller must destroy the window when finished with it.

  objectKey dialogWindow = NULL;
  componentParameters params;
  
  // Check params.  It's okay for parentWindow to be NULL.
  if ((title == NULL) || (message == NULL))
    return (dialogWindow = NULL);

  bzero(&params, sizeof(componentParameters));

  params.gridWidth = 1;
  params.gridHeight = 1;
  params.padLeft = 5;
  params.padRight = 5;
  params.padTop = 5;
  params.padBottom = 5;
  params.orientationX = orient_center;
  params.orientationY = orient_middle;

  // Create the dialog.  Arbitrary size and coordinates
  if (parentWindow)
    dialogWindow = windowNewDialog(parentWindow, title);
  else
    dialogWindow = windowNew(multitaskerGetCurrentProcessId(), title);
  if (dialogWindow == NULL)
    return (dialogWindow);

  // Create the label
  if (windowNewTextLabel(dialogWindow, message, &params) == NULL)
    return (dialogWindow = NULL);

  // No need for a close button because there's no handler for it
  windowRemoveCloseButton(dialogWindow);

  if (parentWindow)
    windowCenterDialog(parentWindow, dialogWindow);

  windowSetVisible(dialogWindow, 1);

  return (dialogWindow);
}
