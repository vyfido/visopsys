//
//  Visopsys
//  Copyright (C) 1998-2006 J. Andrew McLaughlin
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
//  kernelWindowCanvas.c
//

// This code is for managing kernelWindowCanvas objects.
// These are just kernelWindowImage components that can be drawn upon.

#include "kernelWindow.h"     // Our prototypes are here
#include "kernelMalloc.h"
#include "kernelMemory.h"
#include "kernelMisc.h"
#include "kernelError.h"
#include <string.h>


static int resize(void *componentData, int width, int height)
{
  int status = 0;
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindowCanvas *canvas = (kernelWindowCanvas *) component->data;
  void *savePtr = NULL;

  if (canvas->imageData.data)
    {
      savePtr = canvas->imageData.data;
      canvas->imageData.data = NULL;
    }

  // Get a new image
  status = kernelGraphicNewKernelImage((image *) &(canvas->imageData), width,
				       height);
  if (status < 0)
    {
      canvas->imageData.data = savePtr;
      return (status);
    }
  
  // If there was old data, free it
  if (savePtr)
    kernelFree(savePtr);

  return (status = 0);
}


static int setData(void *componentData, void *data, int size)
{
  // This is where we implement drawing on the canvas.  Our parameter
  // is a structure that specifies the drawing operation and parameters

  int status = 0;
  windowDrawParameters *params = (windowDrawParameters *) data;
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindow *window = component->window;
  kernelGraphicBuffer *buffer = &(window->buffer);
  kernelWindowCanvas *canvas = (kernelWindowCanvas *) component->data;

  int xCoord1 = component->xCoord + params->xCoord1;
  int xCoord2 = component->xCoord + params->xCoord2;
  int yCoord1 = component->yCoord + params->yCoord1;
  int yCoord2 = component->yCoord + params->yCoord2;

  // We ignore the 'size' parameter.  This keeps the compiler happy.
  if (size == 0)
    return (status = ERR_INVALID);

  switch (params->operation)
    {
    case draw_pixel:
      status = kernelGraphicDrawPixel(buffer, &(params->foreground),
				      params->mode, xCoord1, yCoord1);
      break;
    case draw_line:
      status = kernelGraphicDrawLine(buffer, &(params->foreground),
				     params->mode, xCoord1, yCoord1,
				     xCoord2, yCoord2);
      break;
    case draw_rect:
      status = kernelGraphicDrawRect(buffer, &(params->foreground),
				     params->mode, xCoord1, yCoord1,
				     params->width, params->height,
				     params->thickness, params->fill);
      break;
    case draw_oval:
      status = kernelGraphicDrawOval(buffer, &(params->foreground),
				     params->mode, xCoord1, yCoord1,
				     params->width, params->height,
				     params->thickness, params->fill);
      break;
    case draw_image:
      status = kernelGraphicDrawImage(buffer, (image *) params->data,
				      params->mode, xCoord1, yCoord1, xCoord2,
				      yCoord2, params->width, params->height);
      break;
    case draw_text:
      status = kernelGraphicDrawText(buffer, &(params->foreground),
				     &(params->background), params->font,
				     (char *) params->data, params->mode,
				     xCoord1, yCoord1);
      break;
    default:
      break;
    }

  // Get the component's new image
  status =
    kernelGraphicGetKernelImage(buffer, (image *) &(canvas->imageData),
				component->xCoord, component->yCoord,
				canvas->imageData.width,
				canvas->imageData.height);
  
  kernelWindowUpdateBuffer(buffer, component->xCoord, component->yCoord,
			   component->width, component->height);

  return (status);
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


kernelWindowComponent *kernelWindowNewCanvas(volatile void *parent, 
					     int width, int height,
					     componentParameters *params)
{
  // Formats a kernelWindowComponent as a kernelWindowCanvas.  A
  // kernelWindowCanvas is a type of kernelWindowImage, but we allow
  // drawing operations on it.

  int status = 0;
  kernelWindowComponent *component = NULL;
  image tmpImage;

  // Check params
  if ((parent == NULL) || (params == NULL))
    return (component = NULL);

  // Get a temporary image of the correct size
  tmpImage.data = NULL;
  status = kernelGraphicNewKernelImage(&tmpImage, width, height);
  if (status < 0)
    return (component = NULL);

  // Get the kernelWindowImage that underlies this canvas
  component = kernelWindowNewImage(parent, &tmpImage, draw_normal, params);

  // Free our temporary image data
  kernelFree(tmpImage.data);
      
  if (component == NULL)
    return (component);
      
  // Now override some bits
  component->type = canvasComponentType;

  // The functions
  component->resize = &resize;
  component->setData = &setData;

  return (component);
}
