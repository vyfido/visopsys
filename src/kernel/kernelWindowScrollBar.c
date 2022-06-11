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
#include "kernelMemoryManager.h"
#include "kernelMiscFunctions.h"
#include "kernelError.h"
#include <string.h>


static int borderThickness = 3;
static int borderShadingIncrement = 15;


static inline void calculatePositionPercent(kernelWindowComponent *
					    componentData)
{
  // Based on the position of the slider, calculate the display percentage

  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindowScrollBar *scrollBar = (kernelWindowScrollBar *) component->data;

  int extraSpace =
    ((component->height - (borderThickness * 2)) - scrollBar->sliderHeight);

  if (extraSpace)
    scrollBar->positionPercent = ((scrollBar->sliderY * 100) / extraSpace);
  else
    scrollBar->positionPercent = 0;
}


static int draw(void *componentData)
{
  // Draw the scroll bar component

  color background = { DEFAULT_GREY, DEFAULT_GREY, DEFAULT_GREY };
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindowScrollBar *scrollBar = (kernelWindowScrollBar *) component->data;
  kernelGraphicBuffer *buffer = (kernelGraphicBuffer *)
    &(((kernelWindow *) component->window)->buffer);

  if (!component->parameters.useDefaultBackground)
    {
      // Use user-supplied color
      background.red = component->parameters.background.red;
      background.green = component->parameters.background.green;
      background.blue = component->parameters.background.blue;
    }

  // Draw the background of the scroll bar
  kernelGraphicDrawRect(buffer, &background, draw_normal,
			(component->xCoord + borderThickness),
			(component->yCoord + borderThickness),
			(component->width - (borderThickness * 2)),
			(component->height - (borderThickness * 2)),
			1, 1);

  // Draw the border
  kernelGraphicDrawGradientBorder(buffer, component->xCoord, component->yCoord,
				  component->width, component->height,
				  borderThickness, borderShadingIncrement,
				  draw_reverse);

  // Draw the slider
  scrollBar->sliderWidth = (component->width - (borderThickness * 2));
  scrollBar->sliderHeight = (((component->height - (borderThickness * 2)) *
			      scrollBar->displayPercent) / 100);

  scrollBar->sliderY =
    (((component->height - (borderThickness * 2) -
       scrollBar->sliderHeight) * scrollBar->positionPercent) / 100);

  kernelGraphicDrawGradientBorder(buffer,
				  (component->xCoord + borderThickness +
				   scrollBar->sliderX),
				  (component->yCoord + borderThickness +
				   scrollBar->sliderY), scrollBar->sliderWidth,
				  scrollBar->sliderHeight, borderThickness,
				  borderShadingIncrement, draw_normal);

  return (0);
}


static int mouseEvent(void *componentData, windowEvent *event)
{
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindowScrollBar *scrollBar = (kernelWindowScrollBar *) component->data;
  kernelWindow *window = (kernelWindow *) component->window;
  kernelGraphicBuffer *buffer = &(window->buffer);
  static int dragging = 0;
  static windowEvent dragEvent;
  int newY = 0;

  if ((event->xPosition >= (window->xCoord + component->xCoord +
			    borderThickness + scrollBar->sliderX)) &&
      (event->xPosition < (window->xCoord + component->xCoord +
			   borderThickness + scrollBar->sliderX +
			   scrollBar->sliderWidth)))
    {
      // Is the mouse event in the slider?
      if ((event->yPosition >= (window->yCoord + component->yCoord +
				borderThickness + scrollBar->sliderY)) &&
	  (event->yPosition < (window->yCoord +  component->yCoord +
			       borderThickness + scrollBar->sliderY +
			       scrollBar->sliderHeight)))
	{
	  if (dragging)
	    {
	      if (event->type & EVENT_MOUSE_DRAG)
		{
		  // The scroll bar is still moving
		  
		  // Set the new position
		  newY = (scrollBar->sliderY +
			  (event->yPosition - dragEvent.yPosition));
		  if ((newY >= 0) &&
		      ((newY + scrollBar->sliderHeight) <=
		       (component->height - (borderThickness * 2))))
		    scrollBar->sliderY = newY;

		  // Save a copy of the dragging event
		  kernelMemCopy(event, &dragEvent, sizeof(windowEvent));
		}

	      else
		// The move is finished
		dragging = 0;
	    }

	  else if (event->type & EVENT_MOUSE_DRAG)
	    {
	      // The scroll bar has started moving
	  
	      // Save a copy of the dragging event
	      kernelMemCopy(event, &dragEvent, sizeof(windowEvent));
	      dragging = 1;
	    }
	}

      // Not in the slider.  Is it in the space above or below it?
      else if ((event->yPosition > (window->yCoord + component->yCoord +
				    borderThickness)) &&
	       (event->yPosition < (window->yCoord + component->yCoord +
				    borderThickness + scrollBar->sliderY)))
	{
	  // It's above the slider
	  scrollBar->sliderY -= scrollBar->sliderHeight;
	  if (scrollBar->sliderY < 0)
	    scrollBar->sliderY = 0;
	}

      else if ((event->yPosition > (window->yCoord + component->yCoord +
				    borderThickness + scrollBar->sliderY +
				    scrollBar->sliderHeight)) &&
	       (event->yPosition < (window->yCoord + component->yCoord +
				    component->height - borderThickness)))
	{
	  // It's below the slider
	  scrollBar->sliderY += scrollBar->sliderHeight;
	  if ((scrollBar->sliderY + scrollBar->sliderHeight) >
	      (component->height - (borderThickness * 2)))
	    scrollBar->sliderY = ((component->height - (borderThickness * 2)) -
				  scrollBar->sliderHeight);
	}


      // Recalculate the position percentage
      calculatePositionPercent(component);

      if (component->draw)
	component->draw((void *) component);
      kernelWindowUpdateBuffer(buffer, component->xCoord, component->yCoord,
			       component->width, component->height);

      // Redraw the mouse
      kernelMouseDraw();
    }

  return (0);
}


static int destroy(void *componentData)
{
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindowScrollBar *scrollBar = (kernelWindowScrollBar *) component->data;

  // Release all our memory
  if (scrollBar)
    kernelFree((void *) scrollBar);

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
			scrollBarType type, componentParameters *params)
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
  component->width = 20;

  scrollBar = kernelMalloc(sizeof(kernelWindowScrollBar));
  if (scrollBar == NULL)
    {
      kernelFree((void *) component);
      return (component = NULL);
    }

  scrollBar->type = type;
  scrollBar->displayPercent = 100;
  scrollBar->positionPercent = 0;
  
  component->data = (void *) scrollBar;

  // The functions
  component->draw = &draw;
  component->mouseEvent = &mouseEvent;
  component->destroy = &destroy;

  return (component);
}
