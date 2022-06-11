//
//  Visopsys
//  Copyright (C) 1998-2004 J. Andrew McLaughlin
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


#include "kernelWindowManager.h"     // Our prototypes are here
#include "kernelMalloc.h"
#include "kernelMiscFunctions.h"
#include <string.h>


static int draw(void *componentData)
{
  // Draw the image component
  
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelGraphicBuffer *buffer =
    &(((kernelWindow *) component->window)->buffer);
  image *baseImage = (image *) component->data;

  kernelGraphicDrawImage(buffer, baseImage, component->xCoord,
			 component->yCoord, 0, 0, 0, 0);

  if (component->parameters.hasBorder)
    component->drawBorder((void *) component);

  return (0);
}


static int erase(void *componentData)
{
  return (0);
}


static int destroy(void *componentData)
{
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  image *baseImage = (image *) component->data;

  // Release all our memory
  if (baseImage != NULL)
    {
      if (baseImage->data != NULL)
	kernelFree(baseImage->data);
      kernelFree((void *) baseImage);
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


kernelWindowComponent *kernelWindowNewImage(kernelWindow *window,
					    image *imageCopy)
{
  // Formats a kernelWindowComponent as a kernelWindowImage

  kernelWindowComponent *component = NULL;
  image *baseImage = NULL;

  // Check parameters
  if ((window == NULL) || (imageCopy == NULL))
    return (component = NULL);

  // Get the basic component structure
  component = kernelWindowNewComponent();
  if (component == NULL)
    return (component);

  // Now populate it
  component->type = windowImageComponent;
  component->width = imageCopy->width;
  component->height = imageCopy->height;

  // Copy all the relevant data into our memory
  baseImage = kernelMalloc(sizeof(image));
  if (baseImage == NULL)
    {
      kernelFree((void *) component);
      return (component = NULL);
    }

  kernelMemCopy(imageCopy, baseImage, sizeof(image));
  baseImage->data = kernelMalloc(baseImage->dataLength);
  if (baseImage->data == NULL)
    {
      kernelFree((void *) component);
      return (component = NULL);
    }
  kernelMemCopy(imageCopy->data, baseImage->data, baseImage->dataLength);

  component->data = baseImage;

  // The functions
  component->draw = &draw;
  component->mouseEvent = NULL;
  component->erase = &erase;
  component->destroy = &destroy;

  return (component);
}
