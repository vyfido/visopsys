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
//  window.c
//

// This command will create a new window

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/window.h>
#include <sys/api.h>


static int processId = 0;
objectKey window = NULL;


static void eventHandler(objectKey key, windowEvent *event)
{
  // This is just to handle a window shutdown event.

  if ((key == window) && (event->type == EVENT_WINDOW_CLOSE))
    // The window is being closed by a GUI event.  Just kill our shell
    // process -- the main process will stop blocking and do the rest of the
    // shutdown.
    multitaskerKillProcess(processId, 0 /* no force */);
}


int main(int argc, char *argv[])
{
  int status = 0;
  int myPrivilege = 0;
  componentParameters params;
  objectKey textArea = NULL;
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
  myPrivilege = multitaskerGetProcessPrivilege(processId);

  // Load a shell process
  processId = loaderLoadProgram("/programs/vsh", myPrivilege, 0, NULL);
  if (processId < 0)
    {
      printf("Unable to load shell\n");
      errno = processId;
      return (status = processId);
    }

  // Create a new window
  window = windowNew(processId, "Command window");

  // Put a text area in the window
  bzero(&params, sizeof(componentParameters));
  params.gridWidth = 1;
  params.gridHeight = 1;
  params.orientationX = orient_center;
  params.orientationY = orient_middle;
  params.stickyFocus = 1;
  params.useDefaultForeground = 1;
  params.useDefaultBackground = 1;
  textArea = windowNewTextArea(window, 80, 40, 200, &params);

  // Use the text area for all our input and output
  windowSetTextOutput(textArea);

  // Go live.
  windowSetVisible(window, 1);

  // Register an event handler to catch window close events
  windowRegisterEventHandler(window, &eventHandler);

  // Run the GUI as a thread
  windowGuiThread();

  // Execute the shell
  status = loaderExecProgram(processId, 1 /* block */);

  // If we get to here, the shell has exited.

  // Stop our GUI thread
  windowGuiStop();

  // Destroy the window
  windowDestroy(window);

  // Done
  return (status);
}
