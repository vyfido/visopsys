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
//  kernelWindowImage.c
//

// This code is for managing kernelWindowImage objects.
// These are just images that appear inside windows and buttons, etc


#include "kernelWindow.h"     // Our prototypes are here
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

  if ((component->params.flags & WINDOW_COMPFLAG_HASBORDER) ||
      (component->flags & WINFLAG_HASFOCUS))
    component->drawBorder(component, 1);

  return (0);
}


static int destroy(kernelWindowComponent *component)
{
  kernelWindowImage *windowImage = component->data;

  // Release all our memory
  if (windowImage)
    {
      if (windowImage->image.data)
	{
	  kernelFree(windowImage->image.data);
	  windowImage->image.data = NULL;
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

  // Copy the image data into it
  kernelMemCopy(imageCopy, (void *) &(windowImage->image), sizeof(image));
  windowImage->image.data = kernelMalloc(windowImage->image.dataLength);
  if (windowImage->image.data == NULL)
    {
      kernelFree((void *) component);
      return (component = NULL);
    }
  kernelMemCopy(imageCopy->data, windowImage->image.data,
		windowImage->image.dataLength);

  // Set the drawing mode
  windowImage->mode = mode;

  component->data = (void *) windowImage;

  // The functions
  component->draw = &draw;
  component->destroy = &destroy;

  return (component);
}
