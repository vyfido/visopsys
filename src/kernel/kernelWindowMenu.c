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
#include "kernelWindowEventStream.h"
#include "kernelError.h"
#include <string.h>

static int borderThickness = 3;
static int borderShadingIncrement = 15;


static int draw(void *componentData)
{
  // Draw the underlying container component, and then the border around
  // it.

  int status = 0;
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindowMenu *menu = (kernelWindowMenu *) component->data;
  kernelWindow *window = component->window;
  kernelGraphicBuffer *buffer = &(window->buffer);
  int count;

  if (!menu->numComponents)
    return (status = 0);

  // Draw the background of the menu bar
  kernelGraphicDrawRect(buffer, (color *) &(component->parameters.background),
			draw_normal, (component->xCoord + borderThickness),
			(component->yCoord + borderThickness),
			(component->width - (borderThickness * 2)),
			(component->height - (borderThickness * 2)), 1, 1);

  kernelGraphicDrawGradientBorder(buffer, component->xCoord, component->yCoord,
				  component->width, component->height,
				  borderThickness, (color *)
				  &(component->parameters.background),
				  borderShadingIncrement,
				  draw_normal);

  // Draw all the menu items
  for (count = 0; count < menu->numComponents; count ++)
    if (menu->components[count]->draw)
      menu->components[count]->draw((void *) menu->components[count]);

  return (status);
}


static int mouseEvent(void *componentData, windowEvent *event)
{
  // When mouse events happen to menu components, we pass them on to the
  // appropriate kernelWindowMenuItem component

  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindow *window = (kernelWindow *) component->window;
  kernelWindowMenu *menu = (kernelWindowMenu *) component->data;
  kernelWindowComponent *clickedItem = NULL;
  int count;

  if (menu->numComponents && (event->type & EVENT_MASK_MOUSE))
    {
      // Figure out which menu item was clicked based on the coordinates
      // of the event
      int tmp = (event->yPosition - window->yCoord);

      for (count = 0; count < menu->numComponents; count ++)
	{
	  if ((tmp >= menu->components[count]->yCoord) &&
	      (tmp < (menu->components[count]->yCoord +
		      menu->components[count]->height)))
	    {
	      clickedItem = menu->components[count];
	      break;
	    }
	}

      // Is there an item in this space?
      if (clickedItem == NULL)
	return (0);
	  
      if ((clickedItem->flags & WINFLAG_VISIBLE) &&
	  (clickedItem->flags & WINFLAG_ENABLED) && clickedItem->mouseEvent)
	{
	  clickedItem->mouseEvent((void *) clickedItem, event);

	  if (event->type & EVENT_MOUSE_LEFTUP)
	    // Make this also a 'selection' event
	    event->type |= EVENT_SELECTION;

	  // Copy the event into the event stream of the menu item
	  kernelWindowEventStreamWrite(&(clickedItem->events), event);
	}

      if (event->type & EVENT_MOUSE_LEFTUP && clickedItem->setSelected)
	// We don't want menu selections to be persistent (i.e. we don't
	// want the same item to appear selected the next time the menu is
	// visible
	clickedItem->setSelected((void *) clickedItem, 0);
    }

  return (0);
}


static int containerLayout(kernelWindowComponent *containerComponent)
{
  // Do layout for the menu.

  int status = 0;
  kernelWindowMenu *menu = (kernelWindowMenu *) containerComponent->data;
  kernelWindowComponent *itemComponent = NULL;
  int count;

  containerComponent->width = 0;
  containerComponent->height = 0;
  
  // Set the parameters of all the menu items
  for (count = 0; count < menu->numComponents; count ++)
    {
      itemComponent = menu->components[count];

      // Make sure it's a menu item
      if (itemComponent->type != listItemComponentType)
	{
	  kernelError(kernel_error, "Window component is not a menu item!");
	  return (status = ERR_INVALID);
	}

      itemComponent->xCoord =
	(containerComponent->xCoord + borderThickness);
      if (count == 0)
        itemComponent->yCoord = (containerComponent->yCoord + borderThickness);
      else
        itemComponent->yCoord = (menu->components[count - 1]->yCoord +
				 menu->components[count - 1]->height);
      
      if (containerComponent->width < itemComponent->width)
	containerComponent->width = itemComponent->width;
    }

  if (menu->numComponents)
    containerComponent->height =
      ((menu->components[menu->numComponents - 1]->yCoord +
       menu->components[menu->numComponents - 1]->height) -
	containerComponent->yCoord);

  containerComponent->width += (borderThickness * 2);
  containerComponent->height += borderThickness;

  // Make sure we have minimum width and height
  if (containerComponent->width < 50)
    containerComponent->width = 50;

  containerComponent->minWidth = containerComponent->width;
  containerComponent->minHeight = containerComponent->height;

  // Set the flag to indicate layout complete
  menu->doneLayout = 1;

  return(status);
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


kernelWindowComponent *kernelWindowNewMenu(volatile void *parent,
					   const char *name,
					   componentParameters *params)
{
  // Formats a kernelWindowComponent as a kernelWindowMenu

  kernelWindowComponent *component = NULL;
  kernelWindowContainer *container = NULL;

  // Check parameters.
  if ((parent == NULL) || (name == NULL) || (params == NULL))
    return (component = NULL);

  // Get the superclass container component
  component = kernelWindowNewContainer(parent, name, params);
  if (component == NULL)
    return (component);

  component->subType = menuComponentType;

  // Don't want this resized
  component->flags &= ~WINFLAG_RESIZABLE;

  component->draw = &draw;
  component->mouseEvent = &mouseEvent;

  container = (kernelWindowContainer *) component->data;

  container->containerLayout = &containerLayout;

  return (component);
}
