//
//  Visopsys
//  Copyright (C) 1998-2007 J. Andrew McLaughlin
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

#define WINDOW_MANAGER_CONFIG  "/system/config/windowmanager.conf"
#define CLOCK_VARIABLE         "program.clock"
#define CLOCK_PROGRAM          "/programs/clock"

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
static listItemParameters *listItemParams = NULL;
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

  if (listItemParams)
    free(listItemParams);

  listItemParams = malloc(numberModes * sizeof(listItemParameters));
  if (listItemParams == NULL)
    return (status = ERR_MEMORY);

  // Construct the mode strings
  for (count = 0; count < numberModes; count ++)
    snprintf(listItemParams[count].text, WINDOW_MAX_LABEL_LENGTH,
	     " %d x %d, %d bit ",  videoModes[count].xRes,
	     videoModes[count].yRes, videoModes[count].bitsPerPixel);

  // Get the current mode
  graphicGetMode(&currentMode);

  return (status = 0);
}


static color *getSelectedColor(void)
{
  int selected = 0;

  windowComponentGetSelected(colorsRadio, &selected);
  
  switch (selected)
    {
    case 0:
    default:
      return (&foreground);
    case 1:
      return(&background);
    case 2:
      return(&desktop);
    }
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
  drawParams.width = windowComponentGetWidth(canvas);
  drawParams.height = windowComponentGetHeight(canvas);
  drawParams.thickness = 1;
  drawParams.fill = 1;
  windowComponentSetData(canvas, &drawParams, sizeof(windowDrawParameters));
}


static void eventHandler(objectKey key, windowEvent *event)
{
  color *selectedColor = getSelectedColor();
  int mode = 0;
  int clockSelected = 0;
  variableList list;
  char string[160];
  int selected = 0;
  file tmp;

  if (key == window)
    {
      // Check for the window being closed by a GUI event.
      if (event->type == EVENT_WINDOW_CLOSE)
	windowGuiStop();

      // Check for window resize
      if (event->type == EVENT_WINDOW_RESIZE)
	// Redraw the canvas
	drawColor(selectedColor);
    }

  else if ((key == wallpaperButton) && (event->type == EVENT_MOUSE_LEFTUP))
    {
      loaderLoadAndExec("/programs/wallpaper", privilege, 0);
      return;
    }

  else if ((key == colorsRadio) || (key == changeColorsButton))
    {
      if ((key == changeColorsButton) && (event->type == EVENT_MOUSE_LEFTUP))
	windowNewColorDialog(window, selectedColor);
 
      if (((key == changeColorsButton) &&
	   (event->type == EVENT_MOUSE_LEFTUP)) ||
	  ((key == colorsRadio) && (event->type & EVENT_SELECTION)))
	drawColor(selectedColor);
    }

  else if ((key == okButton) && (event->type == EVENT_MOUSE_LEFTUP))
    {
      // Does the user not want to boot in graphics mode?
      windowComponentGetSelected(bootGraphicsCheckbox, &selected);
      if (!selected)
	{
	  // Try to create the /nograph file
	  bzero(&tmp, sizeof(file));
	  fileOpen("/nograph", (OPENMODE_WRITE | OPENMODE_CREATE |
				OPENMODE_TRUNCATE), &tmp);
	  fileClose(&tmp);
	}

      // Does the user want to show a clock on the desktop?
      windowComponentGetSelected(showClockCheckbox, &clockSelected);
      if ((!showingClock && clockSelected) || (showingClock && !clockSelected))
	{
	  if (!readOnly)
	    configurationReader(WINDOW_MANAGER_CONFIG, &list);

	  if (!showingClock && clockSelected)
	    {
	      // Run the clock program now.  No block.
	      loaderLoadAndExec(CLOCK_PROGRAM, privilege, 0);
	      
	      if (list.memory)
		// Add a variable for the clock
		variableListSet(&list, CLOCK_VARIABLE, CLOCK_PROGRAM);
	    }
	  else
	    {
	      // Try to kill any clock program(s) currently running
	      multitaskerKillByName("clock", 0);

	      if (list.memory)
		// Remove any clock variable
		variableListUnset(&list, CLOCK_VARIABLE);
	    }

	  if (list.memory)
	    {
	      configurationWriter(WINDOW_MANAGER_CONFIG, &list);
	      variableListDestroy(&list);
	    }
	}

      windowComponentGetSelected(modeList, &mode);
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
      windowResetColors();

      windowGuiStop();
    }

  else if ((key == cancelButton) && (event->type == EVENT_MOUSE_LEFTUP))
    windowGuiStop();

  return;
}


static void constructWindow(void)
{
  // If we are in graphics mode, make a window rather than operating on the
  // command line.

  componentParameters params;
  objectKey container = NULL;
  process tmpProc;
  file tmpFile;
  int count;

  // Create a new window, with small, arbitrary size and location
  window = windowNew(processId, "Display Properties");
  if (window == NULL)
    return;

  bzero(&params, sizeof(componentParameters));
  params.gridWidth = 1;
  params.gridHeight = 1;
  params.padTop = 5;
  params.padLeft = 5;
  params.orientationX = orient_left;
  params.orientationY = orient_top;

  // Make a container for the left hand side components
  container = windowNewContainer(window, "leftContainer", &params);
  
  // Make a list with all the available graphics modes
  params.padTop = 0;
  params.padLeft = 0;
  params.orientationX = orient_center;
  modeList = windowNewList(container, windowlist_textonly, 5, 1, 0,
			   listItemParams, numberModes, &params);

  // Select the current mode
  for (count = 0; count < numberModes; count ++)
    if (videoModes[count].mode == currentMode.mode)
      {
	windowComponentSetSelected(modeList, count);
	break;
      }
  if (readOnly || privilege)
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

  if (fileFind(CLOCK_PROGRAM, &tmpFile) < 0)
    windowComponentSetEnabled(showClockCheckbox, 0);

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

  // A label for the colors
  params.gridY++;
  params.gridWidth = 1;
  params.padTop = 5;
  windowNewTextLabel(container, "Colors:", &params);

  // Create the colors radio button
  params.gridY++;
  params.gridHeight = 2;
  colorsRadio = windowNewRadioButton(container, 2, 1, (char *[])
      { "Foreground", "Background", "Desktop" }, 3 , &params);
  windowRegisterEventHandler(colorsRadio, &eventHandler);

  // The canvas to show the current color
  params.gridX = 1;
  params.gridHeight = 1;
  params.padLeft = 5;
  params.flags |= WINDOW_COMPFLAG_HASBORDER;
  canvas = windowNewCanvas(container, 50, 50, &params);

  // Create the change color button
  params.gridY++;
  params.flags &= ~WINDOW_COMPFLAG_HASBORDER;
  changeColorsButton = windowNewButton(container, "Change", NULL, &params);
  windowRegisterEventHandler(changeColorsButton, &eventHandler);

  // Adjust the canvas width so that it matches the width of the button.
  windowComponentSetWidth(canvas, windowComponentGetWidth(changeColorsButton));

  // Make a container for the OK/Cancel buttons
  params.gridX = 0;
  params.gridY = 1;
  params.gridWidth = 2;
  params.padLeft = 5;
  params.padRight = 5;
  params.padTop = 5;
  params.padBottom = 5;
  params.orientationX = orient_center;
  params.flags |= WINDOW_COMPFLAG_FIXEDHEIGHT;
  container = windowNewContainer(window, "buttonContainer", &params);

  // Create the OK button
  params.gridY = 0;
  params.gridWidth = 1;
  params.padLeft = 0;
  params.padTop = 0;
  params.padBottom = 0;
  params.orientationX = orient_right;
  params.flags |= WINDOW_COMPFLAG_FIXEDWIDTH;
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

  // Get the current colors we're interested in
  graphicGetColor("foreground", &foreground);
  graphicGetColor("background", &background);
  graphicGetColor("desktop", &desktop);

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

  if (listItemParams)
    free(listItemParams);

  errno = status;
  return (status);
}
