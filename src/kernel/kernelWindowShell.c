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
//  kernelWindowShell.c
//

// This is the code that manages the 'root' window in the GUI environment.

#include "kernelWindow.h"
#include "kernelParameters.h"
#include "kernelMultitasker.h"
#include "kernelMemory.h"
#include "kernelMalloc.h"
#include "kernelMisc.h"
#include "kernelLoader.h"
#include "kernelWindowEventStream.h"
#include "kernelLog.h"
#include "kernelError.h"
#include <string.h>
#include <stdio.h>

typedef struct {
  kernelWindowComponent *itemComponent;
  char command[MAX_PATH_NAME_LENGTH];
  kernelWindow *window;
} windowMenuItem;

static kernelWindow *rootWindow = NULL;
static kernelWindowComponent *taskMenuBar = NULL;
static kernelWindowComponent *windowMenu = NULL;
static windowMenuItem *menuItems = NULL;
static int numberMenuItems = 0;
static windowMenuItem *windowMenuItems = NULL;
static int privilege = 0;


static void iconEvent(objectKey componentData, windowEvent *event)
{
  int status = 0;
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindowIcon *iconComponent = (kernelWindowIcon *) component->data;
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

      kernelMouseBusy(1);

      // Run the command
      status =
	kernelLoaderLoadAndExec((const char *) iconComponent->command,
				privilege, 0 /* no block */);
      kernelMouseBusy(0);
      
      if (status < 0)
	kernelError(kernel_error, "Unable to execute program %s",
		    iconComponent->command);
    }

  return;
}


static void menuEvent(objectKey componentData, windowEvent *event)
{
  int status = 0;
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  int count;

  if (event->type & EVENT_SELECTION)
    {
      for (count = 0; count < numberMenuItems; count ++)
	{
	  if (menuItems[count].itemComponent == component)
	    {
	      if (menuItems[count].command[0] != NULL)
		{
		  kernelMouseBusy(1);

		  // Run the command, no block
		  status = kernelLoaderLoadAndExec(menuItems[count].command,
						   privilege, 0);

		  kernelMouseBusy(0);

		  if (status < 0)
		    kernelError(kernel_error, "Unable to execute program %s",
				menuItems[count].command);
		}
	    }
	}
    }

  return;
}


static void windowMenuEvent(objectKey componentData, windowEvent *event)
{
  char command[MAX_PATH_NAME_LENGTH];
  kernelWindowMenu *menu = NULL;
  int count;

  command[0] = NULL;

  if (windowMenu && (event->type & EVENT_SELECTION))
    {
      menu = (kernelWindowMenu *) windowMenu->data;
	  
      // See if this is one of our window menu components
      for (count = 0; count < menu->numComponents; count ++)
	{
	  if (componentData == menu->components[count])
	    {
	      // Restore it
	      kernelWindowSetMinimized(windowMenuItems[count].window, 0);
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
  if (kernelConfigurationReader(WINDOW_MANAGER_DEFAULT_CONFIG, &settings) < 0)
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
      if (kernelWindowEventStreamRead(&(component->events), &event) > 0)
	{
	  if (component->eventHandler)
	    component->eventHandler((objectKey) component, &event);
	}

      // If this component is a container type, recurse
      if (component->type == containerComponentType)
	scanContainerEvents((kernelWindowContainer *) component->data);
    }
}


static void windowShellThread(void)
{
  // This thread runs as the 'window shell' to watch for window events on
  // 'root window' GUI components, and which functions as the user's login
  // shell in graphics mode.

  // Run programs
  runPrograms();
  
  while(1)
    {
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


kernelWindow *kernelWindowMakeRoot(variableList *settings)
{
  // Make a graphic buffer for the whole background

  int status = 0;
  windowMenuItem *tmpMenuItems = NULL;
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
  int count1, count2;

  // We get default colors from here
  extern color kernelDefaultDesktop;

  // Check params
  if (settings == NULL)
    return (rootWindow = NULL);

  // Get a new window
  rootWindow = kernelWindowNew(KERNELPROCID, WINNAME_ROOTWINDOW);
  if (rootWindow == NULL)
    return (rootWindow);

  // The window will have no border, title bar or close button, is not
  // movable or resizable, and is packed
  rootWindow->flags &= ~(WINFLAG_MOVABLE | WINFLAG_RESIZABLE);
  kernelWindowSetHasTitleBar(rootWindow, 0);
  kernelWindowSetHasBorder(rootWindow, 0);

  status = kernelWindowSetSize(rootWindow, kernelGraphicGetScreenWidth(),
			       kernelGraphicGetScreenHeight());
  if (status < 0)
    return (rootWindow = NULL);

  status = kernelWindowSetLocation(rootWindow, 0, 0);
  if (status < 0)
    return (rootWindow = NULL);

  // The window is always at the bottom level
  rootWindow->level = WINDOW_MAXWINDOWS;

  // Set our background color preference
  rootWindow->background.red = kernelDefaultDesktop.red;
  rootWindow->background.green = kernelDefaultDesktop.green;
  rootWindow->background.blue = kernelDefaultDesktop.blue;

  // Try to load the background image
  if (!kernelVariableListGet(settings, "background.image", propertyValue,
			     128) && strncmp(propertyValue, "", 128))
    {
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

      if (tmpImage.data)
	{
	  // Release the image memory
	  kernelMemoryRelease(tmpImage.data);
	  tmpImage.data = NULL;
	}
    }

  // Make a task menu at the top
  kernelMemClear(&params, sizeof(componentParameters));
  params.gridWidth = 256;
  params.gridHeight = 1;
  params.orientationX = orient_left;
  params.orientationY = orient_top;
  params.foreground.red = 255;
  params.foreground.green = 255;
  params.foreground.blue = 255;
  params.background.red = 40;
  params.background.green = 93;
  params.background.blue = 171;
  params.flags |=
    (WINDOW_COMPFLAG_CUSTOMFOREGROUND | WINDOW_COMPFLAG_CUSTOMBACKGROUND);
  kernelFontLoad(WINDOW_DEFAULT_VARFONT_MEDIUM_FILE,
		 WINDOW_DEFAULT_VARFONT_MEDIUM_NAME,
		 (kernelAsciiFont **) &(params.font), 0);
  taskMenuBar = kernelWindowNewMenuBar(rootWindow, &params);
  params.padLeft = 10;
  params.padRight = 10;

  // Try to load taskbar menus and menu items
      
  // Allocate temporary memory for the normal menu items
  tmpMenuItems = kernelMalloc(256 * sizeof(windowMenuItem)); 
  if (tmpMenuItems == NULL)
    return (rootWindow = NULL);

  // Loop for variables with "taskBar.menu.*"
  for (count1 = 0; count1 < settings->numVariables; count1 ++)
    {
      if (!strncmp(settings->variables[count1], "taskBar.menu.", 13))
	{
	  menuName = (settings->variables[count1] + 13);
	  kernelVariableListGet(settings, settings->variables[count1],
				menuLabel, 128);

	  menuComponent =
	    kernelWindowNewMenu(taskMenuBar, menuLabel, &params);

	  // Now loop and get any components for this menu
	  for (count2 = 0; count2 < settings->numVariables; count2 ++)
	    {
	      sprintf(propertyName, "taskBar.%s.item.", menuName);

	      if (!strncmp(settings->variables[count2], propertyName,
			   strlen(propertyName)))
		{
		  itemName =
		    (settings->variables[count2] + strlen(propertyName));
		  kernelVariableListGet(settings,
					settings->variables[count2],
					itemLabel, 128);

		  tmpMenuItems[numberMenuItems].itemComponent =
		    kernelWindowNewMenuItem(menuComponent, itemLabel,
					    &params);
		  kernelWindowRegisterEventHandler((void *)
				   tmpMenuItems[numberMenuItems].itemComponent,
						   &menuEvent);

		  // See if there's an associated command
		  sprintf(propertyName, "taskBar.%s.%s.command", menuName,
			  itemName);
		  kernelVariableListGet(settings, propertyName,
					(tmpMenuItems[numberMenuItems]
					 .command), MAX_PATH_NAME_LENGTH);

		  numberMenuItems += 1;
		}
	    }

	  // We treat any "window" menu specially, since it is not usually
	  // populated at startup time, only as windows are created or
	  // destroyed.
	  if (!strcmp(menuName, "window"))
	    {
	      windowMenu = menuComponent;

	      // Get memory for the window menu items
	      windowMenuItems =
		kernelMalloc(WINDOW_MAXWINDOWS * sizeof(windowMenuItem));
	      if (windowMenuItems == NULL)
		{
		  kernelFree(tmpMenuItems);
		  return (rootWindow = NULL);
		}
	    }
	}
    }

  // Free our temporary menu item memory and just allocate the amount we
  // actually need.
  menuItems = kernelMalloc(numberMenuItems * sizeof(windowMenuItem)); 
  if (menuItems == NULL)
    return (rootWindow = NULL);
  kernelMemCopy(tmpMenuItems, menuItems,
		(numberMenuItems * sizeof(windowMenuItem)));
  kernelFree(tmpMenuItems);

  kernelLog("Task menu initialized");

  // Try to load icons

  // These parameters are the same for all icons
  kernelMemClear(&params, sizeof(componentParameters));
  params.gridX = 0;
  params.gridWidth = 1;
  params.gridHeight = 1;
  params.padLeft = 5;
  params.padRight = 5;
  params.padTop = 5;
  params.padBottom = 0;
  params.orientationX = orient_center;
  params.orientationY = orient_top;

  // Loop for variables with "icon.name.*"
  for (count1 = 0, count2 = 1; count1 < settings->numVariables; count1 ++)
    {
      if (!strncmp(settings->variables[count1], "icon.name.", 10))
	{
	  iconName = (settings->variables[count1] + 10);
	  kernelVariableListGet(settings, settings->variables[count1],
				itemLabel, 128);

	  // Get the rest of the recognized properties for this icon.
	  sprintf(propertyName, "icon.%s.image", iconName);
	  kernelVariableListGet(settings, propertyName, propertyValue,
				128);

	  status = kernelImageLoad(propertyValue, 0, 0, &tmpImage);
	  if (status == 0)
	    {
	      params.gridY = count2++;
		  
	      iconComponent = kernelWindowNewIcon(rootWindow, &tmpImage,
						  itemLabel, &params);
	      if (iconComponent == NULL)
		continue;

	      // See if there's a command associated with this.
	      sprintf(propertyName, "icon.%s.command", iconName);
	      kernelVariableListGet(settings, propertyName, (char *)
				    ((kernelWindowIcon *)
				     iconComponent->data)->command,
				    MAX_PATH_NAME_LENGTH);

	      // Register the event handler for the icon command execution
	      kernelWindowRegisterEventHandler((objectKey) iconComponent,
					       &iconEvent);
	  
	      // Release the image memory
	      kernelMemoryRelease(tmpImage.data);
	      tmpImage.data = NULL;
	    }
	}
    }

  // Snap the icons to a grid
  kernelWindowSnapIcons((void *) rootWindow);

  kernelLog("Desktop icons loaded");

  // Done.  We don't set it visible for now.
  return (rootWindow);
}


int kernelWindowShell(int priv)
{
  // Launch the window shell thread

  // Check initialization
  if (rootWindow == NULL)
    {
      kernelError(kernel_error, "Can't start window shell without root "
		  "window!");
      return (ERR_NOTINITIALIZED);
    }

  privilege = priv;
  
  // Spawn the window shell thread
  return(kernelMultitaskerSpawn(windowShellThread, "window shell", 0, NULL));
}


void kernelWindowShellUpdateList(kernelWindow *windowList[], int numberWindows)
{
  // When the list of open windows has changed, the window environment can
  // call this function so we can update our taskbar.

  kernelWindowMenu *menu = NULL;
  componentParameters params;
  kernelWindowComponent *menuItem = NULL;
  int numMenuItems = 0;
  int menuVisible = 0;
  int count;

  // Check initialization
  if (rootWindow == NULL)
    return;

  if (windowMenu)
    {
      menu = (kernelWindowMenu *) windowMenu->data;

      if (windowMenu->flags & WINFLAG_VISIBLE)
	{
	  kernelWindowComponentSetVisible(windowMenu, 0);
	  menuVisible = 1;
	}

      while (menu->numComponents)
	kernelWindowComponentDestroy(menu
				     ->components[menu->numComponents - 1]);

      kernelMemClear(windowMenuItems, (WINDOW_MAXWINDOWS *
				       sizeof(windowMenuItem)));

      kernelMemCopy((void *) &(windowMenu->parameters), &params,
		    sizeof(componentParameters));

      for (count = 0; count < numberWindows; count ++)
	{
	  // Skip a couple of system windows we don't want to include
	  if (windowList[count] == rootWindow)
	    continue;
	  if (!strcmp((char *) windowList[count]->title, WINNAME_TEMPCONSOLE))
	    continue;

	  // Skip dialog windows
	  if (windowList[count]->parentWindow)
	    continue;

	  params.gridX = numMenuItems;
	  menuItem =
	    kernelWindowNewMenuItem(windowMenu,
				    (char *) windowList[count]->title,
				    &params);
	  kernelWindowRegisterEventHandler((objectKey) menuItem,
					   &windowMenuEvent);
	  windowMenuItems[numMenuItems].itemComponent = menuItem;
	  windowMenuItems[numMenuItems].window = windowList[count];
	  numMenuItems += 1;
	}
	  
      if (menu->containerLayout)
	menu->containerLayout(windowMenu);

      if (menuVisible)
	kernelWindowComponentSetVisible(windowMenu, 1);
    }
}
