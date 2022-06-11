//
//  Visopsys
//  Copyright (C) 1998-2003 J. Andrew McLaughlin
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
#include <sys/api.h>


int main(int argc, char *argv[])
{
  int status = 0;
  int processId = 0;
  int myPrivilege = 0;
  objectKey window = NULL;
  componentParameters params;
  objectKey textArea = NULL;

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

  // Create a new window, with small, arbitrary size and location
  window = windowManagerNewWindow(processId, "Command window", 100, 100, 100,
				  100);

  // Put a text area in the window
  textArea = windowNewTextAreaComponent(window, 60, 40,
					NULL /* (default font) */);

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
  windowAddClientComponent(window, textArea, &params);

  windowLayout(window);

  // Autosize the window to fit our text area
  windowAutoSize(window);

  // Use the text area for all our input and output
  windowManagerSetTextOutput(textArea);

  // Go live.
  status = windowSetVisible(window, 1);

  // Execute the shell
  status = loaderExecProgram(processId, 1 /* block */);

  // Destroy the window
  windowManagerDestroyWindow(window);

  // Done
  return (status);
}
