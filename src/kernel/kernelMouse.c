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
//  kernelMouse.c
//

// This contains utility functions for managing mouses.

#include "kernelMouse.h"
#include "kernelError.h"
#include "kernelLog.h"
#include "kernelMalloc.h"
#include "kernelMemory.h"
#include "kernelWindow.h"
#include <string.h>

// The graphics environment
static int screenWidth = 0;
static int screenHeight = 0;

// The system mouse pointers
static kernelMousePointer *currentPointer;
static kernelMousePointer *pointerList[MOUSE_MAX_POINTERS];
static int numberPointers = 0;

static int initialized = 0;

// Keeps mouse pointer size and location data
volatile struct {
  int xPosition;
  int yPosition;
  unsigned width;
  unsigned height;
  int button1;
  int button2;
  int button3;
  int eventMask;

} mouseStatus = {
  0, 0, 0, 0, 0, 0, 0, 0
};


static inline void draw(void)
{
  if (currentPointer == NULL)
    return;
  
  // Draw the mouse pointer
  kernelGraphicDrawImage(NULL, &(currentPointer->pointerImage),
			 draw_translucent, mouseStatus.xPosition,
			 mouseStatus.yPosition, 0, 0, 0, 0);
}


static inline void erase(void)
{
  // Redraw whatever the mouse was covering
  kernelWindowRedrawArea(mouseStatus.xPosition, mouseStatus.yPosition,
			 mouseStatus.width, mouseStatus.height);
}


static inline int findPointerSlot(const char *pointerName)
{
  // Find the named pointer

  int count;
  
  // Find the pointer with the name
  for (count = 0; count < numberPointers; count ++)
    if (!strncmp(pointerName, pointerList[count]->name, MOUSE_POINTER_NAMELEN))
      return (count);

  return (ERR_NOSUCHENTRY);
}


static inline void status2event(windowEvent *event)
{
  event->type = mouseStatus.eventMask;
  event->xPosition = mouseStatus.xPosition;
  event->yPosition = mouseStatus.yPosition;
  event->key = 0;
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


int kernelMouseInitialize(void)
{
  // Initialize the mouse functions

  int status = 0;
  char name[128];
  char value[128];
  int count;

  // When you load a mouse pointer it automatically switches to it, so load
  // the 'busy' one last
  char *mousePointerTypes[][2] = {
    { "default", MOUSE_DEFAULT_POINTER_DEFAULT },
    { "busy", MOUSE_DEFAULT_POINTER_BUSY }
  };

  extern variableList *kernelVariables;

  screenWidth = kernelGraphicGetScreenWidth();
  screenHeight = kernelGraphicGetScreenHeight();

  // Put the mouse in the center of the screen

  mouseStatus.xPosition = (screenWidth / 2);
  mouseStatus.yPosition = (screenHeight / 2);

  initialized = 1;

  // Load the mouse pointers
  for (count = 0; count < 2; count ++)
    {
      strcpy(name, "mouse.pointer.");
      strcat(name, mousePointerTypes[count][0]);

      if (kernelVariableListGet(kernelVariables, name, value, 128))
	{
	  // Nothing specified.  Use the default.
	  strcpy(value, mousePointerTypes[count][1]);
	  // Save it
	  kernelVariableListSet(kernelVariables, name, value);
	}

      status = kernelMouseLoadPointer(mousePointerTypes[count][0],value);
      if (status < 0)
	kernelError(kernel_warn, "Unable to load mouse pointer %s=\"%s\"",
		    name, value);
    }

  return (status = 0);
}


int kernelMouseShutdown(void)
{
  // Stop processing mouse stuff.
  
  initialized = 0;
  return (0);
}


int kernelMouseLoadPointer(const char *pointerName, const char *fileName)
{
  // Load a new pointer.

  int status = 0;
  kernelMousePointer *newPointer = NULL;
  int pointerSlot = -1;
  
  // Make sure we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check parameters
  if ((pointerName == NULL) || (fileName == NULL))
    return (status = ERR_NULLPARAMETER);

  newPointer = kernelMalloc(sizeof(kernelMousePointer));
  if (newPointer == NULL)
    return (status = ERR_MEMORY);

  status = kernelImageLoad(fileName, 0, 0, &(newPointer->pointerImage));
  if (status < 0)
    {
      kernelError(kernel_error, "Error loading mouse pointer image %s",
		  pointerName);
      return (status);
    }

  // Save the name
  strncpy(newPointer->name, pointerName, MOUSE_POINTER_NAMELEN);

  // Mouse pointers are translucent, and the translucent color is pure green
  newPointer->pointerImage.translucentColor.red = 0;
  newPointer->pointerImage.translucentColor.green = 255;
  newPointer->pointerImage.translucentColor.blue = 0;

  // Let's see whether this is a new pointer, or whether this will replace
  // an existing one
  pointerSlot = findPointerSlot(pointerName);

  if (pointerSlot < 0)
    {
      // This is a new pointer, so add it to the list.
      
      if (numberPointers >= MOUSE_MAX_POINTERS)
	{
	  kernelError(kernel_error, "Can't exceed max number of mouse "
		      "pointers (%d)", MOUSE_MAX_POINTERS);
	  kernelMemoryRelease(newPointer->pointerImage.data);
	  kernelFree(newPointer);
	  return (status = ERR_BOUNDS);
	}
      
      pointerList[numberPointers++] = newPointer;
    }
  else
    {
      // Replace the existing pointer with this one
      kernelMemoryRelease(pointerList[pointerSlot]->pointerImage.data);
      kernelFree(pointerList[pointerSlot]);
      pointerList[pointerSlot] = newPointer;
    }

  kernelLog("Loaded mouse pointer %s from file %s", newPointer->name,
	    fileName);

  return (status = 0);
}


kernelMousePointer *kernelMouseGetPointer(const char *pointerName)
{
  // Returns a pointer to the requested mouse pointer, by name

  kernelMousePointer *pointer = NULL;
  int pointerSlot = -1;

  // Make sure we've been initialized
  if (!initialized)
    return (pointer = NULL);

  // Check parameters
  if (pointerName == NULL)
    {
      kernelError(kernel_error, "NULL mouse pointer name");
      return (pointer = NULL);
    }

  pointerSlot = findPointerSlot(pointerName);

  if (pointerSlot >= 0)
    pointer = pointerList[pointerSlot];
  else
    kernelError(kernel_error, "Mouse pointer \"%s\" not found", pointerName);

  return (pointer);
}


int kernelMouseSetPointer(kernelMousePointer *pointer)
{
  // Sets the current mouse pointer

  int status = 0;

  // Make sure we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check parameters
  if (pointer == NULL)
    {
      kernelError(kernel_error, "NULL mouse pointer");
      return (status = ERR_NULLPARAMETER);
    }

  erase();
  currentPointer = pointer;
  mouseStatus.width = currentPointer->pointerImage.width;
  mouseStatus.height = currentPointer->pointerImage.height;
  draw();
  
  return (status = 0);
}


void kernelMouseDraw(void)
{
  // Just draw the mouse pointer.  Don't erase any previous ones or
  // anything

  // Make sure we've been initialized
  if (!initialized)
    return;

  draw();

  return;
}


void kernelMouseMove(int xChange, int yChange)
{
  // Move the mouse relative to its current position

  windowEvent event;

  // Make sure we've been initialized
  if (!initialized)
    return;

  erase();

  mouseStatus.xPosition += xChange;
  mouseStatus.yPosition += yChange;

  // Make sure the new position is valid
  if (mouseStatus.xPosition < 0)
    mouseStatus.xPosition = 0;
  else if (mouseStatus.xPosition > (screenWidth - 3))
    mouseStatus.xPosition = (screenWidth - 3);

  if (mouseStatus.yPosition < 0)
    mouseStatus.yPosition = 0;
  else if (mouseStatus.yPosition > (screenHeight - 3))
    mouseStatus.yPosition = (screenHeight - 3);

  
  if (mouseStatus.button1 || mouseStatus.button2 || mouseStatus.button3)
    mouseStatus.eventMask = EVENT_MOUSE_DRAG;
  else
    mouseStatus.eventMask = EVENT_MOUSE_MOVE;

  draw();

  // Tell the window manager, if it cares
  status2event(&event);
  kernelWindowProcessEvent(&event);

  return;
}


void kernelMouseButtonChange(int buttonNumber, int status)
{
  // Adjust the mouse button settings and maybe call the window manager
  // to process the event

  windowEvent event;

  // Make sure we've been initialized
  if (!initialized)
    return;

  switch (buttonNumber)
    {
    case 1:
      mouseStatus.button1 = status;
      if (status)
	mouseStatus.eventMask = EVENT_MOUSE_LEFTDOWN;
      else
	mouseStatus.eventMask = EVENT_MOUSE_LEFTUP;
      break;
    case 2:
      mouseStatus.button2 = status;
      if (status)
	mouseStatus.eventMask = EVENT_MOUSE_MIDDLEDOWN;
      else
	mouseStatus.eventMask = EVENT_MOUSE_MIDDLEUP;
      break;
    case 3:
      mouseStatus.button3 = status;
      if (status)
	mouseStatus.eventMask = EVENT_MOUSE_RIGHTDOWN;
      else
	mouseStatus.eventMask = EVENT_MOUSE_RIGHTUP;
      break;
    }

  // Tell the window manager
  status2event(&event);
  kernelWindowProcessEvent(&event);

  return;
}


int kernelMouseGetX(void)
{
  // Make sure we've been initialized
  if (!initialized)
    return (ERR_NOTINITIALIZED);

  return (mouseStatus.xPosition);
}


int kernelMouseGetY(void)
{
  // Make sure we've been initialized
  if (!initialized)
    return (ERR_NOTINITIALIZED);

  return (mouseStatus.yPosition);
}
