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
//  windowColorDialog.c
//

// This contains functions for user programs to operate GUI components.

#include <string.h>
#include <stdio.h>
#include <sys/window.h>
#include <sys/api.h>
#include <sys/errors.h>

#define CANVAS_WIDTH  35
#define CANVAS_HEIGHT 100
#define SLIDER_HEIGHT 100

static objectKey canvas = NULL;
static objectKey redLabel = NULL;
static objectKey greenLabel = NULL;
static objectKey blueLabel = NULL;


static void drawColor(color *draw)
{
  // Draw the current color on the canvas

  windowDrawParameters params;
  char colorString[4];

  // Clear our drawing parameters
  bzero(&params, sizeof(windowDrawParameters));
  params.operation = draw_rect;
  params.mode = draw_normal;
  params.foreground.red = draw->red;
  params.foreground.green = draw->green;
  params.foreground.blue = draw->blue;
  params.xCoord1 = 0;
  params.yCoord1 = 0;
  params.width = CANVAS_WIDTH;
  params.height = CANVAS_HEIGHT;
  params.thickness = 1;
  params.fill = 1;
  windowComponentSetData(canvas, &params, sizeof(windowDrawParameters));

  sprintf(colorString, "%03d", draw->red);
  windowComponentSetData(redLabel, colorString, 4);
  sprintf(colorString, "%03d", draw->green);
  windowComponentSetData(greenLabel, colorString, 4);
  sprintf(colorString, "%03d", draw->blue);
  windowComponentSetData(blueLabel, colorString, 4);
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


_X_ int windowNewColorDialog(objectKey parentWindow, color *pickedColor)
{
  // Desc: Create an 'color chooser' dialog box, with the parent window 'parentWindow', and a pointer to the color structure 'pickedColor'.  Currently the window consists of red/green/blue sliders and a canvas displaying the current color.  The initial color displayed will be whatever is supplied in 'pickedColor'.  If 'parentWindow' is NULL, the dialog box is actually created as an independent window that looks the same as a dialog.  This is a blocking call that returns when the user closes the dialog window (i.e. the dialog is 'modal').

  int status = 0;
  objectKey dialogWindow = NULL;
  objectKey redSlider = NULL;
  objectKey greenSlider = NULL;
  objectKey blueSlider = NULL;
  objectKey buttonContainer = NULL;
  objectKey okButton = NULL;
  objectKey cancelButton = NULL;
  componentParameters params;
  windowEvent event;
  color tmpColor;
  scrollBarState scrollState;
  
  // Check params.  It's okay for parentWindow to be NULL.
  if (pickedColor == NULL)
    return (status = ERR_NULLPARAMETER);

  // Create the dialog.  Arbitrary size and coordinates
  if (parentWindow)
    dialogWindow = windowNewDialog(parentWindow, "Color chooser");
  else
    dialogWindow =
      windowNew(multitaskerGetCurrentProcessId(), "Color chooser");
  if (dialogWindow == NULL)
    return (status = ERR_NOCREATE);

  // Copy the current color into the temporary color
  bcopy(pickedColor, &tmpColor, sizeof(color));

  bzero(&params, sizeof(componentParameters));
  params.gridWidth = 1;
  params.gridHeight = 1;
  params.padLeft = 5;
  params.padTop = 5;
  params.orientationX = orient_center;
  params.orientationY = orient_middle;
  params.useDefaultForeground = 1;
  params.useDefaultBackground = 1;

  // Create labels for the red, green, and blue colors
  params.gridX = 1;
  windowNewTextLabel(dialogWindow, NULL, "Red", &params);
  params.gridX = 2;
  windowNewTextLabel(dialogWindow, NULL, "Green", &params);
  params.gridX = 3;
  params.padRight = 5;
  windowNewTextLabel(dialogWindow, NULL, "Blue", &params);

  // Get a canvas for drawing the color
  params.gridX = 0;
  params.gridY = 1;
  params.hasBorder = 1;
  canvas = windowNewCanvas(dialogWindow, CANVAS_WIDTH, CANVAS_HEIGHT, &params);

  // Create scroll bars for the red, green, and blue colors
  params.gridX = 1;
  params.hasBorder = 0;
  redSlider = windowNewScrollBar(dialogWindow, scrollbar_vertical, 0,
				 SLIDER_HEIGHT, &params);
  scrollState.displayPercent = 20;
  scrollState.positionPercent = (100 - ((tmpColor.red * 100) / 255));
  windowComponentSetData(redSlider, &scrollState, sizeof(scrollBarState));

  params.gridX = 2;
  greenSlider = windowNewScrollBar(dialogWindow, scrollbar_vertical, 0,
				   SLIDER_HEIGHT, &params);
  scrollState.positionPercent = (100 - ((tmpColor.green * 100) / 255));
  windowComponentSetData(greenSlider, &scrollState, sizeof(scrollBarState));

  params.gridX = 3;
  params.padRight = 5;
  blueSlider = windowNewScrollBar(dialogWindow, scrollbar_vertical, 0,
				  SLIDER_HEIGHT, &params);
  scrollState.positionPercent = (100 - ((tmpColor.blue * 100) / 255));
  windowComponentSetData(blueSlider, &scrollState, sizeof(scrollBarState));

  // Make labels to show the numerical color values
  params.gridX = 1;
  params.gridY = 2;
  params.padRight = 0;
  redLabel = windowNewTextLabel(dialogWindow, NULL, "000", &params);
  params.gridX = 2;
  greenLabel = windowNewTextLabel(dialogWindow, NULL, "000", &params);
  params.gridX = 3;
  blueLabel = windowNewTextLabel(dialogWindow, NULL, "000", &params);

  // Make a panel for the buttons
  params.gridX = 0;
  params.gridY = 3;
  params.gridWidth = 4;
  params.padRight = 5;
  params.padTop = 5;
  params.padBottom = 5;
  params.hasBorder = 0;
  buttonContainer =
    windowNewContainer(dialogWindow, "buttonContainer", &params);

  // Create the OK button
  params.gridY = 0;
  params.gridWidth = 1;
  params.padTop = 0;
  params.padBottom = 0;
  params.padLeft = 0;
  params.orientationX = orient_right;
  okButton = windowNewButton(buttonContainer, "OK", NULL, &params);
  if (okButton == NULL)
    return (status = ERR_NOCREATE);

  // Create the Cancel button
  params.gridX = 1;
  params.padLeft = 5;
  params.padRight = 0;
  params.orientationX = orient_left;
  cancelButton = windowNewButton(buttonContainer, "Cancel", NULL, &params);
  if (cancelButton == NULL)
    return (status = ERR_NOCREATE);

  if (parentWindow)
    windowCenterDialog(parentWindow, dialogWindow);
  windowSetVisible(dialogWindow, 1);

  // Draw the current color on the canvas
  drawColor(&tmpColor);

  while(1)
    {
      // Check for sliders
      status = windowComponentEventGet(redSlider, &event);
      if (status > 0)
	{
	  windowComponentGetData(redSlider, &scrollState,
				 sizeof(scrollBarState));
	  tmpColor.red = (((100 - scrollState.positionPercent) * 255) / 100);
	  drawColor(&tmpColor);
	}
      status = windowComponentEventGet(greenSlider, &event);
      if (status > 0)
	{
	  windowComponentGetData(greenSlider, &scrollState,
				 sizeof(scrollBarState));
	  tmpColor.green = (((100 - scrollState.positionPercent) * 255) / 100);
	  drawColor(&tmpColor);
	}
      status = windowComponentEventGet(blueSlider, &event);
      if (status > 0)
	{
	  windowComponentGetData(blueSlider, &scrollState,
				 sizeof(scrollBarState));
	  tmpColor.blue = (((100 - scrollState.positionPercent) * 255) / 100);
	  drawColor(&tmpColor);
	}

      // Check for our OK button
      status = windowComponentEventGet(okButton, &event);
      if ((status > 0) && (event.type == EVENT_MOUSE_LEFTUP))
	{
	  // Copy the temporary color into picked color
	  bcopy(&tmpColor, pickedColor, sizeof(color));
	  break;
	}

      // Check for the cancel button or window close events
      status = windowComponentEventGet(dialogWindow, &event);
      if ((status < 0) || ((status > 0) && (event.type == EVENT_WINDOW_CLOSE)))
	break;
      status = windowComponentEventGet(cancelButton, &event);
      if ((status < 0) || ((status > 0) && (event.type == EVENT_MOUSE_LEFTUP)))
	break;

      // Done
      multitaskerYield();
    }
      
  windowDestroy(dialogWindow);
  return (status = 0);
}
