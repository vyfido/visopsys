//
//  Visopsys
//  Copyright (C) 1998-2011 J. Andrew McLaughlin
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
#include "kernelDebug.h"
#include "kernelError.h"
#include "kernelFont.h"
#include "kernelGraphic.h"
#include "kernelMalloc.h"
#include "kernelMisc.h"
#include "kernelWindowEventStream.h"
#include <stdlib.h>
#include <string.h>

static image minimizeImage;
static image closeImage;
static int imagesCreated = 0;

extern kernelWindowVariables *windowVariables;


static int isMouseInButton(windowEvent *event, kernelWindowComponent *button)
{
  // We use this to determine whether a mouse event is inside one of our
  // buttons

  if (((event->xPosition >= (button->window->xCoord + button->xCoord)) &&
       (event->xPosition < (button->window->xCoord + button->xCoord +
			    button->width)) &&
       ((event->yPosition >= (button->window->yCoord + button->yCoord)) &&
	(event->yPosition < (button->window->yCoord + button->yCoord +
			     button->height)))))
    return (1);
  else
    return (0);
}


static void createImages(int width, int height)
{
  // Create some standard, shared images for close buttons, etc.
  
  kernelGraphicBuffer graphicBuffer;
  image tmpImage;
  color greenColor;
  int crossSize, crossStartX, crossStartY, crossEndX, crossEndY;

  kernelMemClear((void *) &graphicBuffer, sizeof(kernelGraphicBuffer));

  kernelMemClear(&greenColor, sizeof(color));
  greenColor.green = 0xFF;

  // Get a buffer to draw our close button graphic
  graphicBuffer.width = width;
  graphicBuffer.height = height;
  graphicBuffer.data =
    kernelMalloc(kernelGraphicCalculateAreaBytes(graphicBuffer.width,
						 graphicBuffer.height));
  if (graphicBuffer.data == NULL)
    return;

  // Do the minimize button
  kernelGraphicClearArea(&graphicBuffer, &greenColor, 0, 0,
			 graphicBuffer.width, graphicBuffer.height);
  kernelGraphicDrawRect(&graphicBuffer, &COLOR_BLACK, draw_normal,
			((width - 4) / 2), ((height - 4) / 2), 4, 4, 1, 0);
  kernelGraphicGetImage(&graphicBuffer, &tmpImage, 0, 0, graphicBuffer.width,
			graphicBuffer.height);
  kernelImageCopyToKernel(&tmpImage, &minimizeImage);
  kernelImageFree(&tmpImage);

  // Do the close button
  crossSize = min(8, (width - 4));
  crossStartX = ((width - crossSize) / 2);
  crossStartY = ((height - crossSize) / 2);
  crossEndX = (crossStartX + crossSize - 1);
  crossEndY = (crossStartY + crossSize - 1);
  kernelGraphicClearArea(&graphicBuffer, &greenColor, 0, 0,
			 graphicBuffer.width, graphicBuffer.height);

  kernelGraphicDrawLine(&graphicBuffer, &COLOR_BLACK, draw_normal,
			crossStartX, crossStartY, crossEndX, crossEndY);
  kernelGraphicDrawLine(&graphicBuffer, &COLOR_BLACK, draw_normal,
			(crossStartX + 1), crossStartY, crossEndX,
			(crossEndY - 1));
  kernelGraphicDrawLine(&graphicBuffer, &COLOR_BLACK, draw_normal,
			crossStartX, (crossStartY + 1), (crossEndX - 1),
			crossEndY);

  kernelGraphicDrawLine(&graphicBuffer, &COLOR_BLACK, draw_normal,
			crossStartX, crossEndY, crossEndX, crossStartY);
  kernelGraphicDrawLine(&graphicBuffer, &COLOR_BLACK, draw_normal,
			crossStartX, (crossEndY - 1), (crossEndX - 1),
			crossStartY);
  kernelGraphicDrawLine(&graphicBuffer, &COLOR_BLACK, draw_normal,
			(crossStartX + 1), crossEndY, crossEndX,
			(crossStartY + 1));

  kernelGraphicGetImage(&graphicBuffer, &tmpImage, 0, 0,
			graphicBuffer.width, graphicBuffer.height);
  kernelImageCopyToKernel(&tmpImage, &closeImage);
  kernelImageFree(&tmpImage);

  kernelFree(graphicBuffer.data);
  graphicBuffer.data = NULL;

  imagesCreated = 1;
}


static void minimizeWindow(kernelWindow *window, windowEvent *event)
{
  // This function gets called when the minimize button gets pushed

  // Minimize the window
  kernelWindowSetMinimized(window, 1);
      
  // Transfer this event into the window's event stream, so the application
  // can find out about it.
  event->type = EVENT_WINDOW_MINIMIZE;
  kernelWindowEventStreamWrite(&(window->events), event);
  
  return;
}


static void closeWindow(kernelWindow *window, windowEvent *event)
{
  // This function gets called when the close button gets pushed

  // Transfer this event into the window's event stream, so the application
  // can find out about it.
  event->type = EVENT_WINDOW_CLOSE;
  kernelWindowEventStreamWrite(&(window->events), event);

  return;
}


static int draw(kernelWindowComponent *component)
{
  // Draw the title bar component atop the window

  kernelWindowTitleBar *titleBar = component->data;
  asciiFont *font = (asciiFont *) component->params.font;
  int titleWidth = 0;
  char title[128];
  color foregroundColor;
  color backgroundColor;

  // The color will be different depending on whether the window has
  // the focus
  if (component->window->flags & WINFLAG_HASFOCUS)
    {
      if (component->params.flags & WINDOW_COMPFLAG_CUSTOMBACKGROUND)
	{
	  // Use user-supplied colors
	  kernelMemCopy((color *) &component->params.background,
			&backgroundColor, sizeof(color));
	}
      else
	{
	  // Use default color blue
	  backgroundColor.red = 0;
	  backgroundColor.green = 0;
	  backgroundColor.blue = 200;
	}
    }
  else
    {
      backgroundColor.red = 0;
      backgroundColor.green = 0;
      backgroundColor.blue = 150;
    }

  kernelGraphicConvexShade(component->buffer, &backgroundColor,
			   component->xCoord, component->yCoord,
			   component->width, component->height, shade_fromtop);

  // Put the title on the title bar
  
  if (component->params.flags & WINDOW_COMPFLAG_CUSTOMFOREGROUND)
    {
      // Use user-supplied colors
      kernelMemCopy((color *) &component->params.foreground, &foregroundColor,
		    sizeof(color));
    }
  else
    {
      // Use default color white
      foregroundColor.red = 255;
      foregroundColor.green = 255;
      foregroundColor.blue = 255;
    }

  if (font)
    {
      strncpy(title, (char *) component->window->title, 128);
      titleWidth = (component->width - 1);
      if (titleBar->minimizeButton)
	titleWidth -= titleBar->minimizeButton->width;
      if (titleBar->closeButton)
	titleWidth -= titleBar->closeButton->width;

      while (kernelFontGetPrintedWidth(font, title) > titleWidth)
	title[strlen(title) - 2] = '\0';

      kernelGraphicDrawText(component->buffer, &foregroundColor,
			    &backgroundColor, font, title, draw_translucent,
			    (component->xCoord + 5),
			    (component->yCoord +
			     ((component->height - font->charHeight) / 2)));
    }

  if (titleBar->minimizeButton && titleBar->minimizeButton->draw)
    titleBar->minimizeButton->draw(titleBar->minimizeButton);

  if (titleBar->closeButton && titleBar->closeButton->draw)
    titleBar->closeButton->draw(titleBar->closeButton);

  return (0);
}


static int move(kernelWindowComponent *component, int xCoord, int yCoord)
{
  kernelWindowTitleBar *titleBar = component->data;
  int buttonX, buttonY = (yCoord + 2);

  // Move our buttons

  if (titleBar->closeButton)
    {
      buttonX =
	(xCoord + (component->width - (titleBar->closeButton->width + 2)));

      if (titleBar->closeButton->move)
	titleBar->closeButton->move(titleBar->closeButton, buttonX, buttonY);

      titleBar->closeButton->xCoord = buttonX;
      titleBar->closeButton->yCoord = buttonY;
    }

  if (titleBar->minimizeButton)
    {
      buttonX =
	(xCoord + (component->width - (titleBar->minimizeButton->width + 2)));

      if (titleBar->closeButton)
	buttonX -= (titleBar->closeButton->width + 2);

      if (titleBar->minimizeButton->move)
	titleBar->minimizeButton
	  ->move(titleBar->minimizeButton, buttonX, buttonY);

      titleBar->minimizeButton->xCoord = buttonX;
      titleBar->minimizeButton->yCoord = buttonY;
    }

  return (0);
}

 
static int resize(kernelWindowComponent *component, int width, int height)
{
  kernelWindowTitleBar *titleBar = component->data;
  int buttonX, buttonY = (component->yCoord + 2);

  // Move our buttons

  if (titleBar->closeButton)
    {
      buttonX =
	(component->xCoord + (width - (titleBar->closeButton->width + 2)));

      if (titleBar->closeButton->move)
	titleBar->closeButton->move(titleBar->closeButton, buttonX, buttonY);

      titleBar->closeButton->width = (height - 4);
      titleBar->closeButton->height = (height - 4);
      titleBar->closeButton->minWidth = titleBar->closeButton->width;
      titleBar->closeButton->minHeight = titleBar->closeButton->height;
      titleBar->closeButton->xCoord = buttonX;
      titleBar->closeButton->yCoord = buttonY;
    }

  if (titleBar->minimizeButton)
    {
      buttonX =
	(component->xCoord + (width - (titleBar->minimizeButton->width + 2)));

      if (titleBar->closeButton)
	buttonX -= (titleBar->closeButton->width + 2);

      if (titleBar->minimizeButton->move)
	titleBar->minimizeButton->move(titleBar->minimizeButton, buttonX,
				       buttonY);

      titleBar->minimizeButton->width = (height - 4);
      titleBar->minimizeButton->height = (height - 4);
      titleBar->minimizeButton->minWidth = titleBar->minimizeButton->width;
      titleBar->minimizeButton->minHeight = titleBar->minimizeButton->height;
      titleBar->minimizeButton->xCoord = buttonX;
      titleBar->minimizeButton->yCoord = buttonY;
    }

  return (0);
}


static int mouseEvent(kernelWindowComponent *component, windowEvent *event)
{
  int status = 0;
  kernelWindowTitleBar *titleBar = component->data;
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
	  kernelWindowRedrawArea(component->window->xCoord,
				 component->window->yCoord,
				 component->window->buffer.width, 1);
	  kernelWindowRedrawArea(component->window->xCoord,
				 component->window->yCoord, 1,
				 component->window->buffer.height);
	  kernelWindowRedrawArea((component->window->xCoord +
				  component->window->buffer.width - 1),
				 component->window->yCoord, 1,
				 component->window->buffer.height);
	  kernelWindowRedrawArea(component->window->xCoord,
				 (component->window->yCoord +
				  component->window->buffer.height - 1),
				 component->window->buffer.width, 1);

	  // Set the new position
	  component->window->xCoord +=
	    (event->xPosition - dragEvent.xPosition);
	  component->window->yCoord +=
	    (event->yPosition - dragEvent.yPosition);

	  // Draw an xor'ed outline
	  kernelGraphicDrawRect(NULL, &((color) { 255, 255, 255 }),
				draw_xor, component->window->xCoord,
				component->window->yCoord,
				component->window->buffer.width,
				component->window->buffer.height, 1, 0);

	  // Save a copy of the dragging event
	  kernelMemCopy(event, &dragEvent, sizeof(windowEvent));
	}

      else
	{
	  // The move is finished

	  // Temp
	  newWindowX = component->window->xCoord;
	  newWindowY = component->window->yCoord;
	  component->window->xCoord = oldWindowX;
	  component->window->yCoord = oldWindowY;
	  
	  component->window->xCoord = newWindowX;
	  component->window->yCoord = newWindowY;

	  component->window->flags |= WINFLAG_VISIBLE;

	  // Re-render it at the new location
	  kernelWindowRedrawArea(component->window->xCoord,
				 component->window->yCoord,
				 component->window->buffer.width,
				 component->window->buffer.height);

	  dragging = 0;
	}

      // Redraw the mouse
      kernelMouseDraw();

      return (status = 0);
    }

  else if (titleBar->minimizeButton &&
	   isMouseInButton(event, titleBar->minimizeButton))
    {
      // Pass the event to the button
      if (titleBar->minimizeButton->mouseEvent)
	titleBar->minimizeButton->mouseEvent(titleBar->minimizeButton, event);

      // Minimize the window
      if (event->type == EVENT_MOUSE_LEFTUP)
	minimizeWindow(component->window, event);
	
      return (status = 0);
    }

  else if (titleBar->closeButton &&
	   isMouseInButton(event, titleBar->closeButton))
    {
      // Pass the event to the button
      if (titleBar->closeButton->mouseEvent)
	titleBar->closeButton->mouseEvent(titleBar->closeButton, event);

      // Close the window
      if (event->type == EVENT_MOUSE_LEFTUP)
	closeWindow(component->window, event);
	
      return (status = 0);
    }
  
  else if (event->type == EVENT_MOUSE_DRAG)
    {
      if (component->window->flags & WINFLAG_MOVABLE)
	{
	  // The user has started dragging the window

	  oldWindowX = component->window->xCoord;
	  oldWindowY = component->window->yCoord;

	  // Don't show it while it's moving
	  component->window->flags &= ~WINFLAG_VISIBLE;
	  kernelWindowRedrawArea(component->window->xCoord,
				 component->window->yCoord,
				 component->window->buffer.width,
				 component->window->buffer.height);
		      
	  // Draw an xor'ed outline
	  kernelGraphicDrawRect(NULL, &((color) { 255, 255, 255 }),
				draw_xor, component->window->xCoord,
				component->window->yCoord,
				component->window->buffer.width,
				component->window->buffer.height, 1, 0);

	  // Save a copy of the dragging event
	  kernelMemCopy(event, &dragEvent, sizeof(windowEvent));
	  dragging = 1;
	}

      return (status = 0);
    }

  return (status = 0);
}


static int destroy(kernelWindowComponent *component)
{
  kernelWindowTitleBar *titleBar = component->data;

  kernelDebug(debug_gui, "Destroying \"%s\" title bar",
	      component->window->title);

  if (titleBar)
    {
      // Destroy minimize and close buttons, if applicable
      if (titleBar->minimizeButton && titleBar->minimizeButton->destroy)
	titleBar->minimizeButton->destroy(titleBar->minimizeButton);
      if (titleBar->closeButton && titleBar->closeButton->destroy)
	titleBar->closeButton->destroy(titleBar->closeButton);

      component->window->titleBar = NULL;

      // Release the title bar itself
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


kernelWindowComponent *kernelWindowNewTitleBar(kernelWindow *window,
					       componentParameters *params)
{
  // Formats a kernelWindowComponent as a kernelWindowTitleBar

  kernelWindowComponent *component = NULL;
  kernelWindowTitleBar *titleBar = NULL;
  int titleBarHeight = windowVariables->titleBar.height;
  componentParameters buttonParams;
  
  // Check parameters
  if ((window == NULL) || (params == NULL))
    return (component = NULL);

  if (window->type != windowType)
    {
      kernelError(kernel_error, "Title bars can only be added to windows");
      return (component = NULL);
    }

  if (!imagesCreated)
    createImages((titleBarHeight - 4), (titleBarHeight - 4));

  // Get the basic component structure
  component = kernelWindowComponentNew(window->sysContainer, params);
  if (component == NULL)
    return (component);

  // If font is NULL, use the default
  if (component->params.font == NULL)
    component->params.font = windowVariables->font.varWidth.medium.font;

  // Now populate the main component
  component->type = titleBarComponentType;
  component->flags &= ~WINFLAG_CANFOCUS;
  component->width = windowVariables->titleBar.minWidth;
  component->height = titleBarHeight;
  component->minWidth = component->width;
  component->minHeight = component->height;

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

  // Standard parameters for the buttons
  kernelMemClear(&buttonParams, sizeof(componentParameters));

  titleBar->minimizeButton =
    kernelWindowNewButton(window->sysContainer, NULL,
			  ((minimizeImage.data == NULL)?
			  NULL : &minimizeImage), &buttonParams);

  if (titleBar->minimizeButton)
    {
      titleBar->minimizeButton->width = (titleBarHeight - 4);
      titleBar->minimizeButton->height = (titleBarHeight - 4);
      titleBar->minimizeButton->minWidth = titleBar->minimizeButton->width;
      titleBar->minimizeButton->minHeight = titleBar->minimizeButton->height;

      // We don't want minimize buttons to get the focus
      titleBar->minimizeButton->flags &= ~WINFLAG_CANFOCUS;

      // Remove it from the system container
      removeFromContainer(titleBar->minimizeButton);
    }

  titleBar->closeButton =
    kernelWindowNewButton(window->sysContainer, NULL,
			  ((closeImage.data == NULL)? NULL : &closeImage),
			  &buttonParams);

  if (titleBar->closeButton)
    {
      titleBar->closeButton->width = (titleBarHeight - 4);
      titleBar->closeButton->height = (titleBarHeight - 4);
      titleBar->closeButton->minWidth = titleBar->closeButton->width;
      titleBar->closeButton->minHeight = titleBar->closeButton->height;

      // We don't want close buttons to get the focus
      titleBar->closeButton->flags &= ~WINFLAG_CANFOCUS;

      // Remove it from the system container
      removeFromContainer(titleBar->closeButton);
    }

  component->data = (void *) titleBar;

  return (component);
}
