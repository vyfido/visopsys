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
//  kernelWindowMenu.c
//

// This code is for managing kernelWindowMenu objects.  These are subclasses
// of containers, which are filled with kernelWindowMenuItems

#include "kernelWindow.h"     // Our prototypes are here
#include "kernelDebug.h"
#include "kernelError.h"
#include "kernelMalloc.h"
#include "kernelWindowEventStream.h"
#include <stdlib.h>
#include <string.h>

static int borderThickness = 3;
static int borderShadingIncrement = 15;


static int getGraphicBuffer(kernelWindowComponent *component)
{
  // Gets a new graphic buffer for the menu, appropriate for its size.  Also
  // deallocates any previous buffer memory, if applicable.

  int status = 0;
  kernelWindowMenu *menu = NULL;
  kernelWindowContainer *container = NULL;

  if (component->type != menuComponentType)
    {
      kernelError(kernel_error, "Component is not a menu");
      return (status = ERR_INVALID);
    }

  menu = component->data;
  container = menu->container->data;

  // Free any existing buffer
  if (menu->buffer.data)
    {
      kernelFree(menu->buffer.data);
      menu->buffer.data = NULL;
    }
  
  menu->buffer.width = component->width;
  menu->buffer.height = component->height;

  if (component->width && component->height)
    {
      menu->buffer.data =
	kernelMalloc(kernelGraphicCalculateAreaBytes(menu->buffer.width,
						     menu->buffer.height));
      if (menu->buffer.data == NULL)
	return (status = ERR_MEMORY);
    }

  if (container->containerSetBuffer)
    container->containerSetBuffer(menu->container, &(menu->buffer));

  return (status = 0);
}


static int findSelected(kernelWindowMenu *menu)
{
  kernelWindowComponent *menuItemComponent = NULL;
  kernelWindowContainer *container = menu->container->data;
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


static int draw(kernelWindowComponent *component)
{
  kernelWindowMenu *menu = component->data;
  kernelWindowContainer *container = menu->container->data;
  int selected = 0;
  int count;

  if (container->numComponents)
    {
      kernelDebug(debug_gui, "menu \"%s\" draw", container->name);

      // De-select any previously-selected menu items
      selected = findSelected(menu);
      if (selected >= 0)
	{
	  if (container->components[selected]->setSelected)
	    container->components[selected]
	      ->setSelected(container->components[selected], 0);
	}

      // Draw the background of the menu
      kernelGraphicDrawRect(component->buffer,
			    (color *) &(component->parameters.background),
			    draw_normal, borderThickness, borderThickness,
			    (component->width - (borderThickness * 2)),
			    (component->height - (borderThickness * 2)), 1, 1);

      kernelGraphicDrawGradientBorder(component->buffer, 0, 0,
				      component->width, component->height,
				      borderThickness, (color *)
				      &(component->parameters.background),
				      borderShadingIncrement,
				      draw_normal, border_all);

      // Draw all the menu items
      for (count = 0; count < container->numComponents; count ++)
	if (container->components[count]->draw)
	  container->components[count]->draw(container->components[count]);

      kernelGraphicRenderBuffer(component->buffer,
				(component->window->xCoord +
				 component->xCoord),
				(component->window->yCoord +
				 component->yCoord), 0, 0,
				component->width, component->height);
    }

  kernelWindowComponentFocus(component);

  return (0);
}


static int erase(kernelWindowComponent *component)
{
  kernelWindowRedrawArea((component->window->xCoord + component->xCoord),
			 (component->window->yCoord + component->yCoord),
			 component->width, component->height);
  return (0);
}


static int focus(kernelWindowComponent *component, int yesNo)
{
  // We just want to know when we've lost the focus, so we can make the
  // menu disappear

  kernelDebug(debug_gui, "menu %s %s focus",
	      ((kernelWindowContainer *) ((kernelWindowMenu *) component->data)
	       ->container->data)->name, (yesNo? "got" : "lost"));

  if (!yesNo)
    {
      // This is a little hack to short-circuit an endless loop, since
      // the following 'set visible' call would end up calling this function
      component->flags &= ~WINFLAG_HASFOCUS;
      component->window->focusComponent = NULL;
      
      kernelWindowComponentSetVisible(component, 0);
    }

  return (0);
}


static int getData(kernelWindowComponent *component, void *buffer,
		   int size __attribute__((unused)))
{
  kernelWindowMenu *menu = component->data;
  kernelWindowContainer *container = menu->container->data;
  objectKey *key = buffer;
  int selected = -1;

  selected = findSelected(menu);
  if (selected < 0)
    {
      *key = NULL;
      return (selected);
    }

  *key = container->components[selected];

  return (0);
}


static int getSelected(kernelWindowComponent *component, int *selected)
{
  *selected = findSelected(component->data);
  
  if (*selected < 0)
    return (*selected);
  else
    return (0);
}


static int mouseEvent(kernelWindowComponent *component, windowEvent *event)
{
  // When mouse events happen to menu components, we pass them on to the
  // appropriate kernelWindowMenuItem component

  kernelWindowMenu *menu = component->data;
  kernelWindowContainer *container = menu->container->data;
  kernelWindowComponent *clickedComponent = NULL;
  kernelWindowMenuItem *clickedItem = NULL;
  int count;

  kernelDebug(debug_gui, "menu mouseEvent");

  // Figure out which menu item was clicked based on the coordinates
  // of the event
  int tmp =
    (event->yPosition - (component->window->yCoord + component->yCoord));

  for (count = 0; count < container->numComponents; count ++)
    {
      if ((tmp >= container->components[count]->yCoord) &&
	  (tmp < (container->components[count]->yCoord +
		  container->components[count]->height)))
	{
	  clickedComponent = container->components[count];
	  clickedItem = clickedComponent->data;
	  break;
	}
    }

  // Is there an item in this space?
  if (clickedComponent && (clickedComponent->flags & WINFLAG_VISIBLE) &&
      (clickedComponent->flags & WINFLAG_ENABLED))
    {
      kernelDebug(debug_gui, "menu clicked item %s", clickedItem->params.text);

      if (clickedComponent->mouseEvent)
	clickedComponent->mouseEvent(clickedComponent, event);

      kernelGraphicRenderBuffer(component->buffer,
				(component->window->xCoord +
				 component->xCoord),
				(component->window->yCoord +
				 component->yCoord), 0, 0,
				component->width, component->height);
      kernelMouseDraw();

      if (event->type & EVENT_MOUSE_LEFTUP)
	// Make this also a 'selection' event
	event->type |= EVENT_SELECTION;

      // Copy the event into the event stream of the menu item
      kernelWindowEventStreamWrite(&(clickedComponent->events), event);
    }

  if (event->type & EVENT_MOUSE_LEFTUP)
    kernelWindowComponentUnfocus(component);

  return (0);
}


static int containerLayout(kernelWindowComponent *containerComponent)
{
  // Do layout for the menu.

  int status = 0;
  kernelWindowComponent *menuComponent = containerComponent->container;
  kernelWindowContainer *container = containerComponent->data;
  kernelWindowComponent *itemComponent = NULL;
  int xCoord = 0;
  int yCoord = 0;
  int count;

  if (containerComponent->type != containerComponentType)
    {
      kernelError(kernel_error, "Component to layout is not a container");
      return (status = ERR_INVALID);
    }

  kernelDebug(debug_gui, "menu layout for \"%s\"", container->name);

  menuComponent->width = 0;
  menuComponent->height = 0;

  // Set the parameters of all the menu items
  for (count = 0; count < container->numComponents; count ++)
    {
      itemComponent = container->components[count];

      // Make sure it's a menu item
      if (itemComponent->type != listItemComponentType)
	{
	  kernelError(kernel_error, "Menu component is not a menu item!");
	  return (status = ERR_INVALID);
	}

      xCoord = borderThickness;
      if (count == 0)
	yCoord = borderThickness;
      else
	yCoord = (container->components[count - 1]->yCoord +
		  container->components[count - 1]->height);

      if (itemComponent->move)
	itemComponent->move(itemComponent, xCoord, yCoord);

      itemComponent->xCoord = xCoord;
      itemComponent->yCoord = yCoord;

      if (menuComponent->width < itemComponent->width)
	menuComponent->width = itemComponent->width;
      menuComponent->height += itemComponent->height;
    }

  if (container->numComponents)
    {
      menuComponent->width += (borderThickness * 2);
      menuComponent->height += (borderThickness * 2);
    }

  menuComponent->minWidth = menuComponent->width;
  menuComponent->minHeight = menuComponent->height;

  // Get a new graphic buffer
  status = getGraphicBuffer(menuComponent);
  if (status < 0)
    return (status);

  // Set the flag to indicate layout complete
  container->doneLayout = 1;

  return (status = 0);
}


static int destroy(kernelWindowComponent *component)
{
  kernelWindowMenu *menu = component->data;

  // Release all our memory
  if (menu)
    {
      if (menu->container)
	kernelWindowComponentDestroy(menu->container);

      // Free any graphic buffer
      if (menu->buffer.data)
	kernelFree(menu->buffer.data);

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


kernelWindowComponent *kernelWindowNewMenu(objectKey parent, const char *name,
					   windowMenuContents *contents,
					   componentParameters *params)
{
  // Formats a kernelWindowComponent as a kernelWindowMenu

  kernelWindowComponent *component = NULL;
  kernelWindowMenu *menu = NULL;
  kernelWindowContainer *container = NULL;
  int count;

  // Check parameters.  It's okay for 'contents' to be NULL.
  if ((parent == NULL) || (name == NULL) || (params == NULL))
    return (component = NULL);

  // Get the basic component structure
  component = kernelWindowComponentNew(parent, params);
  if (component == NULL)
    return (component);

  // Get memory for this menu component
  menu = kernelMalloc(sizeof(kernelWindowMenu));
  if (menu == NULL)
    {
      kernelFree((void *) component);
      return (component = NULL);
    }

  // Now populate this component
  component->type = menuComponentType;
  component->flags |= WINFLAG_CANFOCUS;
  // Don't want this resized, and by default it is not visible
  component->flags &= ~(WINFLAG_RESIZABLE | WINFLAG_VISIBLE);
  component->buffer = &(menu->buffer);
  component->data = (void *) menu;

  component->draw = &draw;
  component->erase = &erase;
  component->focus = &focus;
  component->getData = &getData;
  component->getSelected = &getSelected;
  component->mouseEvent = &mouseEvent;
  component->destroy = &destroy;

  // Get our container component
  menu->container = kernelWindowNewContainer(parent, name, params);
  if (menu->container == NULL)
    {
      kernelFree((void *) component);
      kernelFree((void *) menu);
      return (component = NULL);
    }

  // Remove the container from the parent container
  if (((kernelWindow *) parent)->type == windowType)
    {
      kernelWindow *tmpWindow = getWindow(parent);
      kernelWindowContainer *tmpContainer = tmpWindow->mainContainer->data;
      tmpContainer->containerRemove(tmpWindow->mainContainer, menu->container);
    }
  else
    {
      kernelWindowComponent *tmpComponent = parent;
      kernelWindowContainer *tmpContainer = tmpComponent->data;
      tmpContainer->containerRemove(tmpComponent, menu->container);
    }

  // This is a hack, since a menu component is not a container, but we
  // need the container component to contain a reference to its menu
  menu->container->container = component;

  container = menu->container->data;

  // Override the container's 'layout' function
  container->containerLayout = &containerLayout;

  if (contents)
    {
      // Loop through the contents structure, adding menu items
      for (count = 0; count < contents->numItems; count ++)
	{
	  contents->items[count].key = (objectKey)
	    kernelWindowNewMenuItem(component, contents->items[count].text,
				    params);
	  if (contents->items[count].key == NULL)
	    {	     
	      kernelFree((void *) component);
	      kernelFree((void *) menu->container);
	      kernelFree((void *) menu);
	      return (component = NULL);
	    }
	}

      // Do the layout
      containerLayout(menu->container);
    }

  return (component);
}
