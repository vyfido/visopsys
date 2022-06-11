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
//  kernelWindowTextField.c
//

// This code is for managing kernelWindowTextField components.
// These are just kernelWindowTextArea that consist of a single line

#include "kernelWindowManager.h"     // Our prototypes are here
#include "kernelMiscFunctions.h"
#include <string.h>

static int (*saveFocus) (void *, int) = NULL;
static int (*saveKeyEvent) (void *, windowEvent *) = NULL;


static int focus(void *componentData, int focus)
{
  // This gets called when a component gets or loses the focus

  int status = 0;
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelTextArea *textArea = ((kernelWindowTextArea *) component->data)->area;

  // Call the 'focus' routine of the underlying text area
  status = saveFocus(componentData, focus);
  if (status < 0)
    return (status);

  kernelTextStreamSetCursor(((kernelTextOutputStream *)
			     textArea->outputStream), focus);
  return (0);
}


static int keyEvent(void *componentData, windowEvent *event)
{
  kernelWindowComponent *component = (kernelWindowComponent *) componentData;
  kernelTextArea *textArea = ((kernelWindowTextArea *) component->data)->area;

  if (event->type == EVENT_KEY_DOWN)
    {
      if (event->key == 8)
	kernelTextStreamBackSpace(textArea->outputStream);

      else if ((event->key >= 32) && (event->key <= 126))
	kernelTextStreamPutc(textArea->outputStream, event->key);
    }

  if (saveKeyEvent)
    return (saveKeyEvent(componentData, event));
  else
    return (0);
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


kernelWindowComponent *kernelWindowNewTextField(volatile void *parent,
						int columns,
						componentParameters *params)
{
  // Just returns a kernelWindowTextArea with only one row, but there are
  // a couple of other things we do as well.

  kernelWindowComponent *component = NULL;
  kernelTextArea *textArea = NULL;
  componentParameters newParams;
  extern color kernelDefaultForeground;

  kernelMemCopy(params, &newParams, sizeof(componentParameters));
  params = &newParams;

  // If the user wants the default colors, we change set them to the
  // default for a text field, since it's different from text areas
  if (params->useDefaultForeground)
    {
      params->foreground.red = kernelDefaultForeground.red;
      params->foreground.green = kernelDefaultForeground.green;
      params->foreground.blue = kernelDefaultForeground.blue;
      params->useDefaultForeground = 0;
    }
  if (params->useDefaultBackground)
    {
      params->background.red = 0xFF;
      params->background.green = 0xFF;
      params->background.blue = 0xFF;
      params->useDefaultBackground = 0;
    }
  
  component = kernelWindowNewTextArea(parent, columns, 1, 0, params);
  if (component == NULL)
    return (component);

  textArea = ((kernelWindowTextArea *) component->data)->area;

  // Turn off the cursor until we get the focus
  textArea->cursorState = 0;

  // Turn echo off
  ((kernelTextInputStream *) textArea->inputStream)->echo = 0;

  // We want different focus behaviour than a text area
  saveFocus = component->focus;
  component->focus = focus;

  // Change the key event handler
  saveKeyEvent = component->keyEvent;
  component->keyEvent = keyEvent;

  return (component);
}
