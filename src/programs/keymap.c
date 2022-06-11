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
//  keymap.c
//

// This is a program for showing and changing the keyboard mapping.  Works
// in both text and graphics modes.

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/api.h>

static int graphics = 0;
static char mapBuffer[1024];
static char *mapNames[64];
static int numMapNames = 0;
static objectKey window = NULL;
static objectKey mapList = NULL;
static objectKey okButton = NULL;
static objectKey cancelButton = NULL;


static int getMapNames(void)
{
  // Get the list of keyboard map names from the kernel

  int status = 0;
  char *buffPtr = NULL;
  int count;

  bzero(mapBuffer, 1024);

  status = keyboardGetMaps(mapBuffer, 1024);
  if (status < 0)
    return (status);

  numMapNames = status;
  buffPtr = mapBuffer;

  for (count = 0; count < numMapNames; count ++)
    {
      mapNames[count] = buffPtr;
      buffPtr += (strlen(mapNames[count]) + 1);
    }

  return (status = 0);
}


static int setMap(const char *mapName)
{
  int status = 0;

  // Change the mapping in the kernel config for the next reboot
  variableList *kernelConf = configurationReader("/system/kernel.conf");
  if (kernelConf)
    {
      status = keyboardSetMap(mapName);
      if (status < 0)
	return (status);

      variableListSet(kernelConf, "keyboard.map", mapName);
      configurationWriter("/system/kernel.conf", kernelConf);
      free(kernelConf);
    }

  return (status = 0);
}


static void eventHandler(objectKey key, windowEvent *event)
{
  // Check for the window being closed by a GUI event.
  if (((key == window) && (event->type == EVENT_WINDOW_CLOSE)) ||
      ((key == cancelButton) && (event->type == EVENT_MOUSE_LEFTUP)))
    {
      windowGuiStop();
    }

  if ((key == okButton) && (event->type == EVENT_MOUSE_LEFTUP))
    {
      int mapNumber = windowComponentGetSelected(mapList);  
      if (mapNumber < 0)
	return;
      setMap(mapNames[mapNumber]);

      windowGuiStop();
    }
}


static void constructWindow(void)
{
  // If we are in graphics mode, make a window rather than operating on the
  // command line.

  componentParameters params;

  // Create a new window
  window = windowNew(multitaskerGetCurrentProcessId(), "Keyboard Map");
  if (window == NULL)
    return;

  bzero(&params, sizeof(componentParameters));
  params.gridWidth = 2;
  params.gridHeight = 1;
  params.padLeft = 5;
  params.padRight = 5;
  params.padTop = 10;
  params.orientationX = orient_center;
  params.orientationY = orient_middle;
  params.useDefaultForeground = 1;
  params.useDefaultBackground = 1;

  mapList = windowNewList(window, 5, 1, 0, mapNames, numMapNames, &params);

  // Create an 'OK' button
  params.gridY = 1;
  params.gridWidth = 1;
  params.padTop = 5;
  params.padBottom = 5;
  params.orientationX = orient_right;
  okButton = windowNewButton(window, "OK", NULL, &params);
  windowRegisterEventHandler(okButton, &eventHandler);

  // Create a 'Cancel' button
  params.gridX = 1;
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
  char *buffPtr = NULL;
  int names = 0;
  int count;
  
  // Graphics enabled?
  graphics = graphicsAreEnabled();

  errno = getMapNames();
  if (errno)
    return (errno);

  if (graphics)
    {
      // Make our window
      constructWindow();

      // Run the GUI
      windowGuiRun();

      // ...and when we come back...
      windowDestroy(window);
    }
  else
    {
      printf("\n");

      if (argc > 1)
	{
	  // The user wants to set the keyboard map
	  errno = setMap(argv[1]);
	  if (errno)
	    return (errno);
	}

      // Print the list of keyboard maps
      names = keyboardGetMaps(mapBuffer, 1024);
      if (names <= 0)
	return (errno = names);
      
      buffPtr = mapBuffer;
      for (count = 0; count < names; count ++)
	{
	  printf("%s%s\n", buffPtr, ((count == 0)? " (current)" : ""));
	  buffPtr += (strlen(buffPtr) + 1);
	}

      errno = 0;
    }
  
  return (errno);
}
