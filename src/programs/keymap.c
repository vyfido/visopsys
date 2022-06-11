//
//  Visopsys
//  Copyright (C) 1998-2015 J. Andrew McLaughlin
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
  keymap [-T] [-p] [-s file_name] [keymap_name]

The keymap program can be used to view the available keyboard mapping, or
set the current map.  It works in both text and graphics modes:

A particular keymap can be selected by supplying a file name for keymap_name,
or else using its descriptive name (with double quotes (") around it if it
contains space characters).

If no keymap is specified on the command line, the current default one will
be selected.

In text mode:

  The -p option will print a detailed listing of the selected keymap.

  The -s option will save the selected keymap using the supplied file name.

  If a keymap is specified without the -p or -s options, then the keymap will
  be set as the current default.

  With no options, all available mappings are listed, with the current default
  indicated.

In graphics mode, the program is interactive and the user can select and
manipulate keymaps visually.

Options:
-p  : Print a detailed listing of the keymap (text mode).
-s  : Save the specified keymap to the supplied file name (text mode).
-T  : Force text mode operation

</help>
*/

#include <ctype.h>
#include <libgen.h>
#include <libintl.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/api.h>
#include <sys/ascii.h>
#include <sys/env.h>
#include <sys/font.h>
#include <sys/keyboard.h>
#include <sys/paths.h>

#define _(string) gettext(string)

#define WINDOW_TITLE		_("Keyboard Map")
#define CURRENT				_("Current:")
#define NAME				_("Name:")
#define SAVE				_("Save")
#define SET_DEFAULT			_("Set as default")
#define CLOSE				_("Close")
#define KERNEL_CONF			PATH_SYSTEM_CONFIG "/kernel.conf"

typedef struct {
	int scanCode;
	unsigned char regMap;
	unsigned char shiftMap;
	unsigned char controlMap;
	unsigned char altGrMap;
	objectKey button;
	int buttonRow;
	int buttonColumn;
	int show;
	int grey;

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
static objectKey currentNameLabel = NULL;
static objectKey nameLabel = NULL;
static objectKey nameField = NULL;
static objectKey diagContainer = NULL;
static objectKey saveButton = NULL;
static objectKey defaultButton = NULL;
static objectKey closeButton = NULL;

// A map of "universal" values.  -1 means unspecified.
static keyMap univMap = {
	KEYMAP_MAGIC,
	"Universal",
	// Regular map
	{ 0, 0, 0, ASCII_SPACE,										// 00-03
	  0, 0, 0, 0,												// 04-07
	  ASCII_CRSRLEFT, ASCII_CRSRDOWN, ASCII_CRSRRIGHT, 0,		// 08-0B
	  ASCII_DEL, ASCII_ENTER, 0, -1,							// 0C-0F
	  -1, -1, -1, -1,											// 10-13
	  -1, -1, -1, -1,											// 14-17
	  -1, -1, 0, ASCII_CRSRUP,									// 18-1B
	  0, ASCII_CRSRDOWN, ASCII_PAGEDOWN, 0,						// 1C-1F
	  -1, -1, -1, -1,											// 20-23
	  -1, -1, -1, -1,											// 24-27
	  -1, -1, -1, -1,											// 28-2B
	  ASCII_CRSRLEFT, 0, ASCII_CRSRRIGHT, '+',					// 2C-2F
	  ASCII_TAB, -1, -1, -1,									// 30-33
	  -1, -1, -1, -1,											// 34-37
	  -1, -1, -1, -1,											// 38-3B
	  -1, -1, ASCII_DEL, 0,										// 3C-3F
	  ASCII_PAGEDOWN, ASCII_HOME, ASCII_CRSRUP, ASCII_PAGEUP,	// 40-43
	  -1, -1, -1, -1,											// 44-47
	  -1, -1, -1, -1, -1, -1, -1, -1,							// 48-4F
	  -1, ASCII_BACKSPACE, 0, ASCII_HOME,						// 50-53
	  ASCII_PAGEUP, 0, '/', '*',								// 54-57
	  '-', ASCII_ESC, 0, 0, 0, 0, 0, 0,							// 58-5F
	  0, 0, 0, 0,												// 60-63
	  0, 0, 0, 0, 0												// 64-68
	},
	// Shift map
	{ 0, 0, 0, ASCII_SPACE,										// 00-03
	  0, 0, 0, 0,												// 04-07
	  ASCII_CRSRLEFT, ASCII_CRSRDOWN, ASCII_CRSRRIGHT, 0,		// 08-0B
	  ASCII_DEL, ASCII_ENTER, 0, -1,							// 0C-0F
	  -1, -1, -1, -1,											// 10-13
	  -1, -1, -1, -1,											// 14-17
	  -1, -1, 0, ASCII_CRSRUP,									// 18-1B
	  0, ASCII_CRSRDOWN, ASCII_PAGEDOWN, 0,						// 1C-1F
	  -1, -1, -1, -1,											// 20-23
	  -1, -1, -1, -1,											// 24-27
	  -1, -1, -1, -1,											// 28-2B
	  ASCII_CRSRLEFT, 0, ASCII_CRSRRIGHT, '+',					// 2C-2F
	  ASCII_TAB, -1, -1, -1,									// 30-33
	  -1, -1, -1, -1,											// 34-37
	  -1, -1, -1, -1,											// 38-3B
	  -1, -1, ASCII_DEL, 0,										// 3C-3F
	  ASCII_PAGEDOWN, ASCII_HOME, ASCII_CRSRUP, ASCII_PAGEUP,	// 40-43
	  -1, -1, -1, -1,											// 44-47
	  -1, -1, -1, -1, -1, -1, -1, -1,							// 48-4F
	  -1, ASCII_BACKSPACE, 0, ASCII_HOME,						// 50-53
	  ASCII_PAGEUP, 0, '/', '*',								// 54-57
	  '-', ASCII_ESC, 0, 0, 0, 0, 0, 0,							// 58-5F
	  0, 0, 0, 0,												// 60-63
	  0, 0, 0, 0, 0												// 64-68
	},
	// Control map
	{ 0, 0, 0, ASCII_SPACE,										// 00-03
	  0, 0, 0, 0,												// 04-07
	  ASCII_CRSRLEFT, ASCII_CRSRDOWN, ASCII_CRSRRIGHT, 0,		// 08-0B
	  ASCII_DEL, ASCII_ENTER, 0, -1,							// 0C-0F
	  ASCII_SUB, ASCII_CAN, ASCII_ETX, ASCII_SYN,				// 10-13
	  ASCII_STX, ASCII_SHIFTOUT, ASCII_ENTER, 0,				// 14-17
	  -1, -1, 0, ASCII_CRSRUP,									// 18-1B
	  0, ASCII_CRSRDOWN, ASCII_PAGEDOWN, 0,						// 1C-1F
	  ASCII_SOH, ASCII_CRSRRIGHT, ASCII_ENDOFFILE, ASCII_ACK,	// 20-23
	  ASCII_BEL, ASCII_BACKSPACE, ASCII_ENTER, ASCII_PAGEUP,	// 24-27
	  ASCII_PAGEDOWN, -1, -1, -1,								// 28-2B
	  ASCII_CRSRLEFT, 0, ASCII_CRSRRIGHT, '+',					// 2C-2F
	  ASCII_TAB, ASCII_CRSRUP, ASCII_ETB, ASCII_ENQ,			// 30-33
	  ASCII_CRSRLEFT, ASCII_CRSRDOWN, ASCII_EOM, ASCII_NAK,		// 34-37
	  ASCII_TAB, ASCII_SHIFTIN, ASCII_DLE, -1,					// 38-3B
	  -1, -1, ASCII_DEL, 0,										// 3C-3F
	  ASCII_PAGEDOWN, ASCII_HOME, ASCII_CRSRUP, ASCII_PAGEUP,	// 40-43
	  -1, -1, -1, -1,											// 44-47
	  -1, -1, -1, -1, -1, -1, -1, -1,							// 48-4F
	  -1, ASCII_BACKSPACE, 0, ASCII_HOME,						// 50-53
	  ASCII_PAGEUP, 0, '/', '*',								// 54-57
	  '-', ASCII_ESC, 0, 0, 0, 0, 0, 0,							// 58-5F
	  0, 0, 0, 0,												// 60-63
	  0, 0, 0, 0, 0												// 64-68
	},
	// Alt-Gr map
	{ 0, 0, 0, ASCII_SPACE,										// 00-03
	  0, 0, 0, 0,												// 04-07
	  ASCII_CRSRLEFT, ASCII_CRSRDOWN, ASCII_CRSRRIGHT, 0,		// 08-0B
	  ASCII_DEL, ASCII_ENTER, 0, -1,							// 0C-0F
	  -1, -1, -1, -1,											// 10-13
	  -1, -1, -1, -1,											// 14-17
	  -1, -1, 0, ASCII_CRSRUP,									// 18-1B
	  0, ASCII_CRSRDOWN, ASCII_PAGEDOWN, 0,						// 1C-1F
	  -1, -1, -1, -1,											// 20-23
	  -1, -1, -1, -1,											// 24-27
	  -1, -1, -1, -1,											// 28-2B
	  ASCII_CRSRLEFT, 0, ASCII_CRSRRIGHT, '+',					// 2C-2F
	  ASCII_TAB, -1, -1, -1,									// 30-33
	  -1, -1, -1, -1,											// 34-37
	  -1, -1, -1, -1,											// 38-3B
	  -1, -1, ASCII_DEL, 0,										// 3C-3F
	  ASCII_PAGEDOWN, ASCII_HOME, ASCII_CRSRUP, ASCII_PAGEUP,	// 40-43
	  -1, -1, -1, -1,											// 44-47
	  -1, -1, -1, -1, -1, -1, -1, -1,							// 48-4F
	  -1, ASCII_BACKSPACE, 0, ASCII_HOME,						// 50-53
	  ASCII_PAGEUP, 0, '/', '*',								// 54-57
	  '-', ASCII_ESC, 0, 0, 0, 0, 0, 0,							// 58-5F
	  0, 0, 0, 0,												// 60-63
	  0, 0, 0, 0, 0												// 64-68
	}
};

static const char *scan2String[KEYBOARD_SCAN_CODES] = {
	"LCtrl", "A0", "LAlt", "SpaceBar",							// 00-03
	"A2", "A3", "A4", "RCtrl",									// 04-07
	"LeftArrow", "DownArrow", "RightArrow", "Zero",				// 08-0B
	"Period", "Enter", "LShift", "B0",							// 0C-0F
	"B1", "B2", "B3", "B4",										// 10-13
	"B5", "B6", "B7", "B8",										// 14-17
	"B9", "B10", "RShift", "UpArrow",							// 18-1B
	"One", "Two", "Three", "CapsLock",							// 1C-1F
	"C1", "C2", "C3", "C4",										// 20-23
	"C5", "C6", "C7", "C8",										// 24-27
	"C9", "C10", "C11", "C12",									// 28-2B
	"Four", "Five", "Six", "Plus",								// 2C-2F
	"Tab", "D1", "D2", "D3",									// 30-33
	"D4", "D5", "D6", "D7",										// 34-37
	"D8", "D9", "D10", "D11",									// 38-3B
	"D12", "D13", "Del", "End",									// 3C-3F
	"PgDn", "Seven", "Eight", "Nine",							// 40-43
	"E0", "E1", "E2", "E3",										// 44-47
	"E4", "E5", "E6", "E7", "E8", "E9", "E10", "E11",			// 48-4F
	"E12", "BackSpace", "Ins", "Home",							// 50-53
	"PgUp", "NLck", "Slash", "Asterisk",						// 54-57
	"Minus", "Esc", "F1", "F2", "F3", "F4", "F5", "F6",			// 58-5F
	"F7", "F8", "F9", "F10",									// 60-63
	"F11", "F12", "Print", "SLck", "Pause"						// 64-68
};


static void usage(char *name)
{
	printf("%s", _("usage:\n"));
	printf(_("%s [-T] [-p] [-s file_name] [map_name]\n"), name);
	return;
}


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

	memset(&theStream, 0, sizeof(fileStream));

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


static int findMapFile(const char *mapName, char *fileName)
{
	// Look in the current directory for the keymap file with the supplied map
	// name

	int status = 0;
	file theFile;
	keyMap *map = NULL;
	int count;

	map = malloc(sizeof(keyMap));
	if (!map)
		return (status = ERR_MEMORY);

	memset(&theFile, 0, sizeof(file));

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


static int setMap(const char *mapName)
{
	// Change the current mapping in the kernel, and also change the config for
	// persistence at the next reboot

	int status = 0;
	char *fileName = NULL;
	disk confDisk;

	fileName = malloc(MAX_PATH_NAME_LENGTH);
	if (!fileName)
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
	memset(&confDisk, 0, sizeof(disk));
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


static int loadMap(const char *mapName)
{
	int status = 0;
	char *fileName = NULL;

	fileName = malloc(MAX_PATH_NAME_LENGTH);
	if (!fileName)
		return (status = ERR_MEMORY);

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


static void makeKeyArray(keyMap *map)
{
	int count;

	for (count = 0; count < KEYBOARD_SCAN_CODES; count ++)
	{
		keyArray[count].scanCode = count;
		keyArray[count].regMap = map->regMap[count];
		keyArray[count].shiftMap = map->shiftMap[count];
		keyArray[count].controlMap = map->controlMap[count];
		keyArray[count].altGrMap = map->altGrMap[count];
	}

	return;
}


static int saveMap(const char *fileName)
{
	int status = 0;
	disk mapDisk;
	fileStream theStream;
	int count;

	memset(&mapDisk, 0, sizeof(disk));
	memset(&theStream, 0, sizeof(fileStream));

	// Find out whether the file is on a read-only filesystem
	if (!fileGetDisk(fileName, &mapDisk) && mapDisk.readOnly)
	{
		error(_("Can't write %s:\nFilesystem is read-only"), fileName);
		return (status = ERR_NOWRITE);
	}

	if (graphics && nameField)
		// Get the map name
		windowComponentGetData(nameField, selectedMap->name, 32);

	for (count = 0; count < KEYBOARD_SCAN_CODES; count ++)
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
	map = malloc(sizeof(keyMap));
	if (!fileName || !map)
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
		{
			snprintf(fileName, MAX_PATH_NAME_LENGTH, "%s/%s", cwd,
				theFile.name);
		}
		else
		{
			snprintf(fileName, MAX_PATH_NAME_LENGTH, "/%s", theFile.name);
		}

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
	if (!nameBuffer)
		return (status = ERR_MEMORY);

	status = getMapNames(nameBuffer);
	if (status < 0)
		goto out;

	if (mapListParams)
		free(mapListParams);

	mapListParams = malloc(numMapNames * sizeof(listItemParameters));
	if (!mapListParams)
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


static void getText(unsigned char ascii, char *output)
{
	// Convert an ASCII code into a printable character in the output.

	if (ascii >= 33)
		sprintf(output, "%c", ascii);
	else
		strcat(output, " ");
}


static void makeButtonString(scanKey *scan, char *string)
{
	string[0] = '\0';

	switch (scan->scanCode)
	{
		case keyBackSpace:
			strcpy(string, _("Backspace"));
			return;

		case keyTab:
			strcpy(string, _("Tab"));
			return;

		case keyCapsLock:
			strcpy(string, _("CapsLock"));
			return;

		case keyEnter:
			strcpy(string, _("Enter"));
			return;

		case keyLShift:
		case keyRShift:
			strcpy(string, _("Shift"));
			return;
	}

	// 'Normal' key
	getText(scan->regMap, string);

	if (scan->shiftMap && (scan->shiftMap != scan->regMap))
		getText(scan->shiftMap, (string + strlen(string)));

	if (scan->altGrMap && (scan->altGrMap != scan->regMap))
		getText(scan->altGrMap, (string + strlen(string)));
}


static void updateKeyDiag(keyMap *map)
{
	char string[32];
	int count;

	windowComponentSetData(nameField, map->name, sizeof(map->name),
		1 /* redraw */);

	for (count = 0; count < 53; count ++)
	{
		if (keyArray[count].button)
		{
			makeButtonString(&keyArray[count], string);
			windowComponentSetData(keyArray[count].button, string,
				sizeof(string), 1 /* redraw */);
		}
	}
}


static void refreshWindow(void)
{
	// We got a 'window refresh' event (probably because of a language switch),
	// so we need to update things

	// Re-get the language setting
	setlocale(LC_ALL, getenv(ENV_LANG));
	textdomain("keymap");

	// Refresh the keyboard diagram
	updateKeyDiag(selectedMap);

	// Refresh the 'current' label
	windowComponentSetData(currentLabel, CURRENT, strlen(CURRENT),
		1 /* redraw */);

	// Refresh the 'name' field
	windowComponentSetData(nameLabel, NAME, strlen(NAME), 1 /* redraw */);

	// Refresh the 'save' button
	windowComponentSetData(saveButton, SAVE, strlen(SAVE), 1 /* redraw */);

	// Refresh the 'set as default' button
	windowComponentSetData(defaultButton, SET_DEFAULT, strlen(SET_DEFAULT),
		1 /* redraw */);

	// Refresh the 'close' button
	windowComponentSetData(closeButton, CLOSE, strlen(CLOSE), 1 /* redraw */);

	// Refresh the window title
	windowSetTitle(window, WINDOW_TITLE);
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

	// Check for window events.
	if (key == window)
	{
		// Check for window refresh
		if (event->type == EVENT_WINDOW_REFRESH)
			refreshWindow();

		// Check for the window being closed
		else if (event->type == EVENT_WINDOW_CLOSE)
			windowGuiStop();
	}

	else if ((key == mapList) && (event->type & EVENT_SELECTION) &&
		(event->type & EVENT_MOUSE_DOWN))
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
		if (!fullName)
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

		windowComponentSetData(mapList, mapListParams, numMapNames,
			1 /* redraw */);

		selectMap(selectedMap->name);

		windowNewInfoDialog(window, _("Saved"), _("Map saved"));
	}

	else if ((key == defaultButton) && (event->type == EVENT_MOUSE_LEFTUP))
	{
		if (windowComponentGetSelected(mapList, &selected) < 0)
			return;
		if (setMap(mapListParams[selected].text) < 0)
			return;
		windowComponentSetData(currentNameLabel, mapListParams[selected].text,
			strlen(mapListParams[selected].text), 1 /* redraw */);
	}

	// Check for the window being closed by a GUI event.
	else if ((key == closeButton) && (event->type == EVENT_MOUSE_LEFTUP))
		windowGuiStop();
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
	if (!dialogWindow)
		return (status = ERR_NOCREATE);

	memset(&params, 0, sizeof(componentParameters));
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
	windowComponentSetData(regField, string, 10, 1 /* redraw */);

	params.gridX = 0;
	params.gridY += 1;
	windowNewTextLabel(dialogWindow, _("Shift code:"), &params);

	params.gridX += 1;
	shiftField = windowNewTextField(dialogWindow, 10, &params);
	snprintf(string, 10, "%d", scan->shiftMap);
	windowComponentSetData(shiftField, string, 10, 1 /* redraw */);

	params.gridX = 0;
	params.gridY += 1;
	windowNewTextLabel(dialogWindow, _("Alt Gr code:"), &params);

	params.gridX += 1;
	altGrField = windowNewTextField(dialogWindow, 10, &params);
	snprintf(string, 10, "%d", scan->altGrMap);
	windowComponentSetData(altGrField, string, 10, 1 /* redraw */);

	params.gridX = 0;
	params.gridY += 1;
	windowNewTextLabel(dialogWindow, _("Ctrl code:"), &params);

	params.gridX += 1;
	ctrlField = windowNewTextField(dialogWindow, 10, &params);
	snprintf(string, 10, "%d", scan->controlMap);
	windowComponentSetData(ctrlField, string, 10, 1 /* redraw */);

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

	while (1)
	{
		// Check for the OK button, or 'enter' in any of the text fields
		if (((windowComponentEventGet(_okButton, &event) > 0) &&
			(event.type == EVENT_MOUSE_LEFTUP)) ||
			((windowComponentEventGet(regField, &event) > 0) &&
				(event.type == EVENT_KEY_DOWN) && (event.key == keyEnter)) ||
			((windowComponentEventGet(shiftField, &event) > 0) &&
				(event.type == EVENT_KEY_DOWN) && (event.key == keyEnter)) ||
			((windowComponentEventGet(altGrField, &event) > 0) &&
				(event.type == EVENT_KEY_DOWN) && (event.key == keyEnter)) ||
			((windowComponentEventGet(ctrlField, &event) > 0) &&
				(event.type == EVENT_KEY_DOWN) && (event.key == keyEnter)))
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
		for (count = 0; count < KEYBOARD_SCAN_CODES; count ++)
		{
			if (key == keyArray[count].button)
			{
				changeKeyDialog(&keyArray[count]);
				makeButtonString(&keyArray[count], string);
				windowComponentSetData(keyArray[count].button, string,
					sizeof(string), 1 /* redraw */);
				break;
			}
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
	if (!mainContainer)
		return (mainContainer);

	memset(&params, 0, sizeof(componentParameters));
	params.gridWidth = 1;
	params.gridHeight = 1;
	params.padTop = 5;
	params.padLeft = 5;
	params.padRight = 5;
	params.orientationX = orient_left;
	params.orientationY = orient_middle;
	params.flags |= WINDOW_COMPFLAG_FIXEDWIDTH;

	if (fileFind(PATH_SYSTEM_FONTS "/xterm-normal-10.vbf", NULL) >= 0)
		fontLoadSystem("xterm-normal-10.vbf", "xterm-normal-10",
			&(params.font), 1);

	// Make a container for the name field
	nameContainer = windowNewContainer(mainContainer, "nameContainer", &params);

	// The name label and field
	params.padLeft = 0;
	nameLabel = windowNewTextLabel(nameContainer, NAME, &params);
	params.gridX += 1;
	nameField = windowNewTextField(nameContainer, 30, &params);
	windowComponentSetData(nameField, map->name, MAX_PATH_LENGTH,
		1 /* redraw */);

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

	// 2nd row
	for (count = keyE0; count <= keyBackSpace; count ++)
	{
		keyArray[count].buttonColumn = (count - keyE0);
		keyArray[count].buttonRow = 0;
		keyArray[count].show = 1;
	}

	// 3rd row
	for (count = keyTab; count <= keyD13; count ++)
	{
		keyArray[count].buttonColumn = (count - keyTab);
		keyArray[count].buttonRow = 1;
		keyArray[count].show = 1;
	}

	// 4th row
	for (count = keyCapsLock; count <= keyC12; count ++)
	{
		keyArray[count].buttonColumn = (count - keyCapsLock);
		keyArray[count].buttonRow = 2;
		keyArray[count].show = 1;
	}

	keyArray[keyEnter].buttonColumn = (count - keyCapsLock);
	keyArray[keyEnter].buttonRow = 2;
	keyArray[keyEnter].show = 1;

	// Fourth row
	for (count = keyLShift; count <= keyRShift; count ++)
	{
		keyArray[count].show = 1;
		keyArray[count].buttonColumn = (count - keyLShift);
		keyArray[count].buttonRow = 3;
	}

	for (count = 0; count < KEYBOARD_SCAN_CODES; count ++)
		if (univMap.regMap[count] != (unsigned char) -1)
			keyArray[count].grey = 1;

	params.padTop = 0;
	params.padBottom = 0;
	params.padLeft = 0;
	params.padRight = 0;

	// Now put the buttons in their containers
	for (count = 0; count < KEYBOARD_SCAN_CODES; count ++)
	{
		if (keyArray[count].show)
		{
			makeButtonString(&keyArray[count], string);
			params.gridX = keyArray[count].buttonColumn;
			keyArray[count].button =
				windowNewButton(rowContainer[keyArray[count].buttonRow],
					string, NULL, &params);

			windowRegisterEventHandler(keyArray[count].button,
				&editKeyHandler);

			if (fontGetPrintedWidth(params.font, string) <
				fontGetPrintedWidth(params.font, "@@@"))
			{
				windowComponentSetWidth(keyArray[count].button,
					fontGetPrintedWidth(params.font, "@@@"));
			}

			if (keyArray[count].grey)
				windowComponentSetEnabled(keyArray[count].button, 0);
		}
	}

	return (mainContainer);
}


static void constructWindow(void)
{
	// If we are in graphics mode, make a window rather than operating on the
	// command line.

	objectKey rightContainer = NULL;
	objectKey bottomContainer = NULL;
	componentParameters params;

	// Create a new window
	window = windowNew(multitaskerGetCurrentProcessId(), WINDOW_TITLE);
	if (!window)
		return;

	memset(&params, 0, sizeof(componentParameters));
	params.gridWidth = 1;
	params.gridHeight = 1;
	params.padTop = 5;
	params.padLeft = 5;
	params.orientationX = orient_left;
	params.orientationY = orient_top;

	// Create a list component for the keymap names
	mapList = windowNewList(window, windowlist_textonly, 5, 1, 0,
		mapListParams, numMapNames, &params);
	windowRegisterEventHandler(mapList, &eventHandler);
	windowComponentFocus(mapList);

	// Select the map
	selectMap(selectedMap->name);

	// Make a container for the current keymap labels
	params.gridX += 1;
	params.padRight = 5;
	rightContainer = windowNewContainer(window, "rightContainer", &params);

	// Create labels for the current keymap
	params.gridX = 0;
	params.gridY = 0;
	params.flags |= (WINDOW_COMPFLAG_FIXEDWIDTH | WINDOW_COMPFLAG_FIXEDHEIGHT);
	currentLabel = windowNewTextLabel(rightContainer, CURRENT, &params);
	params.gridY += 1;
	currentNameLabel = windowNewTextLabel(rightContainer, currentName,
		&params);

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
	saveButton = windowNewButton(bottomContainer, SAVE, NULL, &params);
	windowRegisterEventHandler(saveButton, &eventHandler);

	// Create a 'Set as default' button
	params.gridX += 1;
	params.padLeft = 0;
	params.padRight = 0;
	params.orientationX = orient_center;
	defaultButton =	windowNewButton(bottomContainer, SET_DEFAULT, NULL,
		&params);
	windowRegisterEventHandler(defaultButton, &eventHandler);

	// Create a 'Close' button
	params.gridX += 1;
	params.padLeft = 5;
	params.orientationX = orient_left;
	closeButton = windowNewButton(bottomContainer, CLOSE, NULL, &params);
	windowRegisterEventHandler(closeButton, &eventHandler);

	// Register an event handler to catch window close events
	windowRegisterEventHandler(window, &eventHandler);

	windowSetVisible(window, 1);

	return;
}


static void printRow(int start, int end, unsigned char *map)
{
	int printed = 0;
	int count;

	printf("  ");
	for (count = start; count <= end; count ++)
	{
		printf("%s=", scan2String[count]);
		if (isgraph(map[count]))
			printf("'%c' ", map[count]);
		else
			printf("%x ", map[count]);

		// Only print 8 on a line
		if (printed && !(printed % 8))
		{
			printed = 0;
			printf("\n  ");
		}
		else
		{
			printed += 1;
		}
	}
	printf("\n");
}


static void printMap(unsigned char *map)
{
	printf("%s\n", _("1st row"));
	printRow(keyEsc, keyPause, map);
	printf("%s\n", _("2nd row"));
	printRow(keyE0, keyMinus, map);
	printf("%s\n", _("3rd row"));
	printRow(keyTab, keyNine, map);
	printf("%s\n", _("4th row"));
	printRow(keyCapsLock, keyPlus, map);
	printf("%s\n", _("5th row"));
	printRow(keyLShift, keyThree, map);
	printf("%s\n", _("6th row"));
	printRow(keyLCtrl, keyEnter, map);
	printf("\n");
}


static void printKeyboard(void)
{
	// Print out the detail of the selected keymap
	printf(_("\nPrinting out keymap \"%s\"\n\n"), selectedMap->name);
	printf("-- %s --\n", _("Regular map"));
	printMap(selectedMap->regMap);
	printf("-- %s --\n", _("Shift map"));
	printMap(selectedMap->shiftMap);
	printf("-- %s --\n", _("Ctrl map"));
	printMap(selectedMap->controlMap);
	printf("-- %s --\n", _("AltGr map"));
	printMap(selectedMap->altGrMap);
}


int main(int argc, char *argv[])
{
	int status = 0;
	int print = 0;
	char *mapName = NULL;
	char *saveName = NULL;
	char *dirName = NULL;
	char opt;
	int count;

	setlocale(LC_ALL, getenv(ENV_LANG));
	textdomain("keymap");

	// Graphics enabled?
	graphics = graphicsAreEnabled();

	// Check options
	while (strchr("psT:?", (opt = getopt(argc, argv, "ps:T"))))
	{
		switch (opt)
		{
			case 'p':
				// Just print out the map, if we're in text mode
				print = 1;
				break;

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

			default:
				fprintf(stderr, _("Unknown option '%c'\n"), optopt);
				usage(argv[0]);
				return (status = ERR_INVALID);
		}
	}

	cwd = malloc(MAX_PATH_LENGTH);
	selectedMap = malloc(sizeof(keyMap));
	if (!cwd || !selectedMap)
	{
		status = ERR_MEMORY;
		goto out;
	}

	strncpy(cwd, PATH_SYSTEM_KEYMAPS, MAX_PATH_LENGTH);

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
		{
			// Assume we've been given a map name.
			mapName = argv[optind];
		}

		if (!graphics && !saveName && !print)
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

	keyArray = malloc(KEYBOARD_SCAN_CODES * sizeof(scanKey));
	if (!keyArray)
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
		if (print)
		{
			// Print out the whole keyboard for the selected map
			printKeyboard();
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

