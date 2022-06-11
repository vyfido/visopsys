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
//  keymap.c
//

// This is a program for showing and changing the keyboard mapping.  Works
// in both text and graphics modes.

/* This is the text that appears when a user requests help about this program
<help>

 -- keymap --

View or change the current keyboard mapping

Usage:
  keymap [-T] [-s file_name] [map_name]

The keymap program can be used to view the available keyboard mapping, or
set the current map.  It works in both text and graphics modes:

In text mode: If no map is specified on the command line, then all available
mappings are listed, with the current one indicated at the top of the list.
To change the map, the user must enter the new name in exactly the same
format as shown by the command (with double quotes (") around it if it
contains space characters).

In graphics mode, the program is interactive and the user can choose a new
mapping simply by clicking.

Options:
-s  : Save the specified map to the specified file name.
-T  : Force text mode operation

</help>
*/

#include <libgen.h>
#include <libintl.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/api.h>
#include <sys/ascii.h>
#include <sys/font.h>

#define KERNEL_CONF		"/system/config/kernel.conf"

#define _(string) gettext(string)

typedef struct {
	int scanCode;
	unsigned char regMap;
	unsigned char shiftMap;
	unsigned char controlMap;
	unsigned char altGrMap;
	objectKey button;
	int buttonRow;
	int buttonColumn;

} scanKey;

static int graphics = 0;
static char *cwd = NULL;
static char currentName[KEYMAP_NAMELEN];
static keyMap *selectedMap = NULL;
static listItemParameters *mapListParams = NULL;
static int numMapNames = 0;
static scanKey *keyArray = NULL;
static objectKey window = NULL;
static objectKey mapList = NULL;
static objectKey currentLabel = NULL;
static objectKey nameField = NULL;
static objectKey diagContainer = NULL;
static objectKey saveButton = NULL;
static objectKey defaultButton = NULL;
static objectKey closeButton = NULL;


__attribute__((format(printf, 1, 2)))
static void error(const char *format, ...)
{
	// Generic error message code for either text or graphics modes
	
	va_list list;
	char output[MAXSTRINGLENGTH];
	
	va_start(list, format);
	vsnprintf(output, MAXSTRINGLENGTH, format, list);
	va_end(list);

	if (graphics)
		windowNewErrorDialog(window, _("Error"), output);
	else
		fprintf(stderr, _("\n\nERROR: %s\n\n"), output);
}


static int readMap(const char *fileName, keyMap *map)
{
	int status = 0;
	fileStream theStream;

	bzero(&theStream, sizeof(fileStream));

	status = fileStreamOpen(fileName, OPENMODE_READ, &theStream);
	if (status < 0)
	{
		error(_("Couldn't open file %s"), fileName);
		return (status);
	}

	status = fileStreamRead(&theStream, sizeof(keyMap), (char *) map);

	fileStreamClose(&theStream);

	if (status < 0)
	{
		error(_("Couldn't read file %s"), fileName);
		return (status);
	}

	// Check the magic number
	if (strncmp(map->magic, KEYMAP_MAGIC, sizeof(KEYMAP_MAGIC)))
		return (status = ERR_BADDATA);

	return (status = 0);
}


static int getMapNames(char *nameBuffer)
{
	// Look in the keymap directory for keymap files.

	int status = 0;
	file theFile;
	char *fileName = NULL;
	keyMap *map = NULL;
	int bufferChar = 0;
	int count;

	fileName = malloc(MAX_PATH_NAME_LENGTH);
	map = malloc(sizeof(file));
	if ((fileName == NULL) || (map == NULL))
		return (status = ERR_MEMORY);

	nameBuffer[0] = '\0';
	numMapNames = 0;

	// Loop through the files in the keymap directory
	for (count = 0; ; count ++)
	{
		if (count)
			status = fileNext(cwd, &theFile);
		else
			status = fileFirst(cwd, &theFile);

		if (status < 0)
			break;

		if (theFile.type != fileT)
			continue;

		if (strcmp(cwd, "/"))
			snprintf(fileName, MAX_PATH_NAME_LENGTH, "%s/%s", cwd, theFile.name);
		else
			snprintf(fileName, MAX_PATH_NAME_LENGTH, "/%s", theFile.name);

		status = readMap(fileName, map);
		if (status < 0)
			// Not a keymap file we can read.
			continue;

		strncpy((nameBuffer + bufferChar), map->name, sizeof(map->name));
		bufferChar += (strlen(map->name) + 1);
		numMapNames += 1;
	}

	free(fileName);
	free(map);
	return (status = 0);
}


static int getMapNameParams(void)
{
	// Get the list of keyboard map names from the kernel

	int status = 0;
	char *nameBuffer = NULL;
	char *buffPtr = NULL;
	int count;

	nameBuffer = malloc(1024);
	if (nameBuffer == NULL)
		return (status = ERR_MEMORY);

	status = getMapNames(nameBuffer);
	if (status < 0)
		goto out;

	if (mapListParams)
		free(mapListParams);

	mapListParams = malloc(numMapNames * sizeof(listItemParameters));
	if (mapListParams == NULL)
	{
		status = ERR_MEMORY;
		goto out;
	}

	buffPtr = nameBuffer;

	for (count = 0; count < numMapNames; count ++)
	{
		strncpy(mapListParams[count].text, buffPtr, WINDOW_MAX_LABEL_LENGTH);
		buffPtr += (strlen(mapListParams[count].text) + 1);
	}
	
	status = 0;

out:
	free(nameBuffer);
	return (status);
}


static void makeKeyArray(keyMap *map)
{
	int count;

	for (count = 0; count < KEYSCAN_CODES; count ++)
	{
		keyArray[count].scanCode = (count + 1);
		keyArray[count].regMap = map->regMap[count];
		keyArray[count].shiftMap = map->shiftMap[count];
		keyArray[count].controlMap = map->controlMap[count];
		keyArray[count].altGrMap = map->altGrMap[count];
	}

	return;
}


static int findMapFile(const char *mapName, char *fileName)
{
	// Look in the current directory for the keymap file with the supplied map
	// name

	int status = 0;
	file theFile;
	keyMap *map = NULL;
	int count;

	map = malloc(sizeof(keyMap));
	if (map == NULL)
		return (status = ERR_MEMORY);

	bzero(&theFile, sizeof(file));

	// Loop through the files in the keymap directory
	for (count = 0; ; count ++)
	{
		if (count)
			status = fileNext(cwd, &theFile);
		else
			status = fileFirst(cwd, &theFile);

		if (status < 0)
			// No more files.
			break;

		if (theFile.type != fileT)
			continue;

		if (strcmp(cwd, "/"))
			snprintf(fileName, MAX_PATH_NAME_LENGTH, "%s/%s", cwd, theFile.name);
		else
			snprintf(fileName, MAX_PATH_NAME_LENGTH, "/%s", theFile.name);

		status = readMap(fileName, map);
		if (status < 0)
			continue;

		if (!strncmp(map->name, mapName, sizeof(map->name)))
		{
			status = 0;
			goto out;
		}
	}

	// If we fall through to here, it wasn't found.
	fileName[0] = '\0';
	status = ERR_NOSUCHENTRY;

out:
	free(map);
	return (status);
}


static int loadMap(const char *mapName)
{
	int status = 0;
	char *fileName = NULL;
	fileStream theStream;

	fileName = malloc(MAX_PATH_NAME_LENGTH);
	if (fileName == NULL)
		return (status = ERR_MEMORY);

	bzero(&theStream, sizeof(fileStream));

	// Find the map by name
	status = findMapFile(mapName, fileName);
	if (status < 0)
	{
		error(_("Couldn't find keyboard map %s"), mapName);
		goto out;
	}

	// Read it in
	status = readMap(fileName, selectedMap);

out:
	free(fileName);
	return (status);
}


static int saveMap(const char *fileName)
{
	int status = 0;
	disk mapDisk;
	fileStream theStream;
	int count;

	bzero(&mapDisk, sizeof(disk));
	bzero(&theStream, sizeof(fileStream));

	// Find out whether the file is on a read-only filesystem
	if (!fileGetDisk(fileName, &mapDisk) && mapDisk.readOnly)
	{
		error(_("Can't write %s:\nFilesystem is read-only"), fileName);
		return (status = ERR_NOWRITE);
	}

	if (graphics && nameField)
		// Get the map name
		windowComponentGetData(nameField, selectedMap->name, 32);

	for (count = 0; count < KEYSCAN_CODES; count ++)
	{
		selectedMap->regMap[count] = keyArray[count].regMap;
		selectedMap->shiftMap[count] = keyArray[count].shiftMap;
		selectedMap->controlMap[count] = keyArray[count].controlMap;
		selectedMap->altGrMap[count] = keyArray[count].altGrMap;
	}

	status = fileStreamOpen(fileName, (OPENMODE_CREATE | OPENMODE_WRITE |
		OPENMODE_TRUNCATE), &theStream);
	if (status < 0)
	{
		error(_("Couldn't open file %s"), fileName);
		return (status);
	}

	status = fileStreamWrite(&theStream, sizeof(keyMap), (char *) selectedMap);

	fileStreamClose(&theStream);

	if (status < 0)
		error(_("Couldn't write file %s"), fileName);

	return (status);
}


static int setMap(const char *mapName)
{
	// Change the current mapping in the kernel, and also change the config for
	// persistence at the next reboot

	int status = 0;
	char *fileName = NULL;
	disk confDisk;

	fileName = malloc(MAX_PATH_NAME_LENGTH);
	if (fileName == NULL)
		return (status = ERR_MEMORY);

	status = findMapFile(mapName, fileName);
	if (status < 0)
	{
		error(_("Couldn't find keyboard map %s"), mapName);
		goto out;
	}

	status = keyboardSetMap(fileName);
	if (status < 0)
	{
		error(_("Couldn't set keyboard map to %s"), fileName);
		goto out;
	}

	status = keyboardGetMap(selectedMap);
	if (status < 0)
	{
		error("%s", _("Couldn't get current keyboard map"));
		goto out;
	}

	strncpy(currentName, selectedMap->name, KEYMAP_NAMELEN);

	// Find out whether the kernel config file is on a read-only filesystem
	bzero(&confDisk, sizeof(disk));
	if (!fileGetDisk(KERNEL_CONF, &confDisk) && !confDisk.readOnly)
	{
		status = configSet(KERNEL_CONF, "keyboard.map", fileName);
		if (status < 0)
			error("%s", _("Couldn't write keyboard map setting"));
	}

out:
	free(fileName);
	return (status);
}


static void getText(unsigned char ascii, char *output)
{
	// Convert an ASCII code into a printable character in the output.

	if (ascii >= 33)
	{
		sprintf(output, "%c", ascii);
		return;
	}

	switch (ascii)
	{
		case ASCII_BACKSPACE:
			strcat(output, _("Backspace"));
			break;
		case ASCII_TAB:
			strcat(output, _("Tab"));
			break;
		case ASCII_ENTER:
			strcat(output, _("Enter"));
			break;
		case ASCII_CRSRUP:
			strcat(output, "/\\");
			break;
		case ASCII_CRSRLEFT:
			strcat(output, "<");
			break;
		case ASCII_CRSRRIGHT:
			strcat(output, ">");
			break;
		case ASCII_CRSRDOWN:
			strcat(output, "\\/");
			break;
		case ASCII_ESC:
			strcat(output, _("Esc"));
			break;
		case ASCII_SPACE:
			strcat(output, _("Space"));
			break;
		case ASCII_DEL:
			strcat(output, _("Del"));
			break;
		case 0:
		default:
			strcat(output, " ");
			break;
	}
}


static void makeButtonString(scanKey *scan, char *string)
{
	string[0] = '\0';

	getText(scan->regMap, string);

	if (scan->shiftMap && (scan->shiftMap != scan->regMap))
		getText(scan->shiftMap, (string + strlen(string)));

	if (scan->altGrMap && (scan->altGrMap != scan->regMap))
		getText(scan->altGrMap, (string + strlen(string)));
}


static int changeKeyDialog(scanKey *scan)
{
	int status = 0;
	objectKey dialogWindow = NULL;
	objectKey regField = NULL;
	objectKey shiftField = NULL;
	objectKey altGrField = NULL;
	objectKey ctrlField = NULL;
	objectKey buttonContainer = NULL;
	objectKey _okButton = NULL;
	objectKey _cancelButton = NULL;
	char string[80];
	windowEvent event;
	componentParameters params;

	dialogWindow = windowNewDialog(window, _("Change key settings"));
	if (dialogWindow == NULL)
		return (status = ERR_NOCREATE);

	bzero(&params, sizeof(componentParameters));
	params.gridWidth = 2;
	params.gridHeight = 1;
	params.padTop = 5;
	params.padLeft = 5;
	params.padRight = 5;
	params.orientationX = orient_left;
	params.orientationY = orient_middle;

	snprintf(string, 80, _("Scan code: %d (0x%02x)"), scan->scanCode,
		scan->scanCode);
	windowNewTextLabel(dialogWindow, string, &params);
	
	params.gridY += 1;
	windowNewTextLabel(dialogWindow, _("ASCII codes:"), &params);

	params.gridWidth = 1;
	params.gridY += 1;
	windowNewTextLabel(dialogWindow, _("Regular code:"), &params);

	params.gridX += 1;
	regField = windowNewTextField(dialogWindow, 10, &params);
	snprintf(string, 10, "%d", scan->regMap);
	windowComponentSetData(regField, string, 10);

	params.gridX = 0;
	params.gridY += 1;
	windowNewTextLabel(dialogWindow, _("Shift code:"), &params);

	params.gridX += 1;
	shiftField = windowNewTextField(dialogWindow, 10, &params);
	snprintf(string, 10, "%d", scan->shiftMap);
	windowComponentSetData(shiftField, string, 10);

	params.gridX = 0;
	params.gridY += 1;
	windowNewTextLabel(dialogWindow, _("Alt Gr code:"), &params);

	params.gridX += 1;
	altGrField = windowNewTextField(dialogWindow, 10, &params);
	snprintf(string, 10, "%d", scan->altGrMap);
	windowComponentSetData(altGrField, string, 10);

	params.gridX = 0;
	params.gridY += 1;
	windowNewTextLabel(dialogWindow, _("Ctrl code:"), &params);

	params.gridX += 1;
	ctrlField = windowNewTextField(dialogWindow, 10, &params);
	snprintf(string, 10, "%d", scan->controlMap);
	windowComponentSetData(ctrlField, string, 10);

	params.gridX = 0;
	params.gridY += 1;
	params.gridWidth = 2;
	params.padBottom = 5;
	params.orientationX = orient_center;
	params.flags |= WINDOW_COMPFLAG_FIXEDWIDTH;
	buttonContainer =
	windowNewContainer(dialogWindow, "buttonContainer", &params);

	params.gridY = 0;
	params.gridWidth = 1;
	params.padTop = 0;
	params.padBottom = 0;
	params.orientationX = orient_right;
	_okButton = windowNewButton(buttonContainer, _("OK"), NULL, &params);
	
	params.gridX += 1;
	params.orientationX = orient_left;
	_cancelButton = windowNewButton(buttonContainer, _("Cancel"), NULL, &params);
	windowComponentFocus(_cancelButton);

	windowCenterDialog(window, dialogWindow);
	windowSetVisible(dialogWindow, 1);

	while(1)
	{
		// Check for the OK button, or 'enter' in any of the text fields
		if (((windowComponentEventGet(_okButton, &event) > 0) &&
			(event.type == EVENT_MOUSE_LEFTUP)) ||
			((windowComponentEventGet(regField, &event) > 0) &&
				(event.type == EVENT_KEY_DOWN) && (event.key == 10)) ||
			((windowComponentEventGet(shiftField, &event) > 0) &&
				(event.type == EVENT_KEY_DOWN) && (event.key == 10)) ||
			((windowComponentEventGet(altGrField, &event) > 0) &&
				(event.type == EVENT_KEY_DOWN) && (event.key == 10)) ||
			((windowComponentEventGet(ctrlField, &event) > 0) &&
				(event.type == EVENT_KEY_DOWN) && (event.key == 10)))
			{
				windowComponentGetData(regField, string, 10);
				scan->regMap = atoi(string);
				windowComponentGetData(shiftField, string, 10);
				scan->shiftMap = atoi(string);
				windowComponentGetData(altGrField, string, 10);
				scan->altGrMap = atoi(string);
				windowComponentGetData(ctrlField, string, 10);
				scan->controlMap = atoi(string);
				break;
			}

		// Check for Cancel button or window close events
		if (((windowComponentEventGet(_cancelButton, &event) > 0) &&
				(event.type == EVENT_MOUSE_LEFTUP)) ||
			((windowComponentEventGet(dialogWindow, &event) > 0) &&
				(event.type == EVENT_WINDOW_CLOSE)))
		{
			break;
		}

		// Done
		multitaskerYield();
	}

	windowDestroy(dialogWindow);
	return (0);
}


static void editKeyHandler(objectKey key, windowEvent *event)
{
	char string[32];
	int count;

	if (event->type == EVENT_MOUSE_LEFTUP)
	{
		// Search through our array and see if it's one of our key buttons
		for (count = 0; count < KEYSCAN_CODES; count ++)
		{
			if (key == keyArray[count].button)
			{
				changeKeyDialog(&keyArray[count]);
				makeButtonString(&keyArray[count], string);
				windowComponentSetData(keyArray[count].button, string,
					sizeof(string));
				break;
			}
		}
	}
}


static void updateKeyDiag(keyMap *map)
{
	char string[32];
	int count;

	windowComponentSetData(nameField, map->name, sizeof(map->name));

	for (count = 0; count < 53; count ++)
	{
		if (keyArray[count].button)
		{
			makeButtonString(&keyArray[count], string);
			windowComponentSetData(keyArray[count].button, string,
				sizeof(string));
		}
	}
}


static objectKey constructKeyDiag(objectKey parent, keyMap *map,
	componentParameters *mainParams)
{
	objectKey mainContainer = NULL;
	objectKey nameContainer = NULL;
	objectKey rowContainer[4];
	char string[32];
	componentParameters params;
	int count = 0;

	mainContainer = windowNewContainer(parent, "diagContainer", mainParams);
	if (mainContainer == NULL)
		return (mainContainer);

	bzero(&params, sizeof(componentParameters));
	params.gridWidth = 1;
	params.gridHeight = 1;
	params.padTop = 5;
	params.padLeft = 5;
	params.padRight = 5;
	params.orientationX = orient_left;
	params.orientationY = orient_middle;
	params.flags |= WINDOW_COMPFLAG_FIXEDWIDTH;

	if (fileFind(FONT_SYSDIR "/xterm-normal-10.vbf", NULL) >= 0)
		fontLoad("xterm-normal-10.vbf", "xterm-normal-10", &(params.font), 1);

	// Make a container for the name field
	nameContainer = windowNewContainer(mainContainer, "nameContainer", &params);

	// The name field
	params.padLeft = 0;
	windowNewTextLabel(nameContainer, _("Name:"), &params);
	params.gridX += 1;
	nameField = windowNewTextField(nameContainer, 30, &params);
	windowComponentSetData(nameField, map->name, MAX_PATH_LENGTH);

	// Make containers for the rows
	params.gridX = 0;
	params.padTop = 0;
	params.padBottom = 0;
	params.padLeft = 5;
	params.padRight = 5;
	params.orientationX = orient_center;
	params.orientationY = orient_middle;
	params.flags &= ~WINDOW_COMPFLAG_FIXEDWIDTH;
	for (count = 0; count < 4; count ++)
	{
		params.gridY += 1;
		if (count == 3)
			params.padBottom = 5;
		rowContainer[count] =
			windowNewContainer(mainContainer, "rowContainer", &params);
	}

	// Loop through the key array and set their columns and rows
	
	// First row
	keyArray[40].buttonColumn = 0;
	keyArray[40].buttonRow = 0;
	for (count = 1; count < 14; count ++)
	{
		keyArray[count].buttonColumn = count;
		keyArray[count].buttonRow = 0;
	}

	// Second row
	for (count = 14; count < 27; count ++)
	{
		keyArray[count].buttonColumn = (count - 14);
		keyArray[count].buttonRow = 1;
	}
	keyArray[42].buttonColumn = (count - 14);
	keyArray[42].buttonRow = 1;

	// Third row
	for (count = 27; count < 41; count ++)
	{
		// Skip any keys that are in different rows
		if ((count == 27) || (count == 40))
			continue;
		keyArray[count].buttonColumn = (count - 27);
		keyArray[count].buttonRow = 2;
	}
	keyArray[27].buttonColumn = (count - 27);
	keyArray[27].buttonRow = 2;

	// Fourth row
	for (count = 41; count < 53; count ++)
	{
		// Skip any keys that are in different rows
		if (count == 42)
			continue;
		keyArray[count].buttonColumn = (count - 41);
		keyArray[count].buttonRow = 3;
	}

	params.padTop = 0;
	params.padBottom = 0;
	params.padLeft = 0;
	params.padRight = 0;
	// Now put the buttons in their containers
	for (count = 0; count < 53; count ++)
	{
		// Skip any keys we don't want
		if ((count == 0) || (count == 28))
			continue;

		makeButtonString(&keyArray[count], string);
		params.gridX = keyArray[count].buttonColumn;
		keyArray[count].button =
			windowNewButton(rowContainer[keyArray[count].buttonRow], string,
				NULL, &params);
		windowRegisterEventHandler(keyArray[count].button, &editKeyHandler);
		if (fontGetPrintedWidth(&params.font, string) <
			fontGetPrintedWidth(&params.font, "@@@"))
		{
			windowComponentSetWidth(keyArray[count].button,
				fontGetPrintedWidth(&params.font, "@@@"));
		}
	}

	return (mainContainer);
}


static void selectMap(const char *mapName)
{
	int count;

	// Select the current map
	for (count = 0; count < numMapNames; count ++)
	{
		if (!strcmp(mapListParams[count].text, mapName))
		{
			windowComponentSetSelected(mapList, count);
			break;
		}
	}
}


static void eventHandler(objectKey key, windowEvent *event)
{
	int status = 0;
	int selected = 0;
	char *fullName = NULL;
	char *dirName = NULL;

	// Check for the window being closed by a GUI event.
	if (((key == window) && (event->type == EVENT_WINDOW_CLOSE)) ||
		((key == closeButton) && (event->type == EVENT_MOUSE_LEFTUP)))
	{
		windowGuiStop();
	}

	else if ((key == mapList) && (event->type & EVENT_SELECTION))
	{
		if (windowComponentGetSelected(mapList, &selected) < 0)
			return;
		if (loadMap(mapListParams[selected].text) < 0)
			return;
		makeKeyArray(selectedMap);
		updateKeyDiag(selectedMap);
	}

	else if ((key == saveButton) && (event->type == EVENT_MOUSE_LEFTUP))
	{
		fullName = malloc(MAX_PATH_NAME_LENGTH);
		if (fullName == NULL)
			return;

		findMapFile(selectedMap->name, fullName);

		status = windowNewFileDialog(window, _("Save as"),
			_("Choose the output file:"), cwd, fullName, MAX_PATH_NAME_LENGTH,
			0);
		if (status != 1)
		{
			free(fullName);
			return;
		}

		status = saveMap(fullName);
		if (status < 0)
		{
			free(fullName);
			return;
		}

		// Are we working in a new directory?
		dirName = dirname(fullName);
		if (dirName)
		{
			strncpy(cwd, dirName, MAX_PATH_LENGTH);
			free(dirName);
		}

		free(fullName);

		if (getMapNameParams() < 0)
			return;

		windowComponentSetData(mapList, mapListParams, numMapNames);

		selectMap(selectedMap->name);

		windowNewInfoDialog(window, _("Saved"), _("Map saved"));
	}

	else if ((key == defaultButton) && (event->type == EVENT_MOUSE_LEFTUP))
	{
		if (windowComponentGetSelected(mapList, &selected) < 0)
			return;
		if (setMap(mapListParams[selected].text) < 0)
			return;
		windowComponentSetData(currentLabel, mapListParams[selected].text,
			strlen(mapListParams[selected].text));
	}
}


static void constructWindow(void)
{
	// If we are in graphics mode, make a window rather than operating on the
	// command line.

	objectKey leftContainer = NULL;
	objectKey bottomContainer = NULL;
	componentParameters params;

	// Create a new window
	window = windowNew(multitaskerGetCurrentProcessId(), _("Keyboard Map"));
	if (window == NULL)
		return;

	bzero(&params, sizeof(componentParameters));
	params.gridWidth = 1;
	params.gridHeight = 1;
	params.padTop = 5;
	params.padLeft = 5;
	params.orientationX = orient_left;
	params.orientationY = orient_top;

	// Create a list component for the keymap names
	mapList = windowNewList(window, windowlist_textonly, 5, 1, 0, mapListParams,
		numMapNames, &params);
	windowRegisterEventHandler(mapList, &eventHandler);
	windowComponentFocus(mapList);

	// Select the map
	selectMap(selectedMap->name);

	params.gridX += 1;
	params.padRight = 5;
	leftContainer = windowNewContainer(window, "leftContainer", &params);

	// Create labels for the current keymap
	params.gridX = 0;
	params.gridY = 0;
	params.flags |= (WINDOW_COMPFLAG_FIXEDWIDTH | WINDOW_COMPFLAG_FIXEDHEIGHT);
	windowNewTextLabel(leftContainer, _("Current:"), &params);
	params.gridY += 1;
	currentLabel = windowNewTextLabel(leftContainer, currentName, &params);

	// Create the diagram of the selected map
	params.gridX = 0;
	params.gridY += 1;
	params.gridWidth = 2;
	params.padTop = 5;
	params.orientationX = orient_center;
	params.flags &= ~(WINDOW_COMPFLAG_FIXEDWIDTH | WINDOW_COMPFLAG_FIXEDHEIGHT);
	diagContainer = constructKeyDiag(window, selectedMap, &params);  

	params.gridX = 0;
	params.gridY += 1;
	params.padBottom = 5;
	params.flags |= (WINDOW_COMPFLAG_FIXEDWIDTH | WINDOW_COMPFLAG_FIXEDHEIGHT);
	bottomContainer = windowNewContainer(window, "bottomContainer", &params);

	// Create a 'Save' button
	params.gridY = 0;
	params.gridWidth = 1;
	params.padTop = 0;
	params.padBottom = 0;
	params.padLeft = 0;
	params.padRight = 5;
	params.orientationX = orient_right;
	saveButton = windowNewButton(bottomContainer, _("Save"), NULL, &params);
	windowRegisterEventHandler(saveButton, &eventHandler);

	// Create a 'Set as default' button
	params.gridX += 1;
	params.padLeft = 0;
	params.padRight = 0;
	params.orientationX = orient_center;
	defaultButton =
	windowNewButton(bottomContainer, _("Set as default"), NULL, &params);
	windowRegisterEventHandler(defaultButton, &eventHandler);

	// Create a 'Close' button
	params.gridX += 1;
	params.padLeft = 5;
	params.orientationX = orient_left;
	closeButton = windowNewButton(bottomContainer, _("Close"), NULL, &params);
	windowRegisterEventHandler(closeButton, &eventHandler);

	// Register an event handler to catch window close events
	windowRegisterEventHandler(window, &eventHandler);

	windowSetVisible(window, 1);

	return;
}


static void usage(char *name)
{
	printf("%s", _("usage:\n"));
	printf(_("%s [-T] [-s file_name] [map_name]\n"), name);
	return;
}


int main(int argc, char *argv[])
{
	int status = 0;  
	char *language = "";
	char *mapName = NULL;  
	char *saveName = NULL;
	char *dirName = NULL;
	char opt;
	int count;

	#ifdef BUILDLANG
		language=BUILDLANG;
	#endif
	setlocale(LC_ALL, language);
	textdomain("keymap");

	// Graphics enabled?
	graphics = graphicsAreEnabled();

	// Check for options
	while (strchr("s:T?", (opt = getopt(argc, argv, "s:T"))))
	{
		switch (opt)
		{
			case 's':
				// Save the map to a file
				if (!optarg)
				{
					fprintf(stderr, "%s", _("Missing filename argument for -s "
						"option\n"));
					usage(argv[0]);
					return (status = ERR_NULLPARAMETER);
				}
				saveName = optarg;
				break;

			case 'T':
				// Force text mode
				graphics = 0;
				break;

			case ':':
				fprintf(stderr, _("Missing parameter for %s option\n"),
					argv[optind - 1]);
				usage(argv[0]);
				return (status = ERR_NULLPARAMETER);

			case '?':
				fprintf(stderr, _("Unknown option '%c'\n"), optopt);
				usage(argv[0]);
				return (status = ERR_INVALID);
		}
	}

	cwd = malloc(MAX_PATH_LENGTH);
	selectedMap = malloc(sizeof(keyMap));
	if ((cwd == NULL) || (selectedMap == NULL))
	{
		status = ERR_MEMORY;
		goto out;
	}

	strncpy(cwd, KEYMAP_DIR, MAX_PATH_LENGTH);

	// Get the current map
	status = keyboardGetMap(selectedMap);
	if (status < 0)
		goto out;

	strncpy(currentName, selectedMap->name, KEYMAP_NAMELEN);
	mapName = selectedMap->name;

	// Did the user supply either a map name or a key map file name?
	if ((argc > 1) && (optind < argc))
	{
		// Is it a file name?
		status = fileFind(argv[optind], NULL);
		if (status >= 0)
		{
			status = readMap(argv[optind], selectedMap);
			if (status < 0)
				goto out;

			mapName = selectedMap->name;

			dirName = dirname(argv[optind]);
			if (dirName)
			{
				strncpy(cwd, dirName, MAX_PATH_LENGTH);
				free(dirName);
			}
		}
		else
			// Assume we've been given a map name.
			mapName = argv[optind];

		if (!graphics && !saveName)
		{
			// The user wants to set the current keyboard map to the supplied
			// name.
			status = setMap(mapName);
			goto out;
		}

		// Load the supplied map name
		status = loadMap(mapName);
		if (status < 0)
			goto out;
	}

	keyArray = malloc(KEYSCAN_CODES * sizeof(scanKey));
	if (keyArray == NULL)
	{
		status = ERR_MEMORY;
		goto out;
	}

	// Make the initial key array based on the current map.
	makeKeyArray(selectedMap);

	if (saveName)
	{
		// The user wants to save the current keyboard map to the supplied
		// file name.
		status = saveMap(saveName);
		goto out;
	}

	status = getMapNameParams();
	if (status < 0)
		goto out;

	if (graphics)
	{
		// Make our window
		constructWindow();

		// Run the GUI
		windowGuiRun();

		// ...and when we come back...
		windowDestroy(window);
	}
	else
	{
		// Just print the list of map names
		printf("\n");

		for (count = 0; count < numMapNames; count ++)
			printf("%s%s\n", mapListParams[count].text,
				(!strcmp(mapListParams[count].text, selectedMap->name)?
					_(" (current)") : ""));
	}

	status = 0;

out:
	if (cwd)
		free(cwd);
	if (selectedMap)
		free(selectedMap);
	if (mapListParams)
		free(mapListParams);
	if (keyArray)
		free(keyArray);

	return (status);
}
