//
//  Visopsys
//  Copyright (C) 1998-2007 J. Andrew McLaughlin
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

extern kernelWindowVariables *windowVariables;


static int getGraphicBuffer(kernelWindowComponent *component)
{
  // Gets a new graphic buffer for the menu, appropriate for its size.  Also
  // deallocates any previous buffer memory, if applicable.

  int status = 0;
  kernelWindowMenu *menu = NULL;

  if (component->type != menuComponentType)
    {
      kernelError(kernel_error, "Component is not a menu");
      return (status = ERR_INVALID);
    }

  menu = component->data;

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

  if (component->setBuffer)
    component->setBuffer(component, &(menu->buffer));
  component->buffer = &(menu->buffer);

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


static int add(kernelWindowComponent *menuComponent,
	       kernelWindowComponent *component)
{
  // Add the supplied component to the menu.

  int status = 0;
  kernelWindowMenu *menu = menuComponent->data;
  
  if (menu->container && menu->container->add)
    status = menu->container->add(menu->container, component);

  return (status);
}


static int numComps(kernelWindowComponent *component)
{
  int numItems = 0;
  kernelWindowMenu *menu = component->data;

  if (menu->container && menu->container->numComps)
    // Count our container's components
    numItems = menu->container->numComps(menu->container);

  return (numItems);
}


static int flatten(kernelWindowComponent *component,
		   kernelWindowComponent **array, int *numItems,
		   unsigned flags)
{
  int status = 0;
  kernelWindowMenu *menu = component->data;

  if (menu->container && menu->container->flatten)
    // Flatten our container
    status = menu->container->flatten(menu->container, array, numItems, flags);

  return (status);
}


static int layout(kernelWindowComponent *component)
{
  // Do layout for the menu.

  int status = 0;
  kernelWindowMenu *menu = component->data;
  kernelWindowContainer *container = menu->container->data;
  kernelWindowComponent *itemComponent = NULL;
  int xCoord = 0;
  int yCoord = 0;
  int count;

  kernelDebug(debug_gui, "Menu layout for \"%s\"", container->name);

  component->width = 0;
  component->height = 0;

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

      xCoord = windowVariables->border.thickness;
      if (count == 0)
	yCoord = windowVariables->border.thickness;
      else
	yCoord = (container->components[count - 1]->yCoord +
		  container->components[count - 1]->height);

      if (itemComponent->move)
	itemComponent->move(itemComponent, xCoord, yCoord);

      itemComponent->xCoord = xCoord;
      itemComponent->yCoord = yCoord;

      if (component->width < itemComponent->width)
	component->width = itemComponent->width;
      component->height += itemComponent->height;
    }

  if (container->numComponents)
    {
      component->width += (windowVariables->border.thickness * 2);
      component->height += (windowVariables->border.thickness * 2);
    }

  component->minWidth = component->width;
  component->minHeight = component->height;

  // Get a new graphic buffer
  status = getGraphicBuffer(component);
  if (status < 0)
    return (status);

  // Set the flag to indicate layout complete
  component->doneLayout = 1;

  return (status = 0);
}


static int setBuffer(kernelWindowComponent *component,
		     kernelGraphicBuffer *buffer)
{
  // Set the graphics buffer for the component's subcomponents.

  int status = 0;
  kernelWindowMenu *menu = component->data;

  if (menu->container && menu->container->setBuffer)
    {
      // Do our container
      status = menu->container->setBuffer(menu->container, buffer);
      menu->container->buffer = buffer;
    }

  return (status);
}


static int draw(kernelWindowComponent *component)
{
  kernelWindowMenu *menu = component->data;
  kernelWindowContainer *container = menu->container->data;
  int borderThickness = windowVariables->border.thickness;
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
			    (color *) &(component->params.background),
			    draw_normal, borderThickness, borderThickness,
			    (component->width - (borderThickness * 2)),
			    (component->height - (borderThickness * 2)), 1, 1);

      kernelGraphicDrawGradientBorder(component->buffer, 0, 0,
				      component->width, component->height,
				      borderThickness, (color *)
				      &(component->params.background),
				      windowVariables->border.shadingIncrement,
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

  component->add = &add;
  component->numComps = &numComps;
  component->flatten = &flatten;
  component->layout = &layout;
  component->setBuffer = &setBuffer;
  component->draw = &draw;
  component->erase = &erase;
  component->focus = &focus;
  component->getData = &getData;
  component->getSelected = &getSelected;
  component->mouseEvent = &mouseEvent;
  component->destroy = &destroy;

  // Get our container component
  menu->container = kernelWindowNewContainer(component, name, params);
  if (menu->container == NULL)
    {
      kernelFree((void *) component);
      kernelFree((void *) menu);
      return (component = NULL);
    }

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
    }

  return (component);
}
