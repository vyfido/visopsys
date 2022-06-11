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
//  kernelWindowTextArea.c
//

// This code is for managing kernelWindowTextArea objects.
// These are just textareas that appear inside windows and buttons, etc

#include "kernelWindow.h"     // Our prototypes are here
#include "kernelWindowEventStream.h"
#include "kernelMalloc.h"
#include "kernelMisc.h"
#include <stdlib.h>
#include <string.h>


static inline int isMouseInScrollBar(windowEvent *event,
				     kernelWindowComponent *scrollBar)
{
  // We use this to determine whether a mouse event is inside the slider

  kernelWindow *window = scrollBar->window;

  if (event->xPosition >= (window->xCoord + scrollBar->xCoord))
    return (1);
  else
    return (0);
}


static inline void updateScrollBar(kernelWindowTextArea *textArea)
{
  scrollBarState state;

  if (textArea->scrollBar->setData)
    {
      state.displayPercent =
	((textArea->area->rows * 100) /
	 (textArea->area->rows + textArea->area->scrollBackLines));
      state.positionPercent = 100;
      if (textArea->area->scrolledBackLines)
	state.positionPercent -= ((textArea->area->scrolledBackLines * 100) /
				  textArea->area->scrollBackLines);
      textArea->scrollBar->setData(textArea->scrollBar, &state,
				   sizeof(scrollBarState));
    }
}


static int numComps(kernelWindowComponent *component)
{
  kernelWindowTextArea *textArea = component->data;

  if (textArea->scrollBar)
    // Return 1 for our scrollbar, 
    return (1);
  else
    return (0);
}


static int flatten(kernelWindowComponent *component,
		   kernelWindowComponent **array, int *numItems,
		   unsigned flags)
{
  kernelWindowTextArea *textArea = component->data;

  if (textArea->scrollBar && ((textArea->scrollBar->flags & flags) == flags))
    // Add our scrollbar
    array[*numItems++] = textArea->scrollBar;

  return (0);
}


static int setBuffer(kernelWindowComponent *component,
		     kernelGraphicBuffer *buffer)
{
  // Set the graphics buffer for the component's subcomponents.

  int status = 0;
  kernelWindowTextArea *textArea = component->data;

  if (textArea->scrollBar && textArea->scrollBar->setBuffer)
    {
      // Do our scrollbar
      status = textArea->scrollBar->setBuffer(textArea->scrollBar, buffer);
      textArea->scrollBar->buffer = buffer;
    }

  return (status);
}


static int draw(kernelWindowComponent *component)
{
  // Draw the textArea component

  kernelWindowTextArea *textArea = component->data;
  kernelTextArea *area = textArea->area;

  // Tell the text area to draw itself
  area->outputStream->outputDriver->screenDraw(area);

  // If there's a scroll bar, draw it too
  if (textArea->scrollBar && textArea->scrollBar->draw)
    textArea->scrollBar->draw(textArea->scrollBar);

  if (component->params.flags & WINDOW_COMPFLAG_HASBORDER)
    component->drawBorder(component, 1);

  return (0);
}


static int update(kernelWindowComponent *component)
{
  // This gets called when the text area has done something, and we use
  // it to update the scroll bar.

  kernelWindowTextArea *textArea = component->data;

  if (textArea->scrollBar)
    updateScrollBar(textArea);

  return (0);
}


static int move(kernelWindowComponent *component, int xCoord, int yCoord)
{
  kernelWindowTextArea *textArea = component->data;
  kernelTextArea *area = textArea->area;
  int scrollBarX = 0;

  area->xCoord = xCoord;
  area->yCoord = yCoord;

  // If we have a scroll bar, move it too
  if (textArea->scrollBar)
    {
      scrollBarX = (xCoord + textArea->areaWidth);

      if (textArea->scrollBar->move)
	textArea->scrollBar->move(textArea->scrollBar, scrollBarX, yCoord);

      textArea->scrollBar->xCoord = scrollBarX;
      textArea->scrollBar->yCoord = yCoord;
    }

  return (0);
}


static int resize(kernelWindowComponent *component, int width, int height)
{
  int status = 0;
  kernelWindowTextArea *textArea = component->data;
  kernelTextArea *area = textArea->area;
  int newColumns = 0, newRows = 0;
  int scrollBarX = 0;

  textArea->areaWidth = width;
  if (textArea->scrollBar)
    textArea->areaWidth -= textArea->scrollBar->width;

  // Calculate the new columns and rows.
  newColumns = (textArea->areaWidth / area->font->charWidth);
  newRows = (height / area->font->charHeight);

  if ((newColumns != area->columns) || (newRows != area->rows))
    {
      status = kernelTextAreaResize(area, newColumns, newRows);
      if (status < 0)
	return (status);
    }

  // If we have a scroll bar, move/resize it too
  if (textArea->scrollBar)
    {
      if (width != component->width)
	{
	  scrollBarX = (component->xCoord + textArea->areaWidth);

	  if (textArea->scrollBar->move)
	    textArea->scrollBar->move(textArea->scrollBar, scrollBarX,
				      textArea->scrollBar->yCoord);

	  textArea->scrollBar->xCoord = scrollBarX;
	}

      if (height != component->height)
	{
	  if (textArea->scrollBar->resize)
	    textArea->scrollBar->resize(textArea->scrollBar,
					textArea->scrollBar->width, height);

	  textArea->scrollBar->height = height;
	}
    }

  return (status = 0);
}


static int focus(kernelWindowComponent *component, int yesNo)
{
  kernelWindowTextArea *textArea = component->data;
  kernelTextArea *area = textArea->area;

  if (yesNo)
    {
      kernelTextSetCurrentInput(area->inputStream);
      kernelTextSetCurrentOutput(area->outputStream);
    }

  return (0);
}


static int getData(kernelWindowComponent *component, void *buffer, int size)
{
  // Copy the text (up to size bytes) from the text area to the supplied
  // buffer.
  kernelWindowTextArea *textArea = component->data;
  kernelTextArea *area = textArea->area;

  if (size > (area->columns * area->rows))
    size = (area->columns * area->rows);

  kernelMemCopy(area->visibleData, buffer, size);

  return (0);
}


static int setData(kernelWindowComponent *component, void *buffer, int size)
{
  // Copy the text (up to size bytes) from the supplied buffer to the
  // text area.
  kernelWindowTextArea *textArea = component->data;
  kernelTextArea *area = textArea->area;

  kernelTextStreamScreenClear(area->outputStream);

  if (size)
    kernelTextStreamPrint(area->outputStream, buffer);

  return (0);
}


static int mouseEvent(kernelWindowComponent *component, windowEvent *event)
{
  int status = 0;
  kernelWindowTextArea *textArea = component->data;
  kernelWindowScrollBar *scrollBar = NULL;
  int scrolledBackLines = 0;
  windowEvent cursorEvent;
  int cursorColumn = 0, cursorRow = 0;

  // Is the event in one of our scroll bars?
  if (textArea->scrollBar && isMouseInScrollBar(event, textArea->scrollBar))
    {
      scrollBar = textArea->scrollBar->data;

      // First, pass on the event to the scroll bar
      if (textArea->scrollBar->mouseEvent)
	textArea->scrollBar->mouseEvent(textArea->scrollBar, event);

      scrolledBackLines = (((100 - scrollBar->state.positionPercent) *
			    textArea->area->scrollBackLines) / 100);

      if (scrolledBackLines != textArea->area->scrolledBackLines)
	{
	  // Adjust the scrollback values of the text area based on the
	  // positioning of the scroll bar.
	  textArea->area->scrolledBackLines = scrolledBackLines;
	  component->draw(component);
	}
    }
  else if ((event->type == EVENT_MOUSE_LEFTDOWN) &&
	   (component->params.flags & WINDOW_COMPFLAG_CLICKABLECURSOR))
    {
      // The event was a click in the text area.  Move the cursor to the
      // clicked location.

      cursorColumn = ((event->xPosition - (component->window->xCoord +
					   textArea->area->xCoord)) /
		      textArea->area->font->charWidth);
      cursorColumn = min(cursorColumn, textArea->area->columns);

      cursorRow = ((event->yPosition - (component->window->yCoord +
					textArea->area->yCoord)) /
		   textArea->area->font->charHeight);
      cursorRow = min(cursorRow, textArea->area->rows);

      if (textArea->area && textArea->area->outputStream &&
	  textArea->area->font)
	{
	  kernelTextStreamSetColumn(textArea->area->outputStream,
				    cursorColumn);
	  kernelTextStreamSetRow(textArea->area->outputStream, cursorRow);

	  // Write a 'cursor moved' event to the component event stream
	  kernelMemClear(&cursorEvent, sizeof(windowEvent));
	  cursorEvent.type = EVENT_CURSOR_MOVE;
	  kernelWindowEventStreamWrite(&(component->events), &cursorEvent);
	}
    }

  return (status);
}


static int keyEvent(kernelWindowComponent *component, windowEvent *event)
{
  // Puts window key events into the input stream of the text area

  kernelWindowTextArea *textArea = component->data;
  kernelTextInputStream *inputStream = textArea->area->inputStream;

  if ((event->type == EVENT_KEY_DOWN) && inputStream && inputStream->s.append)
    inputStream->s.append(&(inputStream->s), (char) event->key);

  if (textArea->scrollBar)
    updateScrollBar(textArea);

  return (0);
}


static int destroy(kernelWindowComponent *component)
{
  kernelWindowTextArea *textArea = component->data;

  if (textArea)
    {
      if (textArea->area)
	{
	  // If the current input/output streams are currently pointing at our
	  // input/output streams, set the current ones to NULL
	  if (kernelTextGetCurrentInput() == textArea->area->inputStream)
	    kernelTextSetCurrentInput(NULL);
	  if (kernelTextGetCurrentOutput() == textArea->area->outputStream)
	    kernelTextSetCurrentOutput(NULL);

	  kernelTextAreaDestroy(textArea->area);
	  textArea->area = NULL;
	}

      if (textArea->scrollBar)
	{
	  kernelWindowComponentDestroy(textArea->scrollBar);
	  textArea->scrollBar = NULL;
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


kernelWindowComponent *kernelWindowNewTextArea(objectKey parent, int columns,
					       int rows, int bufferLines,
					       componentParameters *params)
{
  // Formats a kernelWindowComponent as a kernelWindowTextArea

  int status = 0;
  kernelWindow *window = NULL;
  kernelWindowComponent *component = NULL;
  kernelWindowTextArea *textArea = NULL;
  componentParameters subParams;

  // Check parameters.
  if ((parent == NULL) || (params == NULL))
    return (component = NULL);

  window = getWindow(parent);

  // Get the basic component structure
  component = kernelWindowComponentNew(parent, params);
  if (component == NULL)
    return (component);

  // If the user wants the default colors, we change set them to the
  // default for a text area
  if (!(component->params.flags & WINDOW_COMPFLAG_CUSTOMBACKGROUND))
    {
      component->params.background.blue = 0xFF;
      component->params.background.green = 0xFF;
      component->params.background.red = 0xFF;
    }

  // If font is NULL, get the default font
  if (component->params.font == NULL)
    {
      status = kernelFontGetDefault((kernelAsciiFont **)
				    &(component->params.font));
      if (status < 0)
	return (component = NULL);
    }

  // Get memory for the kernelWindowTextArea
  textArea = kernelMalloc(sizeof(kernelWindowTextArea));
  if (textArea == NULL)
    {
      kernelFree((void *) component);
      return (component = NULL);
    }

  // Create the text area inside it
  textArea->area = kernelTextAreaNew(columns, rows, 1, bufferLines);
  if (textArea->area == NULL)
    {
      kernelFree((void *) textArea);
      kernelFree((void *) component);
      return (component = NULL);
    }

  // Set some values
  textArea->area->foreground.red = component->params.foreground.red;
  textArea->area->foreground.green = component->params.foreground.green;
  textArea->area->foreground.blue = component->params.foreground.blue;
  textArea->area->background.red = component->params.background.red;
  textArea->area->background.green = component->params.background.green;
  textArea->area->background.blue = component->params.background.blue;
  textArea->area->font = (kernelAsciiFont *) component->params.font;
  textArea->area->windowComponent = (void *) component;
  textArea->areaWidth =
    (columns * ((kernelAsciiFont *) component->params.font)->charWidth);

  // Populate the rest of the component fields
  component->type = textAreaComponentType;
  component->width = textArea->areaWidth;
  component->height =
    (rows * ((kernelAsciiFont *) component->params.font)->charHeight);
  component->flags |= (WINFLAG_CANFOCUS | WINFLAG_RESIZABLE);

  // If there are any buffer lines, we need a scroll bar as well.
  if (bufferLines)
    {
      // Standard parameters for a scroll bar
      kernelMemCopy(params, &subParams, sizeof(componentParameters));
      subParams.flags &=
	~(WINDOW_COMPFLAG_CUSTOMFOREGROUND | WINDOW_COMPFLAG_CUSTOMBACKGROUND);

      textArea->scrollBar =
	kernelWindowNewScrollBar(parent, scrollbar_vertical, 0,
				 component->height, &subParams);
      if (textArea->scrollBar == NULL)
	{
	  kernelTextAreaDestroy(textArea->area);
	  kernelFree((void *) textArea);
	  kernelFree((void *) component);
	  return (component = NULL);
	}

      // Remove the scrollbar from the parent container
      if (((kernelWindow *) parent)->type == windowType)
	{
	  kernelWindowContainer *tmpContainer = window->mainContainer->data;
	  if (tmpContainer->remove)
	    tmpContainer->remove(window->mainContainer, textArea->scrollBar);
	}
      else
	{
	  kernelWindowComponent *tmpComponent = parent;
	  kernelWindowContainer *tmpContainer = tmpComponent->data;
	  if (tmpContainer->remove)
	    tmpContainer->remove(tmpComponent, textArea->scrollBar);
	}

      textArea->scrollBar->xCoord = component->width;
      component->width += textArea->scrollBar->width;
    }

  // After our width and height are finalized...
  component->minWidth = component->width;
  component->minHeight = component->height;

  // The functions
  component->numComps = &numComps;
  component->flatten = &flatten;
  component->setBuffer = &setBuffer;
  component->draw = &draw;
  component->update = &update;
  component->move = &move;
  component->resize = &resize;
  component->focus = &focus;
  component->getData = &getData;
  component->setData = &setData;
  component->mouseEvent = &mouseEvent;
  component->keyEvent = &keyEvent;
  component->destroy = &destroy;

  component->data = (void *) textArea;

  return (component);
}
