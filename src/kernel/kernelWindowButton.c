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
//  kernelWindowButton.c
//

// This code is for managing kernelWindowButton objects.

#include "kernelWindow.h"     // Our prototypes are here
#include "kernelMalloc.h"
#include "kernelMemoryManager.h"
#include "kernelMiscFunctions.h"
#include "kernelError.h"
#include <string.h>

static int borderThickness = 3;
static int borderShadingIncrement = 15;
static kernelAsciiFont *labelFont = NULL;


static int draw(void *componentData)
{
  // Draw the button component

  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindowButton *button = (kernelWindowButton *) component->data;
  kernelGraphicBuffer *buffer = (kernelGraphicBuffer *)
    &(((kernelWindow *) component->window)->buffer);

  // Draw the background of the button
  kernelGraphicDrawRect(buffer, (color *) &(component->parameters.background),
			draw_normal, component->xCoord, component->yCoord,
			component->width, component->height, 1, 1);

  // If there is a label on the button, draw it
  if (button->label)
    {
      kernelGraphicDrawText(buffer,
			    (color *) &(component->parameters.foreground),
			    (color *) &(component->parameters.background),
			    labelFont, (const char *) button->label,
			    draw_translucent,
	    (component->xCoord + ((component->width -
				   kernelFontGetPrintedWidth(labelFont,
				     (const char *) button->label)) / 2)),
	    (component->yCoord + ((component->height -
		   labelFont->charHeight) / 2)));
    }

  // If there is an image on the button, draw it centered on the button
  if (button->buttonImage.data)
    {
      unsigned tmpX, tmpY, tmpXoff = 0, tmpYoff = 0;
      tmpX = component->xCoord +
	((component->width - button->buttonImage.width) / 2);
      tmpY = component->yCoord +
	((component->height - button->buttonImage.height) / 2);
      
      if (button->buttonImage.width > (unsigned) component->width)
	tmpXoff = -((button->buttonImage.width - component->width) / 2);
      if (button->buttonImage.height > (unsigned) component->height)
	tmpYoff = -((button->buttonImage.height - component->height) / 2);

      kernelGraphicDrawImage(buffer, (image *) &(button->buttonImage),
			     draw_translucent, tmpX, tmpY, tmpXoff, tmpYoff,
			     component->width, component->height);
    }

  if (component->flags & WINFLAG_HASFOCUS)
    kernelGraphicDrawRect(buffer,
			  (color *) &(component->parameters.foreground),
			  draw_normal, (component->xCoord + borderThickness),
			  (component->yCoord + borderThickness),
			  (component->width - (borderThickness * 2)),
			  (component->height - (borderThickness * 2)),
			  1, 0);
  else
    kernelGraphicDrawRect(buffer,
			  (color *) &(component->parameters.background),
			  draw_normal, (component->xCoord + borderThickness),
			  (component->yCoord + borderThickness),
			  (component->width - (borderThickness * 2)),
			  (component->height - (borderThickness * 2)),
			  1, 0);

  // Draw the border last, since if there's an image on the button it's
  // normally the size of the whole button
  kernelGraphicDrawGradientBorder(buffer, component->xCoord, component->yCoord,
				  component->width, component->height,
				  borderThickness, (color *)
				  &(component->parameters.background),
				  borderShadingIncrement, draw_normal);
  return (0);
}


static int focus(void *componentData, int focus)
{
  // Just redraw.

  int status = 0;
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelGraphicBuffer *buffer = (kernelGraphicBuffer *)
    &(((kernelWindow *) component->window)->buffer);

  // Ignore the 'focus' argument.  This keeps the compiler happy.
  if (focus)
    {
    }

  if (component->draw)
    status = component->draw(componentData);

  kernelWindowUpdateBuffer(buffer, component->xCoord, component->yCoord,
			   component->width, component->height);
  return (status);
}


static int mouseEvent(void *componentData, windowEvent *event)
{
  int status = 0;
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindowButton *button = (kernelWindowButton *) component->data;
  kernelWindow *window = (kernelWindow *) component->window;
  kernelGraphicBuffer *buffer = &(window->buffer);

  // Just take care of drawing any changes we need to do
  
  if ((event->type == EVENT_MOUSE_LEFTUP) || (event->type == EVENT_MOUSE_DRAG))
    {
      kernelGraphicDrawGradientBorder(buffer, component->xCoord,
				      component->yCoord, component->width,
				      component->height, borderThickness,
				      (color *)
				      &(component->parameters.background),
				      borderShadingIncrement, draw_normal);
      button->state = 0;
    }
  else if (event->type == EVENT_MOUSE_LEFTDOWN)
    {
      // Reverse the border (show 'clicked')
      kernelGraphicDrawGradientBorder(buffer, component->xCoord,
				      component->yCoord, component->width,
				      component->height, borderThickness,
				      (color *)
				      &(component->parameters.background),
				      borderShadingIncrement, draw_reverse);
      button->state = 1;
    }

  kernelWindowUpdateBuffer(buffer, component->xCoord, component->yCoord,
			   component->width, component->height);
  return (status);
}


static int keyEvent(void *componentData, windowEvent *event)
{
  int status = 0;
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindowButton *button = (kernelWindowButton *) component->data;

  if ((event->type & EVENT_MASK_KEY) && (event->key == 10))
    {
      // If the button is not pushed, ignore this
      if ((event->type == EVENT_KEY_UP) && !(button->state))
	return (status = 0);

      if (event->type == EVENT_KEY_DOWN)
	event->type = EVENT_MOUSE_LEFTDOWN;
      if (event->type == EVENT_KEY_UP)
	event->type = EVENT_MOUSE_LEFTUP;

      status = mouseEvent(componentData, event);
    }

  return (status);
}


static int destroy(void *componentData)
{
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindowButton *button = (kernelWindowButton *) component->data;

  // Release all our memory
  if (button)
    {
      // If we have an image, release the image data
      if (button->buttonImage.data)
	{
	  kernelFree(button->buttonImage.data);
	  button->buttonImage.data = NULL;
	}

      // The button itself.
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


kernelWindowComponent *kernelWindowNewButton(volatile void *parent,
					     const char *label,
					     image *buttonImage,
					     componentParameters *params)
{
  // Formats a kernelWindowComponent as a kernelWindowButton

  int status = 0;
  kernelWindowComponent *component = NULL;
  kernelWindowButton *button = NULL;
  //color *background = NULL;

  // Check parameters.  It's okay for the image or label to be NULL
  if ((parent == NULL) || (params == NULL))
    return (component = NULL);

  if (labelFont == NULL)
    {
      // Try to load a nice-looking font
      status = kernelFontLoad(DEFAULT_VARIABLEFONT_MEDIUM_FILE,
			      DEFAULT_VARIABLEFONT_MEDIUM_NAME, &labelFont, 0);
      if (status < 0)
	// Font's not there, we suppose.  There's always a default.
	kernelFontGetDefault(&labelFont);
    }

  /*
  if (params->useDefaultBackground)
    {
      background = (color *) &(getWindow(parent)->background);
      params->background.red = background->red;
      params->background.green = background->green;
      params->background.blue = background->blue;
    }
  */

  // Get the basic component structure
  component = kernelWindowComponentNew(parent, params);
  if (component == NULL)
    return (component);

  // Now populate it
  component->type = buttonComponentType;
  component->flags |= (WINFLAG_CANFOCUS | WINFLAG_RESIZABLE);

  button = kernelMalloc(sizeof(kernelWindowButton));
  if (button == NULL)
    {
      kernelFree((void *) component);
      return (component = NULL);
    }

  // If the button has a label, copy it
  if (label)
    {
      strncpy((char *) button->label, label, WINDOW_MAX_LABEL_LENGTH);
      int tmp =
	(kernelFontGetPrintedWidth(labelFont, (const char *) button->label) +
	 (borderThickness * 2) + 6);

      if (tmp > component->width)
	component->width = tmp;
      tmp = (labelFont->charHeight + (borderThickness * 2) + 6);
      if (tmp > component->height)
	component->height = tmp;
    }

  // If the button has an image, copy it
  if (buttonImage && buttonImage->data)
    {
      kernelMemCopy(buttonImage, (void *) &(button->buttonImage),
		    sizeof(image));

      // Button images use pure green as the transparency color
      button->buttonImage.translucentColor.blue = 0;
      button->buttonImage.translucentColor.green = 255;
      button->buttonImage.translucentColor.red = 0;
      
      button->buttonImage.data = kernelMalloc(button->buttonImage.dataLength);
      if (button->buttonImage.data == NULL)
	{
	  kernelFree((void *) component);
	  return (component = NULL);
	}

      kernelMemCopy(buttonImage->data, button->buttonImage.data,
		    button->buttonImage.dataLength);
    }
  else
    button->buttonImage.data = NULL;

  component->data = (void *) button;

  // The functions
  component->draw = &draw;
  component->focus = &focus;
  component->mouseEvent = &mouseEvent;
  component->keyEvent = &keyEvent;
  component->destroy = &destroy;

  return (component);
}
