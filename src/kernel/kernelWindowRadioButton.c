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


#include "kernelWindow.h"     // Our prototypes are here
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
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindow *window = (kernelWindow *) component->window;
  kernelWindowRadioButton *radio = (kernelWindowRadioButton *) component->data;
  int xCoord = 0, yCoord = 0;
  int count;

  char *tmp = radio->text;
  for (count = 0; count < radio->numItems; count ++)
    {
      xCoord = (component->xCoord + (BUTTON_SIZE / 2));
      yCoord = (component->yCoord +
		(((kernelAsciiFont *) component->parameters.font)
		->charHeight * count) + (BUTTON_SIZE / 2));

      kernelGraphicDrawOval(&(window->buffer),
			    (color *) &(component->parameters.foreground),
			    draw_normal, xCoord, yCoord, BUTTON_SIZE,
			    BUTTON_SIZE, 1, 0);

      if (radio->selectedItem == count)
	kernelGraphicDrawOval(&(window->buffer),
			      (color *) &(component->parameters.foreground),
			      draw_normal, xCoord, yCoord, (BUTTON_SIZE - 4),
			      (BUTTON_SIZE - 4), 1, 1);
      else
	kernelGraphicDrawOval(&(window->buffer),
			      (color *) &(component->parameters.background),
			      draw_normal, xCoord, yCoord, (BUTTON_SIZE - 4),
			      (BUTTON_SIZE - 4), 1, 1);

      status =
	kernelGraphicDrawText(&(window->buffer),
			      (color *) &(component->parameters.foreground),
			      (color *) &(component->parameters.background),
			      component->parameters.font, tmp, draw_normal,
			      (component->xCoord + BUTTON_SIZE + 2),
			      (component->yCoord + (((kernelAsciiFont *)
				component->parameters.font)->charHeight *
				count)));
      if (status < 0)
	break;

      tmp += (strlen(tmp) + 1);
    }

  if (component->parameters.hasBorder)
    component->drawBorder((void *) component, 1);

  return (status);
}


static int focus(void *componentData, int focus)
{
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindow *window = component->window;

  component->drawBorder((void *) component, focus);
  kernelWindowUpdateBuffer(&(window->buffer), (component->xCoord - 2),
			   (component->yCoord - 2), (component->width + 4),
			   (component->height + 4));
  return (0);
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
  
  if (radio->numItems && (event->type == EVENT_MOUSE_LEFTDOWN))
    {
      // Figure out which item was clicked based on the coordinates of the
      // event
      clickedItem =
	((event->yPosition - (window->yCoord + component->yCoord)) /
	 ((kernelAsciiFont *) component->parameters.font)->charHeight);

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


static int keyEvent(void *componentData, windowEvent *event)
{
  // We allow the user to control the list widget with key presses, such
  // as cursor movements.  The radio button accepts cursor up and cursor
  // down movements.

  int status = 0;
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindow *window = (kernelWindow *) component->window;
  kernelWindowRadioButton *radio = (kernelWindowRadioButton *) component->data;

  if ((event->type == EVENT_KEY_DOWN) &&
      ((event->key == 17) || (event->key == 20)))
    {
      if (event->key == 17)
	{
	  // UP cursor
	  if (radio->selectedItem > 0)
	    radio->selectedItem -= 1;
	}
      else
	{
	  // DOWN cursor
	  if (radio->selectedItem < (radio->numItems - 1))
	    radio->selectedItem += 1;
	}

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

  return (status);
}


static int destroy(void *componentData)
{
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindowRadioButton *radio = (kernelWindowRadioButton *) component->data;

  // Release all our memory
  if (radio)
    {
      if (radio->text)
	{
	  kernelFree((void *) radio->text);
	  radio->text = NULL;
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


kernelWindowComponent *kernelWindowNewRadioButton(volatile void *parent,
				  int rows, int columns, const char **items,
				  int numItems, componentParameters *params)
{
  // Formats a kernelWindowComponent as a kernelWindowRadioButton

  int status = 0;
  kernelWindowComponent *component = NULL;
  kernelWindowRadioButton *radioButton = NULL;
  int textMemorySize = 0;
  char *tmp = NULL;
  int count;

  // Check parameters.
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
			      DEFAULT_VARIABLEFONT_SMALL_NAME, &radioFont, 0);
      if (status < 0)
	// Font's not there, we suppose.  There's always a default.
	kernelFontGetDefault(&radioFont);
    }

  // If font is NULL, use the default
  if (component->parameters.font == NULL)
    component->parameters.font = radioFont;

  // Now populate it
  component->type = radioButtonComponentType;
  component->flags |= WINFLAG_CANFOCUS;

  // The functions
  component->draw = &draw;
  component->focus = &focus;
  component->getSelected = &getSelected;
  component->setSelected = &setSelected;
  component->mouseEvent = &mouseEvent;
  component->keyEvent = &keyEvent;
  component->destroy = &destroy;

  // Get the radio button
  radioButton = kernelMalloc(sizeof(kernelWindowRadioButton));
  if (radioButton == NULL)
    {
      kernelFree((void *) component);
      return (component = NULL);
    }

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

      if ((kernelFontGetPrintedWidth(component->parameters.font,
				     items[count]) + BUTTON_SIZE + 3) >
	  component->width)
	component->width = 
	  (kernelFontGetPrintedWidth(component->parameters.font,
				     items[count]) + BUTTON_SIZE + 3);

      radioButton->numItems += 1;
    }

  // The height of the radio button is the height of the font times the number
  // of items.
  component->height =
    (numItems * ((kernelAsciiFont *) component->parameters.font)->charHeight);
  
  component->data = (void *) radioButton;

  return (component);
}
