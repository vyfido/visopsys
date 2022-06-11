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
//  kernelWindowContainer.c
//

// This code is for managing kernelWindowContainer objects.  These are
// containers for all other types of components.

#include "kernelWindow.h"     // Our prototypes are here
#include "kernelMalloc.h"
#include "kernelMisc.h"
#include "kernelError.h"
#include <string.h>


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


static void calculateGrid(kernelWindowComponent *containerComponent,
			  int *columnStartX, int *columnWidth, int *rowStartY,
			  int *rowHeight, int extraWidth, int extraHeight)
{
  kernelWindowContainer *container = NULL;
  kernelWindowComponent *component = NULL;
  int componentSize = 0;
  int count1, count2;

  container = containerComponent->data;

  // Clear our arrays
  for (count1 = 0; count1 < container->maxComponents; count1++)
    {
      columnWidth[count1] = 0;
      columnStartX[count1] = 0;
      rowHeight[count1] = 0;
      rowStartY[count1] = 0;
    }

  container->numColumns = 0;
  container->numRows = 0;

  // Find the width and height of each column and row based on the widest
  // or tallest component of each.
  for (count1 = 0; count1 < container->numComponents; count1 ++)
    {
      component = container->components[count1];
      
      componentSize = (component->minWidth + component->parameters.padLeft +
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
	}

      componentSize = (component->minHeight + component->parameters.padTop +
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
	}
    }

  // Count the numbers of rows and columns that have components in them
  for (count1 = 0; count1 < container->maxComponents; count1 ++)
    if (columnWidth[count1])
      container->numColumns += 1;
  for (count1 = 0; count1 < container->maxComponents; count1 ++)
    if (rowHeight[count1])
      container->numRows += 1;

  // Set the starting X coordinates of the columns, and distribute any extra
  // width over all the columns that have width
  if (container->numColumns)
    extraWidth /= container->numColumns;
  for (count1 = 0; count1 < container->maxComponents; count1 ++)
    {
      if (count1 == 0)
	columnStartX[count1] = containerComponent->xCoord;
      else
	columnStartX[count1] =
	  (columnStartX[count1 - 1] + columnWidth[count1 - 1]);
      
      if (columnWidth[count1])
	columnWidth[count1] += extraWidth;
    }

  // Set the starting Y coordinates of the rows, and distribute any extra
  // height over all the rows that have height
  if (container->numRows)
    extraHeight /= container->numRows;
  for (count1 = 0; count1 < container->maxComponents; count1 ++)
    {
      if (count1 == 0)
	rowStartY[count1] = containerComponent->yCoord;
      else
	rowStartY[count1] = (rowStartY[count1 - 1] + rowHeight[count1 - 1]);
      
      if (rowHeight[count1])
	rowHeight[count1] += extraHeight;
    }
}


static int layoutSize(kernelWindowComponent *containerComponent, int width,
		      int height)
{
  // Loop through the subcomponents, adjusting their dimensions and calling
  // their resize() functions

  int status = 0;
  kernelWindowContainer *container = NULL;
  kernelWindowComponent *component = NULL;
  int *columnWidth = NULL;
  int *columnStartX = NULL;
  int *rowHeight = NULL;
  int *rowStartY = NULL;
  int tmpWidth, tmpHeight;
  int tmpX, tmpY;
  int count1, count2;

  container = (kernelWindowContainer *) containerComponent->data;

  columnWidth = kernelMalloc(container->maxComponents * sizeof(int));
  columnStartX = kernelMalloc(container->maxComponents * sizeof(int));
  rowHeight = kernelMalloc(container->maxComponents * sizeof(int));
  rowStartY = kernelMalloc(container->maxComponents * sizeof(int));
  if ((columnWidth == NULL) || (columnStartX == NULL) ||
      (rowHeight == NULL) || (rowStartY == NULL))
    {
      status = ERR_MEMORY;
      goto out;
    }

  // Don't go beyond minimum sizes
  if (width < containerComponent->minWidth)
    width = containerComponent->minWidth;
  if (height < containerComponent->minHeight)
    height = containerComponent->minHeight;

  // Calculate the grid with the extra/less space factored in
  calculateGrid(containerComponent, columnStartX, columnWidth, rowStartY,
		rowHeight, (width - containerComponent->minWidth),
		(height - containerComponent->minHeight));

  for (count1 = 0; count1 < container->numComponents; count1 ++)
    {
      // Resize the component
      
      component = container->components[count1];

      tmpWidth = 0;
      if ((component->flags & WINFLAG_RESIZABLEX) &&
	  !(component->parameters.flags & WINDOW_COMPFLAG_FIXEDWIDTH))
	{
	  for (count2 = 0; count2 < component->parameters.gridWidth; count2 ++)
	    tmpWidth += columnWidth[component->parameters.gridX + count2];
	  tmpWidth -=
	    (component->parameters.padLeft + component->parameters.padRight);
	}
      else
	tmpWidth = component->width;
      if (tmpWidth < component->minWidth)
	tmpWidth = component->minWidth;

      tmpHeight = 0;
      if ((component->flags & WINFLAG_RESIZABLEY) &&
	  !(component->parameters.flags & WINDOW_COMPFLAG_FIXEDHEIGHT))
	{
	  for (count2 = 0; count2 < component->parameters.gridHeight;
	       count2 ++)
	    tmpHeight += rowHeight[component->parameters.gridY + count2];
	  tmpHeight -=
	    (component->parameters.padTop + component->parameters.padBottom);
	}
      else
	tmpHeight = component->height;
      if (tmpHeight < component->minHeight)
	tmpHeight = component->minHeight;

      if ((tmpWidth != component->width) || (tmpHeight != component->height))
	{
	  if (component->resize)
	    component->resize((void *) component, tmpWidth, tmpHeight);

	  component->width = tmpWidth;
	  component->height = tmpHeight;
	}

      // Move it too, if applicable

      tmpX = columnStartX[component->parameters.gridX];
      tmpWidth = 0;
      for (count2 = 0; count2 < component->parameters.gridWidth; count2 ++)
	tmpWidth += columnWidth[component->parameters.gridX + count2];
      switch (component->parameters.orientationX)
	{
	  case orient_left:
	    tmpX += component->parameters.padLeft;
	    break;
	  case orient_center:
	    tmpX += ((tmpWidth - component->width) / 2);
	    break;
	  case orient_right:
	    tmpX += ((tmpWidth - component->width) -
		     component->parameters.padRight);
	    break;
	}
      
      tmpY = rowStartY[component->parameters.gridY];
      tmpHeight = 0;
      for (count2 = 0; count2 < component->parameters.gridHeight; count2 ++)
	tmpHeight += rowHeight[component->parameters.gridY + count2];
      switch (component->parameters.orientationY)
	{
	  case orient_top:
	    tmpY += component->parameters.padTop;
	    break;
	  case orient_middle:
	    tmpY += ((tmpHeight - component->height) / 2);
	    break;
	  case orient_bottom:
	    tmpY += ((tmpHeight - component->height) -
		     component->parameters.padBottom);
	    break;
	}

      if ((tmpX != component->xCoord) || (tmpY != component->yCoord))
	{
	  if (component->move)
	    component->move((void *) component, tmpX, tmpY);
      
	  component->xCoord = tmpX;
	  component->yCoord = tmpY;
	}

      // Determine whether this component expands the bounds of our container
      tmpWidth = (component->xCoord + component->width +
		  component->parameters.padRight);
      if (tmpWidth > (containerComponent->xCoord + containerComponent->width))
	containerComponent->width = (tmpWidth - containerComponent->xCoord);
      tmpHeight = (component->yCoord + component->height +
		   component->parameters.padBottom);
      if (tmpHeight >
	  (containerComponent->yCoord + containerComponent->height))
	containerComponent->height = (tmpHeight - containerComponent->yCoord);
    }

  status = 0;

 out:
  if (columnWidth)
    kernelFree(columnWidth);
  if (columnStartX)
    kernelFree(columnStartX);
  if (rowHeight)
    kernelFree(rowHeight);
  if (rowStartY)
    kernelFree(rowStartY);

  return (status);
}


static int draw(void *componentData)
{
  // Draw the component, which is really just a collection of other components.

  int status = 0;
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;

  if (component->parameters.flags & WINDOW_COMPFLAG_HASBORDER)
    component->drawBorder((void *) component, 1);

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


static int resize(void *componentData, int width, int height)
{
  // Calls our internal 'layoutSize' function
  return (layoutSize(componentData, width, height));
}


static int destroy(void *componentData)
{
  int status = 0;
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindowContainer *container = (kernelWindowContainer *) component->data;

  // Release all our memory
  if (container)
    {
      if (container->components)
	kernelFree(container->components);

      kernelFree(component->data);
      component->data = NULL;
    }

  return (status = 0);
}


static int containerAdd(kernelWindowComponent *containerComponent,
			kernelWindowComponent *component,
			componentParameters *params)
{
  // Add the supplied component to the container.

  int status = 0;
  kernelWindowContainer *container = NULL;
  int maxComponents = 0;
  kernelWindowComponent **components = NULL;
  int count;
  extern color kernelDefaultForeground;
  extern color kernelDefaultBackground;

  // Check params
  if ((containerComponent == NULL) || (component == NULL) || (params == NULL))
    {
      kernelError(kernel_error, "NULL container or component");
      return (status = ERR_NULLPARAMETER);
    }
  
  container = (kernelWindowContainer *) containerComponent->data;

  // Make sure there's room for more components
  if (container->numComponents >= container->maxComponents)
    {
      // Try to make more room
      maxComponents = (container->maxComponents * 2);
      components =
	kernelMalloc(maxComponents * sizeof(kernelWindowComponent *));
      if (components == NULL)
	{
	  kernelError(kernel_error, "Component container is full");
	  return (status = ERR_MEMORY);
	}

      for (count = 0; count < container->numComponents; count ++)
	components[count] = container->components[count];

      container->maxComponents = maxComponents;
      kernelFree(container->components);
      container->components = components;
    }

  // Add it to the container
  container->components[container->numComponents++] = component;
  component->container = (void *) containerComponent;
  component->window = containerComponent->window;

  // Copy the parameters into the component
  kernelMemCopy(params, (void *) &(component->parameters),
		sizeof(componentParameters));

  // If the default colors are requested, copy them into the component
  // parameters
  if (!(component->parameters.flags & WINDOW_COMPFLAG_CUSTOMFOREGROUND))
    {
      component->parameters.foreground.blue = kernelDefaultForeground.blue;
      component->parameters.foreground.green = kernelDefaultForeground.green;
      component->parameters.foreground.red = kernelDefaultForeground.red;
    }
  if (!(component->parameters.flags & WINDOW_COMPFLAG_CUSTOMBACKGROUND))
    {
      component->parameters.background.blue = kernelDefaultBackground.blue;
      component->parameters.background.green = kernelDefaultBackground.green;
      component->parameters.background.red = kernelDefaultBackground.red;
    }

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

	component->container = NULL;
	return (status = 0);
      }

  // If we fall through, it was not found
  kernelError(kernel_error, "No such component in container");
  return (status = ERR_NOSUCHENTRY);
}


static int containerLayout(kernelWindowComponent *containerComponent)
{
  // Do layout for the container.

  int status = 0;
  kernelWindowContainer *container = NULL;
  kernelWindowComponent *component = NULL;
  int count1, count2;

  // Check params
  if (containerComponent == NULL)
    {
      kernelError(kernel_error, "NULL container for layout");
      return (status = ERR_NULLPARAMETER);
    }

  container = (kernelWindowContainer *) containerComponent->data;

  // For any components that are containers, have them do their layouts first
  // so we know their sizes.
  for (count1 = 0; count1 < container->numComponents; count1 ++)
    {
      component = container->components[count1];

      if (component->type == containerComponentType)
	{
	  status = ((kernelWindowContainer *) component->data)
	    ->containerLayout(component);
	  if (status < 0)
	    return (status);
	}
    }

  containerComponent->minWidth = 0;
  containerComponent->minHeight = 0; 

  // Call our 'layoutSize' function to do the layout, but don't specify
  // any extra size since we want 'minimum' sizes for now
  status = layoutSize(containerComponent, 0, 0);
  if (status < 0)
    return (status);

  containerComponent->minWidth = containerComponent->width;
  containerComponent->minHeight = containerComponent->height; 

  // Loop through the container's components, checking to see whether
  // each overlaps any others, and if so, decrement their levels
  for (count1 = 0; count1 < container->numComponents; count1 ++)
    {
      component = container->components[count1];

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

  return (status = 0);
}


static void containerDrawGrid(kernelWindowComponent *containerComponent)
{
  // This function draws grid boxes around all the grid cells containing
  // components (or parts thereof)

  kernelWindow *window = NULL;
  kernelWindowContainer *container = NULL;
  kernelWindowComponent *component = NULL;
  int *columnStartX = NULL;
  int *columnWidth = NULL;
  int *rowStartY = NULL;
  int *rowHeight = NULL;
  int count1, count2, count3;

  // Check params
  if (containerComponent == NULL)
    return;

  window = containerComponent->window;
  container = (kernelWindowContainer *) containerComponent->data;

  columnWidth = kernelMalloc(container->maxComponents * sizeof(int));
  columnStartX = kernelMalloc(container->maxComponents * sizeof(int));
  rowHeight = kernelMalloc(container->maxComponents * sizeof(int));
  rowStartY = kernelMalloc(container->maxComponents * sizeof(int));
  if ((columnWidth == NULL) || (columnStartX == NULL) ||
      (rowHeight == NULL) || (rowStartY == NULL))
    goto out;

  for (count1 = 0; count1 < container->numComponents; count1 ++)
    if (container->components[count1]->type == containerComponentType)
      containerDrawGrid(container->components[count1]);

  // Calculate the grid
  calculateGrid(containerComponent, columnStartX, columnWidth, rowStartY,
		rowHeight,
		(containerComponent->width - containerComponent->minWidth),
		(containerComponent->height - containerComponent->minHeight));

  for (count1 = 0; count1 < container->numComponents; count1 ++)
    {
      component = container->components[count1];

      for (count2 = 0; count2 < component->parameters.gridHeight; count2 ++)
	for (count3 = 0; count3 < component->parameters.gridWidth; count3 ++)
	  {
	    kernelGraphicDrawRect(&(window->buffer), &((color) {0,0,0}),
				  draw_normal,
				  columnStartX[component->parameters.gridX +
					       count3],
				  rowStartY[component->parameters.gridY +
					    count2],
				  columnWidth[component->parameters.gridX +
					      count3],
				  rowHeight[component->parameters.gridY +
					    count2], 1, 0);
	  }
    }

 out:
  if (columnWidth)
    kernelFree(columnWidth);
  if (columnStartX)
    kernelFree(columnStartX);
  if (rowHeight)
    kernelFree(rowHeight);
  if (rowStartY)
    kernelFree(rowStartY);

  return;
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

  // Arbitrary -- sufficient for many windows, will expand dynamically
  container->maxComponents = 64;

  container->components =
    kernelMalloc(container->maxComponents * sizeof(kernelWindowComponent *));
  if (container->components == NULL)
    {
      kernelError(kernel_error, "Can't allocate container component array");
      kernelFree((void *) component);
      kernelFree((void *) container);
      return (component = NULL);
    }

  container->containerAdd = &containerAdd;
  container->containerRemove = &containerRemove;
  container->containerLayout = &containerLayout;
  container->containerDrawGrid = &containerDrawGrid;

  component->type = containerComponentType;
  component->flags |= WINFLAG_RESIZABLE;
  component->data = (void *) container;

  // The functions
  component->draw = &draw;
  component->move = &move;
  component->resize = &resize;
  component->destroy = &destroy;
  
  return (component);
}


