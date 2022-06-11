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
//  kernelWindowButton.c
//

// This code is for managing kernelWindowButton objects.

#include "kernelWindow.h"     // Our prototypes are here
#include "kernelMalloc.h"
#include "kernelMemory.h"
#include "kernelMisc.h"
#include "kernelError.h"
#include <string.h>

extern kernelWindowVariables *windowVariables;


static void setText(kernelWindowComponent *component, char *label, int length)
{
  kernelWindowButton *button = (kernelWindowButton *) component->data;
  kernelAsciiFont *labelFont = (kernelAsciiFont *) component->params.font;
  int borderThickness = windowVariables->border.thickness;

  strncpy((char *) button->label, label, length);
  int tmp =
    (kernelFontGetPrintedWidth(labelFont, (const char *) button->label) +
     (borderThickness * 2) + 6);

  if (tmp > component->width)
    component->width = tmp;
  tmp = (labelFont->charHeight + (borderThickness * 2) + 6);
  if (tmp > component->height)
    component->height = tmp;
}


static void drawFocus(kernelWindowComponent *component, int focus)
{
  color *drawColor = NULL;
  int borderThickness = windowVariables->border.thickness;

  if (focus)
    drawColor = (color *) &(component->params.foreground);
  else
    drawColor = (color *) &(component->params.background);

  kernelGraphicDrawRect(component->buffer, drawColor, draw_normal,
			(component->xCoord + borderThickness),
			(component->yCoord + borderThickness),
			(component->width - (borderThickness * 2)),
			(component->height - (borderThickness * 2)),
			1, 0);
  return;
}


static int draw(kernelWindowComponent *component)
{
  // Draw the button component

  kernelWindowButton *button = (kernelWindowButton *) component->data;
  kernelAsciiFont *labelFont = (kernelAsciiFont *) component->params.font;
  int borderThickness = windowVariables->border.thickness;
  int borderShadingIncrement = windowVariables->border.shadingIncrement;

  // Draw the background of the button
  kernelGraphicDrawRect(component->buffer,
			(color *) &(component->params.background),
			draw_normal, component->xCoord, component->yCoord,
			component->width, component->height, 1, 1);

  // If there is a label on the button, draw it
  if (button->label)
    {
      kernelGraphicDrawText(component->buffer,
			    (color *) &(component->params.foreground),
			    (color *) &(component->params.background),
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

      kernelGraphicDrawImage(component->buffer,
			     (image *) &(button->buttonImage),
			     draw_translucent, tmpX, tmpY, tmpXoff, tmpYoff,
			     component->width, component->height);
    }

  drawFocus(component, (component->flags & WINFLAG_HASFOCUS));

  // Draw the border last, since if there's an image on the button it's
  // normally the size of the whole button
  kernelGraphicDrawGradientBorder(component->buffer,
				  component->xCoord, component->yCoord,
				  component->width, component->height,
				  borderThickness, (color *)
				  &(component->params.background),
				  borderShadingIncrement, draw_normal,
				  border_all);
  return (0);
}


static int focus(kernelWindowComponent *component, int yesNo)
{

  drawFocus(component, yesNo);
  component->window
    ->update(component->window, component->xCoord, component->yCoord,
	     component->width, component->height);
  return (0);
}


static int setData(kernelWindowComponent *component, void *text, int length)
{
  // Set the button text
  
  setText(component, text, length);

  if (component->draw)
    draw(component);

  component->window
    ->update(component->window, component->xCoord, component->yCoord,
	     component->width, component->height);

  return (0);
}


static int mouseEvent(kernelWindowComponent *component, windowEvent *event)
{
  kernelWindowButton *button = (kernelWindowButton *) component->data;
  int borderThickness = windowVariables->border.thickness;
  int borderShadingIncrement = windowVariables->border.shadingIncrement;

  // Just take care of drawing any changes we need to do
  
  if ((event->type == EVENT_MOUSE_LEFTUP) || (event->type == EVENT_MOUSE_DRAG))
    {
      kernelGraphicDrawGradientBorder(component->buffer, component->xCoord,
				      component->yCoord, component->width,
				      component->height, borderThickness,
				      (color *)
				      &(component->params.background),
				      borderShadingIncrement, draw_normal,
				      border_all);
      button->state = 0;
    }
  else if (event->type == EVENT_MOUSE_LEFTDOWN)
    {
      // Reverse the border (show 'clicked')
      kernelGraphicDrawGradientBorder(component->buffer, component->xCoord,
				      component->yCoord, component->width,
				      component->height, borderThickness,
				      (color *)
				      &(component->params.background),
				      borderShadingIncrement, draw_reverse,
				      border_all);
      button->state = 1;
    }

  component->window
    ->update(component->window, component->xCoord, component->yCoord,
	     component->width, component->height);

  return (0);
}


static int keyEvent(kernelWindowComponent *component, windowEvent *event)
{
  int status = 0;
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

      status = mouseEvent(component, event);
    }

  return (status);
}


static int destroy(kernelWindowComponent *component)
{
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


kernelWindowComponent *kernelWindowNewButton(objectKey parent,
					     const char *label,
					     image *buttonImage,
					     componentParameters *params)
{
  // Formats a kernelWindowComponent as a kernelWindowButton

  kernelWindowComponent *component = NULL;
  kernelWindowButton *button = NULL;

  // Check parameters.  It's okay for the image or label to be NULL
  if ((parent == NULL) || (params == NULL))
    return (component = NULL);

  // Get the basic component structure
  component = kernelWindowComponentNew(parent, params);
  if (component == NULL)
    return (component);

  // If font is NULL, use the default
  if (component->params.font == NULL)
    component->params.font = windowVariables->font.varWidth.medium.font;

  // Now populate it
  component->type = buttonComponentType;
  component->flags |= (WINFLAG_CANFOCUS | WINFLAG_RESIZABLEX);

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
      int tmp =	(kernelFontGetPrintedWidth((kernelAsciiFont *)
					   component->params.font,
					   (const char *) button->label) +
		 (windowVariables->border.thickness * 2) + 6);

      if (tmp > component->width)
	component->width = tmp;
      tmp = (((kernelAsciiFont *) component->params.font)->charHeight +
	     (windowVariables->border.thickness * 2) + 6);
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

  component->minWidth = component->width;
  component->minHeight = component->height;
  component->data = (void *) button;

  // The functions
  component->draw = &draw;
  component->focus = &focus;
  component->setData = &setData;
  component->mouseEvent = &mouseEvent;
  component->keyEvent = &keyEvent;
  component->destroy = &destroy;

  return (component);
}
