//
//  Visopsys
//  Copyright (C) 1998-2007 J. Andrew McLaughlin
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

#include "kernelWindow.h"     // Our prototypes are here
#include "kernelMisc.h"
#include <string.h>

static int (*saveFocus) (kernelWindowComponent *, int) = NULL;
static int (*saveKeyEvent) (kernelWindowComponent *, windowEvent *) = NULL;


static int focus(kernelWindowComponent *component, int yesNo)
{
  // This gets called when a component gets or loses the focus

  int status = 0;
  kernelWindowTextArea *textArea = component->data;
  kernelTextArea *area = textArea->area;

  // Call the 'focus' routine of the underlying text area
  status = saveFocus(component, yesNo);
  if (status < 0)
    return (status);

  kernelTextStreamSetCursor(area->outputStream, yesNo);
  return (0);
}


static int keyEvent(kernelWindowComponent *component, windowEvent *event)
{
  kernelWindowTextArea *textArea = component->data;
  kernelTextArea *area = textArea->area;

  if (event->type == EVENT_KEY_DOWN)
    {
      if (event->key == 8)
	kernelTextStreamBackSpace(area->outputStream);

      else if ((event->key >= 32) && (event->key <= 126))
	kernelTextStreamPutc(area->outputStream, event->key);
    }

  if (saveKeyEvent)
    return (saveKeyEvent(component, event));
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


kernelWindowComponent *kernelWindowNewTextField(objectKey parent, int columns,
						componentParameters *params)
{
  // Just returns a kernelWindowTextArea with only one row, but there are
  // a couple of other things we do as well.

  kernelWindowComponent *component = NULL;
  kernelWindowTextArea *textArea = NULL;
  kernelTextArea *area = NULL;
  componentParameters newParams;

  kernelMemCopy(params, &newParams, sizeof(componentParameters));
  params = &newParams;

  component = kernelWindowNewTextArea(parent, columns, 1, 0, params);
  if (component == NULL)
    return (component);

  textArea = component->data;
  area = textArea->area;

  // Only X-resizable
  component->flags &= ~WINFLAG_RESIZABLEY;

  // Turn off the cursor until we get the focus
  area->cursorState = 0;

  // Turn echo off
  area->inputStream->echo = 0;

  // We want different focus behaviour than a text area
  saveFocus = component->focus;
  component->focus = focus;

  // Change the key event handler
  saveKeyEvent = component->keyEvent;
  component->keyEvent = keyEvent;

  return (component);
}
