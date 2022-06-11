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
//  kernelWindowIconComponent.c
//

// This code is for managing kernelWindowIconComponent objects.
// These are just images that appear inside windows and buttons, etc


#include "kernelWindowManager.h"     // Our prototypes are here
#include "kernelParameters.h"
#include "kernelMemoryManager.h"
#include "kernelMultitasker.h"
#include "kernelLoader.h"
#include "kernelMiscAsmFunctions.h"
#include <string.h>


static kernelAsciiFont *defaultFont = NULL;


static int draw(void *componentData)
{
  // Draw the image component

  color foreground = { 255, 255, 255 };
  color background = { 0xAB, 0x5D, 0x28 };
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelGraphicBuffer *buffer =
    &(((kernelWindow *) component->window)->buffer);
  kernelWindowIconComponent *iconComponent =
    (kernelWindowIconComponent *) component->data;

  int imageX = (component->xCoord +
		((component->width - iconComponent->iconImage.width) / 2));
  int labelWidth = ((strlen((char *) iconComponent->label) *
		    defaultFont->charWidth) + 4);
  int labelX = (component->xCoord + (component->width - labelWidth) / 2);
  int labelY = (component->yCoord + iconComponent->iconImage.height + 3);

  if (!component->parameters.useDefaultForeground)
    {
      // Use user-supplied colors
      foreground.red = component->parameters.foreground.red;
      foreground.green = component->parameters.foreground.green;
      foreground.blue = component->parameters.foreground.blue;
    }
  if (!component->parameters.useDefaultBackground)
    {
      // Use user-supplied colors
      background.red = component->parameters.background.red;
      background.green = component->parameters.background.green;
      background.blue = component->parameters.background.blue;
    }

  // Draw the icon image
  kernelGraphicDrawImage(buffer, (image *) &(iconComponent->iconImage),
			 imageX, component->yCoord);

  kernelGraphicClearArea(buffer, &background, labelX, labelY, labelWidth,
			 (defaultFont->charHeight + 4));

  kernelGraphicDrawText(buffer, &foreground, defaultFont,
			(char *) iconComponent->label,
			draw_normal, (labelX + 2), (labelY + 2));

  if (component->parameters.hasBorder)
    component->drawBorder((void *) component);

  return (0);
}


static int runCommandThread(void *componentData)
{
  int status = 0;
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindowIconComponent *iconComponent =
    (kernelWindowIconComponent *) component->data;

  status = kernelLoaderLoadProgram((const char *) iconComponent->command,
				   PRIVILEGE_USER, 0, NULL);

  // Send output to the console for now
  kernelMultitaskerSetTextOutput(status, kernelTextGetConsoleOutput());

  // Exec, don't block
  kernelLoaderExecProgram(status, 0);

  kernelMultitaskerTerminate(status);
}


static int mouseEvent(void *componentData, kernelMouseStatus *mouseStatus)
{
  // Launch a thread to load and run the program

  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindow *window = (kernelWindow *) component->window;
  kernelGraphicBuffer *buffer = &(window->buffer);
  kernelWindowIconComponent *iconComponent =
    (kernelWindowIconComponent *) component->data;
  int procId = 0;

  int imageX = (component->xCoord +
		((component->width - iconComponent->iconImage.width) / 2));


  // Is the icon being dragged around?

  if (window->mouseEvent.eventMask & MOUSE_DRAG)
    {
      if (mouseStatus->eventMask & MOUSE_DRAG)
	{
	  // The icon is still moving

	  // Erase the xor'ed outline
	  kernelGraphicDrawRect(NULL, &((color) { 255, 255, 255 }),
				draw_xor, (window->xCoord + component->xCoord),
				(window->yCoord + component->yCoord),
				component->width, component->height, 1, 0);
	      
	  // Set the new position
	  component->xCoord +=
	    (mouseStatus->xPosition - window->mouseEvent.xPosition);
	  
	  component->yCoord +=
	    (mouseStatus->yPosition - window->mouseEvent.yPosition);

	  // Draw an xor'ed outline
	  kernelGraphicDrawRect(NULL, &((color) { 255, 255, 255 }),
				draw_xor, (window->xCoord + component->xCoord),
				(window->yCoord + component->yCoord),
				component->width, component->height, 1, 0);
	  return (0);
	}

      else
	{
	  // The move is finished

	  component->visible = 1;

	  // Erase the xor'ed outline
	  kernelGraphicDrawRect(NULL, &((color) { 255, 255, 255 }),
				draw_xor, (window->xCoord + component->xCoord),
				(window->yCoord + component->yCoord),
				component->width, component->height, 1, 0);

	  // Re-render it at the new location
	  draw((void *) component);
	  kernelWindowManagerUpdateBuffer(&(window->buffer), 
					  component->xCoord, component->yCoord,
					  component->width, component->height);
	  
	  // Redraw the mouse
	  kernelMouseDraw();

	  return (0);
	}
    }

  else if (mouseStatus->eventMask & MOUSE_DRAG)
    {
      // The icon has started moving
		  
      // Don't show it while it's moving
      component->visible = 0;
      
      window->drawClip((void *) window, component->xCoord, component->yCoord,
		       component->width, component->height);

      // kernelWindowManagerRedrawArea((window->xCoord + component->xCoord),
      // 			    (window->yCoord + component->yCoord),
      // 			    component->width, component->height);
		      
      // Draw an xor'ed outline
      kernelGraphicDrawRect(NULL, &((color) { 255, 255, 255 }),
			    draw_xor, (window->xCoord + component->xCoord),
			    (window->yCoord + component->yCoord),
			    component->width, component->height, 1, 0);
      return (0);
    }

  else
    {
      // Just a click

      if (mouseStatus->eventMask & MOUSE_UP)
	{
	  kernelGraphicDrawRect(buffer, &((color) { 255, 255, 255 }),
				draw_xor, imageX, component->yCoord,
				iconComponent->iconImage.width,
				iconComponent->iconImage.height, 1, 1);
	  
	  procId = kernelMultitaskerSpawnKernelThread(runCommandThread,
				      "command execution", 1, &componentData);
	  if (procId < 0)
	    return (procId);

	  kernelMultitaskerSetProcessState(procId, ready);
	}

      else if (mouseStatus->eventMask & MOUSE_DOWN)
	{
	  // Show the icon being clicked
	  kernelGraphicDrawRect(buffer, &((color) { 255, 255, 255 }),
				draw_xor, imageX, component->yCoord,
				iconComponent->iconImage.width,
				iconComponent->iconImage.height, 1, 1);
	}

      kernelWindowManagerUpdateBuffer(buffer, imageX, component->yCoord,
				      iconComponent->iconImage.width,
				      iconComponent->iconImage.height);

      return (procId);
    }
}


static int erase(void *componentData)
{
  return (0);
}


static int destroy(void *componentData)
{
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindowIconComponent *iconComponent =
    (kernelWindowIconComponent *) component->data;

  // Release all our memory
  if (iconComponent != NULL)
    {
      kernelMemoryReleaseSystemBlock(iconComponent->iconImage.data);
      kernelMemoryReleaseSystemBlock((void *) iconComponent);
    }
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


kernelWindowComponent *kernelWindowNewIconComponent(kernelWindow *window,
				    image *imageCopy, const char *label,
				    const char *command)
{
  // Formats a kernelWindowComponent as a kernelWindowIconComponent

  kernelWindowComponent *component = NULL;
  kernelWindowIconComponent *iconComponent = NULL;
  int labelWidth = 0;

  // Check parameters
  if ((window == NULL) || (imageCopy == NULL) || (label == NULL) ||
      (command == NULL))
    return (component = NULL);

  // Get the basic component structure
  component = kernelWindowNewComponent();
  if (component == NULL)
    return (component);

  // Copy all the relevant data into our memory
  iconComponent =
    kernelMemoryRequestSystemBlock(sizeof(kernelWindowIconComponent),
				   0, "icon component");
  if (iconComponent == NULL)
    {
      kernelMemoryReleaseSystemBlock((void *) component);
      return (component = NULL);
    }

  kernelMemCopy(imageCopy, (image *) &(iconComponent->iconImage),
		sizeof(image));

  // Icons use pure green as the transparency color
  iconComponent->iconImage.isTranslucent = 1;
  iconComponent->iconImage.translucentColor.blue = 0;
  iconComponent->iconImage.translucentColor.green = 255;
  iconComponent->iconImage.translucentColor.red = 0;

  strncpy((char *) iconComponent->label, label, MAX_LABEL_LENGTH);
  iconComponent->label[MAX_LABEL_LENGTH - 1] = '\0';

  strncpy((char *) iconComponent->command, command, 128);
  iconComponent->command[127] = '\0';

  if (defaultFont == NULL)
    kernelFontGetDefault(&defaultFont);

  labelWidth = ((strlen((char *) iconComponent->label) *
		defaultFont->charWidth) + 4);

  // Copy the image data itself
  iconComponent->iconImage.data =
    kernelMemoryRequestSystemBlock(imageCopy->dataLength, 0,
				   "window icon data");
  if (iconComponent->iconImage.data != NULL)
    kernelMemCopy(imageCopy->data, iconComponent->iconImage.data,
		  imageCopy->dataLength);

  // Now populate the main component
  component->type = windowIconComponent;
  if (imageCopy->width > labelWidth)
    component->width = imageCopy->width;
  else
    component->width = labelWidth;
  component->height = (imageCopy->height + 7 + defaultFont->charHeight);
  
  component->data = (void *) iconComponent;

  // The functions
  component->draw = &draw;
  component->mouseEvent = &mouseEvent;
  component->erase = &erase;
  component->destroy = &destroy;

  return (component);
}
