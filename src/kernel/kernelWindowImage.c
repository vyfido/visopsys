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
//  kernelWindowImage.c
//

// This code is for managing kernelWindowImage objects.
// These are just images that appear inside windows and buttons, etc


#include "kernelWindow.h"     // Our prototypes are here
#include "kernelMalloc.h"
#include "kernelMiscFunctions.h"
#include <string.h>


static int draw(void *componentData)
{
  // Draw the image component
  
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelGraphicBuffer *buffer =
    &(((kernelWindow *) component->window)->buffer);
  kernelWindowImage *windowImage = (kernelWindowImage *) component->data;

  kernelGraphicDrawImage(buffer, (image *) &(windowImage->imageData),
			 windowImage->mode, component->xCoord,
			 component->yCoord, 0, 0, 0, 0);

  if (component->parameters.hasBorder)
    component->drawBorder((void *) component, 1);

  return (0);
}


static int destroy(void *componentData)
{
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindowImage *windowImage = (kernelWindowImage *) component->data;

  // Release all our memory
  if (windowImage)
    {
      if (windowImage->imageData.data)
	{
	  kernelFree(windowImage->imageData.data);
	  windowImage->imageData.data = NULL;
	}

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


kernelWindowComponent *kernelWindowNewImage(volatile void *parent,
					    image *imageCopy, drawMode mode,
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

  // Get the kernelWindowImage component memory
  windowImage = kernelMalloc(sizeof(kernelWindowImage));
  if (windowImage == NULL)
    {
      kernelFree((void *) component);
      return (component = NULL);
    }

  // Copy the image data into it
  kernelMemCopy(imageCopy, (void *) &(windowImage->imageData), sizeof(image));
  windowImage->imageData.data =
    kernelMalloc(windowImage->imageData.dataLength);
  if (windowImage->imageData.data == NULL)
    {
      kernelFree((void *) component);
      return (component = NULL);
    }
  kernelMemCopy(imageCopy->data, windowImage->imageData.data,
		windowImage->imageData.dataLength);

  // Set the drawing mode
  windowImage->mode = mode;

  component->data = (void *) windowImage;

  // The functions
  component->draw = &draw;
  component->destroy = &destroy;

  return (component);
}
