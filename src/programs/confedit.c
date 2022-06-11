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
//  confedit.c
//

// This is a program for conveniently editing generic configuration files in
// graphics mode

/* This is the text that appears when a user requests help about this program
<help>

 -- confedit --

Edit Visopsys configuration files

Usage:
  confedit [file_name]

(Only available in graphics mode)

The confedit (Configuration Editor) program is interactive.  The name of the
file to edit can (optionally) be specified on the command line; otherwise
the program will prompt for the name of the file.  You can add, delete, and
modify variables.

Examples of configuration files include the kernel configuration,
kernel.conf, and the window manager configuration, windowmanager.conf.

</help>
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/api.h>
#include <sys/vsh.h>

static int processId = 0;
static int privilege = 0;
static char fileName[MAX_PATH_NAME_LENGTH];
static int readOnly = 1;
static variableList list;
static listItemParameters *listItemParams = NULL;
static int changesPending = 0;
static objectKey window = NULL;
static objectKey menuSave = NULL;
static objectKey menuQuit = NULL;
static objectKey listList = NULL;
static objectKey addVariableButton = NULL;
static objectKey changeVariableButton = NULL;
static objectKey deleteVariableButton = NULL;


static int readConfigFile(void)
{
  // Read the configuration file

  int status = configurationReader(fileName, &list);
  if (status < 0)
    return (status);

  changesPending = 0;

  return (status = 0);
}


static int writeConfigFile(void)
{
  // Write the configuration file

  int status = configurationWriter(fileName, &list);

  changesPending = 0;

  return(status);
}


static void fillList(void)
{
  int count;

  if (listItemParams)
    free(listItemParams);
  listItemParams = malloc(list.numVariables * sizeof(listItemParameters));

  for (count = 0; count < list.numVariables; count ++)
    snprintf(listItemParams[count].text, WINDOW_MAX_LABEL_LENGTH, "%s=%s",
	     list.variables[count], list.values[count]);
}


static void setVariableDialog(const char *variable)
{
  // This will pop up a dialog that prompts the user to set either the
  // variable name and value, or just the value (depending on whether the
  // 'variable' parameter, above, is NULL.  After it gets the info it
  // sets them in the list and refreshes the display

  int status = 0;
  objectKey dialogWindow = NULL;
  componentParameters params;
  int fieldWidth = 30;
  objectKey variableField = NULL;
  objectKey valueField = NULL;
  objectKey okButton = NULL;
  objectKey cancelButton = NULL;
  char variableBuffer[128];
  char valueBuffer[128];
  int okay = 0;
  windowEvent event;
  int count;

  // Create the dialog
  if (variable)
    {
      dialogWindow = windowNewDialog(window, "Change variable");
      strncpy(variableBuffer, variable, 128);
    }
  else
    dialogWindow = windowNewDialog(window, "Add variable");
  if (dialogWindow == NULL)
    return;

  bzero(&params, sizeof(componentParameters));
  params.gridWidth = 1;
  params.gridHeight = 1;
  params.padLeft = 5;
  params.padRight = 5;
  params.padTop = 5;
  params.orientationX = orient_center;
  params.orientationY = orient_middle;

  params.orientationX = orient_right;
  windowNewTextLabel(dialogWindow, "Variable name:", &params);

  if (variable)
    {
      variableListGet(&list, variableBuffer, valueBuffer, 128);
      fieldWidth = max(strlen(variableBuffer), strlen(valueBuffer)) + 1;
      fieldWidth = max(fieldWidth, 30);
    }

  params.gridX = 1;
  params.padRight = 5;
  params.orientationX = orient_left;
  if (variable)
    {
      windowNewTextLabel(dialogWindow, variableBuffer, &params);
    }
  else
    {
      params.flags |= WINDOW_COMPFLAG_HASBORDER;
      variableField = windowNewTextField(dialogWindow, fieldWidth, &params);
    }

  params.gridX = 0;
  params.gridY = 1;
  params.padRight = 0;
  params.orientationX = orient_right;
  params.flags &= ~WINDOW_COMPFLAG_HASBORDER;
  windowNewTextLabel(dialogWindow, "value:", &params);

  params.gridX = 1;
  params.padRight = 5;
  params.flags |= WINDOW_COMPFLAG_HASBORDER;
  valueField = windowNewTextField(dialogWindow, fieldWidth, &params);
  if (variable)
    {
      variableListGet(&list, variableBuffer, valueBuffer, 128);
      windowComponentSetData(valueField, valueBuffer, 128);
    }

  // Create the OK button
  params.gridX = 0;
  params.gridY = 2;
  params.padBottom = 5;
  params.padRight = 0;
  params.orientationX = orient_right;
  params.flags &= ~WINDOW_COMPFLAG_HASBORDER;
  params.flags |= WINDOW_COMPFLAG_FIXEDWIDTH;
  okButton = windowNewButton(dialogWindow, "OK", NULL, &params);

  // Create the Cancel button
  params.gridX = 1;
  params.padRight = 5;
  params.orientationX = orient_left;
  cancelButton = windowNewButton(dialogWindow, "Cancel", NULL, &params);

  windowCenterDialog(window, dialogWindow);
  windowSetVisible(dialogWindow, 1);
  
  while(1)
    {
      // Check for the OK button
      status = windowComponentEventGet(okButton, &event);
      if ((status > 0) && (event.type == EVENT_MOUSE_LEFTUP))
	okay = 1;

      // Check for pressing enter in either of the text fields
      status = windowComponentEventGet(valueField, &event);
      if ((status > 0) && (event.type == EVENT_KEY_DOWN) &&
	  (event.key == (unsigned char) 10))
	okay = 1;
      if (!variable)
	{
	  status = windowComponentEventGet(variableField, &event);
	  if ((status > 0) && (event.type == EVENT_KEY_DOWN) &&
	      (event.key == (unsigned char) 10))
	    okay = 1;
	}

      if (okay)
	{
	  windowComponentGetData(valueField, valueBuffer, 128);

	  if (variable)
	    {
	      variableListSet(&list, variableBuffer, valueBuffer);
	      changesPending += 1;
	    }
	  else
	    {
	      windowComponentGetData(variableField, variableBuffer, 128);
	      if (variableBuffer[0] != '\0')
		{
		  variableListSet(&list, variableBuffer, valueBuffer);
		  changesPending += 1;
		}
	    }

	  fillList();
	  windowComponentSetData(listList, listItemParams, list.numVariables);
	  windowComponentSetSelected(listList, 1);

	  // Select the one we just added/changed
	  for (count = 0; count < list.numVariables; count ++)
	    if (!strncmp(variableBuffer, listItemParams[count].text,
			 strlen(variableBuffer)))
	      {
		windowComponentSetSelected(listList, count);
		break;
	      }

	  break;
	}

      // Check for the Cancel button
      status = windowComponentEventGet(cancelButton, &event);
      if ((status > 0) && (event.type == EVENT_MOUSE_LEFTUP))
	break;
      
      // Check for window close events
      status = windowComponentEventGet(dialogWindow, &event);
      if ((status > 0) && (event.type == EVENT_WINDOW_CLOSE))
	break;

      // Done
      multitaskerYield();
    }
  
  windowDestroy(dialogWindow);
  return;
}


static void eventHandler(objectKey key, windowEvent *event)
{
  int selected = 0;

  // Check for the window being closed by a GUI event.
  if (((key == window) && (event->type == EVENT_WINDOW_CLOSE)) ||
      ((key == menuQuit) && (event->type & EVENT_SELECTION)))
    {
      if (changesPending && !readOnly)
	{
	  selected =
	    windowNewChoiceDialog(window, "Unsaved changes",
				  "Quit without saving changes?",
				  (char *[]) { "Save", "Quit", "Cancel" },
				  3, 0);

	  if ((selected < 0) || (selected == 2))
	    return;

	  else if (selected == 0)
	    writeConfigFile();
	}
      windowGuiStop();
    }
  else if ((key == menuSave) && (event->type & EVENT_SELECTION))
    {
      writeConfigFile();
    }
  else if ((key == addVariableButton) && (event->type == EVENT_MOUSE_LEFTUP))
    {
      setVariableDialog(NULL);
    }
  else if ((key == changeVariableButton) &&
	   (event->type == EVENT_MOUSE_LEFTUP))
    {
      windowComponentGetSelected(listList, &selected);
      setVariableDialog(list.variables[selected]);
    }
  else if ((key == deleteVariableButton) &&
	   (event->type == EVENT_MOUSE_LEFTUP))
    {
      windowComponentGetSelected(listList, &selected);
      variableListUnset(&list, list.variables[selected]);
      changesPending += 1;
      fillList();
      windowComponentSetData(listList, listItemParams, list.numVariables);
    }
}


static void constructWindow(void)
{
  // If we are in graphics mode, make a window rather than operating on the
  // command line.

  componentParameters params;
  objectKey buttonContainer = NULL;

  // Create a new window
  window = windowNew(processId, "Configuration Editor");
  if (window == NULL)
    return;

  bzero(&params, sizeof(componentParameters));
  params.gridWidth = 1;
  params.gridHeight = 1;
  params.padLeft = 5;
  params.padRight = 5;
  params.padTop = 5;
  params.padBottom = 5;
  params.orientationX = orient_left;
  params.orientationY = orient_middle;

  // Create the top 'file' menu
  objectKey menuBar = windowNewMenuBar(window, &params);
  objectKey menu = windowNewMenu(menuBar, "File", &params);
  menuSave = windowNewMenuItem(menu, "Save", &params);
  windowRegisterEventHandler(menuSave, &eventHandler);
  if (privilege || readOnly)
    windowComponentSetEnabled(menuSave, 0);
  menuQuit = windowNewMenuItem(menu, "Quit", &params);
  windowRegisterEventHandler(menuQuit, &eventHandler);

  params.gridY = 1;
  params.padRight = 0;
  params.orientationX = orient_center;
  fontLoad("arial-bold-10.bmp", "arial-bold-10", &(params.font), 0);
  listList =
    windowNewList(window, windowlist_textonly, min(10, list.numVariables),
		  1, 0, listItemParams, list.numVariables, &params);

  // Make a container component for the buttons
  params.gridX = 1;
  params.padRight = 5;
  params.orientationX = orient_left;
  params.orientationY = orient_top;
  params.flags |= WINDOW_COMPFLAG_FIXEDHEIGHT;
  params.font = NULL;
  buttonContainer = windowNewContainer(window, "buttonContainer", &params);

  // Create an 'add variable' button
  params.gridX = 0;
  params.gridY = 0;
  params.padLeft = 0;
  params.padRight = 0;
  params.padTop = 0;
  params.padBottom = 0;
  params.flags &= ~WINDOW_COMPFLAG_FIXEDHEIGHT;
  addVariableButton =
    windowNewButton(buttonContainer, "Add variable", NULL, &params);
  windowRegisterEventHandler(addVariableButton, &eventHandler);

  // Create a 'change variable' button
  params.gridY = 1;
  params.padTop = 5;
  changeVariableButton =
    windowNewButton(buttonContainer, "Change variable", NULL, &params);
  windowRegisterEventHandler(changeVariableButton, &eventHandler);
      
  // Create a 'delete variable' button
  params.gridY = 2;
  deleteVariableButton =
    windowNewButton(buttonContainer, "Delete variable", NULL, &params);
  windowRegisterEventHandler(deleteVariableButton, &eventHandler);

  // Register an event handler to catch window close events
  windowRegisterEventHandler(window, &eventHandler);

  windowSetVisible(window, 1);

  return;
}


int main(int argc, char *argv[])
{
  int status = 0;
  disk theDisk;
  char tmpFilename[MAX_PATH_NAME_LENGTH];

  // Only work in graphics mode
  if (!graphicsAreEnabled())
    {
      printf("\nThe \"%s\" command only works in graphics mode\n", argv[0]);
      errno = ERR_NOTINITIALIZED;
      return (status = errno);
    }

  processId = multitaskerGetCurrentProcessId();
  privilege = multitaskerGetProcessPrivilege(processId);

  // If a configuration file was not specified, ask for it
  if (argc < 2)
    {
      // Start in the config dir by default
      status =
	windowNewFileDialog(NULL, "Enter filename", "Please enter a "
			    "configuration file to edit:", "/system/config",
			    tmpFilename, MAX_PATH_NAME_LENGTH);
      if (status != 1)
	{
	  windowGuiStop();
	  return (errno = status);
	}
    }
  else
    strncpy(tmpFilename, argv[1], MAX_PATH_NAME_LENGTH);

  vshMakeAbsolutePath(tmpFilename, fileName);

  // Find out whether we are currently running on a read-only filesystem
  if (!fileGetDisk(fileName, &theDisk))
    readOnly = theDisk.readOnly;

  // Read the config file
  status = readConfigFile();
  if (status < 0)
    return (errno = status);

  fillList();

  // Make our window
  constructWindow();

  // Run the GUI
  windowGuiRun();

  windowDestroy(window);
  
  variableListDestroy(&list);

  // Done
  return (errno = 0);
}
