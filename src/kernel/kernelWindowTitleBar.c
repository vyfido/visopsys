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
//  kernelWindowTitleBar.c
//

// This code is for managing kernelWindowTitleBar objects.

#include "kernelWindow.h"     // Our prototypes are here
#include "kernelWindowEventStream.h"
#include "kernelMalloc.h"
#include "kernelMemoryManager.h"
#include "kernelMultitasker.h"
#include "kernelMiscFunctions.h"
#include <string.h>

static kernelAsciiFont *titleBarFont = NULL;
static image minimizeImage;
static image closeImage;
static int imagesCreated = 0;


static int isMouseInButton(windowEvent *event, kernelWindowComponent *button)
{
  // We use this to determine whether a mouse event is inside one of our
  // buttons

  kernelWindow *window = (kernelWindow *) button->window;

  if (((event->xPosition >= (window->xCoord + button->xCoord)) &&
       (event->xPosition < (window->xCoord + button->xCoord +
			    button->width)) &&
       ((event->yPosition >= (window->yCoord + button->yCoord)) &&
	(event->yPosition < (window->yCoord + button->yCoord +
				   button->height)))))
    return (1);
  else
    return (0);
}


static void createImages(int width, int height)
{
  // Create some standard, shared images for close buttons, etc.
  
  kernelGraphicBuffer graphicBuffer;
  extern color kernelDefaultBackground;

  kernelMemClear((void *) &graphicBuffer, sizeof(kernelGraphicBuffer));

  // Get a buffer to draw our close button graphic
  graphicBuffer.width = width;
  graphicBuffer.height = height;
  graphicBuffer.data =
    kernelMalloc(kernelGraphicCalculateAreaBytes(graphicBuffer.width,
						 graphicBuffer.height));
  if (graphicBuffer.data == NULL)
    return;

  // Do the minimize button
  kernelMemClear(&minimizeImage, sizeof(image));
  kernelGraphicClearArea(&graphicBuffer, &kernelDefaultBackground,
			 0, 0, graphicBuffer.width,
			 graphicBuffer.height);
  kernelGraphicDrawRect(&graphicBuffer, &((color){0,0,0}), draw_normal,
			((graphicBuffer.width - 4) / 2),
			((graphicBuffer.height - 4) / 2), 4, 4, 1, 0);
  kernelGraphicGetKernelImage(&graphicBuffer, &minimizeImage, 0, 0,
			      graphicBuffer.width, graphicBuffer.height);

  // Do the close button
  kernelMemClear(&closeImage, sizeof(image));
  kernelGraphicClearArea(&graphicBuffer, &kernelDefaultBackground,
			 0, 0, graphicBuffer.width,
			 graphicBuffer.height);
  kernelGraphicDrawLine(&graphicBuffer, &((color){0,0,0}), draw_normal,
			0, 0, (graphicBuffer.width - 1),
			(graphicBuffer.height - 1));
  kernelGraphicDrawLine(&graphicBuffer, &((color){0,0,0}), draw_normal,
			1, 0, (graphicBuffer.width - 1),
			(graphicBuffer.height - 2));
  kernelGraphicDrawLine(&graphicBuffer, &((color){0,0,0}), draw_normal,
			0, 1, (graphicBuffer.width - 2),
			(graphicBuffer.height - 1));
  kernelGraphicDrawLine(&graphicBuffer, &((color){0,0,0}), draw_normal,
			0, (graphicBuffer.width - 1),
			(graphicBuffer.height - 1), 0);
  kernelGraphicDrawLine(&graphicBuffer, &((color){0,0,0}), draw_normal,
			0, (graphicBuffer.width - 2),
			(graphicBuffer.height - 2), 0);
  kernelGraphicDrawLine(&graphicBuffer, &((color){0,0,0}), draw_normal,
			1, (graphicBuffer.width - 1),
			(graphicBuffer.height - 1), 1);
  kernelGraphicGetKernelImage(&graphicBuffer, &closeImage, 0, 0,
			      graphicBuffer.width, graphicBuffer.height);

  kernelFree(graphicBuffer.data);
  graphicBuffer.data = NULL;

  imagesCreated = 1;
}


static void minimizeWindow(objectKey componentData, windowEvent *event)
{
  // This function gets called when the minimize button gets pushed

  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindow *window = (kernelWindow *) component->window;

  if (event->type == EVENT_MOUSE_LEFTUP)
    {
      kernelWindowSetMinimized(window, 1);
      
      // Transfer this event into the window's event stream
      event->type = EVENT_WINDOW_MINIMIZE;
      kernelWindowEventStreamWrite(&(window->events), event);
    }
  
  return;
}


static void closeWindow(objectKey componentData, windowEvent *event)
{
  // This function gets called when the close button gets pushed

  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindow *window = (kernelWindow *) component->window;

  if (event->type == EVENT_MOUSE_LEFTUP)
    {
      // Transfer this event into the window's event stream
      event->type = EVENT_WINDOW_CLOSE;
      kernelWindowEventStreamWrite(&(window->events), event);
    }

  return;
}


static int draw(void *componentData)
{
  // Draw the title bar component atop the window

  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindowTitleBar *titleBarComponent =
    (kernelWindowTitleBar *) component->data;
  kernelWindow *window = (kernelWindow *) component->window;
  kernelGraphicBuffer *buffer = &(window->buffer);
  int titleWidth = 0;
  char title[128];
  color foregroundColor;
  color backgroundColor;
  int count;

  // The color will be different depending on whether the window has
  // the focus
  if (window->flags & WINFLAG_HASFOCUS)
    {
      if (component->parameters.useDefaultBackground)
	{
	  // Use default color blue
	  backgroundColor.red = 0;
	  backgroundColor.green = 0;
	  backgroundColor.blue = 200;
	}
      else
	{
	  // Use user-supplied colors
	  backgroundColor.red = component->parameters.background.red;
	  backgroundColor.green = component->parameters.background.green;
	  backgroundColor.blue = component->parameters.background.blue;
	}
    }
  else
    {
      backgroundColor.red = 0;
      backgroundColor.green = 0;
      backgroundColor.blue = 150;
    }

  // We draw it inside the border as a series of lines.  It starts as a
  // darker blue and lightens in color
  for (count = 0; count < component->height; count ++)
    {
      kernelGraphicDrawLine(buffer, &backgroundColor, draw_normal,
			    component->xCoord, (component->yCoord + count),
			    (component->xCoord + component->width - 1),
			    (component->yCoord + count));
      if (backgroundColor.red > 0)
	backgroundColor.red -= 5;
      if (backgroundColor.green > 0)
	backgroundColor.green -= 5;
      if (backgroundColor.blue > 0)
	backgroundColor.blue -= 5;
    }

  // Put the title on the title bar
  
  if (component->parameters.useDefaultForeground)
    {
      // Use default color white
      foregroundColor.red = 255;
      foregroundColor.green = 255;
      foregroundColor.blue = 255;
    }
  else
    {
      // Use user-supplied colors
      foregroundColor.red = component->parameters.foreground.red;
      foregroundColor.green = component->parameters.foreground.green;
      foregroundColor.blue = component->parameters.foreground.blue;
    }

  strncpy(title, (char *) window->title, 128);
  titleWidth = (component->width - 1);
  if ((window->flags & WINFLAG_HASMINIMIZEBUTTON) &&
      titleBarComponent->minimizeButton)
    titleWidth -= titleBarComponent->minimizeButton->width;
  if ((window->flags & WINFLAG_HASCLOSEBUTTON) &&
      titleBarComponent->closeButton)
    titleWidth -= titleBarComponent->closeButton->width;

  while (kernelFontGetPrintedWidth(titleBarFont, title) > titleWidth)
    title[strlen(title) - 2] = '\0';

  kernelGraphicDrawText(buffer, &foregroundColor, &backgroundColor,
			titleBarFont, title, draw_translucent,
			(component->xCoord + 5), (component->yCoord +
		  ((component->height - titleBarFont->charHeight) / 2)));

  if ((window->flags & WINFLAG_HASMINIMIZEBUTTON) &&
      titleBarComponent->minimizeButton &&
      titleBarComponent->minimizeButton->draw)
    titleBarComponent->minimizeButton
      ->draw((void *) titleBarComponent->minimizeButton);

  if ((window->flags & WINFLAG_HASCLOSEBUTTON) &&
      titleBarComponent->closeButton && titleBarComponent->closeButton->draw)
    titleBarComponent->closeButton
      ->draw((void *) titleBarComponent->closeButton);

  return (0);
}


static int move(void *componentData, int xCoord, int yCoord)
{
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindow *window = (kernelWindow *) component->window;
  kernelWindowTitleBar *titleBar = (kernelWindowTitleBar *) component->data;
  int buttonX, buttonY = (yCoord + 2);

  // Move our buttons

  if ((window->flags & WINFLAG_HASCLOSEBUTTON) && titleBar->closeButton)
    {
      buttonX =
	(xCoord + (component->width - (titleBar->closeButton->width + 2)));

      if (titleBar->closeButton->move)
	titleBar->closeButton
	  ->move((void *) titleBar->closeButton, buttonX, buttonY);

      titleBar->closeButton->xCoord = buttonX;
      titleBar->closeButton->yCoord = buttonY;
    }

  if ((window->flags & WINFLAG_HASMINIMIZEBUTTON) && titleBar->minimizeButton)
    {
      buttonX =
	(xCoord + (component->width - (titleBar->minimizeButton->width + 2)));

      if ((window->flags & WINFLAG_HASCLOSEBUTTON) && titleBar->closeButton)
	buttonX -= (titleBar->closeButton->width + 2);

      if (titleBar->minimizeButton->move)
	titleBar->minimizeButton
	  ->move((void *) titleBar->minimizeButton, buttonX, buttonY);

      titleBar->minimizeButton->xCoord = buttonX;
      titleBar->minimizeButton->yCoord = buttonY;
    }

  return (0);
}

 
static int resize(void *componentData, int width, int height)
{
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindow *window = (kernelWindow *) component->window;
  kernelWindowTitleBar *titleBar = (kernelWindowTitleBar *) component->data;
  int buttonX, buttonY = (component->yCoord + 2);

  // Move our buttons

  if ((window->flags & WINFLAG_HASCLOSEBUTTON) && titleBar->closeButton)
    {
      buttonX =
	(component->xCoord + (width - (titleBar->closeButton->width + 2)));

      if (titleBar->closeButton->move)
	titleBar->closeButton
	  ->move((void *) titleBar->closeButton, buttonX, buttonY);

      titleBar->closeButton->width = (height - 4);
      titleBar->closeButton->height = (height - 4);
      titleBar->closeButton->xCoord = buttonX;
      titleBar->closeButton->yCoord = buttonY;
    }

  if ((window->flags & WINFLAG_HASMINIMIZEBUTTON) && titleBar->minimizeButton)
    {
      buttonX =
	(component->xCoord + (width - (titleBar->minimizeButton->width + 2)));

      if ((window->flags & WINFLAG_HASCLOSEBUTTON) && titleBar->closeButton)
	buttonX -= (titleBar->closeButton->width + 2);

      if (titleBar->minimizeButton->move)
	titleBar->minimizeButton
	  ->move((void *) titleBar->minimizeButton, buttonX, buttonY);

      titleBar->minimizeButton->width = (height - 4);
      titleBar->minimizeButton->height = (height - 4);
      titleBar->minimizeButton->xCoord = buttonX;
      titleBar->minimizeButton->yCoord = buttonY;
    }

  return (0);
}


static int mouseEvent(void *componentData, windowEvent *event)
{
  int status = 0;
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindowTitleBar *titleBar = (kernelWindowTitleBar *) component->data;
  kernelWindow *window = (kernelWindow *) component->window;
  static windowEvent dragEvent;
  static int dragging = 0;
  static int oldWindowX = 0;
  static int oldWindowY = 0;
  int newWindowX = 0;
  int newWindowY = 0;

  // Is the window being dragged by the title bar?

  if (dragging)
    {
      if (event->type == EVENT_MOUSE_DRAG)
	{
	  // The window is still moving

	  // Erase the xor'ed outline
	  kernelWindowRedrawArea(window->xCoord, window->yCoord,
				 window->buffer.width, 1);
	  kernelWindowRedrawArea(window->xCoord, window->yCoord, 1,
				 window->buffer.height);
	  kernelWindowRedrawArea((window->xCoord + window->buffer.width - 1),
				 window->yCoord, 1, window->buffer.height);
	  kernelWindowRedrawArea(window->xCoord,
				 (window->yCoord + window->buffer.height - 1),
				 window->buffer.width, 1);
	      
	  // Set the new position
	  window->xCoord += (event->xPosition - dragEvent.xPosition);
	  window->yCoord += (event->yPosition - dragEvent.yPosition);

	  // Draw an xor'ed outline
	  kernelGraphicDrawRect(NULL, &((color) { 255, 255, 255 }),
				draw_xor, window->xCoord, window->yCoord,
				window->buffer.width, window->buffer.height,
				1, 0);

	  // Save a copy of the dragging event
	  kernelMemCopy(event, &dragEvent, sizeof(windowEvent));
	}

      else
	{
	  // The move is finished

	  // Temp
	  newWindowX = window->xCoord;
	  newWindowY = window->yCoord;
	  window->xCoord = oldWindowX;
	  window->yCoord = oldWindowY;
	  
	  window->xCoord = newWindowX;
	  window->yCoord = newWindowY;

	  window->flags |= WINFLAG_VISIBLE;

	  // Re-render it at the new location
	  kernelGraphicRenderBuffer(&(window->buffer), window->xCoord,
				    window->yCoord, 0, 0,
				    window->buffer.width,
				    window->buffer.height);
	  dragging = 0;
	}

      // Redraw the mouse
      kernelMouseDraw();

      return (status = 0);
    }

  else if ((window->flags & WINFLAG_HASCLOSEBUTTON) && titleBar->closeButton &&
	   isMouseInButton(event, titleBar->closeButton))
    {
      // Call the 'event' function for buttons
      if (titleBar->closeButton->mouseEvent)
	status = titleBar->closeButton
	  ->mouseEvent((void *) titleBar->closeButton, event);

      // Put this mouse event into the button's windowEventStream
      kernelWindowEventStreamWrite(&(titleBar->closeButton->events), event);
	  
      return (status);
    }
  
  else if ((window->flags & WINFLAG_HASMINIMIZEBUTTON) &&
	   titleBar->minimizeButton &&
	   isMouseInButton(event, titleBar->minimizeButton))
    {
      // Call the 'event' function for buttons
      if (titleBar->minimizeButton->mouseEvent)
	status = titleBar->minimizeButton
	  ->mouseEvent((void *) titleBar->minimizeButton, event);

      // Put this mouse event into the button's windowEventStream
      kernelWindowEventStreamWrite(&(titleBar->minimizeButton->events), event);
	  
      return (status);
    }
  
  else if (event->type == EVENT_MOUSE_DRAG)
    {
      if (window->flags & WINFLAG_MOVABLE)
	{
	  // The user has started dragging the window

	  oldWindowX = window->xCoord;
	  oldWindowY = window->yCoord;
		  
	  // Don't show it while it's moving
	  window->flags &= ~WINFLAG_VISIBLE;
	  kernelWindowRedrawArea(window->xCoord, window->yCoord,
				 window->buffer.width, window->buffer.height);
		      
	  // Draw an xor'ed outline
	  kernelGraphicDrawRect(NULL, &((color) { 255, 255, 255 }),
				draw_xor, window->xCoord,
				window->yCoord, window->buffer.width,
				window->buffer.height, 1, 0);

	  // Save a copy of the dragging event
	  kernelMemCopy(event, &dragEvent, sizeof(windowEvent));
	  dragging = 1;
	}

      return (status = 0);
    }

  return (status = 0);
}


static int destroy(void *componentData)
{
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;

  if (component->data)
    {
      // Release the title bar itself
      // NO, THIS IS CRASHY.  TMP TMP TMP
      // kernelFree(component->data);
      // component->data = NULL;
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


kernelWindowComponent *kernelWindowNewTitleBar(volatile void *parent,
					       int width,
					       componentParameters *params)
{
  // Formats a kernelWindowComponent as a kernelWindowTitleBar

  int status = 0;
  kernelWindowComponent *component = NULL;
  kernelWindowTitleBar *titleBar = NULL;
  componentParameters buttonParams;
  
  // Check parameters
  if ((parent == NULL) || (params == NULL))
    return (component = NULL);

  if (titleBarFont == NULL)
    {
      // Try to load a nice-looking font
      status = kernelFontLoad(DEFAULT_VARIABLEFONT_MEDIUM_FILE,
			      DEFAULT_VARIABLEFONT_MEDIUM_NAME,
			      &titleBarFont, 0);
      if (status < 0)
	// Font's not there, we suppose.  There's always a default.
	kernelFontGetDefault(&titleBarFont);
    }

  if (!imagesCreated)
    createImages((DEFAULT_TITLEBAR_HEIGHT - 4), (DEFAULT_TITLEBAR_HEIGHT - 4));

  // Get the basic component structure
  component = kernelWindowComponentNew(parent, params);
  if (component == NULL)
    return (component);

  // Now populate the main component
  component->type = titleBarComponentType;
  component->width = width;
  component->height = DEFAULT_TITLEBAR_HEIGHT;

  // The functions
  component->draw = &draw;
  component->move = &move;
  component->resize = &resize;
  component->mouseEvent = &mouseEvent;
  component->destroy = &destroy;

  // Get memory for the title bar structure
  titleBar = kernelMalloc(sizeof(kernelWindowTitleBar));
  if (titleBar == NULL)
    {
      kernelFree((void *) component);
      return (component = NULL);
    }

  // Put any minimize/maximize/close buttons on the title bar.

  // Standard parameters for a close button
  kernelMemClear((void *) &buttonParams, sizeof(componentParameters));
  buttonParams.useDefaultForeground = 1;
  buttonParams.useDefaultBackground = 1;

  titleBar->closeButton =
    kernelWindowNewButton(parent, NULL, ((closeImage.data == NULL)?
			   NULL : &closeImage), &buttonParams);

  if (titleBar->closeButton)
    {
      titleBar->closeButton->width = (DEFAULT_TITLEBAR_HEIGHT - 4);
      titleBar->closeButton->height = (DEFAULT_TITLEBAR_HEIGHT - 4);

      // We don't want close buttons to get the focus
      titleBar->closeButton->flags &= ~WINFLAG_CANFOCUS;

      kernelWindowRegisterEventHandler((objectKey) titleBar->closeButton,
				       &closeWindow);

      getWindow(parent)->flags |= WINFLAG_HASCLOSEBUTTON;
    }

  titleBar->minimizeButton =
    kernelWindowNewButton(parent, NULL, ((minimizeImage.data == NULL)?
			   NULL : &minimizeImage), &buttonParams);

  if (titleBar->minimizeButton)
    {
      titleBar->minimizeButton->width = (DEFAULT_TITLEBAR_HEIGHT - 4);
      titleBar->minimizeButton->height = (DEFAULT_TITLEBAR_HEIGHT - 4);

      // We don't want close buttons to get the focus
      titleBar->minimizeButton->flags &= ~WINFLAG_CANFOCUS;

      kernelWindowRegisterEventHandler((objectKey) titleBar->minimizeButton,
				       &minimizeWindow);

      getWindow(parent)->flags |= WINFLAG_HASMINIMIZEBUTTON;
    }

  component->data = (void *) titleBar;

  return (component);
}
