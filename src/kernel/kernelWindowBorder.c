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
//  kernelWindowBorder.c
//

// This code is for managing kernelWindowBorder objects.
// These are just images that appear inside windows and buttons, etc


#include "kernelWindowManager.h"     // Our prototypes are here
#include "kernelWindowEventStream.h"
#include "kernelMalloc.h"
#include "kernelMultitasker.h"
#include "kernelParameters.h"
#include "kernelMiscFunctions.h"
#include "kernelError.h"
#include <string.h>


static int borderShadingIncrement = DEFAULT_SHADING_INCREMENT;
static unsigned newWindowWidth = 0;
static unsigned newWindowHeight = 0;


static void resizeWindow(void *componentData, windowEvent *event)
{
  // This gets called by the window manager thread when a window resize
  // has been requested

  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindow *window = (kernelWindow *) component->window;

  if (event->type & EVENT_WINDOW_RESIZE)
    {
      kernelWindowSetSize(window, newWindowWidth, newWindowHeight);

      // Don't show it while it's being resized
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
  static int dragging = 0;

  if (dragging)
    {
      if (event->type & EVENT_MOUSE_DRAG)
	{
	  // The window is still being resized
	  
	  // Erase the xor'ed outline
	  kernelGraphicDrawRect(NULL, &((color) { 255, 255, 255 }),
				draw_xor, window->xCoord, window->yCoord,
				newWindowWidth, newWindowHeight, 1, 0);

	  // Set the new size
	  if (border->type == border_top)
	    {
	      if (event->yPosition < window->yCoord)
		{
		  int diff = (window->yCoord - event->yPosition);
		  window->yCoord -= diff;
		  newWindowHeight += diff;
		}
	      else if (event->yPosition > window->yCoord)
		{
		  int diff = (event->yPosition - window->yCoord);
		  window->yCoord += diff;
		  newWindowHeight -= diff;
		}
	    }
	  else if (border->type == border_bottom)
	    {
	      if (event->yPosition < (window->yCoord +
				      newWindowHeight))
		{
		  int diff = ((window->yCoord + newWindowHeight) -
			      event->yPosition);
		  newWindowHeight -= diff;
		}
	      else if (event->yPosition > (window->yCoord +
					   newWindowHeight))
		{
		  int diff = (event->yPosition - (window->yCoord +
						  newWindowHeight));
		  newWindowHeight += diff;
		}
	    }
	  else if (border->type == border_left)
	    {
	      if (event->xPosition < window->xCoord)
		{
		  int diff = (window->xCoord - event->xPosition);
		  window->xCoord -= diff;
		  newWindowWidth += diff;
		}
	      else if (event->xPosition > window->xCoord)
		{
		  int diff = (event->xPosition - window->xCoord);
		  window->xCoord += diff;
		  newWindowWidth -= diff;
		}
	    }
	  else if (border->type == border_right)
	    {
	      if (event->xPosition < (window->xCoord +
				      newWindowWidth))
		{
		  int diff = ((window->xCoord + newWindowWidth) -
			      event->xPosition);
		  newWindowWidth -= diff;
		}
	      else if (event->xPosition > (window->xCoord +
					   newWindowWidth))
		{
		  int diff = (event->xPosition - (window->xCoord +
						  newWindowWidth));
		  newWindowWidth += diff;
		}
	    }

	  // Draw an xor'ed outline
	  kernelGraphicDrawRect(NULL, &((color) { 255, 255, 255 }),
				draw_xor, window->xCoord, window->yCoord,
				newWindowWidth,	newWindowHeight, 1, 0);
	  return (0);
	}
      else
	{
	  // The resize is finished
	  
	  // Erase the xor'ed outline
	  kernelGraphicDrawRect(NULL, &((color) { 255, 255, 255 }),
				draw_xor, window->xCoord, window->yCoord,
				newWindowWidth, newWindowHeight, 1, 0);

	  // Write a resize event to the component event stream
	  resizeEvent.type = EVENT_WINDOW_RESIZE;
	  kernelWindowEventStreamWrite(&(component->events), &resizeEvent);

	  dragging = 0;
	  return (0);
	}
    }

  else if ((event->type & EVENT_MOUSE_DRAG) &&
	   (window->flags & WINFLAG_RESIZABLE))
    {
      // Don't show it while it's being resized
      kernelWindowSetVisible(window, 0);

      // Draw an xor'ed outline
      kernelGraphicDrawRect(NULL, &((color) { 255, 255, 255 }),
			    draw_xor, window->xCoord,
			    window->yCoord, window->buffer.width,
			    window->buffer.height, 1, 0);
      
      newWindowWidth = window->buffer.width;
      newWindowHeight = window->buffer.height;
      dragging = 1;
    }

  return (0);
}


static int destroy(void *componentData)
{
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;

  if (component->data)
    kernelFree(component->data)

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
