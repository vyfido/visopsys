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
//  kernelWindowIcon.c
//

// This code is for managing kernelWindowIcon objects.

#include "kernelWindow.h"	// Our prototypes are here
#include "kernelError.h"
#include "kernelFont.h"
#include "kernelImage.h"
#include "kernelMalloc.h"
#include "kernelMisc.h"
#include <stdlib.h>
#include <string.h>

#define IMAGEX	\
	(component->xCoord + ((component->width - icon->iconImage.width) / 2))

extern kernelWindowVariables *windowVariables;


static void setLabel(kernelWindowIcon *icon, const char *label,
	asciiFont *font)
{
	// Given a string, try and fit it into our maximum number of label lines
	// with each having a maximum width.  For a long icon label, try to split it
	// up in a sensible and pleasing way.

	int labelLen = 0;
	int labelSplit = 0;
	int count1, count2;

	labelLen = min(strlen(label), WINDOW_MAX_LABEL_LENGTH);

	// By default just copy the label into a single line.
	strncpy((char *) icon->label[0], label, labelLen);
	icon->label[0][labelLen] = '\0';
	icon->labelWidth = kernelFontGetPrintedWidth(font, label);
	icon->labelLines = 1;

	// Is the label too wide?  If so, we will break it into 2 lines
	if (icon->labelWidth <= 90)
		return;

	// First try to split it at a space character nearest the the center
	// of the string.  If the first part is still too long, split the string at
	// an arbitrary maximum width.  If the second part is still too long after
	// the split, truncate it.

	labelSplit = (labelLen / 2);

	// Try to locate the 'space' character closest to the center of the
	// string (if any).
	for (count1 = count2 = labelSplit; ((count1 >= 0) && (count2 < labelLen));
		count1--, count2++)
	{
		if (label[count1] == ' ')
		{
			labelSplit = count1;
			break;
		}
		else if (label[count2] == ' ')
		{
			labelSplit = count2;
			break;
		}
	}

	// Try splitting the string at labelSplit
	strncpy((char *) icon->label[0], label, labelSplit);
	if (label[labelSplit] == ' ')
		icon->label[0][labelSplit] = '\0';
	else
		icon->label[0][labelSplit + 1] = '\0';
	strncpy((char *) icon->label[1], (label + labelSplit + 1),
		(labelLen - (labelSplit + 1)));
	icon->label[1][labelLen - labelSplit] = '\0';

	if (kernelFontGetPrintedWidth(font, (char *) icon->label[0]) > 90)
	{
		// The first line is still too long.
		for (labelSplit = (labelSplit - 1); labelSplit >= 0; labelSplit --)
		{
			icon->label[0][labelSplit] = '\0';
			if (kernelFontGetPrintedWidth(font, (char *) icon->label[0]) <= 90)
				break;
		}

		// Copy the rest into the second line
		strncpy((char *) icon->label[1], (label + labelSplit),
			(labelLen - labelSplit));
		icon->label[1][labelLen - labelSplit] = '\0';

		if (kernelFontGetPrintedWidth(font, (char *) icon->label[1]) > 90)
		{
			// The second line is still too long.
			for (count1 = (strlen((char *) icon->label[1]) - 3); count1 >= 0;
				count1 --)
			{
				strcpy((char *)(icon->label[1] + count1), "...");
				if (kernelFontGetPrintedWidth(font, (char *) icon->label[1]) <=
					90)
				{
					break;
				}
			}
		}
	}

	count1 = kernelFontGetPrintedWidth(font, (char *) icon->label[0]);
	count2 = kernelFontGetPrintedWidth(font, (char *) icon->label[1]);
	icon->labelWidth = max(count1, count2);
	icon->labelLines = 2;
}


static int draw(kernelWindowComponent *component)
{
	// Draw the icon component

	kernelWindowIcon *icon = component->data;
	color *textColor = NULL;
	color *textBackground = NULL;
	asciiFont *font = (asciiFont *) component->params.font;
	int count;

	int labelX = (component->xCoord + (component->width - icon->labelWidth) / 2);
	int labelY = (component->yCoord + icon->iconImage.height + 3);

	// Draw the icon image
	kernelGraphicDrawImage(component->buffer, (image *) &icon->iconImage,
		draw_translucent, IMAGEX, component->yCoord, 0, 0, 0, 0);

	if (icon->selected)
	{
		textColor = (color *) &component->params.background;
		textBackground = (color *) &component->params.foreground;
	}
	else
	{
		textColor = (color *) &component->params.foreground;
		textBackground = (color *) &component->params.background;
	}

	for (count = 0; count < icon->labelLines; count ++)
	{
		if (font)
		{
			labelX = (component->xCoord +
				((component->width -
					kernelFontGetPrintedWidth(font, (char *)
						icon->label[count])) / 2) + 1);
			labelY = (component->yCoord + icon->iconImage.height + 4 +
				(font->charHeight * count));

			kernelGraphicDrawText(component->buffer, textBackground, textColor,
				font, (char *) icon->label[count], draw_translucent,
				(labelX + 1), (labelY + 1));
			kernelGraphicDrawText(component->buffer, textColor, textBackground,
				font, (char *) icon->label[count], draw_translucent,
				labelX, labelY);
		}
	}

	if (component->params.flags & WINDOW_COMPFLAG_HASBORDER)
		component->drawBorder(component, 1);

	return (0);
}


static int focus(kernelWindowComponent *component, int gotFocus)
{
	kernelWindowIcon *icon = component->data;

	if (gotFocus)
	{
		kernelGraphicDrawImage(component->buffer, (image *) &icon->selectedImage,
			draw_translucent, IMAGEX, component->yCoord, 0, 0, 0, 0);
		component->window
			->update(component->window, component->xCoord, component->yCoord,
				 component->width, component->height);
	}
	else if (component->window->drawClip)
		component->window
			->drawClip(component->window, component->xCoord, component->yCoord,
				 component->width, component->height);

	return (0);
}


static int setData(kernelWindowComponent *component, void *label,
	int length __attribute__((unused)))
{
	// Set the icon label

	kernelWindowIcon *icon = component->data;

	if (component->params.font)
		setLabel(icon, label, (asciiFont *) component->params.font);

	// Re-draw
	if (component->draw)
		draw(component);

	component->window
		->update(component->window, component->xCoord, component->yCoord,
			component->width, component->height);

	return (0);
}


static int mouseEvent(kernelWindowComponent *component, windowEvent *event)
{
	// Launch a thread to load and run the program

	kernelWindowIcon *icon = component->data;
	static int dragging = 0;
	static windowEvent dragEvent;

	// Is the icon being dragged around?
	if (dragging)
	{
		if (event->type == EVENT_MOUSE_DRAG)
		{
			// The icon is still moving

			// Erase the moving image
			kernelWindowRedrawArea((component->window->xCoord +
				component->xCoord),
				(component->window->yCoord + component->yCoord),
					component->width, component->height);

			// Set the new position
			component->xCoord += (event->xPosition - dragEvent.xPosition);
			component->yCoord += (event->yPosition - dragEvent.yPosition);

			// Draw the moving image.
			kernelGraphicDrawImage(NULL, (image *) &icon->selectedImage,
				draw_translucent, (component->window->xCoord + IMAGEX),
				(component->window->yCoord + component->yCoord),
				0, 0, 0, 0);

			// Save a copy of the dragging event
			kernelMemCopy(event, &dragEvent, sizeof(windowEvent));
		}
		else
		{
			// The move is finished

			component->flags |= WINFLAG_VISIBLE;

			// Erase the moving image
			kernelWindowRedrawArea((component->window->xCoord +
				component->xCoord),
				(component->window->yCoord + component->yCoord),
				component->width, component->height);

			icon->selected = 0;

			// Re-render it at the new location
			if (component->draw)
				component->draw(component);

			// If we've moved the icon outside the parent container, expand
			// the container to contain it.
			if ((component->xCoord + component->width) >=
				(component->container->xCoord + component->container->width))
			{
				component->container->width =
					((component->xCoord - component->container->xCoord) +
						component->width + 1);
			}
			if ((component->yCoord + component->height) >=
				(component->container->yCoord + component->container->height))
			{
				component->container->height =
					((component->yCoord - component->container->yCoord) +
						component->height + 1);
			}

			component->window
				->update(component->window, component->xCoord,
					component->yCoord, component->width, component->height);

			// If the new location intersects any other components of the
			// window, we need to focus the icon
			kernelWindowComponentFocus(component);

			dragging = 0;
		}

		// Redraw the mouse
		kernelMouseDraw();
	}

	else if (event->type == EVENT_MOUSE_DRAG)
	{
		// The icon has started moving

		// Don't show it while it's moving
		component->flags &= ~WINFLAG_VISIBLE;

		if (component->window->drawClip)
			component->window
				->drawClip(component->window, component->xCoord,
					component->yCoord, component->width, component->height);

		// Draw the moving image.
		kernelGraphicDrawImage(NULL, (image *) &icon->selectedImage,
			draw_translucent, (component->window->xCoord + IMAGEX),
			(component->window->yCoord + component->yCoord), 0, 0, 0, 0);

		// Save a copy of the dragging event
		kernelMemCopy(event, &dragEvent, sizeof(windowEvent));
		dragging = 1;
	}

	else if ((event->type == EVENT_MOUSE_LEFTDOWN) ||
		(event->type == EVENT_MOUSE_LEFTUP))
	{
		// Just a click

		if (event->type == EVENT_MOUSE_LEFTDOWN)
		{
			kernelGraphicDrawImage(component->buffer,
				(image *) &icon->selectedImage, draw_translucent, IMAGEX,
				component->yCoord, 0, 0, 0, 0);
			icon->selected = 1;
		}
		else if (event->type == EVENT_MOUSE_LEFTUP)
		{
			icon->selected = 0;

			// Remove the focus from the icon.  This will cause it to be redrawn
			// in its default way.
			if (component->window->changeComponentFocus)
				component->window->changeComponentFocus(component->window, NULL);
		}

		component->window->update(component->window, IMAGEX, component->yCoord,
			icon->iconImage.width, icon->iconImage.height);
	}

	return (0);
}


static int keyEvent(kernelWindowComponent *component, windowEvent *event)
{
	int status = 0;

	// We're only looking for 'enter' key releases, which we turn into mouse
	// button presses.
	if ((event->type & EVENT_MASK_KEY) && (event->key == ASCII_ENTER))
	{
		if (event->type == EVENT_KEY_DOWN)
			event->type = EVENT_MOUSE_LEFTDOWN;
		if (event->type == EVENT_KEY_UP)
			event->type = EVENT_MOUSE_LEFTUP;

		status = mouseEvent(component, event);
	}

	return (status);
}


static int destroy(kernelWindowComponent *component)
{
	kernelWindowIcon *icon = component->data;

	// Release all our memory
	if (icon)
	{
		kernelImageFree((image *) &icon->iconImage);
		kernelImageFree((image *) &icon->selectedImage);

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


kernelWindowComponent *kernelWindowNewIcon(objectKey parent, image *imageCopy,
	const char *label, componentParameters *params)
{
	// Formats a kernelWindowComponent as a kernelWindowIcon

	kernelWindowComponent *component = NULL;
	kernelWindowIcon *icon = NULL;
	pixel *pix = NULL;
	unsigned count;

	// Check params
	if (!parent || !imageCopy || !label || !params)
	{
		kernelError(kernel_error, "NULL parameter");
		return (component = NULL);
	}

	if (!imageCopy->data)
	{
		kernelError(kernel_error, "Image data is NULL");
		return (component = NULL);
	}

	// Get the basic component structure
	component = kernelWindowComponentNew(parent, params);
	if (component == NULL)
		return (component);

	component->type = iconComponentType;

	// Set the functions
	component->draw = &draw;
	component->focus = &focus;
	component->setData = &setData;
	component->mouseEvent = &mouseEvent;
	component->keyEvent = &keyEvent;
	component->destroy = &destroy;

	// If default colors are requested, override the standard component colors
	// with the ones we prefer
	if (!(component->params.flags & WINDOW_COMPFLAG_CUSTOMFOREGROUND))
	{
		component->params.foreground.red = 0x28;
		component->params.foreground.green = 0x5D;
		component->params.foreground.blue = 0xAB;
		component->params.flags |= WINDOW_COMPFLAG_CUSTOMFOREGROUND;
	}
	if (!(component->params.flags & WINDOW_COMPFLAG_CUSTOMBACKGROUND))
	{
		kernelMemCopy(&COLOR_WHITE, (color *) &component->params.background,
			sizeof(color));
		component->params.flags |= WINDOW_COMPFLAG_CUSTOMBACKGROUND;
	}

	// Always use our font
	component->params.font = windowVariables->font.varWidth.small.font;

	// Copy all the relevant data into our memory
	icon = kernelMalloc(sizeof(kernelWindowIcon));
	if (icon == NULL)
	{
		kernelWindowComponentDestroy(component);
		return (component = NULL);
	}

	component->data = (void *) icon;

	// Copy the image to kernel memory
	if (kernelImageCopyToKernel(imageCopy, (image *) &icon->iconImage) < 0)
	{
		kernelWindowComponentDestroy(component);
		return (component = NULL);
	}

	// Icons use pure green as the transparency color
	icon->iconImage.transColor.blue = 0;
	icon->iconImage.transColor.green = 255;
	icon->iconImage.transColor.red = 0;

	// When the icon is selected, we do a little effect that makes the image
	// appear yellowish.
	if (kernelImageCopyToKernel(imageCopy, (image *) &icon->selectedImage) < 0)
	{
		kernelWindowComponentDestroy(component);
		return (component = NULL);
	}

	// Icons use pure green as the transparency color
	icon->selectedImage.transColor.blue = 0;
	icon->selectedImage.transColor.green = 255;
	icon->selectedImage.transColor.red = 0;

	for (count = 0; count < icon->selectedImage.pixels; count ++)
	{
		pix = &((pixel *) icon->selectedImage.data)[count];

		if (!PIXELS_EQ(pix, &icon->selectedImage.transColor))
		{
			pix->red = ((pix->red + 255) / 2);
			pix->green = ((pix->green + 255) / 2);
			pix->blue /= 2;
		}
	}

	if (component->params.font)
		setLabel(icon, label, (asciiFont *) component->params.font);

	// Now populate the main component
	component->width = max(imageCopy->width, ((unsigned)(icon->labelWidth + 3)));
	component->height = (imageCopy->height + 5);
	if (component->params.font)
		component->height += (((asciiFont *) component->params.font)->charHeight *
			icon->labelLines);

	component->minWidth = component->width;
	component->minHeight = component->height;

	return (component);
}

