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
//  kernelWindowSysContainer.c
//

// This code is for managing kernelWindowSysContainer objects.  These are
// containers for just the 'system' components of a window.

#include "kernelWindow.h"     // Our prototypes are here
#include "kernelDebug.h"
#include <string.h>


static int containerLayout(kernelWindowComponent *containerComponent)
{
  // Do layout for the system container.  This involves setting the default
  // locations and sizes of the standard system components such as borders,
  // title bars, and menus.

  int status = 0;
  kernelWindowContainer *container = containerComponent->data;
  kernelWindow *window = containerComponent->window;
  kernelWindowComponent *tmpComponent = NULL;
  kernelWindowContainer *tmpContainer = NULL;
  kernelWindowBorder *border = NULL;
  int clientAreaX = 0;
  int clientAreaY = 0;
  int count = 0;

  // Does the window have a border?
  if (window->flags & WINFLAG_HASBORDER)
    {
      for (count = 0; count < 4; count ++)
	{
	  tmpComponent = window->borders[count];
	  border = tmpComponent->data;

	  if (border->type == border_top)
	    {
	      tmpComponent->xCoord = 0;
	      tmpComponent->yCoord = 0;
	      tmpComponent->width = window->buffer.width;
	      tmpComponent->height = WINDOW_BORDER_THICKNESS;
	    }
	  else if (border->type == border_bottom)
	    {
	      tmpComponent->xCoord = 0;
	      tmpComponent->yCoord = 
		(window->buffer.height - WINDOW_BORDER_THICKNESS + 1);
	      tmpComponent->width = window->buffer.width;
	      tmpComponent->height = WINDOW_BORDER_THICKNESS;
	    }
	  else if (border->type == border_left)
	    {
	      tmpComponent->xCoord = 0;
	      tmpComponent->yCoord = 0;
	      tmpComponent->width = WINDOW_BORDER_THICKNESS;
	      tmpComponent->height = window->buffer.height;
	    }
	  else if (border->type == border_right)
	    {
	      tmpComponent->xCoord =
		(window->buffer.width - WINDOW_BORDER_THICKNESS + 1);
	      tmpComponent->yCoord = 0;
	      tmpComponent->width = WINDOW_BORDER_THICKNESS;
	      tmpComponent->height = window->buffer.height;
	    }

	  tmpComponent->minWidth = tmpComponent->width;
	  tmpComponent->minHeight = tmpComponent->height;
	}

      clientAreaX += WINDOW_BORDER_THICKNESS;
      clientAreaY += WINDOW_BORDER_THICKNESS;
    }

  // Does the window have a title bar?
  if (window->titleBar)
    {
      // Resize the title bar
      if (window->titleBar->resize)
	window->titleBar->resize(window->titleBar,
				 (window->buffer.width - (clientAreaX * 2)),
				 window->titleBar->height);

      window->titleBar->width = (window->buffer.width - (clientAreaX * 2));

      // Move the title bar
      if (window->titleBar->move)
	window->titleBar->move(window->titleBar, clientAreaX, clientAreaY);

      window->titleBar->xCoord = clientAreaX;
      window->titleBar->yCoord = clientAreaY;

      clientAreaY += window->titleBar->height;
    }

  // Does the window have a menu bar?
  if (window->menuBar)
    {
      kernelDebug(debug_gui, "sysContainer layout: do menu bar");

      tmpContainer = window->menuBar->data;

      // Do menu bar layout
      if (tmpContainer->containerLayout)
	tmpContainer->containerLayout(window->menuBar);

      kernelDebug(debug_gui, "sysContainer layout: resize menu bar");

      // Resize the menu bar
      if (window->menuBar->resize)
	window->menuBar->resize(window->menuBar,
				(window->buffer.width - (clientAreaX * 2)),
				window->menuBar->height);

      window->menuBar->width = (window->buffer.width - (clientAreaX * 2));

      kernelDebug(debug_gui, "sysContainer layout: move menu bar");
      // Move the menu bar
      if (window->menuBar->move)
	window->menuBar->move(window->menuBar, clientAreaX, clientAreaY);

      window->menuBar->xCoord = clientAreaX;
      window->menuBar->yCoord = clientAreaY;

      clientAreaY += window->menuBar->height;
    }

  if (window->mainContainer)
    {
      kernelDebug(debug_gui, "sysContainer layout: move main container");
      // Move the window's main container
      if (window->mainContainer->move)
	window->mainContainer->move(window->mainContainer, clientAreaX,
				    clientAreaY);

      window->mainContainer->xCoord = clientAreaX;
      window->mainContainer->yCoord = clientAreaY;
    }

  containerComponent->xCoord = 0;
  containerComponent->yCoord = 0;
  containerComponent->width = window->buffer.width;
  containerComponent->height = clientAreaY;

  // Set the flag to indicate layout complete
  container->doneLayout = 1;

  return (status = 0);
}


static int resize(kernelWindowComponent *component, int width, int height)
{
  // Just redo the layout, setting the new size first (normally it gets done
  // after this call returns)

  component->width = width;
  component->height = height;

  return (containerLayout(component));
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


kernelWindowComponent *kernelWindowNewSysContainer(kernelWindow *window,
						   componentParameters *params)
{
  // Formats a kernelWindowContainer as a kernelWindowSysContainer

  kernelWindowComponent *component = NULL;
  kernelWindowContainer *container = NULL;

  // Check parameters.
  if ((window == NULL) || (params == NULL))
    return (component = NULL);

  // Get the underlying kernelWindowContainer

  // Get the basic component structure
  component = kernelWindowNewContainer(window, "sysContainer", params);
  if (component == NULL)
    return (component);

  // Now populate the component

  component->subType = sysContainerComponentType;

  component->resize = &resize;

  container = component->data;

  container->containerLayout = &containerLayout;
  
  return (component);
}


