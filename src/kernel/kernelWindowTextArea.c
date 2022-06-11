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
//  kernelWindowTextArea.c
//

// This code is for managing kernelWindowTextArea objects.
// These are just textareas that appear inside windows and buttons, etc


#include "kernelWindow.h"     // Our prototypes are here
#include "kernelMalloc.h"
#include "kernelMiscFunctions.h"
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


static int draw(void *componentData)
{
  // Draw the textArea component

  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindowTextArea *textArea = (kernelWindowTextArea *) component->data;
  kernelTextArea *area = textArea->area;

  // Tell the text area to draw itself
  ((kernelTextOutputStream *) area->outputStream)->outputDriver
    ->screenDraw(area);

  // If there's a scroll bar, draw it too
  if (textArea->scrollBar && textArea->scrollBar->draw)
    textArea->scrollBar->draw((void *) textArea->scrollBar);

  if (component->parameters.hasBorder)
    component->drawBorder((void *) component, 1);

  return (0);
}


static int move(void *componentData, int xCoord, int yCoord)
{
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindowTextArea *textArea = (kernelWindowTextArea *) component->data;
  kernelTextArea *area = textArea->area;
  int scrollBarX = 0;

  area->xCoord = xCoord;
  area->yCoord = yCoord;

  // If we have a scroll bar, move it too
  if (textArea->scrollBar)
    {
      scrollBarX = (xCoord + textArea->areaWidth);

      if (textArea->scrollBar->move)
	textArea->scrollBar->move((void *) textArea->scrollBar, scrollBarX,
				  yCoord);

      textArea->scrollBar->xCoord = scrollBarX;
      textArea->scrollBar->yCoord = yCoord;
    }

  return (0);
}


static int resize(void *componentData, int width, int height)
{
  int status = 0;
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindowTextArea *textArea = (kernelWindowTextArea *) component->data;
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
	    textArea->scrollBar->move((void *) textArea->scrollBar, scrollBarX,
				      textArea->scrollBar->yCoord);

	  textArea->scrollBar->xCoord = scrollBarX;
	}

      if (height != component->height)
	{
	  if (textArea->scrollBar->resize)
	    textArea->scrollBar->resize((void *) textArea->scrollBar,
					textArea->scrollBar->width, height);

	  textArea->scrollBar->height = height;
	}
    }

  return (status = 0);
}


static int focus(void *componentData, int focus)
{
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelTextArea *area = ((kernelWindowTextArea *) component->data)->area;

  if (focus)
    {
      kernelTextSetCurrentInput(area->inputStream);
      kernelTextSetCurrentOutput(area->outputStream);
    }

  return (0);
}


static int getData(void *componentData, void *buffer, int size)
{
  // Copy the text (up to size bytes) from the text area to the supplied
  // buffer.
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelTextArea *area = ((kernelWindowTextArea *) component->data)->area;

  if (size > (area->columns * area->rows))
    size = (area->columns * area->rows);

  kernelMemCopy(area->visibleData, buffer, size);

  return (0);
}


static int setData(void *componentData, void *buffer, int size)
{
  // Copy the text (up to size bytes) from the supplied buffer to the
  // text area.
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelTextArea *area = ((kernelWindowTextArea *) component->data)->area;

  if (size > (area->columns * area->rows))
    size = (area->columns * area->rows);

  kernelTextStreamScreenClear(area->outputStream);
  kernelTextStreamPrint(area->outputStream, buffer);

  return (0);
}


static int mouseEvent(void *componentData, windowEvent *event)
{
  int status = 0;
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindowTextArea *textArea = (kernelWindowTextArea *) component->data;
  kernelWindowScrollBar *scrollBar = NULL;
  int scrolledBackLines = 0;

  // Is the event in one our scroll bar?
  if (textArea->scrollBar && isMouseInScrollBar(event, textArea->scrollBar) &&
      textArea->scrollBar->mouseEvent)
    {
      scrollBar = (kernelWindowScrollBar *) textArea->scrollBar->data;;

      // First, pass on the event to the scroll bar
      status = textArea->scrollBar
	->mouseEvent((void *) textArea->scrollBar, event);
      if (status < 0)
	return (status);

      scrolledBackLines = (((100 - scrollBar->state.positionPercent) *
	  textArea->area->scrollBackLines) / 100);

      if (scrolledBackLines != textArea->area->scrolledBackLines)
	{
	  // Adjust the scrollback values of the text area based on the
	  // positioning of the scroll bar.
	  textArea->area->scrolledBackLines = scrolledBackLines;
	  component->draw(componentData);
	}
    }

  return (status);
}


static int keyEvent(void *componentData, windowEvent *event)
{
  // Puts window key events into the input stream of the text area

  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindowTextArea *textArea = (kernelWindowTextArea *) component->data;
  kernelTextInputStream *inputStream =
    (kernelTextInputStream *) textArea->area->inputStream;
  scrollBarState state;

  if ((event->type == EVENT_KEY_DOWN) && inputStream && inputStream->s.append)
    inputStream->s.append(textArea->area->inputStream, (char) event->key);

  if (textArea->scrollBar && textArea->scrollBar->setData)
    {
      state.displayPercent =
	((textArea->area->rows * 100) /
	 (textArea->area->rows + textArea->area->scrollBackLines));
      state.positionPercent = 100;
      textArea->scrollBar->setData((void *) textArea->scrollBar, &state,
      				   sizeof(scrollBarState));
    }

  return (0);
}


static int destroy(void *componentData)
{
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindowTextArea *textArea = (kernelWindowTextArea *) component->data;

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


kernelWindowComponent *kernelWindowNewTextArea(volatile void *parent,
					       int columns, int rows,
					       int bufferLines,
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
  if (component->parameters.useDefaultForeground)
    {
      component->parameters.foreground.red = 0;
      component->parameters.foreground.green = 0;
      component->parameters.foreground.blue = 0;
    }

  // If font is NULL, get the default font
  if (component->parameters.font == NULL)
    {
      status = kernelFontGetDefault((kernelAsciiFont **)
				    &(component->parameters.font));
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
  textArea->area->foreground.red = component->parameters.foreground.red;
  textArea->area->foreground.green = component->parameters.foreground.green;
  textArea->area->foreground.blue = component->parameters.foreground.blue;
  textArea->area->background.red = component->parameters.background.red;
  textArea->area->background.green = component->parameters.background.green;
  textArea->area->background.blue = component->parameters.background.blue;
  textArea->area->font = component->parameters.font;
  textArea->area->graphicBuffer = &(window->buffer);
  textArea->areaWidth =
    (columns * ((kernelAsciiFont *) component->parameters.font)->charWidth);

  // Populate the rest of the component fields
  component->type = textAreaComponentType;
  component->width = textArea->areaWidth;
  component->height =
    (rows * ((kernelAsciiFont *) component->parameters.font)->charHeight);
  component->flags |= (WINFLAG_CANFOCUS | WINFLAG_RESIZABLE);

  // If there are any buffer lines, we need a scroll bar as well.
  if (bufferLines)
    {
      // Standard parameters for a scroll bar
      kernelMemCopy(params, &subParams, sizeof(componentParameters));
      subParams.useDefaultForeground = 1;
      subParams.useDefaultBackground = 1;

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
	  kernelWindowContainer *tmpContainer =
	    (kernelWindowContainer *) window->mainContainer->data;
	  tmpContainer->containerRemove(window->mainContainer,
					textArea->scrollBar);
	}
      else
	{
	  kernelWindowContainer *tmpContainer = (kernelWindowContainer *)
	    ((kernelWindowComponent *) parent)->data;
	  tmpContainer->containerRemove((kernelWindowComponent *) parent,
					textArea->scrollBar);
	}

      textArea->scrollBar->xCoord = component->width;
      component->width += textArea->scrollBar->width;
    }

  // After our width and height are finalized...
  component->minWidth = component->width;
  component->minHeight = component->height;

  // The functions
  component->draw = &draw;
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
