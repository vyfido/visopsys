//
//  Visopsys
//  Copyright (C) 1998-2005 J. Andrew McLaughlin
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
//  kernelGraphic.c
//

// This file contains abstracted functions for drawing raw graphics on
// the screen

#include "kernelGraphic.h"
#include "kernelParameters.h"
#include "kernelText.h"
#include "kernelFile.h"
#include "kernelMalloc.h"
#include "kernelMemoryManager.h"
#include "kernelMiscFunctions.h"
#include "kernelError.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// The global colors
color kernelDefaultForeground = {
  DEFAULT_FOREGROUND_BLUE,
  DEFAULT_FOREGROUND_GREEN,
  DEFAULT_FOREGROUND_RED
};
color kernelDefaultBackground = {
  DEFAULT_BACKGROUND_BLUE,
  DEFAULT_BACKGROUND_GREEN,
  DEFAULT_BACKGROUND_RED
};
color kernelDefaultDesktop = {
  DEFAULT_DESKTOP_BLUE,
  DEFAULT_DESKTOP_GREEN,
  DEFAULT_DESKTOP_RED
};

static kernelDevice *systemAdapter = NULL;
static kernelGraphicAdapter *adapterDevice = NULL;
static kernelGraphicOps *ops = NULL;
static kernelGraphicBuffer tmpConsoleBuffer;

// This is data for a temporary console when we first arrive in a graphical
// mode
static kernelTextArea *tmpGraphicConsole = NULL;


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


int kernelGraphicInitialize(kernelDevice *dev)
{
  // This function initializes the graphic routines.

  int status = 0;
  kernelTextInputStream *inputStream = NULL;

  if (dev == NULL)
    {
      kernelError(kernel_error, "The graphic adapter device is NULL");
      return (status = ERR_NOTINITIALIZED);
    }

  systemAdapter = dev;

  if ((systemAdapter->data == NULL) || (systemAdapter->driver == NULL) ||
      (systemAdapter->driver->ops == NULL))
    {
      kernelError(kernel_error, "The graphic adapter, driver or ops are NULL");
      return (status = ERR_NULLPARAMETER);
    }

  adapterDevice = (kernelGraphicAdapter *) systemAdapter->data;
  ops = systemAdapter->driver->ops;

  // Are we in a graphics mode?
  if (adapterDevice->mode == 0)
    return (status = ERR_INVALID);
  
  // Get a temporary text area for console output, and use the graphic screen
  // as a temporary output
  tmpGraphicConsole = kernelTextAreaNew(80, 50, 1, DEFAULT_SCROLLBACKLINES);
  if (tmpGraphicConsole == NULL)
    // Better not try to print any error messages...
    return (status = ERR_NOTINITIALIZED);
  
  // Assign some extra things to the text area
  tmpGraphicConsole->foreground.blue = 255;
  tmpGraphicConsole->foreground.green = 255;
  tmpGraphicConsole->foreground.red = 255;
  tmpGraphicConsole->background.blue = DEFAULT_DESKTOP_BLUE;
  tmpGraphicConsole->background.green = DEFAULT_DESKTOP_GREEN;
  tmpGraphicConsole->background.red = DEFAULT_DESKTOP_RED;
  // Change the input and output streams to the console
  inputStream = tmpGraphicConsole->inputStream;
  if (inputStream)
    {
      if (inputStream->s.buffer)
	{
	  kernelFree((void *) inputStream->s.buffer);
	  inputStream->s.buffer = NULL;
	}
	  
      kernelFree(tmpGraphicConsole->inputStream);
      tmpGraphicConsole->inputStream = (void *) kernelTextGetConsoleInput();
    }
  if (tmpGraphicConsole->outputStream)
    {
      kernelFree(tmpGraphicConsole->outputStream);
      tmpGraphicConsole->outputStream = (void *) kernelTextGetConsoleOutput();
    }
  tmpConsoleBuffer.width = adapterDevice->xRes;
  tmpConsoleBuffer.height = adapterDevice->yRes;
  tmpConsoleBuffer.data = adapterDevice->framebuffer;
  tmpGraphicConsole->graphicBuffer = &tmpConsoleBuffer;

  // Initialize the font functions
  status = kernelFontInitialize();
  if (status < 0)
    {
      kernelError(kernel_error, "Font initialization failed");
      return (status);
    }

  // Assign the default system font to our console text area
  kernelFontGetDefault((kernelAsciiFont **) &(tmpGraphicConsole->font));

  // Switch the console
  kernelTextSwitchToGraphics(tmpGraphicConsole);

  // Clear the screen with our default background color
  ops->driverClearScreen(&kernelDefaultDesktop);

  // Return success
  return (status = 0);
}


int kernelGraphicsAreEnabled(void)
{
  // Returns 1 if graphics are enabled, 0 otherwise
  if (systemAdapter != NULL)
    return (1);
  else
    return (0);
}


int kernelGraphicGetModes(videoMode *modeBuffer, unsigned size)
{
  // Return the list of graphics modes supported by the adapter

  size = max(size, (sizeof(videoMode) * MAXVIDEOMODES));
  kernelMemCopy(&(adapterDevice->supportedModes), modeBuffer, size);
  return (adapterDevice->numberModes);
}


int kernelGraphicGetMode(videoMode *mode)
{
  // Get the current graphics mode
  mode->mode = adapterDevice->mode;
  mode->xRes = adapterDevice->xRes;
  mode->yRes = adapterDevice->yRes;
  mode->bitsPerPixel = adapterDevice->bitsPerPixel;
  return (0);
}


int kernelGraphicSetMode(videoMode *mode)
{
  // Set the preferred graphics mode for the next reboot.  We create a
  // little binary file that the loader can easily understand

  int status = 0;
  file modeFile;
  int buffer[4];

  kernelFileOpen("/grphmode",
		 (OPENMODE_WRITE | OPENMODE_CREATE | OPENMODE_TRUNCATE),
		 &modeFile);

  buffer[0] = mode->xRes;
  buffer[1] = mode->yRes;
  buffer[2] = mode->bitsPerPixel;
  buffer[3] = 0;

  status = kernelFileWrite(&modeFile, 0, 1, (unsigned char *) buffer);

  kernelFileSetSize(modeFile.handle, 16);
  kernelFileClose(&modeFile);

  return (status);
}


int kernelGraphicGetScreenWidth(void)
{
  // Yup, returns the screen width

  // Make sure we've been initialized
  if (systemAdapter == NULL)
    return (ERR_NOTINITIALIZED);

  return (adapterDevice->xRes);
}


int kernelGraphicGetScreenHeight(void)
{
  // Yup, returns the screen height

  // Make sure we've been initialized
  if (systemAdapter == NULL)
    return (ERR_NOTINITIALIZED);

  return (adapterDevice->yRes);
}


int kernelGraphicCalculateAreaBytes(int width, int height)
{
  // Return the number of bytes needed to store a kernelGraphicBuffer's data
  // that can be drawn on the current display (this varies depending on the
  // bites-per-pixel, etc, that higher-level code shouldn't have to know
  // about)
  return (width * height * adapterDevice->bytesPerPixel);
}


int kernelGraphicClearScreen(color *background)
{
  // Clears the whole screen to the requested color
  
  int status = 0;

  // Make sure we've been initialized
  if (systemAdapter == NULL)
    return (status = ERR_NOTINITIALIZED);

  if (background == NULL)
    return (status = ERR_NULLPARAMETER);

  // Now make sure the device driver drawPixel routine has been 
  // installed
  if (ops->driverClearScreen == NULL)
    {
      kernelError(kernel_error, "The driver routine is NULL");
      return (status = ERR_NOSUCHFUNCTION);
    }

  // Ok, now we can call the routine.
  status = ops->driverClearScreen(background);
  return (status);
}


int kernelGraphicGetColor(const char *colorName, color *getColor)
{
  // Given the color name, get it.

  int status = 0;

  if ((colorName == NULL) || (getColor == NULL))
    return (status = ERR_NULLPARAMETER);

  if (!strcmp(colorName, "foreground"))
    kernelMemCopy(&kernelDefaultForeground, getColor, sizeof(color));
  if (!strcmp(colorName, "background"))
    kernelMemCopy(&kernelDefaultBackground, getColor, sizeof(color));
  if (!strcmp(colorName, "desktop"))
    kernelMemCopy(&kernelDefaultDesktop, getColor, sizeof(color));

  return (status = 0);
}


int kernelGraphicSetColor(const char *colorName, color *setColor)
{
  // Given the color name, set it.

  int status = 0;
  char fullColorName[128];
  char value[4];

  // The kernel config file.
  extern variableList *kernelVariables;

  if ((colorName == NULL) || (setColor == NULL))
    return (status = ERR_NULLPARAMETER);

  // Try to set the variables

  sprintf(fullColorName, "%s.color.red", colorName);
  sprintf(value, "%d", setColor->red);
  status = kernelVariableListSet(kernelVariables, fullColorName, value);
  if (status < 0)
    return (status);

  sprintf(fullColorName, "%s.color.green", colorName);
  sprintf(value, "%d", setColor->green);
  status = kernelVariableListSet(kernelVariables, fullColorName, value);
  if (status < 0)
    return (status);

  sprintf(fullColorName, "%s.color.blue", colorName);
  sprintf(value, "%d", setColor->blue);
  status = kernelVariableListSet(kernelVariables, fullColorName, value);
  if (status < 0)
    return (status);

  // Set the current color values
  if (!strcmp(colorName, "foreground"))
    kernelMemCopy(setColor, &kernelDefaultForeground, sizeof(color));
  else if (!strcmp(colorName, "background"))
    kernelMemCopy(setColor, &kernelDefaultBackground, sizeof(color));
  else if (!strcmp(colorName, "desktop"))
    kernelMemCopy(setColor, &kernelDefaultDesktop, sizeof(color));

  // Save the values
  if (!(kernelFilesystemGet("/")->readOnly))
    status = kernelConfigurationWriter(DEFAULT_KERNEL_CONFIG, kernelVariables);
  return (status);
}


int kernelGraphicDrawPixel(kernelGraphicBuffer *buffer, color *foreground,
			   drawMode mode, int xCoord, int yCoord)
{
  // This is a generic routine for drawing a single pixel

  int status = 0;

  // Make sure we've been initialized
  if (systemAdapter == NULL)
    return (status = ERR_NOTINITIALIZED);

  // Check parameters
  if (foreground == NULL)
    return (status = ERR_NULLPARAMETER);

  // Now make sure the device driver drawPixel routine has been 
  // installed
  if (ops->driverDrawPixel == NULL)
    {
      kernelError(kernel_error, "The driver routine is NULL");
      return (status = ERR_NOSUCHFUNCTION);
    }

  // Ok, now we can call the routine.
  status = ops->driverDrawPixel(buffer, foreground, mode, xCoord, yCoord);

  return (status);
}


int kernelGraphicDrawLine(kernelGraphicBuffer *buffer, color *foreground,
			  drawMode mode, int xCoord1, int yCoord1, int xCoord2,
			  int yCoord2)
{
  // This is a generic routine for drawing a simple line

  int status = 0;

  // Make sure we've been initialized
  if (systemAdapter == NULL)
    return (status = ERR_NOTINITIALIZED);

  // Check parameters
  if (foreground == NULL)
    return (status = ERR_NULLPARAMETER);

  // Now make sure the device driver drawLine routine has been 
  // installed
  if (ops->driverDrawLine == NULL)
    {
      kernelError(kernel_error, "The driver routine is NULL");
      return (status = ERR_NOSUCHFUNCTION);
    }

  // Ok, now we can call the routine.
  status = ops->driverDrawLine(buffer, foreground, mode, xCoord1, yCoord1,
			       xCoord2, yCoord2);
  return (status);
}


int kernelGraphicDrawRect(kernelGraphicBuffer *buffer, color *foreground,
			  drawMode mode, int xCoord, int yCoord, int width,
			  int height, int thickness, int fill)
{
  // This is a generic routine for drawing a rectangle

  int status = 0;

  // Make sure we've been initialized
  if (systemAdapter == NULL)
    return (status = ERR_NOTINITIALIZED);

  // Color not NULL
  if (foreground == NULL)
    return (status = ERR_NULLPARAMETER);

  // Zero size?
  if (!width || !height)
    return (status = 0);

  // Now make sure the device driver drawRect routine has been 
  // installed
  if (ops->driverDrawRect == NULL)
    {
      kernelError(kernel_error, "The driver routine is NULL");
      return (status = ERR_NOSUCHFUNCTION);
    }

  // Ok, now we can call the routine.
  status = ops->driverDrawRect(buffer, foreground, mode, xCoord, yCoord,
			       width, height, thickness, fill);
  return (status);
}


int kernelGraphicDrawOval(kernelGraphicBuffer *buffer, color *foreground,
			  drawMode mode, int xCoord, int yCoord, int width,
			  int height, int thickness, int fill)
{
  // This is a generic routine for drawing an oval

  int status = 0;

  // Make sure we've been initialized
  if (systemAdapter == NULL)
    return (status = ERR_NOTINITIALIZED);

  // Color not NULL
  if (foreground == NULL)
    return (status = ERR_NULLPARAMETER);

  // Now make sure the device driver drawOval routine has been 
  // installed
  if (ops->driverDrawOval == NULL)
    {
      kernelError(kernel_error, "The driver routine is NULL");
      return (status = ERR_NOSUCHFUNCTION);
    }

  // Ok, now we can call the routine.
  status = ops->driverDrawOval(buffer, foreground, mode, xCoord, yCoord,
			       width, height, thickness, fill);
  return (status);
}


int kernelGraphicImageToKernel(image *convImage)
{
  // Given an image received from the kernelGraphicGetImage function, above,
  // which has its memory in application space, convert the memory to
  // globally-accessible kernel memory

  int status = 0;
  void *savePtr = NULL;

  // Make sure we've been initialized
  if (systemAdapter == NULL)
    return (status = ERR_NOTINITIALIZED);

  // Check parameters
  if ((convImage == NULL) || (convImage->data == NULL) ||
      !convImage->dataLength)
    return (status = ERR_NULLPARAMETER);
  
  // If the image memory is already in kernel space...
  if (convImage->data >= (void *) KERNEL_VIRTUAL_ADDRESS)
    // Don't make it an error
    return (status = 0);

  // Save the current pointer
  savePtr = convImage->data;

  // Get new memory
  convImage->data = kernelMalloc(convImage->dataLength);
  if (convImage->data == NULL)
    {
      convImage->data = savePtr;
      return (status = ERR_MEMORY);
    }

  // Copy the data
  kernelMemCopy(savePtr, convImage->data, convImage->dataLength);

  // Free the old memory
  kernelMemoryRelease(savePtr);

  return (status = 0);
}


int kernelGraphicNewImage(image *blankImage, int width, int height)
{
  // This allocates a new image of the specified size, with a blank grey
  // background.
  
  int status = 0;
  kernelGraphicBuffer tmpBuffer;
  
  // Get a temporary buffer to clear with our desired color
  tmpBuffer.data =
    kernelMalloc(kernelGraphicCalculateAreaBytes(width, height));
  if (tmpBuffer.data == NULL)
    return (status = ERR_MEMORY);
  tmpBuffer.width = width;
  tmpBuffer.height = height;
  
  // Clear our buffer with our background color
  kernelGraphicDrawRect(&tmpBuffer, &kernelDefaultBackground, draw_normal,
			0, 0, width, height, 1, 1);
  
  // Get an image of the correct size
  status = kernelGraphicGetImage(&tmpBuffer, blankImage, 0, 0, width, height);
  
  // Free our buffer data
  kernelFree(tmpBuffer.data);
  tmpBuffer.data = NULL;

  return (status);
}


int kernelGraphicNewKernelImage(image *blankImage, int width, int height)
{
  // This is a wrapper for the kernelGraphicNewImage and
  // kernelGraphicImageToKernel functions, that should be used in the
  // kernel if one wants the image memory to be in kernel space (i.e.
  // window system components and the like)
  
  int status = 0;
  
  // Don't need to check parameters here.
  status = kernelGraphicNewImage(blankImage, width, height);
  if (status < 0)
    return (status);

  status = kernelGraphicImageToKernel(blankImage);
  return (status);
}


int kernelGraphicDrawImage(kernelGraphicBuffer *buffer, image *drawImage,
			   drawMode mode, int xCoord, int yCoord, int xOffset,
			   int yOffset, int width, int height)
{
  // This is a generic routine for drawing an image

  int status = 0;

  // Make sure we've been initialized
  if (systemAdapter == NULL)
    return (status = ERR_NOTINITIALIZED);

  // Check parameters
  if (drawImage == NULL)
    return (status = ERR_NULLPARAMETER);

  // Now make sure the device driver drawImage routine has been 
  // installed
  if (ops->driverDrawImage == NULL)
    {
      kernelError(kernel_error, "The driver routine is NULL");
      return (status = ERR_NOSUCHFUNCTION);
    }

  // Ok, now we can call the routine.
  status = ops->driverDrawImage(buffer, drawImage, mode, xCoord, yCoord,
				xOffset, yOffset, width, height);
  return (status);
}


int kernelGraphicGetImage(kernelGraphicBuffer *buffer, image *getImage,
			  int xCoord, int yCoord, int width, int height)
{
  // This is a generic routine for getting an image from a buffer.  The
  // image memory returned is in the application space of the current
  // process.

  int status = 0;

  // Make sure we've been initialized
  if (systemAdapter == NULL)
    return (status = ERR_NOTINITIALIZED);

  // Check parameters
  if (getImage == NULL)
    return (status = ERR_NULLPARAMETER);

  // Now make sure the device driver getImage routine has been 
  // installed
  if (ops->driverGetImage == NULL)
    {
      kernelError(kernel_error, "The driver routine is NULL");
      return (status = ERR_NOSUCHFUNCTION);
    }

  // Ok, now we can call the routine.
  status =
    ops->driverGetImage(buffer, getImage, xCoord, yCoord, width, height);
  return (status);
}


int kernelGraphicGetKernelImage(kernelGraphicBuffer *buffer, image *getImage,
				int xCoord, int yCoord, int width, int height)
{
  // This is a wrapper for the kernelGraphicGetImage and
  // kernelGraphicImageToKernel functions, that should be used in the
  // kernel if one wants the image memory to be in kernel space (i.e.
  // window system icons and the like)

  int status = 0;

  // The functions we're calling will check parameters

  // Get the application space image
  status =
    kernelGraphicGetImage(buffer, getImage, xCoord, yCoord, width, height);
  if (status < 0)
    return (status);

  // Convert it to kernel space
  status = kernelGraphicImageToKernel(getImage);
  return (status);
}


int kernelGraphicDrawText(kernelGraphicBuffer *buffer, color *foreground,
			  color *background, kernelAsciiFont *font,
			  const char *text, drawMode mode,
			  int xCoord, int yCoord)
{
  // Draws a line of text using the supplied ASCII font at the requested
  // coordinates.  Uses the default foreground and background colors.

  int status = 0;
  int length = 0;
  int index = 0;
  int count;

  // Make sure we've been initialized
  if (systemAdapter == NULL)
    return (status = ERR_NOTINITIALIZED);

  // Check parameters
  if ((font == NULL) || (text == NULL) || (foreground == NULL))
    return (status = ERR_NULLPARAMETER);

  // Now make sure the device driver drawImage routine has been installed
  if (ops->driverDrawMonoImage == NULL)
    {
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
      status =
	ops->driverDrawMonoImage(buffer, &(font->chars[index]), mode,
				 foreground, background, xCoord, yCoord);

      xCoord += font->chars[index].width;
    }

  return (status = 0);
}


int kernelGraphicCopyArea(kernelGraphicBuffer *buffer, int xCoord1,
			  int yCoord1, int width, int height, int xCoord2,
			  int yCoord2)
{
  // Copies the requested area of the screen to the new location.

  int status = 0;
  
  // Make sure we've been initialized
  if (systemAdapter == NULL)
    return (status = ERR_NOTINITIALIZED);

  // Now make sure the device driver drawImage routine has been installed
  if (ops->driverCopyArea == NULL)
    {
      kernelError(kernel_error, "The driver routine is NULL");
      return (status = ERR_NOSUCHFUNCTION);
    }

  // Call the driver routine to copy the area
  status = ops->driverCopyArea(buffer, xCoord1, yCoord1, width, height,
			       xCoord2, yCoord2);
  return (status);
}


int kernelGraphicClearArea(kernelGraphicBuffer *buffer, color *background,
			   int xCoord, int yCoord, int width, int height)
{
  // Clears the requested area of the screen.  This is just a convenience
  // function that draws a filled rectangle over the spot using the
  // background color

  int status = 0;
  
  // Make sure we've been initialized
  if (systemAdapter == NULL)
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
			      int drawY, int clipX, int clipY, int clipWidth,
			      int clipHeight)
{
  // Take a kernelGraphicBuffer and render it on the screen
  int status = 0;
  
  // Make sure we've been initialized
  if (systemAdapter == NULL)
    return (status = ERR_NOTINITIALIZED);

  // Buffer is not allowed to be NULL this time
  if (buffer == NULL)
    {
      kernelError(kernel_error, "NULL buffer parameter");
      return (status = ERR_NULLPARAMETER);
    }

  // Make sure the clip is fully inside the buffer
  if (clipX < 0)
    {
      clipWidth += clipX;
      clipX = 0;
    }
  if (clipY < 0)
    {
      clipHeight += clipY;
      clipY = 0;
    }
  if ((clipX + clipWidth) >= buffer->width)
    clipWidth = (buffer->width - clipX);
  if ((clipY + clipHeight) >= buffer->height)
    clipHeight = (buffer->height - clipY);
  if ((clipWidth <= 0) || (clipHeight <= 0))
    return (status = 0);

  // Now make sure the device driver drawImage routine has been installed
  if (ops->driverRenderBuffer == NULL)
    {
      kernelError(kernel_error, "The driver routine is NULL");
      return (status = ERR_NOSUCHFUNCTION);
    }

  // Call the driver routine to render the buffer
  status = ops->driverRenderBuffer(buffer, drawX, drawY, clipX, clipY,
				   clipWidth, clipHeight);
  return (status);
}


int kernelGraphicFilter(kernelGraphicBuffer *buffer, color *filterColor,
			int xCoord, int yCoord, int width, int height)
{
  // Take an area of a buffer and average it with the supplied color

  int status = 0;

  // Make sure we've been initialized
  if (systemAdapter == NULL)
    return (status = ERR_NOTINITIALIZED);

  // Color not NULL
  if (filterColor == NULL)
    return (status = ERR_NULLPARAMETER);

  // Zero size?
  if (!width || !height)
    return (status = 0);

  // Now make sure the device driver filter routine has been installed
  if (ops->driverFilter == NULL)
    {
      kernelError(kernel_error, "The driver routine is NULL");
      return (status = ERR_NOSUCHFUNCTION);
    }

  // Ok, now we can call the routine.
  status =
    ops->driverFilter(buffer, filterColor, xCoord, yCoord, width, height);
  return (status);
}


void kernelGraphicDrawGradientBorder(kernelGraphicBuffer *buffer, int drawX,
				     int drawY, int width, int height,
				     int thickness, color *drawColor,
				     int shadingIncrement, drawMode mode)
{
  // Draws a gradient border

  color tmpColor;
  int drawRed = 0, drawGreen = 0, drawBlue = 0;
  int count;

  if (drawColor)
    kernelMemCopy(drawColor, &tmpColor, sizeof(color));
  else
    {
      tmpColor.red = kernelDefaultBackground.red;
      tmpColor.green = kernelDefaultBackground.green;
      tmpColor.blue = kernelDefaultBackground.blue;
    }
  drawColor = &tmpColor;

  // These are the starting points of the 'inner' border lines
  int leftX = (drawX + thickness);
  int rightX = (drawX + width - thickness - 1);
  int topY = (drawY + thickness);
  int bottomY = (drawY + height - thickness - 1);

  if (mode == draw_reverse)
    shadingIncrement *= -1;

  // The top and left
  for (count = thickness; count > 0; count --)
    {
      drawRed =	(drawColor->red + (count * shadingIncrement));
      if (drawRed > 255)
	drawRed = 255;
      if (drawRed < 0)
	drawRed = 0;

      drawGreen = (drawColor->green + (count * shadingIncrement));
      if (drawGreen > 255)
	drawGreen = 255;
      if (drawGreen < 0)
	drawGreen = 0;

      drawBlue = (drawColor->blue + (count * shadingIncrement));
      if (drawBlue > 255)
	drawBlue = 255;
      if (drawBlue < 0)
	drawBlue = 0;

      // Top
      kernelGraphicDrawLine(buffer, &((color){drawBlue, drawGreen, drawRed}),
			    draw_normal, (leftX - count), (topY - count),
			    (rightX + count), (topY - count));
      // Left
      kernelGraphicDrawLine(buffer, &((color){drawBlue, drawGreen, drawRed}),
			    draw_normal, (leftX - count), (topY - count),
			    (leftX - count), (bottomY + count));
    }

  shadingIncrement *= -1;

  // The bottom and right
  for (count = thickness; count > 0; count --)
    {
      drawRed =	(drawColor->red + (count * shadingIncrement));
      if (drawRed > 255)
	drawRed = 255;
      if (drawRed < 0)
	drawRed = 0;

      drawGreen = (drawColor->green + (count * shadingIncrement));
      if (drawGreen > 255)
	drawGreen = 255;
      if (drawGreen < 0)
	drawGreen = 0;

      drawBlue = (drawColor->blue + (count * shadingIncrement));
      if (drawBlue > 255)
	drawBlue = 255;
      if (drawBlue < 0)
	drawBlue = 0;
 
      // Bottom
      kernelGraphicDrawLine(buffer, &((color){drawBlue, drawGreen, drawRed}),
			    draw_normal, (leftX - count), (bottomY + count),
			    (rightX + count), (bottomY + count));
      // Right
      kernelGraphicDrawLine(buffer, &((color){drawBlue, drawGreen, drawRed}),
			    draw_normal, (rightX + count), (topY - count),
			    (rightX + count), (bottomY + count));
    }

  return;
}
