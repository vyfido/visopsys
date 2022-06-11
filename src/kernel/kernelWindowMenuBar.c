//
//  Visopsys
//  Copyright (C) 1998-2007 J. Andrew McLaughlin
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

static int (*saveMenuFocus) (kernelWindowComponent *, int) = NULL;
static int (*saveMenuMouseEvent) (kernelWindowComponent *, windowEvent *)
  = NULL;

extern kernelWindowVariables *windowVariables;


static inline int menuTitleWidth(kernelWindowComponent *component, int num)
{
  kernelWindowMenuBar *menuBar = component->data;
  kernelAsciiFont *font = (kernelAsciiFont *) component->params.font;
  kernelWindowContainer *menuBarContainer = menuBar->container->data;
  kernelWindowComponent *menuComponent = menuBarContainer->components[num];
  kernelWindowMenu *menu = menuComponent->data;
  kernelWindowContainer *container = menu->container->data;
  return (kernelFontGetPrintedWidth(font, (const char *) container->name)
	  + (windowVariables->border.thickness * 2));
}


static inline int menuTitleHeight(kernelWindowComponent *component)
{
  kernelAsciiFont *font = (kernelAsciiFont *) component->params.font;
  return (font->charHeight + (windowVariables->border.thickness * 2));
}


static int menuXCoord(kernelWindowComponent *component, int num)
{
  int xCoord = 0;
  int count;

  for (count = 0; count < num; count ++)
    xCoord += menuTitleWidth(component, count);

  return (xCoord);
}


static void changedVisible(kernelWindowComponent *component)
{
  kernelDebug(debug_gui, "menuBar changed visible title");

  if (component->draw)
    component->draw(component);

  component->window
    ->update(component->window, component->xCoord, component->yCoord,
	     component->width, component->height);
}


static int menuFocus(kernelWindowComponent *component, int focus)
{
  // We just want to know when the menu has lost focus, so we can
  // un-highlight the appropriate menu bar header

  kernelWindowComponent *containerComponent = component->container;
  kernelWindowComponent *menuBarComponent = containerComponent->container;
  kernelWindowMenuBar *menuBar = menuBarComponent->data;

  kernelDebug(debug_gui, "menuBar menu %s focus", (focus? "got" : "lost"));

  if (!focus)
    {
      if (saveMenuFocus)
	// Pass the event on.
	saveMenuFocus(component, focus);

      if (component == menuBar->visibleMenu)
	{
	  // No longer visible
	  menuBar->visibleMenu = NULL;
	  changedVisible(menuBarComponent);
	}
    }

  return (0);
}


static int menuMouseEvent(kernelWindowComponent *component, windowEvent *event)
{
  kernelWindowComponent *containerComponent = component->container;
  kernelWindowComponent *menuBarComponent = containerComponent->container;
  kernelWindowMenuBar *menuBar = menuBarComponent->data;
  
  kernelDebug(debug_gui, "menuBar menu mouse event");

  if (saveMenuMouseEvent)
    // Pass the event on.
    saveMenuMouseEvent(component, event);

  // Now determine whether the menu goes away
  if ((event->type & EVENT_MOUSE_LEFTUP) &&
      (component == menuBar->visibleMenu))
    {
      // No longer visible
      menuBar->visibleMenu = NULL;
      changedVisible(menuBarComponent);
    }

  return (0);
}


static int add(kernelWindowComponent *menuBarComponent,
	       kernelWindowComponent *component)
{
  // Add the supplied component to the menu bar.

  int status = 0;
  kernelWindowMenuBar *menuBar = menuBarComponent->data;
  
  if (menuBar->container && menuBar->container->add)
    status = menuBar->container->add(menuBar->container, component);

  return (status);
}


static int numComps(kernelWindowComponent *component)
{
  int numItems = 0;
  kernelWindowMenuBar *menuBar = component->data;

  if (menuBar->container && menuBar->container->numComps)
    // Count our container's components
    numItems = menuBar->container->numComps(menuBar->container);

  return (numItems);
}


static int flatten(kernelWindowComponent *component,
		   kernelWindowComponent **array, int *numItems,
		   unsigned flags)
{
  int status = 0;
  kernelWindowMenuBar *menuBar = component->data;

  if (menuBar->container && menuBar->container->flatten)
    // Flatten our container
    status =
      menuBar->container->flatten(menuBar->container, array, numItems, flags);

  return (status);
}


static int layout(kernelWindowComponent *menuBarComponent)
{
  // Do layout for the menu bar.

  int status = 0;
  kernelWindowMenuBar *menuBar = menuBarComponent->data;
  kernelWindowContainer *container = menuBar->container->data;
  kernelWindowComponent *menuComponent = NULL;
  kernelWindowMenu *menu = NULL;
  int xCoord = 0;
  int count;

  kernelDebug(debug_gui, "menuBar layout");

  // Set the menu locations, etc.
  for (count = 0; count < container->numComponents; count ++)
    {
      menuComponent = container->components[count];

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
      if (!menuComponent->doneLayout && menuComponent->layout)
      	menuComponent->layout(menuComponent);

      xCoord +=	menuTitleWidth(menuBarComponent, count);
    }

  menuBarComponent->width = xCoord;
  menuBarComponent->minWidth = menuBarComponent->width;
  
  menuBarComponent->doneLayout = 1;

  return (status = 0);
}


static int setBuffer(kernelWindowComponent *component,
		     kernelGraphicBuffer *buffer)
{
  // Set the graphics buffer for the component's subcomponents.

  int status = 0;
  kernelWindowMenuBar *menuBar = component->data;

  if (menuBar->container && menuBar->container->setBuffer)
    {
      // Do our container
      status = menuBar->container->setBuffer(menuBar->container, buffer);
      menuBar->container->buffer = buffer;
    }

  return (status);
}


static int draw(kernelWindowComponent *component)
{
  // Draw the menu bar component 

  int status = 0;
  kernelWindowMenuBar *menuBar = component->data;
  kernelWindowContainer *menuBarContainer = menuBar->container->data;
  kernelWindowComponent *menuComponent = NULL;
  kernelWindowMenu *menu = NULL;
  kernelWindowContainer *container = NULL;
  int xCoord = 0, titleWidth = 0, titleHeight = 0;
  int count;

  kernelDebug(debug_gui, "menuBar \"%s\" draw", component->window->title);

  // Draw the background of the menu bar
  kernelGraphicDrawRect(component->buffer,
			(color *) &(component->params.background),
			draw_normal, component->xCoord, component->yCoord,
			component->width, component->height, 1, 1);

  // Loop through all the menu components and draw their names on the menu bar
  for (count = 0; count < menuBarContainer->numComponents; count ++)
    {
      menuComponent = menuBarContainer->components[count];
      menu = menuComponent->data;
      container = menu->container->data;

      xCoord = menuXCoord(component, count);
      titleWidth = menuTitleWidth(component, count);
      titleHeight = menuTitleHeight(component);

      if (menuComponent == menuBar->visibleMenu)
	{
	  kernelDebug(debug_gui, "menuBar title \"%s\" is visible",
		      container->name);
	  kernelGraphicDrawGradientBorder(component->buffer,
		  (component->xCoord + xCoord), component->yCoord, titleWidth,
		  titleHeight, windowVariables->border.thickness,
		  (color *) &(menuComponent->params.background),
		  windowVariables->border.shadingIncrement, draw_normal,
		  border_all);
	}

      kernelGraphicDrawText(component->buffer,
			    (color *) &(component->params.foreground),
			    (color *) &(component->params.background),
			    (kernelAsciiFont *) component->params.font,
			    (const char *) container->name, draw_normal,
			    (component->xCoord + xCoord +
			     windowVariables->border.thickness),
			    (component->yCoord +
			     windowVariables->border.thickness));
    }

  return (status);
}


static int resize(kernelWindowComponent *component __attribute__((unused)),
		  int width __attribute__((unused)),
		  int height __attribute__((unused)))
{
  // Nothing to do, but we want the upper layers to treat us as resizable
  // without defaulting to the regular container resize function
  return (0);
}


static int mouseEvent(kernelWindowComponent *component, windowEvent *event)
{
  kernelWindowMenuBar *menuBar = component->data;
  kernelWindowContainer *menuBarContainer = menuBar->container->data;
  kernelWindowComponent *menuComponent = NULL;
  kernelWindowMenu *menu = NULL;
  kernelWindowComponent *visibleMenu = NULL;
  int xCoord = 0, width = 0;
  int count;

  // If there are no menu components, quit here
  if (!(menuBarContainer->numComponents))
    return (0);

  // Events other than left mouse down are not interesting
  if (event->type != EVENT_MOUSE_LEFTDOWN)
    return (0);

  kernelDebug(debug_gui, "menuBar mouse event");

  // Remember any visible menu and clear it
  visibleMenu = menuBar->visibleMenu;
  menuBar->visibleMenu = NULL;

  // Determine whether to set a menu visible now by figuring out whether a
  // menu title was clicked.
  for (count = 0; count < menuBarContainer->numComponents; count ++)
    {
      menuComponent = menuBarContainer->components[count];
      menu = menuComponent->data;

      xCoord = (component->xCoord + menuXCoord(component, count));
      width = menuTitleWidth(component, count);

      if ((event->xPosition >= (component->window->xCoord + xCoord)) &&
	  (event->xPosition < (component->window->xCoord + xCoord + width)) &&
	  (menuComponent != visibleMenu))
	{
	  // The menu was not previously visible, so we will show it.
	  kernelDebug(debug_gui, "menuBar show menu");
	  menuComponent->xCoord = xCoord;
	  menuComponent->yCoord =
	    (component->yCoord + menuTitleHeight(component));
	  kernelWindowComponentSetVisible(menuComponent, 1);
	  menuBar->visibleMenu = menuComponent;
	}
    }

  if (menuBar->visibleMenu != visibleMenu)
    // We have either hidden a menu or made one visible
    changedVisible(component);

  return (0);
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

  // Get the basic component structure
  component = kernelWindowComponentNew(window->sysContainer, params);
  if (component == NULL)
    return (component);

  // Get memory for this menu bar component
  menuBar = kernelMalloc(sizeof(kernelWindowMenuBar));
  if (menuBar == NULL)
    {
      kernelFree((void *) component);
      return (component = NULL);
    }

  if (component->params.font == NULL)
    component->params.font = windowVariables->font.varWidth.small.font;

  component->type = menuBarComponentType;
  component->flags |= WINFLAG_CANFOCUS;
  // Only want this to be resizable horizontally
  component->flags &= ~WINFLAG_RESIZABLEY;
  component->data = (void *) menuBar;

  component->width = window->buffer.width;
  component->height = (((kernelAsciiFont *) component->params.font)
		       ->charHeight + (windowVariables->border.thickness * 2));
  component->minWidth = component->width;
  component->minHeight = component->height;

  component->add = &add;
  component->numComps = &numComps;
  component->flatten = &flatten;
  component->layout = &layout;
  component->setBuffer = &setBuffer;
  component->draw = &draw;
  component->resize = &resize;
  component->mouseEvent = &mouseEvent;

  // Get our container component
  menuBar->container =
    kernelWindowNewContainer(component, "menuBar", params);
  if (menuBar->container == NULL)
    {
      kernelFree((void *) component);
      kernelFree((void *) menuBar);
      return (component = NULL);
    }

  window->menuBar = component;

  return (component);
}
