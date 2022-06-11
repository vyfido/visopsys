//
//  Visopsys
//  Copyright (C) 1998-2013 J. Andrew McLaughlin
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
//  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
//
//  cmdwin.c
//

// This command will create a new command window

/* This is the text that appears when a user requests help about this program
<help>

 -- cmdwin --

Open a new command window.

Usage:
  cmdwin

(Only available in graphics mode)

This command will open a new text window running a new instance of the
'vsh' command shell.

</help>
*/

#include <errno.h>
#include <libintl.h>
#include <locale.h>
#include <stdio.h>
#include <string.h>
#include <sys/api.h>
#include <sys/font.h>
#include <sys/window.h>

#define _(string) gettext(string)

static int shellProcessId = 0;
objectKey window = NULL;


static void eventHandler(objectKey key, windowEvent *event)
{
  // This is just to handle a window shutdown event.

  if ((key == window) && (event->type == EVENT_WINDOW_CLOSE))
    // The window is being closed by a GUI event.  Just kill our shell
    // process -- the main process will stop blocking and do the rest of the
    // shutdown.
    multitaskerKillProcess(shellProcessId, 0 /* no force */);
}


int main(int argc, char *argv[])
{
  int status = 0;
  char *language = "";
  int myProcessId = 0;
  int myPrivilege = 0;
  componentParameters params;
  int rows = 25;
  objectKey textArea = NULL;

#ifdef BUILDLANG
  language=BUILDLANG;
#endif
  setlocale(LC_ALL, language);
  textdomain("cmdwin");

  // Only work in graphics mode
  if (!graphicsAreEnabled())
    {
      printf(_("\nThe \"%s\" command only works in graphics mode\n"),
	     (argc? argv[0] : ""));
      errno = ERR_NOTINITIALIZED;
      return (status = errno);
    }

  myProcessId = multitaskerGetCurrentProcessId();
  myPrivilege = multitaskerGetProcessPrivilege(myProcessId);

  // Load a shell process
  shellProcessId = loaderLoadProgram("/programs/vsh", myPrivilege);
  if (shellProcessId < 0)
    {
      printf("%s", _("Unable to load shell\n"));
      errno = shellProcessId;
      return (status = shellProcessId);
    }

  // Create a new window
  window = windowNew(myProcessId, _("Command Window"));

  // Put a text area in the window
  bzero(&params, sizeof(componentParameters));
  params.gridWidth = 1;
  params.gridHeight = 1;
  params.padLeft = 1;
  params.padRight = 1;
  params.padTop = 1;
  params.padBottom = 1;
  params.orientationX = orient_center;
  params.orientationY = orient_middle;
  params.flags |= WINDOW_COMPFLAG_STICKYFOCUS;
  status = fileFind(FONT_SYSDIR "/xterm-normal-10.vbf", NULL);
  if (status >= 0)
    status = fontLoad("xterm-normal-10.vbf", "xterm-normal-10",
		      &(params.font), 1);
  if (status < 0)
    {
      params.font = NULL;
      // The system font can comfortably show more rows
      rows = 40;
    }
  textArea = windowNewTextArea(window, 80, rows, 200, &params);
  windowComponentFocus(textArea);

  // Use the text area for all our input and output
  windowSetTextOutput(textArea);

  // Go live.
  windowSetVisible(window, 1);

  // Register an event handler to catch window close events
  windowRegisterEventHandler(window, &eventHandler);

  // Run the GUI as a thread
  windowGuiThread();

  // Execute the shell
  status = loaderExecProgram(shellProcessId, 1 /* block */);

  // If we get to here, the shell has exited.

  // Stop our GUI thread
  windowGuiStop();

  // Destroy the window
  windowDestroy(window);

  // Done
  return (status);
}
