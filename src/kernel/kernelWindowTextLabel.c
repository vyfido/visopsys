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
//  kernelWindowTextLabel.c
//

// This code is for managing kernelWindowTextLabel objects.
// These are just lines of text that occur inside windows


#include "kernelWindow.h"     // Our prototypes are here
#include "kernelMalloc.h"
#include "kernelError.h"
#include <string.h>

static kernelAsciiFont *labelFont = NULL;


static int setText(kernelWindowComponent *component, const char *text,
		   int length)
{
  // Set the label text
  
  int status = 0;
  kernelWindowTextLabel *label = component->data;
  int count;

  // Set the text
  if (label->text)
    {
      kernelFree((void *) label->text);
      label->text = NULL;
    }

  label->text = kernelMalloc(length + 1);
  if (label->text == NULL)
    return (status = ERR_NOCREATE);

  strncpy((char *) label->text, text, length);

  // How many lines?  We replace any newlines with NULLS and count them
  label->lines = 1;
  for (count = 0; count < length; count ++)
    if (label->text[count] == '\n')
      {
	label->text[count] = '\0';
	label->lines += 1;
      }

  // Set the width and height of the component based on the widest line and
  // the number of lines, respectively
  char *tmp  = label->text;
  for (count = 0; count < label->lines; count ++)
    {
      int width = kernelFontGetPrintedWidth((kernelAsciiFont *)
					    component->parameters.font, tmp);
      if (width > component->width)
	component->width = width;
      
      tmp += (strlen(tmp) + 1);
    }

  component->height = (((kernelAsciiFont *) component->parameters.font)
			->charHeight * label->lines);
  component->minWidth = component->width;
  component->minHeight = component->height;

  return (status = 0);
}


static int draw(kernelWindowComponent *component)
{
  // Draw the label component

  int status = 0;
  kernelWindowTextLabel *label = component->data;
  int count;

  char *tmp = label->text;
  for (count = 0; count < label->lines; count ++)
    {
      status =
	kernelGraphicDrawText(component->buffer,
			      (color *) &(component->parameters.foreground),
			      (color *) &(component->parameters.background),
			      (kernelAsciiFont *) component->parameters.font,
			      tmp, draw_normal, component->xCoord,
			      (component->yCoord +
			      (((kernelAsciiFont *) component->parameters.font)
				->charHeight * count)));
      if (status < 0)
	break;

      tmp += (strlen(tmp) + 1);
    }

  if (component->parameters.flags & WINDOW_COMPFLAG_HASBORDER)
    component->drawBorder(component, 1);

  return (status);
}


static int setData(kernelWindowComponent *component, void *text, int length)
{
  // Set the label text

  int status = 0;

  if (component->erase)
    component->erase(component);

  status = setText(component, text, length);
  if (status < 0)
    return (status);

  draw(component);

  component->window
    ->update(component->window, component->xCoord, component->yCoord,
	     component->width, component->height);

  return (status = 0);
}


static int destroy(kernelWindowComponent *component)
{
  kernelWindowTextLabel *label = component->data;

  // Release all our memory
  if (label)
    {
      if (label->text)
	{
	  kernelFree((void *) label->text);
	  label->text = NULL;
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


kernelWindowComponent *kernelWindowNewTextLabel(objectKey parent,
						const char *text,
						componentParameters *params)
{
  // Formats a kernelWindowComponent as a kernelWindowTextLabel

  int status = 0;
  kernelWindowComponent *component = NULL;
  kernelWindowTextLabel *textLabelComponent = NULL;

  // Check parameters.
  if ((parent == NULL) || (text == NULL) || (params == NULL))
    return (component = NULL);

  // Get the basic component structure
  component = kernelWindowComponentNew(parent, params);
  if (component == NULL)
    return (component);

  if (labelFont == NULL)
    {
      // Try to load a nice-looking font
      status =
	kernelFontLoad(WINDOW_DEFAULT_VARFONT_MEDIUM_FILE,
		       WINDOW_DEFAULT_VARFONT_MEDIUM_NAME, &labelFont, 0);
      if (status < 0)
	// Font's not there, we suppose.  There's always a default.
	kernelFontGetDefault(&labelFont);
    }

  // If font is NULL, use the default
  if (component->parameters.font == NULL)
    component->parameters.font = labelFont;

  // Now populate it
  component->type = textLabelComponentType;

  // The functions
  component->draw = &draw;
  component->setData = &setData;
  component->destroy = &destroy;

  // Get the label component
  textLabelComponent = kernelMalloc(sizeof(kernelWindowTextLabel));
  if (textLabelComponent == NULL)
    {
      kernelFree((void *) component);
      return (component = NULL);
    }

  component->data = (void *) textLabelComponent;

  // Set the label data
  status = setText(component, text, strlen(text));
  if (status < 0)
    {
      kernelFree((void *) textLabelComponent);
      kernelFree((void *) component);
      return (component = NULL);
    }

  return (component);
}
