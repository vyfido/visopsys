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

#include "kernelWindow.h"     // Our prototypes are here
#include "kernelWindowEventStream.h"
#include "kernelMalloc.h"
#include "kernelError.h"
#include <string.h>
#include <sys/errors.h>

static kernelAsciiFont *menuBarFont = NULL;
static int borderThickness = 3;
static int borderShadingIncrement = 15;
static int (*saveMenuMouseEvent) (void *, windowEvent *) = NULL;
static int (*saveContainerDraw) (void *) = NULL;


static void menuSetVisible(kernelWindowComponent *menuBarComponent,
			   kernelWindowComponent *menuComponent, int visible)
{
  kernelWindow *window = (kernelWindow *) menuBarComponent->window;
  kernelWindowMenu *menu = (kernelWindowMenu *) menuComponent->data;

  if (visible)
    {
      // Focus *before* drawing, so that the menu will appear on top of any
      // components it covers
      kernelWindowComponentFocus(menuComponent);
      
      // Make our menu bar temporarily focused, so that we'll know when we
      // lose it
      menuBarComponent->flags |= WINFLAG_CANFOCUS;
      kernelWindowComponentFocus(menuBarComponent);
    }

  kernelWindowComponentSetVisible(menuComponent, visible);

  if (visible)
    {
      kernelGraphicDrawGradientBorder(&(window->buffer),
	      menuComponent->xCoord, menuBarComponent->yCoord,
	      (kernelFontGetPrintedWidth(menuBarComponent->parameters.font,
		 (const char *) menu->name) + (borderThickness * 2)),
	      (((kernelAsciiFont *) menuBarComponent->parameters.font)
	       ->charHeight + (borderThickness * 2)),
	      borderThickness,
	      (color *) &(menuComponent->parameters.background),
	      borderShadingIncrement, draw_normal);
      kernelWindowUpdateBuffer(&(window->buffer), menuBarComponent->xCoord,
			       menuBarComponent->yCoord,
			       menuBarComponent->width,
			       menuBarComponent->height);
    }
  else
    {
      // Not normally focusable
      menuBarComponent->flags &= ~WINFLAG_CANFOCUS;

      window->drawClip((void *) window, menuBarComponent->xCoord,
		       menuBarComponent->yCoord, menuBarComponent->width,
		       menuBarComponent->height);
      kernelMouseDraw();
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
  int count;

  // Draw the background of the menu bar
  kernelGraphicDrawRect(buffer, (color *) &(component->parameters.background),
			draw_normal, component->xCoord, component->yCoord,
			component->width, component->height, 1, 1);

  // Loop through all the menu components and draw their names on the menu bar
  for (count = 0; count < menuBar->numComponents; count ++)
    {
      menuComponent = menuBar->components[count];
      menu = (kernelWindowMenu *) menuComponent->data;
      kernelGraphicDrawText(buffer,
			    (color *) &(component->parameters.foreground),
			    (color *) &(component->parameters.background),
			    component->parameters.font,
			    (const char *) menu->name, draw_normal,
			    (menuComponent->xCoord + borderThickness),
			    (component->yCoord + borderThickness));
    }

  // Call the container's 'draw' routine
  if (saveContainerDraw)
    status = saveContainerDraw(componentData);

  return (status);
}


static int focus(void *componentData, int focus)
{
  // We just want to know when we've lost the focus, so we can make the
  // menu disappear
  
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindowMenuBar *menuBar = (kernelWindowMenuBar *) component->data;
  int count;

  if (!focus)
    {
      // The first thing is to determine whether any menu is visible
      for (count = 0; count < menuBar->numComponents; count ++)
	if (menuBar->components[count]->flags & WINFLAG_VISIBLE)
	  menuSetVisible(componentData, menuBar->components[count], 0);
    }

  return (0);
}


static int menuMouseEvent(void *componentData, windowEvent *event)
{
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;

  if (saveMenuMouseEvent)
    // Pass the event on.
    saveMenuMouseEvent(componentData, event);

  // Now determine whether the menu goes away
  if (event->type == EVENT_MOUSE_LEFTUP)
    // No longer visible
    menuSetVisible(component->container, component, 0);

  return (0);
}


static int mouseEvent(void *componentData, windowEvent *event)
{
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindowMenuBar *menuBar = (kernelWindowMenuBar *) component->data;
  kernelWindowComponent *menuComponent = NULL;
  kernelWindowMenu *menu = NULL;
  kernelWindow *window = (kernelWindow *) component->window;
  int visibleMenu = -1;
  int count;

  // If there are no menu components, quit here
  if (!(menuBar->numComponents))
    return (0);

  // The first thing is to determine whether any menu is visible
  for (count = 0; count < menuBar->numComponents; count ++)
    {
      menuComponent = menuBar->components[count];

      if (menuComponent->flags & WINFLAG_VISIBLE)
	{
	  if (event->type == EVENT_MOUSE_LEFTDOWN)
	    // No longer visible
	    menuSetVisible(component, menuComponent, 0);

	  visibleMenu = count;
	  break;
	}
    }

  // Beyond this point, events other than mouse down are not interesting
  if (event->type != EVENT_MOUSE_LEFTDOWN)
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
	      
      unsigned tmpWidth = kernelFontGetPrintedWidth(component->parameters.font,
						    (const char *) menu->name);
      
      if ((event->yPosition < (window->yCoord + menuComponent->yCoord)) &&
	  (event->xPosition >= (window->xCoord + menuComponent->xCoord)) &&
	  (event->xPosition <
	   (window->xCoord + menuComponent->xCoord + tmpWidth)))
	{
	  menuSetVisible(component, menuComponent, 1);
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

      menuComponent->flags &= ~WINFLAG_VISIBLE;
      menuComponent->xCoord = (menuBarComponent->xCoord + xCoord);
      menuComponent->yCoord = (menuBarComponent->yCoord +
			       menuBarComponent->height);

      menu = (kernelWindowMenu *) menuComponent->data;

      // If we don't have the menu component's mouseEvent function pointer
      // saved, save it now
      if ((saveMenuMouseEvent == NULL) && (menuComponent->mouseEvent != NULL))
	saveMenuMouseEvent = menuComponent->mouseEvent;

      // See if we need to wrap this menu's mouseEvent function
      if (menuComponent->mouseEvent != menuMouseEvent)
	menuComponent->mouseEvent = menuMouseEvent;

      if (!menu->doneLayout && menu->containerLayout)
      	menu->containerLayout(menuComponent);

      int tmpWidth =
	(kernelFontGetPrintedWidth(menuBarComponent->parameters.font,
				   (const char *) menu->name) +
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
  kernelWindowMenuBar *menuBar = NULL;

  // Check parameters
  if ((parent == NULL) || (params == NULL))
    return (component = NULL);

  if (menuBarFont == NULL)
    {
      // Try to load a nice-looking font
      if (kernelFontLoad(DEFAULT_VARIABLEFONT_SMALL_FILE,
			 DEFAULT_VARIABLEFONT_SMALL_NAME, &menuBarFont, 0) < 0)
	// Font's not there, we suppose.  There's always a default.
	kernelFontGetDefault(&menuBarFont);
    }

  // Get the superclass container component
  component = kernelWindowNewContainer(parent, "menuBar", params);
  if (component == NULL)
    return (component);

  if (component->parameters.font == NULL)
    component->parameters.font = menuBarFont;

  component->width = (getWindow(parent))->buffer.width;
  component->height = (((kernelAsciiFont *) component->parameters.font)
		       ->charHeight + (borderThickness * 2));

  // Save the old draw function, and superimpose our own
  saveContainerDraw = component->draw;
  component->draw = &draw;
  component->focus = &focus;
  component->mouseEvent = &mouseEvent;

  menuBar = (kernelWindowMenuBar *) component->data;

  // Override the layout function
  menuBar->containerLayout = &containerLayout;

  return (component);
}
