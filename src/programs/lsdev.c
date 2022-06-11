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

#define COLUMNS           60
#define NORMAL_ROWS       25
#define MORE_ROWS         40
#define SCROLLBACK_LINES  200

static int graphics = 0;
static int rows = NORMAL_ROWS;
static objectKey window = NULL;


static void printTree(device *dev, int level)
{
  device child;
  char vendor[64];
  char model[64];
  int count1, count2;

  while (1)
    {
      for (count1 = 0; count1 < level; count1 ++)
	printf("   ");

      vendor[0] = '\0';
      model[0] = '\0';
      variableListGet(&(dev->attrs), DEVICEATTRNAME_VENDOR, vendor, 64);
      variableListGet(&(dev->attrs), DEVICEATTRNAME_MODEL, model, 64);

      if (vendor[0] || model[0])
	{
	  if (vendor[0] && model[0])
	    printf("\"%s %s\" ", vendor, model);
	  else if (vendor[0])
	    printf("\"%s\" ", vendor);
	  else if (model[0])
	    printf("\"%s\" ", model);
	}

      if (dev->subClass.name[0])
	printf("%s ", dev->subClass.name);

      printf("%s\n", dev->class.name);

      // Print any additional attributes
      if (dev->attrs.numVariables > 2)
	{
	  for (count1 = 0; count1 < dev->attrs.numVariables; count1 ++)
	    {
	      for (count2 = 0; count2 <= level; count2 ++)
		printf("   ");
	      printf("%s=%s\n", dev->attrs.variables[count1],
		     dev->attrs.values[count1]);
	    }
	}

      if (deviceTreeGetChild(dev, &child) >= 0)
	printTree(&child, (level + 1));

      if (deviceTreeGetNext(dev) < 0)
	break;
    }

  if (graphics)
    // Scroll back to the very top
    textScroll(-(SCROLLBACK_LINES / rows));

  return;
}

static void quit(int) __attribute__((noreturn));
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
  if ((key == window) && (event->type == EVENT_WINDOW_CLOSE))
    quit(0);

  return;
}


static void constructWindow(void)
{
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
      rows = MORE_ROWS;
    }

  // Create a text area to show our stuff
  bzero(&params, sizeof(componentParameters));
  params.gridWidth = 1;
  params.gridHeight = 1;
  params.padLeft = 5;
  params.padRight = 5;
  params.padTop = 5;
  params.padBottom = 5;
  params.orientationX = orient_center;
  params.orientationY = orient_middle;
  textArea =
    windowNewTextArea(window, COLUMNS, rows, SCROLLBACK_LINES, &params);
  windowSetTextOutput(textArea);
  textSetCursor(0);
  textInputSetEcho(0);
  
  // Register an event handler to catch window close events
  windowRegisterEventHandler(window, &eventHandler);

  windowSetVisible(window, 1);

  return;
}


int main(int, char *[]) __attribute__((noreturn));
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
}
