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
//  filebrowse.c
//

// This is a graphical program for navigating the file system.

/* This is the text that appears when a user requests help about this program
<help>

 -- filebrowse --

A graphical program for navigating the file system.

Usage:
  filebrowse [start_dir]

The filebrowse program is interactive, and may only be used in graphics
mode.  It displays a window with icons representing files and directories.
Clicking on a directory (folder) icon will change to that directory and
repopulate the window with its contents.  Clicking on any other icon will
cause filebrowse to attempt to 'use' the file in a default way, which will
be a different action depending on the file type.  For example, if you
click on an image or document, filebrowse will attempt to display it using
the 'view' command.  In the case of clicking on an executable program,
filebrowse will attempt to execute it -- etc.

</help>
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/vsh.h>
#include <sys/window.h>
#include <sys/lock.h>
#include <sys/api.h>

static int processId;
static int privilege;
static objectKey window = NULL;
static objectKey fileMenu = NULL;
static objectKey viewMenu = NULL;
static windowFileList *fileList = NULL;
static char *cwd = NULL;
static unsigned cwdModifiedDate = 0;
static unsigned cwdModifiedTime = 0;
static int stop = 0;

#define FILEMENU_QUIT 0
windowMenuContents fileMenuContents = {
  1,
  {
    { "Quit", NULL }
  }
};

#define VIEWMENU_REFRESH 0
windowMenuContents viewMenuContents = {
  1,
  {
    { "Refresh", NULL }
  }
};


static void error(const char *, ...) __attribute__((format(printf, 1, 2)));
static void error(const char *format, ...)
{
  // Generic error message code
  
  va_list list;
  char output[MAXSTRINGLENGTH];
  
  va_start(list, format);
  vsnprintf(output, MAXSTRINGLENGTH, format, list);
  va_end(list);

  windowNewErrorDialog(window, "Error", output);
}


static void changeDir(char *fullName)
{
  file cwdFile;

  if (multitaskerSetCurrentDirectory(fullName) >= 0)
    {
      // Look up the directory and save the modified date and time, so we
      // can rescan it if it gets modified
      if (fileFind(fullName, &cwdFile) >= 0)
	{
	  multitaskerGetCurrentDirectory(cwd, MAX_PATH_LENGTH);
	  cwdModifiedDate = cwdFile.modifiedDate;
	  cwdModifiedTime = cwdFile.modifiedTime;
	}
    }
}


static void execProgram(int argc, char *argv[])
{
  windowSwitchPointer(window, "busy");

  // Exec the command, no block
  if (argc == 2)
    loaderLoadAndExec(argv[1], privilege, 0);

  windowSwitchPointer(window, "default");
  multitaskerTerminate(0);
}


static void doFileSelection(file *theFile, char *fullName,
			    loaderFileClass *loaderClass)
{
  char command[MAX_PATH_NAME_LENGTH];

  switch (theFile->type)
    {
    case fileT:
      {
	if (loaderClass->flags & LOADERFILECLASS_EXEC)
	  strcpy(command, fullName);
	else if (loaderClass->flags & LOADERFILECLASS_IMAGE)
	  sprintf(command, "/programs/view %s", fullName);
	else if (loaderClass->flags & LOADERFILECLASS_CONFIG)
	  sprintf(command, "/programs/confedit %s", fullName);
	else if (loaderClass->flags & LOADERFILECLASS_TEXT)
	  sprintf(command, "/programs/view %s", fullName);
	else
	  return;
	
	// Exec the command, no block
	multitaskerSpawn(&execProgram, "exec program", 1,
			 (void *[]){ command });
	break;
      }

    case dirT:
      changeDir(fullName);
      break;

    case linkT:
      if (!strcmp(theFile->name, ".."))
	changeDir(fullName);
      break;

    default:
      break;
    }
}


static void eventHandler(objectKey key, windowEvent *event)
{
  objectKey selectedItem = 0;

  // Check for the window being closed by a GUI event.
  if ((key == window) && (event->type == EVENT_WINDOW_CLOSE))
    stop = 1;

  else if ((key == fileMenu) && (event->type & EVENT_SELECTION))
    {
      windowComponentGetData(fileMenu, &selectedItem, 1);
      if (selectedItem)
	{
	  // Check for the window being closed.
	  if (selectedItem == fileMenuContents.items[FILEMENU_QUIT].key)
	    stop = 1;
	}
    }

  else if ((key == viewMenu) && (event->type & EVENT_SELECTION))
    {
      windowComponentGetData(viewMenu, &selectedItem, 1);
      if (selectedItem)
	{
	  // Check for manual refresh requests
	  if (selectedItem == viewMenuContents.items[VIEWMENU_REFRESH].key)
	    fileList->update(fileList, cwd);
	}
    }

  // Check for events to be passed to the file list widget
  else if (key == fileList->key)
    fileList->eventHandler(fileList, event);
}


static int constructWindow(const char *directory)
{
  int status = 0;
  componentParameters params;

  // Create a new window, with small, arbitrary size and location
  window = windowNew(processId, "File Browser");
  if (window == NULL)
    return (status = ERR_NOTINITIALIZED);

  bzero(&params, sizeof(componentParameters));

  // Create the top menu bar
  objectKey menuBar = windowNewMenuBar(window, &params);
  // The 'file' menu
  fileMenu = windowNewMenu(menuBar, "File", &fileMenuContents, &params);
  windowRegisterEventHandler(fileMenu, &eventHandler);
  // The 'view' menu
  viewMenu = windowNewMenu(menuBar, "View", &viewMenuContents, &params);
  windowRegisterEventHandler(viewMenu, &eventHandler);

  params.gridWidth = 1;
  params.gridHeight = 1;
  params.padTop = 5;
  params.padBottom = 5;
  params.padLeft = 5;
  params.padRight = 5;
  params.orientationX = orient_center;
  params.orientationY = orient_middle;

  // Create a window list to hold the icons
  fileList = windowNewFileList(window, windowlist_icononly, 4, 5, directory,
			       WINFILEBROWSE_ALL, doFileSelection, &params);
  windowRegisterEventHandler(fileList->key, &eventHandler);

  // Register an event handler to catch window close events
  windowRegisterEventHandler(window, &eventHandler);

  windowSetVisible(window, 1);

  return (status = 0);
}


int main(int argc, char *argv[])
{
  int status = 0;
  int guiThreadPid = 0;
  file cwdFile;

  // Only work in graphics mode
  if (!graphicsAreEnabled())
    {
      printf("\nThe \"%s\" command only works in graphics mode\n",
	     (argc? argv[0] : ""));
      errno = ERR_NOTINITIALIZED;
      return (status = errno);
    }

  // What is my process id?
  processId = multitaskerGetCurrentProcessId();

  // What is my privilege level?
  privilege = multitaskerGetProcessPrivilege(processId);

  cwd = malloc(MAX_PATH_LENGTH);
  if (cwd == NULL)
    {
      error("Memory allocation error");
      errno =  ERR_MEMORY;
      return (status = errno);
    }

  // Get the starting current directory.  If one was specified on the command
  // line, try to use that.

  if (argc > 1)
    {
      status = multitaskerSetCurrentDirectory(argv[argc - 1]);
      if (status < 0)
	{
	  error("Can't change to directory \"%s\"", argv[argc - 1]);
	  free(cwd);
	  return (errno = status);
	}
    }

  status = multitaskerGetCurrentDirectory(cwd, MAX_PATH_LENGTH);
  if (status < 0)
    {
      error("Can't determine current directory");
      free(cwd);
      return (errno = status);
    }

  status = constructWindow(cwd);
  if (status < 0)
    {
      free(cwd);
      return (errno = status);
    }

  // Run the GUI as a thread because we want to keep checking for directory
  // updates.
  guiThreadPid = windowGuiThread();

  // Loop, looking for changes in the current directory
  while (!stop && multitaskerProcessIsAlive(guiThreadPid))
    {
      if (fileFind(cwd, &cwdFile) >= 0)
	{
	  if ((cwdFile.modifiedDate != cwdModifiedDate) ||
	      (cwdFile.modifiedTime != cwdModifiedTime))
	    {
	      if (fileList->update(fileList, cwd) < 0)
		break;

	      cwdModifiedDate = cwdFile.modifiedDate;
	      cwdModifiedTime = cwdFile.modifiedTime;
	    }
	}
      else
	// Filesystem unmounted or something?  Quit.
	break;
      
      multitaskerYield();
    }

  // We're back.
  windowGuiStop();
  fileList->destroy(fileList);
  windowDestroy(window);
  free(cwd);

  return (status = 0);
}
