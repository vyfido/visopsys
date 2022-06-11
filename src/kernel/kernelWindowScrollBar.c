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
//  kernelWindowScrollBar.c
//

// This code is for managing kernelWindowScrollBar objects

#include "kernelWindow.h"	// Our prototypes are here
#include "kernelError.h"
#include "kernelMalloc.h"
#include <stdlib.h>
#include <string.h>

#define BORDER_THICKNESS	1
#define BORDER_GAP			1
#define SLIDER_OFFSET		(BORDER_THICKNESS + BORDER_GAP)
#define SLIDER_OFFSET_X2	(SLIDER_OFFSET * 2)

extern kernelWindowVariables *windowVariables;


static void calcSliderSizePos(kernelWindowScrollBar *scrollBar, int width,
	int height)
{
	if (scrollBar->type == scrollbar_horizontal)
	{
		scrollBar->sliderWidth = (((width - SLIDER_OFFSET_X2) *
			scrollBar->state.displayPercent) / 100);
		scrollBar->sliderHeight = (height - SLIDER_OFFSET_X2);

		// Need a minimum width
		scrollBar->sliderWidth = max(scrollBar->sliderWidth, 6);

		scrollBar->sliderX = ((((width - SLIDER_OFFSET_X2) -
			scrollBar->sliderWidth) * scrollBar->state.positionPercent) /
			100);
		scrollBar->sliderY = 0;
	}

	else if (scrollBar->type == scrollbar_vertical)
	{
		scrollBar->sliderWidth = (width - SLIDER_OFFSET_X2);
		scrollBar->sliderHeight = (((height - SLIDER_OFFSET_X2) *
			scrollBar->state.displayPercent) / 100);

		// Need a minimum height
		scrollBar->sliderHeight = max(scrollBar->sliderHeight, 6);

		scrollBar->sliderX = 0;
		scrollBar->sliderY = ((((height - SLIDER_OFFSET_X2) -
			scrollBar->sliderHeight) * scrollBar->state.positionPercent) /
			100);
	}
}


static void calcSliderPosPercent(kernelWindowScrollBar *scrollBar, int width,
	int height)
{
	int extraSpace = 0;

	if (scrollBar->type == scrollbar_horizontal)
	{
		extraSpace = ((width - SLIDER_OFFSET_X2) - scrollBar->sliderWidth);

		if (extraSpace > 0)
		{
			scrollBar->state.positionPercent = ((scrollBar->sliderX * 100) /
				extraSpace);
		}
		else
		{
			scrollBar->state.positionPercent = 0;
		}
	}

	else if (scrollBar->type == scrollbar_vertical)
	{
		extraSpace = ((height - SLIDER_OFFSET_X2) - scrollBar->sliderHeight);

		if (extraSpace > 0)
		{
			scrollBar->state.positionPercent = ((scrollBar->sliderY * 100) /
				extraSpace);
		}
		else
		{
			scrollBar->state.positionPercent = 0;
		}
	}
}


static int draw(kernelWindowComponent *component)
{
	// Draw the scroll bar component

	kernelWindowScrollBar *scrollBar = component->data;
	int sliderDrawX = (component->xCoord + SLIDER_OFFSET +
		scrollBar->sliderX);
	int sliderDrawY = (component->yCoord + SLIDER_OFFSET +
		scrollBar->sliderY);
	color drawColor;

	// Clear the background
	kernelGraphicDrawRect(component->buffer, (color *)
		&component->params.background, draw_normal, (component->xCoord +
		BORDER_THICKNESS), (component->yCoord + BORDER_THICKNESS),
		(component->width - (BORDER_THICKNESS * 2)), (component->height -
		(BORDER_THICKNESS * 2)), 1 /* thickness */, 1 /* fill */);

	// Draw the outer border

	COLOR_ADJUST(&drawColor, &component->params.background, 5,
		6 /* 5/6ths */);

	kernelGraphicDrawRect(component->buffer, &drawColor, draw_normal,
		component->xCoord, component->yCoord, component->width,
		component->height, BORDER_THICKNESS, 0 /* fill */);

	// Draw the slider
	kernelGraphicDrawRect(component->buffer, &drawColor, draw_normal,
		sliderDrawX, sliderDrawY, scrollBar->sliderWidth,
		scrollBar->sliderHeight, 1 /* thickness */, 1 /* fill */);

	return (0);
}


static int resize(kernelWindowComponent *component, int width, int height)
{
	calcSliderSizePos(component->data, width, height);
	return (0);
}


static int getData(kernelWindowComponent *component, void *buffer, int size)
{
	// Gets the state of the scroll bar

	kernelWindowScrollBar *scrollBar = component->data;

	memcpy(buffer, (void *) &scrollBar->state, max(size, (int)
		sizeof(scrollBarState)));

	return (0);
}


static int setData(kernelWindowComponent *component, void *buffer, int size)
{
	// Sets the state of the scroll bar

	kernelWindowScrollBar *scrollBar = component->data;

	memcpy((void *) &scrollBar->state, buffer, max(size, (int)
		sizeof(scrollBarState)));

	calcSliderSizePos(scrollBar, component->width, component->height);

	draw(component);

	component->window->update(component->window, component->xCoord,
		component->yCoord, component->width, component->height);

	return (0);
}


static int mouseEvent(kernelWindowComponent *component, windowEvent *event)
{
	kernelWindowScrollBar *scrollBar = component->data;
	int eventX = 0;
	int eventY = 0;

	// Get X and Y coordinates relative to the component
	eventX = (event->coord.x - component->window->xCoord - component->xCoord);
	eventY = (event->coord.y - component->window->yCoord - component->yCoord);

	// Is the mouse event in the slider, or a mouse scroll?
	if (scrollBar->dragging ||
		((eventX >= scrollBar->sliderX) &&
		(eventX < (scrollBar->sliderX + scrollBar->sliderWidth)) &&
		(eventY >= scrollBar->sliderY) &&
		(eventY < (scrollBar->sliderY + scrollBar->sliderHeight))) ||
		(event->type & WINDOW_EVENT_MOUSE_SCROLL))
	{
		if (event->type == WINDOW_EVENT_MOUSE_DRAG)
		{
			if (scrollBar->dragging)
			{
				// The scroll bar is still moving.  Set the new position
				if (scrollBar->type == scrollbar_horizontal)
					scrollBar->sliderX += (eventX - scrollBar->dragX);
				else if (scrollBar->type == scrollbar_vertical)
					scrollBar->sliderY += (eventY - scrollBar->dragY);
			}
			else
			{
				// The scroll bar has started moving
				scrollBar->dragging = 1;
			}

			// Save the current dragging Y and Y coordinate
			scrollBar->dragX = eventX;
			scrollBar->dragY = eventY;
		}

		else
		{
			if (event->type & WINDOW_EVENT_MOUSE_SCROLL)
			{
				if (scrollBar->type == scrollbar_horizontal)
				{
					if (event->type == WINDOW_EVENT_MOUSE_SCROLLUP)
					{
						scrollBar->sliderX -= max(1,
							(scrollBar->sliderWidth / 3));
					}
					else
					{
						scrollBar->sliderX += max(1,
							(scrollBar->sliderWidth / 3));
					}
				}

				else if (scrollBar->type == scrollbar_vertical)
				{
					if (event->type == WINDOW_EVENT_MOUSE_SCROLLUP)
					{
						scrollBar->sliderY -= max(1,
							(scrollBar->sliderHeight / 3));
					}
					else
					{
						scrollBar->sliderY += max(1,
							(scrollBar->sliderHeight / 3));
					}
				}
			}

			// Not dragging
			scrollBar->dragging = 0;
		}
	}

	// Not in the slider, or a mouse scroll.  A click in the empty space?

	else if (scrollBar->type == scrollbar_horizontal)
	{
		// Is it in the space on either side of the slider?

		if ((event->type == WINDOW_EVENT_MOUSE_LEFTDOWN) &&
			(eventX > 0) && (eventX < scrollBar->sliderX))
		{
			// It's to the left of the slider
			scrollBar->sliderX -= scrollBar->sliderWidth;
		}

		else if ((event->type == WINDOW_EVENT_MOUSE_LEFTDOWN) &&
			(eventX >= (scrollBar->sliderX + scrollBar->sliderWidth)) &&
			(eventX < component->width))
		{
			// It's to the right of the slider
			scrollBar->sliderX += scrollBar->sliderWidth;
		}

		else
		{
			// Do nothing
			return (0);
		}
	}

	else if (scrollBar->type == scrollbar_vertical)
	{
		// Is it in the space above or below the slider?

		if ((event->type == WINDOW_EVENT_MOUSE_LEFTDOWN) &&
			(eventY > 0) && (eventY < scrollBar->sliderY))
		{
			// It's above the slider
			scrollBar->sliderY -= scrollBar->sliderHeight;
		}

		else if ((event->type == WINDOW_EVENT_MOUSE_LEFTDOWN) &&
			(eventY >= (scrollBar->sliderY + scrollBar->sliderHeight)) &&
			(eventY < component->height))
		{
			// It's below the slider
			scrollBar->sliderY += scrollBar->sliderHeight;
		}

		else
		{
			// Do nothing
			return (0);
		}
	}

	// Make sure the slider stays within the bounds

	if (scrollBar->type == scrollbar_horizontal)
	{
		if (scrollBar->sliderX < 0)
			scrollBar->sliderX = 0;

		if ((scrollBar->sliderX + scrollBar->sliderWidth) >=
			(component->width - SLIDER_OFFSET_X2))
		{
			scrollBar->sliderX = ((component->width - SLIDER_OFFSET_X2) -
				scrollBar->sliderWidth);
		}
	}

	else if (scrollBar->type == scrollbar_vertical)
	{
		if (scrollBar->sliderY < 0)
			scrollBar->sliderY = 0;

		if ((scrollBar->sliderY + scrollBar->sliderHeight) >=
			(component->height - SLIDER_OFFSET_X2))
		{
			scrollBar->sliderY = ((component->height - SLIDER_OFFSET_X2) -
				scrollBar->sliderHeight);
		}
	}

	// Recalculate the position percentage
	calcSliderPosPercent(scrollBar, component->width, component->height);

	draw(component);

	component->window->update(component->window, component->xCoord,
		component->yCoord, component->width, component->height);

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

kernelWindowComponent *kernelWindowNewScrollBar(objectKey parent,
	scrollBarType type, int width, int height, componentParameters *params)
{
	// Formats a kernelWindowComponent as a kernelWindowScrollBar

	kernelWindowComponent *component = NULL;
	kernelWindowScrollBar *scrollBar = NULL;

	// Check params
	if (!parent || !params)
	{
		kernelError(kernel_error, "NULL parameter");
		return (component = NULL);
	}

	// Get the basic component structure
	component = kernelWindowComponentNew(parent, params);
	if (!component)
		return (component);

	component->type = scrollBarComponentType;
	component->flags |= WINDOW_COMP_FLAG_RESIZABLE;

	// Set the functions
	component->draw = &draw;
	component->resize = &resize;
	component->getData = &getData;
	component->setData = &setData;
	component->mouseEvent = &mouseEvent;
	component->destroy = &destroy;

	scrollBar = kernelMalloc(sizeof(kernelWindowScrollBar));
	if (!scrollBar)
	{
		kernelWindowComponentDestroy(component);
		return (component = NULL);
	}

	component->data = (void *) scrollBar;

	if ((type == scrollbar_vertical) && !width)
		width = windowVariables->slider.width;
	if ((type == scrollbar_horizontal) && !height)
		height = windowVariables->slider.width;

	component->width = width;
	component->height = height;

	scrollBar->type = type;
	scrollBar->state.displayPercent = 100;
	scrollBar->state.positionPercent = 0;
	calcSliderSizePos(scrollBar, component->width, component->height);

	component->minWidth = component->width;
	component->minHeight = component->height;

	return (component);
}

