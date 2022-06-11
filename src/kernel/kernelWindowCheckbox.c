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
//  kernelWindowCheckbox.c
//

// This code is for managing kernelWindowCheckbox objects.


#include "kernelWindowManager.h"     // Our prototypes are here
#include "kernelMalloc.h"
#include "kernelError.h"
#include <string.h>
#include <stdlib.h>
#include <sys/errors.h>

#define CHECKBOX_SIZE 10

static kernelAsciiFont *checkboxFont = NULL;


static int draw(void *componentData)
{
  // Draw the checkbox component

  int status = 0;
  color foreground = { DEFAULT_BLUE, DEFAULT_GREEN, DEFAULT_RED };
  color background = { DEFAULT_GREY, DEFAULT_GREY, DEFAULT_GREY };
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindow *window = (kernelWindow *) component->window;
  kernelWindowCheckbox *checkbox = (kernelWindowCheckbox *) component->data;
  int yCoord = 0;

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

  yCoord = (component->yCoord + ((component->height - CHECKBOX_SIZE) / 2));

  kernelGraphicDrawRect(&(window->buffer), &((color){255, 255, 255}),
			draw_normal, (component->xCoord + 2), yCoord,
			CHECKBOX_SIZE, CHECKBOX_SIZE, 1, 1);
  kernelGraphicDrawRect(&(window->buffer), &foreground, draw_normal,
			(component->xCoord + 2), yCoord, CHECKBOX_SIZE,
			CHECKBOX_SIZE, 1, 0);

  if (checkbox->selected)
    {
      kernelGraphicDrawLine(&(window->buffer), &foreground, draw_normal,
			    (component->xCoord + 2), yCoord,
			    (component->xCoord + 2 + (CHECKBOX_SIZE - 1)),
			    (yCoord + (CHECKBOX_SIZE - 1)));
      kernelGraphicDrawLine(&(window->buffer), &foreground, draw_normal,
			    (component->xCoord + 2),
			    (yCoord + (CHECKBOX_SIZE - 1)),
			    (component->xCoord + 2 + (CHECKBOX_SIZE - 1)),
			    yCoord);
    }

  kernelGraphicDrawText(&(window->buffer), &foreground, &background,
			checkbox->font, checkbox->text, draw_normal,
			(component->xCoord + CHECKBOX_SIZE + 5),
			(component->yCoord + 2));

  if (component->parameters.hasBorder)
    component->drawBorder((void *) component);
  
  return (status = 0);
}


static int focus(void *componentData, int focus)
{
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindow *window = component->window;
  kernelGraphicBuffer *buffer = &(window->buffer);

  if (focus)
    kernelGraphicDrawRect(buffer,
			  (color *) &(component->parameters.foreground),
			  draw_normal, component->xCoord, component->yCoord,
			  component->width, component->height, 1, 0);
  else
    kernelGraphicDrawRect(buffer, (color *) &(window->background), draw_normal,
			  component->xCoord, component->yCoord,
			  component->width, component->height, 1, 0);

  kernelWindowUpdateBuffer(buffer, component->xCoord, component->yCoord,
			   component->width, component->height);
  
  return (0);
}


static int getSelected(void *componentData)
{
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindowCheckbox *checkbox = (kernelWindowCheckbox *) component->data;
  return (checkbox->selected);
}


static int setSelected(void *componentData, int selected)
{
  int status = 0;
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindow *window = (kernelWindow *) component->window;
  kernelWindowCheckbox *checkbox = (kernelWindowCheckbox *) component->data;

  checkbox->selected = selected;

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
  
  if (event->type & EVENT_MOUSE_DOWN)
    {
      if (((kernelWindowCheckbox *) component->data)->selected)
	setSelected(componentData, 0);
      else
	setSelected(componentData, 1);
    }

  return (status = 0);
}


static int keyEvent(void *componentData, windowEvent *event)
{
  // We allow the user to control the list widget with key presses, such
  // as cursor movements.

  int status = 0;
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;

  if ((event->type & EVENT_KEY_DOWN) && (event->key == 32))
    {
      if (((kernelWindowCheckbox *) component->data)->selected)
	setSelected(componentData, 0);
      else
	setSelected(componentData, 1);
    }

  return (status);
}


static int destroy(void *componentData)
{
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindowCheckbox *checkbox = (kernelWindowCheckbox *) component->data;

  // Release all our memory
  if (checkbox)
    {
      if (checkbox->text)
	kernelFree((void *) checkbox->text);
      kernelFree((void *) checkbox);
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


kernelWindowComponent *kernelWindowNewCheckbox(volatile void *parent,
					       kernelAsciiFont *font,
					       const char *text,
					       componentParameters *params)
{
  // Formats a kernelWindowComponent as a kernelWindowCheckbox

  int status = 0;
  kernelWindowComponent *component = NULL;
  kernelWindowCheckbox *checkbox = NULL;

  // Check parameters.  It's okay for the font to be NULL.
  if ((parent == NULL) || (text == NULL) || (params == NULL))
    return (component = NULL);

  // Get the basic component structure
  component = kernelWindowComponentNew(parent, params);
  if (component == NULL)
    return (component);

  if (checkboxFont == NULL)
    {
      // Try to load a nice-looking font
      status = kernelFontLoad(DEFAULT_VARIABLEFONT_SMALL_FILE,
			      DEFAULT_VARIABLEFONT_SMALL_NAME, &checkboxFont);
      if (status < 0)
	// Font's not there, we suppose.  There's always a default.
	kernelFontGetDefault(&checkboxFont);
    }

  // If font is NULL, use the default
  if (font == NULL)
    font = checkboxFont;

  // Now populate it
  component->type = checkboxComponentType;
  component->flags |= WINFLAG_CANFOCUS;

  // The functions
  component->draw = &draw;
  component->focus = &focus;
  component->getSelected = &getSelected;
  component->setSelected = &setSelected;
  component->mouseEvent = &mouseEvent;
  component->keyEvent = &keyEvent;
  component->destroy = &destroy;

  // Get the checkbox
  checkbox = kernelMalloc(sizeof(kernelWindowCheckbox));
  if (checkbox == NULL)
    {
      kernelFree((void *) component);
      return (component = NULL);
    }

  checkbox->font = font;

  // Try to get memory for the text
  checkbox->text = kernelMalloc(strlen(text) + 1);
  if (checkbox->text == NULL)
    {
      kernelFree((void *) component);
      kernelFree((void *) checkbox);
      return (component = NULL);
    }
  strcpy(checkbox->text, text);

  // The width of the checkbox is the width of the checkbox, plus a bit
  // of padding, plus the printed width of the text
  component->width =
    (CHECKBOX_SIZE + 7 +
     kernelFontGetPrintedWidth(checkbox->font, checkbox->text));

  // The height of the checkbox is the height of the font, or the height
  // of the checkbox, whichever is greater
  component->height = (max(font->charHeight, CHECKBOX_SIZE) + 4);
  
  component->data = (void *) checkbox;

  return (component);
}
