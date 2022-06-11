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
//  kernelWindowMenuBar.c
//

// This code is for managing kernelWindowMenuBar objects.
// These are just images that appear inside windows and buttons, etc

#include "kernelWindow.h"     // Our prototypes are here
#include "kernelDebug.h"
#include "kernelError.h"
#include "kernelMalloc.h"
#include "kernelWindowEventStream.h"
#include <string.h>

static kernelAsciiFont *menuBarFont = NULL;
static int borderThickness = 3;
static int borderShadingIncrement = 15;
static int (*saveMenuFocus) (kernelWindowComponent *, int) = NULL;
static int (*saveMenuMouseEvent) (kernelWindowComponent *, windowEvent *)
     = NULL;


static inline int menuTitleWidth(kernelWindowComponent *component, int num)
{
  kernelWindowMenuBar *menuBar = component->data;
  kernelAsciiFont *font = (kernelAsciiFont *) component->parameters.font;
  kernelWindowComponent *menuComponent = menuBar->components[num];
  kernelWindowMenu *menu = menuComponent->data;
  kernelWindowContainer *container = menu->container->data;
  return (kernelFontGetPrintedWidth(font, (const char *) container->name)
	 + (borderThickness * 2));
}


static inline int menuTitleHeight(kernelWindowComponent *component)
{
  kernelAsciiFont *font = (kernelAsciiFont *) component->parameters.font;
  return (font->charHeight + (borderThickness * 2));
}


static int menuXCoord(kernelWindowComponent *component, int num)
{
  int xCoord = 0;
  int count;

  for (count = 0; count < num; count ++)
    xCoord += menuTitleWidth(component, count);

  return (xCoord);
}


static int draw(kernelWindowComponent *component)
{
  // Draw the menu bar component 

  int status = 0;
  kernelWindowMenuBar *menuBar = component->data;
  kernelWindowComponent *menuComponent = NULL;
  kernelWindowMenu *menu = NULL;
  kernelWindowContainer *container = NULL;
  int xCoord = 0, titleWidth = 0, titleHeight = 0;
  int count;

  kernelDebug(debug_gui, "menuBar \"%s\" draw", component->window->title);

  // Draw the background of the menu bar
  kernelGraphicDrawRect(component->buffer,
			(color *) &(component->parameters.background),
			draw_normal, component->xCoord, component->yCoord,
			component->width, component->height, 1, 1);

  // Loop through all the menu components and draw their names on the menu bar
  for (count = 0; count < menuBar->numComponents; count ++)
    {
      menuComponent = menuBar->components[count];
      menu = menuComponent->data;
      container = menu->container->data;

      xCoord = menuXCoord(component, count);
      titleWidth = menuTitleWidth(component, count);
      titleHeight = menuTitleHeight(component);

      if (menuComponent->flags & WINFLAG_VISIBLE)
	{
	  kernelDebug(debug_gui, "menuBar title \"%s\" is visible",
		      container->name);
	  kernelGraphicDrawGradientBorder(component->buffer,
		  (component->xCoord + xCoord), component->yCoord, titleWidth,
		  titleHeight, borderThickness,
		  (color *) &(menuComponent->parameters.background),
		  borderShadingIncrement, draw_normal, border_all);
	}

      kernelGraphicDrawText(component->buffer,
			    (color *) &(component->parameters.foreground),
			    (color *) &(component->parameters.background),
			    (kernelAsciiFont *) component->parameters.font,
			    (const char *) container->name, draw_normal,
			    (component->xCoord + xCoord + borderThickness),
			    (component->yCoord + borderThickness));
    }

  return (status);
}


static void changedVisible(kernelWindowComponent *component)
{
  kernelDebug(debug_gui, "menuBar changed visible title");

  draw(component);
  component->window
    ->update(component->window, component->xCoord, component->yCoord,
	     component->width, component->height);
}


static int resize(kernelWindowComponent *component __attribute__((unused)),
		  int width __attribute__((unused)),
		  int height __attribute__((unused)))
{
  // Nothing to do, but we want the upper layers to treat us as resizable
  // without defaulting to the regular container resize function
  return (0);
}


static int menuFocus(kernelWindowComponent *component, int focus)
{
  // We just want to know when the menu has lost focus, so we can
  // un-highlight the appropriate menu bar header

  kernelWindowMenu *menu = component->data;
  
  kernelDebug(debug_gui, "menuBar menu %s focus", (focus? "got" : "lost"));

  if (!focus)
    {
      if (saveMenuFocus)
	// Pass the event on.
	saveMenuFocus(component, focus);

      if (menu->menuBarSelected)
	changedVisible(component->container);
    }

  return (0);
}


static int menuMouseEvent(kernelWindowComponent *component, windowEvent *event)
{
  kernelWindowMenu *menu = component->data;
  
  kernelDebug(debug_gui, "menuBar menu mouse event");

  if (saveMenuMouseEvent)
    // Pass the event on.
    saveMenuMouseEvent(component, event);

  // Now determine whether the menu goes away
  if ((event->type & EVENT_MOUSE_LEFTUP) && menu->menuBarSelected)
    // No longer visible
    changedVisible(component->container);

  return (0);
}


static int mouseEvent(kernelWindowComponent *component, windowEvent *event)
{
  kernelWindowMenuBar *menuBar = component->data;
  kernelWindowComponent *menuComponent = NULL;
  kernelWindowMenu *menu = NULL;
  kernelWindowContainer *container = NULL;
  int xCoord = 0, width = 0;
  int count;

  // If there are no menu components, quit here
  if (!(menuBar->numComponents))
    return (0);

  // Events other than left mouse down are not interesting
  if (event->type != EVENT_MOUSE_LEFTDOWN)
    return (0);

  kernelDebug(debug_gui, "menuBar mouse event");

  // Hide any previously-visible menu
  for (count = 0; count < menuBar->numComponents; count ++)
    {
      menuComponent = menuBar->components[count];
      menu = menuComponent->data;
      container = menu->container->data;
	
      if (menu->menuBarSelected)
	{
	  // The menu was previously visible, so we hide it.
	  kernelDebug(debug_gui, "menuBar hide menu %s", container->name);
	  if (menuComponent->flags & WINFLAG_HASFOCUS)
	    kernelWindowComponentUnfocus(menuComponent);
	  changedVisible(component);
	  menu->menuBarSelected = 0;
	}
    }

  // Determine whether to set a menu visible now by figuring out whether a
  // menu title was clicked.
  for (count = 0; count < menuBar->numComponents; count ++)
    {
      menuComponent = menuBar->components[count];
      menu = menuComponent->data;
      container = menu->container->data;

      xCoord = (component->xCoord + menuXCoord(component, count));
      width = menuTitleWidth(component, count);

      if ((event->xPosition >= (component->window->xCoord + xCoord)) &&
	  (event->xPosition < (component->window->xCoord + xCoord + width)))
	{
	  // The menu was not previously visible, so we show it.
	  kernelDebug(debug_gui, "menuBar show menu %s", container->name);
	  menuComponent->xCoord = xCoord;
	  menuComponent->yCoord =
	    (component->yCoord + menuTitleHeight(component));
	  kernelWindowComponentSetVisible(menuComponent, 1);
	  changedVisible(component);
	  menu->menuBarSelected = 1;
	}
    }

  return (0);
}


static int containerLayout(kernelWindowComponent *menuBarComponent)
{
  // Do layout for the menu bar.

  int status = 0;
  kernelWindowMenuBar *menuBar = menuBarComponent->data;
  kernelWindowComponent *menuComponent = NULL;
  kernelWindowMenu *menu = NULL;
  kernelWindowContainer *container = NULL;
  int xCoord = 0;
  int count;

  kernelDebug(debug_gui, "menuBar layout");

  // Set the menu locations, etc.
  for (count = 0; count < menuBar->numComponents; count ++)
    {
      menuComponent = menuBar->components[count];

      // Make sure it's a menu (container) component
      if (menuComponent->type != menuComponentType)
	{
	  kernelError(kernel_error, "Menu component is not a menu!");
	  return (status = ERR_INVALID);
	}

      menuComponent->flags &= ~WINFLAG_VISIBLE;

      if (menuComponent->move)
	menuComponent
	  ->move(menuComponent, (menuBarComponent->xCoord + xCoord),
		 (menuBarComponent->yCoord + menuBarComponent->height));

      menuComponent->xCoord = (menuBarComponent->xCoord + xCoord);
      menuComponent->yCoord =
	(menuBarComponent->yCoord + menuBarComponent->height);

      menu = menuComponent->data;
      container = menu->container->data;

      // If we don't have the menu component's mouseEvent() function pointer
      // saved, save it now
      if ((saveMenuMouseEvent == NULL) && (menuComponent->mouseEvent != NULL))
	saveMenuMouseEvent = menuComponent->mouseEvent;
      menuComponent->mouseEvent = menuMouseEvent;

      // Likewise for the menu component's focus() function pointer
      if ((saveMenuFocus == NULL) && (menuComponent->focus != NULL))
	saveMenuFocus = menuComponent->focus;
      menuComponent->focus = menuFocus;

      // Do the layout for the menu itself.
      if (!container->doneLayout && container->containerLayout)
      	container->containerLayout(menu->container);

      xCoord +=	menuTitleWidth(menuBarComponent, count);
    }

  menuBarComponent->width = xCoord;
  menuBarComponent->minWidth = menuBarComponent->width;
  
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


kernelWindowComponent *kernelWindowNewMenuBar(kernelWindow *window,
					      componentParameters *params)
{
  // Formats a kernelWindowComponent as a kernelWindowMenuBar

  kernelWindowComponent *component = NULL;
  kernelWindowMenuBar *menuBar = NULL;

  // Check parameters
  if ((window == NULL) || (params == NULL))
    return (component = NULL);

  if (window->type != windowType)
    {
      kernelError(kernel_error, "Menu bars can only be added to windows");
      return (component = NULL);
    }

  if (menuBarFont == NULL)
    {
      // Try to load a nice-looking font
      if (kernelFontLoad(WINDOW_DEFAULT_VARFONT_SMALL_FILE,
			 WINDOW_DEFAULT_VARFONT_SMALL_NAME,
			 &menuBarFont, 0) < 0)
	// Font's not there, we suppose.  There's always a default.
	kernelFontGetDefault(&menuBarFont);
    }

  // Get the superclass container component
  component =
    kernelWindowNewContainer(window->sysContainer, "menuBar", params);
  if (component == NULL)
    return (component);

  component->subType = menuBarComponentType;

  if (component->parameters.font == NULL)
    component->parameters.font = menuBarFont;

  component->width = window->buffer.width;
  component->height = (((kernelAsciiFont *) component->parameters.font)
		       ->charHeight + (borderThickness * 2));
  component->minWidth = component->width;
  component->minHeight = component->height;

  component->flags |= WINFLAG_CANFOCUS;
  // Only want this to be resizable horizontally
  component->flags &= ~WINFLAG_RESIZABLEY;

  component->draw = &draw;
  component->resize = &resize;
  component->mouseEvent = &mouseEvent;

  menuBar = component->data;

  // Override the layout function
  menuBar->containerLayout = &containerLayout;

  window->menuBar = component;

  return (component);
}
