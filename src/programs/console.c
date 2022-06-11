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
//  console.c
//

// This command will display/close the console window

#include <stdio.h>
#include <string.h>
#include <sys/window.h>
#include <sys/api.h>


objectKey window = NULL;


static void eventHandler(objectKey key, windowEvent *event)
{
  // This is just to handle a window shutdown event.

  if ((key == window) && (event->type == EVENT_WINDOW_CLOSE))
    windowGuiStop();
}


int main(int argc, char *argv[])
{
  int status = 0;
  int processId = 0;
  componentParameters params;
  int count;

  // Make sure none of our arguments are NULL
  for (count = 0; count < argc; count ++)
    if (argv[count] == NULL)
      return (status = ERR_NULLPARAMETER);

  // Only work in graphics mode
  if (!graphicsAreEnabled())
    {
      printf("\nThe \"%s\" command only works in graphics mode\n", argv[0]);
      errno = ERR_NOTINITIALIZED;
      return (status = errno);
    }

  processId = multitaskerGetCurrentProcessId();

  // Create a new window, with small, arbitrary size and location
  window = windowManagerNewWindow(processId, "Console Window", 100, 100, 200,
				  200);

  // Put it in the client area of the window
  params.gridX = 0;
  params.gridY = 0;
  params.gridWidth = 1;
  params.gridHeight = 1;
  params.padLeft = 0;
  params.padRight = 0;
  params.padTop = 0;
  params.padBottom = 0;
  params.orientationX = orient_center;
  params.orientationY = orient_middle;
  params.hasBorder = 0;
  params.useDefaultForeground = 1;
  params.useDefaultBackground = 1;

  // Put the console text area in the window
  status = windowAddConsoleTextArea(window, &params);
  if (status < 0)
    {
      if (status == ERR_ALREADY)
	// There's already a console window open somewhere
	windowNewErrorDialog(NULL, "Error", "Cannot open more than one "
			     "console window!");
      
      else
	windowNewErrorDialog(NULL, "Error", "Error opening the console "
			     "window!");

      windowManagerDestroyWindow(window);
      return (status);
    }

  // Lay out and autosize the window, and make it visible
  windowLayout(window);
  windowAutoSize(window);
  windowCenter(window);
  windowSetVisible(window, 1);

  // Register an event handler to catch window close events
  windowRegisterEventHandler(window, &eventHandler);

  // Run the GUI
  windowGuiRun();

  // Destroy the window
  windowManagerDestroyWindow(window);

  // Done
  return (status);
}
