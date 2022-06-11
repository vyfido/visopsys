//
//  Visopsys
//  Copyright (C) 1998-2014 J. Andrew McLaughlin
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
//  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
//
//  kernelWindowCanvas.c
//

// This code is for managing kernelWindowCanvas objects.
// These are just kernelWindowImage components that can be drawn upon.

#include "kernelWindow.h"	// Our prototypes are here
#include "kernelDebug.h"
#include "kernelError.h"
#include "kernelMalloc.h"
#include "kernelMemory.h"
#include "kernelMisc.h"
#include <string.h>

static int (*saveDraw) (kernelWindowComponent *) = NULL;


static void drawFocus(kernelWindowComponent *component, int focus)
{
	color *drawColor = NULL;

	if (focus)
		drawColor = (color *) &component->params.foreground;
	else
		drawColor = (color *) &component->window->background;

	kernelGraphicDrawRect(component->buffer, drawColor, draw_normal,
		(component->xCoord - 1), (component->yCoord - 1),
		(component->width + 2),	(component->height + 2), 1, 0);

	return;
}


static int draw(kernelWindowComponent *component)
{
	// First draw the underlying image component, and then if it has the focus,
	// draw another border

	int status = 0;

	kernelDebug(debug_gui, "Canvas draw");

	if (saveDraw)
	{
		status = saveDraw(component);
		if (status < 0)
			return (status);
	}

	if (component->flags & WINFLAG_HASFOCUS)
		drawFocus(component, 1);

	return (status = 0);
}


static int resize(kernelWindowComponent *component, int width, int height)
{
	int status = 0;
	kernelWindowCanvas *canvas = component->data;
	image tmpImage;

	kernelDebug(debug_gui, "Canvas resize from %d,%d to %d,%d", component->width,
		component->height, width, height);

	kernelMemCopy((image *) &canvas->image, &tmpImage, sizeof(image));

	// Resize the canvas image
	status = kernelImageResize(&tmpImage, width, height);
	if (status < 0)
		return (status);

	// Copy it to kernel memory
	status = kernelImageCopyToKernel(&tmpImage, (image *) &canvas->image);
	if (status < 0)
		return (status);

	kernelImageFree(&tmpImage);

	return (status = 0);
}


static int focus(kernelWindowComponent *component, int yesNo)
{
	kernelDebug(debug_gui, "Canvas focus");
	
	drawFocus(component, yesNo);
	component->window->update(component->window, (component->xCoord - 1),
		(component->yCoord - 1), (component->width + 2),
		(component->height + 2));
	return (0);
}


static int setData(kernelWindowComponent *component, void *data, int size
	__attribute__((unused)))
{
	// This is where we implement drawing on the canvas.  Our parameter
	// is a structure that specifies the drawing operation and parameters

	int status = 0;
	kernelWindowCanvas *canvas = component->data;
	windowDrawParameters *params = data;
	image tmpImage;

	kernelDebug(debug_gui, "Canvas set data");

	int xCoord1 = component->xCoord + params->xCoord1;
	int xCoord2 = component->xCoord + params->xCoord2;
	int yCoord1 = component->yCoord + params->yCoord1;
	int yCoord2 = component->yCoord + params->yCoord2;

	switch (params->operation)
	{
		case draw_pixel:
			status = kernelGraphicDrawPixel(component->buffer,
				&params->foreground, params->mode, xCoord1, yCoord1);
			break;
		case draw_line:
			status = kernelGraphicDrawLine(component->buffer,
				&params->foreground, params->mode, xCoord1, yCoord1, xCoord2,
				yCoord2);
			break;
		case draw_rect:
			status = kernelGraphicDrawRect(component->buffer,
				&params->foreground, params->mode, xCoord1, yCoord1,
				params->width, params->height, params->thickness, params->fill);
			break;
		case draw_oval:
			status = kernelGraphicDrawOval(component->buffer,
				&params->foreground, params->mode, xCoord1, yCoord1,
				params->width, params->height, params->thickness, params->fill);
			break;
		case draw_image:
			status = kernelGraphicDrawImage(component->buffer,
				(image *) params->data, params->mode, xCoord1, yCoord1,
				(params->xCoord2? xCoord2 : 0), (params->yCoord2? yCoord2 : 0),
				params->width, params->height);
			break;
		case draw_text:
			if (params->font)
				status = kernelGraphicDrawText(component->buffer,
					&params->foreground, &params->background,
					(asciiFont *) params->font, (char *) params->data,
					params->mode, xCoord1, yCoord1);
			break;
		default:
			break;
		}

	// Get the component's new image
	status = kernelGraphicGetImage(component->buffer, &tmpImage,
		component->xCoord, component->yCoord, canvas->image.width,
		canvas->image.height);
	if (status < 0)
		return (status);

	kernelImageFree((image *) &canvas->image);
	kernelImageCopyToKernel(&tmpImage, (image *) &canvas->image);
	kernelImageFree(&tmpImage);

	component->window
		->update(component->window, component->xCoord, component->yCoord,
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


kernelWindowComponent *kernelWindowNewCanvas(objectKey parent, int width,
	int height, componentParameters *params)
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
	status = kernelImageNew(&tmpImage, width, height);
	if (status < 0)
		return (component = NULL);

	// Get the kernelWindowImage that underlies this canvas
	component = kernelWindowNewImage(parent, &tmpImage, draw_normal, params);

	// Free our temporary image data
	kernelImageFree(&tmpImage);
		
	if (component == NULL)
		return (component);
		
	// Now override some bits
	component->subType = canvasComponentType;
	component->flags |= WINFLAG_RESIZABLE;

	// The functions
	saveDraw = component->draw;
	component->draw = &draw;
	component->resize = &resize;
	component->focus = &focus;
	component->setData = &setData;

	return (component);
}
