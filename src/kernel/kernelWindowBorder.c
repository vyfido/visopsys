//
//  Visopsys
//  Copyright (C) 1998-2005 J. Andrew McLaughlin
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
//  kernelWindowBorder.c
//

// This code is for managing kernelWindowBorder objects.
// These are just images that appear inside windows and buttons, etc


#include "kernelWindow.h"     // Our prototypes are here
#include "kernelWindowEventStream.h"
#include "kernelMalloc.h"
#include "kernelMultitasker.h"
#include "kernelParameters.h"
#include "kernelMiscFunctions.h"
#include "kernelError.h"
#include <string.h>


static int borderShadingIncrement = DEFAULT_SHADING_INCREMENT;
static int newWindowX = 0;
static int newWindowY = 0;
static int newWindowWidth = 0;
static int newWindowHeight = 0;


static void resizeWindow(void *componentData, windowEvent *event)
{
  // This gets called by the window manager thread when a window resize
  // has been requested

  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindow *window = (kernelWindow *) component->window;

  if (event->type == EVENT_WINDOW_RESIZE)
    {
      window->xCoord = newWindowX;
      window->yCoord = newWindowY;

      kernelWindowSetSize(window, newWindowWidth, newWindowHeight);

      kernelWindowSetVisible(window, 1);
	  
      // Redraw the mouse
      kernelMouseDraw();
    }

  return;
}


static int draw(void *componentData)
{
  // Draws all the border components on the window.  Really we should implement
  // this so that each border gets drawn individually, but this is faster

  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindowBorder *border = (kernelWindowBorder *) component->data;
  kernelWindow *window = (kernelWindow *) component->window;
  kernelGraphicBuffer *buffer = &(window->buffer);

  if (border->type == border_top)
    {
      component->xCoord = 0;
      component->yCoord = 0;
      component->width = window->buffer.width;
      component->height = DEFAULT_BORDER_THICKNESS;
    }
  else if (border->type == border_bottom)
    {
      component->xCoord = 0;
      component->yCoord = 
	(window->buffer.height - DEFAULT_BORDER_THICKNESS + 1);
      component->width = window->buffer.width;
      component->height = DEFAULT_BORDER_THICKNESS;
    }
  else if (border->type == border_left)
    {
      component->xCoord = 0;
      component->yCoord = 0;
      component->width = DEFAULT_BORDER_THICKNESS;
      component->height = window->buffer.height;
    }
  else if (border->type == border_right)
    {
      component->xCoord =
	(window->buffer.width - DEFAULT_BORDER_THICKNESS + 1);
      component->yCoord = 0;
      component->width = DEFAULT_BORDER_THICKNESS;
      component->height = window->buffer.height;
    }

  // Only draw when the top border component is requested
  if (border->type != border_top)
    return (0);

  kernelGraphicDrawGradientBorder(buffer, 0, 0, window->buffer.width,
				  window->buffer.height,
				  DEFAULT_BORDER_THICKNESS,
				  (color *) &(window->background),
				  borderShadingIncrement, draw_normal);
  return (0);
}


static int mouseEvent(void *componentData, windowEvent *event)
{
  // When dragging mouse events happen to border components, we resize the
  // window.

  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindowBorder *border = (kernelWindowBorder *) component->data;
  kernelWindow *window = (kernelWindow *) component->window;
  windowEvent resizeEvent;
  int diff = 0;
  int tmpWindowX = newWindowX;
  int tmpWindowY = newWindowY;
  int tmpWindowWidth = newWindowWidth;
  int tmpWindowHeight = newWindowHeight;
  static int dragging = 0;

  if (dragging)
    {
      if (event->type == EVENT_MOUSE_DRAG)
	{
	  // The window is still being resized
	  
	  // Erase the xor'ed outline
	  kernelWindowRedrawArea(newWindowX, newWindowY, newWindowWidth, 1);
	  kernelWindowRedrawArea(newWindowX, newWindowY, 1, newWindowHeight);
	  kernelWindowRedrawArea((newWindowX + newWindowWidth - 1),
				 newWindowY, 1, newWindowHeight);
	  kernelWindowRedrawArea(newWindowX,
				 (newWindowY + newWindowHeight - 1),
				 newWindowWidth, 1);

	  // Set the new size
	  if ((border->type == border_top) &&
	      (event->yPosition < (newWindowY + newWindowHeight)))
	    {
	      diff = (event->yPosition - newWindowY);
	      tmpWindowY += diff;
	      tmpWindowHeight -= diff;
	    }
	  else if ((border->type == border_bottom) &&
		   (event->yPosition > newWindowY))
	    {
	      diff = (event->yPosition - (newWindowY + newWindowHeight));
	      tmpWindowHeight += diff;
	    }
	  else if ((border->type == border_left) &&
		   (event->xPosition < (newWindowX + newWindowWidth)))
	    {
	      diff = (event->xPosition - newWindowX);
	      tmpWindowX += diff;
	      tmpWindowWidth -= diff;
	    }
	  else if ((border->type == border_right) &&
		   (event->xPosition > newWindowX))
	    {
	      diff = (event->xPosition - (newWindowX + newWindowWidth));
	      tmpWindowWidth += diff;
	    }

	  // Don't resize below reasonable minimums
	  if (tmpWindowWidth < (DEFAULT_TITLEBAR_HEIGHT * 4))
	    newWindowWidth = (DEFAULT_TITLEBAR_HEIGHT * 4);
	  else
	    {
	      newWindowX = tmpWindowX;
	      newWindowWidth = tmpWindowWidth;
	    }

	  if (tmpWindowHeight <
	      (DEFAULT_TITLEBAR_HEIGHT + (DEFAULT_BORDER_THICKNESS * 2)))
	    newWindowHeight =
	      (DEFAULT_TITLEBAR_HEIGHT + (DEFAULT_BORDER_THICKNESS * 2));
	  else
	    {
	      newWindowY = tmpWindowY;
	      newWindowHeight = tmpWindowHeight;
	    }

	  // Draw an xor'ed outline
	  kernelGraphicDrawRect(NULL, &((color) { 255, 255, 255 }),
				draw_xor, newWindowX, newWindowY,
				newWindowWidth,	newWindowHeight, 1, 0);
	}
      else
	{
	  // The resize is finished
	  
	  // Erase the xor'ed outline
	  kernelGraphicDrawRect(NULL, &((color) { 255, 255, 255 }),
				draw_xor, newWindowX, newWindowY,
				newWindowWidth, newWindowHeight, 1, 0);

	  // Write a resize event to the component event stream
	  resizeEvent.type = EVENT_WINDOW_RESIZE;
	  kernelWindowEventStreamWrite(&(component->events), &resizeEvent);

	  dragging = 0;
	}
    }

  else if ((event->type == EVENT_MOUSE_DRAG) &&
	   (window->flags & WINFLAG_RESIZABLE))
    {
      // Don't show it while it's being resized
      kernelWindowSetVisible(window, 0);

      // Draw an xor'ed outline
      kernelGraphicDrawRect(NULL, &((color) { 255, 255, 255 }),
			    draw_xor, window->xCoord,
			    window->yCoord, window->buffer.width,
			    window->buffer.height, 1, 0);
      
      newWindowX = window->xCoord;
      newWindowY = window->yCoord;
      newWindowWidth = window->buffer.width;
      newWindowHeight = window->buffer.height;
      dragging = 1;
    }

  kernelMouseDraw();
  return (0);
}


static int destroy(void *componentData)
{
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;

  if (component->data)
    {
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


kernelWindowComponent *kernelWindowNewBorder(volatile void *parent,
					     borderType type,
					     componentParameters *params)
{
  // Formats a kernelWindowComponent as a kernelWindowBorder

  kernelWindow *window = NULL;
  kernelWindowComponent *component = NULL;
  kernelWindowBorder *borderComponent = NULL;

  // Check parameters
  if ((parent == NULL) || (params == NULL))
    return (component = NULL);

  window = getWindow(parent);

  // Get the basic component structure
  component = kernelWindowComponentNew(parent, params);
  if (component == NULL)
    return (component);

  // Get the border component
  borderComponent = kernelMalloc(sizeof(kernelWindowBorder));
  if (borderComponent == NULL)
    {
      kernelFree((void *) component);
      return (component = NULL);
    }

  borderComponent->type = type;

  // Now populate the main component

  component->type = borderComponentType;
  component->xCoord = 0;
  component->yCoord = 0;
  component->width = window->buffer.width;
  component->height = window->buffer.height;

  component->data = (void *) borderComponent;

  // The functions
  component->draw = &draw;
  component->mouseEvent = &mouseEvent;
  component->destroy = &destroy;

  kernelWindowRegisterEventHandler((objectKey) component, &resizeWindow);

  return (component);
}
