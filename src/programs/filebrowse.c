//
//  Visopsys
//  Copyright (C) 1998-2021 J. Andrew McLaughlin
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
#include <sys/env.h>
#include <sys/lock.h>
#include <sys/paths.h>
#include <sys/window.h>

#define _(string) gettext(string)
#define gettext_noop(string) (string)

#define WINDOW_TITLE		_("File Browser")
#define FILE_MENU			_("File")
#define QUIT				gettext_noop("Quit")
#define VIEW_MENU			_("View")
#define REFRESH				gettext_noop("Refresh")
#define EXECPROG_ARCHMAN	PATH_PROGRAMS "/archman"
#define EXECPROG_CONFEDIT	PATH_PROGRAMS "/confedit"
#define EXECPROG_FONTUTIL	PATH_PROGRAMS "/fontutil"
#define EXECPROG_KEYMAP		PATH_PROGRAMS "/keymap"
#define EXECPROG_SOFTWARE	PATH_PROGRAMS "/software"
#define EXECPROG_VIEW		PATH_PROGRAMS "/view"

#define FILEMENU_QUIT 0
windowMenuContents fileMenuContents = {
	1,
	{
		{ QUIT, NULL }
	}
};

#define VIEWMENU_REFRESH 0
windowMenuContents viewMenuContents = {
	1,
	{
		{ REFRESH, NULL }
	}
};

typedef struct {
	char name[MAX_PATH_LENGTH + 1];
	time_t dirModified;
	time_t fileModified;
	unsigned filesSize;
	int selected;

} dirRecord;

static int processId = 0;
static int privilege = 0;
static objectKey window = NULL;
static objectKey fileMenu = NULL;
static objectKey viewMenu = NULL;
static objectKey locationField = NULL;
static windowFileList *fileList = NULL;
static dirRecord *dirStack = NULL;
static int dirStackCurr = 0;
static spinLock dirStackLock;
static int stop = 0;


__attribute__((format(printf, 1, 2)))
static void error(const char *format, ...)
{
	// Generic error message code

	va_list list;
	char *output = NULL;

	output = malloc(MAXSTRINGLENGTH + 1);
	if (!output)
		return;

	va_start(list, format);
	vsnprintf(output, MAXSTRINGLENGTH, format, list);
	va_end(list);

	windowNewErrorDialog(window, _("Error"), output);
	free(output);
}


static void scanDir(dirRecord *dirRec, char *dirName)
{
	// Scan the contents of the directory, so that we'll know if it has
	// changed.  The caller should probably acquire the dirStackLock before
	// invoking this function.

	file dirFile;

	dirRec->dirModified = 0;
	dirRec->fileModified = 0;
	dirRec->filesSize = 0;

	memset(&dirFile, 0, sizeof(file));

	if (fileFind(dirName, &dirFile) >= 0)
	{
		// Note the modification time of the directory itself.
		dirRec->dirModified = mktime(&dirFile.modified);

		// Size up the files in the directory and note the most recent
		// modification time

		if (fileFirst(dirName, &dirFile) >= 0)
		{
			do {
				// Ignore 'dot' dirs
				if (!strcmp(dirFile.name, ".") || !strcmp(dirFile.name, ".."))
					continue;

				dirRec->filesSize += dirFile.size;

				if (mktime(&dirFile.modified) > dirRec->fileModified)
					dirRec->fileModified = mktime(&dirFile.modified);

			} while (fileNext(dirName, &dirFile) >= 0);
		}
	}
}


static void changeDir(file *dir, char *dirName)
{
	while (lockGet(&dirStackLock) < 0)
		multitaskerYield();

	if (!strcmp(dir->name, ".."))
	{
		if (dirStackCurr > 0)
		{
			dirStackCurr -= 1;
			windowComponentSetSelected(fileList->key,
				dirStack[dirStackCurr].selected);
		}
		else
		{
			strncpy(dirStack[dirStackCurr].name, dirName, MAX_PATH_LENGTH);
			dirStack[dirStackCurr].selected = 0;
		}
	}
	else
	{
		dirStackCurr += 1;
		strncpy(dirStack[dirStackCurr].name, dirName, MAX_PATH_LENGTH);
		dirStack[dirStackCurr].selected = 0;
	}

	if (multitaskerSetCurrentDirectory(dirStack[dirStackCurr].name) >= 0)
	{
		// Record the new modification times and file sizes
		scanDir(&dirStack[dirStackCurr], dirStack[dirStackCurr].name);

		windowComponentSetData(locationField, dirStack[dirStackCurr].name,
			strlen(dirName), 1 /* redraw */);
	}

	lockRelease(&dirStackLock);
}


static void execProgram(int argc, char *argv[])
{
	// Exec the command, no block
	if (argc == 2)
		loaderLoadAndExec(argv[1], privilege, 0);

	multitaskerTerminate(0);
}


static void doFileSelection(windowFileList *list __attribute__((unused)),
	file *theFile, char *fullName, loaderFileClass *loaderClass)
{
	char command[MAX_PATH_NAME_LENGTH + 1];
	int pid = 0;

	switch (theFile->type)
	{
		case fileT:
		{
			if (loaderClass->type & LOADERFILECLASS_EXEC)
			{
				strcpy(command, fullName);
			}
			else if ((loaderClass->type & LOADERFILECLASS_ARCHIVE) &&
				!fileFind(EXECPROG_ARCHMAN, NULL))
			{
				sprintf(command, EXECPROG_ARCHMAN " \"%s\"", fullName);
			}
			else if (((loaderClass->type & LOADERFILECLASS_DATA) &&
				(loaderClass->subType & LOADERFILESUBCLASS_CONFIG)) &&
					!fileFind(EXECPROG_CONFEDIT, NULL))
			{
				sprintf(command, EXECPROG_CONFEDIT " \"%s\"", fullName);
			}
			else if ((loaderClass->type & LOADERFILECLASS_FONT) &&
				!fileFind(EXECPROG_FONTUTIL, NULL))
			{
				sprintf(command, EXECPROG_FONTUTIL " \"%s\"", fullName);
			}
			else if (((loaderClass->type & LOADERFILECLASS_INSTALL) &&
				(loaderClass->subType & LOADERFILESUBCLASS_VSP)) &&
					!fileFind(EXECPROG_SOFTWARE, NULL))
			{
				sprintf(command, EXECPROG_SOFTWARE " -i \"%s\"", fullName);
			}
			else if ((loaderClass->type & LOADERFILECLASS_KEYMAP) &&
				!fileFind(EXECPROG_KEYMAP, NULL))
			{
				sprintf(command, EXECPROG_KEYMAP " \"%s\"", fullName);
			}
			else if ((loaderClass->type & LOADERFILECLASS_TEXT) &&
				!fileFind(EXECPROG_VIEW, NULL))
			{
				sprintf(command, EXECPROG_VIEW " \"%s\"", fullName);
			}
			else if ((loaderClass->type & LOADERFILECLASS_IMAGE) &&
				!fileFind(EXECPROG_VIEW, NULL))
			{
				sprintf(command, EXECPROG_VIEW " \"%s\"", fullName);
			}
			else
			{
				return;
			}

			windowSwitchPointer(window, MOUSE_POINTER_BUSY);

			// Run a thread to execute the command
			pid = multitaskerSpawn(&execProgram, "exec program", 1,
				(void *[]){ command }, 1 /* run */);
			if (pid < 0)
				error(_("Couldn't execute command \"%s\""), command);
			else
				while (multitaskerProcessIsAlive(pid));

			windowSwitchPointer(window, MOUSE_POINTER_DEFAULT);

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


static void initMenuContents(void)
{
	strncpy(fileMenuContents.items[FILEMENU_QUIT].text, gettext(QUIT),
		WINDOW_MAX_LABEL_LENGTH);
	strncpy(viewMenuContents.items[VIEWMENU_REFRESH].text, gettext(REFRESH),
		WINDOW_MAX_LABEL_LENGTH);
}


static void refreshWindow(void)
{
	// We got a 'window refresh' event (probably because of a language
	// switch), so we need to update things

	const char *charSet = NULL;

	// Re-get the language setting
	setlocale(LC_ALL, getenv(ENV_LANG));
	textdomain("filebrowse");

	// Re-get the character set
	charSet = getenv(ENV_CHARSET);

	if (charSet)
		windowSetCharSet(window, charSet);

	// Refresh all the menu contents
	initMenuContents();

	// Refresh the 'file' menu
	windowMenuUpdate(fileMenu, FILE_MENU, charSet, &fileMenuContents,
		NULL /* params */);

	// Refresh the 'view' menu
	windowMenuUpdate(viewMenu, VIEW_MENU, charSet, &viewMenuContents,
		NULL /* params */);

	// Refresh the window title
	windowSetTitle(window, WINDOW_TITLE);

	// Re-layout the window
	windowLayout(window);
}


static void eventHandler(objectKey key, windowEvent *event)
{
	// Check for window events.
	if (key == window)
	{
		// Check for window refresh
		if (event->type == WINDOW_EVENT_WINDOW_REFRESH)
			refreshWindow();

		// Check for the window being closed
		else if (event->type == WINDOW_EVENT_WINDOW_CLOSE)
			stop = 1;
	}

	// Check for 'file' menu events
	else if (key == fileMenuContents.items[FILEMENU_QUIT].key)
	{
		if (event->type & WINDOW_EVENT_SELECTION)
			stop = 1;
	}

	// Check for 'view' menu events
	else if (key == viewMenuContents.items[VIEWMENU_REFRESH].key)
	{
		if (event->type & WINDOW_EVENT_SELECTION)
			// Manual refresh request
			fileList->update(fileList);
	}

	// Check for events to be passed to the file list widget
	else if (key == fileList->key)
	{
		if ((event->type & WINDOW_EVENT_MOUSE_DOWN) ||
			(event->type & WINDOW_EVENT_KEY_DOWN))
		{
			windowComponentGetSelected(fileList->key,
				&dirStack[dirStackCurr].selected);
		}

		fileList->eventHandler(fileList, event);
	}
}


static void handleMenuEvents(windowMenuContents *contents)
{
	int count;

	for (count = 0; count < contents->numItems; count ++)
		windowRegisterEventHandler(contents->items[count].key, &eventHandler);
}


static int constructWindow(const char *directory)
{
	int status = 0;
	objectKey menuBar = NULL;
	componentParameters params;

	// Create a new window, with small, arbitrary size and location
	window = windowNew(processId, WINDOW_TITLE);
	if (!window)
		return (status = ERR_NOTINITIALIZED);

	memset(&params, 0, sizeof(componentParameters));

	// Create the top menu bar
	menuBar = windowNewMenuBar(window, &params);

	initMenuContents();

	// Create the top 'file' menu
	fileMenu = windowNewMenu(window, menuBar, FILE_MENU, &fileMenuContents,
		&params);
	handleMenuEvents(&fileMenuContents);

	// Create the top 'view' menu
	viewMenu = windowNewMenu(window, menuBar, VIEW_MENU, &viewMenuContents,
		&params);
	handleMenuEvents(&viewMenuContents);

	params.gridWidth = 1;
	params.gridHeight = 1;
	params.padTop = 5;
	params.padLeft = 5;
	params.padRight = 5;
	params.orientationX = orient_center;
	params.orientationY = orient_middle;

	// Create the location text field
	locationField = windowNewTextField(window, 40, &params);
	windowComponentSetData(locationField, (char *) directory,
		strlen(directory), 1 /* redraw */);
	windowRegisterEventHandler(locationField, &eventHandler);
	windowComponentSetEnabled(locationField, 0); // For now

	// Create the file list widget
	params.gridY += 1;
	params.padBottom = 5;
	fileList = windowNewFileList(window, windowlist_icononly, 5, 8, directory,
		WINDOW_FILEBROWSE_ALL, doFileSelection, &params);
	if (!fileList)
		return (status = ERR_NOTINITIALIZED);

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
	int guiThreadPid = 0;
	dirRecord dirRec;
	int update = 0;

	setlocale(LC_ALL, getenv(ENV_LANG));
	textdomain("filebrowse");

	// Only work in graphics mode
	if (!graphicsAreEnabled())
	{
		fprintf(stderr, _("\nThe \"%s\" command only works in graphics "
			"mode\n"), (argc? argv[0] : ""));
		return (status = ERR_NOTINITIALIZED);
	}

	// What is my process id?
	processId = multitaskerGetCurrentProcessId();

	// What is my privilege level?
	privilege = multitaskerGetProcessPrivilege(processId);

	dirStack = calloc((MAX_PATH_LENGTH + 1), sizeof(dirRecord));
	if (!dirStack)
	{
		error("%s", _("Memory allocation error"));
		status = ERR_MEMORY;
		goto out;
	}

	// Set the starting directory.  If one was specified on the command line,
	// try to use that.  Otherwise, default to '/'

	strcpy(dirStack[dirStackCurr].name, "/");
	if (argc > 1)
		fileFixupPath(argv[argc - 1], dirStack[dirStackCurr].name);

	status = multitaskerSetCurrentDirectory(dirStack[dirStackCurr].name);
	if (status < 0)
	{
		error(_("Can't change to directory \"%s\""),
			dirStack[dirStackCurr].name);

		status = multitaskerGetCurrentDirectory(dirStack[dirStackCurr].name,
			MAX_PATH_LENGTH);
		if (status < 0)
		{
			error("%s", _("Can't determine current directory"));
			goto out;
		}
	}

	// Record the initial modification times and file sizes
	scanDir(&dirStack[dirStackCurr], dirStack[dirStackCurr].name);

	status = constructWindow(dirStack[dirStackCurr].name);
	if (status < 0)
		goto out;

	// Run the GUI as a thread because we want to keep checking for directory
	// updates.
	guiThreadPid = windowGuiThread();

	// Loop, looking for changes in the current directory
	while (!stop && multitaskerProcessIsAlive(guiThreadPid))
	{
		while (lockGet(&dirStackLock) < 0)
			multitaskerYield();

		if (fileFind(dirStack[dirStackCurr].name, NULL) >= 0)
		{
			// Get the latest modification times and file sizes
			scanDir(&dirRec, dirStack[dirStackCurr].name);

			update = 0;

			if ((dirRec.dirModified != dirStack[dirStackCurr].dirModified) ||
				(dirRec.fileModified != dirStack[dirStackCurr].fileModified) ||
				(dirRec.filesSize != dirStack[dirStackCurr].filesSize))
			{
				dirStack[dirStackCurr].dirModified = dirRec.dirModified;
				dirStack[dirStackCurr].fileModified = dirRec.fileModified;
				dirStack[dirStackCurr].filesSize = dirRec.filesSize;
				update = 1;
			}

			if (update)
			{
				fileList->update(fileList);
				windowComponentSetSelected(fileList->key,
					dirStack[dirStackCurr].selected);
			}

			lockRelease(&dirStackLock);

			if (update)
				// Don't update more than once per second
				multitaskerWait(MS_PER_SEC);
			else
				multitaskerYield();
		}
		else
		{
			// Filesystem unmounted, directory deleted, or something?  Quit.
			lockRelease(&dirStackLock);
			break;
		}
	}

	// We're back.
	status = 0;

out:
	windowGuiStop();

	if (fileList && fileList->destroy)
		fileList->destroy(fileList);

	if (window)
		windowDestroy(window);

	if (dirStack)
		free(dirStack);

	return (status);
}

