//
//  Visopsys
//  Copyright (C) 1998-2003 J. Andrew McLaughlin
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
//  kernelWindowTitleBarComponent.c
//

// This code is for managing kernelWindowTitleBarComponent objects.
// These are just images that appear inside windows and buttons, etc


#include "kernelWindowManager.h"     // Our prototypes are here
#include "kernelMemoryManager.h"
#include "kernelMultitasker.h"
#include "kernelParameters.h"
#include "kernelError.h"
#include <string.h>


static kernelAsciiFont *titleBarFont = NULL;


static inline int isMouseInButton(kernelMouseStatus *mouseStatus,
				  kernelWindowComponent *button)
{
  // We use this to determine whether a mouse event is inside one of our
  // buttons

  kernelWindow *window = (kernelWindow *) button->window;

  if (((mouseStatus->xPosition >= (window->xCoord + button->xCoord)) &&
       (mouseStatus->xPosition < (window->xCoord + button->xCoord +
				  button->width)) &&
       ((mouseStatus->yPosition >= (window->yCoord + button->yCoord)) &&
	(mouseStatus->yPosition < (window->yCoord + button->yCoord +
				   button->height)))))
    return (1);
  else
    return (0);
}


static int closeWindow(void *componentData)
{
  // This function gets called when the close button gets pushed

  int status = 0;

  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindow *window = (kernelWindow *) component->window;

  // Kill the process associated with this window.  This is temporary, until
  // we can send the process an 'event' that tells it the window has closed.
  if (window->processId != KERNELPROCID)
    kernelMultitaskerKillProcess(window->processId, 1);

  status = kernelWindowManagerDestroyWindow(window); 
  if (status < 0)
    kernelError(kernel_warn, "Unable to destroy window \"%s\"", window->title);

  // Don't do anything much after this point, as we no longer exist.

  kernelMouseDraw();

  return (status);
}


static int draw(void *componentData)
{
  // Draw the title bar component atop the window

  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindowTitleBarComponent *titleBarComponent =
    (kernelWindowTitleBarComponent *) component->data;
  kernelWindow *window = (kernelWindow *) component->window;
  kernelGraphicBuffer *buffer = &(window->buffer);
  char title[128];
  color drawColor;
  int count;

  // The color will be different depending on whether the window has
  // the focus
  if (window->hasFocus)
    {
      if (component->parameters.useDefaultBackground)
	{
	  // Use default color blue
	  drawColor.red = 0;
	  drawColor.green = 0;
	  drawColor.blue = 200;
	}
      else
	{
	  // Use user-supplied colors
	  drawColor.red = component->parameters.background.red;
	  drawColor.green = component->parameters.background.green;
	  drawColor.blue = component->parameters.background.blue;
	}
    }
  else
    {
      drawColor.red = 100;
      drawColor.green = 100;
      drawColor.blue = 100;
    }

  // We draw it inside the border as a series of lines.  It starts as a
  // darker blue and lightens in color
  for (count = 0; count < component->height; count ++)
    {
      kernelGraphicDrawLine(buffer, &drawColor, draw_normal,
			    component->xCoord, (component->yCoord + count),
			    (component->xCoord + component->width - 1),
			    (component->yCoord + count));
      if (drawColor.red > 0)
	drawColor.red -= 5;
      if (drawColor.green > 0)
	drawColor.green -= 5;
      if (drawColor.blue > 0)
	drawColor.blue -= 5;
    }

  // Put the title on the title bar
  
  if (component->parameters.useDefaultForeground)
    {
      // Use default color white
      drawColor.red = 255;
      drawColor.green = 255;
      drawColor.blue = 255;
    }
  else
    {
      // Use user-supplied colors
      drawColor.red = component->parameters.foreground.red;
      drawColor.green = component->parameters.foreground.green;
      drawColor.blue = component->parameters.foreground.blue;
    }

  strncpy(title, (char *) window->title, 128);
  while (kernelFontGetPrintedWidth(titleBarFont, title) >
	 (component->width - (window->hasCloseButton?
			      (titleBarComponent->closeButton->width + 1): 1)))
    title[strlen(title) - 2] = '\0';

  kernelGraphicDrawText(buffer, &drawColor, titleBarFont, title, draw_normal,
			(component->xCoord + 5), (component->yCoord +
		  ((component->height - titleBarFont->charHeight) / 2)));
  
  // Draw any buttons on the title bar
  if (window->hasCloseButton && (titleBarComponent->closeButton->draw != NULL))
    titleBarComponent->closeButton
      ->draw((void *) titleBarComponent->closeButton);

  if (component->parameters.hasBorder)
    component->drawBorder((void *) component);

  return (0);
}


static int erase(void *componentData)
{
  return (0);
}


static int move(void *componentData, int xCoord, int yCoord)
{
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindow *window = (kernelWindow *) component->window;
  kernelWindowTitleBarComponent *titleBarComponent =
    (kernelWindowTitleBarComponent *) component->data;
  kernelWindowComponent *closeButton =
    (kernelWindowComponent *) titleBarComponent->closeButton;

  if ((window->hasCloseButton) && (closeButton != NULL))
    {
      closeButton->xCoord =
	(xCoord + component->width - (component->height - 1));
      closeButton->yCoord = (yCoord + 1);
    }

  return (0);
}

 
static int resize(void *componentData, unsigned width, unsigned height)
{
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindow *window = (kernelWindow *) component->window;
  kernelWindowTitleBarComponent *titleBarComponent =
    (kernelWindowTitleBarComponent *) component->data;

  // Resize ourselves
  component->width = width;
  component->height = height;
  
  // Move our buttons
  if ((window->hasCloseButton) && (titleBarComponent->closeButton != NULL))
    {
      titleBarComponent->closeButton->width = (height - 2);
      titleBarComponent->closeButton->height = (height - 2);
      titleBarComponent->closeButton->xCoord = (component->xCoord +
			(width - (titleBarComponent->closeButton->width + 1)));
      titleBarComponent->closeButton->yCoord = (component->yCoord + 1);
    }

  return (0);
}


static int mouseEvent(void *componentData, kernelMouseStatus *mouseStatus)
{
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindowTitleBarComponent *titleBarComponent =
    (kernelWindowTitleBarComponent *) component->data;
  kernelWindowComponent *closeButton = (kernelWindowComponent *)
    titleBarComponent->closeButton;
  kernelWindow *window = (kernelWindow *) component->window;
  static int dragging = 0;
  static int oldWindowX = 0;
  static int oldWindowY = 0;
  int newWindowX = 0;
  int newWindowY = 0;

  // Is the window being dragged by the title bar?

  if (dragging)
    {
      if (mouseStatus->eventMask & MOUSE_DRAG)
	{
	  // The window is still moving

	  // Erase the xor'ed outline
	  kernelGraphicDrawRect(NULL, &((color) { 255, 255, 255 }),
				draw_xor, window->xCoord, window->yCoord,
				window->buffer.width, window->buffer.height,
				1, 0);
	      
	  // Set the new position
	  window->xCoord +=
	    (mouseStatus->xPosition - window->mouseEvent.xPosition);
	  
	  window->yCoord +=
	    (mouseStatus->yPosition - window->mouseEvent.yPosition);

	  // Draw an xor'ed outline
	  kernelGraphicDrawRect(NULL, &((color) { 255, 255, 255 }),
				draw_xor, window->xCoord, window->yCoord,
				window->buffer.width, window->buffer.height,
				1, 0);
	  return (0);
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

	  window->visible = 1;

	  // Decrement the levels of any windows now covered
	  // decrementCoveredLevels(window);

	  // Re-render it at the new location
	  kernelGraphicRenderBuffer(&(window->buffer), window->xCoord,
				    window->yCoord, 0, 0,
				    window->buffer.width,
				    window->buffer.height);
	  
	  // Redraw the mouse
	  kernelMouseDraw();

	  dragging = 0;

	  return (0);
	}
    }

  // Make sure the mouse event wasn't really inside one of our buttons
  else if ((window->hasCloseButton && (closeButton != NULL)) &&
	   (isMouseInButton(mouseStatus, closeButton)))
    return (closeButton->mouseEvent((void *) closeButton, mouseStatus));

  else if (mouseStatus->eventMask & MOUSE_DRAG)
    {
      // The window has started moving

      oldWindowX = window->xCoord;
      oldWindowY = window->yCoord;
		  
      // Don't show it while it's moving
      window->visible = 0;
      kernelWindowManagerRedrawArea(window->xCoord, window->yCoord,
				    window->buffer.width,
				    window->buffer.height);
		      
      // Draw an xor'ed outline
      kernelGraphicDrawRect(NULL, &((color) { 255, 255, 255 }),
			    draw_xor, window->xCoord,
			    window->yCoord, window->buffer.width,
			    window->buffer.height, 1, 0);

      dragging = 1;

      return (0);
    }

  // Nothing
  return (0);
}


static int destroy(void *componentData)
{
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindowTitleBarComponent *titleBarComponent =
    (kernelWindowTitleBarComponent *) component->data;

  // Release our memory
  if (titleBarComponent != NULL)
    kernelMemoryReleaseSystemBlock((void *) titleBarComponent);
  kernelMemoryReleaseSystemBlock(componentData);
  return (0);
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


kernelWindowComponent *kernelWindowNewTitleBarComponent(kernelWindow *window,
					unsigned width, unsigned height)
{
  // Formats a kernelWindowComponent as a kernelWindowTitleBarComponent

  int status = 0;
  kernelWindowComponent *component = NULL;
  kernelWindowTitleBarComponent *titleBarComponent = NULL;
  componentParameters params;

  // We don't want to load images for the buttons every time
  static image closeImage;
  
  // Check parameters
  if (window == NULL)
    return (component = NULL);

  // Get the basic component structure
  component = kernelWindowNewComponent();
  if (component == NULL)
    return (component);

  // Copy all the relevant data into our memory
  titleBarComponent =
    kernelMemoryRequestSystemBlock(sizeof(kernelWindowTitleBarComponent), 0,
				   "title bar component");
  if (titleBarComponent == NULL)
    {
      kernelMemoryReleaseSystemBlock((void *) component);
      return (component = NULL);
    }

  if (titleBarFont == NULL)
    {
      // Try to load a nice-looking font
      status = kernelFontLoad(DEFAULT_VARIABLEFONT_SMALL_FILE,
			      DEFAULT_VARIABLEFONT_SMALL_NAME, &titleBarFont);
      if (status < 0)
	// Font's not there, we suppose.  There's always a default.
	kernelFontGetDefault(&titleBarFont);
    }

  // Now populate the main component
  component->type = windowTitleBarComponent;
  component->width = width;
  component->height = height;
  
  component->data = (void *) titleBarComponent;

  // The functions
  component->draw = &draw;
  component->erase = &erase;
  component->move = &move;
  component->resize = &resize;
  component->mouseEvent = &mouseEvent;
  component->destroy = &destroy;

  // Put the minimize/maximize/close buttons on the title bar.

  if (window->hasCloseButton)
    {
      if (closeImage.data == NULL)
	// Load the close button image
	if (kernelImageLoadBmp("/system/closebtn.bmp", &closeImage) < 0)
	  closeImage.data = NULL;
  
      titleBarComponent->closeButton =
	kernelWindowNewButtonComponent(window, (height - 2), (height - 2),
				       NULL, ((closeImage.data == NULL)? NULL :
					      &closeImage), closeWindow);
      params.gridX = 0;
      params.gridY = 0;
      params.gridWidth = 1;
      params.gridHeight = 1;
      params.padLeft = 0;
      params.padRight = 0;
      params.padTop = 0;
      params.padBottom = 0;
      params.orientationX = orient_right;
      params.orientationY = orient_middle;
      params.hasBorder = 0;
      params.useDefaultForeground = 1;
      params.useDefaultBackground = 1;

      kernelWindowAddComponent(window, titleBarComponent->closeButton,
			       &params);
    }

  return (component);
}
