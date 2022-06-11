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
//  kernelWindowList.c
//

// This code is for managing kernelWindowList objects.  These are containers
// for kernelWindowListItem components.

#include "kernelWindow.h"     // Our prototypes are here
#include "kernelMalloc.h"
#include "kernelMiscFunctions.h"
#include "kernelError.h"
#include <string.h>

static kernelAsciiFont *labelFont = NULL;


static inline int isMouseInScrollBar(windowEvent *event,
				     kernelWindowComponent *scrollBar)
{
  // We use this to determine whether a mouse event is inside the slider

  kernelWindow *window = (kernelWindow *) scrollBar->window;

  if (event->xPosition >= (window->xCoord + scrollBar->xCoord))
    return (1);
  else
    return (0);
}


static void setScrollBar(kernelWindowList *list)
{
  // Set the scroll bar display and position percentages

  kernelWindowContainer *container = (kernelWindowContainer *)
    list->container->data;
  scrollBarState state;

  if (container->numComponents > list->rows)
    {
      state.positionPercent = ((list->firstVisible * 100) /
			       (container->numComponents - list->rows));
      state.displayPercent = ((list->rows * 100) / container->numComponents);
    }
  else
    {
      state.positionPercent = 0;
      state.displayPercent = 100;
    }

  if (list->scrollBar->setData)
    list->scrollBar->setData((void *) list->scrollBar, &state,
			     sizeof(scrollBarState));
}


static void checkScroll(kernelWindowComponent *component)
{
  kernelWindowList *list = (kernelWindowList *) component->data;

  // Do we have to scroll the list?
  if ((list->selectedItem < list->firstVisible) ||
      (list->selectedItem >= (list->firstVisible + list->rows)))
    {
      if (list->selectedItem < list->firstVisible)
	list->firstVisible = list->selectedItem;
      else if (list->selectedItem >= (list->firstVisible + list->rows))
	list->firstVisible = ((list->selectedItem - list->rows) + 1);

      if (list->scrollBar)
	// Set the scroll bar display percent
	setScrollBar(list);

      component->draw((void *) component);
      kernelWindowUpdateBuffer(&((kernelWindow *) component->window)->buffer,
			       component->xCoord, component->yCoord,
			       component->width, component->height);
    }
}


static void populateList(kernelWindowComponent *listComponent,
			 const char *items[], int numItems)
{
  // Sets up all the kernelWindowList subcomponents of the list
  
  kernelWindowList *list = (kernelWindowList *) listComponent->data;
  kernelWindowContainer *container =
    (kernelWindowContainer *) list->container->data;
  kernelWindowComponent *listItemComponent = NULL;
  int itemWidth = 0;
  int setSize = 0;
  int count;

  // If the list already has components, get rid of them
  while (container->numComponents)
    kernelWindowComponentDestroy(container
			 ->components[container->numComponents - 1]);

  if (!listComponent->width || !listComponent->height)
    setSize = 1;
  else
    itemWidth = list->itemWidth;

  // If no items, nothing to do
  if (numItems == 0)
    return;

  // If the selected item is greater than the new number we have, make it the
  // last one
  if (list->selectedItem >= numItems)
    list->selectedItem = (numItems - 1);

  // Loop through the strings, creating kernelWindowListItem components and
  // adding them to this component
  for (count = 0; ((count < numItems) && (count < WINDOW_MAX_LISTITEMS));
       count ++)
    {
      listItemComponent =
	kernelWindowNewListItem(list->container, items[count],
				(componentParameters *)
				&(listComponent->parameters));
      if (listItemComponent == NULL)
	continue;

      if (count == list->selectedItem)
	((kernelWindowListItem *) listItemComponent->data)->selected = 1;

      // Keep a record of the widest list item width
      if (listItemComponent->width > itemWidth)
	itemWidth = listItemComponent->width;
    }

  if (setSize)
    {
      // If we are setting the size, give it the width of the widest list item.
      list->itemWidth = itemWidth;
      listComponent->width = itemWidth;

      // The height of the list component is the height of the first item times
      // the number of rows.
      if (container->numComponents)
	listComponent->height =
	  (list->rows * container->components[0]->height);

      if (list->scrollBar)
	{
	  // Set up the scroll bar size and location and adjust the list
	  // component size to account for it
	  list->scrollBar->xCoord = (listComponent->xCoord + list->itemWidth);
	  list->scrollBar->height = listComponent->height;
	  listComponent->width += list->scrollBar->width;
	}
    }

  // Set the widths of all list item subcomponents
  for (count = 0; count < container->numComponents; count ++)
    container->components[count]->width = list->itemWidth;

  // Update the scroll bar position percent
  setScrollBar(list);

  return;
}


static int draw(void *componentData)
{
  // Draw the component, which is really just a collection of other components.

  int status = 0;
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindowList *list = (kernelWindowList *) component->data;
  kernelWindowContainer *container =
    (kernelWindowContainer *) list->container->data;
  kernelGraphicBuffer *buffer = (kernelGraphicBuffer *)
    &(((kernelWindow *) component->window)->buffer);
  int count;

  // Draw the background of the list
  kernelGraphicDrawRect(buffer, (color *) &(component->parameters.background),
			draw_normal, component->xCoord,
			component->yCoord, component->width, component->height,
			1, 1);

  // Loop through the visible subcomponents, calling their draw() functions
  for (count = 0; count < container->numComponents; count ++)
    {
      if ((count >= list->firstVisible) &&
	  (count < (list->rows + list->firstVisible)))
	{
	  // Make sure the subcomponent has the same parameters as the main one
	  kernelMemCopy((void *) &(component->parameters),
			(void *) &(container->components[count]->parameters),
			sizeof(componentParameters));

	  container->components[count]->xCoord = component->xCoord;
	  container->components[count]->yCoord =
	    (component->yCoord + ((count - list->firstVisible) *
				  container->components[count]->height));
	  
	  container->components[count]->flags |= WINFLAG_VISIBLE;

	  if (container->components[count]->draw)
	    {
	      status = container->components[count]
		->draw((void *) container->components[count]);
	      if (status < 0)
		return (status);
	    }
	}
      else
	container->components[count]->flags &= ~WINFLAG_VISIBLE;
    }

  // Draw the scroll bar
  if (list->scrollBar && list->scrollBar->draw)
    list->scrollBar->draw((void *) list->scrollBar);

  if (component->parameters.hasBorder || (component->flags & WINFLAG_HASFOCUS))
    component->drawBorder((void *) component, 1);

  return (status);
}


static int focus(void *componentData, int focus)
{
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindow *window = component->window;

  component->drawBorder((void *) component, focus);
  kernelWindowUpdateBuffer(&(window->buffer), (component->xCoord - 2),
			   (component->yCoord - 2),
			   (component->width + 4), (component->height + 4));
  return (0);
}


static int getSelected(void *componentData)
{
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindowList *item = (kernelWindowList *) component->data;
  return (item->selectedItem);
}


static int setSelected(void *componentData, int selected)
{
  int status = 0;
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindowList *list = (kernelWindowList *) component->data;
  kernelWindowContainer *container =
    (kernelWindowContainer *) list->container->data;

  // Check params
  if ((selected < 0) || (selected >= container->numComponents))
    {
      kernelError(kernel_error, "Illegal component number %d", selected);
      return (status = ERR_BOUNDS);
    }

  container->components[selected]
    ->setSelected((void *) container->components[selected], selected);

  if (selected)
    {
      // If some other item has been clicked, deselect the old one
      if (!(list->selectMultiple) &&
	  (list->selectedItem != -1) && (selected != list->selectedItem))
	container->components[list->selectedItem]
	  ->setSelected((void *) container->components[list->selectedItem], 0);
      list->selectedItem = selected;
    }
  else
    {
       if (!(list->selectMultiple))
	 {
	   // Can't deselect an item when 'select multiple' is not in effect.
	   // Have to select something else insead.
	   container->components[selected]
	     ->setSelected((void *) container->components[selected], 1);
	   list->selectedItem = selected;
	 }
       else
	 list->selectedItem = -1;
    }

  // See if we have to scroll
  checkScroll(component);

  return (status = 0);
}


static int setData(void *componentData, void *buffer, int size)
{
  // Resets the subcomponents 

  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindow *window = (kernelWindow *) component->window;

  // Re-populate the list
  populateList(component, buffer, size);

  // Re-draw
  if (component->draw)
    component->draw(componentData);
  kernelWindowUpdateBuffer(&(window->buffer), component->xCoord,
			   component->yCoord, component->width,
			   component->height);
  return (0);
}


static int move(void *componentData, int xCoord, int yCoord)
{
  int status = 0;
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindowList *list = (kernelWindowList *) component->data;
  int scrollBarX = 0;

  // Move our scroll bars
  if (list->scrollBar)
    {
      scrollBarX = (xCoord + list->itemWidth);

      if (list->scrollBar->move)
	status = list->scrollBar
	  ->move((void *) list->scrollBar, scrollBarX, yCoord);

      list->scrollBar->xCoord = scrollBarX;
      list->scrollBar->yCoord = yCoord;
    }

  return (status);
}


static int resize(void *componentData, int width, int height)
{
  int status = 0;
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindowList *list = (kernelWindowList *) component->data;
  kernelWindowComponent *containerComponent = list->container;
  kernelWindowContainer *container = containerComponent->data;
  int scrollBarX = 0;
  int count;

  if (width != component->width)
    {
      list->itemWidth =
	(width - (list->scrollBar? list->scrollBar->width : 0));
 
      // Set all the item widths
      for (count = 0; count < container->numComponents; count ++)
	{
	  if (container->components[count]->resize)
	    status = container->components[count]
	      ->resize((void *) container->components[count], list->itemWidth,
		       container->components[count]->height);

	  container->components[count]->width = list->itemWidth;
	}
    }

  if (height != component->height)
    {
      // See if we want to change the number of visible rows
      if (container->numComponents)
	{
	  if ((height / container->components[0]->height) != list->rows)
	    list->rows = (height / container->components[0]->height);
	}
    }

  // Move and resize our scroll bars
  if (list->scrollBar)
    {
      if (width != component->width)
	{
	  scrollBarX = ((component->xCoord + width) - list->scrollBar->width);
	  
	  if (list->scrollBar->move)
	    status = list->scrollBar
	      ->move((void *) list->scrollBar, scrollBarX, component->yCoord);

	  list->scrollBar->xCoord = scrollBarX;
	}

      if (height != component->height)
	{
	  if (list->scrollBar->resize)
	    status = list->scrollBar->resize((void *) list->scrollBar,
	  				     list->scrollBar->width, height);

	  list->scrollBar->height = height;
	}
    }

  return (status);
}

 
static int mouseEvent(void *componentData, windowEvent *event)
{
  // When mouse events happen to list components, we pass them on to the
  // appropriate kernelWindowListItem component

  int status = 0;
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindow *window = (kernelWindow *) component->window;
  kernelGraphicBuffer *buffer = &(window->buffer);
  kernelWindowList *list = (kernelWindowList *) component->data;
  kernelWindowContainer *container =
    (kernelWindowContainer *) list->container->data;
  kernelWindowScrollBar *scrollBar = NULL;
  int clickedItem = 0;
  int firstVisible = 0;
  
  // Is the event in one of our scroll bars?
  if (list->scrollBar && isMouseInScrollBar(event, list->scrollBar))
    {
      if (list->scrollBar->mouseEvent)
	{
	  // First, pass on the event to the scroll bar
	  status =
	    list->scrollBar->mouseEvent((void *) list->scrollBar, event);
	  if (status < 0)
	    return (status);
	}

      scrollBar = (kernelWindowScrollBar *) list->scrollBar->data;

      // Now, adjust the visible subcomponents based on the 'position percent'
      // of the scroll bar
      if (container->numComponents > list->rows)
	{
	  firstVisible = (((container->numComponents - list->rows) *
			   scrollBar->state.positionPercent) / 100);

	  if (firstVisible != list->firstVisible)
	    {
	      list->firstVisible = firstVisible;

	      if (component->draw)
		status = component->draw(componentData);

	      kernelWindowUpdateBuffer(buffer, component->xCoord,
				       component->yCoord, component->width,
				       component->height);
	    }
	}

      return (status);
    }

  else if (container->numComponents && (event->type == EVENT_MOUSE_LEFTDOWN))
    {
      // Figure out which list item was clicked based on the coordinates
      // of the event
      clickedItem =
	(((event->yPosition - (window->yCoord + component->yCoord)) /
	  container->components[0]->height) + list->firstVisible);

      // Is there an item in this space?
      if (clickedItem >= container->numComponents)
	return (status = 0);
	  
      if (container->components[clickedItem]->mouseEvent)
	status = container->components[clickedItem]
	  ->mouseEvent((void *) container->components[clickedItem], event);

      if (container->components[clickedItem]
	  ->getSelected((void *) container->components[clickedItem]))
	{
	  // If some other item has been clicked, deselect the old one
	  if (!(list->selectMultiple) && (list->selectedItem != -1) &&
	      (clickedItem != list->selectedItem))
	    container->components[list->selectedItem]
	      ->setSelected((void *) container->components[list->selectedItem],
			    0);
	  list->selectedItem = clickedItem;
	}
      else
	{
	  if (!(list->selectMultiple))
	    {
	      // Can't deselect an item when 'select multiple' is not in
	      // effect.  Have to select something else insead.
	      container->components[clickedItem]
		->setSelected((void *) container->components[clickedItem], 1);
	      list->selectedItem = clickedItem;
	    }
	  else
	    list->selectedItem = -1;
	}
    }

  return (status);
}


static int keyEvent(void *componentData, windowEvent *event)
{
  // We allow the user to control the list widget with key presses, such
  // as cursor movements.

  int status = 0;
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindowList *list = (kernelWindowList *) component->data;
  kernelWindowContainer *container =
    (kernelWindowContainer *) list->container->data;
  int newSelected;

  if ((event->type == EVENT_KEY_DOWN) &&
      ((event->key == 17) || (event->key == 20)))
    {
      newSelected = list->selectedItem;

      // Check for cursor up
      if ((event->key == 17) && (list->selectedItem > 0))
	newSelected = (list->selectedItem - 1);
      // Must be cursor down
      else if ((event->key == 20) &&
	       (list->selectedItem < (container->numComponents - 1)))
	newSelected = (list->selectedItem + 1);

      if (newSelected != list->selectedItem)
	{
	  // The selected list item has changed.

	  container->components[list->selectedItem]
	    ->setSelected((void *) container->components[list->selectedItem],
			  0);
	  list->selectedItem = newSelected;
	  container->components[list->selectedItem]
	    ->setSelected((void *) container->components[list->selectedItem],
			  1);

	  // See if we have to scroll
	  checkScroll(component);
	}
    }

  return (status);
}


static int destroy(void *componentData)
{
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindowList *list = (kernelWindowList *) component->data;
  kernelWindowContainer *container =
    (kernelWindowContainer *) list->container->data;

  // Loop through the subcomponents, calling their destroy() functions
  while (container->numComponents)
    kernelWindowComponentDestroy(container
				 ->components[container->numComponents - 1]);

  // Release all our memory
  if (list)
    {
      if (list->container)
	kernelWindowComponentDestroy(list->container);
      
      if (list->scrollBar)
	kernelWindowComponentDestroy(list->scrollBar);

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


kernelWindowComponent *kernelWindowNewList(volatile void *parent, int rows,
					   int columns, int selectMultiple,
					   const char *items[], int numItems,
					   componentParameters *params)
{
  // Formats a kernelWindowComponent as a kernelWindowList

  int status = 0;
  kernelWindowComponent *component = NULL;
  kernelWindowList *list = NULL;
  kernelWindowContainer *container = NULL;

  // Check parameters.
  if ((parent == NULL) || (items == NULL) || (params == NULL))
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

  // Get memory for this list component
  list = kernelMalloc(sizeof(kernelWindowList));
  if (list == NULL)
    {
      kernelFree((void *) component);
      return (component = NULL);
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

  list->columns = columns;
  list->rows = rows;
  list->selectMultiple = selectMultiple;

  // Now populate this component
  component->type = listComponentType;
  component->flags |= (WINFLAG_CANFOCUS | WINFLAG_RESIZABLE);
  component->data = (void *) list;

  // The functions
  component->draw = &draw;
  component->focus = &focus;
  component->getSelected = &getSelected;
  component->setSelected = &setSelected;
  component->setData = &setData;
  component->move = &move;
  component->resize = &resize;
  component->mouseEvent = &mouseEvent;
  component->keyEvent = &keyEvent;
  component->destroy = &destroy;

  // Get our container component
  list->container =
    kernelWindowNewContainer(parent, "windowlist container", params);
  if (list->container == NULL)
    {
      kernelFree((void *) component);
      kernelFree((void *) list);
      return (component = NULL);
    }
  container = (kernelWindowContainer *) list->container->data;

  // Get our scrollbar component
  list->scrollBar =
    kernelWindowNewScrollBar(parent, scrollbar_vertical, 0, component->height,
			     params);
  if (list->scrollBar == NULL)
    {
      kernelFree((void *) list);
      kernelFree((void *) component);
      return (component = NULL);
    }

  // Standard parameters for a scroll bar
  list->scrollBar->parameters.useDefaultForeground = 1;
  list->scrollBar->parameters.useDefaultBackground = 1;

  // Remove the container and scrollbar from the parent container
  if (((kernelWindow *) parent)->type == windowType)
    {
      kernelWindow *tmpWindow = getWindow(parent);
      kernelWindowContainer *tmpContainer =
	(kernelWindowContainer *) tmpWindow->mainContainer->data;
      tmpContainer->containerRemove(tmpWindow->mainContainer, list->container);
      tmpContainer->containerRemove(tmpWindow->mainContainer, list->scrollBar);
    }
  else
    {
      kernelWindowContainer *tmpContainer = (kernelWindowContainer *)
	((kernelWindowComponent *) parent)->data;
      tmpContainer->containerRemove((kernelWindowComponent *) parent,
				    list->container);
      tmpContainer->containerRemove((kernelWindowComponent *) parent,
				    list->scrollBar);
    }

  // Fill up
  populateList(component, items, numItems);

  component->minWidth = component->width;
  component->minHeight = component->height;

  // Take care of any default selection
  if (selectMultiple)
    list->selectedItem = -1;
  else
    // Multiple selections are not allowed, so we select the first one
    if (numItems)
      {
	((kernelWindowListItem *) container->components[0]->data)
	  ->selected = 1;
	list->selectedItem = 0;
      }

  return (component);
}
