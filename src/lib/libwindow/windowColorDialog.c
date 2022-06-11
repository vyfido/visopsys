// 
//  Visopsys
//  Copyright (C) 1998-2011 J. Andrew McLaughlin
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

#include <libintl.h>
#include <string.h>
#include <stdio.h>
#include <sys/api.h>
#include <sys/errors.h>
#include <sys/window.h>

#define _(string) gettext(string)
#define TITLE          _("Color Chooser")
#define CANVAS_WIDTH   35
#define CANVAS_HEIGHT  100
#define SLIDER_WIDTH   100

extern int libwindow_initialized;
extern void libwindowInitialize(void);

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
  params.width = windowComponentGetWidth(canvas);
  params.height = windowComponentGetHeight(canvas);
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
  
  if (!libwindow_initialized)
    libwindowInitialize();

  // Check params.  It's okay for parentWindow to be NULL.
  if (pickedColor == NULL)
    return (status = ERR_NULLPARAMETER);

  // Create the dialog.  Arbitrary size and coordinates
  if (parentWindow)
    dialogWindow = windowNewDialog(parentWindow, TITLE);
  else
    dialogWindow =
      windowNew(multitaskerGetCurrentProcessId(), TITLE);
  if (dialogWindow == NULL)
    return (status = ERR_NOCREATE);

  // Copy the current color into the temporary color
  memcpy(&tmpColor, pickedColor, sizeof(color));

  bzero(&params, sizeof(componentParameters));
  params.gridWidth = 1;
  params.padLeft = 5;
  params.padTop = 5;
  params.orientationX = orient_left;
  params.orientationY = orient_top;

  // A canvas for drawing the color
  params.gridHeight = 6;
  params.flags = WINDOW_COMPFLAG_HASBORDER;
  canvas = windowNewCanvas(dialogWindow, CANVAS_WIDTH, CANVAS_HEIGHT, &params);

  // Red label and slider
  params.gridX += 1;
  params.gridHeight = 1;
  params.padLeft = 10;
  params.flags = 0;
  windowNewTextLabel(dialogWindow, _("Red"), &params);
  params.gridY += 1;
  redSlider = windowNewSlider(dialogWindow, scrollbar_horizontal,
			      SLIDER_WIDTH, 0, &params);
  if (redSlider == NULL)
    return (status = ERR_NOCREATE);
  scrollState.displayPercent = 20;
  scrollState.positionPercent = ((tmpColor.red * 100) / 255);
  windowComponentSetData(redSlider, &scrollState, sizeof(scrollBarState));

  // Green label and slider
  params.gridY += 1;
  windowNewTextLabel(dialogWindow, _("Green"), &params);
  params.gridY += 1;
  greenSlider = windowNewSlider(dialogWindow, scrollbar_horizontal,
				SLIDER_WIDTH, 0, &params);
  if (greenSlider == NULL)
    return (status = ERR_NOCREATE);
  scrollState.positionPercent = ((tmpColor.green * 100) / 255);
  windowComponentSetData(greenSlider, &scrollState, sizeof(scrollBarState));

  // Blue label and slider
  params.gridY += 1;
  windowNewTextLabel(dialogWindow, _("Blue"), &params);
  params.gridY += 1;
  blueSlider = windowNewSlider(dialogWindow, scrollbar_horizontal,
			       SLIDER_WIDTH, 0, &params);
  if (blueSlider == NULL)
    return (status = ERR_NOCREATE);
  scrollState.positionPercent = ((tmpColor.blue * 100) / 255);
  windowComponentSetData(blueSlider, &scrollState, sizeof(scrollBarState));

  // Make labels to show the numerical color values
  params.gridX += 1;
  params.gridY = 1;
  params.padLeft = 5;
  params.padRight = 5;
  redLabel = windowNewTextLabel(dialogWindow, "000", &params);
  if (redLabel == NULL)
    return (status = ERR_NOCREATE);
  params.gridY += 2;
  greenLabel = windowNewTextLabel(dialogWindow, "000", &params);
  if (greenLabel == NULL)
    return (status = ERR_NOCREATE);
  params.gridY += 2;
  blueLabel = windowNewTextLabel(dialogWindow, "000", &params);
  if (blueLabel == NULL)
    return (status = ERR_NOCREATE);

  // Make a container for the buttons
  params.gridX = 0;
  params.gridY += 1;
  params.gridWidth = 3;
  params.padTop = 10;
  params.padBottom = 5;
  params.orientationX = orient_center;
  params.flags = WINDOW_COMPFLAG_FIXEDWIDTH;
  buttonContainer =
    windowNewContainer(dialogWindow, "buttonContainer", &params);
  if (buttonContainer == NULL)
    return (status = ERR_NOCREATE);

  // Create the OK button
  params.gridY = 0;
  params.gridWidth = 1;
  params.padTop = 0;
  params.padBottom = 0;
  params.padLeft = 0;
  params.padRight = 0;
  params.orientationX = orient_right;
  okButton = windowNewButton(buttonContainer, _("OK"), NULL, &params);
  if (okButton == NULL)
    return (status = ERR_NOCREATE);

  // Create the Cancel button
  params.gridX = 1;
  params.padLeft = 5;
  params.padRight = 0;
  params.orientationX = orient_left;
  cancelButton = windowNewButton(buttonContainer, _("Cancel"), NULL, &params);
  if (cancelButton == NULL)
    return (status = ERR_NOCREATE);
  windowComponentFocus(cancelButton);

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
	  tmpColor.red = ((scrollState.positionPercent * 255) / 100);
	  drawColor(&tmpColor);
	}
      status = windowComponentEventGet(greenSlider, &event);
      if (status > 0)
	{
	  windowComponentGetData(greenSlider, &scrollState,
				 sizeof(scrollBarState));
	  tmpColor.green = ((scrollState.positionPercent * 255) / 100);
	  drawColor(&tmpColor);
	}
      status = windowComponentEventGet(blueSlider, &event);
      if (status > 0)
	{
	  windowComponentGetData(blueSlider, &scrollState,
				 sizeof(scrollBarState));
	  tmpColor.blue = ((scrollState.positionPercent * 255) / 100);
	  drawColor(&tmpColor);
	}

      // Check for our OK button
      status = windowComponentEventGet(okButton, &event);
      if ((status > 0) && (event.type == EVENT_MOUSE_LEFTUP))
	{
	  // Copy the temporary color into picked color
	  memcpy(pickedColor, &tmpColor, sizeof(color));
	  break;
	}

      // Check for the cancel button
      status = windowComponentEventGet(cancelButton, &event);
      if ((status < 0) || ((status > 0) && (event.type == EVENT_MOUSE_LEFTUP)))
	break;
      
      // Check for window events
      status = windowComponentEventGet(dialogWindow, &event);
      if (status > 0)
	{
	  if (event.type == EVENT_WINDOW_CLOSE)
	    break;
	  else if (event.type == EVENT_WINDOW_RESIZE)
	    drawColor(&tmpColor);
	}

      // Done
      multitaskerYield();
    }
      
  windowDestroy(dialogWindow);
  return (status = 0);
}
