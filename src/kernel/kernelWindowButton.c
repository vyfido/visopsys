//
//  Visopsys
//  Copyright (C) 1998-2021 J. Andrew McLaughlin
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
//  kernelWindowButton.c
//

// This code is for managing kernelWindowButton objects

#include "kernelWindow.h"	// Our prototypes are here
#include "kernelError.h"
#include "kernelFont.h"
#include "kernelImage.h"
#include "kernelMalloc.h"
#include "kernelMemory.h"
#include "kernelMisc.h"
#include <stdlib.h>
#include <string.h>
#include <sys/color.h>

#define MIN_PAD		5
#define MIN_PAD_X2	(MIN_PAD * 2)

extern kernelWindowVariables *windowVariables;


static void setText(kernelWindowComponent *component, const char *label,
	int length)
{
	kernelWindowButton *button = (kernelWindowButton *) component->data;
	kernelFont *labelFont = (kernelFont *) component->params.font;

	strncpy((char *) button->label, label, min(length,
		WINDOW_MAX_LABEL_LENGTH));
	button->label[length] = '\0';

	int tmp = MIN_PAD_X2;
	if (labelFont)
	{
		tmp += kernelFontGetPrintedWidth(labelFont, (char *)
			component->charSet, (char *) button->label);
	}

	if (tmp > component->width)
		component->width = tmp;

	tmp = MIN_PAD_X2;
	if (labelFont)
		tmp += labelFont->glyphHeight;

	if (tmp > component->height)
		component->height = tmp;
}


static int setImage(kernelWindowComponent *component, image *img)
{
	int status = 0;
	kernelWindowButton *button = (kernelWindowButton *) component->data;

	status = kernelImageCopyToKernel(img, (image *) &button->buttonImage);
	if (status < 0)
		return (status);

	// Button images use pure green as the transparency color
	button->buttonImage.transColor.blue = 0;
	button->buttonImage.transColor.green = 255;
	button->buttonImage.transColor.red = 0;

	int tmp = (MIN_PAD_X2 + img->width);
	if (tmp > component->width)
		component->width = tmp;

	tmp = (MIN_PAD_X2 + img->height);
	if (tmp > component->height)
		component->height = tmp;

	return (status = 0);
}


static void drawFocus(kernelWindowComponent *component, int focus)
{
	color drawColor;

	// Draw the appropriate border around the button

	if ((component->flags & WINDOW_COMP_FLAG_CANFOCUS) && focus)
	{
		COLOR_COPY(&drawColor, &component->params.foreground);
	}
	else
	{
		COLOR_ADJUST(&drawColor, &component->params.background, 5,
			6 /* 5/6ths */);
	}

	kernelGraphicDrawRect(component->buffer, &drawColor, draw_normal,
		component->xCoord, component->yCoord, component->width,
		component->height, 1 /* thickness */, 0 /* fill */);
}


static int draw(kernelWindowComponent *component)
{
	// Draw the button component

	kernelWindowButton *button = (kernelWindowButton *) component->data;
	kernelFont *labelFont = (kernelFont *) component->params.font;
	color drawColor;

	// Draw the background of the button

	if (button->state)
	{
		COLOR_ADJUST(&drawColor, &component->params.background, 5,
			6 /* 5/6ths */);
	}
	else
	{
		COLOR_COPY(&drawColor, &component->params.background);
	}

	kernelGraphicDrawRect(component->buffer, &drawColor, draw_normal,
		(component->xCoord + 1), (component->yCoord + 1),
		(component->width - 2), (component->height - 2),
		1 /* thickness */, 1 /* fill */);

	// If there is a label, draw it centered on the button
	if (button->label[0])
	{
		kernelGraphicDrawText(component->buffer, (color *)
			&component->params.foreground, (color *)
			&component->params.background, labelFont, (char *)
			component->charSet, (char *) button->label, draw_translucent,
			(component->xCoord + ((component->width -
				kernelFontGetPrintedWidth(labelFont, (char *)
					component->charSet, (char *) button->label)) / 2)),
			(component->yCoord + ((component->height -
				labelFont->glyphHeight) / 2)));
	}

	// If there is an image, draw it centered on the button
	if (button->buttonImage.data)
	{
		unsigned tmpX, tmpY, tmpXoff = 0, tmpYoff = 0;
		tmpX = component->xCoord + ((component->width -
			button->buttonImage.width) / 2);
		tmpY = component->yCoord + ((component->height -
			button->buttonImage.height) / 2);

		if (button->buttonImage.width > (unsigned) component->width)
			tmpXoff = -((button->buttonImage.width - component->width) / 2);
		if (button->buttonImage.height > (unsigned) component->height)
			tmpYoff = -((button->buttonImage.height - component->height) / 2);

		kernelGraphicDrawImage(component->buffer, (image *)
			&button->buttonImage, draw_alphablend, tmpX, tmpY, tmpXoff,
			tmpYoff, component->width, component->height);
	}

	drawFocus(component, (component->flags & WINDOW_COMP_FLAG_HASFOCUS));

	return (0);
}


static int focus(kernelWindowComponent *component, int yesNo)
{
	drawFocus(component, yesNo);

	component->window->update(component->window, component->xCoord,
		component->yCoord, component->width, component->height);

	return (0);
}


static int setData(kernelWindowComponent *component, void *data, int length)
{
	// Set the button text

	kernelWindowButton *button = (kernelWindowButton *) component->data;

	if (button->label[0])
		setText(component, data, length);
	else
		setImage(component, data);

	if (component->draw)
		component->draw(component);

	component->window->update(component->window, component->xCoord,
		component->yCoord, component->width, component->height);

	return (0);
}


static int mouseEvent(kernelWindowComponent *component, windowEvent *event)
{
	kernelWindowButton *button = (kernelWindowButton *) component->data;

	if ((event->type & WINDOW_EVENT_MOUSE_DOWN) || (event->type &
		WINDOW_EVENT_MOUSE_UP) || (event->type & WINDOW_EVENT_MOUSE_DRAG))
	{
		if ((event->type == WINDOW_EVENT_MOUSE_LEFTUP) ||
			(event->type == WINDOW_EVENT_MOUSE_DRAG))
		{
			button->state = 0;
		}
		else if (event->type == WINDOW_EVENT_MOUSE_LEFTDOWN)
		{
			button->state = 1;
		}

		if (component->draw)
			draw(component);

		component->window->update(component->window, component->xCoord,
			component->yCoord, component->width, component->height);
	}

	return (0);
}


static int keyEvent(kernelWindowComponent *component, windowEvent *event)
{
	int status = 0;
	kernelWindowButton *button = (kernelWindowButton *) component->data;

	// We're only looking for 'enter' key releases, which we turn into mouse
	// button presses
	if ((event->type & WINDOW_EVENT_MASK_KEY) &&
		(event->key.scan == keyEnter))
	{
		// If the button is not pushed, ignore this
		if ((event->type == WINDOW_EVENT_KEY_UP) && !button->state)
			return (status = 0);

		if (event->type == WINDOW_EVENT_KEY_DOWN)
			event->type = WINDOW_EVENT_MOUSE_LEFTDOWN;
		if (event->type == WINDOW_EVENT_KEY_UP)
			event->type = WINDOW_EVENT_MOUSE_LEFTUP;

		status = mouseEvent(component, event);
	}

	return (status);
}


static int destroy(kernelWindowComponent *component)
{
	kernelWindowButton *button = (kernelWindowButton *) component->data;

	// Release all our memory
	if (button)
	{
		// If we have an image, release the image data
		if (button->buttonImage.data)
			kernelImageFree((image *) &button->buttonImage);

		// The button itself
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

kernelWindowComponent *kernelWindowNewButton(objectKey parent,
	const char *label, image *buttonImage, componentParameters *params)
{
	// Formats a kernelWindowComponent as a kernelWindowButton

	kernelWindowComponent *component = NULL;
	kernelWindowButton *button = NULL;

	// Check params.  It's okay for the image or label to be NULL.
	if (!parent || !params)
	{
		kernelError(kernel_error, "NULL parameter");
		return (component = NULL);
	}

	// Get the basic component structure
	component = kernelWindowComponentNew(parent, params);
	if (!component)
		return (component);

	component->type = buttonComponentType;
	component->flags |= (WINDOW_COMP_FLAG_CANFOCUS |
		WINDOW_COMP_FLAG_RESIZABLEX);

	// Set the functions
	component->draw = &draw;
	component->focus = &focus;
	component->setData = &setData;
	component->mouseEvent = &mouseEvent;
	component->keyEvent = &keyEvent;
	component->destroy = &destroy;

	// If font is NULL, use the default
	if (!component->params.font)
		component->params.font = windowVariables->font.varWidth.medium.font;

	button = kernelMalloc(sizeof(kernelWindowButton));
	if (!button)
	{
		kernelWindowComponentDestroy(component);
		return (component = NULL);
	}

	component->data = (void *) button;

	// If the button has a label, copy it
	if (label)
		setText(component, label, strlen(label));

	// If the button has an image, copy it
	if (buttonImage && buttonImage->data)
	{
		if (setImage(component, buttonImage) < 0)
		{
			kernelWindowComponentDestroy(component);
			return (component = NULL);
		}
	}

	component->minWidth = component->width;
	component->minHeight = component->height;

	return (component);
}

