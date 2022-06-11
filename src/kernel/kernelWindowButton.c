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
//  kernelWindowButton.c
//

// This code is for managing kernelWindowButton objects.
// These are just images that appear inside windows and buttons, etc


#include "kernelWindowManager.h"     // Our prototypes are here
#include "kernelMalloc.h"
#include "kernelMemoryManager.h"
#include "kernelMiscFunctions.h"
#include "kernelError.h"
#include <string.h>


static int borderThickness = 3;
static int borderShadingIncrement = 15;


static void drawBorder(kernelGraphicBuffer *buffer,
		       kernelWindowComponent *component, int converse)
{
  // Draws the plain border around the window
  
  int greyColor = 0;
  color drawColor;
  int count;

  // These are the starting points of the 'inner' border lines
  int leftX = (component->xCoord + borderThickness);
  int rightX = (component->xCoord + component->width - borderThickness - 1);
  int topY = (component->yCoord + borderThickness);
  int bottomY = (component->yCoord + component->height - borderThickness - 1);

  // The top and left
  for (count = borderThickness; count > 0; count --)
    {
      if (converse)
	{
	  greyColor = (DEFAULT_GREY + (count * borderShadingIncrement));
	  if (greyColor > 255)
	    greyColor = 255;
	}
      else
	{
	  greyColor = (DEFAULT_GREY - (count * borderShadingIncrement));
	  if (greyColor < 0)
	    greyColor = 0;
	}

      drawColor.red = greyColor;
      drawColor.green = greyColor;
      drawColor.blue = greyColor;

      // Top
      kernelGraphicDrawLine(buffer, &drawColor, draw_normal, 
			    (leftX - count), (topY - count),
			    (rightX + count), (topY - count));
      // Left
      kernelGraphicDrawLine(buffer, &drawColor, draw_normal,
			    (leftX - count), (topY - count), (leftX - count),
			    (bottomY + count));
    }

  // The bottom and right
  for (count = borderThickness; count > 0; count --)
    {
      if (converse)
	{
	  greyColor = (DEFAULT_GREY - (count * borderShadingIncrement));
	  if (greyColor < 0)
	    greyColor = 0;
	}
      else
	{
	  greyColor = (DEFAULT_GREY + (count * borderShadingIncrement));
	  if (greyColor > 255)
	    greyColor = 255;
	}

      drawColor.red = greyColor;
      drawColor.green = greyColor;
      drawColor.blue = greyColor;

      // Bottom
      kernelGraphicDrawLine(buffer, &drawColor, draw_normal,
			    (leftX - count), (bottomY + count),
			    (rightX + count), (bottomY + count));
      // Right
      kernelGraphicDrawLine(buffer, &drawColor, draw_normal,
			    (rightX + count), (topY - count),
			    (rightX + count), (bottomY + count));

      greyColor += borderShadingIncrement;
    }

  return;
}


static int draw(void *componentData)
{
  // Draw the button component

  color foreground = { 0, 0, 0 };
  color background = { DEFAULT_GREY, DEFAULT_GREY, DEFAULT_GREY };
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindowButton *button = (kernelWindowButton *) component->data;
  kernelGraphicBuffer *buffer = (kernelGraphicBuffer *)
    &(((kernelWindow *) component->window)->buffer);

  if (!component->parameters.useDefaultForeground)
    {
      // Use user-supplied color
      foreground.red = component->parameters.foreground.red;
      foreground.green = component->parameters.foreground.green;
      foreground.blue = component->parameters.foreground.blue;
    }
  if (!component->parameters.useDefaultBackground)
    {
      // Use user-supplied color
      background.red = component->parameters.background.red;
      background.green = component->parameters.background.green;
      background.blue = component->parameters.background.blue;
    }
  
  // Draw the background of the button
  kernelGraphicDrawRect(buffer, &background, draw_normal,
			component->xCoord, component->yCoord,
			component->width, component->height, 1, 1);

  // If there is a label on the button, draw it
  if (button->label != NULL)
    {
      kernelWindowTextLabel *tmpLabel =
	(kernelWindowTextLabel *) button->label->data;
      
      kernelGraphicDrawText(buffer, &foreground, tmpLabel->font,
			    tmpLabel->text, draw_normal,
			    (component->xCoord + ((component->width - button
						   ->label->width) / 2)),
			    (component->yCoord + ((component->height - button
						   ->label->height) / 2)));
    }

  // If there is an image on the button, draw it centered on the button
  if (button->buttonImage.data != NULL)
    kernelGraphicDrawImage(buffer, (image *) &(button->buttonImage),
 			   component->xCoord, component->yCoord, 0, 0, 0, 0);

  // Draw the border last, since if there's an image on the button it's
  // normally the size of the whole button
  drawBorder(buffer, component, 1);

  return (0);
}


static int mouseEvent(void *componentData, windowEvent *event)
{
  int status = 0;
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindow *window = (kernelWindow *) component->window;
  kernelGraphicBuffer *buffer = &(window->buffer);

  // Just take care of drawing any changes we need to do
  
  if ((event->type & EVENT_MOUSE_UP) || (event->type & EVENT_MOUSE_DRAG))
    drawBorder(buffer, component, 1);

  else if (event->type & EVENT_MOUSE_DOWN)
    // Reverse the border (show 'clicked')
    drawBorder(buffer, component, 0);

  kernelWindowManagerUpdateBuffer(buffer, component->xCoord,
				  component->yCoord, component->width,
				  component->height);
  return (status);
}


static int erase(void *componentData)
{
  return (0);
}


static int destroy(void *componentData)
{
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindowButton *button = (kernelWindowButton *) component->data;

  // Release all our memory
  if (button != NULL)
    {
      // If we have a label component, destroy it.
      if (button->label != NULL)
	kernelWindowDestroyComponent(button->label);

      // If we have an image, release the image data
      if (button->buttonImage.data != NULL)
	kernelFree(button->buttonImage.data);

      // The button itself.
      kernelFree((void *) button);
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


kernelWindowComponent *kernelWindowNewButton(kernelWindow *window,
				      unsigned width, unsigned height,
				      kernelWindowComponent *label,
				      image *buttonImage)
{
  // Formats a kernelWindowComponent as a kernelWindowButton

  kernelWindowComponent *component = NULL;
  kernelWindowButton *button = NULL;
  kernelGraphicBuffer tmpBuffer;
  void *tmpImageData = NULL;
  unsigned bufferBytes = 0;
  int labelOrImageX = 0;
  int labelOrImageY = 0;

  // Check parameters.  It's okay for the image or label to be NULL
  if (window == NULL)
    return (component = NULL);

  // Get the basic component structure
  component = kernelWindowNewComponent();
  if (component == NULL)
    return (component);

  // Now populate it
  component->type = windowButtonComponent;
  component->width = width;
  component->height = height;

  button = kernelMalloc(sizeof(kernelWindowButton));
  if (button == NULL)
    {
      kernelFree((void *) component);
      return (component = NULL);
    }

  // If the button has a label, copy it
  if (label != NULL)
    {
      button->label = label;
      unsigned tmp = (button->label->width + (borderThickness * 2));
      if (tmp > component->width)
	component->width = tmp;
      tmp = (button->label->height + (borderThickness * 2));
      if (tmp > component->height)
	component->height = tmp;
    }

  // If the button has an image, copy it
  if ((buttonImage != NULL) && (buttonImage->data != NULL))
    {
      // Get a temporary graphic buffer into which we draw the image
      tmpBuffer.width = width;
      tmpBuffer.height = height;
      bufferBytes = kernelGraphicCalculateAreaBytes(width, height);
      tmpBuffer.data = kernelMalloc(bufferBytes);

      if (buttonImage->width > width)
	labelOrImageX = -((buttonImage->width - width) / 2);
      else
	labelOrImageX = ((width - buttonImage->width) / 2);
      if (buttonImage->height > height)
	labelOrImageY = -((buttonImage->height - height) / 2);
      else
	labelOrImageY = ((height - buttonImage->height) / 2);
      
      // Button images use pure green as the transparency color
      buttonImage->isTranslucent = 1;
      buttonImage->translucentColor.blue = 0;
      buttonImage->translucentColor.green = 255;
      buttonImage->translucentColor.red = 0;
      
      kernelGraphicDrawRect(&tmpBuffer, &((color)
      { DEFAULT_GREY, DEFAULT_GREY, DEFAULT_GREY }),
			    draw_normal, 0, 0, width, height, 1, 1);

      kernelGraphicDrawImage(&tmpBuffer, buttonImage, labelOrImageX,
 			     labelOrImageY, 0, 0, 0, 0);
      
      // Get a new (centered, correctly-sized) image from that
      kernelGraphicGetImage(&tmpBuffer, (image *) &(button->buttonImage), 0, 0,
			    width, height);

      // The GetImage routine returns the image data to us as a normal
      // memory block.  We need to ensure that it's in a system memory
      // block.
      tmpImageData = kernelMalloc(button->buttonImage.dataLength);
      if (tmpImageData != NULL)
	{
	  kernelMemCopy(button->buttonImage.data, tmpImageData,
			button->buttonImage.dataLength);
	  // Free the old image data
	  kernelMemoryRelease(button->buttonImage.data);
	  button->buttonImage.data = tmpImageData;
	}
      else
	{
	  kernelError(kernel_warn, "Unable to get system memory for button "
		      "image");
	  // Free the old image data
	  kernelMemoryRelease(button->buttonImage.data);
	  button->buttonImage.data = NULL;
	}

      // Free the temporary buffer
      kernelFree(tmpBuffer.data);
    }
  else
    button->buttonImage.data = NULL;

  component->data = (void *) button;

  // The functions
  component->draw = &draw;
  component->mouseEvent = &mouseEvent;
  component->erase = &erase;
  component->destroy = &destroy;

  return (component);
}
