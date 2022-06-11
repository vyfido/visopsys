//
//  Visopsys
//  Copyright (C) 1998-2020 J. Andrew McLaughlin
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
//  kernelWindowMenu.c
//

// This code is for managing kernelWindowMenu objects.  These are a special
// class of windows, which are filled with kernelWindowMenuItems

#include "kernelWindow.h"	// Our prototypes are here
#include "kernelDebug.h"
#include "kernelError.h"
#include "kernelWindowEventStream.h"
#include <string.h>

static void (*saveFocus)(kernelWindow *, int) = NULL;

extern kernelWindowVariables *windowVariables;


static int findSelected(kernelWindow *menu)
{
	kernelWindowContainer *container = menu->mainContainer->data;
	kernelWindowComponent *menuItemComponent = NULL;
	kernelWindowMenuItem *menuItem = NULL;
	int count;

	for (count = 0; count < container->numComponents; count ++)
	{
		menuItemComponent = container->components[count];
		menuItem = menuItemComponent->data;

		if (menuItem->selected)
			return (count);
	}

	// Not found
	return (ERR_NOSUCHENTRY);
}


static void focus(kernelWindow *menu, int got)
{
	kernelWindowContainer *container = menu->mainContainer->data;
	kernelWindowComponent *itemComponent = NULL;
	int selected = 0;
	int count;

	kernelDebug(debug_gui, "WindowMenu %s focus", (got? "got" : "lost"));

	if (saveFocus)
		// Pass the event on.
		saveFocus(menu, got);

	if (got)
	{
		// Set the width of all menu items to the width of the menu
		for (count = 0; count < container->numComponents; count ++)
		{
			itemComponent = container->components[count];

			if (itemComponent->width != menu->mainContainer->width)
			{
				kernelWindowComponentSetWidth(itemComponent,
					menu->mainContainer->width);
			}
		}
	}
	else
	{
		// If any menu item is currently selected, de-select it.
		if ((selected = findSelected(menu)) >= 0)
		{
			itemComponent = container->components[selected];
			if (itemComponent->setSelected)
				itemComponent->setSelected(itemComponent, 0);
		}

		// No longer visible
		kernelWindowSetVisible(menu, 0);
	}
}


static int mouseEvent(kernelWindow *menu, kernelWindowComponent *component,
	windowEvent *event)
{
	windowEvent tmpEvent;

	kernelDebug(debug_gui, "WindowMenu mouseEvent");

	// We don't care about anything other than left-button events
	if (!(event->type & WINDOW_EVENT_MOUSE_LEFT))
		return (0);

	// We only care about clicks in our menu items
	if (component->type != listItemComponentType)
		return (0);

	if (component && (component->flags & WINDOW_COMP_FLAG_VISIBLE) &&
		(component->flags & WINDOW_COMP_FLAG_ENABLED))
	{
		kernelDebug(debug_gui, "WindowMenu clicked item %s",
			((kernelWindowMenuItem *) component->data)->params.text);

		if (event->type & WINDOW_EVENT_MOUSE_LEFTUP)
		{
			memcpy(&tmpEvent, event, sizeof(windowEvent));

			// Make this also a 'selection' event
			tmpEvent.type |= WINDOW_EVENT_SELECTION;

			// Adjust to the coordinates of the component
			tmpEvent.coord.x -= (menu->xCoord + component->xCoord);
			tmpEvent.coord.y -= (menu->yCoord + component->yCoord);

			// Copy the event into the event stream of the menu item
			kernelWindowEventStreamWrite(&component->events, &tmpEvent);

			// Tell the menu item not to show selected any more
			component->setSelected(component, 0);
		}
	}

	if (!(event->type & WINDOW_EVENT_MOUSE_DOWN))
		// No longer visible
		kernelWindowSetVisible(menu, 0);

	return (0);
}


static int keyEvent(kernelWindow *menu, kernelWindowComponent *itemComponent,
	windowEvent *event)
{
	// If the user presses the up/down cursor keys when the menu is in focus,
	// we use it to select menu items.

	kernelWindowContainer *container = menu->mainContainer->data;
	int oldSelected = findSelected(menu);
	int tmpSelected = oldSelected;
	int newSelected = oldSelected;
	windowEvent tmpEvent;
	int count;

	kernelDebug(debug_gui, "WindowMenu keyEvent");

	if (event->type != WINDOW_EVENT_KEY_DOWN)
		// Not interested
		return (0);

	if ((event->key.scan == keyUpArrow) || (event->key.scan == keyDownArrow))
	{
		// Find the next thing to select.
		for (count = 0; count < container->numComponents; count ++)
		{
			if (event->key.scan == keyUpArrow)
			{
				// Cursor up
				if (!tmpSelected)
				{
					newSelected = -1;
					break;
				}
				else if (tmpSelected < 0)
				{
					tmpSelected = (container->numComponents - 1);
				}
				else
				{
					tmpSelected -= 1;
				}
			}
			else
			{
				// Cursor down
				if ((tmpSelected < 0) ||
					(tmpSelected >= (container->numComponents - 1)))
				{
					tmpSelected = 0;
				}
				else
				{
					tmpSelected += 1;
				}
			}

			itemComponent = container->components[tmpSelected];

			if ((itemComponent->flags & WINDOW_COMP_FLAG_VISIBLE) &&
				(itemComponent->flags & WINDOW_COMP_FLAG_ENABLED))
			{
				kernelDebug(debug_gui, "WindowMenu selected item %s",
					((kernelWindowMenuItem *) itemComponent->data)
						->params.text);
				newSelected = tmpSelected;
				break;
			}
		}

		if (newSelected != oldSelected)
		{
			if (oldSelected >= 0)
			{
				// De-select the old one
				itemComponent = container->components[oldSelected];
				if (itemComponent->setSelected)
					itemComponent->setSelected(itemComponent, 0);
			}

			if (newSelected >= 0)
			{
				// Select the new one
				itemComponent = container->components[newSelected];
				if (itemComponent->setSelected)
					itemComponent->setSelected(itemComponent, 1);
			}
		}
	}

	else if (event->key.scan == keyEnter)
	{
		// ENTER.  Is any item currently selected?
		if (oldSelected >= 0)
		{
			itemComponent = container->components[oldSelected];

			memcpy(&tmpEvent, event, sizeof(windowEvent));

			// Make this also a 'selection' event
			tmpEvent.type |= WINDOW_EVENT_SELECTION;

			// Copy the event into the event stream of the menu item
			kernelWindowEventStreamWrite(&itemComponent->events, &tmpEvent);

			// Tell the menu item not to show selected any more
			itemComponent->setSelected(itemComponent, 0);
		}

		// No longer visible
		kernelWindowSetVisible(menu, 0);
	}

	else if (event->key.scan == keyEsc)
	{
		// No longer visible
		kernelWindowSetVisible(menu, 0);
	}

	return (0);
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////

kernelWindow *kernelWindowNewMenu(kernelWindow *parentWindow,
	kernelWindowComponent *menuBarComponent, const char *name,
	windowMenuContents *contents, componentParameters *params)
{
	// Formats a kernelWindow as a kernelWindowMenu

	kernelWindow *menu = NULL;
	int count;

	// Check params.  It's okay for 'parentWindow', 'menuBarComponent', or
	// 'contents' to be NULL.
	if (!name || !params)
	{
		kernelError(kernel_error, "NULL parameter");
		return (menu = NULL);
	}

	// Get the basic child window
	menu = kernelWindowNewChild(parentWindow, name);
	if (!menu)
		return (menu);

	// Remove title bar
	kernelWindowSetHasTitleBar(menu, 0);

	// Make it not resizable
	kernelWindowSetResizable(menu, 0);

	// Any custom colours?
	if (params->flags & COMP_PARAMS_FLAG_CUSTOMBACKGROUND)
		kernelWindowSetBackgroundColor(menu, &params->background);

	if (contents)
	{
		// Loop through the contents structure, adding menu items
		for (count = 0; count < contents->numItems; count ++)
		{
			contents->items[count].key = (objectKey)
				kernelWindowNewMenuItem(menu, contents->items[count].text,
					params);
			if (!contents->items[count].key)
			{
				kernelWindowDestroy(menu);
				return (menu = NULL);
			}
		}
	}

	// If we don't have the menu's focus() function pointer saved, save it now
	if (!saveFocus)
		saveFocus = menu->focus;

	menu->focus = &focus;
	menu->mouseEvent = &mouseEvent;
	menu->keyEvent = &keyEvent;

	// If the menu will be part of a menuBar, add it
	if (menuBarComponent)
	{
		if (kernelWindowContainerAdd(menuBarComponent, (objectKey) menu) >= 0)
			menu->parentMenuBar = menuBarComponent;
	}

	return (menu);
}


int kernelWindowMenuUpdate(kernelWindow *menu, const char *name,
	const char *charSet, windowMenuContents *contents,
	componentParameters *params)
{
	// Update a menu's name, character set, contents, or parameters.  All are
	// optional.

	int status = 0;
	int count;

	// Check params.  It's okay for 'name', 'charSet', 'contents', or 'params'
	// to be NULL.
	if (!menu)
	{
		kernelError(kernel_error, "NULL parameter");
		return (status = ERR_NULLPARAMETER);
	}

	if (name)
		kernelWindowSetTitle(menu, name);

	if (charSet)
		kernelWindowSetCharSet(menu, charSet);

	if (contents)
	{
		for (count = 0; count < contents->numItems; count ++)
		{
			if (contents->items[count].key)
			{
				kernelWindowComponentSetData(contents->items[count].key,
					contents->items[count].text,
					strlen(contents->items[count].text),
					(count == (contents->numItems - 1)));
			}
		}

		kernelWindowSetResizable(menu, 1);

		kernelWindowLayout(menu);

		kernelWindowSetResizable(menu, 0);
	}

	if (params)
	{
		// Any custom colours?
		if (params->flags & COMP_PARAMS_FLAG_CUSTOMBACKGROUND)
			kernelWindowSetBackgroundColor(menu, &params->background);
	}

	return (status = 0);
}

int kernelWindowMenuDestroy(kernelWindow *menu)
{
	// Menus are special instances of windows.  They can be destroyed using
	// the usual function kernelWindowDestroy(), but this function can be used
	// to ensure that they are first removed from any menu bar.

	int status = 0;

	// Check params
	if (!menu)
	{
		kernelError(kernel_error, "NULL parameter");
		return (status = ERR_NULLPARAMETER);
	}

	if (menu->parentMenuBar)
		kernelWindowContainerDelete(menu->parentMenuBar, menu);

	return (status = kernelWindowDestroy(menu));
}

