//
//  Visopsys
//  Copyright (C) 1998-2006 J. Andrew McLaughlin
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

#include "kernelWindow.h"     // Our prototypes are here
#include "kernelParameters.h"
#include "kernelMalloc.h"
#include "kernelMultitasker.h"
#include "kernelUser.h"
#include "kernelMisc.h"
#include "kernelError.h"
#include <string.h>
#include <stdlib.h>

extern kernelWindowVariables *windowVariables;


static int draw(kernelWindowComponent *component)
{
  // Draw the image component

  kernelWindowIcon *icon = component->data;
  kernelAsciiFont *font = (kernelAsciiFont *) component->params.font;
  int count;

  int imageX =
    (component->xCoord + ((component->width - icon->iconImage.width) / 2));
  int labelX = (component->xCoord + (component->width - icon->labelWidth) / 2);
  int labelY = (component->yCoord + icon->iconImage.height + 3);

  // Draw the icon image
  kernelGraphicDrawImage(component->buffer, (image *) &(icon->iconImage),
			 draw_translucent, imageX, component->yCoord,
			 0, 0, 0, 0);

  // Clear the text area
  kernelGraphicClearArea(component->buffer,
			 (color *) &(component->params.background),
			 labelX, labelY, (icon->labelWidth + 2),
			 ((icon->labelLines * font->charHeight) + 2));

  for (count = 0; count < icon->labelLines; count ++)
    {
      labelX = (component->xCoord + ((component->width -
			      kernelFontGetPrintedWidth(font, (char *)
						icon->label[count])) / 2) + 1);
      labelY = (component->yCoord + icon->iconImage.height + 4 +
		(font->charHeight * count));
      
      kernelGraphicDrawText(component->buffer,
			    (color *) &(component->params.foreground),
			    (color *) &(component->params.background),
			    font, (char *) icon->label[count],
			    draw_normal, labelX, labelY);
    }

  if (component->params.flags & WINDOW_COMPFLAG_HASBORDER)
    component->drawBorder(component, 1);

  return (0);
}


static int mouseEvent(kernelWindowComponent *component, windowEvent *event)
{
  // Launch a thread to load and run the program

  kernelWindowIcon *icon = component->data;
  static int dragging = 0;
  static windowEvent dragEvent;

  int imageX =
    (component->xCoord + ((component->width - icon->iconImage.width) / 2));

  // Is the icon being dragged around?

  if (dragging)
    {
      if (event->type == EVENT_MOUSE_DRAG)
	{
	  // The icon is still moving

	  // Erase the xor'ed outline
	  kernelWindowRedrawArea((component->window->xCoord +
				  component->xCoord),
				 (component->window->yCoord +
				  component->yCoord),
				 component->width, component->height);	      
	      
	  // Set the new position
	  component->xCoord += (event->xPosition - dragEvent.xPosition);
	  
	  component->yCoord += (event->yPosition - dragEvent.yPosition);

	  // Draw an xor'ed outline
	  kernelGraphicDrawRect(NULL, &((color) { 255, 255, 255 }),
				draw_xor,
				(component->window->xCoord +
				 component->xCoord),
				(component->window->yCoord +
				 component->yCoord),
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
				draw_xor,
				(component->window->xCoord +
				 component->xCoord),
				(component->window->yCoord +
				 component->yCoord),
				component->width, component->height, 1, 0);

	  icon->selected = 0;

	  // Re-render it at the new location
	  if (component->draw)
	    component->draw(component);

	  component->window
	    ->update(component->window, component->xCoord, component->yCoord,
		     component->width, component->height);

	  // If the new location intersects any other components of the
	  // window, we need to focus the icon
	  kernelWindowComponentFocus(component);

	  dragging = 0;
	}
	  
      // Redraw the mouse
      kernelMouseDraw();
    }

  else if (event->type == EVENT_MOUSE_DRAG)
    {
      // The icon has started moving
		  
      // Don't show it while it's moving
      component->flags &= ~WINFLAG_VISIBLE;

      if (component->window->drawClip)
	component->window
	  ->drawClip(component->window, component->xCoord, component->yCoord,
		     component->width, component->height);

      // Draw an xor'ed outline
      kernelGraphicDrawRect(NULL, &((color) { 255, 255, 255 }),
			    draw_xor,
			    (component->window->xCoord + component->xCoord),
			    (component->window->yCoord + component->yCoord),
			    component->width, component->height, 1, 0);

      // Save a copy of the dragging event
      kernelMemCopy(event, &dragEvent, sizeof(windowEvent));
      dragging = 1;
    }

  else
    {
      // Just a click

      if (((event->type == EVENT_MOUSE_LEFTDOWN) && !icon->selected) ||
	  ((event->type == EVENT_MOUSE_LEFTUP) && icon->selected))
	{
	  kernelGraphicDrawRect(component->buffer,
				&((color) { 255, 255, 255 }),
				draw_xor, imageX, component->yCoord,
				icon->iconImage.width,
				icon->iconImage.height, 1, 1);

	  if ((event->type == EVENT_MOUSE_LEFTDOWN) && !icon->selected)
	    icon->selected = 1;
	  else
	    icon->selected = 0;

	  component->window
	    ->update(component->window, imageX, component->yCoord,
		     icon->iconImage.width, icon->iconImage.height);
	}
    }
      
  return (0);
}


static int destroy(kernelWindowComponent *component)
{
  kernelWindowIcon *icon = component->data;

  // Release all our memory
  if (icon)
    {
      if (icon->iconImage.data)
	{
	  kernelFree(icon->iconImage.data);
	  icon->iconImage.data = NULL;
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


kernelWindowComponent *kernelWindowNewIcon(objectKey parent, image *imageCopy,
					   const char *label,
					   componentParameters *params)
{
  // Formats a kernelWindowComponent as a kernelWindowIcon

  kernelWindowComponent *component = NULL;
  kernelWindowIcon *icon = NULL;
  int labelSplit = 0;
  unsigned count1, count2;

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
  if (!(component->params.flags & WINDOW_COMPFLAG_CUSTOMFOREGROUND))
    {
      component->params.foreground.blue = 255;
      component->params.foreground.green = 255;
      component->params.foreground.red = 255;
      component->params.flags |= WINDOW_COMPFLAG_CUSTOMFOREGROUND;
    }
  if (!(component->params.flags & WINDOW_COMPFLAG_CUSTOMBACKGROUND))
    {
      component->params.background.blue = 0xAB;
      component->params.background.green = 0x5D;
      component->params.background.red = 0x28;
      component->params.flags |= WINDOW_COMPFLAG_CUSTOMBACKGROUND;
    }

  // Always use our font
  component->params.font = windowVariables->font.varWidth.small.font;

  // Copy all the relevant data into our memory
  icon = kernelMalloc(sizeof(kernelWindowIcon));
  if (icon == NULL)
    {
      kernelFree((void *) component);
      return (component = NULL);
    }

  kernelMemCopy(imageCopy, (image *) &(icon->iconImage), sizeof(image));

  // Icons use pure green as the transparency color
  icon->iconImage.translucentColor.blue = 0;
  icon->iconImage.translucentColor.green = 255;
  icon->iconImage.translucentColor.red = 0;

  strncpy((char *) icon->label[0], label, WINDOW_MAX_LABEL_LENGTH);
  icon->label[0][WINDOW_MAX_LABEL_LENGTH - 1] = '\0';

  // Copy the image data
  icon->iconImage.data = kernelMalloc(imageCopy->dataLength);
  if (icon->iconImage.data)
    kernelMemCopy(imageCopy->data, icon->iconImage.data,
		  imageCopy->dataLength);

  icon->labelWidth =
    kernelFontGetPrintedWidth(((kernelAsciiFont *) component->params.font),
			      (char *) icon->label[0]);
  icon->labelLines = 1;

  // Is the label too wide?  If so, we will break it into 2 lines
  if (icon->labelWidth > 90)
    {
      labelSplit = (strlen((char *) icon->label[0]) / 2);

      // Try to locate the 'space' character closest to the center of the
      // string (if any).
      count1 = labelSplit;
      count2 = labelSplit;
      
      while ((count1 > 0) && (count2 < strlen((char *) icon->label[0])))
	{
	  if (icon->label[0][count1] == ' ')
	    {
	      labelSplit = count1;
	      break;
	    }
	  else if (icon->label[0][count2] == ' ')
	    {
	      labelSplit = count2;
	      break;
	    }

	  count1--;
	  count2++;
	}

      // Split the string at labelSplit

      if (icon->label[0][labelSplit] == ' ')
	{
	  // Skip past the space
	  icon->label[0][labelSplit] = '\0';
	  labelSplit++;
	}

      strncpy((char *) icon->label[1], (char *) (icon->label[0] + labelSplit), 
	      WINDOW_MAX_LABEL_LENGTH);

      icon->label[0][labelSplit] = '\0';

      count1 = kernelFontGetPrintedWidth(((kernelAsciiFont *)
					  component->params.font),
					 (char *) icon->label[0]);
      count2 = kernelFontGetPrintedWidth(((kernelAsciiFont *)
					  component->params.font),
					 (char *) icon->label[1]);
      icon->labelWidth = max(count1, count2);
      icon->labelLines = 2;
    }

  // Now populate the main component
  component->type = iconComponentType;
  component->width = max(imageCopy->width, ((unsigned)(icon->labelWidth + 3)));
  component->height = ((imageCopy->height + 5 +
			(((kernelAsciiFont *)
			  component->params.font)->charHeight *
			  icon->labelLines)));
  
  component->minWidth = component->width;
  component->minHeight = component->height;
  component->data = (void *) icon;

  component->flags |= WINFLAG_CANFOCUS;

  // The functions
  component->draw = &draw;
  component->mouseEvent = &mouseEvent;
  component->destroy = &destroy;

  return (component);
}
