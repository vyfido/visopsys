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
//  kernelWindowListItem.c
//

// This code is for managing kernelWindowListItem objects.  These are
// selectable items that occur inside of kernelWindowList components.


#include "kernelWindowManager.h"     // Our prototypes are here
#include "kernelMalloc.h"
#include <string.h>
#include <sys/errors.h>

static kernelAsciiFont *labelFont = NULL;


static int draw(void *componentData)
{
  // Draw the component

  int status = 0;
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindow *window = (kernelWindow *) component->window;
  kernelWindowListItem *item = (kernelWindowListItem *) component->data;
  char *textBuffer = NULL;

  if (item->selected)
    status = kernelGraphicDrawRect(&(window->buffer), (color *)
				   &(component->parameters.foreground),
				   draw_normal, component->xCoord,
				   component->yCoord, component->width,
				   component->height, 1, 1);
  else
    status = kernelGraphicDrawRect(&(window->buffer), (color *)
				   &(component->parameters.background),
				   draw_normal, component->xCoord,
				   component->yCoord, component->width,
				   component->height, 1, 1);
  if (status < 0)
    return (status);

  textBuffer = kernelMalloc(strlen(item->text + 1));
  if (textBuffer == NULL)
    return (status = ERR_MEMORY);
  strcpy(textBuffer, item->text);
      
  // Don't draw text outside our component area
  while ((kernelFontGetPrintedWidth(component->parameters.font, textBuffer) >
	  component->width) && strlen(textBuffer))
    textBuffer[strlen(textBuffer) - 1] = '\0';

  if (item->selected)
    status =
      kernelGraphicDrawText(&(window->buffer),
			    (color *) &(component->parameters.background),
			    (color *) &(component->parameters.foreground),
			    component->parameters.font, textBuffer,
			    draw_normal, (component->xCoord + 1),
			    (component->yCoord + 1));
  else
    status =
      kernelGraphicDrawText(&(window->buffer),
			    (color *) &(component->parameters.foreground),
			    (color *) &(component->parameters.background),
			    component->parameters.font, textBuffer,
			    draw_normal, (component->xCoord + 1),
			    (component->yCoord + 1));

  kernelFree(textBuffer);

  if (component->parameters.hasBorder)
    component->drawBorder((void *) component, 1);

  return (status);
}


static int getSelected(void *componentData)
{
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindowListItem *item = (kernelWindowListItem *) component->data;

  return (item->selected);
}


static int setSelected(void *componentData, int selected)
{
  int status = 0;
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindow *window = (kernelWindow *) component->window;
  kernelWindowListItem *item = (kernelWindowListItem *) component->data;

  item->selected = selected;

  if (component->flags & WINFLAG_VISIBLE)
    {
      if (component->draw)
	{
	  status = component->draw(componentData);
	  if (status < 0)
	    return (status);
	}

      status = kernelWindowUpdateBuffer(&(window->buffer), component->xCoord,
					component->yCoord, component->width,
					component->height);
    }

  return (status);
}


static int mouseEvent(void *componentData, windowEvent *event)
{
  // When a click mouse events happen to border components, we reverse the
  // 'selected' value

  int status = 0;
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindow *window = (kernelWindow *) component->window;
  kernelWindowListItem *item = (kernelWindowListItem *) component->data;

  if (event->type == EVENT_MOUSE_LEFTDOWN)
    {
      if (item->selected)
	item->selected = 0;
      else
	item->selected = 1;

      if (component->flags & WINFLAG_VISIBLE)
	{
	  if (component->draw)
	    {
	      status = component->draw(componentData);
	      if (status < 0)
		return (status);
	    }

	  status =
	    kernelWindowUpdateBuffer(&(window->buffer), component->xCoord,
				     component->yCoord, component->width,
				     component->height);
	}
    }
  
  return (status);
}


static int destroy(void *componentData)
{
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindowListItem *label = (kernelWindowListItem *) component->data;

  // Release all our memory
  if (label)
    {
      if (label->text)
	{
	  kernelFree((void *) label->text);
	  label->text = NULL;
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


kernelWindowComponent *kernelWindowNewListItem(volatile void *parent,
					       const char *text,
					       componentParameters *params)
{
  // Formats a kernelWindowComponent as a kernelWindowListItem

  int status = 0;
  kernelWindowComponent *component = NULL;
  kernelWindowListItem *listItemComponent = NULL;

  // Check parameters.
  if ((parent == NULL) || (text == NULL) || (params == NULL))
    return (component = NULL);

  // Get the basic component structure
  component = kernelWindowComponentNew(parent, params);
  if (component == NULL)
    return (component);

  // If default colors were requested, override the standard background color
  // with the one we prefer (white)
  if (component->parameters.useDefaultBackground)
    {
      component->parameters.background.blue = 0xFF;
      component->parameters.background.green = 0xFF;
      component->parameters.background.red = 0xFF;
    }

  if (labelFont == NULL)
    {
      // Try to load a nice-looking font
      status = kernelFontLoad(DEFAULT_VARIABLEFONT_MEDIUM_FILE,
			      DEFAULT_VARIABLEFONT_MEDIUM_NAME, &labelFont, 0);
      if (status < 0)
	// Font's not there, we suppose.  There's always a default.
	kernelFontGetDefault(&labelFont);
    }

  // If font is NULL, use the default
  if (component->parameters.font == NULL)
    component->parameters.font = labelFont;

  // Now populate it
  component->type = listItemComponentType;
  component->width =
    (kernelFontGetPrintedWidth(component->parameters.font, text) + 2);
  component->height =
    (((kernelAsciiFont *) component->parameters.font)->charHeight + 2);

  // The label data
  listItemComponent = kernelMalloc(sizeof(kernelWindowListItem));
  if (listItemComponent == NULL)
    {
      kernelFree((void *) component);
      return (component = NULL);
    }

  listItemComponent->text = kernelMalloc(strlen(text) + 1);
  if (listItemComponent->text == NULL)
    {
      kernelFree((void *) listItemComponent);
      kernelFree((void *) component);
      return (component = NULL);
    }
  strcpy((char *) listItemComponent->text, text);

  listItemComponent->selected = 0;

  // The functions
  component->draw = &draw;
  component->getSelected = &getSelected;
  component->setSelected = &setSelected;
  component->mouseEvent = &mouseEvent;
  component->destroy = &destroy;

  component->data = (void *) listItemComponent;

  return (component);
}
