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
//  kernelWindowTextArea.c
//

// This code is for managing kernelWindowTextArea objects.
// These are just textareas that appear inside windows and buttons, etc


#include "kernelWindowManager.h"     // Our prototypes are here
#include "kernelMalloc.h"
#include "kernelMultitasker.h"
#include "kernelMiscFunctions.h"
#include <sys/errors.h>
#include <string.h>


static int draw(void *componentData)
{
  // Draw the textArea component

  // Not sure what is the right thing to do here.  We might be able to do
  // some sort of "refresh", but normally the kernelTextArea keeps redrawing
  // itself all the time.

  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelTextArea *area = (kernelTextArea *) component->data;
  unsigned char *data = area->data;
  unsigned char *line = NULL;
  int rowCount;

  if (!component->parameters.useDefaultForeground)
    {
      area->foreground.red = component->parameters.foreground.red;
      area->foreground.green = component->parameters.foreground.green;
      area->foreground.blue = component->parameters.foreground.blue;
    }
  if (!component->parameters.useDefaultBackground)
    {
      area->background.red = component->parameters.background.red;
      area->background.green = component->parameters.background.green;
      area->background.blue = component->parameters.background.blue;
    }

  // Clear the area visually
  kernelGraphicClearArea(area->graphicBuffer, (color *) &(area->background),
			 component->xCoord, component->yCoord,
			 component->width, component->height);
  area->cursorColumn = 0;
  area->cursorRow = 0;

  // Loop through contents of the data area and print them.

  line = kernelMalloc(area->columns + 1);
  if (line == NULL)
    return (ERR_MEMORY);

  for (rowCount = 0; rowCount < area->rows; rowCount ++)
    {
      // Copy up to a screen line of data into our buffer
      strncpy(line, (data + (rowCount * area->columns)), area->columns);
      line[area->columns] = '\0';
      kernelTextStreamPrint(area->outputStream, line);
    }

  // Free our temporary memory
  kernelFree(line);

  kernelTextStreamSetCursor(area->outputStream,
			    (component->flags & WINFLAG_HASFOCUS));

  if (component->parameters.hasBorder)
    component->drawBorder((void *) component);

  return (0);
}


static int move(void *componentData, int xCoord, int yCoord)
{
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelTextArea *area = (kernelTextArea *) component->data;

  area->xCoord = xCoord;
  area->yCoord = yCoord;

  return (0);
}


static int resize(void *componentData, unsigned width, unsigned height)
{
  int status = 0;
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelTextArea *area = (kernelTextArea *) component->data;
  unsigned oldColumns = 0, oldRows = 0;
  unsigned char *oldBuffer = NULL;
  unsigned char *oldLine = NULL;
  unsigned rowCount;

  oldColumns = area->columns;
  oldRows = area->rows;
  oldBuffer = area->data;

  // Set the new columns and rows.
  area->columns = (width / area->font->charWidth);
  area->rows = (height / area->font->charHeight);
  area->cursorColumn = 0;
  area->cursorRow = 0;

  area->data = kernelMalloc(area->columns * area->rows);
  if (area->data == NULL)
    return (status = ERR_MEMORY);

  for (rowCount = 0; ((rowCount < oldRows) &&
		      (rowCount < area->rows)); rowCount ++)
    {
      oldLine = (oldBuffer + (rowCount * oldColumns));
      kernelTextStreamPrint(area->outputStream, oldLine);
    }

  // Free the old data buffer and assign the new one
  kernelFree(oldBuffer);

  return (status = 0);
}


static int focus(void *componentData, int focus)
{
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelTextArea *area = (kernelTextArea *) component->data;

  if (focus)
    {
      kernelTextSetCurrentInput(area->inputStream);
      kernelTextSetCurrentOutput(area->outputStream);
    }

  return (0);
}


static int getData(void *componentData, void *buffer, unsigned size)
{
  // Copy the text (up to size bytes) from the text area to the supplied
  // buffer.
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelTextArea *area = (kernelTextArea *) component->data;

  if (size > (area->columns * area->rows))
    size = (area->columns * area->rows);

  kernelMemCopy(area->data, buffer, size);

  return (0);
}


static int setData(void *componentData, void *buffer, unsigned size)
{
  // Copy the text (up to size bytes) from the supplied buffer to the
  // text area.
  int status = 0;
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelTextArea *area = (kernelTextArea *) component->data;

  if (size > (area->columns * area->rows))
    size = (area->columns * area->rows);

  kernelTextStreamScreenClear(area->outputStream);

  kernelMemCopy(buffer, area->data, size);

  if (component->draw)
    status = component->draw((void *) component);

  return (status);
}


static int keyEvent(void *componentData, windowEvent *event)
{
  // Puts window key events into the input stream of the text area

  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelTextArea *area = (kernelTextArea *) component->data;
  kernelTextInputStream *inputStream =
    (kernelTextInputStream *) area->inputStream;

  if ((event->type & EVENT_KEY_DOWN) && inputStream && inputStream->s.append)
    inputStream->s.append(area->inputStream, (char) event->key);

  return (0);
}


static int destroy(void *componentData)
{
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelTextArea *area = (kernelTextArea *) component->data;

  if (area)
    {
      // If the current input/output streams are currently pointing at our
      // input/output streams, set the current ones to NULL
      if (kernelTextGetCurrentInput() == area->inputStream)
	kernelTextSetCurrentInput(NULL);
      if (kernelTextGetCurrentOutput() == area->outputStream)
	kernelTextSetCurrentOutput(NULL);

      // Release all of our memory
      kernelFree((void *)(((kernelTextInputStream *)
			   area->inputStream)->s.buffer));
      kernelFree(area->inputStream);
      kernelFree(area->outputStream);
      kernelFree(area->data);
      kernelFree((void *) area);
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
					       kernelAsciiFont *font,
					       componentParameters *params)
{
  // Formats a kernelWindowComponent as a kernelWindowTextArea

  int status = 0;
  kernelWindowComponent *component = NULL;
  kernelTextArea *area = NULL;

  // Check parameters.  It's okay for the font to be NULL.
  if ((parent == NULL) || (params == NULL))
    return (component = NULL);

  // Get the basic component structure
  component = kernelWindowComponentNew(parent, params);
  if (component == NULL)
    return (component);

  // If font is NULL, get the default font
  if (font == NULL)
    {
      status = kernelFontGetDefault(&font);
      if (status < 0)
	// Couldn't get the default font
	return (component = NULL);
    }

  // Now populate it
  component->type = textAreaComponentType;
  component->width = (columns * font->charWidth);
  component->height = (rows * font->charHeight);
  component->flags |= (WINFLAG_CANFOCUS | WINFLAG_RESIZABLE);

  // The functions
  component->draw = &draw;
  component->move = &move;
  component->resize = &resize;
  component->focus = &focus;
  component->getData = &getData;
  component->setData = &setData;
  component->keyEvent = &keyEvent;
  component->destroy = &destroy;

  // Create the text area
  area = kernelMalloc(sizeof(kernelTextArea));
  if (area == NULL)
    {
      kernelFree((void *) component);
      return (component = NULL);
    }

  area->columns = columns;
  area->rows = rows;
  area->cursorColumn = 0;
  area->cursorRow = 0;
  area->foreground.red = 0;
  area->foreground.green = 0;
  area->foreground.blue = 0;
  area->background.red = DEFAULT_GREY;
  area->background.green = DEFAULT_GREY;
  area->background.blue = DEFAULT_GREY;
  area->inputStream = kernelMalloc(sizeof(kernelTextInputStream));
  kernelTextNewInputStream(area->inputStream);
  area->outputStream = kernelMalloc(sizeof(kernelTextOutputStream));
  kernelTextNewOutputStream(area->outputStream);
  ((kernelTextOutputStream *) area->outputStream)->textArea = area;
  area->data = (unsigned char *) kernelMalloc(columns * rows);
  area->font = font;
  area->graphicBuffer = &(getWindow(parent)->buffer);

  component->data = (void *) area;

  return (component);
}
