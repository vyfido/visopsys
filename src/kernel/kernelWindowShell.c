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
//  kernelWindowShell.c
//

// This is the code that manages the 'root' window in the GUI environment.

#include "kernelWindow.h"
#include "kernelDebug.h"
#include "kernelDisk.h"
#include "kernelError.h"
#include "kernelLoader.h"
#include "kernelLog.h"
#include "kernelMalloc.h"
#include "kernelMemory.h"
#include "kernelMisc.h"
#include "kernelMultitasker.h"
#include "kernelParameters.h"
#include "kernelWindowEventStream.h"
#include <string.h>
#include <stdio.h>

typedef struct {
	kernelWindowComponent *itemComponent;
	char command[MAX_PATH_NAME_LENGTH];
	kernelWindow *window;

} menuItem;

static kernelWindow *rootWindow = NULL;
static kernelWindowComponent *taskMenuBar = NULL;
static kernelWindowComponent *windowMenu = NULL;
static menuItem *menuItems = NULL;
static int numMenuItems = 0;
static menuItem *winMenuItems = NULL;
static int numWinMenuItems = 0;
static int privilege = 0;
static int initialized = 0;

extern kernelWindowVariables *windowVariables;


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

		kernelDebug(debug_gui, "Icon mouse click");

		kernelWindowSwitchPointer(rootWindow, "busy");

		// Run the command
		status =
			kernelLoaderLoadAndExec((const char *) iconComponent->command,
				privilege, 0 /* no block */);

		kernelWindowSwitchPointer(rootWindow, "default");
		
		if (status < 0)
			kernelError(kernel_error, "Unable to execute program %s",
				iconComponent->command);
	}

	return;
}


static void menuEvent(kernelWindowComponent *component, windowEvent *event)
{
	int status = 0;
	int count;

	kernelDebug(debug_gui, "Taskbar menu event");

	if (event->type & EVENT_SELECTION)
	{
		kernelDebug(debug_gui, "Taskbar menu selection");

		for (count = 0; count < numMenuItems; count ++)
		{
			if (component == menuItems[count].itemComponent)
			{
				if (menuItems[count].command[0] != NULL)
				{
					kernelWindowSwitchPointer(rootWindow, "busy");

					// Run the command, no block
					status = kernelLoaderLoadAndExec(menuItems[count].command,
						privilege, 0);

					kernelWindowSwitchPointer(rootWindow, "default");

					if (status < 0)
						kernelError(kernel_error, "Unable to execute program %s",
							menuItems[count].command);
				}

				break;
			}
		}
	}

	return;
}


static void windowMenuEvent(kernelWindowComponent *component,
	windowEvent *event)
{
	int count;

	kernelDebug(debug_gui, "Taskbar window menu event");

	if (windowMenu && (event->type & EVENT_SELECTION))
	{
		kernelDebug(debug_gui, "Taskbar window menu selection");

		// See if this is one of our window menu components
		for (count = 0; count < numWinMenuItems; count ++)
		{
			if (component == winMenuItems[count].itemComponent)
			{
				// Restore it
				kernelDebug(debug_gui, "Restore window %s",
					winMenuItems[count].window->title);
				kernelWindowSetMinimized(winMenuItems[count].window, 0);

				// If it has a dialog box, restore that too
				if (winMenuItems[count].window->dialogWindow)
					kernelWindowSetMinimized(winMenuItems[count].window
						->dialogWindow, 0);

				break;
			}
		}
	}

	return;
}


static void runPrograms(void)
{
	// Get any programs we're supposed to run automatically and run them.

	variableList settings;
	char programName[MAX_PATH_NAME_LENGTH];
	int count;

	// Read the config file
	if (kernelConfigRead(WINDOW_DEFAULT_DESKTOP_CONFIG, &settings) < 0)
		return;

	// Loop for variables with "program.*"
	for (count = 0; count < settings.numVariables; count ++)
	{
		if (!strncmp(settings.variables[count], "program.", 8))
		{
			if (!kernelVariableListGet(&settings, settings.variables[count],
				programName, MAX_PATH_NAME_LENGTH))

			// Try to run the program
			kernelLoaderLoadAndExec(programName, privilege, 0);
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
			kernelDebug(debug_gui, "Root menu item got event");
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
			kernelDebug(debug_gui, "Scan container got event");
			component->eventHandler(component, &event);
		}

		// If this component is a container type, recurse
		if (component->type == containerComponentType)
			scanContainerEvents(component->data);
	}

	return;
}


__attribute__((noreturn))
static void windowShellThread(void)
{
	// This thread runs as the 'window shell' to watch for window events on
	// 'root window' GUI components, and which functions as the user's login
	// shell in graphics mode.

	while(1)
	{
		if (winMenuItems)
			scanMenuItemEvents(winMenuItems, numWinMenuItems);

		if (menuItems)
			scanMenuItemEvents(menuItems, numMenuItems);

		scanContainerEvents(rootWindow->mainContainer->data);

		// Done
		kernelMultitaskerYield();
	}
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


kernelWindow *kernelWindowMakeRoot(void)
{
	// Make a main root window to serve as the background for the window
	// environment

	int status = 0;
	variableList settings;
	menuItem *tmpMenuItems = NULL;
	char *iconName = NULL;
	char *menuName = NULL;
	char *itemName = NULL;
	char propertyName[128];
	char propertyValue[128];
	char itemLabel[128];
	char menuLabel[128];
	image tmpImage;
	kernelWindowComponent *iconComponent = NULL;
	kernelWindowComponent *menuComponent = NULL;
	componentParameters params;
	disk configDisk;
	int count1, count2;

	// Get a new window
	rootWindow = kernelWindowNew(KERNELPROCID, WINNAME_ROOTWINDOW);
	if (rootWindow == NULL)
		return (rootWindow);

	// The window will have no border, title bar or close button, is not
	// movable or resizable, and is packed
	rootWindow->flags &= ~(WINFLAG_MOVABLE | WINFLAG_RESIZABLE);
	kernelWindowSetHasTitleBar(rootWindow, 0);
	kernelWindowSetHasBorder(rootWindow, 0);

	// Location in the top corner
	status = kernelWindowSetLocation(rootWindow, 0, 0);
	if (status < 0)
		return (rootWindow = NULL);

	// Size is the whole screen
	status = kernelWindowSetSize(rootWindow, kernelGraphicGetScreenWidth(),
		kernelGraphicGetScreenHeight());
	if (status < 0)
		return (rootWindow = NULL);

	// The window is always at the bottom level
	rootWindow->level = WINDOW_MAXWINDOWS;

	// Set our background color preference
	rootWindow->background.red = windowVariables->color.desktop.red;
	rootWindow->background.green = windowVariables->color.desktop.green;
	rootWindow->background.blue = windowVariables->color.desktop.blue;

	// Read the config file
	status = kernelConfigRead(WINDOW_DEFAULT_DESKTOP_CONFIG, &settings);
	if (status < 0)
	{
		// Argh.  No file?  Create a reasonable, empty list for us to use
		status = kernelVariableListCreate(&settings);
		if (status < 0)
			return (rootWindow = NULL);
	}

	// Try to load the background image
	if (!kernelVariableListGet(&settings, "background.image", propertyValue,
		128) && propertyValue[0])
	{
		kernelDebug(debug_gui, "Loading background image \"%s\"", propertyValue);
		status = kernelImageLoad(propertyValue, 0, 0, &tmpImage);

		if (status == 0)
		{
			// Put the background image into our window.
			kernelWindowSetBackgroundImage(rootWindow, &tmpImage);
			kernelLog("Background image loaded");
		}
		else
			kernelError(kernel_error, "Error loading background image %s",
				propertyValue);

		// Release the image memory
		kernelImageFree(&tmpImage);
	}

	// Make a task menu at the top
	kernelMemClear(&params, sizeof(componentParameters));
	params.foreground.red = 255;
	params.foreground.green = 255;
	params.foreground.blue = 255;
	params.background.red = windowVariables->color.foreground.red;
	params.background.green = windowVariables->color.foreground.green;
	params.background.blue = windowVariables->color.foreground.blue;
	params.flags |=
		(WINDOW_COMPFLAG_CUSTOMFOREGROUND | WINDOW_COMPFLAG_CUSTOMBACKGROUND);
	params.font = windowVariables->font.varWidth.medium.font;
	taskMenuBar = kernelWindowNewMenuBar(rootWindow, &params);

	// Try to load taskbar menus and menu items
		
	// Allocate temporary memory for the normal menu items
	tmpMenuItems = kernelMalloc(256 * sizeof(menuItem)); 
	if (tmpMenuItems == NULL)
	{
		kernelVariableListDestroy(&settings);
		return (rootWindow = NULL);
	}

	// Loop for variables with "taskBar.menu.*"
	for (count1 = 0; count1 < settings.numVariables; count1 ++)
	{
		if (!strncmp(settings.variables[count1], "taskBar.menu.", 13))
		{
			menuName = (settings.variables[count1] + 13);
			kernelVariableListGet(&settings, settings.variables[count1],
				menuLabel, 128);

			menuComponent =
				kernelWindowNewMenu(taskMenuBar, menuLabel, NULL, &params);

			// Now loop and get any components for this menu
			for (count2 = 0; count2 < settings.numVariables; count2 ++)
			{
				sprintf(propertyName, "taskBar.%s.item.", menuName);

				if (!strncmp(settings.variables[count2], propertyName,
					strlen(propertyName)))
				{
					itemName =
						(settings.variables[count2] + strlen(propertyName));
					status = kernelVariableListGet(&settings,
						settings.variables[count2], itemLabel, 128);
					if (status < 0)
						continue;

					// See if there's an associated command
					sprintf(propertyName, "taskBar.%s.%s.command", menuName,
						itemName);
					status = kernelVariableListGet(&settings, propertyName,
						propertyValue, 128);
					if ((status < 0) ||
						(kernelLoaderCheckCommand(propertyValue) < 0))
					{
						// No such command.  Don't show this one.
						continue;
					}

					tmpMenuItems[numMenuItems].itemComponent =
						kernelWindowNewMenuItem(menuComponent, itemLabel,
							&params);

					strncpy(tmpMenuItems[numMenuItems].command, propertyValue,
						MAX_PATH_NAME_LENGTH);

					kernelWindowRegisterEventHandler(tmpMenuItems[numMenuItems]
						.itemComponent, &menuEvent);

					numMenuItems += 1;
				}
			}

			// We treat any "window" menu specially, since it is not usually
			// populated at startup time, only as windows are created or
			// destroyed.
			if (!strcmp(menuName, "window"))
			{
				kernelDebug(debug_gui, "Created window menu");
				windowMenu = menuComponent;

				// Get memory for the window menu items
				winMenuItems =
					kernelMalloc(WINDOW_MAXWINDOWS * sizeof(menuItem));
				if (winMenuItems == NULL)
				{
					kernelVariableListDestroy(&settings);
					kernelFree(tmpMenuItems);
					return (rootWindow = NULL);
				}
			}
		}
	}

	// Free our temporary menu item memory and just allocate the amount we
	// actually need.
	menuItems = kernelMalloc(numMenuItems * sizeof(menuItem)); 
	if (menuItems == NULL)
	{
		kernelVariableListDestroy(&settings);
		return (rootWindow = NULL);
	}
	kernelMemCopy(tmpMenuItems, menuItems, (numMenuItems * sizeof(menuItem)));
	kernelFree(tmpMenuItems);

	kernelLog("Task menu initialized");

	// Try to load icons

	// These parameters are the same for all icons
	kernelMemClear(&params, sizeof(componentParameters));
	params.gridWidth = 1;
	params.gridHeight = 1;
	params.padLeft = 5;
	params.padRight = 5;
	params.padTop = 5;
	params.padBottom = 0;
	params.flags = (WINDOW_COMPFLAG_CUSTOMFOREGROUND |
		WINDOW_COMPFLAG_CUSTOMBACKGROUND | WINDOW_COMPFLAG_CANFOCUS);
	kernelMemCopy(&COLOR_WHITE, &params.foreground, sizeof(color));
	kernelMemCopy(&windowVariables->color.desktop, &params.background,
		sizeof(color));
	params.orientationX = orient_center;
	params.orientationY = orient_middle;

	// Loop for variables with "icon.name.*"
	for (count1 = 0; count1 < settings.numVariables; count1 ++)
	{
		if (!strncmp(settings.variables[count1], "icon.name.", 10))
		{
			iconName = (settings.variables[count1] + 10);
			kernelVariableListGet(&settings, settings.variables[count1],
				itemLabel, 128);

			// Get the rest of the recognized properties for this icon.

			// Get the image name, make sure it exists, and try to load it.
			sprintf(propertyName, "icon.%s.image", iconName);
			status = kernelVariableListGet(&settings, propertyName,
				propertyValue, 128);
			if ((status < 0) ||
				(kernelFileFind(propertyValue, NULL) < 0) ||
				(kernelImageLoad(propertyValue, 0, 0, &tmpImage) < 0))
			{
				continue;
			}

			// See if there's a command associated with this, and make sure
			// the program exists.
			sprintf(propertyName, "icon.%s.command", iconName);
			status = kernelVariableListGet(&settings, propertyName,
				propertyValue, 128);
			if ((status < 0) || (kernelLoaderCheckCommand(propertyValue) < 0))
				continue;
	
			params.gridY++;

			iconComponent =
				kernelWindowNewIcon(rootWindow, &tmpImage, itemLabel, &params);
			if (iconComponent == NULL)
				continue;

			strncpy((char *) ((kernelWindowIcon *) iconComponent->data)->command,
				propertyValue, MAX_PATH_NAME_LENGTH);

			// Register the event handler for the icon command execution
			kernelWindowRegisterEventHandler((objectKey) iconComponent,
				&iconEvent);
	
			// Release the image memory
			kernelImageFree(&tmpImage);
		}
	}

	// Snap the icons to a grid
	kernelWindowSnapIcons((objectKey) rootWindow);

	kernelDebug(debug_gui, "Rootwindow main container size %dx%d",
		rootWindow->mainContainer->width, rootWindow->mainContainer->height);

	kernelLog("Desktop icons loaded");

	status = kernelFileGetDisk(WINDOW_DEFAULT_DESKTOP_CONFIG, &configDisk);
	if ((status >= 0) && !configDisk.readOnly)
	{
		// Re-write the config file
		status = kernelConfigWrite(WINDOW_DEFAULT_DESKTOP_CONFIG, &settings);
		if (status >= 0)
			kernelLog("Updated desktop configuration");
	}

	kernelVariableListDestroy(&settings);

	initialized = 1;

	// Done.  We don't set it visible for now.
	return (rootWindow);
}


int kernelWindowShell(int priv, int login)
{
	// Launch the window shell thread

	// Check initialization
	if (!initialized)
		return (ERR_NOTINITIALIZED);

	privilege = priv;

	if (login)
		runPrograms();

	// Spawn the window shell thread
	return(kernelMultitaskerSpawn(windowShellThread, "window shell", 0, NULL));
}


void kernelWindowShellUpdateList(kernelWindow *windowList[], int numberWindows)
{
	// When the list of open windows has changed, the window environment can
	// call this function so we can update our taskbar.

	kernelWindowMenu *menu = NULL;
	kernelWindowContainer *container = NULL;
	componentParameters params;
	kernelWindowComponent *item = NULL;
	int count;

	// Check initialization
	if (!initialized)
		return;

	kernelDebug(debug_gui, "Update window list");

	if (windowMenu)
	{
		menu = windowMenu->data;
		container = menu->container->data;

		// Destroy all the menu items in the menu
		while (container->numComponents)
			kernelWindowComponentDestroy(container
				->components[container->numComponents - 1]);
		kernelMemClear(winMenuItems, (numWinMenuItems * sizeof(menuItem)));
		numWinMenuItems = 0;

		// Copy the parameters from the menu to use 
		kernelMemCopy((void *) &(windowMenu->params), &params,
			sizeof(componentParameters));

		for (count = 0; count < numberWindows; count ++)
		{
			// Skip windows we don't want to include

			if ((windowList[count] == rootWindow) ||
				!strcmp((char *) windowList[count]->title, WINNAME_TEMPCONSOLE))
			{
				continue;
			}

			// Skip dialog windows too
			if (windowList[count]->parentWindow)
				continue;

			item = kernelWindowNewMenuItem(windowMenu, (char *)
				windowList[count]->title, &params);

			kernelWindowRegisterEventHandler(item, &windowMenuEvent);

			winMenuItems[numWinMenuItems].itemComponent = item;
			winMenuItems[numWinMenuItems].window = windowList[count];
			numWinMenuItems += 1;
		}
	
		if (windowMenu->layout)
			windowMenu->layout(windowMenu);
	}
}
