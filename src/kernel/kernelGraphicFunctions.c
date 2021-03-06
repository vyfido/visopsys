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
//  kernelGraphicFunctions.c
//

// This file contains abstracted functions for drawing raw graphics on
// the screen

#include "kernelGraphicFunctions.h"
#include "kernelParameters.h"
#include "kernelPageManager.h"
#include "kernelText.h"
#include "kernelError.h"
#include <sys/errors.h>
#include <string.h>


// This is data for a temporary console when we first arrive in a graphical
// mode
static unsigned char tmpConsoleData[80 * 25];
static kernelGraphicBuffer tmpConsoleBuffer;
static kernelTextArea tmpGraphicConsole = {
  0, 0, 80, 25, 0, 0,
  (color) { 255, 255, 255 },
  (color) { 0, 0, 0 },
  (void *) NULL, /* input stream */
  (void *) NULL, /* output stream */
  tmpConsoleData,
  (kernelAsciiFont *) NULL,
  &tmpConsoleBuffer
};

static kernelGraphicAdapterObject *kernelGraphicAdapter = NULL;
static int initialized = 0;


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


int kernelGraphicRegisterDevice(kernelGraphicAdapterObject *theAdapter)
{
  // This routine will register a new graphic adapter object.  On error,
  // it returns negative

  int status = 0;

  if (theAdapter == NULL)
    {
      // Make the error
      kernelError(kernel_error, "The graphic adapter  is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  // Alright.  We'll save the pointer to the device
  kernelGraphicAdapter = theAdapter;

  // Return success
  return (status = 0);
}


int kernelGraphicInstallDriver(kernelGraphicDriver *theDriver)
{
  // Attaches a driver object to a graphic adapter object.  If the pointer to
  // the driver object is NULL, it returns negative.  Otherwise, returns zero.

  int status = 0;

  // Make sure the adapter object isn't NULL
  if (kernelGraphicAdapter == NULL)
    {
      // Make the error
      kernelError(kernel_error, "The graphic adapter  is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  // Make sure the driver isn't NULL
  if (theDriver == NULL)
    {
      // Make the error
      kernelError(kernel_error, "The kernelGraphicAdapterDriver object "
		  "passed or referenced is NULL");
      return (status = ERR_NOSUCHDRIVER);
    }

  // Install the device driver
  kernelGraphicAdapter->deviceDriver = theDriver;
  
  // Return success
  return (status = 0);
}


int kernelGraphicInitialize(void)
{
  // This function initializes the graphic routines.  It pretty much just
  // calls the associated driver routine, but it also does some checks and
  // whatnot to make sure that the device, driver, and driver routines are
  // valid.

  int status = 0;

  // Are we in a graphics mode?
  if (kernelGraphicAdapter->mode == 0)
    return (status = ERR_INVALID);
  
  status = kernelPageMapToFree(KERNELPROCID,
			       kernelGraphicAdapter->framebuffer, 
			       &(kernelGraphicAdapter->framebuffer),
			       (kernelGraphicAdapter->xRes *
				kernelGraphicAdapter->yRes *
				kernelGraphicAdapter->bitsPerPixel) / 8);
  // Success?
  if (status < 0)
    {
      kernelError(kernel_error, "Unable to map linear framebuffer");
      return (status);
    }
  
  // Now make sure the device driver initialize routine has been 
  // installed
  if (kernelGraphicAdapter->deviceDriver->driverInitialize == NULL)
    {
      // Make the error
      kernelError(kernel_error, "The driver routine is NULL");
      return (status = ERR_NOSUCHFUNCTION);
    }

  // Ok, now we can call the initialization routine.
  status = kernelGraphicAdapter->deviceDriver
    ->driverInitialize(kernelGraphicAdapter);

  if (status < 0)
    {
      // Make the error
      kernelError(kernel_error, "The graphic adapter driver initialization "
		  "returned this error code");
      return (status = ERR_NOTINITIALIZED);
    }

  // We are initialized
  initialized = 1;

  // Set the text console to use the graphic screen as a temporary output

  // Initialize the font functions
  status = kernelFontInitialize();
  if (status < 0)
    {
      kernelError(kernel_error, "Font initialization failed");
      return (status);
    }
  kernelFontGetDefault((kernelAsciiFont **) &(tmpGraphicConsole.font));
  tmpGraphicConsole.graphicBuffer->width = kernelGraphicAdapter->xRes;
  tmpGraphicConsole.graphicBuffer->height = kernelGraphicAdapter->yRes;
  tmpGraphicConsole.inputStream = (void *) kernelTextGetConsoleInput();
  tmpGraphicConsole.outputStream = (void *) kernelTextGetConsoleOutput();
  tmpGraphicConsole.graphicBuffer->data = kernelGraphicAdapter->framebuffer;
  kernelTextSwitchToGraphics(&tmpGraphicConsole);

  // Return success
  return (status = 0);
}


int kernelGraphicsAreEnabled(void)
{
  // Returns 1 if graphics are enabled, 0 otherwise
  return (initialized);
}


unsigned kernelGraphicGetScreenWidth(void)
{
  // Yup, returns the screen width

  // Make sure we've been initialized
  if (!initialized)
    return (ERR_NOTINITIALIZED);

  return (kernelGraphicAdapter->xRes);
}


unsigned kernelGraphicGetScreenHeight(void)
{
  // Yup, returns the screen height

  // Make sure we've been initialized
  if (!initialized)
    return (ERR_NOTINITIALIZED);

  return (kernelGraphicAdapter->yRes);
}


unsigned kernelGraphicCalculateAreaBytes(unsigned width, unsigned height)
{
  // Return the number of bytes needed to store a kernelGraphicBuffer's data
  // that can be drawn on the current display (this varies depending on the
  // bites-per-pixel, etc, that higher-level code shouldn't have to know
  // about)
  return ((width * height * kernelGraphicAdapter->bitsPerPixel) / 8);
}


int kernelGraphicClearScreen(color *background)
{
  // Clears the whole screen to the requested color
  
  int status = 0;

  // Make sure we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  if (background == NULL)
    return (status = ERR_NULLPARAMETER);

  // Now make sure the device driver drawPixel routine has been 
  // installed
  if (kernelGraphicAdapter->deviceDriver->driverClearScreen == NULL)
    {
      // Make the error
      kernelError(kernel_error, "The driver routine is NULL");
      return (status = ERR_NOSUCHFUNCTION);
    }

  // Ok, now we can call the routine.
  status = kernelGraphicAdapter->deviceDriver->driverClearScreen(background);
  return (status);
}


int kernelGraphicDrawPixel(kernelGraphicBuffer *buffer, color *foreground,
			   drawMode mode, int xCoord, int yCoord)
{
  // This is a generic routine for drawing a single pixel

  int status = 0;

  // Make sure we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check parameters
  if (foreground == NULL)
    return (status = ERR_NULLPARAMETER);

  // Make sure the pixel is in the buffer
  if ((xCoord >= buffer->width) || (yCoord >= buffer->height))
    // Don't make an error condition, just skip it
    return (status = 0);

  // Now make sure the device driver drawPixel routine has been 
  // installed
  if (kernelGraphicAdapter->deviceDriver->driverDrawPixel == NULL)
    {
      // Make the error
      kernelError(kernel_error, "The driver routine is NULL");
      return (status = ERR_NOSUCHFUNCTION);
    }

  // Ok, now we can call the routine.
  status = kernelGraphicAdapter->deviceDriver
    ->driverDrawPixel(buffer, foreground, mode, xCoord, yCoord);

  return (status);
}


int kernelGraphicDrawLine(kernelGraphicBuffer *buffer, color *foreground,
			  drawMode mode, int xCoord1, int yCoord1,
			  int xCoord2, int yCoord2)
{
  // This is a generic routine for drawing a simple line

  int status = 0;

  // Make sure we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check parameters
  if (foreground == NULL)
    return (status = ERR_NULLPARAMETER);

  // Now make sure the device driver drawLine routine has been 
  // installed
  if (kernelGraphicAdapter->deviceDriver->driverDrawLine == NULL)
    {
      // Make the error
      kernelError(kernel_error, "The driver routine is NULL");
      return (status = ERR_NOSUCHFUNCTION);
    }

  // Ok, now we can call the routine.
  status = kernelGraphicAdapter->deviceDriver
    ->driverDrawLine(buffer, foreground, mode, xCoord1, yCoord1, xCoord2,
		     yCoord2);
  return (status);
}


int kernelGraphicDrawRect(kernelGraphicBuffer *buffer, color *foreground,
			  drawMode mode, int xCoord, int yCoord,
			  unsigned width, unsigned height,
			  unsigned thickness, int fill)
{
  // This is a generic routine for drawing a rectangle

  int status = 0;

  // Make sure we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Color not NULL
  if (foreground == NULL)
    return (status = ERR_NULLPARAMETER);

  // Now make sure the device driver drawRect routine has been 
  // installed
  if (kernelGraphicAdapter->deviceDriver->driverDrawRect == NULL)
    {
      // Make the error
      kernelError(kernel_error, "The driver routine is NULL");
      return (status = ERR_NOSUCHFUNCTION);
    }

  // Ok, now we can call the routine.
  status = kernelGraphicAdapter->deviceDriver
    ->driverDrawRect(buffer, foreground, mode, xCoord, yCoord, width, height,
		     thickness, fill);
  return (status);
}


int kernelGraphicDrawOval(kernelGraphicBuffer *buffer, color *foreground,
			  drawMode mode, int xCoord, int yCoord,
			  unsigned width, unsigned height,
			  unsigned thickness, int fill)
{
  // This is a generic routine for drawing an oval

  int status = 0;

  // Make sure we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Color not NULL
  if (foreground == NULL)
    return (status = ERR_NULLPARAMETER);

  // Now make sure the device driver drawOval routine has been 
  // installed
  if (kernelGraphicAdapter->deviceDriver->driverDrawOval == NULL)
    {
      // Make the error
      kernelError(kernel_error, "The driver routine is NULL");
      return (status = ERR_NOSUCHFUNCTION);
    }

  // Ok, now we can call the routine.
  status = kernelGraphicAdapter->deviceDriver
    ->driverDrawOval(buffer, foreground, mode, xCoord, yCoord, width, height,
		     thickness, fill);
  return (status);
}


int kernelGraphicDrawImage(kernelGraphicBuffer *buffer, image *drawImage,
			   int xCoord, int yCoord)
{
  // This is a generic routine for drawing an image

  int status = 0;

  // Make sure we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check parameters
  if (drawImage == NULL)
    return (status = ERR_NULLPARAMETER);

  // Now make sure the device driver drawImage routine has been 
  // installed
  if (kernelGraphicAdapter->deviceDriver->driverDrawImage == NULL)
    {
      // Make the error
      kernelError(kernel_error, "The driver routine is NULL");
      return (status = ERR_NOSUCHFUNCTION);
    }

  // Ok, now we can call the routine.
  status = kernelGraphicAdapter->deviceDriver
    ->driverDrawImage(buffer, drawImage, xCoord, yCoord);
  return (status);
}


int kernelGraphicGetImage(kernelGraphicBuffer *buffer, image *getImage,
			  int xCoord, int yCoord, unsigned width,
			  unsigned height)
{
  // This is a generic routine for getting an image from the requested
  // area of the screen.  Good for screen shots and such

  int status = 0;

  // Make sure we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check parameters
  if (getImage == NULL)
    return (status = ERR_NULLPARAMETER);

  // Now make sure the device driver getImage routine has been 
  // installed
  if (kernelGraphicAdapter->deviceDriver->driverGetImage == NULL)
    {
      // Make the error
      kernelError(kernel_error, "The driver routine is NULL");
      return (status = ERR_NOSUCHFUNCTION);
    }

  // Ok, now we can call the routine.
  status = kernelGraphicAdapter->deviceDriver
    ->driverGetImage(buffer, getImage, xCoord, yCoord, width, height);
  return (status);
}


int kernelGraphicDrawText(kernelGraphicBuffer *buffer, color *foreground,
			  kernelAsciiFont *font, const char *text,
			  drawMode mode, int xCoord, int yCoord)
{
  // Draws a line of text using the supplied ASCII font at the requested
  // coordinates.  Uses the default foreground and background colors.

  int status = 0;
  int length = 0;
  int index = 0;
  int count;

  // Make sure we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check parameters
  if ((font == NULL) || (text == NULL) || (foreground == NULL))
    return (status = ERR_NULLPARAMETER);

  // Now make sure the device driver drawImage routine has been installed
  if (kernelGraphicAdapter->deviceDriver->driverDrawMonoImage == NULL)
    {
      // Make the error
      kernelError(kernel_error, "The driver routine is NULL");
      return (status = ERR_NOSUCHFUNCTION);
    }

  // How long is the string?
  length = strlen(text);

  // Loop through the string
  for (count = 0; count < length; count ++)
    {
      index = (int) text[count] - 32;

      if ((index < 0) || (index >= ASCII_PRINTABLES))
	// Not printable.  Print a space, which is index zero.
	index = 0;
      
      // Call the driver routine to draw the character
      status = kernelGraphicAdapter->deviceDriver
	->driverDrawMonoImage(buffer, &(font->chars[index]), foreground, NULL,
			      xCoord, yCoord);

      xCoord += font->chars[index].width;
    }

  return (status = 0);
}


int kernelGraphicCopyArea(kernelGraphicBuffer *buffer, int xCoord1,
			  int yCoord1, unsigned width, unsigned height,
			  int xCoord2, int yCoord2)
{
  // Copies the requested area of the screen to the new location.

  int status = 0;
  
  // Make sure we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Now make sure the device driver drawImage routine has been installed
  if (kernelGraphicAdapter->deviceDriver->driverCopyArea == NULL)
    {
      // Make the error
      kernelError(kernel_error, "The driver routine is NULL");
      return (status = ERR_NOSUCHFUNCTION);
    }

  // Call the driver routine to copy the area
  status = kernelGraphicAdapter->deviceDriver
    ->driverCopyArea(buffer, xCoord1, yCoord1, width, height, xCoord2,
		     yCoord2);

  return (status);
}


int kernelGraphicClearArea(kernelGraphicBuffer *buffer, color *background,
			   int xCoord, int yCoord, unsigned width,
			   unsigned height)
{
  // Clears the requested area of the screen.  This is just a convenience
  // function that draws a filled rectangle over the spot using the
  // background color

  int status = 0;
  
  // Make sure we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Color and area not NULL
  if (background == NULL)
    return (status = ERR_NULLPARAMETER);

  // Draw a filled rectangle over the requested area
  status = kernelGraphicDrawRect(buffer, background, draw_normal,
				 xCoord, yCoord, width, height, 1, 1);

  return (status);
}


int kernelGraphicRenderBuffer(kernelGraphicBuffer *buffer, int drawX,
			      int drawY, int clipX, int clipY,
			      unsigned clipWidth, unsigned clipHeight)
{
  // Take a kernelGraphicBuffer and render it on the screen
  int status = 0;
  
  // Make sure we've been initialized
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Now make sure the device driver drawImage routine has been installed
  if (kernelGraphicAdapter->deviceDriver->driverRenderBuffer == NULL)
    {
      // Make the error
      kernelError(kernel_error, "The driver routine is NULL");
      return (status = ERR_NOSUCHFUNCTION);
    }

  // Call the driver routine to render the buffer
  status = kernelGraphicAdapter->deviceDriver
    ->driverRenderBuffer(buffer, drawX, drawY, clipX, clipY, clipWidth,
		       clipHeight);

  return (status);
}
