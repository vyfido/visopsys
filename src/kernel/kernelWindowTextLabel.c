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
//  kernelWindowTextLabel.c
//

// This code is for managing kernelWindowTextLabel objects.
// These are just lines of text that occur inside windows


#include "kernelWindowManager.h"     // Our prototypes are here
#include "kernelMalloc.h"
#include <string.h>
#include <sys/errors.h>

static kernelAsciiFont *labelFont = NULL;


static int setText(void *componentData, const char *text, unsigned length)
{
  // Set the label text
  
  int status = 0;
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindowTextLabel *label = (kernelWindowTextLabel *) component->data;
  int count;

  // Set the text
  if (label->text)
    kernelFree((void *) label->text);

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
      unsigned width = kernelFontGetPrintedWidth(label->font, tmp);
      if (width > component->width)
	component->width = width;
      
      tmp += (strlen(tmp) + 1);
    }

  component->height = (label->font->charHeight * label->lines);

  return (status = 0);
}


static int draw(void *componentData)
{
  // Draw the label component

  int status = 0;
  color foreground = { DEFAULT_BLUE, DEFAULT_GREEN, DEFAULT_RED };
  color background = { DEFAULT_GREY, DEFAULT_GREY, DEFAULT_GREY };
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindow *window = (kernelWindow *) component->window;
  kernelWindowTextLabel *label = (kernelWindowTextLabel *) component->data;
  int count;

  if (!component->parameters.useDefaultForeground)
    {
      // Use the user-supplied foreground color
      foreground.red = component->parameters.foreground.red;
      foreground.green = component->parameters.foreground.green;
      foreground.blue = component->parameters.foreground.blue;
    }
  if (!component->parameters.useDefaultBackground)
    {
      // Use the user-supplied foreground color
      background.red = component->parameters.background.red;
      background.green = component->parameters.background.green;
      background.blue = component->parameters.background.blue;
    }

  char *tmp = label->text;
  for (count = 0; count < label->lines; count ++)
    {
      status =
	kernelGraphicDrawText(&(window->buffer), &foreground, &background,
			      label->font, tmp, draw_normal,
			      component->xCoord,
			      (component->yCoord + (label->font->charHeight *
						    count)));
      if (status < 0)
	break;

      tmp += (strlen(tmp) + 1);
    }

  if (component->parameters.hasBorder)
    component->drawBorder((void *) component);

  return (status);
}


static int setData(void *componentData, void *text, unsigned length)
{
  // Set the label text
  
  int status = 0;
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindow *window = (kernelWindow *) component->window;

  if (component->erase)
    component->erase(componentData);

  kernelWindowUpdateBuffer(&(window->buffer), component->xCoord,
			   component->yCoord, component->width,
			   component->height);

  status = setText(componentData, text, length);
  if (status < 0)
    return (status);

  if (component->draw)
    status = component->draw(componentData);

  kernelWindowUpdateBuffer(&(window->buffer), component->xCoord,
			   component->yCoord, component->width,
			   component->height);
  return (status);
}


static int destroy(void *componentData)
{
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindowTextLabel *label = (kernelWindowTextLabel *) component->data;

  // Release all our memory
  if (label)
    {
      if (label->text)
	kernelFree((void *) label->text);
      kernelFree((void *) label);
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


kernelWindowComponent *kernelWindowNewTextLabel(volatile void *parent,
						kernelAsciiFont *font,
						const char *text,
						componentParameters *params)
{
  // Formats a kernelWindowComponent as a kernelWindowTextLabel

  int status = 0;
  kernelWindowComponent *component = NULL;
  kernelWindowTextLabel *textLabelComponent = NULL;

  // Check parameters.  It's okay for the font to be NULL.
  if ((parent == NULL) || (text == NULL) || (params == NULL))
    return (component = NULL);

  // Get the basic component structure
  component = kernelWindowComponentNew(parent, params);
  if (component == NULL)
    return (component);

  if (labelFont == NULL)
    {
      // Try to load a nice-looking font
      status = kernelFontLoad(DEFAULT_VARIABLEFONT_MEDIUM_FILE,
			      DEFAULT_VARIABLEFONT_MEDIUM_NAME, &labelFont);
      if (status < 0)
	// Font's not there, we suppose.  There's always a default.
	kernelFontGetDefault(&labelFont);
    }

  // If font is NULL, use the default
  if (font == NULL)
    font = labelFont;

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

  // Set the font
  textLabelComponent->font = font;

  // Set the label data
  status = setText((void *) component, text, strlen(text));
  if (status < 0)
    {
      kernelFree((void *) textLabelComponent);
      kernelFree((void *) component);
      return (component = NULL);
    }

  return (component);
}
