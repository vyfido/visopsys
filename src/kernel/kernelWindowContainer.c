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
//  kernelWindowContainer.c
//

// This code is for managing kernelWindowContainer objects.  These are
// containers for all other types of components.

#include "kernelWindowManager.h"     // Our prototypes are here
#include "kernelMalloc.h"
#include "kernelMiscFunctions.h"
#include "kernelError.h"
#include <string.h>
#include <sys/errors.h>


static int doAreasIntersect(screenArea *firstArea, screenArea *secondArea)
{
  // Return 1 if area 1 and area 2 intersect.

  if (isPointInside(firstArea->leftX, firstArea->topY, secondArea) ||
      isPointInside(firstArea->rightX, firstArea->topY, secondArea) ||
      isPointInside(firstArea->leftX, firstArea->bottomY, secondArea) ||
      isPointInside(firstArea->rightX, firstArea->bottomY, secondArea) ||
      isPointInside(secondArea->leftX, secondArea->topY, firstArea) ||
      isPointInside(secondArea->rightX, secondArea->topY, firstArea) ||
      isPointInside(secondArea->leftX, secondArea->bottomY, firstArea) ||
      isPointInside(secondArea->rightX, secondArea->bottomY, firstArea))
    return (1);

  else if (doLinesIntersect(firstArea->leftX, firstArea->topY,
			    firstArea->rightX,
			    secondArea->leftX, secondArea->topY,
			    secondArea->bottomY) ||
	   doLinesIntersect(secondArea->leftX, secondArea->topY,
			    secondArea->rightX,
			    firstArea->leftX, firstArea->topY,
			    firstArea->bottomY))
    return (1);

  else
    // Nope, not intersecting
    return (0);
}


static int draw(void *componentData)
{
  // Draw the component, which is really just a collection of other components.

  int status = 0;
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;

  if (component->parameters.hasBorder)
    component->drawBorder((void *) component);

  return (status);
}


static int move(void *componentData, int xCoord, int yCoord)
{
  // Loop through the subcomponents, adjusting their coordinates and calling
  // their move() functions

  int status = 0;
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindowContainer *container = (kernelWindowContainer *) component->data;
  int count;

  int differenceX = (xCoord - component->xCoord);
  int differenceY = (yCoord - component->yCoord);

  for (count = 0; count < container->numComponents; count ++)
    {
      if (container->components[count]->move)
	{
	  status = container->components[count]
	    ->move((void *) container->components[count],
		   (container->components[count]->xCoord + differenceX),
		   (container->components[count]->yCoord + differenceY));
	  if (status < 0)
	    return (status);
	}

      container->components[count]->xCoord += differenceX;
      container->components[count]->yCoord += differenceY;
    }

  return (status = 0);
}


static int destroy(void *componentData)
{
  int status = 0;
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;

  // Release all our memory
  if (component->data)
    kernelFree(component->data);

  return (status = 0);
}


static int containerAdd(kernelWindowComponent *containerComponent,
			kernelWindowComponent *component,
			componentParameters *params)
{
  // Add the supplied component to the container.

  int status = 0;
  kernelWindowContainer *container = NULL;

  // Check params
  if ((containerComponent == NULL) || (component == NULL) || (params == NULL))
    {
      kernelError(kernel_error, "NULL container or component");
      return (status = ERR_NULLPARAMETER);
    }
  
  container = (kernelWindowContainer *) containerComponent->data;

  // Make sure there's room for more components
  if (container->numComponents >= WINDOW_MAX_COMPONENTS)
    {
      kernelError(kernel_error, "Component container is full");
      return (status = ERR_BOUNDS);
    }

  // Add it to the container
  container->components[container->numComponents++] = component;

  // Copy the parameters into the component
  kernelMemCopy(params, (void *) &(component->parameters),
		sizeof(componentParameters));

  return (status);
}


static int containerRemove(kernelWindowComponent *containerComponent,
			   kernelWindowComponent *component)
{
  // Removes a component from a container

  int status = 0;
  kernelWindowContainer *container = NULL;
  int count = 0;

  // Check params
  if ((containerComponent == NULL) || (component == NULL))
    {
      kernelError(kernel_error, "NULL container or component");
      return (status = ERR_NULLPARAMETER);
    }
  
  container = (kernelWindowContainer *) containerComponent->data;

  for (count = 0; count < container->numComponents; count ++)
    if (container->components[count] == component)
      {
	// Replace the component with the last one, if applicable
	container->numComponents--;
	if ((container->numComponents > 0) &&
	    (count < container->numComponents))
	  container->components[count] =
	    container->components[container->numComponents];
	break;
      }

  return (status = 0);
}


static int containerLayout(kernelWindowComponent *containerComponent)
{
  // Do layout for the container.

  int status = 0;
  kernelWindow *window = NULL;
  kernelWindowContainer *container = NULL;
  unsigned columnWidth[WINDOW_MAX_COMPONENTS];
  unsigned columnStartX[WINDOW_MAX_COMPONENTS];
  unsigned rowHeight[WINDOW_MAX_COMPONENTS];
  unsigned rowStartY[WINDOW_MAX_COMPONENTS];
  unsigned numColumns = 0, numRows = 0, totalWidth = 0, totalHeight = 0;
  kernelWindowComponent *component = NULL;
  unsigned componentSize = 0;
  unsigned columnSpanWidth, rowSpanHeight;
  int padWidth = 0, padHeight = 0;
  int xCoord, yCoord, column, row, count1, count2;

  // Check params
  if (containerComponent == NULL)
    {
      kernelError(kernel_error, "NULL container for layout");
      return (status = ERR_NULLPARAMETER);
    }

  window = (kernelWindow *) containerComponent->window;
  container = (kernelWindowContainer *) containerComponent->data;

  // Clear our arrays
  for (count1 = 0; count1 < WINDOW_MAX_COMPONENTS; count1++)
    {
      columnWidth[count1] = 0;
      columnStartX[count1] = 0;
      rowHeight[count1] = 0;
      rowStartY[count1] = 0;
    }

  // Find the width and height of each column and row based on the widest
  // or tallest component of each.
  for (count1 = 0; count1 < container->numComponents; count1 ++)
    {
      component = container->components[count1];

      // If this is a container component, do its layout so we know its size
      if (component->type == containerComponentType)
	{
	  kernelWindowContainer *tmpContainer =
	    (kernelWindowContainer *) component->data;

	  status = tmpContainer->containerLayout(component);
	  if (status < 0)
	    return (status);
	}

      componentSize = (component->width + component->parameters.padLeft +
		       component->parameters.padRight);
      if (component->parameters.gridWidth != 0)
	{
	  componentSize /= component->parameters.gridWidth;
	  for (count2 = 0; count2 < component->parameters.gridWidth;
	       count2 ++)
	    if (componentSize >
		columnWidth[component->parameters.gridX + count2])
	      columnWidth[component->parameters.gridX + count2] =
		componentSize;

	  if ((component->parameters.gridX + 1) > numColumns)
	    numColumns = (component->parameters.gridX + 1);
	}

      componentSize = (component->height + component->parameters.padTop +
		       component->parameters.padBottom);
      if (component->parameters.gridHeight != 0)
	{
	  componentSize /= component->parameters.gridHeight;
	  for (count2 = 0; count2 < component->parameters.gridHeight;
	       count2 ++)
	    if (componentSize > 
		rowHeight[component->parameters.gridY + count2])
	      rowHeight[component->parameters.gridY + count2] =
		componentSize;

	  if ((component->parameters.gridY + 1) > numRows)
	    numRows = (component->parameters.gridY + 1);
	}
    }

  // Now, if the sums of column widths and column heights are less than the
  // dimensions of the window, average the difference over them.
  if (numColumns)
    {
      for (column = 0; column < WINDOW_MAX_COMPONENTS; column ++)
	totalWidth += columnWidth[column];

      if (!container->doneLayout)
	containerComponent->width = totalWidth;

      if (!(window->flags & WINFLAG_PACKED) &&
	  (containerComponent->width > totalWidth))
	padWidth = ((containerComponent->width - totalWidth) / numColumns);
      else
	containerComponent->width = totalWidth;
    }

  if (numRows)
    {
      for (row = 0; row < WINDOW_MAX_COMPONENTS; row ++)
	totalHeight += rowHeight[row];

      if (!container->doneLayout)
	containerComponent->height = totalHeight;

      if (!(window->flags & WINFLAG_PACKED) &&
	  (containerComponent->height > totalHeight))
	padHeight = ((containerComponent->height - totalHeight) / numRows);
      else
        containerComponent->height = totalHeight;
    }

  // Set the starting X coordinates of the columns. The coordinates of the
  // column or row is the sum of the widths/heights of all previous
  // columns/rows
  for (column = 0; column < WINDOW_MAX_COMPONENTS; column ++)
    if (columnWidth[column])
      {
	columnStartX[column] = containerComponent->xCoord;

	for (count1 = 0; (count1 < column); count1 ++)
	  columnStartX[column] += columnWidth[count1];

	(int) columnWidth[column] += padWidth;
      }

  // Set the starting Y coordinates of the rows
  for (row = 0; row < WINDOW_MAX_COMPONENTS; row ++)
    if (rowHeight[row])
      {
	rowStartY[row] = containerComponent->yCoord;

	for (count1 = 0; (count1 < row); count1 ++)
	  rowStartY[row] += rowHeight[count1];

	(int) rowHeight[row] += padHeight;
      }

  // Loop through each component setting the sizes and coordinates
  for (count1 = 0; count1 < container->numComponents; count1 ++)
    {
      component = container->components[count1];

      // Set the width and X coordinate
      {
	columnSpanWidth = 0;
	for (column = 0; column < component->parameters.gridWidth; column ++)
	  columnSpanWidth +=
	    columnWidth[component->parameters.gridX + column];

	if (padWidth && (component->flags & WINFLAG_RESIZABLE) &&
	    component->parameters.resizableX)
	  component->width =
	    (columnSpanWidth - (component->parameters.padRight +
				component->parameters.padLeft));

	xCoord = columnStartX[component->parameters.gridX];
      
	switch(component->parameters.orientationX)
	  {
	  case orient_right:
	    xCoord += (columnSpanWidth - ((component->width + 1) +
					  component->parameters.padRight));
	    break;
	  case orient_center:
	    xCoord += ((columnSpanWidth - component->width) / 2);
	    break;
	  case orient_left:
	  default:
	    xCoord += component->parameters.padLeft;
	    break;
	  }
      }

      // Set the Y coordinate
      {
	rowSpanHeight = 0;
	for (row = 0; row < component->parameters.gridHeight; row ++)
	  rowSpanHeight += rowHeight[component->parameters.gridY + row];

	if (padHeight && (component->flags & WINFLAG_RESIZABLE) &&
	    (component->parameters.resizableY))
	  component->height =
	    (rowSpanHeight - (component->parameters.padTop +
			      component->parameters.padBottom));

	yCoord = rowStartY[component->parameters.gridY]; 

	switch (component->parameters.orientationY)
	  {
	  case orient_bottom:
	    yCoord += (rowSpanHeight - ((component->height + 1) +
					component->parameters.padBottom));
	    break;
	  case orient_middle:
	    yCoord += ((rowSpanHeight - component->height) / 2);
	    break;
	  case orient_top:
	  default:
	    yCoord += component->parameters.padTop;
	    break;
	  }
      }

      // Tell the component that it has moved
      if (component->move)
	component->move((void *) component, xCoord, yCoord);
      component->xCoord = xCoord;
      component->yCoord = yCoord;

      // Tell the component it resized, if applicable
      if ((component->flags & WINFLAG_RESIZABLE) && component->resize)
	component->resize((void *) component, component->width,
			  component->height);

      // Check whether this component overlaps any others, and if so,
      // decrement their levels
      for (count2 = 0; count2 < container->numComponents; count2 ++)
	if (container->components[count2] != component)
	  {
	    if (doAreasIntersect(makeComponentScreenArea(component),
		 makeComponentScreenArea(container->components[count2])))
	      container->components[count2]->level++;
	  }
    }

  // Set the flag to indicate layout complete
  container->doneLayout = 1;

  return (status);
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


kernelWindowComponent *kernelWindowNewContainer(volatile void *parent,
						const char *name,
						componentParameters *params)
{
  // Formats a kernelWindowComponent as a kernelWindowContainer

  kernelWindowComponent *component = NULL;
  kernelWindowContainer *container = NULL;

  // Check parameters.
  if ((parent == NULL) || (params == NULL))
    return (component = NULL);

  // Get the basic component structure
  component = kernelWindowComponentNew(parent, params);
  if (component == NULL)
    return (component);

  // Get memory for this container component
  container = kernelMalloc(sizeof(kernelWindowContainer));
  if (container == NULL)
    {
      kernelFree((void *) component);
      return (component = NULL);
    }

  // Now populate the component

  strncpy((char *) container->name, name, WINDOW_MAX_LABEL_LENGTH);
  container->containerAdd = &containerAdd;
  container->containerRemove = &containerRemove;
  container->containerLayout = &containerLayout;  

  component->type = containerComponentType;
  component->flags |= WINFLAG_RESIZABLE;
  component->data = (void *) container;
  
  // The functions
  component->draw = &draw;
  component->move = &move;
  component->destroy = &destroy;
  
  return (component);
}
