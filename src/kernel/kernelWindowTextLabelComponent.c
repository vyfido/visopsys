//
//  Visopsys
//  Copyright (C) 1998-2003 J. Andrew McLaughlin
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
//  kernelWindowTextLabelComponent.c
//

// This code is for managing kernelWindowTextLabelComponent objects.
// These are just lines of text that occur inside windows


#include "kernelWindowManager.h"     // Our prototypes are here
#include "kernelMemoryManager.h"
#include <string.h>


static kernelAsciiFont *labelFont = NULL;


static int draw(void *componentData)
{
  // Draw the label component

  int status = 0;
  color foreground = { 0, 0, 0 };
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindow *window = (kernelWindow *) component->window;
  kernelWindowTextLabelComponent *label = (kernelWindowTextLabelComponent *)
    component->data;

  if (!component->parameters.useDefaultForeground)
    {
      // Use the user-supplied foreground color
      foreground.red = component->parameters.foreground.red;
      foreground.green = component->parameters.foreground.green;
      foreground.blue = component->parameters.foreground.blue;
    }

  status = kernelGraphicDrawText(&(window->buffer), &foreground,
				 label->font, label->text, draw_normal,
				 component->xCoord, component->yCoord);

  if (component->parameters.hasBorder)
    component->drawBorder((void *) component);

  return (status);
}


static int erase(void *componentData)
{
  return (0);
}


static int destroy(void *componentData)
{
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelWindowTextLabelComponent *label = (kernelWindowTextLabelComponent *)
    component->data;

  // Release all our memory
  if (label != NULL)
    {
      if (label->text != NULL)
	kernelMemoryReleaseSystemBlock((void *) label->text);
      kernelMemoryReleaseSystemBlock((void *) label);
    }
  kernelMemoryReleaseSystemBlock((void *) component);

  return (0);
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


kernelWindowComponent *kernelWindowNewTextLabelComponent(kernelWindow *window,
			 kernelAsciiFont *font, const char *text)
{
  // Formats a kernelWindowComponent as a kernelWindowTextLabelComponent

  int status = 0;
  kernelWindowComponent *component = NULL;
  kernelWindowTextLabelComponent *textLabelComponent = NULL;

  // Check parameters.  It's okay for the font to be NULL.
  if ((window == NULL) || (text == NULL))
    return (component = NULL);

  // Get the basic component structure
  component = kernelWindowNewComponent();
  if (component == NULL)
    return (component);

  if (labelFont == NULL)
    {
      // Try to load a nice-looking font
      status = kernelFontLoad(DEFAULT_VARIABLEFONT_SMALL_FILE,
			      DEFAULT_VARIABLEFONT_SMALL_NAME, &labelFont);
      if (status < 0)
	{
	  // Font's not there, we suppose.  There's always a default.
	  kernelFontGetDefault(&labelFont);
	}
    }

  // If font is NULL, use the default
  if (font == NULL)
    font = labelFont;

  // Now populate it
  component->type = windowTextLabelComponent;
  component->width = kernelFontGetPrintedWidth(font, text);
  component->height = font->charHeight;

  // The label data
  textLabelComponent =
    kernelMemoryRequestSystemBlock(sizeof(kernelWindowTextLabelComponent), 0,
				   "text label component");
  if (textLabelComponent == NULL)
    {
      kernelMemoryReleaseSystemBlock((void *) component);
      return (component = NULL);
    }
  textLabelComponent->text =
    kernelMemoryRequestSystemBlock(strlen(text), 0, "text label data");
  if (textLabelComponent->text == NULL)
    {
      kernelMemoryReleaseSystemBlock((void *) textLabelComponent);
      kernelMemoryReleaseSystemBlock((void *) component);
      return (component = NULL);
    }
  strcpy((char *) textLabelComponent->text, text);
  textLabelComponent->font = font;

  // The functions
  component->draw = &draw;
  component->mouseEvent = NULL;
  component->erase = &erase;
  component->destroy = &destroy;

  component->data = (void *) textLabelComponent;

  return (component);
}
