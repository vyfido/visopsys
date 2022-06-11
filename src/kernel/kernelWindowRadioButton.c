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
//  kernelWindowRadioButton.c
//

// This code is for managing kernelWindowRadioButton objects.


#include "kernelWindowManager.h"     // Our prototypes are here
#include "kernelMalloc.h"
#include "kernelError.h"
#include <string.h>
#include <sys/errors.h>

#define BUTTON_SIZE 10

static kernelAsciiFont *radioFont = NULL;


static int draw(void *componentData)
{
  // Draw the radio button component

  int status = 0;
  color foreground = { DEFAULT_BLUE, DEFAULT_GREEN, DEFAULT_RED };
  color background = { DEFAULT_GREY, DEFAULT_GREY, DEFAULT_GREY };
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindow *window = (kernelWindow *) component->window;
  kernelWindowRadioButton *radio = (kernelWindowRadioButton *) component->data;
  int xCoord = 0, yCoord = 0;
  int count;

  if (!component->parameters.useDefaultForeground)
    {
      // Use the user-supplied foreground color
      foreground.red = component->parameters.foreground.red;
      foreground.green = component->parameters.foreground.green;
      foreground.blue = component->parameters.foreground.blue;
    }
  if (!component->parameters.useDefaultBackground)
    {
      // Use the user-supplied foreground color
      background.red = component->parameters.background.red;
      background.green = component->parameters.background.green;
      background.blue = component->parameters.background.blue;
    }

  char *tmp = radio->text;
  for (count = 0; count < radio->numItems; count ++)
    {
      xCoord = (component->xCoord + (BUTTON_SIZE / 2));
      yCoord = (component->yCoord + (radio->font->charHeight * count) +
	(BUTTON_SIZE / 2));

      kernelGraphicDrawOval(&(window->buffer), &foreground, draw_normal,
			    xCoord, yCoord, BUTTON_SIZE, BUTTON_SIZE, 1, 0);

      if (radio->selectedItem == count)
	kernelGraphicDrawOval(&(window->buffer), &foreground, draw_normal,
			      xCoord, yCoord, (BUTTON_SIZE - 4),
			      (BUTTON_SIZE - 4), 1, 1);
      else
	kernelGraphicDrawOval(&(window->buffer), &background, draw_normal,
			      xCoord, yCoord, (BUTTON_SIZE - 4),
			      (BUTTON_SIZE - 4), 1, 1);

      status =
	kernelGraphicDrawText(&(window->buffer), &foreground, &background,
			      radio->font, tmp, draw_normal,
			      (component->xCoord + BUTTON_SIZE + 3),
			      (component->yCoord + (radio->font->charHeight *
						    count)));
      if (status < 0)
	break;

      tmp += (strlen(tmp) + 1);
    }

  if (component->parameters.hasBorder)
    component->drawBorder((void *) component);

  return (status);
}


static int getSelected(void *componentData)
{
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindowRadioButton *radio = (kernelWindowRadioButton *) component->data;
  return (radio->selectedItem);
}


static int setSelected(void *componentData, int selected)
{
  int status = 0;
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindow *window = (kernelWindow *) component->window;
  kernelWindowRadioButton *radio = (kernelWindowRadioButton *) component->data;

  // Check params
  if ((selected < 0) || (selected >= radio->numItems))
    {
      kernelError(kernel_error, "Illegal component number %d", selected);
      return (status = ERR_BOUNDS);
    }

  radio->selectedItem = selected;

  // Re-draw
  if (component->draw)
    component->draw(componentData);
  kernelWindowUpdateBuffer(&(window->buffer), component->xCoord,
			   component->yCoord, component->width,
			   component->height);
  return (status = 0);
}


static int mouseEvent(void *componentData, windowEvent *event)
{
  // When mouse events happen to list components, we pass them on to the
  // appropriate kernelWindowListItem component

  int status = 0;
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindow *window = (kernelWindow *) component->window;
  kernelWindowRadioButton *radio = (kernelWindowRadioButton *) component->data;
  int clickedItem = 0;
  
  if (radio->numItems && (event->type & EVENT_MOUSE_DOWN))
    {
      // Figure out which item was clicked based on the coordinates of the
      // event
      clickedItem =
	((event->yPosition - (window->yCoord + component->yCoord)) /
	 radio->font->charHeight);

      // Is this item different from the currently selected item?
      if (clickedItem != radio->selectedItem)
	{
	  radio->selectedItem = clickedItem;
	  if (component->draw)
	    {
	      status = component->draw(componentData);
	      if (status < 0)
		return (status);
	    }
	  kernelWindowUpdateBuffer(&(window->buffer), component->xCoord,
				   component->yCoord, component->width,
				   component->height);
	}
    }

  return (status = 0);
}


static int destroy(void *componentData)
{
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindowRadioButton *radio = (kernelWindowRadioButton *) component->data;

  // Release all our memory
  if (radio)
    {
      if (radio->text)
	kernelFree((void *) radio->text);
      kernelFree((void *) radio);
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


kernelWindowComponent *kernelWindowNewRadioButton(volatile void *parent,
				  kernelAsciiFont *font, unsigned rows,
				  unsigned columns, const char **items,
				  int numItems, componentParameters *params)
{
  // Formats a kernelWindowComponent as a kernelWindowRadioButton

  int status = 0;
  kernelWindowComponent *component = NULL;
  kernelWindowRadioButton *radioButton = NULL;
  unsigned textMemorySize = 0;
  char *tmp = NULL;
  int count;

  // Check parameters.  It's okay for the font to be NULL.
  if ((parent == NULL) || (items == NULL) || (params == NULL))
    return (component = NULL);

  // Get the basic component structure
  component = kernelWindowComponentNew(parent, params);
  if (component == NULL)
    return (component);

  if (radioFont == NULL)
    {
      // Try to load a nice-looking font
      status = kernelFontLoad(DEFAULT_VARIABLEFONT_SMALL_FILE,
			      DEFAULT_VARIABLEFONT_SMALL_NAME, &radioFont);
      if (status < 0)
	// Font's not there, we suppose.  There's always a default.
	kernelFontGetDefault(&radioFont);
    }

  // If font is NULL, use the default
  if (font == NULL)
    font = radioFont;

  // Now populate it
  component->type = radioButtonComponentType;

  // The functions
  component->draw = &draw;
  component->getSelected = &getSelected;
  component->setSelected = &setSelected;
  component->mouseEvent = &mouseEvent;
  component->destroy = &destroy;

  // Get the radio button
  radioButton = kernelMalloc(sizeof(kernelWindowRadioButton));
  if (radioButton == NULL)
    {
      kernelFree((void *) component);
      return (component = NULL);
    }

  radioButton->font = font;
  radioButton->selectedItem = 0;

  // If no items, nothing to do
  if (numItems == 0)
    return (component);

  // Calculate how much memory we need for our text data
  for (count = 0; count < numItems; count ++)
    textMemorySize += (strlen(items[count]) + 1);

  // Try to get memory
  radioButton->text = kernelMalloc(textMemorySize);
  if (radioButton->text == NULL)
    {
      kernelFree((void *) component);
      kernelFree((void *) radioButton);
      return (component = NULL);
    }

  // Loop through the strings (items) and add them to our text memory
  tmp = radioButton->text;
  for (count = 0; count < numItems; count ++)
    {
      strcpy(tmp, items[count]);
      tmp += (strlen(items[count]) + 1);

      if ((kernelFontGetPrintedWidth(font, items[count]) + BUTTON_SIZE + 3) >
	  component->width)
	component->width = 
	  (kernelFontGetPrintedWidth(font, items[count]) + BUTTON_SIZE + 3);

      radioButton->numItems += 1;
    }

  // The height of the radio button is the height of the font times the number
  // of items.
  component->height = (numItems * font->charHeight);
  
  component->data = (void *) radioButton;

  return (component);
}
