//
//  Visopsys
//  Copyright (C) 1998-2013 J. Andrew McLaughlin
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
//  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
//
//  kernelWindowListItem.c
//

// This code is for managing kernelWindowListItem objects.  These are
// selectable items that occur inside of kernelWindowList components.

#include "kernelWindow.h"     // Our prototypes are here
#include "kernelDebug.h"
#include "kernelError.h"
#include "kernelFont.h"
#include "kernelGraphic.h"
#include "kernelMalloc.h"
#include "kernelMisc.h"
#include <string.h>

extern kernelWindowVariables *windowVariables;


static int numComps(kernelWindowComponent *component)
{
  kernelWindowListItem *item = component->data;

  if (item->icon)
    return (1);
  else
    return (0);
}


static int flatten(kernelWindowComponent *component,
		   kernelWindowComponent **array, int *numItems,
		   unsigned flags)
{
  kernelWindowListItem *item = component->data;

  if (item->icon && ((item->icon->flags & flags) == flags))
    {
      // Add our icon
      array[*numItems] = item->icon;
      *numItems += 1;
    }

  return (0);
}


static int setBuffer(kernelWindowComponent *component,
		     kernelGraphicBuffer *buffer)
{
  // Set the graphics buffer for the component's subcomponents.

  int status = 0;
  kernelWindowListItem *item = component->data;

  if (item->icon && item->icon->setBuffer)
    {
      // Do our icon
      status = item->icon->setBuffer(item->icon, buffer);
      item->icon->buffer = buffer;
    }

  return (status);
}


static int draw(kernelWindowComponent *component)
{
  // Draw the component

  int status = 0;
  kernelWindowListItem *item = component->data;
  char *textBuffer = NULL;

  if (!item->selected)
    kernelGraphicDrawRect(component->buffer, (color *)
			  &component->params.background, draw_normal,
			  component->xCoord, component->yCoord,
			  component->width, component->height, 1, 1);

  if (item->type == windowlist_textonly)
    {
      textBuffer = kernelMalloc(strlen((char *) item->params.text) + 1);
      if (textBuffer == NULL)
	return (status = ERR_MEMORY);
      strncpy(textBuffer, (char *) item->params.text,
	      strlen((char *) item->params.text));
      
      if (component->params.font)
	// Don't draw text outside our component area
	while (((int) kernelFontGetPrintedWidth(
		(asciiFont *) component->params.font, textBuffer) >
		(component->width - 2)) && strlen(textBuffer))
	  textBuffer[strlen(textBuffer) - 1] = '\0';

      if (item->selected)
	{
	  kernelGraphicDrawRect(component->buffer,
				(color *) &component->params.foreground,
				draw_normal, component->xCoord,
				component->yCoord, component->width,
				component->height, 1, 1);

	  if (component->params.font)
	    kernelGraphicDrawText(component->buffer,
				  (color *) &component->params.background,
				  (color *) &component->params.foreground,
				  (asciiFont *) component->params.font,
				  textBuffer, draw_normal,
				  (component->xCoord + 1),
				  (component->yCoord + 1));
	}
      else
	{
	  if (component->params.font)
	    kernelGraphicDrawText(component->buffer,
				  (color *) &component->params.foreground,
				  (color *) &component->params.background,
				  (asciiFont *) component->params.font,
				  textBuffer, draw_normal,
				  (component->xCoord + 1),
				  (component->yCoord + 1));
	}

      kernelFree(textBuffer);
    }

  else if (item->type == windowlist_icononly)
    {
      if (item->selected)
	kernelGraphicDrawRect(component->buffer, (color *)
			      &component->params.foreground,
			      draw_normal, (item->icon->xCoord - 1),
			      (item->icon->yCoord - 1),
			      (item->icon->width + 2),
			      (item->icon->height + 2), 1, 1);

      if (item->icon && item->icon->draw)
	item->icon->draw(item->icon);
    }

  if ((component->params.flags & WINDOW_COMPFLAG_HASBORDER) &&
      component->drawBorder)
    component->drawBorder(component, 1);

  return (status);
}


static int move(kernelWindowComponent *component, int xCoord, int yCoord)
{
  kernelWindowListItem *item = component->data;
  int iconXCoord = 0;

  kernelDebug(debug_gui, "Move window list item to (%d, %d)", xCoord, yCoord);

  if (item->icon)
    {
      iconXCoord = (xCoord + ((component->width - item->icon->width) / 2));
      if (iconXCoord == xCoord)
	iconXCoord += 1;

      if (item->icon->move)
	item->icon->move(item->icon, iconXCoord, (yCoord + 1));

      item->icon->xCoord = iconXCoord;
      item->icon->yCoord = (yCoord + 1);
    }

  return (0);
}


static int getSelected(kernelWindowComponent *component, int *selection)
{
  kernelWindowListItem *item = component->data;
  *selection = item->selected;
  return (0);
}


static int setSelected(kernelWindowComponent *component, int selected)
{
  kernelWindowListItem *item = component->data;

  item->selected = selected;

  kernelDebug(debug_gui, "listItem \"%s\" %sselected", item->params.text,
	      (selected? "" : "de"));

  if (item->icon)
    ((kernelWindowIcon *) item->icon->data)->selected = selected;

  if (component->flags & WINFLAG_VISIBLE)
    {
      if (component->draw)
	component->draw(component);

      // List items are also menu items, and menu items have their own buffers,
      // so only render the buffer here if we're using the normal window buffer
      if (component->buffer == &component->window->buffer)
	{
	  component->window
	    ->update(component->window, component->xCoord, component->yCoord,
		     component->width, component->height);
	  //kernelDebug(debug_gui, "listItem \"%s\" window->update()",
	  //	      item->params.text);
	}
    }

  return (0);
}


static int mouseEvent(kernelWindowComponent *component, windowEvent *event)
{
  // When mouse down events happen to list item components, we reverse the
  // 'selected' value

  kernelDebug(debug_gui, "listItem \"%s\" mouse event",
	      ((kernelWindowListItem *) component->data)->params.text);

  if (event->type & EVENT_MOUSE_DOWN)
    setSelected(component, 1);
 
  return (0);
}


static int destroy(kernelWindowComponent *component)
{
  kernelWindowListItem *listItem = component->data;

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


kernelWindowComponent *kernelWindowNewListItem(objectKey parent,
					       windowListType type,
					       listItemParameters *item,
					       componentParameters *params)
{
  // Formats a kernelWindowComponent as a kernelWindowListItem

  kernelWindowComponent *component = NULL;
  kernelWindowListItem *listItem = NULL;

  // Check parameters.
  if ((parent == NULL) || (item == NULL) || (params == NULL))
    return (component = NULL);

  // Get the basic component structure
  component = kernelWindowComponentNew(parent, params);
  if (component == NULL)
    return (component);

  // If default colors were requested, override the standard background color
  // with the one we prefer (white)
  if (!(component->params.flags & WINDOW_COMPFLAG_CUSTOMBACKGROUND))
    kernelMemCopy(&COLOR_WHITE, (color *) &component->params.background,
		  sizeof(color));

  // If font is NULL, use the default
  if (component->params.font == NULL)
    component->params.font = windowVariables->font.varWidth.medium.font;

  // The list item data
  listItem = kernelMalloc(sizeof(kernelWindowListItem));
  if (listItem == NULL)
    {
      kernelFree((void *) component);
      return (component = NULL);
    }

  listItem->type = type;
  kernelMemCopy(item, (listItemParameters *) &listItem->params,
		sizeof(listItemParameters));

  component->type = listItemComponentType;
  if (listItem->type == windowlist_textonly)
    {
      component->width = 2;
      if (component->params.font)
	component->width += 
	  kernelFontGetPrintedWidth((asciiFont *) component->params.font,
				    (char *) listItem->params.text);

      component->height = 2;
      if (component->params.font)
	component->height +=
	  ((asciiFont *) component->params.font)->charHeight;
    }

  else if (listItem->type == windowlist_icononly)
    {
      listItem->icon =
	kernelWindowNewIcon(parent, (image *) &listItem->params.iconImage,
			    (char *) listItem->params.text,
			    (componentParameters *) &component->params);
      if (listItem->icon == NULL)
	{
	  kernelFree((void *) listItem);
	  kernelFree((void *) component);
	  return (component = NULL);
	}

      // Remove the icon from the parent container
      removeFromContainer(listItem->icon);

      component->width = (listItem->icon->width + 2);
      component->height = (listItem->icon->height + 2);
    }

  component->minWidth = component->width;
  component->minHeight = component->height;

  // The functions
  component->numComps = &numComps;
  component->flatten = &flatten;
  component->setBuffer = &setBuffer;
  component->draw = &draw;
  component->move = &move;
  component->getSelected = &getSelected;
  component->setSelected = &setSelected;
  component->mouseEvent = &mouseEvent;
  component->destroy = &destroy;

  component->data = (void *) listItem;

  return (component);
}
