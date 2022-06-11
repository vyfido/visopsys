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
//  kernelWindowMenuBar.c
//

// This code is for managing kernelWindowMenuBar objects.
// These are just images that appear inside windows and buttons, etc

#include "kernelWindow.h"     // Our prototypes are here
#include "kernelDebug.h"
#include "kernelError.h"
#include "kernelFont.h"
#include "kernelMalloc.h"

static int (*saveMenuFocus) (kernelWindowComponent *, int) = NULL;
static int (*saveMenuMouseEvent) (kernelWindowComponent *, windowEvent *)
  = NULL;
static int (*saveMenuKeyEvent) (kernelWindowComponent *, windowEvent *) = NULL;

extern kernelWindowVariables *windowVariables;


static inline int menuTitleWidth(kernelWindowComponent *component, int num)
{
  kernelWindowMenuBar *menuBar = component->data;
  asciiFont *font = (asciiFont *) component->params.font;
  kernelWindowContainer *menuBarContainer = menuBar->container->data;
  kernelWindowComponent *menuComponent = menuBarContainer->components[num];
  kernelWindowMenu *menu = menuComponent->data;
  kernelWindowContainer *container = menu->container->data;

  int width = (windowVariables->border.thickness * 2);

  if (font)
    width += kernelFontGetPrintedWidth(font, (const char *) container->name);

  return (width);
}


static inline int menuTitleHeight(kernelWindowComponent *component)
{
  asciiFont *font = (asciiFont *) component->params.font;

  int height = (windowVariables->border.thickness * 2);

  if (font)
    height += font->charHeight;

  return (height);
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

  if (saveMenuFocus)
    // Pass the event on.
    saveMenuFocus(component, focus);

  if (!focus && (component == menuBar->visibleMenu))
    // No longer visible
    changedVisible(menuBarComponent);

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


static int menuKeyEvent(kernelWindowComponent *component, windowEvent *event)
{
  kernelWindowComponent *containerComponent = component->container;
  kernelWindowComponent *menuBarComponent = containerComponent->container;
  kernelWindowMenuBar *menuBar = menuBarComponent->data;
  kernelWindowContainer *menuBarContainer = menuBar->container->data;
  int menuNumber = -1;
  kernelWindowComponent *menuComponent = NULL;
  int count;

  kernelDebug(debug_gui, "menuBar menu key event");

  if (saveMenuKeyEvent)
    // Pass the event on.
    saveMenuKeyEvent(component, event);

  // If the user has pressed the left or right cursor keys, that means they
  // want to switch menus
  if ((event->type == EVENT_KEY_DOWN) &&
      ((event->key == ASCII_CRSRLEFT) || (event->key == ASCII_CRSRRIGHT)))
    {
      if (menuBar->visibleMenu)
	{
	  for (count = 0; count < menuBarContainer->numComponents; count ++)
	    if (menuBarContainer->components[count] == menuBar->visibleMenu)
	      {
		menuNumber = count;
		break;
	      }

	  if (event->key == ASCII_CRSRLEFT)
	    {
	      // Cursor left
	      if (menuNumber > 0)
		{
		  menuNumber -= 1;
		  menuComponent = menuBarContainer->components[menuNumber];
		}
	    }
	  else
	    {
	      // Cursor right
	      if (menuNumber < (menuBarContainer->numComponents - 1))
		{
		  menuNumber += 1;
		  menuComponent = menuBarContainer->components[menuNumber];
		}
	    }

	  if (menuComponent && (menuComponent != menuBar->visibleMenu))
	    {
	      kernelDebug(debug_gui, "menuBar show new menu");

	      menuComponent->xCoord =
		(menuBarComponent->xCoord +
		 menuXCoord(menuBarComponent, menuNumber));
	      menuComponent->yCoord =
		(menuBarComponent->yCoord + menuTitleHeight(menuBarComponent));

	      if (menuBar->visibleMenu)
		kernelWindowComponentUnfocus(menuBar->visibleMenu);

	      kernelWindowComponentSetVisible(menuComponent, 1);
	      menuBar->visibleMenu = menuComponent;
	      changedVisible(menuBarComponent);
	    }
	}
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

      // If we don't have the menu component's focus() function pointer
      // saved, save it now
      if ((saveMenuFocus == NULL) && (menuComponent->focus != NULL))
	saveMenuFocus = menuComponent->focus;
      menuComponent->focus = menuFocus;

      // Likewise for the menu component's mouseEvent() function pointer
      if ((saveMenuMouseEvent == NULL) && (menuComponent->mouseEvent != NULL))
	saveMenuMouseEvent = menuComponent->mouseEvent;
      menuComponent->mouseEvent = menuMouseEvent;

      // Likewise for the menu component's keyEvent() function pointer
      if ((saveMenuKeyEvent == NULL) && (menuComponent->keyEvent != NULL))
	saveMenuKeyEvent = menuComponent->keyEvent;
      menuComponent->keyEvent = menuKeyEvent;

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

      if ((menuComponent == menuBar->visibleMenu) &&
	  (menuComponent->flags & WINFLAG_VISIBLE))
	{
	  kernelDebug(debug_gui, "menuBar title %d \"%s\" is visible", count,
		      container->name);
	  kernelGraphicDrawGradientBorder(component->buffer,
		  (component->xCoord + xCoord), component->yCoord, titleWidth,
		  titleHeight, windowVariables->border.thickness,
		  (color *) &(menuComponent->params.background),
		  windowVariables->border.shadingIncrement, draw_normal,
		  border_all);
	}

      if (component->params.font)
	kernelGraphicDrawText(component->buffer,
			      (color *) &(component->params.foreground),
			      (color *) &(component->params.background),
			      (asciiFont *) component->params.font,
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
  int xCoord = 0, width = 0;
  int count;

  // If there are no menu components, quit here
  if (!(menuBarContainer->numComponents))
    return (0);

  // Events other than left mouse down are not interesting
  if (event->type != EVENT_MOUSE_LEFTDOWN)
    return (0);

  kernelDebug(debug_gui, "menuBar mouse event");

  // Determine whether to set a menu visible now by figuring out whether a
  // menu title was clicked.
  for (count = 0; count < menuBarContainer->numComponents; count ++)
    {
      menuComponent = menuBarContainer->components[count];

      xCoord = (component->xCoord + menuXCoord(component, count));
      width = menuTitleWidth(component, count);

      if ((event->xPosition >= (component->window->xCoord + xCoord)) &&
	  (event->xPosition < (component->window->xCoord + xCoord + width)))
	{
	  if ((menuComponent != menuBar->visibleMenu) ||
	      !(menuComponent->flags & WINFLAG_VISIBLE))
	    {
	      // The menu was not previously visible, so we will show it.
	      kernelDebug(debug_gui, "menuBar show menu %d", count);

	      menuComponent->xCoord = xCoord;
	      menuComponent->yCoord =
		(component->yCoord + menuTitleHeight(component));

	      kernelWindowComponentSetVisible(menuComponent, 1);
	      menuBar->visibleMenu = menuComponent;
	    }
	  else
	    {
	      // The menu was previously visible, so we will hide it.
	      kernelDebug(debug_gui, "menuBar hide menu %d", count);
	      kernelWindowComponentUnfocus(menuComponent);
	      menuBar->visibleMenu = NULL;
	    }
	}
    }

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
  component->height = (windowVariables->border.thickness * 2);
  if (component->params.font)
    component->height += ((asciiFont *) component->params.font)->charHeight;
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
