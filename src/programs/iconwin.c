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
//  iconwin.c
//

// This is a generic program for creating customized graphical windows
// with just a text configuration file.

/* This is the text that appears when a user requests help about this program
<help>

 -- iconwin --

A program for displaying custom icon windows.

Usage:
  iconwin <config_file>

The iconwin program is interactive, and may only be used in graphics mode.
It creates a window with icons, as specified in the named configuration file.
The 'Administration' icon, for example, on the default Visopsys desktop
uses the iconwin program to display the relevant administration tasks with
custom icons for each.

</help>
*/

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/window.h>
#include <sys/api.h>
#include <sys/vsh.h>
#include <sys/cdefs.h>

#define DEFAULT_WINDOWTITLE  "Icon Window"
#define DEFAULT_ROWS         4
#define DEFAULT_COLUMNS      5

typedef struct {
  char imageFile[MAX_PATH_NAME_LENGTH];
  char command[MAX_PATH_NAME_LENGTH];

} iconInfo;

static int processId;
static int privilege;
static char windowTitle[WINDOW_MAX_TITLE_LENGTH];
static int rows = 0;
static int columns = 0;
static int numIcons = 0;
static listItemParameters *iconParams = NULL;
static iconInfo *icons = NULL;
static objectKey window = NULL;
static objectKey iconList = NULL;


static void error(const char *format, ...)
{
  // Generic error message code
  
  va_list list;
  char output[MAXSTRINGLENGTH];
  
  va_start(list, format);
  _expandFormatString(output, format, list);
  va_end(list);

  windowNewErrorDialog(window, "Error", output);
}


static int readConfig(const char *fileName)
{
  int status = 0;
  variableList config;
  char tmp[MAXSTRINGLENGTH];
  char *icon = NULL;
  char variable[128];
  file tmpFile;
  int count;

  status = configurationReader(fileName, &config);
  if (status < 0)
    {
      error("Can't locate configuration file %s", fileName);
      return (status);
    }

  /*
    window.title=xxx
    list.rows=xxx
    list.columns=xxx
    icons=xxx,xxx
    icon.xxx.text=xxx
    icon.xxx.image=xxx
    icon.xxx.command=xxx
  */

  // Is the window title specified?
  status = variableListGet(&config, "window.title", windowTitle,
			   WINDOW_MAX_TITLE_LENGTH);
  if (status < 0)
    strcpy(windowTitle, DEFAULT_WINDOWTITLE);

  // Are the number of rows specified?
  rows = DEFAULT_ROWS;
  status = variableListGet(&config, "list.rows", tmp, MAXSTRINGLENGTH);
  if ((status >= 0) && (atoi(tmp) > 0))
    rows = atoi(tmp);

  // Are the number of columns specified?
  columns = DEFAULT_COLUMNS;
  status = variableListGet(&config, "list.columns", tmp, MAXSTRINGLENGTH);
  if ((status >= 0) && (atoi(tmp) > 0))
    columns = atoi(tmp);

  memset(tmp, '\0', MAXSTRINGLENGTH);
  status = variableListGet(&config, "icons", tmp, MAXSTRINGLENGTH);
  if (status >= 0)
    {
      if (tmp[0])
	{
	  // Figure out how many icons we have
	  numIcons = 1;
	  icon = tmp;
	  while ((icon = strchr(icon, ',')) && icon[1])
	    {
	      icon[0] = '\0';
	      numIcons += 1;
	      icon += 1;
	    }

	  // Allocate memory for our list of listItemParameters structures
	  // and the commands for each icon
	  iconParams = malloc(numIcons * sizeof(listItemParameters));
	  icons = malloc(numIcons * sizeof(iconInfo));
	  if ((iconParams == NULL) || (icons == NULL))
	    {
	      error("Memory allocation error");
	      variableListDestroy(&config);
	      return (status = ERR_MEMORY);
	    }

	  // Get the text, image, and command for each icon
	  icon = tmp;
	  for (count = 0; count < numIcons; count ++)
	    {
	      sprintf(variable, "icon.%s.text", icon);
	      status = variableListGet(&config, variable,
				       iconParams[count].text,
				       WINDOW_MAX_LABEL_LENGTH);
	      if (status < 0)
		strcpy(iconParams[count].text, " ");

	      sprintf(variable, "icon.%s.image", icon);
	      status = variableListGet(&config, variable,
				       icons[count].imageFile,
				       MAX_PATH_NAME_LENGTH);
	      if (status < 0)
		// Try the default icon
		strcpy(icons[count].imageFile, "/system/icons/icon.bmp");

	      sprintf(variable, "icon.%s.command", icon);
	      status = variableListGet(&config, variable,
				       icons[count].command,
				       MAX_PATH_NAME_LENGTH);
	      if (status < 0)
		{
		  numIcons -= 1;
		  count -= 1;
		  goto skip;
		}

	      // See whether the command exists
	      if (fileFind(icons[count].command, &tmpFile) < 0)
		{
		  numIcons -= 1;
		  count -= 1;
		  goto skip;
		}

	      // Load the icon image
	      status = imageLoad(icons[count].imageFile, 0, 0,
				 &(iconParams[count].iconImage));
	      if (status < 0)
		{
		  // Try the default icon
		  strcpy(icons[count].imageFile, "/system/icons/icon.bmp");

		  status = imageLoad(icons[count].imageFile, 0, 0,
				     &(iconParams[count].iconImage));
		  if (status < 0)
		    {
		      numIcons -= 1;
		      count -= 1;
		      goto skip;
		    }
		}
	      
	    skip:
	      icon += (strlen(icon) + 1);
	    }
	}
    }

  variableListDestroy(&config);
  return (status = 0);
}


static void execProgram(int argc, char *argv[])
{
  mouseSwitchPointer("busy");

  // Exec the command, no block
  if (argc == 2)
    loaderLoadAndExec(argv[1], privilege, 0);

  mouseSwitchPointer("default");
  multitaskerTerminate(0);
}


static void eventHandler(objectKey key, windowEvent *event)
{
  int clickedIcon = -1;

  // Check for the window being closed by a GUI event.
  if ((key == window) && (event->type == EVENT_WINDOW_CLOSE))
    windowGuiStop();

  // Check for events in our icon list.  We consider the icon 'clicked'
  // if it is a mouse click selection, or an ENTER key selection
  else if ((key == iconList) && (event->type & EVENT_SELECTION) &&
      ((event->type & EVENT_MOUSE_LEFTUP) ||
      ((event->type & EVENT_KEY_DOWN) && (event->key == 10))))
    {
      // Get the selected item
      windowComponentGetSelected(iconList, &clickedIcon);
      if (clickedIcon < 0)
	return;

      if (multitaskerSpawn(&execProgram, "exec program", 1,
			   (void *[]){ icons[clickedIcon].command }) < 0)
	error("Unable to exectute command");
    }
}


static int constructWindow(void)
{
  int status = 0;
  componentParameters params;

  // Create a new window, with small, arbitrary size and location
  window = windowNew(processId, windowTitle);
  if (window == NULL)
    return (status = ERR_NOTINITIALIZED);

  bzero(&params, sizeof(componentParameters));
  params.gridWidth = 1;
  params.gridHeight = 1;
  params.padTop = 5;
  params.padBottom = 5;
  params.padLeft = 5;
  params.padRight = 5;
  params.orientationX = orient_center;
  params.orientationY = orient_middle;
  params.useDefaultForeground = 1;
  params.useDefaultBackground = 1;

  // Create a window list to hold the icons
  iconList = windowNewList(window, windowlist_icononly, rows, columns, 0,
			   iconParams, numIcons, &params);
  windowRegisterEventHandler(iconList, &eventHandler);

  // Register an event handler to catch window close events
  windowRegisterEventHandler(window, &eventHandler);

  windowSetVisible(window, 1);

  return (status = 0);
}


static void deallocateMemory(void)
{
  if (iconParams)
    free(iconParams);
  if (icons)
    free(icons);
}


int main(int argc, char *argv[])
{
  int status = 0;
  char fileName[MAX_PATH_NAME_LENGTH];

  // Only work in graphics mode
  if (!graphicsAreEnabled())
    {
      printf("\nThe \"%s\" command only works in graphics mode\n",
	     (argc? argv[0] : ""));
      return (errno = ERR_NOTINITIALIZED);
    }

  // What is my process id?
  processId = multitaskerGetCurrentProcessId();
  
  // What is my privilege level?
  privilege = multitaskerGetProcessPrivilege(processId);

  // Make sure our config file has been specified
  if (argc != 2)
    {
      printf("usage:\n%s <config_file>\n", (argc? argv[0] : ""));
      return (errno = ERR_INVALID);
    }
  
  // If the argument is RELATIVE pathnames, we should insert the pwd before it
  vshMakeAbsolutePath(argv[1], fileName);

  // Try to read the specified config file
  status = readConfig(fileName);
  if (status < 0)
    {
      deallocateMemory();
      return (errno = status);
    }

  status = constructWindow();
  if (status < 0)
    {
      deallocateMemory();
      return (errno = status);
    }

  // Run the GUI
  windowGuiRun();

  // We're back.
  windowGuiStop();
  windowDestroy(window);

  // Deallocate memory
  deallocateMemory();

  return (status = 0);
}
