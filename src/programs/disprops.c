//
//  Visopsys
//  Copyright (C) 1998-2005 J. Andrew McLaughlin
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
//  disprops.c
//

// Sets display properties for the kernel's window manager

/* This is the text that appears when a user requests help about this program
<help>

 -- disprops --

Control the display properties

Usage:
  disprops

The disprops program is interactive, and may only be used in graphics mode.
It can be used to change display settings, such as the screen resolution,
the background wallpaper, and the base colors used by the window manager.

</help>
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/image.h>
#include <sys/window.h>
#include <sys/api.h>

typedef struct {
  char description[32];
  videoMode mode; 
} modeInfo;

static int processId = 0;
static int privilege = 0;
static int readOnly = 1;
static int numberModes = 0;
static int showingClock = 0;
static videoMode currentMode;
static videoMode videoModes[MAXVIDEOMODES];
static char *modeStrings[MAXVIDEOMODES];
static char stringData[MAXVIDEOMODES * 32];
static objectKey window = NULL;
static objectKey modeList = NULL;
static objectKey bootGraphicsCheckbox = NULL;
static objectKey showClockCheckbox = NULL;
static objectKey wallpaperButton = NULL;
static objectKey colorsRadio = NULL;
static objectKey canvas = NULL;
static objectKey changeColorsButton = NULL;
static objectKey okButton = NULL;
static objectKey cancelButton = NULL;

static int canvasWidth = 50;
static color foreground = { 171, 93, 40 };
static color background = { 171, 93, 40 };
static color desktop = { 171, 93, 40 };


static int getVideoModes(void)
{
  int status = 0;
  int count;

  // Try to get the supported video modes from the kernel
  numberModes = graphicGetModes(videoModes, sizeof(videoModes));
  if (numberModes <= 0)
    return (status = ERR_NODATA);

  // Construct the mode strings
  modeStrings[0] = stringData;
  for (count = 0; count < numberModes; count ++)
    {
      sprintf(modeStrings[count], " %d x %d, %d bit ",  videoModes[count].xRes,
	      videoModes[count].yRes, videoModes[count].bitsPerPixel);
      if (count < (numberModes - 1))
	modeStrings[count + 1] =
	  (modeStrings[count] + strlen(modeStrings[count]) + 1);
    }

  // Get the current mode
  graphicGetMode(&currentMode);

  return (status = 0);
}


static void drawColor(color *draw)
{
  // Draw the current color on the canvas

  windowDrawParameters drawParams;

  bzero(&drawParams, sizeof(windowDrawParameters));
  drawParams.operation = draw_rect;
  drawParams.mode = draw_normal;
  drawParams.foreground.red = draw->red;
  drawParams.foreground.green = draw->green;
  drawParams.foreground.blue = draw->blue;
  drawParams.xCoord1 = 0;
  drawParams.yCoord1 = 0;
  drawParams.width = canvasWidth;
  drawParams.height = 50;
  drawParams.thickness = 1;
  drawParams.fill = 1;
  windowComponentSetData(canvas, &drawParams, sizeof(windowDrawParameters));
}


static void eventHandler(objectKey key, windowEvent *event)
{
  int mode = 0;
  int clockSelected = 0;
  variableList list;
  char string[160];
  file tmp;

  // Check for the window being closed by a GUI event.
  if (((key == window) && (event->type == EVENT_WINDOW_CLOSE)) ||
      ((key == cancelButton) && (event->type == EVENT_MOUSE_LEFTUP)))
    windowGuiStop();

  else if ((key == wallpaperButton) && (event->type == EVENT_MOUSE_LEFTUP))
    {
      loaderLoadAndExec("/programs/wallpaper", privilege, 0, NULL, 0);
      return;
    }

  else if ((key == colorsRadio) || (key == changeColorsButton))
    {
      color *selectedColor = NULL;
      int selected = windowComponentGetSelected(colorsRadio);

      switch (selected)
	{
	case 0:
	  selectedColor = &foreground;
	  break;
	case 1:
	  selectedColor = &background;
	  break;
	case 2:
	  selectedColor = &desktop;
	  break;
	}

      if ((key == changeColorsButton) && (event->type == EVENT_MOUSE_LEFTUP))
	windowNewColorDialog(window, selectedColor);
 
      if (((key == changeColorsButton) &&
	   (event->type == EVENT_MOUSE_LEFTUP)) ||
	  ((key == colorsRadio) && (event->type == EVENT_MOUSE_LEFTDOWN)))
	drawColor(selectedColor);
    }

  else if ((key == okButton) && (event->type == EVENT_MOUSE_LEFTUP))
    {
      // Does the user not want to boot in graphics mode?
      if (!windowComponentGetSelected(bootGraphicsCheckbox))
	{
	  // Try to create the /nograph file
	  bzero(&tmp, sizeof(file));
	  fileOpen("/nograph", (OPENMODE_WRITE | OPENMODE_CREATE |
				OPENMODE_TRUNCATE), &tmp);
	  fileClose(&tmp);
	}

      // Does the user want to show a clock on the desktop?
      clockSelected = windowComponentGetSelected(showClockCheckbox);
      if ((!showingClock && clockSelected) || (showingClock && !clockSelected))
	{
	  if (!readOnly)
	    configurationReader("/system/windowmanager.conf", &list);

	  if (!showingClock && clockSelected)
	    {
	      // Run the clock program now.  No block.
	      loaderLoadAndExec("/programs/clock", privilege, 0, NULL, 0);
	      
	      if (list.memory)
		// Add a variable for the clock
		variableListSet(&list, "program.clock", "/programs/clock");
	    }
	  else
	    {
	      // Try to kill any clock program(s) currently running
	      multitaskerKillByName("clock", 0);

	      if (list.memory)
		// Remove any 'program.clock=' variable
		variableListUnset(&list, "program.clock");
	    }

	  if (list.memory)
	    {
	      configurationWriter("/system/windowmanager.conf", &list);
	      free(list.memory);
	    }
	}

      mode = windowComponentGetSelected(modeList);
      if ((mode >= 0) && (videoModes[mode].mode != currentMode.mode))
	{
	  if (!graphicSetMode(&(videoModes[mode])))
	    {
	      sprintf(string, "The resolution has been changed to %dx%d, "
		      "%dbpp\nThis will take effect after you reboot.",
		      videoModes[mode].xRes, videoModes[mode].yRes,
		      videoModes[mode].bitsPerPixel);
	      windowNewInfoDialog(window, "Changed", string);
	    }
	  else
	    {
	      sprintf(string, "Error %d setting mode", mode);
	      windowNewErrorDialog(window, "Error", string);
	    }
	}

      // Set the colors
      graphicSetColor("foreground", &foreground);
      graphicSetColor("background", &background);
      graphicSetColor("desktop", &desktop);

      windowGuiStop();
    }

  return;
}


static void constructWindow(void)
{
  // If we are in graphics mode, make a window rather than operating on the
  // command line.

  componentParameters params;
  objectKey container = NULL;
  process tmpProc;
  int count;

  // Create a new window, with small, arbitrary size and location
  window = windowNew(processId, "Display properties");
  if (window == NULL)
    return;

  bzero(&params, sizeof(componentParameters));
  params.gridWidth = 1;
  params.gridHeight = 1;
  params.padTop = 5;
  params.padLeft = 5;
  params.orientationX = orient_left;
  params.orientationY = orient_top;
  params.useDefaultForeground = 1;
  params.useDefaultBackground = 1;

  // Make a container for the left hand side components
  container = windowNewContainer(window, "leftContainer", &params);
  
  // Make a list with all the available graphics modes
  params.padTop = 0;
  params.padLeft = 0;
  params.orientationX = orient_center;
  modeList =
    windowNewList(container, 5, 1, 0, modeStrings, numberModes, &params);

  // Select the current mode
  for (count = 0; count < numberModes; count ++)
    if (videoModes[count].mode == currentMode.mode)
      {
	windowComponentSetSelected(modeList, count);
	break;
      }
  if (readOnly)
    windowComponentSetEnabled(modeList, 0);

  // Make a checkbox for whether to boot in graphics mode
  params.gridY = 1;
  params.padTop = 5;
  params.orientationX = orient_left;
  bootGraphicsCheckbox =
    windowNewCheckbox(container, "Boot in graphics mode", &params);
  windowComponentSetSelected(bootGraphicsCheckbox, 1);
  if (readOnly)
    windowComponentSetEnabled(bootGraphicsCheckbox, 0);

  // Make a checkbox for whether to show the clock on the desktop
  params.gridY = 2;
  showClockCheckbox =
    windowNewCheckbox(container, "Show a clock on the desktop", &params);

  // Are we currently set to show one?
  bzero(&tmpProc, sizeof(process));
  if (multitaskerGetProcessByName("clock", &tmpProc) == 0)
    {
      showingClock = 1;
      windowComponentSetSelected(showClockCheckbox, showingClock);
    }

  // Make a container for the right hand side components
  params.gridX = 1;
  params.gridY = 0;
  params.padTop = 5;
  params.padLeft = 5;
  params.padRight = 5;
  container = windowNewContainer(window, "rightContainer", &params);

  // Create the background wallpaper button
  params.gridX = 0;
  params.gridWidth = 2;
  params.padTop = 0;
  params.padLeft = 0;
  params.padRight = 0;
  wallpaperButton =
    windowNewButton(container, "Background wallpaper", NULL, &params);
  windowRegisterEventHandler(wallpaperButton, &eventHandler);

  params.gridY = 1;
  params.gridWidth = 1;
  params.padTop = 5;
  windowNewTextLabel(container, "Colors:", &params);

  // Create the colors radio button
  params.gridY = 2;
  params.gridHeight = 2;
  colorsRadio = windowNewRadioButton(container, 2, 1, (char *[])
      { "Foreground", "Background", "Desktop" }, 3 , &params);
  windowRegisterEventHandler(colorsRadio, &eventHandler);

  // Create the change color button
  params.gridX = 1;
  params.gridY = 3;
  params.gridHeight = 1;
  params.padLeft = 5;
  changeColorsButton = windowNewButton(container, "Change", NULL, &params);
  windowRegisterEventHandler(changeColorsButton, &eventHandler);

  // Get the current colors we're interested in
  graphicGetColor("foreground", &foreground);
  graphicGetColor("background", &background);
  graphicGetColor("desktop", &desktop);

  // The canvas to show the current color
  params.gridY = 2;
  params.hasBorder = 1;
  canvasWidth = windowComponentGetWidth(changeColorsButton);
  canvas = windowNewCanvas(container, canvasWidth, 50, &params);

  // Make a container for the OK/Cancel buttons
  params.gridX = 0;
  params.gridY = 1;
  params.gridWidth = 2;
  params.padLeft = 5;
  params.padRight = 5;
  params.padTop = 5;
  params.padBottom = 5;
  params.orientationX = orient_center;
  params.fixedHeight = 1;
  params.hasBorder = 0;
  container = windowNewContainer(window, "buttonContainer", &params);

  // Create the OK button
  params.gridY = 0;
  params.gridWidth = 1;
  params.padLeft = 0;
  params.padTop = 0;
  params.padBottom = 0;
  params.orientationX = orient_right;
  okButton = windowNewButton(container, "OK", NULL, &params);
  windowRegisterEventHandler(okButton, &eventHandler);

  // Create the Cancel button
  params.gridX = 1;
  params.padLeft = 5;
  params.padRight = 0;
  params.orientationX = orient_left;
  cancelButton = windowNewButton(container, "Cancel", NULL, &params);
  windowRegisterEventHandler(cancelButton, &eventHandler);

  // Register an event handler to catch window close events
  windowRegisterEventHandler(window, &eventHandler);
  
  windowSetVisible(window, 1);

  drawColor(&foreground);

  return;
}


int main(int argc, char *argv[])
{
  int status = 0;
  disk sysDisk;

  // Only work in graphics mode
  if (!graphicsAreEnabled())
    {
      printf("\nThe \"%s\" command only works in graphics mode\n", argv[0]);
      errno = ERR_NOTINITIALIZED;
      return (status = errno);
    }

  // We don't use argc.  This keeps the compiler happy
  argc = 0;

  // Find out whether we are currently running on a read-only filesystem
  if (!fileGetDisk("/system", &sysDisk))
    readOnly = sysDisk.readOnly;

  // We need our process ID and privilege to create the windows
  processId = multitaskerGetCurrentProcessId();
  privilege = multitaskerGetProcessPrivilege(processId);

  // Get the list of supported video modes
  status = getVideoModes();
  if (status < 0)
    {
      errno = status;
      return (status);
    }

  // Make the window
  constructWindow();

  // Run the GUI
  windowGuiRun();
  windowDestroy(window);

  errno = status;
  return (status);
}
