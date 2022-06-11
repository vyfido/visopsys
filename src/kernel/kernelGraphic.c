//
//  Visopsys
//  Copyright (C) 1998-2011 J. Andrew McLaughlin
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
#include "kernelDebug.h"
#include "kernelError.h"
#include "kernelFile.h"
#include "kernelFont.h"
#include "kernelLog.h"
#include "kernelMalloc.h"
#include "kernelMisc.h"
#include "kernelPage.h"
#include "kernelParameters.h"
#include "kernelText.h"
#include "kernelWindow.h"
#include <stdlib.h>
#include <string.h>
#include <sys/image.h>

// The global default colors
color kernelDefaultForeground = {
  DEFAULT_FOREGROUND_BLUE, DEFAULT_FOREGROUND_GREEN, DEFAULT_FOREGROUND_RED
};
color kernelDefaultBackground = {
  DEFAULT_BACKGROUND_BLUE, DEFAULT_BACKGROUND_GREEN, DEFAULT_BACKGROUND_RED
};
color kernelDefaultDesktop = {
  DEFAULT_DESKTOP_BLUE, DEFAULT_DESKTOP_GREEN, DEFAULT_DESKTOP_RED
};

static kernelDevice *systemAdapter = NULL;
static kernelGraphicAdapter *adapterDevice = NULL;
static kernelGraphicOps *ops = NULL;

#define VBE_PMINFOBLOCK_SIG "PMID"

typedef struct {
  char signature[4];
  unsigned short entryOffset;
  unsigned short initOffset;
  unsigned short dataSelector;
  unsigned short A0000Selector;
  unsigned short B0000Selector;
  unsigned short B8000Selector;
  unsigned short codeSelector;
  unsigned char protMode;
  unsigned char checksum;

} __attribute__((packed)) vbePmInfoBlock;


static int detectVbe(void)
{
  int status = 0;
  void *biosOrig = NULL;
  vbePmInfoBlock *pmInfo = NULL;
  char checkSum = 0;
  char *tmp = NULL;
  int count1, count2;

  kernelDebug(debug_io, "VBE: detecting VBE protected mode interface");

  // Map the video BIOS image into memory.  Starts at 0xC0000 and 'normally' is
  // 32Kb according to the VBE 3.0 spec (but not really in my experience)
  status = kernelPageMapToFree(KERNELPROCID, (void *) VIDEO_BIOS_MEMORY,
			       &biosOrig, VIDEO_BIOS_MEMORY_SIZE);
  if (status < 0)
    return (status);

  // Scan the video BIOS memory for the "protected mode info block" structure
  kernelDebug(debug_io, "VBE: searching for VBE BIOS pmInfo signature");
  for (count1 = 0; count1 < VIDEO_BIOS_MEMORY_SIZE; count1 ++)
    {
      tmp = (biosOrig + count1);

      if (strncmp((char *) tmp, VBE_PMINFOBLOCK_SIG, 4))
	continue;

      // Maybe we found it
      kernelDebug(debug_io, "VBE: found possible pmInfo signature at %x",
		  count1);

      // Check the checksum
      for (count2 = 0; count2 < (int) sizeof(vbePmInfoBlock); count2 ++)
	checkSum += tmp[count2];
      if (checkSum)
	{
	  kernelDebug(debug_io, "VBE: pmInfo checksum failed (%d)", checkSum);
	  continue;
	}

      // Found it
      pmInfo = (vbePmInfoBlock *) tmp;
      kernelLog("VBE: VESA BIOS extension signature found at %x", count1);
      break;
    }
  
  if (!pmInfo)
    {
      kernelDebug(debug_io, "VBE: pmInfo signature not found");
      status = 0;
      goto out;
    }

  status = 0;

 out:
  // Unmap the video BIOS
  kernelPageUnmap(KERNELPROCID, biosOrig, VIDEO_BIOS_MEMORY_SIZE);

  return (status);
}


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
  // This is data for a temporary console when we first arrive in a
  // graphical mode
  kernelTextArea *tmpConsole = NULL;
  kernelTextInputStream *inputStream = NULL;
  kernelWindowComponent *component = NULL;
  kernelGraphicBuffer *buffer = NULL;

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
  tmpConsole = kernelTextAreaNew(80, 50, 1, TEXT_DEFAULT_SCROLLBACKLINES);
  if (tmpConsole == NULL)
    // Better not try to print any error messages...
    return (status = ERR_NOTINITIALIZED);

  // Assign some extra things to the text area
  tmpConsole->foreground.blue = 255;
  tmpConsole->foreground.green = 255;
  tmpConsole->foreground.red = 255;
  kernelMemCopy(&kernelDefaultDesktop, (color *) &(tmpConsole->background),
		sizeof(color));

  // Change the input and output streams to the console
  inputStream = tmpConsole->inputStream;
  if (inputStream)
    {
      if (inputStream->s.buffer)
	{
	  kernelFree((void *) inputStream->s.buffer);
	  inputStream->s.buffer = NULL;
	}
	  
      kernelFree((void *) tmpConsole->inputStream);
      tmpConsole->inputStream = kernelTextGetConsoleInput();
    }
  if (tmpConsole->outputStream)
    {
      kernelFree((void *) tmpConsole->outputStream);
      tmpConsole->outputStream = kernelTextGetConsoleOutput();
    }

  // Get a NULL kernelWindowComponent to attach the graphic buffer to
  component = kernelMalloc(sizeof(kernelWindowComponent));
  if (component == NULL)
    // Better not try to print any error messages...
    return (status = ERR_NOTINITIALIZED);
  tmpConsole->windowComponent = component;

  // Get a graphic buffer and attach it to the component
  buffer = kernelMalloc(sizeof(kernelGraphicBuffer));
  if (buffer == NULL)
    {
      // Better not try to print any error messages...
      kernelFree((void *) component);
      return (status = ERR_NOTINITIALIZED);
    }

  buffer->width = adapterDevice->xRes;
  buffer->height = adapterDevice->yRes;
  buffer->data = adapterDevice->framebuffer;

  component->buffer = buffer;

  // Initialize the font functions
  status = kernelFontInitialize();
  if (status < 0)
    {
      kernelError(kernel_error, "Font initialization failed");
      return (status);
    }

  // Assign the default system font to our console text area
  kernelFontGetDefault((asciiFont **) &(tmpConsole->font));

  // Switch the console
  kernelTextSwitchToGraphics(tmpConsole);

  // Clear the screen with our default background color
  ops->driverClearScreen(&kernelDefaultDesktop);

  // Try to detect VBE BIOS extensions
  detectVbe();

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

  // Do we need to gather alpha channel data?
  if ((mode == draw_alphablend) && !drawImage->alpha)
    {
      status = kernelImageGetAlpha(drawImage);
      if (status < 0)
	return (status);
    }

  // Ok, now we can call the routine.
  status = ops->driverDrawImage(buffer, drawImage, mode, xCoord, yCoord,
				xOffset, yOffset, width, height);
  return (status);
}


int kernelGraphicDrawText(kernelGraphicBuffer *buffer, color *foreground,
			  color *background, asciiFont *font,
			  const char *text, drawMode mode, int xCoord,
			  int yCoord)
{
  // Draws a line of text using the supplied ASCII font at the requested
  // coordinates.  Uses the default foreground and background colors.

  int status = 0;
  int length = 0;
  unsigned idx = 0;
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
      idx = (unsigned char) text[count];

      if (font->chars[idx].data)
	// Call the driver routine to draw the character
	status =
	  ops->driverDrawMonoImage(buffer, &(font->chars[idx]), mode,
				   foreground, background, xCoord, yCoord);

      xCoord += font->chars[idx].width;
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

  // Color not NULL
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
				     int shadingIncrement, drawMode mode,
				     borderType type)
{
  // Draws a gradient border

  color tmpColor;
  int drawRed = 0, drawGreen = 0, drawBlue = 0;
  int count;

  if (drawColor)
    kernelMemCopy(drawColor, &tmpColor, sizeof(color));
  else
    kernelMemCopy(&kernelDefaultBackground, &tmpColor, sizeof(color));

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
      if (type & border_top)
	kernelGraphicDrawLine(buffer, &((color){drawBlue, drawGreen, drawRed}),
			      draw_normal, (leftX - count), (topY - count),
			      (rightX + count), (topY - count));
      // Left
      if (type & border_left)
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
      if (type & border_bottom)
	kernelGraphicDrawLine(buffer, &((color){drawBlue, drawGreen, drawRed}),
			      draw_normal, (leftX - count), (bottomY + count),
			      (rightX + count), (bottomY + count));
      // Right
      if (type & border_right)
	kernelGraphicDrawLine(buffer, &((color){drawBlue, drawGreen, drawRed}),
			      draw_normal, (rightX + count), (topY - count),
			      (rightX + count), (bottomY + count));
    }

  return;
}


void kernelGraphicConvexShade(kernelGraphicBuffer *buffer, color *drawColor,
			      int drawX, int drawY, int width, int height,
			      shadeType type)
{
  // Given an buffer, area, color, and shading mode, shade the area as a
  // 3D-like, convex object.

  color tmpColor;
  int outerDiff = 30;
  int centerDiff = 10;
  int increment = ((outerDiff - centerDiff) / (height / 2));
  int limit = 0;
  int count;

  if (drawColor)
    kernelMemCopy(drawColor, &tmpColor, sizeof(color));
  else
    kernelMemCopy(&kernelDefaultBackground, &tmpColor, sizeof(color));

  drawColor = &tmpColor;

  if ((type == shade_fromtop) || (type == shade_frombottom))
    limit = height;
  else
    limit = width;

  increment = max(((outerDiff - centerDiff) / (limit / 2)), 3);
  outerDiff = max(outerDiff, (centerDiff + (increment * (limit / 2))));

  if ((type == shade_fromtop) || (type == shade_fromleft))
    {
      drawColor->red = min((drawColor->red + outerDiff), 0xFF);
      drawColor->green = min((drawColor->green + outerDiff), 0xFF);
      drawColor->blue = min((drawColor->blue + outerDiff), 0xFF);
    }
  else
    {
      drawColor->red = max((drawColor->red - outerDiff), 0);
      drawColor->green = max((drawColor->green - outerDiff), 0);
      drawColor->blue = max((drawColor->blue - outerDiff), 0);
    }

  for (count = 0; count < limit; count ++)
    {
      if ((type == shade_fromtop) || (type == shade_frombottom))
	kernelGraphicDrawLine(buffer, drawColor, draw_normal, drawX,
			      (drawY + count), (drawX + width - 1),
			      (drawY + count));
      else
	kernelGraphicDrawLine(buffer, drawColor, draw_normal,
			      (drawX + count), drawY, (drawX + count),
			      (drawY + height - 1));

      if ((type == shade_fromtop) || (type == shade_fromleft))
	{
	  if (count == ((limit / 2) - 1))
	    {
	      drawColor->red = max((drawColor->red - (centerDiff * 2)), 0);
	      drawColor->green = max((drawColor->green - (centerDiff * 2)), 0);
	      drawColor->blue =	max((drawColor->blue - (centerDiff * 2)), 0);
	    }
	  else
	    {
	      drawColor->red = max((drawColor->red - increment), 0);
	      drawColor->green = max((drawColor->green - increment), 0);
	      drawColor->blue = max((drawColor->blue - increment), 0);
	    }
	}
      else
	{
	  if (count == ((limit / 2) - 1))
	    {
	      drawColor->red = min((drawColor->red + (centerDiff  * 2)), 0xFF);
	      drawColor->green =
		min((drawColor->green + (centerDiff * 2)), 0xFF);
	      drawColor->blue =
		min((drawColor->blue + (centerDiff * 2)), 0xFF);
	    }
	  else
	    {
	      drawColor->red = min((drawColor->red + increment), 0xFF);
	      drawColor->green = min((drawColor->green + increment), 0xFF);
	      drawColor->blue = min((drawColor->blue + increment), 0xFF);
	    }
	}
    }

  return;
}
