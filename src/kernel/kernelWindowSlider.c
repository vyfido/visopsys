//
//  Visopsys
//  Copyright (C) 1998-2013 J. Andrew McLaughlin
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
//  kernelWindowSlider.c
//

// This code is for managing kernelWindowSlider objects.

#include "kernelWindow.h"     // Our prototypes are here
#include <string.h>

static int (*saveDraw) (kernelWindowComponent *) = NULL;

extern kernelWindowVariables *windowVariables;


static int draw(kernelWindowComponent *component)
{
  // First draw the underlying scrollbar component, and then if it has the
  // focus, draw another border

  int status = 0;

  if (saveDraw)
    {
      status = saveDraw(component);
      if (status < 0)
	return (status);
    }

  if ((component->params.flags & WINDOW_COMPFLAG_HASBORDER) ||
      (component->flags & WINFLAG_HASFOCUS))
    component->drawBorder(component, 1);

  return (status = 0);
}


static int focus(kernelWindowComponent *component, int yesNo)
{
  component->drawBorder(component, yesNo);

  component->window->update(component->window, (component->xCoord - 2),
			    (component->yCoord - 2), (component->width + 4),
			    (component->height + 4));
  return (0);
}


static int keyEvent(kernelWindowComponent *component, windowEvent *event)
{
  kernelWindowSlider *slider = component->data;

  if (event->type != EVENT_KEY_DOWN)
    return (0);

  if (!component->mouseEvent)
    return (0);

  // If the key event is for an applicable key, convert the event into
  // the appropriate kind of mouse event to make the scrollbar do what
  // we want

  event->xPosition = (component->window->xCoord + component->xCoord +
		      windowVariables->border.thickness);
  event->yPosition = (component->window->yCoord + component->yCoord +
		      windowVariables->border.thickness);

  switch (event->key)
    {
    case ASCII_CRSRLEFT:
    case ASCII_CRSRRIGHT:
      if (slider->type == scrollbar_horizontal)
	{
	  event->type = EVENT_MOUSE_DRAG;
	  event->xPosition += (slider->sliderX + 2);
	  component->mouseEvent(component, event);
      
	  if (event->key == ASCII_CRSRLEFT)
	    // Cursor left, so we make it like it was dragged left by 1 pixel
	    event->xPosition -= 1;
	  else
	    // Cursor right, so we make it like it was dragged right by 1 pixel
	    event->xPosition += 1;

	  event->type = EVENT_MOUSE_DRAG;
	  component->mouseEvent(component, event);
	  event->type = EVENT_MOUSE_LEFTUP;
	  component->mouseEvent(component, event);
	}
      break;

    case ASCII_CRSRUP:
    case ASCII_CRSRDOWN:
      if (slider->type == scrollbar_vertical)
	{
	  event->type = EVENT_MOUSE_DRAG;
	  event->yPosition += (slider->sliderY + 2);
	  component->mouseEvent(component, event);
      
	  if (event->key == ASCII_CRSRUP)
	    // Cursor up, so we make it like it was dragged up by 1 pixel
	    event->yPosition -= 1;
	  else
	    // Cursor down, so we make it like it was dragged down by 1 pixel
	    event->yPosition += 1;

	  event->type = EVENT_MOUSE_DRAG;
	  component->mouseEvent(component, event);
	  event->type = EVENT_MOUSE_LEFTUP;
	  component->mouseEvent(component, event);
	}
      break;
	  
    case ASCII_PAGEUP:
    case ASCII_PAGEDOWN:
      if (slider->type == scrollbar_vertical)
	{
	  event->type = EVENT_MOUSE_LEFTDOWN;

	  if (event->key == ASCII_PAGEUP)
	    // Page up, so we make it like there was a click above
	    event->yPosition += 1;
	  else
	    // Page down, so we make it like there was a click below
	    event->yPosition += (slider->sliderY + slider->sliderHeight + 1);

	  component->mouseEvent(component, event);
	}
      break;
	  
    default:
      break;
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


kernelWindowComponent *kernelWindowNewSlider(objectKey parent,
					     scrollBarType type, int width,
					     int height,
					     componentParameters *params)
{
  // Formats a kernelWindowComponent as a kernelWindowSlider

  kernelWindowComponent *component = NULL;

  // Check parameters.
  if ((parent == NULL) || (params == NULL))
    return (component = NULL);

  // Get the underlying scrollbar component
  component = kernelWindowNewScrollBar(parent, type, width, height, params);
  if (component == NULL)
    return (component);

  // Change applicable things
  component->subType = sliderComponentType;
  component->flags |= WINFLAG_CANFOCUS;

  // The functions
  saveDraw = component->draw;
  component->draw = &draw;
  component->focus = &focus;
  component->keyEvent = &keyEvent;

  return (component);
}
