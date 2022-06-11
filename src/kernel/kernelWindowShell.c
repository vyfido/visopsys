//
//  Visopsys
//  Copyright (C) 1998-2019 J. Andrew McLaughlin
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
#include "kernelEnvironment.h"
#include "kernelError.h"
#include "kernelFile.h"
#include "kernelGraphic.h"
#include "kernelImage.h"
#include "kernelLoader.h"
#include "kernelLog.h"
#include "kernelMalloc.h"
#include "kernelMisc.h"
#include "kernelMultitasker.h"
#include "kernelParameters.h"
#include "kernelTouch.h"
#include "kernelUser.h"
#include "kernelWindowEventStream.h"
#include <locale.h>
#include <string.h>
#include <sys/deskconf.h>
#include <sys/paths.h>
#include <sys/vis.h>

typedef struct {
	kernelWindowComponent *itemComponent;
	char command[MAX_PATH_NAME_LENGTH + 1];
	kernelWindow *window;

} menuItemData;

typedef struct {
	int processId;
	kernelWindowComponent *component;

} menuBarComponent;

static volatile struct {
	char userName[USER_MAX_NAMELENGTH + 1];
	int privilege;
	int processId;
	userSession *session;
	kernelWindow *rootWindow;
	kernelWindowComponent *menuBar;
	linkedList menusList;
	kernelWindow *windowMenu;
	linkedList menuItemsList;
	linkedList winMenuItemsList;
	linkedList menuBarCompsList;
	kernelWindowComponent **icons;
	int numIcons;
	linkedList *windowList;
	int refresh;

} shellData;

extern kernelWindowVariables *windowVariables;


static void menuEvent(kernelWindowComponent *component, windowEvent *event)
{
	int status = 0;
	menuItemData *itemData = NULL;
	linkedListItem *iter = NULL;

	kernelDebug(debug_gui, "WindowShell taskbar menu event");

	if (event->type & WINDOW_EVENT_SELECTION)
	{
		kernelDebug(debug_gui, "WindowShell taskbar menu selection");

		itemData = linkedListIterStart((linkedList *)
			&shellData.menuItemsList, &iter);

		while (itemData)
		{
			if (component == itemData->itemComponent)
			{
				if (itemData->command[0])
				{
					kernelWindowSwitchPointer(shellData.rootWindow,
						MOUSE_POINTER_BUSY);

					// Run the command, no block
					status = kernelLoaderLoadAndExec(itemData->command,
						shellData.privilege, 0);

					kernelWindowSwitchPointer(shellData.rootWindow,
						MOUSE_POINTER_DEFAULT);

					if (status < 0)
					{
						kernelError(kernel_error, "Unable to execute program "
							"%s", itemData->command);
					}
				}

				break;
			}

			itemData = linkedListIterNext((linkedList *)
				&shellData.menuItemsList, &iter);
		}
	}
}


static void iconEvent(kernelWindowComponent *component, windowEvent *event)
{
	int status = 0;
	kernelWindowIcon *iconComponent = component->data;
	static int dragging = 0;

	if (event->type & WINDOW_EVENT_MOUSE_DRAG)
	{
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

		kernelDebug(debug_gui, "WindowShell icon mouse click");

		kernelWindowSwitchPointer(shellData.rootWindow, MOUSE_POINTER_BUSY);

		// Run the command
		status = kernelLoaderLoadAndExec((const char *)
			iconComponent->command, shellData.privilege, 0 /* no block */);

		kernelWindowSwitchPointer(shellData.rootWindow,
			MOUSE_POINTER_DEFAULT);

		if (status < 0)
		{
			kernelError(kernel_error, "Unable to execute program %s",
				iconComponent->command);
		}
	}
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

	status = kernelConfigReadSystem(fileName, settings);

	return (status);
}


static int readConfig(variableList *settings)
{
	// Return a (possibly empty) variable list, filled with any desktop
	// settings we read from various config files.

	int status = 0;
	char fileName[MAX_PATH_NAME_LENGTH + 1];
	variableList userConfig;
	const char *variable = NULL;
	const char *value = NULL;
	char language[LOCALE_MAX_NAMELEN + 1];
	variableList langConfig;
	int count;

	kernelDebug(debug_gui, "WindowShell read configuration");

	memset(&userConfig, 0, sizeof(variableList));
	memset(&langConfig, 0, sizeof(variableList));

	// First try to read the system desktop config.
	status = readFileConfig(PATH_SYSTEM_CONFIG "/" DESKTOP_CONFIG,
		settings);
	if (status < 0)
	{
		// Argh.  No file?  Create an empty list for us to use
		status = variableListCreateSystem(settings);
		if (status < 0)
			return (status);
	}

	if (strcmp((char *) shellData.userName, USER_ADMIN))
	{
		// Try to read any user-specific desktop config.
		sprintf(fileName, PATH_USERS_CONFIG "/" DESKTOP_CONFIG,
			shellData.userName);

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
	// language-specific desktop config file that matches it.
	status = kernelEnvironmentGet(ENV_LANG, language, LOCALE_MAX_NAMELEN);
	if (status >= 0)
	{
		sprintf(fileName, "%s/%s/%s", PATH_SYSTEM_CONFIG, language,
			DESKTOP_CONFIG);

		status = kernelFileFind(fileName, NULL);
		if (status >= 0)
		{
			status = kernelConfigReadSystem(fileName, &langConfig);
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

				variableListDestroy(&langConfig);
			}
		}
	}

	return (status = 0);
}


static int makeMenuBar(variableList *settings)
{
	// Make or re-create a menu bar at the top

	int status = 0;
	const char *variable = NULL;
	const char *value = NULL;
	const char *menuName = NULL;
	const char *menuLabel = NULL;
	kernelWindow *menu = NULL;
	char propertyName[128];
	const char *itemName = NULL;
	const char *itemLabel = NULL;
	menuItemData *itemData = NULL;
	componentParameters params;
	int count1, count2;

	kernelDebug(debug_gui, "WindowShell make menu bar");

	memset(&params, 0, sizeof(componentParameters));

	memcpy(&params.foreground, &COLOR_WHITE, sizeof(color));
	memcpy(&params.background, &windowVariables->color.foreground,
		sizeof(color));
	params.flags |= (COMP_PARAMS_FLAG_CUSTOMFOREGROUND |
		COMP_PARAMS_FLAG_CUSTOMBACKGROUND);
	params.font = windowVariables->font.varWidth.medium.font;

	if (!shellData.menuBar)
	{
		shellData.menuBar = kernelWindowNewMenuBar(shellData.rootWindow,
			&params);
	}

	// Try to load menu bar menus and menu items

	// Loop for variables starting with DESKTOP_TASKBAR_MENU
	for (count1 = 0; count1 < settings->numVariables; count1 ++)
	{
		variable = variableListGetVariable(settings, count1);
		if (variable && !strncmp(variable, DESKVAR_TASKBAR_MENU,
			strlen(DESKVAR_TASKBAR_MENU)))
		{
			menuName = (variable + strlen(DESKVAR_TASKBAR_MENU));
			menuLabel = variableListGet(settings, variable);

			menu = kernelWindowNewMenu(shellData.rootWindow,
				shellData.menuBar, menuLabel, NULL, &params);

			// Add it to our list

			status = linkedListAdd((linkedList *) &shellData.menusList,
				(void *) menu);
			if (status < 0)
			{
				kernelWindowDestroy(menu);
				return (status = ERR_MEMORY);
			}

			// Now loop and get any components for this menu
			for (count2 = 0; count2 < settings->numVariables; count2 ++)
			{
				sprintf(propertyName, DESKVAR_TASKBAR_MENUITEM, menuName);

				variable = variableListGetVariable(settings, count2);
				if (!strncmp(variable, propertyName, strlen(propertyName)))
				{
					itemName = (variable + strlen(propertyName));
					itemLabel = variableListGet(settings, variable);

					if (!itemLabel)
						continue;

					// See if there's an associated command
					sprintf(propertyName, DESKVAR_TASKBAR_MENUITEM_CMD,
						menuName, itemName);
					value = variableListGet(settings, propertyName);
					if (!value || (kernelLoaderCheckCommand(value) < 0))
						// No such command.  Don't show this one.
						continue;

					// Get memory for menu item data
					itemData = kernelMalloc(sizeof(menuItemData));
					if (!itemData)
						continue;

					// Create the menu item
					itemData->itemComponent = kernelWindowNewMenuItem(menu,
						itemLabel, &params);
					if (!itemData->itemComponent)
					{
						kernelFree(itemData);
						continue;
					}

					strncpy(itemData->command, value, MAX_PATH_NAME_LENGTH);

					// Add it to our list
					status = linkedListAdd((linkedList *)
						&shellData.menuItemsList, itemData);
					if (status < 0)
					{
						kernelWindowComponentDestroy(itemData->itemComponent);
						kernelFree(itemData);
						return (status = ERR_MEMORY);
					}

					kernelWindowRegisterEventHandler(itemData->itemComponent,
						&menuEvent);
				}
			}

			// We treat any 'window' menu specially, since it is not usually
			// populated at startup time, only as windows are created or
			// destroyed.
			if (!strcmp(menuName, DESKVAR_TASKBAR_WINDOWMENU))
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
	params.flags = (COMP_PARAMS_FLAG_CANDRAG |
		COMP_PARAMS_FLAG_CUSTOMFOREGROUND |
		COMP_PARAMS_FLAG_CUSTOMBACKGROUND | COMP_PARAMS_FLAG_CANFOCUS |
		COMP_PARAMS_FLAG_FIXEDWIDTH | COMP_PARAMS_FLAG_FIXEDHEIGHT);
	memcpy(&params.foreground, &COLOR_WHITE, sizeof(color));
	memcpy(&params.background, &windowVariables->color.desktop,
		sizeof(color));
	params.orientationX = orient_center;
	params.orientationY = orient_middle;

	// Loop for variables starting with DESKTOP_ICON_NAME
	for (count = 0; count < settings->numVariables; count ++)
	{
		variable = variableListGetVariable(settings, count);
		if (variable && !strncmp(variable, DESKVAR_ICON_NAME,
			strlen(DESKVAR_ICON_NAME)))
		{
			iconName = (variable + strlen(DESKVAR_ICON_NAME));
			iconLabel = variableListGet(settings, variable);

			// Get the rest of the recognized properties for this icon.

			// See if there's a command associated with this, and make sure
			// the program exists.
			sprintf(propertyName, DESKVAR_ICON_COMMAND, iconName);
			command = variableListGet(settings, propertyName);
			if (!command || (kernelLoaderCheckCommand(command) < 0))
				continue;

			// Get the image name, make sure it exists, and try to load it.
			sprintf(propertyName, DESKVAR_ICON_IMAGE, iconName);
			imageFile = variableListGet(settings, propertyName);
			if (!imageFile || (kernelFileFind(imageFile, NULL) < 0) ||
				(kernelImageLoad(imageFile, 64, 64, &tmpImage) < 0))
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
	kernelWindowSetMovable(shellData.rootWindow, 0);
	kernelWindowSetResizable(shellData.rootWindow, 0);
	kernelWindowSetRoot(shellData.rootWindow);
	kernelWindowSetHasTitleBar(shellData.rootWindow, 0);
	kernelWindowSetHasBorder(shellData.rootWindow, 0);

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
	imageFile = variableListGet(&settings, DESKVAR_BACKGROUND_IMAGE);
	if (imageFile)
	{
		kernelDebug(debug_gui, "WindowShell loading background image \"%s\"",
			imageFile);

		if (strcmp(imageFile, DESKVAR_BACKGROUND_NONE))
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
		variableListDestroy(&settings);
		return (status);
	}

	// Make icons
	status = makeIcons(&settings);
	if (status < 0)
	{
		variableListDestroy(&settings);
		return (status);
	}

	variableListDestroy(&settings);

	// Location in the top corner
	status = kernelWindowSetLocation(shellData.rootWindow, 0, 0);
	if (status < 0)
		return (status);

	// Resize to the whole screen
	status = kernelWindowSetSize(shellData.rootWindow,
		kernelGraphicGetScreenWidth(), kernelGraphicGetScreenHeight());
	if (status < 0)
		return (status);

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

	// Loop for variables starting with DESKVAR_PROGRAM
	for (count = 0; count < settings.numVariables; count ++)
	{
		variable = variableListGetVariable(&settings, count);
		if (variable && !strncmp(variable, DESKVAR_PROGRAM,
			strlen(DESKVAR_PROGRAM)))
		{
			programName = variableListGet(&settings, variable);
			if (programName)
			{
				// Try to run the program
				kernelLoaderLoadAndExec(programName, shellData.privilege, 0);
			}
		}
	}

	variableListDestroy(&settings);

	// If touch support is available, we will also run the virtual keyboard
	// program in 'iconified' mode
	if (kernelTouchAvailable() &&
		(kernelFileFind(PATH_PROGRAMS "/keyboard", NULL) >= 0))
	{
		kernelLoaderLoadAndExec(PATH_PROGRAMS "/keyboard -i",
			shellData.privilege, 0);
	}
}


static void scanMenuItemEvents(linkedList *list)
{
	// Scan through events in a list of our menu items

	menuItemData *itemData = NULL;
	linkedListItem *iter = NULL;
	kernelWindowComponent *component = NULL;
	windowEvent event;

	itemData = linkedListIterStart(list, &iter);
	while (itemData)
	{
		component = itemData->itemComponent;

		// Any events pending?  Any event handler?
		if (component->eventHandler &&
			(kernelWindowEventStreamRead(&component->events, &event) > 0))
		{
			kernelDebug(debug_gui, "WindowShell root menu item got event");
			component->eventHandler(component, &event);
		}

		itemData = linkedListIterNext(list, &iter);
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
			(kernelWindowEventStreamRead(&component->events, &event) > 0))
		{
			kernelDebug(debug_gui, "WindowShell scan container got event");
			component->eventHandler(component, &event);
		}

		// If this component is a container type, recurse
		if (component->type == containerComponentType)
			scanContainerEvents(component->data);
	}
}


static void destroy(void)
{
	menuItemData *itemData = NULL;
	linkedListItem *iter = NULL;
	kernelWindow *menu = NULL;
	int count;

	// Destroy icons
	for (count = 0; count < shellData.numIcons; count ++)
		kernelWindowComponentDestroy(shellData.icons[count]);

	shellData.numIcons = 0;
	kernelFree(shellData.icons);
	shellData.icons = NULL;

	// Don't destroy menu bar components, since they're 'owned' by other
	// processes.

	// Deallocate (static) menu item data structures

	itemData = linkedListIterStart((linkedList *) &shellData.menuItemsList,
		&iter);

	while (itemData)
	{
		kernelFree(itemData);

		itemData = linkedListIterNext((linkedList *) &shellData.menuItemsList,
			&iter);
	}

	linkedListClear((linkedList *) &shellData.menuItemsList);

	// Deallocate window menu item data structures

	itemData = linkedListIterStart((linkedList *) &shellData.winMenuItemsList,
		&iter);

	while (itemData)
	{
		kernelFree(itemData);

		itemData = linkedListIterNext((linkedList *)
			&shellData.winMenuItemsList, &iter);
	}

	linkedListClear((linkedList *) &shellData.winMenuItemsList);

	// Do this before destroying menus (because menus are windows, and
	// kernelWindowShellUpdateList() will get called)
	shellData.windowMenu = NULL;

	// Destroy menus

	menu = linkedListIterStart((linkedList *) &shellData.menusList, &iter);

	while (menu)
	{
		kernelWindowMenuDestroy(menu);
		menu = linkedListIterNext((linkedList *) &shellData.menusList, &iter);
	}

	linkedListClear((linkedList *) &shellData.menusList);

	// Don't destroy the menu bar
}


static void refresh(void)
{
	// Refresh the desktop environment

	kernelWindow *listWindow = NULL;
	linkedListItem *iter = NULL;
	process windowProcess;
	char charSet[CHARSET_NAME_LEN + 1];
	variableList settings;
	windowEvent event;

	kernelDebug(debug_gui, "WindowShell refresh");

	// This is a dodgy hack: If any windows' parent processes are no longer
	// alive, make the window shell process be their parents.  This is so that
	// the environment propagation, below, reaches all the windows' processes.
	// Ideally the multitasker would implement a way of tracking the lineage
	// of these processes back to their shell ancestor.
	listWindow = linkedListIterStart(shellData.windowList, &iter);
	while (listWindow)
	{
		if ((listWindow != shellData.rootWindow) &&
			(kernelMultitaskerGetProcess(listWindow->processId,
				&windowProcess) >= 0))
		{
			if ((windowProcess.type != proc_thread) &&
				!kernelMultitaskerProcessIsAlive(windowProcess.
					parentProcessId))
			{
				kernelMultitaskerSetProcessParent(listWindow->processId,
					shellData.processId);
			}
		}

		listWindow = linkedListIterNext(shellData.windowList, &iter);
	}

	// Reload the user environment
	if (kernelEnvironmentLoad((char *) shellData.userName,
		shellData.processId) >= 0)
	{
		// Propagate it to all the shell's child processes
		kernelMultitaskerPropagateEnvironment(shellData.processId, NULL);
	}

	// Re-get the character set
	if (shellData.rootWindow && (kernelEnvironmentGet(ENV_CHARSET, charSet,
		CHARSET_NAME_LEN) >= 0))
	{
		kernelWindowSetCharSet(shellData.rootWindow, charSet);
	}

	// Read the desktop config file(s)
	if (readConfig(&settings) >= 0)
	{
		// Get rid of all our existing stuff
		destroy();

		// Re-create the menu bar
		makeMenuBar(&settings);

		// Re-load the icons
		makeIcons(&settings);

		variableListDestroy(&settings);

		if (shellData.rootWindow)
			// Re-do root window layout
			kernelWindowLayout(shellData.rootWindow);
	}

	// Send a 'window refresh' event to every window
	if (shellData.windowList)
	{
		memset(&event, 0, sizeof(windowEvent));
		event.type = WINDOW_EVENT_WINDOW_REFRESH;

		listWindow = linkedListIterStart(shellData.windowList, &iter);
		while (listWindow)
		{
			if (kernelMultitaskerGetProcessUserSession(
				listWindow->processId) == shellData.session)
			{
				kernelWindowEventStreamWrite(&listWindow->events, &event);

				// Yield after sending each one; hopefully this will allow
				// them all a chance to update before we update the window
				// list.
				kernelMultitaskerYield();
			}

			listWindow = linkedListIterNext(shellData.windowList, &iter);
		}
	}

	// Update the window menu
	kernelWindowShellUpdateList(shellData.windowList);

	shellData.refresh = 0;
}


__attribute__((noreturn))
static void windowShellThread(void)
{
	// This thread runs as the 'window shell' to watch for window events on
	// 'root window' GUI components, and which functions as the user's login
	// shell in graphics mode.

	int status = 0;
	menuBarComponent *menuBarComp = NULL;
	linkedListItem *iter = NULL;

	// Create the root window
	status = makeRootWindow();
	if (status < 0)
		kernelMultitaskerTerminate(status);

	shellData.session =
		kernelMultitaskerGetProcessUserSession(shellData.processId);
	if (!shellData.session)
	{
		kernelError(kernel_error, "Unable to get the user session");
		kernelMultitaskerTerminate(status = ERR_NOSUCHUSER);
	}

	// Run any programs that we're supposed to run after login
	runPrograms();

	// Now loop and process any events
	while (1)
	{
		if (shellData.refresh)
			refresh();

		scanMenuItemEvents((linkedList *) &shellData.winMenuItemsList);
		scanMenuItemEvents((linkedList *) &shellData.menuItemsList);

		scanContainerEvents(((kernelWindowMenuBar *)
			shellData.menuBar->data)->container->data);

		scanContainerEvents(shellData.rootWindow->mainContainer->data);

		// Make sure the owners of any menu bar components are still alive

		menuBarComp = linkedListIterStart((linkedList *)
			&shellData.menuBarCompsList, &iter);

		while (menuBarComp)
		{
			if (!kernelMultitaskerProcessIsAlive(menuBarComp->processId))
				kernelWindowShellDestroyTaskbarComp(menuBarComp->component);

			menuBarComp = linkedListIterNext((linkedList *)
				&shellData.menuBarCompsList, &iter);
		}

		// Done
		kernelMultitaskerYield();
	}
}


static void windowMenuEvent(kernelWindowComponent *component,
	windowEvent *event)
{
	menuItemData *itemData = NULL;
	linkedListItem *iter = NULL;

	kernelDebug(debug_gui, "WindowShell taskbar window menu event");

	if (shellData.windowMenu && (event->type & WINDOW_EVENT_SELECTION))
	{
		kernelDebug(debug_gui, "WindowShell taskbar window menu selection");

		itemData = linkedListIterStart((linkedList *)
			&shellData.winMenuItemsList, &iter);

		while (itemData)
		{
			if (component == itemData->itemComponent)
			{
				// Restore it
				kernelDebug(debug_gui, "WindowShell restore window %s",
					itemData->window->title);
				kernelWindowSetMinimized(itemData->window, 0);

				// If it has a dialog box, restore that too
				if (itemData->window->dialogWindow)
				{
					kernelWindowSetMinimized(itemData->window->dialogWindow,
						0);
				}

				break;
			}

			itemData = linkedListIterNext((linkedList *)
				&shellData.winMenuItemsList, &iter);
		}
	}
}


static void updateMenuBarComponents(void)
{
	// Re-layout the menu bar
	if (shellData.menuBar->layout)
		shellData.menuBar->layout(shellData.menuBar);

	// Re-draw the menu bar
	if (shellData.menuBar->draw)
		shellData.menuBar->draw(shellData.menuBar);

	// Re-render the menu bar on screen
	shellData.rootWindow->update(shellData.rootWindow,
		shellData.menuBar->xCoord, shellData.menuBar->yCoord,
		shellData.menuBar->width, shellData.menuBar->height);
}


static int addMenuBarComponent(kernelWindowComponent *component)
{
	int status = 0;
	menuBarComponent *menuBarComp = NULL;

	// Add the component to the shell's list of menu bar components

	menuBarComp = kernelMalloc(sizeof(menuBarComponent));
	if (!menuBarComp)
		return (status = ERR_MEMORY);

	menuBarComp->processId = kernelMultitaskerGetCurrentProcessId();
	menuBarComp->component = component;

	status = linkedListAdd((linkedList *) &shellData.menuBarCompsList,
		menuBarComp);
	if (status < 0)
	{
		kernelFree(menuBarComp);
		return (status);
	}

	// Re-draw the menu bar
	updateMenuBarComponents();

	return (status = 0);
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
		return (ERR_NULLPARAMETER);

	memset((void *) &shellData, 0, sizeof(shellData));

	memcpy((char *) shellData.userName, user, USER_MAX_NAMELENGTH);
	shellData.privilege = kernelUserGetPrivilege((char *) shellData.userName);

	// Spawn the window shell thread
	shellData.processId = kernelMultitaskerSpawn(windowShellThread,
		"window shell", 0 /* no args */, NULL /* no args */,
		0 /* don't run */);

	return (shellData.processId);
}


void kernelWindowShellUpdateList(linkedList *list)
{
	// When the list of open windows has changed, the window environment can
	// call this function so we can update our taskbar.

	componentParameters params;
	menuItemData *itemData = NULL;
	linkedListItem *iter = NULL;
	kernelWindow *listWindow = NULL;

	// Check params
	if (!list)
		return;

	// If the window shell is dead, don't continue
	if (!kernelMultitaskerProcessIsAlive(shellData.processId))
		return;

	kernelDebug(debug_gui, "WindowShell update window list");

	shellData.windowList = list;

	if (shellData.windowMenu)
	{
		// Destroy all the menu items in the menu

		itemData = linkedListIterStart((linkedList *)
			&shellData.winMenuItemsList, &iter);

		while (itemData)
		{
			linkedListRemove((linkedList *) &shellData.winMenuItemsList,
				itemData);

			kernelWindowComponentDestroy(itemData->itemComponent);

			kernelFree(itemData);

			itemData = linkedListIterNext((linkedList *)
				&shellData.winMenuItemsList, &iter);
		}

		linkedListClear((linkedList *) &shellData.winMenuItemsList);

		// Copy the parameters from the menu to use
		memcpy(&params, (void *) &shellData.menuBar->params,
			sizeof(componentParameters));

		listWindow = linkedListIterStart(shellData.windowList, &iter);
		while (listWindow)
		{
			// Skip windows we don't want to include

			if (
				// Skip the root window
			 	(listWindow == shellData.rootWindow) ||
				// Skip any temporary console window
				!strcmp((char *) listWindow->title, WINNAME_TEMPCONSOLE) ||
				// Skip any iconified windows
				(listWindow->flags & WINDOW_FLAG_ICONIFIED) ||
				// Skip child windows too
				listWindow->parentWindow
			)
			{
				listWindow = linkedListIterNext(shellData.windowList, &iter);
				continue;
			}

			itemData = kernelMalloc(sizeof(menuItemData));
			if (!itemData)
				return;

			itemData->itemComponent =
				kernelWindowNewMenuItem(shellData.windowMenu,
					(char *) listWindow->title, &params);
			itemData->window = listWindow;

			linkedListAdd((linkedList *) &shellData.winMenuItemsList,
				itemData);

			kernelWindowRegisterEventHandler(itemData->itemComponent,
				&windowMenuEvent);

			listWindow = linkedListIterNext(shellData.windowList, &iter);
		}
	}
}


void kernelWindowShellRefresh(void)
{
	// This function tells the window shell to refresh everything.

	// This was implemented in order to facilitate instantaneous language
	// switching, but it can be expanded to cover more things (e.g. desktop
	// configuration, icons, menus, etc.)

	shellData.refresh = 1;
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

	// For the moment, this is not really implemented.  The 'tile' function
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
		(shellData.rootWindow->flags & WINDOW_FLAG_VISIBLE))
	{
		status = kernelWindowToggleMenuBar(shellData.rootWindow);
	}

	return (status);
}


kernelWindowComponent *kernelWindowShellNewTaskbarIcon(image *img)
{
	// Create an icon in the shell's top menu bar.

	kernelWindowComponent *iconComponent = NULL;
	componentParameters params;

	// Check params
	if (!img)
		return (iconComponent = NULL);

	// Make sure we have a root window and menu bar
	if (!shellData.rootWindow || !shellData.menuBar)
		return (iconComponent = NULL);

	memset(&params, 0, sizeof(componentParameters));
	params.flags = COMP_PARAMS_FLAG_CANFOCUS;

	// Create the menu bar icon
	iconComponent = kernelWindowNewMenuBarIcon(shellData.menuBar, img,
		&params);
	if (!iconComponent)
		return (iconComponent);

	// Add it to the shell's list of menu bar components
	if (addMenuBarComponent(iconComponent) < 0)
	{
		kernelWindowComponentDestroy(iconComponent);
		return (iconComponent = NULL);
	}

	return (iconComponent);
}


kernelWindowComponent *kernelWindowShellNewTaskbarTextLabel(const char *text)
{
	// Create an label in the shell's top menu bar.

	kernelWindowComponent *labelComponent = NULL;
	componentParameters params;

	// Check params
	if (!text)
		return (labelComponent = NULL);

	// Make sure we have a root window and menu bar
	if (!shellData.rootWindow || !shellData.menuBar)
		return (labelComponent = NULL);

	memset(&params, 0, sizeof(componentParameters));
	params.foreground = shellData.menuBar->params.foreground;
	params.background = shellData.menuBar->params.background;
	params.flags |= (COMP_PARAMS_FLAG_CUSTOMFOREGROUND |
		COMP_PARAMS_FLAG_CUSTOMBACKGROUND);
	params.font = windowVariables->font.varWidth.small.font;

	// Create the menu bar label
	labelComponent = kernelWindowNewTextLabel(shellData.menuBar, text,
		&params);
	if (!labelComponent)
		return (labelComponent);

	// Add it to the shell's list of menu bar components
	if (addMenuBarComponent(labelComponent) < 0)
	{
		kernelWindowComponentDestroy(labelComponent);
		return (labelComponent = NULL);
	}

	return (labelComponent);
}


int kernelWindowShellDestroyTaskbarComp(kernelWindowComponent *component)
{
	// Destroy a component in the shell's top menu bar.

	int status = 0;
	menuBarComponent *menuBarComp = NULL;
	linkedListItem *iter = NULL;

	// Check params
	if (!component)
	{
		kernelError(kernel_error, "NULL parameter");
		return (status = ERR_NULLPARAMETER);
	}

	// Make sure we have a root window and menu bar
	if (!shellData.rootWindow || !shellData.menuBar)
		return (status = ERR_NOSUCHENTRY);

	// Remove it from the list

	menuBarComp = linkedListIterStart((linkedList *)
		&shellData.menuBarCompsList, &iter);

	while (menuBarComp)
	{
		if (menuBarComp->component == component)
		{
			linkedListRemove((linkedList *) &shellData.menuBarCompsList,
				menuBarComp);

			kernelFree(menuBarComp);

			break;
		}

		menuBarComp = linkedListIterNext((linkedList *)
			&shellData.menuBarCompsList, &iter);
	}

	// Destroy it
	kernelWindowComponentDestroy(component);

	// Re-draw the menu bar
	updateMenuBarComponents();

	return (status = 0);
}


kernelWindowComponent *kernelWindowShellIconify(kernelWindow *window,
	int iconify, image *img)
{
	kernelWindowComponent *iconComponent = NULL;

	// Check params.  img is allowed to be NULL.
	if (!window)
		return (iconComponent = NULL);

	if (img)
	{
		iconComponent = kernelWindowShellNewTaskbarIcon(img);
		if (!iconComponent)
			return (iconComponent);
	}

	if (iconify)
		window->flags |= WINDOW_FLAG_ICONIFIED;
	else
		window->flags &= ~WINDOW_FLAG_ICONIFIED;

	kernelWindowSetVisible(window, !iconify);

	// Update the window menu
	kernelWindowShellUpdateList(shellData.windowList);

	return (iconComponent);
}

