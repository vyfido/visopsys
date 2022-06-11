//
//  Visopsys
//  Copyright (C) 1998-2004 J. Andrew McLaughlin
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

#include "kernelWindowManager.h"     // Our prototypes are here
#include "kernelWindowEventStream.h"
#include "kernelMiscFunctions.h"
#include "kernelError.h"
#include <string.h>
#include <sys/errors.h>

static int borderThickness = 3;
static int borderShadingIncrement = 15;
static int (*saveLayout) (kernelWindowComponent *) = NULL;  


static int draw(void *componentData)
{
  // Draw the underlying container component, and then the border around
  // it.

  int status = 0;
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindow *window = component->window;
  kernelGraphicBuffer *buffer = &(window->buffer);
  color background = { DEFAULT_GREY, DEFAULT_GREY, DEFAULT_GREY };

  if (!component->parameters.useDefaultBackground)
    {
      // Use user-supplied color
      background.red = component->parameters.background.red;
      background.green = component->parameters.background.green;
      background.blue = component->parameters.background.blue;
    }

  // Draw the background of the menu bar
  kernelGraphicDrawRect(buffer, &background, draw_normal,
			(component->xCoord + borderThickness),
			(component->yCoord + borderThickness),
			(component->width - (borderThickness * 2)),
			(component->height - (borderThickness * 2)), 1, 1);

  kernelGraphicDrawGradientBorder(buffer, component->xCoord, component->yCoord,
				  component->width, component->height,
				  borderThickness, borderShadingIncrement,
				  draw_normal);
  return (status);
}


static int mouseEvent(void *componentData, windowEvent *event)
{
  // When mouse events happen to menu components, we pass them on to the
  // appropriate kernelWindowMenuItem component

  int status = 0;
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindow *window = (kernelWindow *) component->window;
  kernelWindowMenu *menu = (kernelWindowMenu *) component->data;
  kernelWindowComponent *clickedItem = NULL;
  
  if (menu->numComponents && (event->type & EVENT_MASK_MOUSE))
    {
      // Figure out which menu item was clicked based on the coordinates
      // of the event
      int tmp = ((event->yPosition - (window->yCoord + component->yCoord)) /
		 menu->components[0]->height);

      // Is there an item in this space?
      if (tmp >= menu->numComponents)
	return (status = 0);
	  
      clickedItem = menu->components[tmp];

      if ((clickedItem->flags & WINFLAG_VISIBLE) &&
	  (clickedItem->flags & WINFLAG_ENABLED) && clickedItem->mouseEvent)
	{
	  status = clickedItem->mouseEvent((void *) clickedItem, event);

	  // Copy the event into the event stream of the menu item
	  kernelWindowEventStreamWrite(&(clickedItem->events), event);
	}

      // We don't want menu selections to be persistent (i.e. we don't want
      // the same item to appear selected the next time the menu is visible)
      // If the event was a mouse up, deselect the item
      if ((event->type & EVENT_MOUSE_UP) && clickedItem->setSelected)
	clickedItem->setSelected((void *) clickedItem, 0);
    }

  return (status);
}


static int containerLayout(kernelWindowComponent *containerComponent)
{
  // Do layout for the menu.

  int status = 0;
  kernelWindowMenu *menu = (kernelWindowMenu *) containerComponent->data;
  kernelWindowComponent *itemComponent = NULL;
  int count;

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

      // Not visible initially
      itemComponent->flags &= ~WINFLAG_VISIBLE;

      // Set almost all the parameters.  Really the only things we want
      // to preserve from the user are the color settings.
      itemComponent->parameters.gridX = 0;
      itemComponent->parameters.gridY = count;
      itemComponent->parameters.gridWidth = 1;
      itemComponent->parameters.gridHeight = 1;
      itemComponent->parameters.padLeft = borderThickness;
      itemComponent->parameters.padRight = borderThickness;
      if (count == 0)
	itemComponent->parameters.padTop = (borderThickness * 2);
      else
	itemComponent->parameters.padTop = 0;
      if (count == (menu->numComponents - 1))
	itemComponent->parameters.padBottom = borderThickness;
      else
	itemComponent->parameters.padBottom = 0;
      itemComponent->parameters.orientationX = orient_left;
      itemComponent->parameters.orientationY = orient_middle;
      itemComponent->parameters.resizableX = 0;
      itemComponent->parameters.resizableY = 0;
      itemComponent->parameters.hasBorder = 0;
      itemComponent->parameters.stickyFocus = 0;
    }

  // Call the saved layout function
  if (saveLayout)
    status = saveLayout(containerComponent);

  if (containerComponent->width < 50)
    containerComponent->width = 50;

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

  //component->flags |= WINFLAG_CANFOCUS;
  component->width = 50;
  component->height = (borderThickness * 2);
  
  // Save the old draw function, and superimpose our own
  component->draw = &draw;
  component->mouseEvent = &mouseEvent;

  container = (kernelWindowContainer *) component->data;

  // Save the old layout function, and superimpose our own
  saveLayout = container->containerLayout;
  container->containerLayout = &containerLayout;

  return (component);
}
