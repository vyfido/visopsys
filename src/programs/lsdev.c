//
//  Visopsys
//  Copyright (C) 1998-2006 J. Andrew McLaughlin
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
//  lsdev.c
//

// Displays a tree of the system's hardware devices.

/* This is the text that appears when a user requests help about this program
<help>

 -- lsdev --

Display devices.

Usage:
  lsdev [-T]

This command will show a listing of the system's hardware devices.

Options:
-T              : Force text mode operation

</help>
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/api.h>

static int graphics = 0;
static objectKey window = NULL;
static objectKey okButton = NULL;


static void printTree(device *dev, int level)
{
  device child;
  int count;

  while (1)
    {
      for (count = 0; count < level; count ++)
	printf("   ");
      
      if (dev->model[0])
	printf("\"%s\" ", dev->model);

      if (dev->subClass.name[0])
	printf("%s ", dev->subClass.name);

      printf("%s\n", dev->class.name);

      if (deviceTreeGetChild(dev, &child) >= 0)
	printTree(&child, (level + 1));

      if (deviceTreeGetNext(dev) < 0)
	break;
    }
}


static void quit(int status)
{
  if (graphics)
    {
      windowGuiStop();

      if (window)
	windowDestroy(window);
    }

  exit(status);
}


static void eventHandler(objectKey key, windowEvent *event)
{
  // Check for the window being closed by a GUI event.
  if (((key == window) && (event->type == EVENT_WINDOW_CLOSE)) ||
      ((key == okButton) && (event->type == EVENT_MOUSE_LEFTUP)))
    quit(0);

  return;
}


static void constructWindow(void)
{
  int rows = 25;
  objectKey textArea = NULL;
  componentParameters params;

  // Create a new window
  window = windowNew(multitaskerGetCurrentProcessId(),
		     "System Device Information");
  if (window == NULL)
    return;

  if (fontLoad("/system/fonts/xterm-normal-10.bmp", "xterm-normal-10",
	       &(params.font), 1) < 0)
    {
      params.font = NULL;
      // The system font can comfortably show more rows
      rows = 40;
    }

  // Create a text area to show our stuff
  bzero(&params, sizeof(componentParameters));
  params.gridWidth = 1;
  params.gridHeight = 1;
  params.padLeft = 5;
  params.padRight = 5;
  params.padTop = 5;
  params.orientationX = orient_left;
  params.orientationY = orient_middle;
  params.useDefaultForeground = 1;
  params.useDefaultBackground = 1;
  textArea = windowNewTextArea(window, 60, rows, 200, &params);
  windowSetTextOutput(textArea);
  textSetCursor(0);
  textInputSetEcho(0);
  
  // Create a 'Stop' button
  params.gridY = 1;
  params.padBottom = 5;
  params.orientationX = orient_center;
  params.fixedWidth = 1;
  params.font = NULL;
  okButton = windowNewButton(window, "Ok", NULL, &params);
  windowComponentFocus(okButton);
  windowRegisterEventHandler(okButton, &eventHandler);

  // Register an event handler to catch window close events
  windowRegisterEventHandler(window, &eventHandler);

  windowSetVisible(window, 1);

  return;
}


int main(int argc, char *argv[])
{
  int status = 0;
  char opt;
  device dev;

  // Are graphics enabled?
  graphics = graphicsAreEnabled();
  
  while (strchr("T", (opt = getopt(argc, argv, "T"))))
    {
      // Force text mode?
      if (opt == 'T')
	graphics = 0;
    }

  if (graphics)
    constructWindow();

  status = deviceTreeGetRoot(&dev);
  if (status < 0)
    quit(status);

  printTree(&dev, 0);

  if (graphics)
    windowGuiRun();
  else
    printf("\n");

  quit(0);
  // Compiler happy
  return (status = 0);
}
