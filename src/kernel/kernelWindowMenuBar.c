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
//  kernelWindowMenuBar.c
//

// This code is for managing kernelWindowMenuBar objects.
// These are just images that appear inside windows and buttons, etc

#include "kernelWindow.h"	// Our prototypes are here
#include "kernelDebug.h"
#include "kernelError.h"
#include "kernelFont.h"
#include "kernelMalloc.h"

static void (*saveMenuFocus)(kernelWindow *, int) = NULL;
static int (*saveMenuMouseEvent)(kernelWindow *, kernelWindowComponent *,
	windowEvent *) = NULL;
static int (*saveMenuKeyEvent)(kernelWindow *, kernelWindowComponent *,
	windowEvent *) = NULL;

extern kernelWindowVariables *windowVariables;


static inline int menuTitleWidth(kernelWindowComponent *component,
	kernelWindowMenuInfo *menuInfo)
{
	kernelFont *font = (kernelFont *) component->params.font;

	int width = (windowVariables->border.thickness * 4);

	if (font)
	{
		width += kernelFontGetPrintedWidth(font, (char *) component->charSet,
			(char *) menuInfo->menu->title);
	}

	return (width);
}


static inline int menuTitleHeight(kernelWindowComponent *component)
{
	kernelFont *font = (kernelFont *) component->params.font;

	int height = (windowVariables->border.thickness * 4);

	if (font)
		height += font->glyphHeight;

	return (height);
}


static void changedVisible(kernelWindowComponent *component)
{
	kernelDebug(debug_gui, "WindowMenuBar changed visible title");

	if (component->draw)
		component->draw(component);

	component->window->update(component->window, component->xCoord,
		component->yCoord, component->width, component->height);
}


static void layoutSized(kernelWindowComponent *component, int width)
{
	// Do layout for the menu bar.

	kernelWindowMenuBar *menuBar = component->data;
	kernelWindowMenuInfo *menuInfo = NULL;
	linkedListItem *iter = NULL;
	int xCoord = 0;
	int titlesWidth = 0;
	kernelWindowContainer *container = menuBar->container->data;
	int count;

	kernelDebug(debug_gui, "WindowMenuBar layoutSized width=%d", width);

	// First do the menu titles

	menuInfo = linkedListIterStart((linkedList *) &menuBar->menuList, &iter);

	while (menuInfo)
	{
		// This will leave the first menu location unchanged
		if (xCoord)
			menuInfo->xCoord = xCoord;

		menuInfo->titleWidth = menuTitleWidth(component, menuInfo);

		titlesWidth += menuInfo->titleWidth;
		xCoord = (menuInfo->xCoord + menuInfo->titleWidth);

		menuInfo = linkedListIterNext((linkedList *) &menuBar->menuList,
			&iter);
	}

	// Now lay out our container
	for (count = 0; count < container->numComponents; count ++)
	{
		container->components[count]->params.gridX =
			(container->numComponents - (count + 1));
		container->components[count]->params.gridY = 0;
		container->components[count]->params.gridWidth = 1;
		container->components[count]->params.gridHeight = 1;
		container->components[count]->params.padLeft = 0;
		container->components[count]->params.padRight = 5;
		container->components[count]->params.padTop = 0;
		container->components[count]->params.padBottom = 0;
		container->components[count]->params.orientationX = orient_center;
		container->components[count]->params.orientationY = orient_top;
	}

	if (menuBar->container->layout)
		menuBar->container->layout(menuBar->container);

	xCoord = titlesWidth;
	if (width)
		xCoord = ((width - menuBar->container->width) - 1);

	kernelDebug(debug_gui, "WindowMenuBar container xCoord=%d", xCoord);

	if (menuBar->container->xCoord != xCoord)
	{
		if (menuBar->container->move)
		{
			menuBar->container->move(menuBar->container, xCoord,
				component->yCoord);
		}

		menuBar->container->xCoord = xCoord;
		menuBar->container->yCoord = component->yCoord;
	}

	component->minWidth = (titlesWidth + menuBar->container->width);

	if (component->width < component->minWidth)
		component->width = component->minWidth;

	component->doneLayout = 1;
}


static void menuFocus(kernelWindow *menu, int focus)
{
	// We just want to know when the menu has lost focus, so we can
	// un-highlight the appropriate menu bar header

	kernelWindowComponent *menuBarComponent = menu->parentWindow->menuBar;

	kernelDebug(debug_gui, "WindowMenuBar menu %s focus",
		(focus? "got" : "lost"));

	if (saveMenuFocus)
		// Pass the event on.
		saveMenuFocus(menu, focus);

	if (!focus)
		// No longer visible
		changedVisible(menuBarComponent);
}


static int menuMouseEvent(kernelWindow *menu,
	kernelWindowComponent *itemComponent, windowEvent *event)
{
	int status = 0;
	kernelWindowComponent *menuBarComponent = menu->parentWindow->menuBar;
	kernelWindowMenuBar *menuBar = menuBarComponent->data;

	kernelDebug(debug_gui, "WindowMenuBar menu mouse event");

	if (saveMenuMouseEvent)
		// Pass the event on.
		status = saveMenuMouseEvent(menu, itemComponent, event);

	// Now determine whether the menu went away
	if (!(menu->flags & WINDOW_FLAG_HASFOCUS))
		menuBar->raisedMenu = NULL;

	return (status);
}


static int menuKeyEvent(kernelWindow *menu,
	kernelWindowComponent *itemComponent, windowEvent *event)
{
	int status = 0;
	kernelWindowComponent *menuBarComponent = menu->parentWindow->menuBar;
	kernelWindowMenuBar *menuBar = menuBarComponent->data;
	kernelWindowMenuInfo *menuInfo = NULL;
	linkedListItem *iter = NULL;
	kernelWindowMenuInfo *newMenuInfo = NULL;
	kernelWindowMenuInfo *prevMenuInfo = NULL;
	kernelWindow *newMenu = NULL;
	kernelWindowContainer *menuContainer = NULL;

	kernelDebug(debug_gui, "WindowMenuBar menu key event");

	if (saveMenuKeyEvent)
		// Pass the event on.
		status = saveMenuKeyEvent(menu, itemComponent, event);

	// Now determine whether the menu went away
	if (!(menu->flags & WINDOW_FLAG_HASFOCUS))
	{
		menuBar->raisedMenu = NULL;
		return (status);
	}

	if (event->type != WINDOW_EVENT_KEY_DOWN)
		// Not interested
		return (status);

	// If the user has pressed the left or right cursor keys, that means they
	// want to switch menus
	if ((event->key.scan == keyLeftArrow) ||
		(event->key.scan == keyRightArrow))
	{
		// Find out where the menu is in our list

		menuInfo = linkedListIterStart((linkedList *) &menuBar->menuList,
			&iter);

		while (menuInfo)
		{
			if (menuInfo->menu == menu)
			{
				if (event->key.scan == keyLeftArrow)
				{
					// Cursor left
					newMenuInfo = prevMenuInfo;
				}
				else
				{
					// Cursor right
					newMenuInfo = linkedListIterNext((linkedList *)
						&menuBar->menuList, &iter);
				}

				break;
			}

			prevMenuInfo = menuInfo;

			menuInfo = linkedListIterNext((linkedList *) &menuBar->menuList,
				&iter);
		}

		if (newMenuInfo && (newMenuInfo->menu != menu))
		{
			newMenu = newMenuInfo->menu;
			menuContainer = newMenu->mainContainer->data;

			if (menuContainer->numComponents)
			{
				kernelDebug(debug_gui, "WindowMenuBar show new menu %s",
					newMenu->title);

				// Old one is no longer visible
				kernelWindowSetVisible(menu, 0);

				newMenu->xCoord = (menu->parentWindow->xCoord +
					menuBarComponent->xCoord + newMenuInfo->xCoord);
				newMenu->yCoord = (menu->parentWindow->yCoord +
					menuBarComponent->yCoord +
					menuTitleHeight(menuBarComponent));

				// Set the new one visible
				kernelWindowSetVisible(newMenu, 1);
				menuBar->raisedMenu = newMenu;

				changedVisible(menuBarComponent);
			}
		}
	}

	return (status);
}


static int add(kernelWindowComponent *menuBarComponent, objectKey obj)
{
	// Add the supplied menu or object to the menu bar

	int status = 0;
	kernelWindowMenuBar *menuBar = menuBarComponent->data;
	kernelWindow *menu = NULL;
	kernelWindowMenuInfo *menuInfo = NULL;
	kernelWindowComponent *component = NULL;

	// If the object is a window, then we treat it as a menu.
	if (((kernelWindow *) obj)->type == windowType)
	{
		menu = obj;

		kernelDebug(debug_gui, "WindowMenuBar add menu %s", menu->title);

		// If we don't have the menu's focus(), mouseEvent(), and keyEvent()
		// function pointers saved, save them now
		if (!saveMenuFocus)
			saveMenuFocus = menu->focus;
		if (!saveMenuMouseEvent)
			saveMenuMouseEvent = menu->mouseEvent;
		if (!saveMenuKeyEvent)
			saveMenuKeyEvent = menu->keyEvent;

		menu->focus = &menuFocus;
		menu->mouseEvent = &menuMouseEvent;
		menu->keyEvent = &menuKeyEvent;

		menuInfo = kernelMalloc(sizeof(kernelWindowMenuInfo));
		if (!menuInfo)
			return (status = ERR_MEMORY);

		menuInfo->menu = menu;

		status = linkedListAdd((linkedList *) &menuBar->menuList,
			(void *) menuInfo);
		if (status < 0)
			return (status);
	}
	else
	{
		// Other things get added to our container
		component = obj;

		kernelDebug(debug_gui, "WindowMenuBar add component");

		status = kernelWindowContainerAdd(menuBar->container, component);
		if (status < 0)
			return (status);
	}

	return (status = 0);
}


static int delete(kernelWindowComponent *menuBarComponent, objectKey obj)
{
	// Deletes the supplied menu or object from the menu bar

	int status = 0;
	kernelWindowMenuBar *menuBar = menuBarComponent->data;
	kernelWindow *menu = NULL;
	kernelWindowMenuInfo *menuInfo = NULL;
	linkedListItem *iter = NULL;
	kernelWindowComponent *component = NULL;

	// If the object is a window, then we treat it as a menu.
	if (((kernelWindow *) obj)->type == windowType)
	{
		menu = obj;

		kernelDebug(debug_gui, "WindowMenuBar delete menu %s", menu->title);

		// We need to find the corresponding kernelWindowMenuInfo and delete
		// that.

		menuInfo = linkedListIterStart((linkedList *) &menuBar->menuList,
			&iter);

		while (menuInfo)
		{
			if (menuInfo->menu == menu)
			{
				status = linkedListRemove((linkedList *) &menuBar->menuList,
					(void *) menuInfo);
				if (status < 0)
					return (status);

				kernelFree((void *) menuInfo);
				break;
			}

			menuInfo = linkedListIterNext((linkedList *) &menuBar->menuList,
				&iter);
		}
	}
	else
	{
		// Other things that have been added to our container
		component = obj;

		kernelDebug(debug_gui, "WindowMenuBar delete component");

		status = kernelWindowContainerDelete(menuBar->container, component);
		if (status < 0)
			return (status);
	}

	return (status = 0);
}


static int numComps(kernelWindowComponent *component)
{
	int numItems = 0;
	kernelWindowMenuBar *menuBar = component->data;

	if (menuBar->container->numComps)
		// Count our container's components
		numItems = menuBar->container->numComps(menuBar->container);

	return (numItems);
}


static int flatten(kernelWindowComponent *component,
	kernelWindowComponent **array, int *numItems, unsigned flags)
{
	int status = 0;
	kernelWindowMenuBar *menuBar = component->data;

	if (menuBar->container->flatten)
	{
		// Flatten our container
		status = menuBar->container->flatten(menuBar->container, array,
			numItems, flags);
	}

	return (status);
}


static int layout(kernelWindowComponent *component)
{
	// Do layout for the menu bar.

	int status = 0;

	kernelDebug(debug_gui, "WindowMenuBar layout");

	layoutSized(component, component->width);

	return (status = 0);
}


static kernelWindowComponent *eventComp(kernelWindowComponent *component,
	windowEvent *event)
{
	// Determine which (if any) of our container items received the event.

	kernelWindowMenuBar *menuBar = component->data;
	kernelWindowComponent *barComponent = NULL;

	kernelDebug(debug_gui, "WindowMenuBar get event component");

	if (menuBar->container->eventComp)
		barComponent = menuBar->container->eventComp(menuBar->container,
			event);

	if (barComponent != menuBar->container)
	{
		kernelDebug(debug_gui, "WindowMenuBar found event component");
		return (barComponent);
	}

	// Nothing found.  Return the windowMenuBar component itself.
	kernelDebug(debug_gui, "WindowMenuBar return main component");
	return (component);
}


static int setBuffer(kernelWindowComponent *component, graphicBuffer *buffer)
{
	// Set the graphics buffer for the component and its subcomponents.

	int status = 0;
	kernelWindowMenuBar *menuBar = component->data;

	if (menuBar->container->setBuffer)
	{
		// Do our container
		status = menuBar->container->setBuffer(menuBar->container, buffer);
		if (status < 0)
			return (status);
	}

	menuBar->container->buffer = buffer;

	return (status = 0);
}


static int draw(kernelWindowComponent *component)
{
	// Draw the menu bar component

	int status = 0;
	kernelWindowMenuBar *menuBar = component->data;
	kernelWindowMenuInfo *menuInfo = NULL;
	linkedListItem *iter = NULL;
	kernelFont *font = (kernelFont *) component->params.font;
	kernelWindowContainer *container = menuBar->container->data;
	int count;

	kernelDebug(debug_gui, "WindowMenuBar draw '%s' menu bar",
		component->window->title);

	// Menu titles can change without our knowledge, and we don't re-layout
	// every time a new element gets added (maybe we should, but we'd still
	// have the titles problem) so we do layout every time we draw.
	layoutSized(component, component->width);

	// Draw the background of the menu bar
	kernelGraphicDrawRect(component->buffer, (color *)
		&component->params.background, draw_normal, component->xCoord,
		component->yCoord, component->width, component->height, 1, 1);

	// Loop through all the menus and draw their names on the menu bar

	menuInfo = linkedListIterStart((linkedList *) &menuBar->menuList, &iter);

	while (menuInfo)
	{
		if (menuInfo->menu->flags & WINDOW_FLAG_VISIBLE)
		{
			kernelDebug(debug_gui, "WindowMenuBar title '%s' is visible",
				menuInfo->menu->title);
			kernelGraphicDrawGradientBorder(component->buffer,
				(component->xCoord + menuInfo->xCoord), component->yCoord,
				menuInfo->titleWidth, menuTitleHeight(component),
				windowVariables->border.thickness,
				(color *) &component->params.background,
				windowVariables->border.shadingIncrement, draw_normal,
				border_all);
		}

		if (font)
		{
			kernelGraphicDrawText(component->buffer,
				(color *) &component->params.foreground,
				(color *) &component->params.background,
				font, (char *) component->charSet, (char *)
				menuInfo->menu->title, draw_normal,
				(component->xCoord + menuInfo->xCoord +
					(windowVariables->border.thickness * 2)),
				(component->yCoord + (windowVariables->border.thickness * 2)));
		}

		menuInfo = linkedListIterNext((linkedList *) &menuBar->menuList,
			&iter);
	}

	// Draw any components in our container
	for (count = 0; count < container->numComponents; count ++)
	{
		if ((container->components[count]->flags &
				WINDOW_COMP_FLAG_VISIBLE) &&
			(container->components[count]->draw))
		{
			container->components[count]->draw(container->components[count]);
		}
	}

	return (status);
}


static int move(kernelWindowComponent *component, int xCoord, int yCoord)
{
	kernelWindowMenuBar *menuBar = component->data;

	kernelDebug(debug_gui, "WindowMenuBar move oldX %d, oldY %d, newX %d, "
		"newY %d (%s%d, %s%d)", component->xCoord, component->yCoord, xCoord,
		yCoord, ((xCoord >= component->xCoord)? "+" : "-"),
		(xCoord - component->xCoord),
		((yCoord >= component->yCoord)? "+" : "-"),
		(yCoord - component->yCoord));

	xCoord += ((component->width - menuBar->container->width) - 1);

	// Move our container
	if ((menuBar->container->xCoord != xCoord) ||
		(menuBar->container->yCoord != yCoord))
	{
		if (menuBar->container->move)
			menuBar->container->move(menuBar->container, xCoord, yCoord);

		menuBar->container->xCoord = xCoord;
		menuBar->container->yCoord = yCoord;
	}

	return (0);
}


static int resize(kernelWindowComponent *component, int width,
	int height __attribute__((unused)))
{
	kernelWindowMenuBar *menuBar = component->data;
	int xCoord = 0;

	kernelDebug(debug_gui, "WindowMenuBar resize oldWidth %d, oldHeight %d, "
		"width %d, height %d", component->width, component->height,
		width, height);

	xCoord = (component->xCoord + ((width - menuBar->container->width) - 1));

	if (menuBar->container->xCoord != xCoord)
	{
		if (menuBar->container->move)
		{
			menuBar->container->move(menuBar->container, xCoord,
				component->yCoord);
		}

		menuBar->container->xCoord = xCoord;
	}

	return (0);
}


static int focus(kernelWindowComponent *component, int got)
{
	// We just want to know if we lost the focus, because in that case,
	// any raised menu will have gone away.

	kernelWindowMenuBar *menuBar = component->data;

	kernelDebug(debug_gui, "WindowMenuBar %s focus", (got? "got" : "lost"));

	if (!got)
	{
		menuBar->raisedMenu = NULL;
		changedVisible(component);
	}

	return (0);
}


static int mouseEvent(kernelWindowComponent *component, windowEvent *event)
{
	kernelWindowMenuBar *menuBar = component->data;
	kernelWindowMenuInfo *menuInfo = NULL;
	linkedListItem *iter = NULL;
	kernelWindow *menu = NULL;
	kernelWindowContainer *menuContainer = NULL;
	int xCoord = 0, width = 0;

	// Events other than left mouse down are not interesting
	if (event->type != WINDOW_EVENT_MOUSE_LEFTDOWN)
		return (0);

	kernelDebug(debug_gui, "WindowMenuBar mouse event");

	// Determine whether to set a menu visible now by figuring out whether a
	// menu title was clicked.

	menuInfo = linkedListIterStart((linkedList *) &menuBar->menuList, &iter);

	while (menuInfo)
	{
		menu = menuInfo->menu;
		menuContainer = menu->mainContainer->data;

		xCoord = (component->window->xCoord + component->xCoord +
			menuInfo->xCoord);
		width = menuInfo->titleWidth;

		if ((event->coord.x >= xCoord) && (event->coord.x < (xCoord + width)))
		{
			if (menu != menuBar->raisedMenu)
			{
				// The menu was not previously raised, so we will show it.
				kernelDebug(debug_gui, "WindowMenuBar show menu '%s'",
					menu->title);

				menu->xCoord = xCoord;
				menu->yCoord = (component->window->yCoord +
					component->yCoord + menuTitleHeight(component));

				if (menuContainer->numComponents)
					kernelWindowSetVisible(menu, 1);

				menuBar->raisedMenu = menu;
			}
			else
			{
				// The menu was previously visible, so we won't re-show it.
				kernelDebug(debug_gui, "WindowMenuBar menu '%s' re-clicked",
					menu->title);

				menuBar->raisedMenu = NULL;
			}

			changedVisible(component);
			break;
		}

		menuInfo = linkedListIterNext((linkedList *) &menuBar->menuList,
			&iter);
	}

	return (0);
}


static int destroy(kernelWindowComponent *component)
{
	kernelWindowMenuBar *menuBar = component->data;
	kernelWindowMenuInfo *menuInfo = NULL;
	linkedListItem *iter = NULL;

	kernelDebug(debug_gui, "WindowMenuBar destroy");

	if (component->window && (component->window->menuBar == component))
		component->window->menuBar = NULL;

	// Release all our memory
	if (menuBar)
	{
		menuInfo = linkedListIterStart((linkedList *) &menuBar->menuList,
			&iter);

		while (menuInfo)
		{
			kernelFree((void *) menuInfo);
			menuInfo = linkedListIterNext((linkedList *) &menuBar->menuList,
				&iter);
		}

		if (menuBar->container)
			kernelWindowComponentDestroy(menuBar->container);

		kernelFree(component->data);
		component->data = NULL;
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

kernelWindowComponent *kernelWindowNewMenuBar(kernelWindow *window,
	componentParameters *params)
{
	// Formats a kernelWindowComponent as a kernelWindowMenuBar

	kernelWindowComponent *component = NULL;
	kernelWindowMenuBar *menuBar = NULL;

	// Check params
	if (!window || !params)
	{
		kernelError(kernel_error, "NULL parameter");
		return (component = NULL);
	}

	if (window->type != windowType)
	{
		kernelError(kernel_error, "Menu bars can only be added to windows");
		return (component = NULL);
	}

	// Get the basic component structure
	component = kernelWindowComponentNew(window->sysContainer, params);
	if (!component)
		return (component);

	component->type = menuBarComponentType;
	component->subType = containerComponentType;
	component->flags |= WINDOW_COMP_FLAG_CANFOCUS;
	// Only want this to be resizable horizontally
	component->flags &= ~WINDOW_COMP_FLAG_RESIZABLEY;

	// Set the functions
	component->add = &add;
	component->delete = &delete;
	component->numComps = &numComps;
	component->flatten = &flatten;
	component->layout = &layout;
	component->eventComp = &eventComp;
	component->setBuffer = &setBuffer;
	component->draw = &draw;
	component->move = &move;
	component->resize = &resize;
	component->focus = &focus;
	component->mouseEvent = &mouseEvent;
	component->destroy = &destroy;

	// If font is NULL, use the default
	if (!component->params.font)
		component->params.font = windowVariables->font.varWidth.small.font;

	// Get memory for this menu bar component
	menuBar = kernelMalloc(sizeof(kernelWindowMenuBar));
	if (!menuBar)
	{
		kernelWindowComponentDestroy(component);
		return (component = NULL);
	}

	// Get our container component
	menuBar->container = kernelWindowNewContainer(window, "windowmenubar "
		"container", params);
	if (!menuBar->container)
	{
		kernelWindowComponentDestroy(component);
		return (component = NULL);
	}

	// Remove it from the parent container
	kernelWindowContainerDelete(menuBar->container->container,
		menuBar->container);

	component->data = (void *) menuBar;

	component->height = component->minHeight = menuTitleHeight(component);

	window->menuBar = component;

	return (component);
}

