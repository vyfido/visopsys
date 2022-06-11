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
//  kernelWindowListItem.c
//

// This code is for managing kernelWindowListItem objects.  These are
// selectable items that occur inside of kernelWindowList components.

#include "kernelWindow.h"     // Our prototypes are here
#include "kernelMalloc.h"
#include "kernelMiscFunctions.h"
#include "kernelError.h"
#include <string.h>

static kernelAsciiFont *labelFont = NULL;


static int draw(void *componentData)
{
  // Draw the component

  int status = 0;
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindow *window = (kernelWindow *) component->window;
  kernelWindowListItem *item = (kernelWindowListItem *) component->data;
  char *textBuffer = NULL;

  if (!item->selected)
    kernelGraphicDrawRect(&(window->buffer), (color *)
			  &(component->parameters.background), draw_normal,
			  component->xCoord, component->yCoord,
			  component->width, component->height, 1, 1);

  if (item->type == windowlist_textonly)
    {
      textBuffer = kernelMalloc(strlen((char *) item->params.text) + 1);
      if (textBuffer == NULL)
	return (status = ERR_MEMORY);
      strcpy(textBuffer, (char *) item->params.text);
      
      // Don't draw text outside our component area
      while (((int) kernelFontGetPrintedWidth(component->parameters.font,
					      textBuffer) > component->width)
	     && strlen(textBuffer))
	textBuffer[strlen(textBuffer) - 1] = '\0';

      if (item->selected)
	{
	  kernelGraphicDrawRect(&(window->buffer), (color *)
				&(component->parameters.foreground),
				draw_normal, component->xCoord,
				component->yCoord, component->width,
				component->height, 1, 1);

	  kernelGraphicDrawText(&(window->buffer),
				(color *) &(component->parameters.background),
				(color *) &(component->parameters.foreground),
				component->parameters.font, textBuffer,
				draw_normal, (component->xCoord + 1),
				(component->yCoord + 1));
	}
      else
	kernelGraphicDrawText(&(window->buffer),
			      (color *) &(component->parameters.foreground),
			      (color *) &(component->parameters.background),
			      component->parameters.font, textBuffer,
			      draw_normal, (component->xCoord + 1),
			      (component->yCoord + 1));
      
      kernelFree(textBuffer);
    }

  else if (item->type == windowlist_icononly)
    {
      if (item->selected)
	kernelGraphicDrawRect(&(window->buffer), (color *)
			      &(component->parameters.foreground),
			      draw_normal, (item->icon->xCoord - 1),
			      (item->icon->yCoord - 1),
			      (item->icon->width + 2),
			      (item->icon->height + 2), 1, 1);

      if (item->icon && item->icon->draw)
	item->icon->draw((void *) item->icon);
    }

  if (component->parameters.hasBorder)
    component->drawBorder((void *) component, 1);

  return (status);
}


static int move(void *componentData, int xCoord, int yCoord)
{
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindowListItem *item = (kernelWindowListItem *) component->data;
  int iconXCoord = 0;

  if (item->icon)
    {
      iconXCoord = (xCoord + ((component->width - item->icon->width) / 2));
      if (iconXCoord == xCoord)
	iconXCoord += 1;

      if (item->icon->move)
	item->icon->move((void *) item->icon, iconXCoord, (yCoord + 1));
      item->icon->xCoord = iconXCoord;
      item->icon->yCoord = (yCoord + 1);
    }

  return (0);
}


static int getSelected(void *componentData, int *selection)
{
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  *selection = ((kernelWindowListItem *) component->data)->selected;
  return (0);
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
  // When mouse down events happen to list item components, we reverse the
  // 'selected' value

  int status = 0;
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindow *window = (kernelWindow *) component->window;
  kernelWindowListItem *item = (kernelWindowListItem *) component->data;

  if (event->type & EVENT_MOUSE_LEFTDOWN)
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
  kernelWindowListItem *listItem = (kernelWindowListItem *) component->data;

  // Release all our memory
  if (listItem)
    {
      if (listItem->icon)
	kernelWindowComponentDestroy(listItem->icon);

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
					       windowListType type,
					       listItemParameters *item,
					       componentParameters *params)
{
  // Formats a kernelWindowComponent as a kernelWindowListItem

  int status = 0;
  kernelWindowComponent *component = NULL;
  kernelWindowListItem *listItemComponent = NULL;

  // Check parameters.
  if ((parent == NULL) || (item == NULL) || (params == NULL))
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

  // The list item data
  listItemComponent = kernelMalloc(sizeof(kernelWindowListItem));
  if (listItemComponent == NULL)
    {
      kernelFree((void *) component);
      return (component = NULL);
    }

  listItemComponent->type = type;
  kernelMemCopy(item, (listItemParameters *) &(listItemComponent->params),
		sizeof(listItemParameters));
  listItemComponent->selected = 0;

  component->type = listItemComponentType;
  if (listItemComponent->type == windowlist_textonly)
    {
      component->width =
	(kernelFontGetPrintedWidth(component->parameters.font,
				   (char *) listItemComponent->params.text) +
	 2);
      component->height =
	(((kernelAsciiFont *) component->parameters.font)->charHeight + 2);
    }

  else if (listItemComponent->type == windowlist_icononly)
    {
      listItemComponent->icon =
	kernelWindowNewIcon(parent, (image *)
			    &(listItemComponent->params.iconImage),
			    (char *) listItemComponent->params.text, params);
      if (listItemComponent->icon == NULL)
	{
	  kernelFree((void *) listItemComponent);
	  kernelFree((void *) component);
	  return (component = NULL);
	}

      // Remove the icon from the parent container
      if (((kernelWindow *) parent)->type == windowType)
	{
	  kernelWindow *tmpWindow = getWindow(parent);
	  kernelWindowContainer *tmpContainer =
	    (kernelWindowContainer *) tmpWindow->mainContainer->data;
	  tmpContainer->containerRemove(tmpWindow->mainContainer,
					listItemComponent->icon);
	}
      else
	{
	  kernelWindowContainer *tmpContainer = (kernelWindowContainer *)
	    ((kernelWindowComponent *) parent)->data;
	  tmpContainer->containerRemove((kernelWindowComponent *) parent,
					listItemComponent->icon);
	}

      component->width = (listItemComponent->icon->width + 2);
      component->height = (listItemComponent->icon->height + 2);
    }

  component->minWidth = component->width;
  component->minHeight = component->height;

  // The functions
  component->draw = &draw;
  component->move = &move;
  component->getSelected = &getSelected;
  component->setSelected = &setSelected;
  component->mouseEvent = &mouseEvent;
  component->destroy = &destroy;

  component->data = (void *) listItemComponent;

  return (component);
}
