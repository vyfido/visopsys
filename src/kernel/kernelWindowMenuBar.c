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
//  kernelWindowMenuBar.c
//

// This code is for managing kernelWindowMenuBar objects.
// These are just images that appear inside windows and buttons, etc

#include "kernelWindowManager.h"     // Our prototypes are here
#include "kernelMalloc.h"
#include "kernelError.h"
#include <string.h>
#include <sys/errors.h>

static kernelAsciiFont *menuBarFont = NULL;
static int borderThickness = 3;
static int borderShadingIncrement = 15;
static int (*saveDraw) (void *) = NULL;


static void menuSetVisible(kernelWindowComponent *component, int visible)
{
  kernelWindowMenu *menu = (kernelWindowMenu *) component->data;
  int count;

  if (visible)
    {
      component->flags |= WINFLAG_VISIBLE;
      kernelWindowComponentFocus(component);

      if (component->draw)
	component->draw((void *) component);
      
      // Set all the menu items visible
      for (count = 0; count < menu->numComponents; count ++)
	{
	  menu->components[count]->flags |= WINFLAG_VISIBLE;
	  if (menu->components[count]->draw)
	    menu->components[count]->draw((void *) menu->components[count]);
	}
    }
  else
    {
      component->flags &= ~WINFLAG_VISIBLE;

      // Set all the menu items not visible
      for (count = 0; count < menu->numComponents; count ++)
	menu->components[count]->flags &= ~WINFLAG_VISIBLE;
    }
}


static int draw(void *componentData)
{
  // Draw the menu bar component 

  int status = 0;
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindowMenuBar *menuBar = (kernelWindowMenuBar *) component->data;
  kernelWindowComponent *menuComponent = NULL;
  kernelWindowMenu *menu = NULL;
  kernelWindow *window = (kernelWindow *) component->window;
  kernelGraphicBuffer *buffer = &(window->buffer);
  color foreground = { DEFAULT_BLUE, DEFAULT_GREEN, DEFAULT_RED };
  color background = { DEFAULT_GREY, DEFAULT_GREY, DEFAULT_GREY };
  int count;

  if (!component->parameters.useDefaultForeground)
    {
      // Use user-supplied color
      foreground.red = component->parameters.foreground.red;
      foreground.green = component->parameters.foreground.green;
      foreground.blue = component->parameters.foreground.blue;
    }
  if (!component->parameters.useDefaultBackground)
    {
      // Use user-supplied color
      background.red = component->parameters.background.red;
      background.green = component->parameters.background.green;
      background.blue = component->parameters.background.blue;
    }

  // Draw the background of the menu bar
  kernelGraphicDrawRect(buffer, &background, draw_normal,
			component->xCoord, component->yCoord,
			component->width, component->height, 1, 1);

  // Loop through all the menu components and draw their names on the menu bar
  for (count = 0; count < menuBar->numComponents; count ++)
    {
      menuComponent = menuBar->components[count];
      menu = (kernelWindowMenu *) menuComponent->data;
      kernelGraphicDrawText(buffer, &foreground, &background, menuBarFont,
			    (const char *) menu->name, draw_normal,
			    (menuComponent->xCoord + borderThickness),
			    (component->yCoord + borderThickness));
    }

  // Call the container's 'draw' routine
  if (saveDraw)
    status = saveDraw(componentData);

  return (status);
}


static int mouseEvent(void *componentData, windowEvent *event)
{
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindowMenuBar *menuBar = (kernelWindowMenuBar *) component->data;
  kernelWindowComponent *menuComponent = NULL;
  kernelWindowMenuBar *menu = NULL;
  kernelWindow *window = (kernelWindow *) component->window;
  int visibleMenu = -1;
  int count;

  // If there are no menu components, quit here
  if (!(menuBar->numComponents))
    return (0);

  // The first thing is to determine whether any menu is visible
  for (count = 0; count < menuBar->numComponents; count ++)
    if (menuBar->components[count]->flags & WINFLAG_VISIBLE)
      {
	visibleMenu = count;
	break;
      }

  // Is there a visible menu?
  if (visibleMenu >= 0)
    {
      menuComponent = menuBar->components[visibleMenu];
      
      // Is this event inside it?
      int inside = isPointInside(event->xPosition, event->yPosition,
				 makeComponentScreenArea(menuComponent));
      
      if (inside && menuComponent->mouseEvent)
	// The click is inside the visible menu.  Pass the event on.
	menuComponent->mouseEvent((void *) menuComponent, event);

      if ((!inside && (event->type & EVENT_MOUSE_DOWN)) ||
	  (inside && (event->type & EVENT_MOUSE_UP)))
	{
	  // No longer visible
	  menuSetVisible(menuComponent, 0);
	  window->drawClip(component->window, component->xCoord,
			   component->yCoord, component->width,
			   component->height);
	  kernelWindowUpdateBuffer(&(window->buffer), component->xCoord,
				   component->yCoord, component->width,
				   component->height);
	  component->height =
	    (menuBarFont->charHeight + (borderThickness * 2));
	}

      // If it was inside the menu, quit here
      if (inside)
	return (0);
    }

  // Beyond this point, events other than mouse down are not interesting
  if (!(event->type & EVENT_MOUSE_DOWN))
    return (0);
	  
  // No menu is currently visible, or else the click was outside of the
  // previously visible menu.  Determine whether to set one visible now by
  // figuring out whether a menu title was clicked.
  for (count = 0; count < menuBar->numComponents; count ++)
    {
      if (count == visibleMenu)
	continue;

      menuComponent = menuBar->components[count];
      menu = (kernelWindowMenu *) menuComponent->data;
	      
      unsigned tmpWidth =
	kernelFontGetPrintedWidth(menuBarFont, (const char *) menu->name);
      
      if ((event->yPosition < (window->yCoord + menuComponent->yCoord)) &&
	  (event->xPosition >= (window->xCoord + menuComponent->xCoord)) &&
	  (event->xPosition <
	   (window->xCoord + menuComponent->xCoord + tmpWidth)))
	{
	  // The user clicked the title of a menu.  Set it visible
	  kernelGraphicDrawGradientBorder(&(window->buffer),
		  menuComponent->xCoord, component->yCoord,
		  (kernelFontGetPrintedWidth(menuBarFont,
		     (const char *) menu->name) + (borderThickness * 2)),
		  (menuBarFont->charHeight + (borderThickness * 2)),
		  borderThickness, borderShadingIncrement, draw_normal);

	  menuSetVisible(menuComponent, 1);

	  kernelWindowUpdateBuffer(&(window->buffer), component->xCoord,
				   component->yCoord, component->width,
				   (component->height +
				    menuComponent->height));
	  
	  // Adjust our component size to catch events that happen to our
	  // visible menu
	  component->height =
	    ((menuBarFont->charHeight + (borderThickness * 2)) +
	     menuComponent->height);

	  return (0);
	}
    }

  return (0);
}


static int containerLayout(kernelWindowComponent *menuBarComponent)
{
  // Do layout for the menu bar.

  int status = 0;
  kernelWindowMenuBar *menuBar = NULL;
  kernelWindowComponent *menuComponent = NULL;
  kernelWindowMenu *menu = NULL;
  int xCoord = 0;
  int count;

  menuBar = (kernelWindowMenuBar *) menuBarComponent->data;

  // Set the menu locations, etc.
  for (count = 0; count < menuBar->numComponents; count ++)
    {
      menuComponent = menuBar->components[count];

      // Make sure it's a menu (container) component
      if (menuComponent->type != containerComponentType)
	{
	  kernelError(kernel_error, "Window component is not a menu!");
	  return (status = ERR_INVALID);
	}

      menu = (kernelWindowMenu *) menuComponent->data;

      menuComponent->flags &= ~WINFLAG_VISIBLE;
      menuComponent->xCoord = (menuBarComponent->xCoord + xCoord);
      menuComponent->yCoord = (menuBarComponent->yCoord +
			       menuBarComponent->height);

      if (menu->containerLayout)
	menu->containerLayout(menuComponent);

      int tmpWidth =
	(kernelFontGetPrintedWidth(menuBarFont, (const char *) menu->name) +
	 (borderThickness * 2));
      xCoord +=	tmpWidth;

      menuBarComponent->width += tmpWidth;
      if (count == (menuBar->numComponents - 1))
	// Add the width of the rightmost menu, since it will extend past the
	// edge of the menu bar
	menuBarComponent->width += (menuComponent->width - tmpWidth);
    }

  // Add the width of a border
  menuBarComponent->width += (borderThickness * 2);
  
  menuBar->doneLayout = 1;

  return (status = 0);
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


kernelWindowComponent *kernelWindowNewMenuBar(volatile void *parent,
					      componentParameters *params)
{
  // Formats a kernelWindowComponent as a kernelWindowMenuBar

  kernelWindowComponent *component = NULL;

  // Check parameters
  if ((parent == NULL) || (params == NULL))
    return (component = NULL);

  if (menuBarFont == NULL)
    {
      // Try to load a nice-looking font
      if (kernelFontLoad(DEFAULT_VARIABLEFONT_SMALL_FILE,
			 DEFAULT_VARIABLEFONT_SMALL_NAME, &menuBarFont) < 0)
	// Font's not there, we suppose.  There's always a default.
	kernelFontGetDefault(&menuBarFont);
    }

  // Get the superclass container component
  component = kernelWindowNewContainer(parent, "menuBar", params);
  if (component == NULL)
    return (component);

  component->height = (menuBarFont->charHeight + (borderThickness * 2));

  // Save the old draw function, and superimpose our own
  saveDraw = component->draw;
  component->draw = &draw;
  component->mouseEvent = &mouseEvent;

  // Override the layout function
  ((kernelWindowContainer *) component->data)->containerLayout =
    &containerLayout;

  return (component);
}