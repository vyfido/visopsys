//
//  Visopsys
//  Copyright (C) 1998-2011 J. Andrew McLaughlin
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

#include <libintl.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/api.h>
#include <sys/lock.h>
#include <sys/vsh.h>
#include <sys/window.h>

#define EXECPROG_VIEW      "/programs/view"
#define EXECPROG_KEYMAP    "/programs/keymap"
#define EXECPROG_CONFEDIT  "/programs/confedit"
#define _(string) gettext(string)
#define gettext_noop(string) (string)

#define FILEMENU_QUIT 0
windowMenuContents fileMenuContents = {
  1,
  {
    { gettext_noop("Quit"), NULL }
  }
};

#define VIEWMENU_REFRESH 0
windowMenuContents viewMenuContents = {
  1,
  {
    { gettext_noop("Refresh"), NULL }
  }
};

typedef struct {
  char name[MAX_PATH_LENGTH];
  int selected;

} dirRecord;

static int processId;
static int privilege;
static objectKey window = NULL;
static objectKey fileMenu = NULL;
static objectKey viewMenu = NULL;
static objectKey locationField = NULL;
static windowFileList *fileList = NULL;
static dirRecord *dirStack = NULL;
static int dirStackCurr = 0;
static lock dirStackLock;
static unsigned cwdModifiedDate = 0;
static unsigned cwdModifiedTime = 0;
static int stop = 0;

__attribute__((format(printf, 1, 2)))
static void error(const char *format, ...)
{
  // Generic error message code
  
  va_list list;
  char *output = NULL;

  output = malloc(MAXSTRINGLENGTH);
  if (output == NULL)
    return;
  
  va_start(list, format);
  vsnprintf(output, MAXSTRINGLENGTH, format, list);
  va_end(list);

  windowNewErrorDialog(window, _("Error"), output);
  free(output);
}


static void changeDir(file *theFile, char *fullName)
{
  file cwdFile;

  while (lockGet(&dirStackLock) < 0)
    multitaskerYield();

  if (!strcmp(theFile->name, ".."))
    {
      if (dirStackCurr > 0)
	{
	  dirStackCurr -= 1;
	  windowComponentSetSelected(fileList->key,
				     dirStack[dirStackCurr].selected);
	}
      else
	{
	  strncpy(dirStack[dirStackCurr].name, fullName, MAX_PATH_LENGTH);
	  dirStack[dirStackCurr].selected = 0;
	}
    }
  else
    {
      dirStackCurr += 1;
      strncpy(dirStack[dirStackCurr].name, fullName, MAX_PATH_LENGTH);
      dirStack[dirStackCurr].selected = 0;
    }

  if (multitaskerSetCurrentDirectory(dirStack[dirStackCurr].name) >= 0)
    {
      // Look up the directory and save the modified date and time, so we
      // can rescan it if it gets modified
      if (fileFind(dirStack[dirStackCurr].name, &cwdFile) >= 0)
	{
	  cwdModifiedDate = cwdFile.modifiedDate;
	  cwdModifiedTime = cwdFile.modifiedTime;
	}

      windowComponentSetData(locationField, dirStack[dirStackCurr].name,
			     strlen(fullName));
    }

  lockRelease(&dirStackLock);
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
	if (loaderClass->class & LOADERFILECLASS_EXEC)
	  strcpy(command, fullName);

	else if ((loaderClass->class & LOADERFILECLASS_IMAGE) &&
		 !fileFind(EXECPROG_VIEW, NULL))
	  sprintf(command, EXECPROG_VIEW " \"%s\"", fullName);

	else if ((loaderClass->class & LOADERFILECLASS_KEYMAP) &&
		 !fileFind(EXECPROG_KEYMAP, NULL))
	  sprintf(command, EXECPROG_KEYMAP " \"%s\"", fullName);

	else if (((loaderClass->class & LOADERFILECLASS_DATA) &&
		  (loaderClass->subClass & LOADERFILESUBCLASS_CONFIG)) &&
		 !fileFind(EXECPROG_CONFEDIT, NULL))
	  sprintf(command, EXECPROG_CONFEDIT " \"%s\"", fullName);

	else if ((loaderClass->class & LOADERFILECLASS_TEXT) &&
		 !fileFind(EXECPROG_VIEW, NULL))
	  sprintf(command, EXECPROG_VIEW " \"%s\"", fullName);

	else
	  return;
	
	// Exec the command, no block
	multitaskerSpawn(&execProgram, "exec program", 1,
			 (void *[]){ command });
	break;
      }

    case dirT:
      changeDir(theFile, fullName);
      break;

    case linkT:
      if (!strcmp(theFile->name, ".."))
	changeDir(theFile, fullName);
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
	    fileList->update(fileList);
	}
    }

  // Check for events to be passed to the file list widget
  else if (key == fileList->key)
    {
      if ((event->type & EVENT_MOUSE_DOWN) || (event->type & EVENT_KEY_DOWN))
	windowComponentGetSelected(fileList->key,
				   &dirStack[dirStackCurr].selected);

      fileList->eventHandler(fileList, event);
    }
}


static void initMenuContents(windowMenuContents *contents)
{
  int count;

  for (count = 0; count < contents->numItems; count ++)
    {
      strncpy(contents->items[count].text, _(contents->items[count].text),
	      WINDOW_MAX_LABEL_LENGTH);
      contents->items[count].text[WINDOW_MAX_LABEL_LENGTH - 1] = '\0';
    }
}


static int constructWindow(const char *directory)
{
  int status = 0;
  componentParameters params;

  // Create a new window, with small, arbitrary size and location
  window = windowNew(processId, _("File Browser"));
  if (window == NULL)
    return (status = ERR_NOTINITIALIZED);

  bzero(&params, sizeof(componentParameters));

  // Create the top menu bar
  objectKey menuBar = windowNewMenuBar(window, &params);
  // The 'file' menu
  initMenuContents(&fileMenuContents);
  fileMenu = windowNewMenu(menuBar, _("File"), &fileMenuContents, &params);
  windowRegisterEventHandler(fileMenu, &eventHandler);
  // The 'view' menu
  initMenuContents(&viewMenuContents);
  viewMenu = windowNewMenu(menuBar, _("View"), &viewMenuContents, &params);
  windowRegisterEventHandler(viewMenu, &eventHandler);

  params.gridWidth = 1;
  params.gridHeight = 1;
  params.padTop = 5;
  params.padLeft = 5;
  params.padRight = 5;
  params.orientationX = orient_center;
  params.orientationY = orient_middle;

  // Create the location text field
  locationField = windowNewTextField(window, 40, &params);
  windowComponentSetData(locationField, (char *) directory, strlen(directory));
  windowRegisterEventHandler(locationField, &eventHandler);
  windowComponentSetEnabled(locationField, 0); // For now

  // Create a window list to hold the icons
  params.gridY += 1;
  params.padBottom = 5;
  fileList = windowNewFileList(window, windowlist_icononly, 4, 5, directory,
			       WINFILEBROWSE_ALL, doFileSelection, &params);
  windowRegisterEventHandler(fileList->key, &eventHandler);
  windowComponentFocus(fileList->key);  

  // Register an event handler to catch window close events
  windowRegisterEventHandler(window, &eventHandler);

  windowSetVisible(window, 1);

  return (status = 0);
}


int main(int argc, char *argv[])
{
  int status = 0;
  char *language = "";
  int guiThreadPid = 0;
  file cwdFile;

#ifdef BUILDLANG
  language=BUILDLANG;
#endif
  setlocale(LC_ALL, language);
  textdomain("filebrowse");

  // Only work in graphics mode
  if (!graphicsAreEnabled())
    {
      fprintf(stderr, _("\nThe \"%s\" command only works in graphics mode\n"),
	     (argc? argv[0] : ""));
      return (status = ERR_NOTINITIALIZED);
    }

  // What is my process id?
  processId = multitaskerGetCurrentProcessId();

  // What is my privilege level?
  privilege = multitaskerGetProcessPrivilege(processId);

  dirStack = malloc(MAX_PATH_LENGTH * sizeof(dirRecord));
  if (dirStack == NULL)
    {
      error("%s", _("Memory allocation error"));
      status = ERR_MEMORY;
      goto out;
    }

  // Get the starting current directory.  If one was specified on the command
  // line, try to use that.

  if (argc > 1)
    {
      status = multitaskerSetCurrentDirectory(argv[argc - 1]);
      if (status < 0)
	{
	  error(_("Can't change to directory \"%s\""), argv[argc - 1]);
	  goto out;
	}
    }

  status = multitaskerGetCurrentDirectory(dirStack[dirStackCurr].name,
					  MAX_PATH_LENGTH);
  if (status < 0)
    {
      error("%s", _("Can't determine current directory"));
      goto out;
    }

  status = constructWindow(dirStack[dirStackCurr].name);
  if (status < 0)
    goto out;

  // Run the GUI as a thread because we want to keep checking for directory
  // updates.
  guiThreadPid = windowGuiThread();

  if (fileFind(dirStack[dirStackCurr].name, &cwdFile) >= 0)
    {
      cwdModifiedDate = cwdFile.modifiedDate;
      cwdModifiedTime = cwdFile.modifiedTime;
    }

  // Loop, looking for changes in the current directory
  while (!stop && multitaskerProcessIsAlive(guiThreadPid))
    {
      while (lockGet(&dirStackLock) < 0)
	multitaskerYield();

      if (fileFind(dirStack[dirStackCurr].name, &cwdFile) >= 0)
	{
	  if ((cwdFile.modifiedDate != cwdModifiedDate) ||
	      (cwdFile.modifiedTime != cwdModifiedTime))
	{
	      fileList->update(fileList);
	      windowComponentSetSelected(fileList->key,
					 dirStack[dirStackCurr].selected);

	      cwdModifiedDate = cwdFile.modifiedDate;
	      cwdModifiedTime = cwdFile.modifiedTime;
	    }

	  lockRelease(&dirStackLock);
	}
      else
	{
	  // Filesystem unmounted or something?  Quit.
	  lockRelease(&dirStackLock);
	  break;
	}

      multitaskerYield();
    }

  // We're back.
  status = 0;

 out:
  windowGuiStop();

  if (fileList)
    fileList->destroy(fileList);

  if (window)
    windowDestroy(window);

  if (dirStack)
    free(dirStack);

  return (status);
}
