//
//  Visopsys
//  Copyright (C) 1998-2014 J. Andrew McLaughlin
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
kernel.conf, and the window manager configuration, window.conf.

</help>
*/

#include <libintl.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/api.h>
#include <sys/ascii.h>
#include <sys/font.h>
#include <sys/vsh.h>

#define _(string) gettext(string)
#define gettext_noop(string) (string)

static int processId = 0;
static int privilege = 0;
static char fileName[MAX_PATH_NAME_LENGTH];
static int readOnly = 1;
static variableList list;
static listItemParameters *listItemParams = NULL;
static int changesPending = 0;
static objectKey window = NULL;
static objectKey fileMenu = NULL;
static objectKey listList = NULL;
static objectKey addVariableButton = NULL;
static objectKey changeVariableButton = NULL;
static objectKey deleteVariableButton = NULL;

#define FILEMENU_SAVE 0
#define FILEMENU_QUIT 1
windowMenuContents fileMenuContents = {
	2,
	{
		{ gettext_noop("Save"), NULL },
		{ gettext_noop("Quit"), NULL }
	}
};


__attribute__((format(printf, 1, 2)))
static void error(const char *format, ...)
{
	// Generic error message code
	
	va_list args;
	char output[MAXSTRINGLENGTH];
	
	va_start(args, format);
	vsnprintf(output, MAXSTRINGLENGTH, format, args);
	va_end(args);

	windowNewErrorDialog(window, _("Error"), output);
}


static int readConfigFile(void)
{
	// Read the configuration file

	int status = 0;

	status = configRead(fileName, &list);
	if (status < 0)
	{
		error(_("Error %d reading configuration file."), status);
		return (status);
	}

	changesPending = 0;
	return (status = 0);
}


static int writeConfigFile(void)
{
	// Write the configuration file

	int status = 0;

	status = configWrite(fileName, &list);
	if (status < 0)
		error(_("Error %d writing configuration file."), status);
	else
		changesPending = 0;

	return (status);
}


static void fillList(void)
{
	int count;

	if (listItemParams)
	free(listItemParams);

	listItemParams = NULL;

	if (list.numVariables)
	{
		listItemParams = malloc(list.numVariables * sizeof(listItemParameters));

		for (count = 0; count < list.numVariables; count ++)
			snprintf(listItemParams[count].text, WINDOW_MAX_LABEL_LENGTH,
				"%s=%s", list.variables[count], list.values[count]);
	}
	else
	{
		// Create an empty list + list item params
		variableListCreate(&list);
		listItemParams = malloc(sizeof(listItemParameters));
	}

	if (changeVariableButton)
		windowComponentSetEnabled(changeVariableButton, list.numVariables);
	if (deleteVariableButton)
		windowComponentSetEnabled(deleteVariableButton, list.numVariables);
}


static void setVariableDialog(char *variable)
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
	char variableBuff[128];
	char value[128];
	int okay = 0;
	windowEvent event;
	int count;

	// Create the dialog
	if (variable)
	{
		dialogWindow = windowNewDialog(window, _("Change Variable"));
		strncpy(variableBuff, variable, 128);
		variable = variableBuff;
	}
	else
		dialogWindow = windowNewDialog(window, _("Add Variable"));
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
	windowNewTextLabel(dialogWindow, _("Variable name:"), &params);

	if (variable)
	{
		variableListGet(&list, variable, value, 128);
		fieldWidth = max(strlen(variable), strlen(value)) + 1;
		fieldWidth = max(fieldWidth, 30);
	}

	params.gridX = 1;
	params.padRight = 5;
	params.orientationX = orient_left;
	if (variable)
		windowNewTextLabel(dialogWindow, variable, &params);
	else
	{
		variableField = windowNewTextField(dialogWindow, fieldWidth, &params);
		windowComponentFocus(variableField);
	}

	params.gridX = 0;
	params.gridY = 1;
	params.padRight = 0;
	params.orientationX = orient_right;
	windowNewTextLabel(dialogWindow, _("value:"), &params);

	params.gridX = 1;
	params.padRight = 5;
	valueField = windowNewTextField(dialogWindow, fieldWidth, &params);
	if (variable)
	{
		windowComponentSetData(valueField, value, 128);
		windowComponentFocus(valueField);
	}

	// Create the OK button
	params.gridX = 0;
	params.gridY = 2;
	params.padBottom = 5;
	params.padRight = 0;
	params.orientationX = orient_right;
	params.flags |= WINDOW_COMPFLAG_FIXEDWIDTH;
	okButton = windowNewButton(dialogWindow, _("OK"), NULL, &params);

	// Create the Cancel button
	params.gridX = 1;
	params.padRight = 5;
	params.orientationX = orient_left;
	cancelButton = windowNewButton(dialogWindow, _("Cancel"), NULL, &params);

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
			(event.key == (unsigned char) ASCII_ENTER))
		{
			okay = 1;
		}

		if (!variable)
		{
			status = windowComponentEventGet(variableField, &event);
			if ((status > 0) && (event.type == EVENT_KEY_DOWN) &&
				(event.key == (unsigned char) ASCII_ENTER))
			{
				okay = 1;
			}
		}

		if (okay)
		{
			windowComponentGetData(valueField, value, 128);

			if (!variable)
			{
				variable = variableBuff;
				windowComponentGetData(variableField, variable, 128);
			}

			if (variable[0] != '\0')
			{
				variableListSet(&list, variable, value);
				changesPending += 1;

				fillList();
				windowComponentSetData(listList, listItemParams,
					list.numVariables);

				// Select the one we just added/changed
				for (count = 0; count < list.numVariables; count ++)
				{
					if (!strcmp(variable, list.variables[count]))
					{
						windowComponentSetSelected(listList, count);
						break;
					}
				}
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


static void quit(void)
{
	int selected = 0;

	if (changesPending && !readOnly)
	{
		selected =
			windowNewChoiceDialog(window, _("Unsaved changes"),
				_("Quit without saving changes?"),
				(char *[]) { _("Save"), _("Quit"), _("Cancel") }, 3, 0);

		if ((selected < 0) || (selected == 2))
			return;

		else if (selected == 0)
			writeConfigFile();
	}

	windowGuiStop();
}


static void eventHandler(objectKey key, windowEvent *event)
{
	int selected = -1;
	objectKey selectedItem = NULL;

	// Check for the window being closed by a GUI event.
	if ((key == window) && (event->type == EVENT_WINDOW_CLOSE))
		quit();

	// Check for menu events
	else if ((key == fileMenu) && (event->type & EVENT_SELECTION))
	{
		windowComponentGetData(fileMenu, &selectedItem, 1);
		if (selectedItem)
		{
			if (selectedItem == fileMenuContents.items[FILEMENU_SAVE].key)
				writeConfigFile();
			else if (selectedItem == fileMenuContents.items[FILEMENU_QUIT].key)
				quit();
		}
	}

	else if ((key == addVariableButton) && (event->type == EVENT_MOUSE_LEFTUP))
		setVariableDialog(NULL);

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


static void constructWindow(void)
{
	// If we are in graphics mode, make a window rather than operating on the
	// command line.

	componentParameters params;
	objectKey buttonContainer = NULL;

	// Create a new window
	window = windowNew(processId, _("Configuration Editor"));
	if (window == NULL)
		return;

	bzero(&params, sizeof(componentParameters));

	// Create the top 'file' menu
	objectKey menuBar = windowNewMenuBar(window, &params);
	initMenuContents(&fileMenuContents);
	fileMenu = windowNewMenu(menuBar, _("File"), &fileMenuContents, &params);
	windowRegisterEventHandler(fileMenu, &eventHandler);
	if (privilege || readOnly)
		windowComponentSetEnabled(fileMenuContents.items[FILEMENU_SAVE].key, 0);

	params.gridWidth = 1;
	params.gridHeight = 1;
	params.padLeft = 5;
	params.padTop = 5;
	params.padBottom = 5;
	params.orientationX = orient_left;
	params.orientationY = orient_middle;

	params.orientationX = orient_center;
	if (fileFind(FONT_SYSDIR "/arial-bold-10.vbf", NULL) >= 0)
		fontLoad("arial-bold-10.vbf", "arial-bold-10", &(params.font), 0);
	listList =
		windowNewList(window, windowlist_textonly, min(10, list.numVariables),
			1, 0, listItemParams, list.numVariables, &params);
	windowComponentFocus(listList);

	// Make a container component for the buttons
	params.gridX += 1;
	params.padRight = 5;
	params.orientationX = orient_left;
	params.orientationY = orient_top;
	params.flags |= (WINDOW_COMPFLAG_FIXEDWIDTH | WINDOW_COMPFLAG_FIXEDHEIGHT);
	params.font = NULL;
	buttonContainer = windowNewContainer(window, "buttonContainer", &params);

	// Create an 'add variable' button
	params.gridX = 0;
	params.gridY = 0;
	params.padLeft = 0;
	params.padRight = 0;
	params.padTop = 0;
	params.padBottom = 0;
	params.flags &= ~(WINDOW_COMPFLAG_FIXEDWIDTH | WINDOW_COMPFLAG_FIXEDHEIGHT);
	addVariableButton =
		windowNewButton(buttonContainer, _("Add variable"), NULL, &params);
	windowRegisterEventHandler(addVariableButton, &eventHandler);

	// Create a 'change variable' button
	params.gridY += 1;
	params.padTop = 5;
	changeVariableButton =
	windowNewButton(buttonContainer, _("Change variable"), NULL, &params);
	windowRegisterEventHandler(changeVariableButton, &eventHandler);
	windowComponentSetEnabled(changeVariableButton, list.numVariables);
	
	// Create a 'delete variable' button
	params.gridY += 1;
	deleteVariableButton =
		windowNewButton(buttonContainer, _("Delete variable"), NULL, &params);
	windowRegisterEventHandler(deleteVariableButton, &eventHandler);
	windowComponentSetEnabled(deleteVariableButton, list.numVariables);

	// Register an event handler to catch window close events
	windowRegisterEventHandler(window, &eventHandler);

	windowSetVisible(window, 1);

	return;
}


int main(int argc, char *argv[])
{
	int status = 0;
	char *language = "";
	disk theDisk;
	file tmpFile;

	#ifdef BUILDLANG
		language=BUILDLANG;
	#endif
	setlocale(LC_ALL, language);
	textdomain("confedit");

	// Only work in graphics mode
	if (!graphicsAreEnabled())
	{
		printf(_("\nThe \"%s\" command only works in graphics mode\n"), argv[0]);
		return (status = ERR_NOTINITIALIZED);
	}

	processId = multitaskerGetCurrentProcessId();
	privilege = multitaskerGetProcessPrivilege(processId);

	// If a configuration file was not specified, ask for it
	if (argc < 2)
	{
		// Start in the config dir by default
		status =
			windowNewFileDialog(NULL, _("Enter filename"),
				_("Please enter a configuration file to edit:"),
				"/system/config", fileName, MAX_PATH_NAME_LENGTH, 0);
		if (status != 1)
		{
			if (status != 0)
				perror(argv[0]);
			
			return (status);
		}
	}
	else
		strncpy(fileName, argv[argc - 1], MAX_PATH_NAME_LENGTH);

	// See whether the file exists
	status = fileFind(fileName, &tmpFile);
	if (status < 0)
	{
		status = fileOpen(fileName, OPENMODE_CREATE, &tmpFile);
		if (status < 0)
		{
			error(_("Error %d creating new configuration file."), status);
			return (status);
		}

		fileClose(&tmpFile);
	}

	// Find out whether we are currently running on a read-only filesystem
	if (!fileGetDisk(fileName, &theDisk))
		readOnly = theDisk.readOnly;

	// Read the config file
	status = readConfigFile();
	if (status < 0)
		return (status);

	fillList();

	// Make our window
	constructWindow();

	// Run the GUI
	windowGuiRun();

	windowDestroy(window);
	
	variableListDestroy(&list);

	// Done
	return (status = 0);
}
