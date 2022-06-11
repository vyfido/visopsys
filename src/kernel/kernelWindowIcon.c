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
//  kernelWindowIcon.c
//

// This code is for managing kernelWindowIcon objects.
// These are just images that appear inside windows and buttons, etc

#include "kernelWindowManager.h"     // Our prototypes are here
#include "kernelParameters.h"
#include "kernelMalloc.h"
#include "kernelMultitasker.h"
#include "kernelUser.h"
#include "kernelMiscFunctions.h"
#include "kernelError.h"
#include <string.h>

static kernelAsciiFont *defaultFont = NULL;


static int draw(void *componentData)
{
  // Draw the image component

  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelGraphicBuffer *buffer =
    &(((kernelWindow *) component->window)->buffer);
  kernelWindowIcon *iconComponent = (kernelWindowIcon *) component->data;
  int count;

  int imageX = (component->xCoord + ((component->width -
				      iconComponent->iconImage.width) / 2));
  int labelX = (component->xCoord + (component->width -
				     iconComponent->labelWidth) / 2);
  int labelY = (component->yCoord + iconComponent->iconImage.height + 3);

  // Draw the icon image
  kernelGraphicDrawImage(buffer, (image *) &(iconComponent->iconImage),
			 draw_translucent, imageX, component->yCoord,
			 0, 0, 0, 0);

  // Clear the text area
  kernelGraphicClearArea(buffer, (color *) &(component->parameters.background),
			 labelX, labelY, (iconComponent->labelWidth + 2),
			 ((iconComponent->labelLines * defaultFont->charHeight)
			  + 2));

  for (count = 0; count < iconComponent->labelLines; count ++)
    {
      labelX = (component->xCoord + ((component->width -
			      kernelFontGetPrintedWidth(defaultFont, (char *)
				iconComponent->label[count])) / 2) + 1);
      labelY = (component->yCoord + iconComponent->iconImage.height + 4 +
		(defaultFont->charHeight * count));
      
      kernelGraphicDrawText(buffer,
			    (color *) &(component->parameters.foreground),
			    (color *) &(component->parameters.background),
			    defaultFont, (char *) iconComponent->label[count],
			    draw_normal, labelX, labelY);
    }

  if (component->parameters.hasBorder)
    component->drawBorder((void *) component, 1);

  return (0);
}


static int mouseEvent(void *componentData, windowEvent *event)
{
  // Launch a thread to load and run the program

  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindow *window = (kernelWindow *) component->window;
  kernelGraphicBuffer *buffer = &(window->buffer);
  kernelWindowIcon *iconComponent = (kernelWindowIcon *) component->data;
  static int dragging = 0;
  static windowEvent dragEvent;

  int imageX = (component->xCoord +
		((component->width - iconComponent->iconImage.width) / 2));

  // Is the icon being dragged around?

  if (dragging)
    {
      if (event->type == EVENT_MOUSE_DRAG)
	{
	  // The icon is still moving

	  // Erase the xor'ed outline
	  kernelWindowRedrawArea((window->xCoord + component->xCoord),
				 (window->yCoord + component->yCoord),
				 component->width, component->height);	      
	      
	  // Set the new position
	  component->xCoord += (event->xPosition - dragEvent.xPosition);
	  
	  component->yCoord += (event->yPosition - dragEvent.yPosition);

	  // Draw an xor'ed outline
	  kernelGraphicDrawRect(NULL, &((color) { 255, 255, 255 }),
				draw_xor, (window->xCoord + component->xCoord),
				(window->yCoord + component->yCoord),
				component->width, component->height, 1, 0);

	  // Save a copy of the dragging event
	  kernelMemCopy(event, &dragEvent, sizeof(windowEvent));
	}

      else
	{
	  // The move is finished

	  component->flags |= WINFLAG_VISIBLE;

	  // Erase the xor'ed outline
	  kernelGraphicDrawRect(NULL, &((color) { 255, 255, 255 }),
				draw_xor, (window->xCoord + component->xCoord),
				(window->yCoord + component->yCoord),
				component->width, component->height, 1, 0);

	  // Re-render it at the new location
	  if (component->draw)
	    component->draw((void *) component);
	  kernelWindowUpdateBuffer(&(window->buffer), component->xCoord,
				   component->yCoord, component->width,
				   component->height);

	  // If the new location intersects any other components of the
	  // window, we need to focus the icon
	  kernelWindowComponentFocus(component);

	  dragging = 0;
	}
	  
      // Redraw the mouse
      kernelMouseDraw();

      return (0);
    }

  else if (event->type == EVENT_MOUSE_DRAG)
    {
      // The icon has started moving
		  
      // Don't show it while it's moving
      component->flags &= ~WINFLAG_VISIBLE;
      
      window->drawClip((void *) window, component->xCoord, component->yCoord,
		       component->width, component->height);

      // Draw an xor'ed outline
      kernelGraphicDrawRect(NULL, &((color) { 255, 255, 255 }),
			    draw_xor, (window->xCoord + component->xCoord),
			    (window->yCoord + component->yCoord),
			    component->width, component->height, 1, 0);

      // Save a copy of the dragging event
      kernelMemCopy(event, &dragEvent, sizeof(windowEvent));
      dragging = 1;

      return (0);
    }

  else
    {
      // Just a click

      if (event->type == EVENT_MOUSE_LEFTUP)
	{
	  kernelGraphicDrawRect(buffer, &((color) { 255, 255, 255 }),
				draw_xor, imageX, component->yCoord,
				iconComponent->iconImage.width,
				iconComponent->iconImage.height, 1, 1);
	}
      else if (event->type == EVENT_MOUSE_LEFTDOWN)
	{
	  // Show the icon being clicked
	  kernelGraphicDrawRect(buffer, &((color) { 255, 255, 255 }),
				draw_xor, imageX, component->yCoord,
				iconComponent->iconImage.width,
				iconComponent->iconImage.height, 1, 1);
	}

      kernelWindowUpdateBuffer(buffer, imageX, component->yCoord,
			       iconComponent->iconImage.width,
			       iconComponent->iconImage.height);
      return (0);
    }
}


static int destroy(void *componentData)
{
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindowIcon *iconComponent = (kernelWindowIcon *) component->data;

  // Release all our memory
  if (iconComponent)
    {
      if (iconComponent->iconImage.data)
	{
	  kernelFree(iconComponent->iconImage.data);
	  iconComponent->iconImage.data = NULL;
	}

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


kernelWindowComponent *kernelWindowNewIcon(volatile void *parent,
					   image *imageCopy, const char *label,
					   const char *command,
					   componentParameters *params)
{
  // Formats a kernelWindowComponent as a kernelWindowIcon

  int status = 0;
  kernelWindowComponent *component = NULL;
  kernelWindowIcon *iconComponent = NULL;
  int labelSplit = 0;
  int count1, count2;

  // Check parameters
  if ((parent == NULL) || (imageCopy == NULL) || (label == NULL) ||
      (params == NULL))
    return (component = NULL);

  // Get the basic component structure
  component = kernelWindowComponentNew(parent, params);
  if (component == NULL)
    return (component);

  // If default colors are requested, override the standard component colors
  // with the ones we prefer
  if (component->parameters.useDefaultForeground)
    {
      component->parameters.foreground.blue = 255;
      component->parameters.foreground.green = 255;
      component->parameters.foreground.red = 255;
    }
  if (component->parameters.useDefaultBackground)
    {
      component->parameters.background.blue = 0xAB;
      component->parameters.background.green = 0x5D;
      component->parameters.background.red = 0x28;
    }

  // Copy all the relevant data into our memory
  iconComponent = kernelMalloc(sizeof(kernelWindowIcon));
  if (iconComponent == NULL)
    {
      kernelFree((void *) component);
      return (component = NULL);
    }

  kernelMemCopy(imageCopy, (image *) &(iconComponent->iconImage),
		sizeof(image));

  // Icons use pure green as the transparency color
  iconComponent->iconImage.translucentColor.blue = 0;
  iconComponent->iconImage.translucentColor.green = 255;
  iconComponent->iconImage.translucentColor.red = 0;

  strncpy((char *) iconComponent->label[0], label, WINDOW_MAX_LABEL_LENGTH);
  iconComponent->label[0][WINDOW_MAX_LABEL_LENGTH - 1] = '\0';

  if (command)
    {
      strncpy((char *) iconComponent->command, command, 128);
      iconComponent->command[127] = '\0';
    }

  if (defaultFont == NULL)
    {
      // Try to get our favorite font
      status = kernelFontLoad(DEFAULT_VARIABLEFONT_SMALL_FILE,
			      DEFAULT_VARIABLEFONT_SMALL_NAME,
			      &defaultFont, 0);
      if (status < 0)
	// Font's not there, we suppose.  There's always a default.
	kernelFontGetDefault(&defaultFont);
    }

  iconComponent->labelWidth =
    kernelFontGetPrintedWidth(defaultFont, (char *) iconComponent->label[0]);
  iconComponent->labelLines = 1;

  // Copy the image data
  iconComponent->iconImage.data = kernelMalloc(imageCopy->dataLength);
  if (iconComponent->iconImage.data)
    kernelMemCopy(imageCopy->data, iconComponent->iconImage.data,
		  imageCopy->dataLength);

  // Is the label too wide?  If so, we will break it into 2 lines
  if (iconComponent->labelWidth > 90)
    {
      labelSplit = (strlen((char *) iconComponent->label[0]) / 2);

      // Try to locate the 'space' character closest to the center of the
      // string (if any).
      count1 = labelSplit;
      count2 = labelSplit;
      
      while ((count1 > 0) &&
	     (count2 < strlen((char *) iconComponent->label[0])))
	{
	  if (iconComponent->label[0][count1] == ' ')
	    {
	      labelSplit = count1;
	      break;
	    }
	  else if (iconComponent->label[0][count2] == ' ')
	    {
	      labelSplit = count2;
	      break;
	    }

	  count1--;
	  count2++;
	}

      // Split the string at labelSplit

      if (iconComponent->label[0][labelSplit] == ' ')
	{
	  // Skip past the space
	  iconComponent->label[0][labelSplit] = '\0';
	  labelSplit++;
	}

      strncpy((char *) iconComponent->label[1],
	      (char *) (iconComponent->label[0] + labelSplit), 
	      WINDOW_MAX_LABEL_LENGTH);

      iconComponent->label[0][labelSplit] = '\0';

      count1 = kernelFontGetPrintedWidth(defaultFont, (char *)
					 iconComponent->label[0]);
      count2 = kernelFontGetPrintedWidth(defaultFont, (char *)
					 iconComponent->label[1]);
	
      if (count1 > count2)
	iconComponent->labelWidth = count1;
      else
	iconComponent->labelWidth = count2;

      iconComponent->labelLines = 2;
    }

  // Now populate the main component
  component->type = iconComponentType;
  if (imageCopy->width > iconComponent->labelWidth)
    component->width = imageCopy->width;
  else
    component->width = (iconComponent->labelWidth + 2);
  component->height = ((imageCopy->height + 5 + (defaultFont->charHeight *
						 iconComponent->labelLines)));
  
  component->data = (void *) iconComponent;

  // The functions
  component->draw = &draw;
  component->mouseEvent = &mouseEvent;
  component->destroy = &destroy;

  return (component);
}
