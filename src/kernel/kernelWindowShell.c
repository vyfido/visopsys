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
//  kernelWindowShell.c
//

// This is the code that manages the 'root' window in the GUI environment.

#include "kernelWindow.h"
#include "kernelDebug.h"
#include "kernelDisk.h"
#include "kernelEnvironment.h"
#include "kernelError.h"
#include "kernelImage.h"
#include "kernelLoader.h"
#include "kernelLog.h"
#include "kernelMalloc.h"
#include "kernelMemory.h"
#include "kernelMisc.h"
#include "kernelMultitasker.h"
#include "kernelParameters.h"
#include "kernelUser.h"
#include "kernelWindowEventStream.h"
#include <locale.h>
#include <stdio.h>
#include <string.h>

typedef struct {
	kernelWindowComponent *itemComponent;
	char command[MAX_PATH_NAME_LENGTH];
	kernelWindow *window;

} menuItem;

static volatile struct {
	char userName[USER_MAX_NAMELENGTH + 1];
	int privilege;
	int processId;
	kernelWindow *rootWindow;
	kernelWindowComponent *menuBar;
	kernelWindow **menus;
	int numMenus;
	kernelWindow *windowMenu;
	menuItem *menuItems;
	int numMenuItems;
	menuItem *winMenuItems;
	int numWinMenuItems;
	kernelWindowComponent **icons;
	int numIcons;
	kernelWindow **windowList;
	int numberWindows;
	int refresh;

} shellData;

extern kernelWindowVariables *windowVariables;


static void menuEvent(kernelWindowComponent *component, windowEvent *event)
{
	int status = 0;
	int count;

	kernelDebug(debug_gui, "WindowShell taskbar menu event");

	if (event->type & EVENT_SELECTION)
	{
		kernelDebug(debug_gui, "WindowShell taskbar menu selection");

		for (count = 0; count < shellData.numMenuItems; count ++)
		{
			if (component == shellData.menuItems[count].itemComponent)
			{
				if (shellData.menuItems[count].command[0])
				{
					kernelWindowSwitchPointer(shellData.rootWindow, "busy");

					// Run the command, no block
					status = kernelLoaderLoadAndExec(
						shellData.menuItems[count].command,
						shellData.privilege, 0);

					kernelWindowSwitchPointer(shellData.rootWindow, "default");

					if (status < 0)
						kernelError(kernel_error, "Unable to execute program "
							"%s", shellData.menuItems[count].command);
				}

				break;
			}
		}
	}

	return;
}


static void iconEvent(kernelWindowComponent *component, windowEvent *event)
{
	int status = 0;
	kernelWindowIcon *iconComponent = component->data;
	static int dragging = 0;

	if (event->type & EVENT_MOUSE_DRAG)
		dragging = 1;

	else if (event->type & EVENT_MOUSE_LEFTUP)
	{
		if (dragging)
		{
			// Drag is finished
			dragging = 0;
			return;
		}

		kernelDebug(debug_gui, "WindowShell icon mouse click");

		kernelWindowSwitchPointer(shellData.rootWindow, "busy");

		// Run the command
		status = kernelLoaderLoadAndExec((const char *) iconComponent->command,
			shellData.privilege, 0 /* no block */);

		kernelWindowSwitchPointer(shellData.rootWindow, "default");

		if (status < 0)
			kernelError(kernel_error, "Unable to execute program %s",
				iconComponent->command);
	}

	return;
}


static int readFileConfig(const char *fileName, variableList *settings)
{
	// Return a (possibly empty) variable list, filled with any desktop
	// settings we read from various config files.

	int status = 0;

	kernelDebug(debug_gui, "WindowShell read configuration %s", fileName);

	status = kernelFileFind(fileName, NULL);
	if (status < 0)
		return (status);

	status = kernelConfigRead(fileName, settings);

	return (status);
}


static int readConfig(variableList *settings)
{
	// Return a (possibly empty) variable list, filled with any desktop
	// settings we read from various config files.

	int status = 0;
	char fileName[MAX_PATH_NAME_LENGTH];
	const char *variable = NULL;
	const char *value = NULL;
	char language[LOCALE_MAX_NAMELEN + 1];
	variableList langConfig;
	int count;

	kernelDebug(debug_gui, "WindowShell read configuration");

	memset(&langConfig, 0, sizeof(variableList));

	// First try to read the system desktop config.
	status = readFileConfig(PATH_SYSTEM_CONFIG "/" WINDOW_DESKTOP_CONFIG,
		settings);
	if (status < 0)
	{
		// Argh.  No file?  Create an empty list for us to use
		status = kernelVariableListCreate(settings);
		if (status < 0)
			return (status);
	}

	// If the 'LANG' environment variable is set, see whether there's another
	// language-specific desktop config file that matches it.
	status = kernelEnvironmentGet(ENV_LANG, language, LOCALE_MAX_NAMELEN);
	if (status >= 0)
	{
		sprintf(fileName, "%s/%s/desktop.conf", PATH_SYSTEM_CONFIG, language);

		status = kernelFileFind(fileName, NULL);
		if (status >= 0)
		{
			status = kernelConfigRead(fileName, &langConfig);
			if (status >= 0)
			{
				// We got one.  Override values.
				for (count = 0; count < langConfig.numVariables; count ++)
				{
					variable = kernelVariableListGetVariable(&langConfig,
						count);
					if (variable)
					{
						value = kernelVariableListGet(&langConfig, variable);
						if (value)
							kernelVariableListSet(settings, variable, value);
					}
				}
			}
		}
	}

	return (status = 0);
}


static int makeMenuBar(variableList *settings)
{
	// Make a menu bar at the top

	int status = 0;
	const char *variable = NULL;
	const char *value = NULL;
	const char *menuName = NULL;
	const char *menuLabel = NULL;
	kernelWindow *menu = NULL;
	char propertyName[128];
	const char *itemName = NULL;
	const char *itemLabel = NULL;
	kernelWindowComponent *item = NULL;
	componentParameters params;
	int count1, count2;

	kernelDebug(debug_gui, "WindowShell make menu bar");

	memset(&params, 0, sizeof(componentParameters));
	params.foreground.red = 255;
	params.foreground.green = 255;
	params.foreground.blue = 255;
	params.background.red = windowVariables->color.foreground.red;
	params.background.green = windowVariables->color.foreground.green;
	params.background.blue = windowVariables->color.foreground.blue;
	params.flags |= (WINDOW_COMPFLAG_CUSTOMFOREGROUND |
		WINDOW_COMPFLAG_CUSTOMBACKGROUND);
	params.font = windowVariables->font.varWidth.medium.font;
	shellData.menuBar = kernelWindowNewMenuBar(shellData.rootWindow, &params);

	// Try to load menu bar menus and menu items

	// Loop for variables with "taskBar.menu.*"
	for (count1 = 0; count1 < settings->numVariables; count1 ++)
	{
		variable = kernelVariableListGetVariable(settings, count1);
		if (variable && !strncmp(variable, "taskBar.menu.", 13))
		{
			menuName = (variable + 13);
			menuLabel = kernelVariableListGet(settings, variable);

			menu = kernelWindowNewMenu(shellData.rootWindow, shellData.menuBar,
				menuLabel, NULL, &params);

			// Add it to our list
			shellData.menus = kernelRealloc(shellData.menus,
				((shellData.numMenus + 1) * sizeof(kernelWindow *)));
			if (!shellData.menus)
				return (status = ERR_MEMORY);

			shellData.menus[shellData.numMenus++] = menu;

			// Now loop and get any components for this menu
			for (count2 = 0; count2 < settings->numVariables; count2 ++)
			{
				sprintf(propertyName, "taskBar.%s.item.", menuName);

				variable = kernelVariableListGetVariable(settings, count2);
				if (!strncmp(variable, propertyName, strlen(propertyName)))
				{
					itemName = (variable + strlen(propertyName));
					itemLabel = kernelVariableListGet(settings, variable);

					if (!itemLabel)
						continue;

					// See if there's an associated command
					sprintf(propertyName, "taskBar.%s.%s.command", menuName,
						itemName);
					value = kernelVariableListGet(settings, propertyName);
					if (!value || (kernelLoaderCheckCommand(value) < 0))
						// No such command.  Don't show this one.
						continue;

					// Create the menu item
					item = kernelWindowNewMenuItem(menu, itemLabel,	&params);
					if (!item)
						continue;

					// Add it to our list
					shellData.menuItems = kernelRealloc(shellData.menuItems,
						((shellData.numMenuItems + 1) * sizeof(menuItem)));
					if (!shellData.menuItems)
						return (status = ERR_MEMORY);

					shellData.menuItems[shellData.numMenuItems].itemComponent =
						item;
					strncpy(shellData.menuItems[shellData.numMenuItems].
						command, value, MAX_PATH_NAME_LENGTH);

					shellData.numMenuItems += 1;

					kernelWindowRegisterEventHandler(item, &menuEvent);
				}
			}

			// We treat any "window" menu specially, since it is not usually
			// populated at startup time, only as windows are created or
			// destroyed.
			if (!strcmp(menuName, "window"))
			{
				kernelDebug(debug_gui, "WindowShell created window menu");
				shellData.windowMenu = menu;
			}
		}
	}

	kernelLog("Task menu initialized");
	return (status = 0);
}


static int makeIcons(variableList *settings)
{
	// Try to load icons

	int status = 0;
	const char *variable = NULL;
	const char *iconName = NULL;
	const char *iconLabel = NULL;
	char propertyName[128];
	const char *command = NULL;
	const char *imageFile = NULL;
	kernelWindowComponent *iconComponent = NULL;
	image tmpImage;
	componentParameters params;
	int count;

	kernelDebug(debug_gui, "WindowShell make icons");

	// These parameters are the same for all icons
	memset(&params, 0, sizeof(componentParameters));
	params.gridWidth = 1;
	params.gridHeight = 1;
	params.padLeft = 5;
	params.padRight = 5;
	params.padTop = 5;
	params.padBottom = 0;
	params.flags = (WINDOW_COMPFLAG_CUSTOMFOREGROUND |
		WINDOW_COMPFLAG_CUSTOMBACKGROUND | WINDOW_COMPFLAG_CANFOCUS);
	memcpy(&params.foreground, &COLOR_WHITE, sizeof(color));
	memcpy(&params.background, &windowVariables->color.desktop, sizeof(color));
	params.orientationX = orient_center;
	params.orientationY = orient_middle;

	// Loop for variables with "icon.name.*"
	for (count = 0; count < settings->numVariables; count ++)
	{
		variable = kernelVariableListGetVariable(settings, count);
		if (variable && !strncmp(variable, "icon.name.", 10))
		{
			iconName = (variable + 10);
			iconLabel = kernelVariableListGet(settings, variable);

			// Get the rest of the recognized properties for this icon.

			// See if there's a command associated with this, and make sure
			// the program exists.
			sprintf(propertyName, "icon.%s.command", iconName);
			command = kernelVariableListGet(settings, propertyName);
			if (!command || (kernelLoaderCheckCommand(command) < 0))
				continue;

			// Get the image name, make sure it exists, and try to load it.
			sprintf(propertyName, "icon.%s.image", iconName);
			imageFile = kernelVariableListGet(settings, propertyName);
			if (!imageFile || (kernelFileFind(imageFile, NULL) < 0) ||
				(kernelImageLoad(imageFile, 0, 0, &tmpImage) < 0))
			{
				continue;
			}

			params.gridY++;
			iconComponent = kernelWindowNewIcon(shellData.rootWindow,
				&tmpImage, iconLabel, &params);

			// Release the image memory
			kernelImageFree(&tmpImage);

			if (!iconComponent)
				continue;

			// Set the command
			strncpy((char *)((kernelWindowIcon *) iconComponent->data)->
				command, command, MAX_PATH_NAME_LENGTH);

			// Add this icon to our list
			shellData.icons = kernelRealloc((void *) shellData.icons,
				((shellData.numIcons + 1) * sizeof(kernelWindowComponent *)));
			if (!shellData.icons)
				return (status = ERR_MEMORY);

			shellData.icons[shellData.numIcons++] = iconComponent;

			// Register the event handler for the icon command execution
			kernelWindowRegisterEventHandler(iconComponent,	&iconEvent);
		}
	}

	// Snap the icons to a grid
	kernelWindowSnapIcons((objectKey) shellData.rootWindow);

	kernelLog("Desktop icons loaded");
	return (status = 0);
}


static int makeRootWindow(void)
{
	// Make a main root window to serve as the background for the window
	// environment

	int status = 0;
	variableList settings;
	const char *imageFile = NULL;
	image tmpImage;

	kernelDebug(debug_gui, "WindowShell make root window");

	// Get a new window
	shellData.rootWindow = kernelWindowNew(KERNELPROCID, WINNAME_ROOTWINDOW);
	if (!shellData.rootWindow)
		return (status = ERR_NOCREATE);

	// The window will have no border, title bar or close button, is not
	// movable or resizable, and we mark it as a root window
	shellData.rootWindow->flags &= ~(WINFLAG_MOVABLE | WINFLAG_RESIZABLE);
	shellData.rootWindow->flags |= WINFLAG_ROOTWINDOW;
	kernelWindowSetHasTitleBar(shellData.rootWindow, 0);
	kernelWindowSetHasBorder(shellData.rootWindow, 0);

	// Location in the top corner
	status = kernelWindowSetLocation(shellData.rootWindow, 0, 0);
	if (status < 0)
		return (status);

	// Size is the whole screen
	status = kernelWindowSetSize(shellData.rootWindow,
		kernelGraphicGetScreenWidth(), kernelGraphicGetScreenHeight());
	if (status < 0)
		return (status);

	// The window is always at the bottom level
	shellData.rootWindow->level = WINDOW_MAXWINDOWS;

	// Set our background color preference
	shellData.rootWindow->background.red =
		windowVariables->color.desktop.red;
	shellData.rootWindow->background.green =
		windowVariables->color.desktop.green;
	shellData.rootWindow->background.blue =
		windowVariables->color.desktop.blue;

	// Read the desktop config file(s)
	status = readConfig(&settings);
	if (status < 0)
		return (status);

	// Try to load the background image
	imageFile = kernelVariableListGet(&settings, "background.image");
	if (imageFile)
	{
		kernelDebug(debug_gui, "WindowShell loading background image \"%s\"",
			imageFile);

		if (strcmp(imageFile, "none"))
		{
			if ((kernelFileFind(imageFile, NULL) >= 0) &&
				(kernelImageLoad(imageFile, 0, 0, &tmpImage) >= 0))
			{
				// Put the background image into our window.
				kernelWindowSetBackgroundImage(shellData.rootWindow,
					&tmpImage);
				kernelLog("Background image loaded");
			}
			else
			{
				kernelError(kernel_error, "Error loading background image %s",
					imageFile);
			}

			// Release the image memory
			kernelImageFree(&tmpImage);
		}
	}

	// Make the top menu bar
	status = makeMenuBar(&settings);
	if (status < 0)
	{
		kernelVariableListDestroy(&settings);
		return (status);
	}

	// Make icons
	status = makeIcons(&settings);
	if (status < 0)
	{
		kernelVariableListDestroy(&settings);
		return (status);
	}

	kernelVariableListDestroy(&settings);

	kernelWindowSetVisible(shellData.rootWindow, 1);

	return (status = 0);
}


static void runPrograms(void)
{
	// Get any programs we're supposed to run automatically and run them.

	variableList settings;
	const char *variable = NULL;
	const char *programName = NULL;
	int count;

	kernelDebug(debug_gui, "WindowShell run programs");

	// Read the desktop config file(s)
	if (readConfig(&settings) < 0)
		return;

	// Loop for variables with "program.*"
	for (count = 0; count < settings.numVariables; count ++)
	{
		variable = kernelVariableListGetVariable(&settings, count);
		if (variable && !strncmp(variable, "program.", 8))
		{
			programName = kernelVariableListGet(&settings, variable);
			if (programName)
				// Try to run the program
				kernelLoaderLoadAndExec(programName, shellData.privilege, 0);
		}
	}

	kernelVariableListDestroy(&settings);
	return;
}


static void scanMenuItemEvents(menuItem *items, int numItems)
{
	// Scan through events in a list of our menu items

	kernelWindowComponent *component = NULL;
	windowEvent event;
	int count;

	for (count = 0; count < numItems; count ++)
	{
		component = items[count].itemComponent;

		// Any events pending?  Any event handler?
		if (component->eventHandler &&
			(kernelWindowEventStreamRead(&(component->events), &event) > 0))
		{
			kernelDebug(debug_gui, "WindowShell root menu item got event");
			component->eventHandler(component, &event);
		}
	}
}


static void scanContainerEvents(kernelWindowContainer *container)
{
	// Recursively scan through events in components of a container

	kernelWindowComponent *component = NULL;
	windowEvent event;
	int count;

	for (count = 0; count < container->numComponents; count ++)
	{
		component = container->components[count];

		// Any events pending?  Any event handler?
		if (component->eventHandler &&
			(kernelWindowEventStreamRead(&(component->events), &event) > 0))
		{
			kernelDebug(debug_gui, "WindowShell scan container got event");
			component->eventHandler(component, &event);
		}

		// If this component is a container type, recurse
		if (component->type == containerComponentType)
			scanContainerEvents(component->data);
	}

	return;
}


static void destroy(void)
{
	int count;

	// Destroy icons
	for (count = 0; count < shellData.numIcons; count ++)
		kernelWindowComponentDestroy(shellData.icons[count]);

	shellData.numIcons = 0;
	kernelFree(shellData.icons);
	shellData.icons = NULL;

	// Destroy (static) menu items
	for (count = 0; count < shellData.numMenuItems; count ++)
		kernelWindowComponentDestroy(shellData.menuItems[count].itemComponent);

	shellData.numMenuItems = 0;
	kernelFree(shellData.menuItems);
	shellData.menuItems = NULL;

	// Destroy window menu items
	for (count = 0; count < shellData.numWinMenuItems; count ++)
		kernelWindowComponentDestroy(
			shellData.winMenuItems[count].itemComponent);

	shellData.numWinMenuItems = 0;
	kernelFree(shellData.winMenuItems);
	shellData.winMenuItems = NULL;

	// Do this before destroying menus (because menus are windows, and
	// kernelWindowShellUpdateList() will get called)
	shellData.windowMenu = NULL;

	// Destroy menus (but not the window menu)
	for (count = 0; count < shellData.numMenus; count ++)
		kernelWindowDestroy(shellData.menus[count]);

	shellData.numMenus = 0;
	kernelFree(shellData.menus);
	shellData.menus = NULL;

	// Destroy the menu bar
	kernelWindowComponentDestroy(shellData.menuBar);
}


static void refresh(void)
{
	// Refresh the desktop environment

	variableList settings;
	windowEvent event;
	int count;

	kernelDebug(debug_gui, "WindowShell refresh");

	// Reload the user environment
	if (kernelEnvironmentLoad((char *) shellData.userName) >= 0)
		// Propagate it to all of our child processes
		kernelMultitaskerPropagateEnvironment(NULL);

	// Read the desktop config file(s)
	if (readConfig(&settings) >= 0)
	{
		// Get rid of all our existing stuff
		destroy();

		// Re-create the menu bar
		makeMenuBar(&settings);

		// Re-load the icons
		makeIcons(&settings);

		kernelVariableListDestroy(&settings);

		if (shellData.rootWindow)
		{
			// Re-do root window layout.  Don't use kernelWindowLayout() for
			// now, since it automatically re-sizes and messes things up
			kernelWindowSetVisible(shellData.rootWindow, 0);

			if (shellData.rootWindow->sysContainer &&
				shellData.rootWindow->sysContainer->layout)
			{
				shellData.rootWindow->sysContainer->
					layout(shellData.rootWindow->sysContainer);
			}

			if (shellData.rootWindow->mainContainer &&
				shellData.rootWindow->mainContainer->layout)
			{
				shellData.rootWindow->mainContainer->
					layout(shellData.rootWindow->mainContainer);
			}

			kernelWindowSetVisible(shellData.rootWindow, 1);
		}
	}

	// Send a 'window refresh' event to every window
	if (shellData.windowList)
	{
		memset(&event, 0, sizeof(windowEvent));
		event.type = EVENT_WINDOW_REFRESH;

		for (count = 0; count < shellData.numberWindows; count ++)
			kernelWindowEventStreamWrite(
				&shellData.windowList[count]->events, &event);
	}

	// Let them update
	kernelMultitaskerYield();

	// Update the window menu
	kernelWindowShellUpdateList(shellData.windowList, shellData.numberWindows);

	shellData.refresh = 0;
}


__attribute__((noreturn))
static void windowShellThread(void)
{
	// This thread runs as the 'window shell' to watch for window events on
	// 'root window' GUI components, and which functions as the user's login
	// shell in graphics mode.

	int status = 0;

	// Create the root window
	status = makeRootWindow();
	if (status < 0)
		kernelMultitaskerTerminate(status);

	// Run any programs that we're supposed to run after login
	runPrograms();

	// Now loop and process any events
	while (1)
	{
		if (shellData.refresh)
			refresh();

		if (shellData.winMenuItems)
			scanMenuItemEvents(shellData.winMenuItems,
				shellData.numWinMenuItems);

		if (shellData.menuItems)
			scanMenuItemEvents(shellData.menuItems, shellData.numMenuItems);

		scanContainerEvents(shellData.rootWindow->mainContainer->data);

		// Done
		kernelMultitaskerYield();
	}
}


static void windowMenuEvent(kernelWindowComponent *component,
	windowEvent *event)
{
	int count;

	kernelDebug(debug_gui, "WindowShell taskbar window menu event");

	if (shellData.windowMenu && (event->type & EVENT_SELECTION))
	{
		kernelDebug(debug_gui, "WindowShell taskbar window menu selection");

		// See if this is one of our window menu components
		for (count = 0; count < shellData.numWinMenuItems; count ++)
		{
			if (component == shellData.winMenuItems[count].itemComponent)
			{
				// Restore it
				kernelDebug(debug_gui, "WindowShell restore window %s",
					shellData.winMenuItems[count].window->title);
				kernelWindowSetMinimized(shellData.winMenuItems[count].window,
					0);

				// If it has a dialog box, restore that too
				if (shellData.winMenuItems[count].window->dialogWindow)
					kernelWindowSetMinimized(
						shellData.winMenuItems[count].window->dialogWindow, 0);

				break;
			}
		}
	}

	return;
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////

int kernelWindowShell(const char *user)
{
	// Launch the window shell thread

	kernelDebug(debug_gui, "WindowShell start");

	// Check params
	if (!user)
		return (shellData.processId = ERR_NULLPARAMETER);

	memset((void *) &shellData, 0, sizeof(shellData));

	memcpy((char *) shellData.userName, user, USER_MAX_NAMELENGTH);
	shellData.privilege = kernelUserGetPrivilege((char *) shellData.userName);

	// Spawn the window shell thread
	shellData.processId = kernelMultitaskerSpawn(windowShellThread,
		"window shell", 0, NULL);

	return (shellData.processId);
}


void kernelWindowShellUpdateList(kernelWindow *list[], int number)
{
	// When the list of open windows has changed, the window environment can
	// call this function so we can update our taskbar.

	componentParameters params;
	kernelWindowComponent *item = NULL;
	process windowProcess;
	int count;

	// Check params
	if (!list)
		return;

	// If the window shell is dead, don't continue
	if (!kernelMultitaskerProcessIsAlive(shellData.processId))
		return;

	kernelDebug(debug_gui, "WindowShell update window list");

	shellData.windowList = list;
	shellData.numberWindows = number;

	if (shellData.windowMenu)
	{
		// Destroy all the menu items in the menu
		for (count = 0; count < shellData.numWinMenuItems; count ++)
			kernelWindowComponentDestroy(
				shellData.winMenuItems[count].itemComponent);
		shellData.numWinMenuItems = 0;
		if (shellData.winMenuItems)
			kernelFree(shellData.winMenuItems);
		shellData.winMenuItems = NULL;

		// Copy the parameters from the menu to use
		memcpy(&params, (void *) &(shellData.menuBar->params),
			sizeof(componentParameters));

		for (count = 0; count < shellData.numberWindows; count ++)
		{
			// Skip windows we don't want to include

			if ((shellData.windowList[count] == shellData.rootWindow) ||
				!strcmp((char *) shellData.windowList[count]->title,
					WINNAME_TEMPCONSOLE))
			{
				continue;
			}

			// Skip child windows too
			if (shellData.windowList[count]->parentWindow)
				continue;

			item = kernelWindowNewMenuItem(shellData.windowMenu, (char *)
				shellData.windowList[count]->title, &params);

			shellData.winMenuItems = kernelRealloc(shellData.winMenuItems,
				((shellData.numWinMenuItems + 1) * sizeof(menuItem)));
			if (!shellData.winMenuItems)
				return;

			shellData.winMenuItems[shellData.numWinMenuItems].itemComponent =
				item;
			shellData.winMenuItems[shellData.numWinMenuItems].window =
				shellData.windowList[count];
			shellData.numWinMenuItems += 1;

			kernelWindowRegisterEventHandler(item, &windowMenuEvent);
		}
	}

	// If any windows' parent processes are no longer alive, make the window
	// shell be its parent.
	for (count = 0; count < shellData.numberWindows; count ++)
	{
		if ((shellData.windowList[count] != shellData.rootWindow) &&
			(kernelMultitaskerGetProcess(shellData.windowList[count]->
				processId, &windowProcess) >= 0))
		{
			if ((windowProcess.type != proc_thread) &&
				!kernelMultitaskerProcessIsAlive(windowProcess.
					parentProcessId))
			{
				kernelMultitaskerSetProcessParent(
					shellData.windowList[count]->processId,
						shellData.processId);
			}
		}
	}
}


int kernelWindowShellRefresh(void)
{
	// This function tells the window shell to refresh everything.

	// This was implemented in order to facilitate instantaneous language
	// switching, but it can be expanded to cover more things (e.g. desktop
	// configuration, icons, menus, etc.)

	shellData.refresh = 1;
	return (0);
}


int kernelWindowShellTileBackground(const char *fileName)
{
	// This will tile the supplied image as the background image of the
	// root window

	int status = 0;
	image backgroundImage;

	// Make sure we have a root window
	if (!shellData.rootWindow)
		return (status = ERR_NOTINITIALIZED);

	// If the window shell is dead, don't continue
	if (!kernelMultitaskerProcessIsAlive(shellData.processId))
		return (status = ERR_NOSUCHPROCESS);

	if (fileName)
	{
		// Try to load the new background image
		status = kernelImageLoad(fileName, 0, 0, &backgroundImage);
		if (status < 0)
		{
			kernelError(kernel_error, "Error loading background image %s",
				fileName);
			return (status);
		}

		// Put the background image into our window.
		kernelWindowSetBackgroundImage(shellData.rootWindow, &backgroundImage);

		// Release the image memory
		kernelImageFree(&backgroundImage);
	}
	else
	{
		kernelWindowSetBackgroundImage(shellData.rootWindow, NULL);
	}

	// Redraw the root window
	if (shellData.rootWindow->draw)
		shellData.rootWindow->draw(shellData.rootWindow);

	return (status = 0);
}


int kernelWindowShellCenterBackground(const char *filename)
{
	// This will center the supplied image as the background

	// If the window shell is dead, don't continue
	if (!kernelMultitaskerProcessIsAlive(shellData.processId))
		return (ERR_NOSUCHPROCESS);

	// For the moment, this is not really implemented.  The 'tile' routine
	// will automatically center the image if it's wider or higher than half
	// the screen size anyway.
	return (kernelWindowShellTileBackground(filename));
}


int kernelWindowShellRaiseWindowMenu(void)
{
	// Focus the root window and raise the window menu.  This would typically
	// be done in response to the user pressing ALT-Tab.

	int status = 0;

	// If the window shell is dead, don't continue
	if (!kernelMultitaskerProcessIsAlive(shellData.processId))
		return (status = ERR_NOSUCHPROCESS);

	kernelDebug(debug_gui, "WindowShell toggle root window menu bar");

	if (shellData.rootWindow &&
		(shellData.rootWindow->flags & WINFLAG_VISIBLE))
	{
		kernelWindowFocus(shellData.rootWindow);

		status = kernelWindowToggleMenuBar();
	}

	return (status);
}

