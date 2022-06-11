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
//  kernelWindowList.c
//

// This code is for managing kernelWindowList objects.  These are containers
// for kernelWindowListItem components.

#include "kernelWindow.h"     // Our prototypes are here
#include "kernelMalloc.h"
#include "kernelMisc.h"
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

  scrollBarState state;

  if (list->itemRows > list->rows)
    {
      state.positionPercent = ((list->firstVisibleRow * 100) /
			       (list->itemRows - list->rows));
      state.displayPercent = ((list->rows * 100) / list->itemRows);
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
  kernelWindowContainer *container =
    (kernelWindowContainer *) list->container->data;
  kernelWindowComponent *listItemComponent = NULL;

  if (list->selectedItem == -1)
    return;

  listItemComponent = container->components[list->selectedItem];

  // Do we have to scroll the list?
  if ((listItemComponent->parameters.gridY < list->firstVisibleRow) ||
      (listItemComponent->parameters.gridY >=
       (list->firstVisibleRow + list->rows)))
    {
      if (listItemComponent->parameters.gridY < list->firstVisibleRow)
	list->firstVisibleRow = listItemComponent->parameters.gridY;
      else if (listItemComponent->parameters.gridY >=
	       (list->firstVisibleRow + list->rows))
	list->firstVisibleRow =
	  ((listItemComponent->parameters.gridY - list->rows) + 1);

      if (list->scrollBar)
	// Set the scroll bar display percent
	setScrollBar(list);

      component->draw((void *) component);
      kernelWindowUpdateBuffer(&((kernelWindow *) component->window)->buffer,
			       component->xCoord, component->yCoord,
			       component->width, component->height);
    }
}


static void setItemRows(kernelWindowList *list)
{
  kernelWindowContainer *container = (kernelWindowContainer *)
    list->container->data;

  if (list->columns)
    {
      list->itemRows = (container->numComponents / list->columns);
      if (container->numComponents % list->columns)
	list->itemRows += 1;
    }
}


static void layoutItems(kernelWindowList *list)
{
  // Loop through the list items in the container and put them into a grid

  kernelWindowContainer *container =
    (kernelWindowContainer *) list->container->data;
  kernelWindowComponent *listItemComponent = NULL;
  int column = 0;
  int row = 0;
  int count;

  for (count = 0; (count < container->numComponents); count ++)
    {
      listItemComponent = container->components[count];

      listItemComponent->parameters.gridX = column;
      listItemComponent->parameters.gridY = row;

      column += 1;
      if (column >= list->columns)
	{
	  row += 1;
	  column = 0;
	}
    }  
}


static void setItemSizes(kernelWindowList *list)
{  
  kernelWindowContainer *container =
    (kernelWindowContainer *) list->container->data;
  kernelWindowComponent *listItemComponent = NULL;
  int count;

  // Set the widths of all the items
  for (count = 0; count < container->numComponents; count ++)
    {
      listItemComponent = container->components[count];
      if (listItemComponent->resize)
	listItemComponent->resize((void *) listItemComponent, list->itemWidth,
				  list->itemHeight);
      listItemComponent->width = list->itemWidth;
      listItemComponent->height = list->itemHeight;
    }
}


static void populateList(kernelWindowComponent *listComponent,
			 listItemParameters *items, int numItems)
{
  // Sets up all the kernelWindowList subcomponents of the list
  
  kernelWindowList *list = (kernelWindowList *) listComponent->data;
  kernelWindowContainer *container =
    (kernelWindowContainer *) list->container->data;
  kernelWindowComponent *listItemComponent = NULL;
  int setSize = 0;
  componentParameters params;
  int count;

  // If the list already has components, get rid of them
  while (container->numComponents)
    kernelWindowComponentDestroy(container
				 ->components[container->numComponents - 1]);

  // If no items, nothing to do
  if (numItems == 0)
    {
      list->selectedItem = ERR_NODATA;
      return;
    }

  if (!listComponent->width || !listComponent->height)
    setSize = 1;

  // If the selected item is greater than the new number we have, make it the
  // last one
  if (list->selectedItem >= numItems)
    list->selectedItem = (numItems - 1);

  // Standard parameters for the list items
  kernelMemCopy((componentParameters *) &(listComponent->parameters), &params,
		sizeof(componentParameters));
  params.gridX = 0;
  params.gridY = 0;
  params.gridWidth = 1;
  params.gridHeight = 1;
  params.padTop = 0;
  params.padBottom = 0;
  params.padLeft = 0;
  params.padRight = 0;
  params.orientationX = orient_top;
  params.orientationY = orient_left;
  params.flags = (WINDOW_COMPFLAG_FIXEDWIDTH | WINDOW_COMPFLAG_FIXEDHEIGHT);

  list->itemWidth = 0;
  list->itemHeight = 0;

  // Loop through the list item parameter list, creating kernelWindowListItem
  // components and adding them to this component
  for (count = 0; count < numItems; count ++)
    {
      listItemComponent = kernelWindowNewListItem(list->container, list->type,
						  &(items[count]), &params);
      if (listItemComponent == NULL)
	continue;

      if (listItemComponent->width > list->itemWidth)
	list->itemWidth = listItemComponent->width;

      if (listItemComponent->height > list->itemHeight)
	list->itemHeight = listItemComponent->height;

      if (count == list->selectedItem)
	((kernelWindowListItem *) listItemComponent->data)->selected = 1;
    }

  if (setSize)
    {
      list->container->width = (list->columns * list->itemWidth);
      list->container->height = (list->rows * list->itemHeight);
      listComponent->width = list->container->width;
      listComponent->height = list->container->height;

      if (list->scrollBar)
	{
	  // Set up the scroll bar size and location and adjust the list
	  // component size to account for it
	  list->scrollBar->xCoord =
	    (listComponent->xCoord + list->container->width);
	  list->scrollBar->height = listComponent->height;
	  listComponent->width += list->scrollBar->width;
	}
    }

  else
    {
      if (list->itemWidth && list->multiColumn &&
	  ((list->container->width / list->itemWidth) != list->columns))
	list->columns = (list->container->width / list->itemWidth);

      if (list->itemHeight && 
	  ((list->container->height / list->itemHeight) != list->rows))
	list->rows = (list->container->height / list->itemHeight);

      if (!list->multiColumn)
	list->itemWidth = list->container->width;
    }

  // Adjust the number of item rows (not just visible ones)
  setItemRows(list);

  // Lay out the grid coordinates of the items based on the numbers of
  // item rows and columns
  layoutItems(list);

  // Set the widths of all the items
  setItemSizes(list);

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
  kernelWindowComponent *listItemComponent = NULL;
  kernelWindowListItem *listItem = NULL;
  int xCoord, yCoord;
  int count;

  // Draw the background of the list
  kernelGraphicDrawRect(buffer, (color *) &(component->parameters.background),
			draw_normal, component->xCoord,
			component->yCoord, component->width, component->height,
			1, 1);

  // Loop through the visible subcomponents, setting visible/not and calling
  // their draw() routines if applicable.
  for (count = 0; count < container->numComponents; count ++)
    {
      listItemComponent = container->components[count];
      listItem = (kernelWindowListItem *) listItemComponent->data;

      if ((listItemComponent->parameters.gridY >= list->firstVisibleRow) &&
	  (listItemComponent->parameters.gridY <
	   (list->rows + list->firstVisibleRow)))
	{
	  xCoord =
	    (component->xCoord + (listItemComponent->parameters.gridX *
				  list->itemWidth));
	  yCoord =
	    (component->yCoord + ((listItemComponent->parameters.gridY -
				   list->firstVisibleRow) *
				  listItemComponent->height));

	  if (listItemComponent->move)
	    listItemComponent
	      ->move((void *) listItemComponent, xCoord, yCoord);
	  listItemComponent->xCoord = xCoord;
	  listItemComponent->yCoord = yCoord;

	  listItemComponent->flags |= WINFLAG_VISIBLE;

	  if (listItemComponent->draw)
	    listItemComponent->draw((void *) listItemComponent);
	}
      else
	listItemComponent->flags &= ~WINFLAG_VISIBLE;
    }

  // Draw the scroll bar
  if (list->scrollBar && list->scrollBar->draw)
    list->scrollBar->draw((void *) list->scrollBar);

  if ((component->parameters.flags & WINDOW_COMPFLAG_HASBORDER) ||
      (component->flags & WINFLAG_HASFOCUS))
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


static int getSelected(void *componentData, int *itemNumber)
{
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindowList *list = (kernelWindowList *) component->data;
  *itemNumber = list->selectedItem;
  return (0);
}


static int setSelected(void *componentData, int item)
{
  // The selected list item has changed.

  int status = 0;
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindowList *list = (kernelWindowList *) component->data;
  kernelWindowContainer *container =
    (kernelWindowContainer *) list->container->data;
  kernelWindowComponent *listItemComponent = NULL;

  if (item >= container->numComponents)
    {
      kernelError(kernel_error, "Illegal component number %d", item);
      return (status = ERR_BOUNDS);
    }

  if (list->selectedItem != -1)
    {
      // Deselect the old selected item
      listItemComponent = container->components[list->selectedItem];
      listItemComponent->setSelected((void *) listItemComponent, 0);
    }

  if (item != -1)
    {
      // Select the selected item
      listItemComponent = container->components[item];
      listItemComponent->setSelected((void *) listItemComponent, 1);
    }

  list->selectedItem = item;

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
  populateList(component, (listItemParameters *) buffer, size);

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
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindowList *list = (kernelWindowList *) component->data;
  int scrollBarX = 0;

  // Move our container
  if (list->container->move)
    list->container->move((void *) list->container, xCoord, yCoord);
  list->container->xCoord = xCoord;
  list->container->yCoord = yCoord;

  // Move our scroll bars
  if (list->scrollBar)
    {
      scrollBarX = (xCoord + list->container->width);

      if (list->scrollBar->move)
	list->scrollBar->move((void *) list->scrollBar, scrollBarX, yCoord);
      list->scrollBar->xCoord = scrollBarX;
      list->scrollBar->yCoord = yCoord;
    }

  return (0);
}


static int resize(void *componentData, int width, int height)
{
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindowList *list = (kernelWindowList *) component->data;
  int containerWidth = 0;
  int scrollBarX = 0;

  containerWidth = (width - (list->scrollBar? list->scrollBar->width : 0));

  if (list->container->resize)
    list->container->resize((void *) list->container, list->itemWidth, height);
  list->container->width = containerWidth;
  list->container->height = height;

  if (list->multiColumn)
    {
      if (list->itemWidth &&
	  ((containerWidth / list->itemWidth) != list->columns))
	list->columns = (containerWidth / list->itemWidth);
    }
  else
    {
      // Re-set the item width to the container width
      list->itemWidth = containerWidth;
  
      // Set the sizes of our items
      setItemSizes(list);
    }

  if (list->itemHeight && ((height / list->itemHeight) != list->rows))
    list->rows = (height / list->itemHeight);

  // Adjust the number of item rows (not just visible ones)
  setItemRows(list);

  // Re-set the grid parameters for all
  layoutItems(list);

  // Move and resize our scroll bars
  if (list->scrollBar)
    {
      if (width != component->width)
	{
	  scrollBarX = (component->xCoord + list->container->width);
	  
	  if (list->scrollBar->move)
	    list->scrollBar->move((void *) list->scrollBar, scrollBarX,
				  component->yCoord);
	  list->scrollBar->xCoord = scrollBarX;
	}

      if (height != component->height)
	{
	  if (list->scrollBar->resize)
	    list->scrollBar->resize((void *) list->scrollBar,
				    list->scrollBar->width, height);
	  list->scrollBar->height = height;
	}
    }

  // Update the scroll bar position percent
  setScrollBar(list);

  return (0);
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
  kernelWindowComponent *listItemComponent = NULL;
  int firstVisibleRow = 0;
  int count;
  
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
	  firstVisibleRow = (((list->itemRows - list->rows) *
			      scrollBar->state.positionPercent) / 100);

	  if (firstVisibleRow != list->firstVisibleRow)
	    {
	      list->firstVisibleRow = firstVisibleRow;

	      if (component->draw)
		component->draw(componentData);

	      kernelWindowUpdateBuffer(buffer, component->xCoord,
				       component->yCoord, component->width,
				       component->height);
	    }
	}
    }

  else if (event->type & EVENT_MASK_MOUSE)
    {
      // Figure out which list item was clicked based on the coordinates
      // of the event
      for (count = 0; count < container->numComponents; count ++)
	{
	  listItemComponent = container->components[count];

	  if ((listItemComponent->flags & WINFLAG_VISIBLE) &&
	      isPointInside(event->xPosition, event->yPosition,
			    makeComponentScreenArea(listItemComponent)))
	    {
	      if (listItemComponent->mouseEvent)
		listItemComponent->mouseEvent((void *) listItemComponent,
					      event);

	      if (event->type & EVENT_MOUSE_LEFTDOWN)
		// Tell the list item to show selected
		setSelected((void *) component, count);

	      else if (event->type & EVENT_MOUSE_LEFTUP)
		// Make this also a 'selection' event
		event->type |= EVENT_SELECTION;
	    }
	}
    }

  return (status = 0);
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
  kernelWindowComponent *listItemComponent = NULL;
  int gridX = 0, gridY = 0;
  int count;

  if (event->type == EVENT_KEY_DOWN)
    {
      if (!container->numComponents)
	return (status = 0);

      // Get the currently selected item
      listItemComponent = container->components[list->selectedItem];
      gridX = listItemComponent->parameters.gridX;
      gridY = listItemComponent->parameters.gridY;

      switch (event->key)
	{
	case 17:
	  // Cursor up
	  if (gridY > 0)
	    gridY -= 1;
	  break;

	case 20:
	  // Cursor down
	  if (gridY < (list->itemRows - 1))
	    gridY += 1;
	  break;

	case 18:
	  // Cursor left
	  if (gridX > 0)
	    gridX -= 1;
	  else if (gridY > 0)
	    {
	      gridX = (list->columns - 1);
	      gridY -= 1;
	    }
	  break;

	case 19:
	  // Cursor right
	  if (gridX < (list->columns - 1))
	    gridX += 1;
	  else if (gridY < (list->itemRows - 1))
	    {
	      gridX = 0;
	      gridY += 1;
	    }
	  break;

	case 10:
	  // ENTER.  We will make this also a 'selection' event.
	  event->type |= EVENT_SELECTION;
	  break;

	default:
	  break;
	}

      if ((gridX != listItemComponent->parameters.gridX) ||
	  (gridY != listItemComponent->parameters.gridY))
	{
	  // Find an item with these coordinates
	  for (count = 0; count < container->numComponents; count ++)
	    {
	      if ((container->components[count]->parameters.gridX == gridX) &&
		  (container->components[count]->parameters.gridY == gridY))
		{
		  setSelected((void *) component, count);

		  // Make this also a 'selection' event
		  event->type |= EVENT_SELECTION;
		  break;
		}
	    }
	}
    }
  
  return (status = 0);
}


static int destroy(void *componentData)
{
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindowList *list = (kernelWindowList *) component->data;

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


kernelWindowComponent *kernelWindowNewList(volatile void *parent,
					   windowListType type, int rows,
					   int columns, int selectMultiple,
					   listItemParameters *items,
					   int numItems,
					   componentParameters *params)
{
  // Formats a kernelWindowComponent as a kernelWindowList

  int status = 0;
  kernelWindowComponent *component = NULL;
  kernelWindowList *list = NULL;
  kernelWindowContainer *container = NULL;
  componentParameters subParams;

  // Check parameters.
  if ((parent == NULL) || (items == NULL) || (params == NULL))
    return (component = NULL);

  // Get the basic component structure
  component = kernelWindowComponentNew(parent, params);
  if (component == NULL)
    return (component);

  // If default colors were requested, override the standard background color
  // with the one we prefer (white)
  if (!(component->parameters.flags & WINDOW_COMPFLAG_CUSTOMBACKGROUND))
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
      status =
	kernelFontLoad(WINDOW_DEFAULT_VARFONT_MEDIUM_FILE,
		       WINDOW_DEFAULT_VARFONT_MEDIUM_NAME, &labelFont, 0);
      if (status < 0)
	// Font's not there, we suppose.  There's always a default.
	kernelFontGetDefault(&labelFont);
    }

  // If font is NULL, use the default
  if (component->parameters.font == NULL)
    component->parameters.font = labelFont;

  list->type = type;
  list->columns = columns;
  list->rows = rows;
  list->selectMultiple = selectMultiple;
  if (list->columns > 1)
    list->multiColumn = 1;

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

  // Standard parameters for a scroll bar
  kernelMemCopy(params, &subParams, sizeof(componentParameters));
  subParams.flags &=
    ~(WINDOW_COMPFLAG_CUSTOMFOREGROUND | WINDOW_COMPFLAG_CUSTOMBACKGROUND);

  // Get our scrollbar component
  list->scrollBar =
    kernelWindowNewScrollBar(parent, scrollbar_vertical, 0, component->height,
			     &subParams);
  if (list->scrollBar == NULL)
    {
      kernelFree((void *) list);
      kernelFree((void *) component);
      return (component = NULL);
    }

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
