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
//  kernelMouseFunctions.c
//

// This contains utility functions for managing mouses.

#include "kernelMouseFunctions.h"
#include "kernelGraphicFunctions.h"
#include "kernelWindowManager.h"
#include "kernelLog.h"
#include "kernelError.h"
#include <sys/errors.h>
#include <string.h>


// This specifies the default system mouse pointers
static kernelMousePointer pointerList[16];
static int numberPointers = 0;
static kernelMousePointer *currentPointer;

static unsigned screenWidth = 0;
static unsigned screenHeight = 0;
static kernelMouseObject *kernelMouse = NULL;

static kernelMouseStatus mouseStatus;

static int initialized = 0;


static int checkObjectAndDriver(char *invokedBy)
{
  // This routine consists of a couple of things commonly done by the
  // other driver wrapper routines.  I've done this to minimize the amount
  // of duplicated code.  As its argument, it takes the name of the 
  // routine that called it so that any error messages can better reflect
  // what was really being done when the error occurred.

  int status = 0;

  // Make sure the mouse object isn't NULL (which could indicate that the
  // device has not been properly registered)
  if (kernelMouse == NULL)
    {
      // Make the error
      kernelError(kernel_error, "NULL mouse object");
      return (status = ERR_NULLPARAMETER);
    }

  // Make sure that the device has a non-NULL driver
  if (kernelMouse->deviceDriver == NULL)
    {
      // Make the error
      kernelError(kernel_error, "Mouse device driver is NULL");
      return (status = ERR_NOSUCHDRIVER);
    }

  return (status = 0);
}


static inline void draw(void)
{
  if (currentPointer == NULL)
    return;
  
  // Draw the mouse pointer
  kernelGraphicDrawImage(NULL, &(currentPointer->pointerImage),
			 mouseStatus.xPosition,
			 mouseStatus.yPosition);
}


static inline void erase(void)
{
  // Redraw whatever the mouse was covering
  kernelWindowManagerRedrawArea(mouseStatus.xPosition, mouseStatus.yPosition,
				mouseStatus.width, mouseStatus.height);
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


int kernelMouseRegisterDevice(kernelMouseObject *theMouse)
{
  // This routine will register a new mouse object.

  int status = 0;

  if (theMouse == NULL)
    {
      // Make the error
      kernelError(kernel_error, "NULL mouse object");
      return (status = ERR_NULLPARAMETER);
    }

  // Alright.  We'll save the pointer to the device
  kernelMouse = theMouse;

  return (status = 0);
}


int kernelMouseInstallDriver(kernelMouseDriver *theDriver)
{
  // Attaches a driver to our mouse object.

  int status = 0;

  // Make sure the driver isn't NULL
  if (theDriver == NULL)
    {
      // Make the error
      kernelError(kernel_error, "NULL mouse driver");
      return (status = ERR_NOSUCHDRIVER);
    }

  // Install the device driver
  kernelMouse->deviceDriver = theDriver;
  
  return (status = 0);
}


int kernelMouseInitialize(void)
{
  // Initialize the mouse functions

  int status = 0;

  // Check the keyboard object and device driver before proceeding
  status = checkObjectAndDriver(__FUNCTION__);
  if (status < 0)
    // Something went wrong, so we can't continue
    return (status);

  // Now make sure the device driver initialize routine has been 
  // installed
  if (kernelMouse->deviceDriver->driverInitialize == NULL)
    {
      // Make the error
      kernelError(kernel_error, "Mouse driver init routine is NULL");
      return (status = ERR_NOSUCHFUNCTION);
    }

  screenWidth = kernelGraphicGetScreenWidth();
  screenHeight = kernelGraphicGetScreenHeight();

  // Put the mouse in the center of the screen

  mouseStatus.xPosition = (screenWidth / 2);
  mouseStatus.yPosition = (screenHeight / 2);

  // Ok, now we can call the driver init routine.
  status = kernelMouse->deviceDriver->driverInitialize();

  initialized = 1;

  return (status = 0);
}


int kernelMouseReadData(void)
{
  // This function calls the mouse driver to read data from the
  // device.  It pretty much just calls the associated driver routines, 
  // but it also does some checks and whatnot to make sure that the 
  // device, driver, and driver routines are

  int status = 0;

  // Make sure we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Make sure the device driver initialize routine has been 
  // installed
  if (kernelMouse->deviceDriver->driverReadData == NULL)
    {
      // Make the error
      kernelError(kernel_error, "No mouse read data function");
      return (status = ERR_NOSUCHFUNCTION);
    }

  // Ok, now we can call the routine.
  kernelMouse->deviceDriver->driverReadData();

  return (status = 0);
}


int kernelMouseLoadPointer(const char *pointerName, const char *fileName)
{
  // Load a new pointer.

  int status = 0;
  
  // Make sure we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check parameters
  if ((pointerName == NULL) || (fileName == NULL))
    return (status = ERR_NULLPARAMETER);

  currentPointer = &pointerList[numberPointers++];

  status = kernelImageLoadBmp(fileName, &(currentPointer->pointerImage));

  if (status < 0)
    {
      kernelError(kernel_error, "Error loading mouse bitmap %s", pointerName);
      return (status);
    }

  // Save the name
  strcpy(currentPointer->name, pointerName);

  // Mouse pointers are translucent, and the translucent color is pure green
  currentPointer->pointerImage.isTranslucent = 1;
  currentPointer->pointerImage.translucentColor.red = 0;
  currentPointer->pointerImage.translucentColor.green = 255;
  currentPointer->pointerImage.translucentColor.blue = 0;

  mouseStatus.width = currentPointer->pointerImage.width;
  mouseStatus.height = currentPointer->pointerImage.height;

  kernelLog("Loaded mouse pointer %s from file %s", pointerName, fileName);

  return (status = 0);
}


int kernelMouseSwitchPointer(const char *pointerName)
{
  // Show a new named pointer
  
  int status = 0;
  int count;
  
  // Make sure we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check parameters
  if (pointerName == NULL)
    return (status = ERR_NULLPARAMETER);

  // Find the pointer with the name
  for (count = 0; count < numberPointers; count ++)
    {
      if (!strcmp(pointerName, pointerList[count].name))
	{
	  erase();
	  currentPointer = &pointerList[count];
	  mouseStatus.width = currentPointer->pointerImage.width;
	  mouseStatus.height = currentPointer->pointerImage.height;
	  draw();
	  break;
	}
    }

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

  mouseStatus.eventMask = MOUSE_MOVE;
  if (mouseStatus.button1 || mouseStatus.button2 || mouseStatus.button3)
    mouseStatus.eventMask |= MOUSE_DRAG;

  draw();

  // Tell the window manager, if it cares
  kernelWindowManagerProcessMouseEvent(&mouseStatus);

  return;
}


void kernelMouseButtonChange(int buttonNumber, int status)
{
  // Adjust the mouse button settings and maybe call the window manager
  // to process the event

  // Make sure we've been initialized
  if (!initialized)
    return;

  if (status)
    mouseStatus.eventMask = MOUSE_DOWN;
  else
    mouseStatus.eventMask = MOUSE_UP;

  switch (buttonNumber)
    {
    case 1:
      mouseStatus.button1 = status;
      break;
    case 2:
      mouseStatus.button2 = status;
      break;
    case 3:
      mouseStatus.button3 = status;
    }

  // Tell the window manager, if it cares
  kernelWindowManagerProcessMouseEvent(&mouseStatus);

  return;
}
