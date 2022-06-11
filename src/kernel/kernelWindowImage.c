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
//  kernelWindowImage.c
//

// This code is for managing kernelWindowImage objects.
// These are just images that appear inside windows and buttons, etc

#include "kernelWindow.h"     // Our prototypes are here
#include "kernelDebug.h"
#include "kernelMalloc.h"
#include "kernelMisc.h"
#include <string.h>


static int draw(kernelWindowComponent *component)
{
  // Draw the image component
  
  kernelWindowImage *windowImage = component->data;

  kernelGraphicDrawImage(component->buffer, (image *) &(windowImage->image),
			 windowImage->mode, component->xCoord,
			 component->yCoord, 0, 0, 0, 0);

  if (component->params.flags & WINDOW_COMPFLAG_HASBORDER)
    component->drawBorder(component, 1);

  return (0);
}


static int setData(kernelWindowComponent *component, void *buffer,
		   int size __attribute__((unused)))
{
  // Resets the subcomponents

  int status = 0;
  kernelWindowImage *windowImage = component->data;
  image *setImage = (image *) buffer;

  kernelDebug(debug_gui, "windowImage set data");

  kernelImageFree((image *) &(windowImage->image));

  // Copy the image to kernel memory
  status = kernelImageCopyToKernel(setImage, (image *) &(windowImage->image));
  if (status < 0)
    return (status);

  if (component->erase)
    component->erase(component);

  // Re-draw the image
  if (component->draw)
    component->draw(component);

  component->window
    ->update(component->window, component->xCoord, component->yCoord,
	     component->width, component->height);

  return (0);
}


static int destroy(kernelWindowComponent *component)
{
  kernelWindowImage *windowImage = component->data;

  // Release all our memory
  if (windowImage)
    {
      kernelImageFree((image *) &windowImage->image);

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


kernelWindowComponent *kernelWindowNewImage(objectKey parent, image *imageCopy,
					    drawMode mode,
					    componentParameters *params)
{
  // Formats a kernelWindowComponent as a kernelWindowImage

  kernelWindowComponent *component = NULL;
  kernelWindowImage *windowImage = NULL;

  // Check parameters
  if ((parent == NULL) || (imageCopy == NULL) || (params == NULL))
    return (component = NULL);

  // Get the basic component structure
  component = kernelWindowComponentNew(parent, params);
  if (component == NULL)
    return (component);

  // Now populate it
  component->type = imageComponentType;
  component->width = imageCopy->width;
  component->height = imageCopy->height;
  component->minWidth = component->width;
  component->minHeight = component->height;

  // Get the kernelWindowImage component memory
  windowImage = kernelMalloc(sizeof(kernelWindowImage));
  if (windowImage == NULL)
    {
      kernelFree((void *) component);
      return (component = NULL);
    }

  // Copy the image to kernel memory
  if (kernelImageCopyToKernel(imageCopy, (image *) &windowImage->image) < 0)
    {
      kernelFree((void *) component);
      kernelFree((void *) windowImage);
      return (component = NULL);
    }

  // Set the drawing mode
  windowImage->mode = mode;

  component->data = (void *) windowImage;

  // The functions
  component->draw = &draw;
  component->setData = &setData;
  component->destroy = &destroy;

  return (component);
}
