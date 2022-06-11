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
//  kernelWindowProgressBar.c
//

// This code is for managing kernelWindowProgressBar objects.


#include "kernelWindow.h"     // Our prototypes are here
#include "kernelMalloc.h"
#include "kernelError.h"
#include <stdio.h>

static int borderThickness = 3;
static int borderShadingIncrement = 15;
static kernelAsciiFont *defaultFont = NULL;


static int draw(kernelWindowComponent *component)
{
  // Draw the progress bar component

  kernelWindowProgressBar *progressBar = component->data;
  char prog[5];

  // Draw the background of the progress bar
  kernelGraphicDrawRect(component->buffer,
			(color *) &(component->parameters.background),
			draw_normal, (component->xCoord + borderThickness),
			(component->yCoord + borderThickness),
			(component->width - (borderThickness * 2)),
			(component->height - (borderThickness * 2)),
			1, 1);

  // Draw the border
  kernelGraphicDrawGradientBorder(component->buffer,
				  component->xCoord, component->yCoord,
				  component->width, component->height,
				  borderThickness, (color *)
				  &(component->parameters.background),
				  borderShadingIncrement, draw_reverse,
				  border_all);

  // Draw the slider
  progressBar->sliderWidth = (((component->width - (borderThickness * 2)) *
			      progressBar->progressPercent) / 100);
  if (progressBar->sliderWidth < (borderThickness * 2))
    progressBar->sliderWidth = (borderThickness * 2);
  
  kernelGraphicDrawGradientBorder(component->buffer,
				  (component->xCoord + borderThickness),
				  (component->yCoord + borderThickness),
				  progressBar->sliderWidth,
				  (component->height - (borderThickness * 2)),
				  borderThickness, (color *)
				  &(component->parameters.background),
				  borderShadingIncrement, draw_normal,
				  border_all);

  // Print the progress percent
  sprintf(prog, "%d%%", progressBar->progressPercent);
  kernelGraphicDrawText(component->buffer,
			(color *) &(component->parameters.foreground),
			(color *) &(component->parameters.background),
			defaultFont, prog, draw_translucent,
			(component->xCoord +
			 ((component->width -
			   kernelFontGetPrintedWidth(defaultFont, prog)) / 2)),
			(component->yCoord +
			 ((component->height - defaultFont->charHeight) / 2)));

  return (0);
}


static int setData(kernelWindowComponent *component, void *data, int length)
{
  // Set the progress percentage.  Our 'data' parameter is just an
  // integer value

  int status = 0;
  kernelWindowProgressBar *progressBar = component->data;

  // We ignore 'length'.  This keeps the compiler happy
  if (length == 0)
    return (status = ERR_NULLPARAMETER);

  if (component->erase)
    component->erase(component);

  progressBar->progressPercent = (int) data;

  if (progressBar->progressPercent < 0)
    progressBar->progressPercent = 0;
  if (progressBar->progressPercent > 100)
    progressBar->progressPercent = 100;

  if (component->draw)
    status = component->draw(component);

  component->window
    ->update(component->window, component->xCoord, component->yCoord,
	     component->width, component->height);

  return (0);
}


static int destroy(kernelWindowComponent *component)
{
  // Release all our memory
  if (component->data)
    {
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


kernelWindowComponent *kernelWindowNewProgressBar(objectKey parent,
						  componentParameters *params)
{
  // Formats a kernelWindowComponent as a kernelWindowProgressBar

  kernelWindowComponent *component = NULL;
  kernelWindowProgressBar *progressBar = NULL;

  // Check parameters.  It's okay for the image or label to be NULL
  if (parent == NULL)
    return (component = NULL);

  if (defaultFont == NULL)
    {
      // Try to load a nice-looking font
      if (kernelFontLoad(WINDOW_DEFAULT_VARFONT_SMALL_FILE,
			 WINDOW_DEFAULT_VARFONT_SMALL_NAME,
			 &defaultFont, 0) < 0)
	// Font's not there, we suppose.  There's always a default.
	kernelFontGetDefault(&defaultFont);
    }

  // Get the basic component structure
  component = kernelWindowComponentNew(parent, params);
  if (component == NULL)
    return (component);

  // Now populate it
  component->type = progressBarComponentType;
  component->width = 200;
  component->height = 25;
  component->minWidth = component->width;
  component->minHeight = component->height;

  progressBar = kernelMalloc(sizeof(kernelWindowProgressBar));
  if (progressBar == NULL)
    {
      kernelFree((void *) component);
      return (component = NULL);
    }

  progressBar->progressPercent = 0;
  
  component->data = (void *) progressBar;

  // The functions
  component->draw = &draw;
  component->setData = &setData;
  component->destroy = &destroy;

  return (component);
}
