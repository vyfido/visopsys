//
//  Visopsys
//  Copyright (C) 1998-2004 J. Andrew McLaughlin
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
static int readOnly = 1;
static int numberModes = 0;
static videoMode currentMode;
static videoMode videoModes[MAXVIDEOMODES];
static char *modeStrings[MAXVIDEOMODES];
static char stringData[MAXVIDEOMODES * 32];
static objectKey window = NULL;
static objectKey modeList = NULL;
static objectKey wallpaperButton = NULL;
static objectKey bootGraphicsCheckbox = NULL;
static objectKey okButton = NULL;
static objectKey cancelButton = NULL;


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


static void eventHandler(objectKey key, windowEvent *event)
{
  int mode = 0;
  char string[160];

  // Check for the window being closed by a GUI event.
  if (((key == window) && (event->type == EVENT_WINDOW_CLOSE)) ||
      ((key == cancelButton) && (event->type == EVENT_MOUSE_UP)))
    {
      windowGuiStop();
      windowDestroy(window);
      exit(0);
    }

  if ((key == wallpaperButton) && (event->type == EVENT_MOUSE_UP))
    system("/programs/wallpaper");

  if ((key == okButton) && (event->type == EVENT_MOUSE_UP))
    {
      // Does the user not want to boot in graphcis mode?
      if (!windowComponentGetSelected(bootGraphicsCheckbox))
	{
	  // Try to create the /nograph file
	  file tmp;
	  fileOpen("/nograph", (OPENMODE_WRITE | OPENMODE_CREATE |
				OPENMODE_TRUNCATE), &tmp);
	  fileClose(&tmp);
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

      windowGuiStop();
      windowDestroy(window);
      exit(0);
    }
}


static void constructWindow(void)
{
  // If we are in graphics mode, make a window rather than operating on the
  // command line.

  componentParameters params;
  int count;

  // Create a new window, with small, arbitrary size and location
  window = windowNew(processId, "Display properties");
  if (window == NULL)
    return;

  bzero(&params, sizeof(componentParameters));
  params.gridWidth = 2;
  params.gridHeight = 1;
  params.padTop = 5;
  params.padLeft = 5;
  params.padRight = 5;
  params.orientationX = orient_center;
  params.orientationY = orient_middle;
  params.useDefaultForeground = 1;
  params.useDefaultBackground = 1;

  // Make a list with all the available graphics modes
  modeList =
    windowNewList(window, NULL, 5, 1, 0, modeStrings, numberModes, &params);

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
  bootGraphicsCheckbox =
    windowNewCheckbox(window, NULL, "Boot in graphics mode", &params);
  windowComponentSetSelected(bootGraphicsCheckbox, 1);
  if (readOnly)
    windowComponentSetEnabled(bootGraphicsCheckbox, 0);

  // Create the background wallpaper button
  params.gridY = 2;
  wallpaperButton = windowNewButton(window, "Change background wallpaper",
				    NULL, &params);
  windowRegisterEventHandler(wallpaperButton, &eventHandler);

  // Create the OK button
  params.gridY = 3;
  params.gridWidth = 1;
  params.padBottom = 5;
  params.orientationX = orient_right;
  okButton = windowNewButton(window, "OK", NULL, &params);
  windowRegisterEventHandler(okButton, &eventHandler);

  // Create the Cancel button
  params.gridX = 1;
  params.padLeft = 0;
  params.orientationX = orient_left;
  cancelButton = windowNewButton(window, "Cancel", NULL, &params);
  windowRegisterEventHandler(cancelButton, &eventHandler);

  // Register an event handler to catch window close events
  windowRegisterEventHandler(window, &eventHandler);

  windowSetVisible(window, 1);

  return;
}


int main(int argc, char *argv[])
{
  int status = 0;
  char bootDisk[DISK_MAX_NAMELENGTH];

  // Only work in graphics mode
  if (!graphicsAreEnabled())
    {
      printf("\nThe \"%s\" command only works in graphics mode\n", argv[0]);
      errno = ERR_NOTINITIALIZED;
      return (status = errno);
    }

  // Find out whether we are currently running on a read-only filesystem
  if (!diskGetBoot(bootDisk))
    readOnly = diskGetReadOnly(bootDisk);

  // We need our process ID to create the windows
  processId = multitaskerGetCurrentProcessId();

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

  errno = status;
  return (status);
}
