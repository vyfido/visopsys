//
//  Visopsys
//  Copyright (C) 1998-2003 J. Andrew McLaughlin
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
//  kernelWindowButtonComponent.c
//

// This code is for managing kernelWindowButtonComponent objects.
// These are just images that appear inside windows and buttons, etc


#include "kernelWindowManager.h"     // Our prototypes are here
#include "kernelMemoryManager.h"
#include "kernelMiscAsmFunctions.h"
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
  kernelWindowButtonComponent *button = (kernelWindowButtonComponent *)
    component->data;
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
  
  
  // If there is an image on the button, draw it centered on the button
  if (button->buttonImage.data != NULL)
    kernelGraphicDrawImage(buffer, (image *) &(button->buttonImage),
 			   component->xCoord, component->yCoord);

  // Draw the border last, since if there's an image on the button it's
  // normally the size of the whole button
  drawBorder(buffer, component, 1);

  return (0);
}


static int mouseEvent(void *componentData, kernelMouseStatus *mouseStatus)
{
  int status = 0;
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindow *window = (kernelWindow *) component->window;
  kernelGraphicBuffer *buffer = &(window->buffer);
  kernelWindowButtonComponent *button =
    (kernelWindowButtonComponent *) component->data;

  // First just take care of drawing any changes we need to do
  
  if ((mouseStatus->eventMask & MOUSE_UP) ||
      (mouseStatus->eventMask & MOUSE_DRAG))
    drawBorder(buffer, component, 1);

  else if (mouseStatus->eventMask & MOUSE_DOWN)
    // Reverse the border (show 'clicked')
    drawBorder(buffer, component, 0);

  kernelWindowManagerUpdateBuffer(buffer, component->xCoord,
				  component->yCoord, component->width,
				  component->height);

  if (mouseStatus->eventMask & MOUSE_UP)
    // Now call the button's action function.  We do this last because if the
    // callback is, for example, a 'destroy window' operation, we don't want to
    // be doing anything after our component is zapped.
    return (status = button->callBackFunction((void *) component));

  return (status);
}


static int erase(void *componentData)
{
  return (0);
}


static int destroy(void *componentData)
{
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindowButtonComponent *button =
    (kernelWindowButtonComponent *) component->data;

  // Release all our memory
  if (button != NULL)
    {
      if (button->buttonImage.data != NULL)
	kernelMemoryReleaseSystemBlock(button->buttonImage.data);
      kernelMemoryReleaseSystemBlock((void *) button);
    }
  kernelMemoryReleaseSystemBlock(componentData);

  return (0);
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


kernelWindowComponent *kernelWindowNewButtonComponent(kernelWindow *window,
		      unsigned width, unsigned height, const char *label,
		      image *buttonImage,
		      int (*callBackFunction)(void *))
{
  // Formats a kernelWindowComponent as a kernelWindowButtonComponent

  kernelWindowComponent *component = NULL;
  kernelWindowButtonComponent *button = NULL;
  kernelGraphicBuffer tmpBuffer;
  void *tmpImageData = NULL;
  unsigned bufferBytes = 0;
  int labelOrImageX = 0;
  int labelOrImageY = 0;

  // Check parameters.  It's okay for the image or label or callback to be NULL
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

  button = kernelMemoryRequestSystemBlock(sizeof(kernelWindowButtonComponent),
					  0, "button component");
  if (button == NULL)
    {
      kernelMemoryReleaseSystemBlock((void *) component);
      return (component = NULL);
    }

  // If the button has a label, copy it
  if (label != NULL)
    {
      strncpy((char *) button->label, label, MAX_LABEL_LENGTH);
      button->label[MAX_LABEL_LENGTH - 1] = '\0';
    }

  // If the button has an image, copy it
  if ((buttonImage != NULL) && (buttonImage->data != NULL))
    {
      // Get a temporary graphic buffer into which we draw the image
      tmpBuffer.width = width;
      tmpBuffer.height = height;
      bufferBytes = kernelGraphicCalculateAreaBytes(width, height);
      tmpBuffer.data =
	kernelMemoryRequestBlock(bufferBytes, 0, "temp graphic buffer");

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
 			     labelOrImageY);
      
      // Get a new (centered, correctly-sized) image from that
      kernelGraphicGetImage(&tmpBuffer, (image *) &(button->buttonImage),
 			    0, 0, width, height);

      // The GetImage routine returns the image data to us as a normal
      // memory block.  We need to ensure that it's in a system memory
      // block.
      tmpImageData =
	kernelMemoryRequestSystemBlock(button->buttonImage.dataLength, 0,
				       "button image data");
      if (tmpImageData != NULL)
	{
	  kernelMemCopy(button->buttonImage.data, tmpImageData,
			button->buttonImage.dataLength);
	  // Free the old image data
	  kernelMemoryReleaseBlock(button->buttonImage.data);
	  button->buttonImage.data = tmpImageData;
	}
      else
	{
	  kernelError(kernel_warn, "Unable to get system memory for button "
		      "image");
	  // Free the old image data
	  kernelMemoryReleaseBlock(button->buttonImage.data);
	  button->buttonImage.data = NULL;
	}

      // Free the temporary buffer
      kernelMemoryReleaseBlock(tmpBuffer.data);
    }
  else
    button->buttonImage.data = NULL;

  component->data = (void *) button;

  // The functions
  component->draw = &draw;
  component->mouseEvent = &mouseEvent;
  component->erase = &erase;
  component->destroy = &destroy;

  // The function to be called when we're pushed
  button->callBackFunction = callBackFunction;

  return (component);
}
