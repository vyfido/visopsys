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

  if (component->parameters.hasBorder)
    component->drawBorder((void *) component);

  return (0);
}


static int erase(void *componentData)
{
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
  return (0);
}


static int destroy(void *componentData)
{
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelTextArea *area = (kernelTextArea *) component->data;

  if (area != NULL)
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


kernelWindowComponent *kernelWindowNewTextArea(kernelWindow *window,
			int columns, int rows, kernelAsciiFont *font)
{
  // Formats a kernelWindowComponent as a kernelWindowTextArea

  int status = 0;
  kernelWindowComponent *component = NULL;
  kernelTextArea *area = NULL;

  // Check parameters.  It's okay for the font to be NULL.
  if (window == NULL)
    return (component = NULL);

  // Get the basic component structure
  component = kernelWindowNewComponent();
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
  component->type = windowTextAreaComponent;
  component->width = (columns * font->charWidth);
  component->height = (rows * font->charHeight);
  // The functions
  component->draw = &draw;
  component->erase = &erase;
  component->move = &move;
  component->resize = &resize;
  component->mouseEvent = NULL;
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
  area->graphicBuffer = &(window->buffer);

  component->data = (void *) area;

  return (component);
}
