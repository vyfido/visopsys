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
//  kernelWindowScrollBar.c
//

// This code is for managing kernelWindowScrollBar objects.

#include "kernelWindowManager.h"     // Our prototypes are here
#include "kernelMalloc.h"
#include "kernelMiscFunctions.h"
#include <stdlib.h>

static int borderThickness = 3;
static int borderShadingIncrement = 15;


static int draw(void *componentData)
{
  // Draw the scroll bar component

  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindowScrollBar *scrollBar = (kernelWindowScrollBar *) component->data;
  kernelGraphicBuffer *buffer = (kernelGraphicBuffer *)
    &(((kernelWindow *) component->window)->buffer);

  // Draw the background of the scroll bar
  kernelGraphicDrawRect(buffer, (color *) &(component->parameters.background),
			draw_normal, (component->xCoord + borderThickness),
			(component->yCoord + borderThickness),
			(component->width - (borderThickness * 2)),
			(component->height - (borderThickness * 2)),
			1, 1);

  // Draw the border
  kernelGraphicDrawGradientBorder(buffer, component->xCoord, component->yCoord,
				  component->width, component->height,
				  borderThickness, (color *)
				  &(component->parameters.background),
				  borderShadingIncrement, draw_reverse);

  // Draw the slider
  scrollBar->sliderWidth = (component->width - (borderThickness * 2));
  scrollBar->sliderHeight = (((component->height - (borderThickness * 2)) *
			      scrollBar->state.displayPercent) / 100);

  kernelGraphicDrawGradientBorder(buffer,
				  (component->xCoord + borderThickness),
				  (component->yCoord + borderThickness +
				   scrollBar->sliderY), scrollBar->sliderWidth,
				  scrollBar->sliderHeight, borderThickness,
				  (color *)
				  &(component->parameters.background),
				  borderShadingIncrement, draw_normal);
  return (0);
}


static int getData(void *componentData, void *buffer, int size)
{
  // Gets the state of the scroll bar

  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindowScrollBar *scrollBar = (kernelWindowScrollBar *) component->data;

  kernelMemCopy((void *) &(scrollBar->state), buffer,
		max(size, sizeof(scrollBarState)));

  return (0);
}


static int setData(void *componentData, void *buffer, int size)
{
  // Sets the state of the scroll bar

  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindowScrollBar *scrollBar = (kernelWindowScrollBar *) component->data;
  kernelWindow *window = (kernelWindow *) component->window;

  kernelMemCopy(buffer, (void *) &(scrollBar->state),
		max(size, sizeof(scrollBarState)));

  scrollBar->sliderHeight = (((component->height - (borderThickness * 2)) *
			      scrollBar->state.displayPercent) / 100);
  scrollBar->sliderY =
    ((((component->height - (borderThickness * 2)) - scrollBar->sliderHeight) *
      scrollBar->state.positionPercent) / 100);

  draw(componentData);

  kernelWindowUpdateBuffer(&(window->buffer), component->xCoord,
			   component->yCoord, component->width,
			   component->height);
  return (0);
}


static int mouseEvent(void *componentData, windowEvent *event)
{
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindowScrollBar *scrollBar = (kernelWindowScrollBar *) component->data;
  kernelWindow *window = (kernelWindow *) component->window;
  kernelGraphicBuffer *buffer = &(window->buffer);
  int eventY = 0;
  static int dragging = 0;
  static int dragY;

  // Get X and Y coordinates relative to the component
  eventY = (event->yPosition - (window->yCoord + component->yCoord +
				borderThickness));

  // Is the mouse event in the slider?
  if ((eventY >= scrollBar->sliderY) &&
      (eventY < (scrollBar->sliderY + scrollBar->sliderHeight)))
    {
      if (event->type == EVENT_MOUSE_DRAG)
	{
	  if (dragging)
	    // The scroll bar is still moving.  Set the new position
	    scrollBar->sliderY += (eventY - dragY);

	  else
	    // The scroll bar has started moving
	    dragging = 1;

	  // Save the current dragging Y coordinate 
	  dragY = eventY;
	}

      else
	// Not dragging.  Do nothing.
	dragging = 0;
    }

  // Not in the slider.  Is it in the space above it, or below it?

  else if ((event->type == EVENT_MOUSE_LEFTDOWN) &&
	   (eventY > 0) && (eventY < scrollBar->sliderY))
    // It's above the slider
    scrollBar->sliderY -= scrollBar->sliderHeight;
      
  else if ((event->type == EVENT_MOUSE_LEFTDOWN) &&
	   (eventY > (scrollBar->sliderY +scrollBar->sliderHeight)) &&
	   (eventY < (component->height - borderThickness)))
    // It's below the slider
    scrollBar->sliderY += scrollBar->sliderHeight;

  else
    // Do nothing.
    return (0);

  // Make sure the slider stays within the bounds
  if (scrollBar->sliderY < 0)
    scrollBar->sliderY = 0;
  else if ((scrollBar->sliderY + scrollBar->sliderHeight) >=
	   (component->height - (borderThickness * 2)))
    scrollBar->sliderY = ((component->height - (borderThickness * 2)) -
			  scrollBar->sliderHeight);

  // Recalculate the position percentage
  int extraSpace =
    ((component->height - (borderThickness * 2)) - scrollBar->sliderHeight);
  if (extraSpace)
    scrollBar->state.positionPercent =
      ((scrollBar->sliderY * 100) / extraSpace);
  else
    scrollBar->state.positionPercent = 0;
  
  if (component->draw)
    component->draw((void *) component);
  
  kernelWindowUpdateBuffer(buffer, component->xCoord, component->yCoord,
			   component->width, component->height);
  
  // Redraw the mouse
  kernelMouseDraw();

  return (0);
}


static int destroy(void *componentData)
{
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;

  // Release all our memory
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


kernelWindowComponent *kernelWindowNewScrollBar(volatile void *parent,
			scrollBarType type, int width, int height,
			componentParameters *params)
{
  // Formats a kernelWindowComponent as a kernelWindowScrollBar

  kernelWindowComponent *component = NULL;
  kernelWindowScrollBar *scrollBar = NULL;

  // Check parameters.  It's okay for the image or label to be NULL
  if ((parent == NULL) || (params == NULL))
    return (component = NULL);

  // Get the basic component structure
  component = kernelWindowComponentNew(parent, params);
  if (component == NULL)
    return (component);

  // Now populate it
  component->type = scrollBarComponentType;
  component->flags |= WINFLAG_RESIZABLE;

  scrollBar = kernelMalloc(sizeof(kernelWindowScrollBar));
  if (scrollBar == NULL)
    {
      kernelFree((void *) component);
      return (component = NULL);
    }

  scrollBar->type = type;
  scrollBar->state.displayPercent = 100;
  scrollBar->state.positionPercent = 0;
  
  component->width = width;
  component->height = height;

  if ((type == scrollbar_vertical) && !width)
    component->width = 20;
  if ((type == scrollbar_horizontal) && !height)
    component->height = 20;

  component->data = (void *) scrollBar;

  // The functions
  component->draw = &draw;
  component->getData = &getData;
  component->setData = &setData;
  component->mouseEvent = &mouseEvent;
  component->destroy = &destroy;

  return (component);
}
