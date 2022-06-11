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
//  console.c
//

// This command will display/close the console window

/* This is the text that appears when a user requests help about this program
<help>

 -- console --

Launch a console window.

Usage:
  console

(Only available in graphics mode)

This command will launch a window in which console messages are displayed.
This is useful for viewing logging or error messages that do not appear
in other windows.

</help>
*/

#include <stdio.h>
#include <string.h>
#include <errno.h>
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

  // Only work in graphics mode
  if (!graphicsAreEnabled())
    {
      printf("\nThe \"%s\" command only works in graphics mode\n",
	     (argc? argv[0] : ""));
      errno = ERR_NOTINITIALIZED;
      return (status = errno);
    }

  processId = multitaskerGetCurrentProcessId();

  // Create a new window, with small, arbitrary size and location
  window = windowNew(processId, "Console Window");

  // Put the console text area in the window
  status = windowAddConsoleTextArea(window);
  if (status < 0)
    {
      if (status == ERR_ALREADY)
	// There's already a console window open somewhere
	windowNewErrorDialog(NULL, "Error", "Cannot open more than one "
			     "console window!");
      
      else
	windowNewErrorDialog(NULL, "Error", "Error opening the console "
			     "window!");

      windowDestroy(window);
      return (status);
    }

  // Not resizable
  windowSetResizable(window, 0);

  // Register an event handler to catch window close events
  windowRegisterEventHandler(window, &eventHandler);

  // Make it visible
  windowSetVisible(window, 1);

  // Run the GUI
  windowGuiRun();

  // Destroy the window
  windowDestroy(window);

  // Done
  return (status);
}
