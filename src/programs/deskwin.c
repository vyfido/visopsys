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
//  deskwin.c
//

// This is the code for the default userspace desktop window shell in the GUI
// environment.

/* This is the text that appears when a user requests help about this program
<help>

 -- deskwin --

A basic desktop window 'shell' environment based on the standard 'desktop'
metaphor.

Usage:
  deskwin

(Only available in graphics mode)

This command will open a new 'root' window with a desktop, icons, taskbar,
etc.  It will only work properly when launched automatically by a user login.

</help>
*/

#include <errno.h>
#include <libintl.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/api.h>
#include <sys/color.h>
#include <sys/deskconf.h>
#include <sys/env.h>
#include <sys/file.h>
#include <sys/font.h>
#include <sys/image.h>
#include <sys/user.h>
#include <sys/vis.h>
#include <sys/winconf.h>
#include <sys/window.h>

#define _(string) gettext(string)

#define WINDOW_TITLE		_("Shell window")
#define MAX_WINMENUITEMS	64


// Debugging messages are off by default even in a debug build
#undef DEBUG

#if defined(DEBUG)
#define debug(message, arg...) do { \
		printf("DEBUG %s:%s(%d): ", __FILE__, __FUNCTION__, __LINE__); \
		printf(message, ##arg); \
		printf("\n"); \
	} while (0)
#else
	#define debug(message, arg...) do { } while (0)
#endif // defined(DEBUG)

#define error(message, arg...) do { \
		printf("Error: %s:%s(%d): ", __FILE__, __FUNCTION__, __LINE__); \
		printf(message, ##arg); \
		printf("\n"); \
	} while (0)

static struct {
	struct {
		color foreground;
		color background;
		color desktop;
	} color;

	struct {
		objectKey defaultFont;
		struct {
			struct {
				char family[FONT_FAMILY_LEN + 1];
				unsigned flags;
				int points;
				objectKey font;
			} small;
		} fixWidth;
		struct {
			struct {
				char family[FONT_FAMILY_LEN + 1];
				unsigned flags;
				int points;
				objectKey font;
			} medium;
		} varWidth;
	} font;

} windowVariables;

typedef struct {
	objectKey itemComponent;
	char command[MAX_PATH_NAME_LENGTH + 1];
	objectKey window;

} menuItemData;

typedef struct {
	objectKey iconComponent;
	char command[MAX_PATH_NAME_LENGTH + 1];

} desktopIconData;

typedef struct {
	objectKey component;
	int processId;

} menuBarComponent;

static struct {
	char userName[USER_MAX_NAMELENGTH + 1];
	int privilege;
	int processId;
	int guiThreadPid;
	objectKey rootWindow;
	objectKey menuBar;
	componentParameters menuBarParams;
	linkedList menusList;
	objectKey windowMenu;
	linkedList menuItemsList;
	linkedList winMenuItemsList;
	linkedList menuBarCompsList;
	linkedList iconsList;

} shellData;

static variableList windowSettings;
static variableList desktopSettings;


static int readFileConfig(const char *fileName, variableList *settings)
{
	// Return a (possibly empty) variable list, filled with any desktop
	// settings we read from various config files

	int status = 0;

	debug("Read configuration %s", fileName);

	status = fileFind(fileName, NULL);
	if (status < 0)
		return (status);

	status = configRead(fileName, settings);

	return (status);
}


static int readConfig(const char *configName, variableList *settings)
{
	// Return a (possibly empty) variable list, filled with any desktop
	// settings we read from various config files

	int status = 0;
	char fileName[MAX_PATH_NAME_LENGTH];
	variableList userConfig;
	const char *variable = NULL;
	const char *value = NULL;
	char language[LOCALE_MAX_NAMELEN + 1];
	variableList langConfig;
	int count;

	debug("Read configuration");

	memset(&userConfig, 0, sizeof(variableList));
	memset(&langConfig, 0, sizeof(variableList));

	variableListClear(settings);

	// First try to read the system config

	sprintf(fileName, "%s/%s", PATH_SYSTEM_CONFIG, configName);

	status = readFileConfig(fileName, settings);
	if (status < 0)
	{
		// Argh.  No file?  Create an empty list for us to use.
		status = variableListCreate(settings);
		if (status < 0)
			return (status);
	}

	if (strcmp((char *) shellData.userName, USER_ADMIN))
	{
		// Try to read any user-specific desktop config

		sprintf(fileName, PATH_USERS_CONFIG "/%s", shellData.userName,
			configName);

		status = readFileConfig(fileName, &userConfig);
		if (status >= 0)
		{
			// We got one.  Override values.
			for (count = 0; count < userConfig.numVariables; count ++)
			{
				variable = variableListGetVariable(&userConfig, count);
				if (variable)
				{
					value = variableListGet(&userConfig, variable);
					if (value)
						variableListSet(settings, variable, value);
				}
			}
		}
	}

	// If the 'LANG' environment variable is set, see whether there's another
	// language-specific desktop config file that matches it
	status = environmentGet(ENV_LANG, language, LOCALE_MAX_NAMELEN);
	if (status >= 0)
	{
		sprintf(fileName, "%s/%s/%s", PATH_SYSTEM_CONFIG, language,
			configName);

		status = fileFind(fileName, NULL);
		if (status >= 0)
		{
			status = configRead(fileName, &langConfig);
			if (status >= 0)
			{
				// We got one.  Override values.
				for (count = 0; count < langConfig.numVariables; count ++)
				{
					variable = variableListGetVariable(&langConfig, count);
					if (variable)
					{
						value = variableListGet(&langConfig, variable);
						if (value)
							variableListSet(settings, variable, value);
					}
				}
			}
		}
	}

	return (status = 0);
}


static int setWindowVariables(void)
{
	int status = 0;
	const char *value = NULL;

	if ((value = variableListGet(&windowSettings, WINVAR_COLOR_FG_RED)) &&
		(atoi(value) >= 0))
	{
		windowVariables.color.foreground.red = atoi(value);
	}
	if ((value = variableListGet(&windowSettings, WINVAR_COLOR_FG_GREEN)) &&
		(atoi(value) >= 0))
	{
		windowVariables.color.foreground.green = atoi(value);
	}
	if ((value = variableListGet(&windowSettings, WINVAR_COLOR_FG_BLUE)) &&
		(atoi(value) >= 0))
	{
		windowVariables.color.foreground.blue = atoi(value);
	}
	if ((value = variableListGet(&windowSettings, WINVAR_COLOR_BG_RED)) &&
		(atoi(value) >= 0))
	{
		windowVariables.color.background.red = atoi(value);
	}
	if ((value = variableListGet(&windowSettings, WINVAR_COLOR_BG_GREEN)) &&
		(atoi(value) >= 0))
	{
		windowVariables.color.background.green = atoi(value);
	}
	if ((value = variableListGet(&windowSettings, WINVAR_COLOR_BG_BLUE)) &&
		(atoi(value) >= 0))
	{
		windowVariables.color.background.blue = atoi(value);
	}
	if ((value = variableListGet(&windowSettings, WINVAR_COLOR_DT_RED)) &&
		(atoi(value) >= 0))
	{
		windowVariables.color.desktop.red = atoi(value);
	}
	if ((value = variableListGet(&windowSettings, WINVAR_COLOR_DT_GREEN)) &&
		(atoi(value) >= 0))
	{
		windowVariables.color.desktop.green = atoi(value);
	}
	if ((value = variableListGet(&windowSettings, WINVAR_COLOR_DT_BLUE)) &&
		(atoi(value) >= 0))
	{
		windowVariables.color.desktop.blue = atoi(value);
	}
	if ((value = variableListGet(&windowSettings,
		WINVAR_FONT_FIXW_SM_FAMILY)))
	{
		strncpy(windowVariables.font.fixWidth.small.family, value,
			FONT_FAMILY_LEN);
	}
	windowVariables.font.fixWidth.small.flags = FONT_STYLEFLAG_FIXED;
	if ((value = variableListGet(&windowSettings, WINVAR_FONT_FIXW_SM_FLAGS)))
	{
		if (!strncmp(value, WINVAR_FONT_FLAG_BOLD, 128))
			windowVariables.font.fixWidth.small.flags |= FONT_STYLEFLAG_BOLD;
	}
	if ((value = variableListGet(&windowSettings,
		WINVAR_FONT_FIXW_SM_POINTS)) && (atoi(value) >= 0))
	{
		windowVariables.font.fixWidth.small.points = atoi(value);
	}
	if ((value = variableListGet(&windowSettings,
		WINVAR_FONT_VARW_MD_FAMILY)))
	{
		strncpy(windowVariables.font.varWidth.medium.family, value,
			FONT_FAMILY_LEN);
	}
	if ((value = variableListGet(&windowSettings, WINVAR_FONT_VARW_MD_FLAGS)))
	{
		if (!strncmp(value, WINVAR_FONT_FLAG_BOLD, 128))
			windowVariables.font.varWidth.medium.flags |= FONT_STYLEFLAG_BOLD;
	}
	if ((value = variableListGet(&windowSettings,
		WINVAR_FONT_VARW_MD_POINTS)) && (atoi(value) >= 0))
	{
		windowVariables.font.varWidth.medium.points = atoi(value);
	}

	return (status = 0);
}


static int getFonts(void)
{
	int status = 0;

	windowVariables.font.fixWidth.small.font =
		fontGet(windowVariables.font.fixWidth.small.family,
			windowVariables.font.fixWidth.small.flags,
			windowVariables.font.fixWidth.small.points, NULL);

	if (!windowVariables.font.fixWidth.small.font)
	{
		// Try the built-in system font
		windowVariables.font.fixWidth.small.font = fontGetSystem();
		if (!windowVariables.font.fixWidth.small.font)
		{
			// This would be sort of serious
			return (status = ERR_NODATA);
		}
	}

	// Use this as our default
	windowVariables.font.defaultFont =
		windowVariables.font.fixWidth.small.font;

	windowVariables.font.varWidth.medium.font =
		fontGet(windowVariables.font.varWidth.medium.family,
			windowVariables.font.varWidth.medium.flags,
			windowVariables.font.varWidth.medium.points, NULL);
	if (!windowVariables.font.varWidth.medium.font)
	{
		windowVariables.font.varWidth.medium.font =
			windowVariables.font.defaultFont;
	}

	return (status = 0);
}


static void windowMenuEvent(objectKey component, windowEvent *event)
{
	menuItemData *itemData = NULL;
	linkedListItem *iter = NULL;
	windowInfo info;

	debug("Taskbar window menu event");

	if (shellData.windowMenu && (event->type & WINDOW_EVENT_SELECTION))
	{
		debug("Taskbar window menu selection");

		itemData = linkedListIterStart(&shellData.winMenuItemsList, &iter);

		while (itemData)
		{
			if (component == itemData->itemComponent)
			{
				if (windowGetInfo(itemData->window, &info) >= 0)
				{
					// Restore it
					debug("Restore window %s", info.title);

					windowSetMinimized(itemData->window, 0);

					// If it has a dialog box, restore that too
					if (info.dialogWindow)
						windowSetMinimized(info.dialogWindow, 0);
				}

				break;
			}

			itemData = linkedListIterNext(&shellData.winMenuItemsList, &iter);
		}
	}
}


static void updateList(void)
{
	// When the list of open windows has changed, this function will update
	// our window menu

	objectKey windowList[MAX_WINMENUITEMS];
	windowInfo info;
	menuItemData *itemData = NULL;
	linkedListItem *iter = NULL;
	componentParameters params;
	objectKey listWindow = NULL;
	int count;

	debug("Update window list");

	memset(windowList, 0, sizeof(windowList));
	memset(&info, 0, sizeof(windowInfo));

	if (shellData.windowMenu)
	{
		// Destroy all the menu items in the menu

		itemData = linkedListIterStart(&shellData.winMenuItemsList, &iter);

		while (itemData)
		{
			windowClearEventHandler(itemData->itemComponent);
			windowComponentDestroy(itemData->itemComponent);
			free(itemData);

			itemData = linkedListIterNext(&shellData.winMenuItemsList, &iter);
		}

		linkedListClear(&shellData.winMenuItemsList);

		// Copy the parameters from the menu to use
		memcpy(&params, (void *) &shellData.menuBarParams,
			sizeof(componentParameters));

		// Get the list of windows
		if (windowGetList(windowList, MAX_WINMENUITEMS) < 0)
			return;

		for (count = 0; count < MAX_WINMENUITEMS; count ++)
		{
			listWindow = windowList[count];
			if (!listWindow)
				break;

			// Get info about the window
			if (windowGetInfo(listWindow, &info) < 0)
				continue;

			// Skip windows we don't want to include
			if (
				// Skip the root window
			 	(listWindow == shellData.rootWindow) ||
				// Skip any iconified windows
				(info.flags & WINDOW_FLAG_ICONIFIED) ||
				// Skip child windows too
				info.parentWindow
			)
			{
				continue;
			}

			itemData = malloc(sizeof(menuItemData));
			if (!itemData)
				return;

			itemData->itemComponent = windowNewMenuItem(shellData.windowMenu,
				info.title, &params);
			itemData->window = listWindow;

			linkedListAddBack(&shellData.winMenuItemsList, itemData);

			windowRegisterEventHandler(itemData->itemComponent,
				&windowMenuEvent);
		}
	}
}


static void destroy(void)
{
	desktopIconData *iconData = NULL;
	menuItemData *itemData = NULL;
	linkedListItem *iter = NULL;
	objectKey menu = NULL;

	// Destroy icons

	iconData = linkedListIterStart(&shellData.iconsList, &iter);

	while (iconData)
	{
		windowClearEventHandler(iconData->iconComponent);
		windowComponentDestroy(iconData->iconComponent);
		free(iconData);

		iconData = linkedListIterNext(&shellData.iconsList, &iter);
	}

	linkedListClear(&shellData.iconsList);

	// Don't destroy menu bar components, since they're 'owned' by other
	// processes

	// Deallocate (static) menu item data structures

	itemData = linkedListIterStart(&shellData.menuItemsList, &iter);

	while (itemData)
	{
		windowClearEventHandler(itemData->itemComponent);
		free(itemData);

		itemData = linkedListIterNext(&shellData.menuItemsList, &iter);
	}

	linkedListClear(&shellData.menuItemsList);

	// Deallocate window menu item data structures

	itemData = linkedListIterStart(&shellData.winMenuItemsList, &iter);

	while (itemData)
	{
		windowClearEventHandler(itemData->itemComponent);
		free(itemData);

		itemData = linkedListIterNext(&shellData.winMenuItemsList, &iter);
	}

	linkedListClear(&shellData.winMenuItemsList);

	// Do this before destroying menus (because menus are windows, and
	// updateList() will get called)
	shellData.windowMenu = NULL;

	// Destroy menus

	menu = linkedListIterStart(&shellData.menusList, &iter);

	while (menu)
	{
		windowMenuDestroy(menu);
		menu = linkedListIterNext(&shellData.menusList, &iter);
	}

	linkedListClear(&shellData.menusList);

	// Don't destroy the menu bar
}


static void menuEvent(objectKey component, windowEvent *event)
{
	int status = 0;
	menuItemData *itemData = NULL;
	linkedListItem *iter = NULL;

	debug("Taskbar menu event");

	if (event->type & WINDOW_EVENT_SELECTION)
	{
		debug("Taskbar menu selection");

		itemData = linkedListIterStart(&shellData.menuItemsList, &iter);

		while (itemData)
		{
			if (component == itemData->itemComponent)
			{
				if (itemData->command[0])
				{
					windowSwitchPointer(shellData.rootWindow,
						MOUSE_POINTER_BUSY);

					// Run the command, no block
					status = loaderLoadAndExec(itemData->command,
						shellData.privilege, 0 /* no block */);

					windowSwitchPointer(shellData.rootWindow,
						MOUSE_POINTER_DEFAULT);

					if (status < 0)
					{
						error("%s %s", _("Unable to execute program"),
							itemData->command);
					}
				}

				break;
			}

			itemData = linkedListIterNext(&shellData.menuItemsList, &iter);
		}
	}
}


static int makeMenuBar(void)
{
	// Make or re-create a menu bar at the top

	int status = 0;
	const char *variable = NULL;
	const char *value = NULL;
	const char *menuName = NULL;
	const char *menuLabel = NULL;
	objectKey menu = NULL;
	char propertyName[128];
	const char *itemName = NULL;
	const char *itemLabel = NULL;
	menuItemData *itemData = NULL;
	componentParameters params;
	int count1, count2;

	debug("Make menu bar");

	memset(&params, 0, sizeof(componentParameters));

	memcpy(&params.foreground, &COLOR_WHITE, sizeof(color));
	memcpy(&params.background, &windowVariables.color.foreground,
		sizeof(color));
	params.flags |= (COMP_PARAMS_FLAG_CUSTOMFOREGROUND |
		COMP_PARAMS_FLAG_CUSTOMBACKGROUND);
	params.font = windowVariables.font.varWidth.medium.font;

	// Make a copy of the parameters we're using to create the menu bar
	memcpy(&shellData.menuBarParams, &params, sizeof(componentParameters));

	if (!shellData.menuBar)
		shellData.menuBar = windowNewMenuBar(shellData.rootWindow, &params);

	// Try to load menu bar menus and menu items

	// Loop for variables starting with DESKTOP_TASKBAR_MENU
	for (count1 = 0; count1 < desktopSettings.numVariables; count1 ++)
	{
		variable = variableListGetVariable(&desktopSettings, count1);
		if (variable && !strncmp(variable, DESKVAR_TASKBAR_MENU,
			strlen(DESKVAR_TASKBAR_MENU)))
		{
			menuName = (variable + strlen(DESKVAR_TASKBAR_MENU));
			menuLabel = variableListGet(&desktopSettings, variable);

			menu = windowNewMenu(shellData.rootWindow, shellData.menuBar,
				menuLabel, NULL, &params);

			// Add it to our list
			status = linkedListAddBack(&shellData.menusList, (void *) menu);
			if (status < 0)
			{
				windowMenuDestroy(menu);
				return (status = ERR_MEMORY);
			}

			// Now loop and get any components for this menu
			for (count2 = 0; count2 < desktopSettings.numVariables; count2 ++)
			{
				sprintf(propertyName, DESKVAR_TASKBAR_MENUITEM, menuName);

				variable = variableListGetVariable(&desktopSettings, count2);
				if (!strncmp(variable, propertyName, strlen(propertyName)))
				{
					itemName = (variable + strlen(propertyName));
					itemLabel = variableListGet(&desktopSettings, variable);

					if (!itemLabel)
						continue;

					// See if there's an associated command
					sprintf(propertyName, DESKVAR_TASKBAR_MENUITEM_CMD,
						menuName, itemName);
					value = variableListGet(&desktopSettings, propertyName);
					if (!value || (loaderCheckCommand(value) < 0))
					{
						// No such command.  Don't show this one.
						continue;
					}

					// Get memory for menu item data
					itemData = calloc(1, sizeof(menuItemData));
					if (!itemData)
						continue;

					// Create the menu item
					itemData->itemComponent = windowNewMenuItem(menu,
						itemLabel, &params);
					if (!itemData->itemComponent)
					{
						free(itemData);
						continue;
					}

					strncpy(itemData->command, value, MAX_PATH_NAME_LENGTH);

					// Add it to our list
					status = linkedListAddBack(&shellData.menuItemsList,
						itemData);
					if (status < 0)
					{
						windowComponentDestroy(itemData->itemComponent);
						free(itemData);
						return (status);
					}

					windowRegisterEventHandler(itemData->itemComponent,
						&menuEvent);
				}
			}

			// We treat any 'window' menu specially, since it is not usually
			// populated at startup time, only as windows are created or
			// destroyed
			if (!strcmp(menuName, DESKVAR_TASKBAR_WINDOWMENU))
			{
				debug("Created window menu");
				shellData.windowMenu = menu;
			}
		}
	}

	return (status = 0);
}


static void iconEvent(objectKey component, windowEvent *event)
{
	int status = 0;
	desktopIconData *iconData = NULL;
	linkedListItem *iter = NULL;
	static int dragging = 0;

	if (event->type & WINDOW_EVENT_MOUSE_DRAG)
	{
		// We want to ignore icon dragging here
		dragging = 1;
	}

	else if (event->type & WINDOW_EVENT_MOUSE_LEFTUP)
	{
		if (dragging)
		{
			// Drag is finished
			dragging = 0;
			return;
		}

		debug("Icon mouse click");

		iconData = linkedListIterStart(&shellData.iconsList, &iter);

		while (iconData)
		{
			if (component == iconData->iconComponent)
			{
				if (iconData->command[0])
				{
					windowSwitchPointer(shellData.rootWindow,
						MOUSE_POINTER_BUSY);

					// Run the command, no block
					status = loaderLoadAndExec(iconData->command,
						shellData.privilege, 0 /* no block */);

					windowSwitchPointer(shellData.rootWindow,
						MOUSE_POINTER_DEFAULT);

					if (status < 0)
					{
						error("%s %s", _("Unable to execute program"),
							iconData->command);
					}
				}

				break;
			}

			iconData = linkedListIterNext(&shellData.iconsList, &iter);
		}
	}
}


static int makeIcons(void)
{
	// Try to load icons

	int status = 0;
	const char *variable = NULL;
	const char *iconName = NULL;
	const char *iconLabel = NULL;
	char propertyName[128];
	const char *command = NULL;
	const char *imageFile = NULL;
	desktopIconData *iconData = NULL;
	image tmpImage;
	componentParameters params;
	int count;

	debug("Make icons");

	// These parameters are the same for all icons
	memset(&params, 0, sizeof(componentParameters));
	params.gridWidth = 1;
	params.gridHeight = 1;
	params.padLeft = 5;
	params.padRight = 5;
	params.padTop = 5;
	params.flags = (COMP_PARAMS_FLAG_CANDRAG |
		COMP_PARAMS_FLAG_CUSTOMFOREGROUND |
		COMP_PARAMS_FLAG_CUSTOMBACKGROUND | COMP_PARAMS_FLAG_CANFOCUS |
		COMP_PARAMS_FLAG_FIXEDWIDTH | COMP_PARAMS_FLAG_FIXEDHEIGHT);
	memcpy(&params.foreground, &COLOR_WHITE, sizeof(color));
	memcpy(&params.background, &windowVariables.color.desktop, sizeof(color));
	params.orientationX = orient_center;
	params.orientationY = orient_middle;

	// Loop for variables starting with DESKTOP_ICON_NAME
	for (count = 0; count < desktopSettings.numVariables; count ++)
	{
		variable = variableListGetVariable(&desktopSettings, count);
		if (variable && !strncmp(variable, DESKVAR_ICON_NAME,
			strlen(DESKVAR_ICON_NAME)))
		{
			iconName = (variable + strlen(DESKVAR_ICON_NAME));
			iconLabel = variableListGet(&desktopSettings, variable);

			// Get the rest of the recognized properties for this icon

			// See if there's a command associated with this, and make sure
			// the program exists
			sprintf(propertyName, DESKVAR_ICON_COMMAND, iconName);
			command = variableListGet(&desktopSettings, propertyName);
			if (!command || (loaderCheckCommand(command) < 0))
			{
				// No such command.  Don't show this one.
				continue;
			}

			// Get the image name, make sure it exists, and try to load it
			sprintf(propertyName, DESKVAR_ICON_IMAGE, iconName);
			imageFile = variableListGet(&desktopSettings, propertyName);
			if (!imageFile || (fileFind(imageFile, NULL) < 0) ||
				(imageLoad(imageFile, 64, 64, &tmpImage) < 0))
			{
				// No such image.  Don't show this one.
				continue;
			}

			// Get memory for icon data
			iconData = calloc(1, sizeof(desktopIconData));
			if (!iconData)
				return (status = ERR_MEMORY);

			params.gridY++;
			iconData->iconComponent = windowNewIcon(shellData.rootWindow,
				&tmpImage, iconLabel, &params);

			// Release the image memory
			imageFree(&tmpImage);

			if (!iconData->iconComponent)
			{
				free(iconData);
				continue;
			}

			// Set the command
			strncpy(iconData->command, command, MAX_PATH_NAME_LENGTH);

			// Add this icon to our list
			status = linkedListAddBack(&shellData.iconsList, iconData);
			if (status < 0)
			{
				windowComponentDestroy(iconData->iconComponent);
				free(iconData);
				return (status);
			}

			// Register the event handler for the icon command execution
			windowRegisterEventHandler(iconData->iconComponent, &iconEvent);
		}
	}

	// Snap the icons to a grid
	windowSnapIcons((objectKey) shellData.rootWindow);

	return (status = 0);
}


static void refresh(void)
{
	// Refresh the desktop environment

	debug("Refresh");

	// Re-get the language setting
	setlocale(LC_ALL, getenv(ENV_LANG));
	textdomain("deskwin");

	// Re-get the character set
	if (shellData.rootWindow && getenv(ENV_CHARSET))
		windowSetCharSet(shellData.rootWindow, getenv(ENV_CHARSET));

	// Re-read the desktop config file(s)
	if (readConfig(DESKTOP_CONFIG, &desktopSettings) >= 0)
	{
		// Get rid of all our existing stuff
		destroy();

		// Re-create the menu bar
		makeMenuBar();

		// Re-load the icons
		makeIcons();

		if (shellData.rootWindow)
		{
			// Re-do root window layout
			windowLayout(shellData.rootWindow);
		}
	}

	// Update the window menu
	updateList();
}


static int loadDesktopBackground(const char *imageFile)
{
	int status = 0;
	image tmpImage;

	// Try to load the background image

	debug("Loading background image \"%s\"", imageFile);

	if (strcmp(imageFile, DESKVAR_BACKGROUND_NONE))
	{
		status = fileFind(imageFile, NULL);
		if (status < 0)
		{
			error(_("Background image %s not found"), imageFile);
			return (status);
		}

		status = imageLoad(imageFile, 0, 0, &tmpImage);
		if (status < 0)
		{
			error("%s %s", _("Error loading background image"), imageFile);
			return (status);
		}

		// Put the background image into our window
		status = windowSetBackgroundImage(shellData.rootWindow, &tmpImage);

		// Release the image memory
		imageFree(&tmpImage);
	}
	else
	{
		status = windowSetBackgroundImage(shellData.rootWindow, NULL);
	}

	return (status);
}


static int changeDesktopBackground(void)
{
	int status = 0;
	const char *imageFile = NULL;

	debug("Change desktop background");

	// Re-read the desktop config file(s)
	status = readConfig(DESKTOP_CONFIG, &desktopSettings);
	if (status < 0)
		return (status);

	imageFile = variableListGet(&desktopSettings, DESKVAR_BACKGROUND_IMAGE);
	if (!imageFile)
		return (status = ERR_NODATA);

	return (status = loadDesktopBackground(imageFile));
}


static int addMenuBarComponent(objectKey component, int processId)
{
	int status = 0;
	menuBarComponent *menuBarComp = NULL;

	debug("Add menu bar component");

	// Add the component to the shell's list of menu bar components

	menuBarComp = malloc(sizeof(menuBarComponent));
	if (!menuBarComp)
		return (status = ERR_MEMORY);

	menuBarComp->component = component;
	menuBarComp->processId = processId;

	status = linkedListAddBack(&shellData.menuBarCompsList, menuBarComp);
	if (status < 0)
	{
		free(menuBarComp);
		return (status);
	}

	return (status = 0);
}


static void removeMenuBarComponent(objectKey component)
{
	// Remove the component to the shell's list of menu bar components

	menuBarComponent *menuBarComp = NULL;
	linkedListItem *iter = NULL;

	debug("Remove menu bar component");

	menuBarComp = linkedListIterStart(&shellData.menuBarCompsList, &iter);

	while (menuBarComp)
	{
		if (menuBarComp->component == component)
		{
			linkedListRemove(&shellData.menuBarCompsList, menuBarComp);
			free(menuBarComp);
			break;
		}

		menuBarComp = linkedListIterNext(&shellData.menuBarCompsList, &iter);
	}
}


static void rootWindowEvent(objectKey window, windowEvent *event)
{
	if (!shellData.rootWindow || (window != shellData.rootWindow))
		return;

	debug("Root window event");

	if (event->type & WINDOW_EVENT_SHELL)
	{
		debug("Shell event");

		if (event->shell.type & WINDOW_SHELL_EVENT_WINDOWLIST)
		{
			updateList();
		}

		else if (event->shell.type & WINDOW_SHELL_EVENT_REFRESH)
		{
			refresh();
		}

		else if (event->shell.type & WINDOW_SHELL_EVENT_CHANGEBACKGRND)
		{
			changeDesktopBackground();
		}

		else if (event->shell.type & WINDOW_SHELL_EVENT_RAISEWINMENU)
		{
			windowToggleMenuBar(shellData.rootWindow);
		}

		else if (event->shell.type & WINDOW_SHELL_EVENT_NEWBARCOMP)
		{
			addMenuBarComponent(event->shell.component,
				event->shell.processId);
		}

		else if (event->shell.type & WINDOW_SHELL_EVENT_DESTROYBARCOMP)
		{
			removeMenuBarComponent(event->shell.component);
		}
	}
}


static int makeRootWindow(void)
{
	// Make a main root window to serve as the background for the window
	// environment

	int status = 0;
	const char *imageFile = NULL;

	debug("Make root window");

	// Get a new window
	shellData.rootWindow = windowNew(multitaskerGetCurrentProcessId(),
		WINDOW_TITLE);
	if (!shellData.rootWindow)
		return (status = ERR_NOCREATE);

	// Register an event handler to catch window events
	windowRegisterEventHandler(shellData.rootWindow, &rootWindowEvent);

	// The window will have no border, title bar or close button, is not
	// movable or resizable, and we mark it as a root window
	windowSetMovable(shellData.rootWindow, 0);
	windowSetResizable(shellData.rootWindow, 0);
	windowSetRoot(shellData.rootWindow);
	windowSetHasTitleBar(shellData.rootWindow, 0);
	windowSetHasBorder(shellData.rootWindow, 0);

	// Set our desktop color preference
	windowSetBackgroundColor(shellData.rootWindow,
		&windowVariables.color.desktop);

	// Try to load the background image
	imageFile = variableListGet(&desktopSettings, DESKVAR_BACKGROUND_IMAGE);
	if (imageFile)
		loadDesktopBackground(imageFile);

	// Make the top menu bar
	status = makeMenuBar();
	if (status < 0)
		return (status);

	// Make icons
	status = makeIcons();
	if (status < 0)
		return (status);

	// Location in the top corner
	status = windowSetLocation(shellData.rootWindow, 0, 0);
	if (status < 0)
		return (status);

	// Resize to the whole screen
	status = windowSetSize(shellData.rootWindow, graphicGetScreenWidth(),
		graphicGetScreenHeight());
	if (status < 0)
		return (status);

	windowSetVisible(shellData.rootWindow, 1);

	return (status = 0);
}


static void runPrograms(void)
{
	// Get any programs we're supposed to run automatically and run them

	const char *variable = NULL;
	const char *programName = NULL;
	int count;

	debug("Run programs");

	// Loop for variables starting with DESKVAR_PROGRAM
	for (count = 0; count < desktopSettings.numVariables; count ++)
	{
		variable = variableListGetVariable(&desktopSettings, count);
		if (variable && !strncmp(variable, DESKVAR_PROGRAM,
			strlen(DESKVAR_PROGRAM)))
		{
			programName = variableListGet(&desktopSettings, variable);
			if (programName)
			{
				// Try to run the program
				loaderLoadAndExec(programName, shellData.privilege, 0);
			}
		}
	}

	// If touch support is available, we will also run the virtual keyboard
	// program in 'iconified' mode
	if (touchAvailable() && (fileFind(PATH_PROGRAMS "/keyboard", NULL) >= 0))
	{
		loaderLoadAndExec(PATH_PROGRAMS "/keyboard -i", shellData.privilege,
			0 /* no block */);
	}
}


int main(int argc, char *argv[])
{
	// Launch the window shell

	int status = 0;

	setlocale(LC_ALL, getenv(ENV_LANG));
	textdomain("deskwin");

	// Only work in graphics mode
	if (!graphicsAreEnabled())
	{
		fprintf(stderr, _("\nThe \"%s\" command only works in graphics "
			"mode\n"), (argc? argv[0] : ""));
		return (status = ERR_NOTINITIALIZED);
	}

	memset(&windowVariables, 0, sizeof(windowVariables));
	memset(&shellData, 0, sizeof(shellData));
	memset(&windowSettings, 0, sizeof(variableList));
	memset(&desktopSettings, 0, sizeof(variableList));

	// Get the current user
	status = userGetCurrent(shellData.userName, USER_MAX_NAMELENGTH);
	if (status < 0)
		return (status);

	// Get the current privilege
	shellData.privilege = userGetPrivilege(shellData.userName);
	if (shellData.privilege < 0)
		return (status = shellData.privilege);

	// Get the current process ID
	shellData.processId = multitaskerGetCurrentProcessId();
	if (shellData.processId < 0)
		return (status = shellData.processId);

	// Read the window config file(s)
	status = readConfig(WINDOW_CONFIG, &windowSettings);
	if (status < 0)
		goto out;

	// Cache them in variables
	status = setWindowVariables();
	if (status < 0)
		goto out;

	// Get the font objects
	status = getFonts();
	if (status < 0)
		goto out;

	// Read the desktop config file(s)
	status = readConfig(DESKTOP_CONFIG, &desktopSettings);
	if (status < 0)
		goto out;

	// Create the root window
	status = makeRootWindow();
	if (status < 0)
		goto out;

	// Run any programs that we're supposed to run after login
	runPrograms();

	// Run the GUI as a separate thread
	shellData.guiThreadPid = windowGuiThread();
	if (shellData.guiThreadPid < 0)
	{
		status = shellData.guiThreadPid;
		goto out;
	}

	// Main loop
	while (1)
	{
		multitaskerWait(2000);

		// Make sure the GUI thread is running
		if (!multitaskerProcessIsAlive(shellData.guiThreadPid))
		{
			shellData.guiThreadPid = windowGuiThread();
			if (shellData.guiThreadPid < 0)
			{
				status = shellData.guiThreadPid;
				goto out;
			}
		}
	}

	windowGuiStop();

	status = 0;

out:
	if (shellData.rootWindow)
		windowDestroy(shellData.rootWindow);

	variableListDestroy(&desktopSettings);
	variableListDestroy(&windowSettings);

	return (status);
}

