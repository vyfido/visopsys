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
//  kernelFramebufferGraphicDriver.c
//

// This is the simple graphics driver for a LFB (Linear Framebuffer)
// -equipped graphics adapter

#include "kernelDriverManagement.h" // Contains my prototypes
#include "kernelMemoryManager.h"
#include "kernelMiscAsmFunctions.h"
#include "kernelError.h"
#include <sys/errors.h>


static kernelGraphicAdapterObject *adapter = NULL;
static int mask = 0;
static int bytesPerPixel = 0;
static int intBits = (sizeof(int) * 8);

static kernelGraphicBuffer wholeScreen;


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


int kernelLFBGraphicDriverInitialize(void *device)
{
  // The standard initialization stuff

  int status = 0;

  // Save the reference to the device information
  adapter = device;

  bytesPerPixel = (adapter->bitsPerPixel / 8);
  
  // Use this to mask off the appropriate number of bits for each pixel
  mask = (-1 << (intBits - adapter->bitsPerPixel));

  // Set up the kernelGraphicBuffer that represents the whole screen
  wholeScreen.width = adapter->xRes;
  wholeScreen.height = adapter->yRes;
  wholeScreen.data = adapter->framebuffer;

  return (status = 0);
}


int kernelLFBGraphicDriverClearScreen(color *background)
{
  // Resets the whole screen to the background color
  
  int status = 0;
  int pixels = (adapter->xRes * adapter->yRes);
  int count;
  
  // Set everything to the background color
  for (count = 0; count < (pixels * adapter->bitsPerPixel) / 8; )
    {
      ((char *) adapter->framebuffer)[count++] = background->blue;
      ((char *) adapter->framebuffer)[count++] = background->green;
      ((char *) adapter->framebuffer)[count++] = background->red;
    }

  return (status = 0);
}


int kernelLFBGraphicDriverDrawPixel(kernelGraphicBuffer *buffer,
				    color *foreground, drawMode mode,
				    int xCoord, int yCoord)
{
  // Draws a single pixel to the graphic buffer using the preset foreground
  // color

  int status = 0;
  unsigned char *framebufferPointer = NULL;

  // If the supplied kernelGraphicBuffer is NULL, we draw directly to the
  // whole screen
  if (buffer == NULL)
    buffer = &wholeScreen;

  // Make sure we're not drawing off the screen
  if ((xCoord < 0) || (xCoord >= buffer->width) ||
      (yCoord < 0) || (yCoord >= buffer->height))
    return (status = 0);

  // Draw the pixel using the supplied color
  framebufferPointer = buffer->data +
    ((((buffer->width * xCoord) + xCoord) * adapter->bitsPerPixel) / 8);
	
  if (mode == draw_normal)
    {
      framebufferPointer[0] = foreground->blue;
      framebufferPointer[1] = foreground->green;
      framebufferPointer[2] = foreground->red;
    }
  else
    {
      framebufferPointer[0] ^= foreground->blue;
      framebufferPointer[1] ^= foreground->green;
      framebufferPointer[2] ^= foreground->red;
    }

  return (status = 0);
}


int kernelLFBGraphicDriverDrawLine(kernelGraphicBuffer *buffer,
				   color *foreground, drawMode mode,
				   int startX, int startY, int endX, int endY)
{
  // Draws a line on the screen using the preset foreground color

  int status = 0;
  unsigned lineLength = 0;
  unsigned lineBytes = 0;
  unsigned char *framebufferPointer = NULL;
  int count;

  // If the supplied kernelGraphicBuffer is NULL, we draw directly to the
  // whole screen
  if (buffer == NULL)
    buffer = &wholeScreen;

  // Is it a horizontal line?
  if (startY == endY)
    {     
      // This is an easy line to draw.

      // If the Y location is off the screen, skip it
      if ((startY < 0) || (startY >= buffer->height))
	return (status = 0);

      // If the line goes off the edge of the screen, only attempt to
      // display what will fit
      if (startX < 0)
	startX = 0;
      if (endX < buffer->width)
	lineLength = (endX - startX + 1);
      else
	lineLength = (buffer->width - startX);
      
      // How many bytes in the line?
      lineBytes = ((adapter->bitsPerPixel * lineLength) / 8);
      if (((adapter->bitsPerPixel * lineLength) % 8))
	lineBytes += 1;

      framebufferPointer = buffer->data +
	((((buffer->width * startY) + startX) * adapter->bitsPerPixel) / 8);

      // Do a loop through the line, copying the color values
      // consecutively
      for (count = 0; count < lineBytes; )
	if (mode == draw_normal)
	  {
	    framebufferPointer[count++] = foreground->blue;
	    framebufferPointer[count++] = foreground->green;
	    framebufferPointer[count++] = foreground->red;
	  }
	else
	  {
	    framebufferPointer[count++] ^= foreground->blue;
	    framebufferPointer[count++] ^= foreground->green;
	    framebufferPointer[count++] ^= foreground->red;
	  }
    }

  // Is it a vertical line?
  else if (startX == endX)
    {
      // This is an easy line to draw.

      // If the X location is off the screen, skip it
      if ((startX < 0) || (startX >= buffer->width))
	return (status = 0);

      // If the line goes off the bottom edge of the screen, only attempt to
      // display what will fit
      if (startY < 0)
	startY = 0;
      if (endY < buffer->height)
	lineLength = (endY - startY + 1);
      else
	lineLength = (buffer->height - startY);
      
      framebufferPointer = buffer->data +
	((((buffer->width * startY) + startX) * adapter->bitsPerPixel) / 8);

      // Do a loop through the line, copying the color values
      // into each row
      for (count = 0; count < lineLength; count ++)
	{
	  if (mode == draw_normal)
	    {
	      framebufferPointer[0] = foreground->blue;
	      framebufferPointer[1] = foreground->green;
	      framebufferPointer[2] = foreground->red;
	    }
	  else
	    {
	      framebufferPointer[0] ^= foreground->blue;
	      framebufferPointer[1] ^= foreground->green;
	      framebufferPointer[2] ^= foreground->red;
	    }

	  framebufferPointer += ((buffer->width * adapter->bitsPerPixel) / 8);
	}
    }

  // It's not horizontal or vertical.  We will use a Bresenham algorithm
  // to make the line
  else
    {
    }

  return (status = 0);
}


int kernelLFBGraphicDriverDrawRect(kernelGraphicBuffer *buffer,
				   color *foreground, drawMode mode,
				   int startX, int startY,
				   unsigned width, unsigned height,
				   unsigned thickness, int fill)
{
  // Draws a rectangle on the screen using the preset foreground color

  int status = 0;
  int endX, endY;
  int count;

  // If the supplied kernelGraphicBuffer is NULL, we draw directly to the
  // whole screen
  if (buffer == NULL)
    buffer = &wholeScreen;

  // TEMP TEMP TEMP: hack.  Don't do it this way.  Terrible performance.

  endX = (startX + (width - 1));
  endY = (startY + (height - 1));

  // If this is a filled rectangle, just draw a bunch of horizontal lines
  if (fill)
    {
      for (count = startY; count <= endY; count ++)
	kernelLFBGraphicDriverDrawLine(buffer, foreground, mode, startX, count,
				       endX, count);
      return (status = 0);
    }

  else
    {
      // Draw the top line 'thickness' times
      for (count = (startY + thickness - 1); count >= startY; count --)
	kernelLFBGraphicDriverDrawLine(buffer, foreground, mode, startX, count,
				       endX, count);

      // Draw the left line 'thickness' times
      for (count = (startX + thickness - 1); count >= startX; count --)
	kernelLFBGraphicDriverDrawLine(buffer, foreground, mode, count,
				       (startY + thickness), count,
				       (endY - thickness));

      // Draw the bottom line 'thickness' times
      for (count = (endY - thickness + 1); count <= endY; count ++)
	kernelLFBGraphicDriverDrawLine(buffer, foreground, mode, startX, count,
				       endX, count);

      // Draw the right line 'thickness' times
      for (count = (endX - thickness + 1); count <= endX; count ++)
	kernelLFBGraphicDriverDrawLine(buffer, foreground, mode, count,
				       (startY + thickness), count,
				       (endY - thickness));
    }

  return (status = 0);
}


int kernelLFBGraphicDriverDrawOval(kernelGraphicBuffer *buffer,
				   color *foreground, drawMode mode,
				   int centerX, int centerY, unsigned width,
				   unsigned height, unsigned thickness,
				   int fill)
{
  // Draws an oval on the screen using the preset foreground color

  int status = 0;

  // If the supplied kernelGraphicBuffer is NULL, we draw directly to the
  // whole screen
  if (buffer == NULL)
    buffer = &wholeScreen;

  return (status = 0);
}


int kernelLFBGraphicDriverDrawMonoImage(kernelGraphicBuffer *buffer,
					image *drawImage,
					color *foreground, color *background,
					int xCoord, int yCoord)
{
  // Draws the supplied image on the screen at the requested coordinates

  int status = 0;
  unsigned lineLength = 0;
  unsigned lineBytes = 0;
  unsigned numberLines = 0;
  unsigned char *framebufferPointer = NULL;
  unsigned char *monoImageData = NULL;
  unsigned lineCounter = 0;
  unsigned pixelCounter = 0;
  int count;

  // If the supplied kernelGraphicBuffer is NULL, we draw directly to the
  // whole screen
  if (buffer == NULL)
    buffer = &wholeScreen;

  // Check params
  if ((xCoord < 0) || (xCoord >= buffer->width) ||
      (yCoord < 0) || (yCoord >= buffer->height))
    return (status = ERR_BOUNDS);

  // Make sure it's a mono image
  if (drawImage->type != IMAGETYPE_MONO)
    return (status = ERR_INVALID);

  // If the image goes off the right edge of the screen, only attempt to
  // display what will fit
  if ((xCoord + drawImage->width) < buffer->width)
    lineLength = drawImage->width;
  else
    lineLength = (buffer->width - xCoord);

  // If the image goes off the bottom of the screen, only show the
  // lines that will fit
  if ((yCoord + drawImage->height) < buffer->height)
    numberLines = drawImage->height;
  else
    numberLines = (buffer->height - yCoord);
  
  // images are lovely little data structures that give us image
  // data in the most convenient form we can imagine.

  // How many bytes in a line of data?
  lineBytes = ((adapter->bitsPerPixel * lineLength) / 8);
  if (((adapter->bitsPerPixel * lineLength) % 8))
    lineBytes += 1;

  framebufferPointer = buffer->data +
    ((((buffer->width * yCoord) + xCoord) * adapter->bitsPerPixel) / 8);

  // A mono image has a bitmap of 'on' bits and 'off' bits.  We will
  // draw all 'on' bits using the current foreground color.
  monoImageData = (unsigned char *) drawImage->data;

  // Loop for each line
  for (lineCounter = 0; lineCounter < numberLines; lineCounter++)
    {
      // Do a loop through the line, copying either the foreground color
      // value or the background color into framebuffer
      for (count = 0; count < lineBytes; )
	{
	  // Isolate the bit from the bitmap
	  if ((monoImageData[pixelCounter / 8] &
	       (0x80 >> (pixelCounter % 8))) != 0)
	    {
	      // 'on' bit.
	      framebufferPointer[count++] = foreground->blue;
	      framebufferPointer[count++] = foreground->green;
	      framebufferPointer[count++] = foreground->red;
	    }
	  else
	    {
	      // 'off' bit.  If the image is translucent, don't draw
	      // anything.  Otherwise draw the background color.
	      if (drawImage->isTranslucent)
		count += 3;
	      else
		{
		  framebufferPointer[count++] = background->blue;
		  framebufferPointer[count++] = background->green;
		  framebufferPointer[count++] = background->red;
		}
	    }

	  pixelCounter += 1;
	}

      // Move to the next line in the framebuffer
      framebufferPointer += ((buffer->width * adapter->bitsPerPixel) / 8);
      
      // Are we skipping any because it's off the screen?
      if (drawImage->width > lineLength)
	pixelCounter += (drawImage->width - lineLength);
    }

  // Success
  return (status = 0);
}


int kernelLFBGraphicDriverDrawImage(kernelGraphicBuffer *buffer,
				    image *drawImage, int xCoord,
				    int yCoord)
{
  // Draws the supplied image on the screen at the requested coordinates

  int status = 0;
  unsigned xOffset = 0;
  unsigned yOffset = 0;
  unsigned lineLength = 0;
  unsigned lineBytes = 0;
  unsigned numberLines = 0;
  unsigned char *framebufferPointer = NULL;
  pixel *imageData = NULL;
  unsigned lineCounter = 0;
  unsigned pixelCounter = 0;
  int count;

  // If the supplied kernelGraphicBuffer is NULL, we draw directly to the
  // whole screen
  if (buffer == NULL)
    buffer = &wholeScreen;

  // If the image is outside the buffer entirely, skip it
  if ((xCoord >= (int) buffer->width) ||
      (yCoord >= (int) buffer->height))
    return (status = ERR_BOUNDS);

  // Make sure it's a color image
  if (drawImage->type == IMAGETYPE_MONO)
    return (status = ERR_INVALID);

  lineLength = drawImage->width;

  // If the image goes off the sides of the screen, only attempt to
  // display the pixels that will fit
  if (xCoord < 0)
    {
      lineLength += xCoord;
      xOffset = -xCoord;
      xCoord = 0;
    }
  if ((xCoord + lineLength) >= buffer->width)
    lineLength -= ((xCoord + lineLength) - buffer->width);

  numberLines = drawImage->height;

  // If the image goes off the top or bottom of the screen, only show the
  // lines that will fit
  if (yCoord < 0)
    {
      numberLines += yCoord;
      yOffset = -yCoord;
      yCoord = 0;
    }
  if ((yCoord + numberLines) >= buffer->height)
    numberLines -= ((yCoord + numberLines) - buffer->height);

  // images are lovely little data structures that give us image
  // data in the most convenient form we can imagine.

  // How many bytes in a line of data?
  lineBytes = ((adapter->bitsPerPixel * lineLength) / 8);
  if (((adapter->bitsPerPixel * lineLength) % 8))
    lineBytes += 1;

  framebufferPointer = buffer->data +
    ((((buffer->width * yCoord) + xCoord) * adapter->bitsPerPixel) / 8);

  imageData = (pixel *)
    (drawImage->data + (((yOffset * drawImage->width) + xOffset) * 3));
  
  // Loop for each line
  for (lineCounter = 0; lineCounter < numberLines; lineCounter++)
    {	  
      // Do a loop through the line, copying the color values from the
      // image into the framebuffer

      for (count = 0; count < lineBytes; )
	{
	  if ((drawImage->isTranslucent) &&
	      (imageData[pixelCounter].red ==
	       drawImage->translucentColor.red) &&
	      (imageData[pixelCounter].green ==
	       drawImage->translucentColor.green) &&
	      (imageData[pixelCounter].blue ==
	       drawImage->translucentColor.blue))
	    count += 3;
	  else
	    {
	      framebufferPointer[count++] = imageData[pixelCounter].blue;
	      framebufferPointer[count++] = imageData[pixelCounter].green;
	      framebufferPointer[count++] = imageData[pixelCounter].red;
	    }

	  pixelCounter += 1;
	}

      // Move to the next line in the framebuffer
      framebufferPointer += ((buffer->width * adapter->bitsPerPixel) / 8);
      
      // Are we skipping any of this line because it's off the screen?
      if (drawImage->width > lineLength)
	pixelCounter += (drawImage->width - lineLength);
    }

  // Success
  return (status = 0);
}


int kernelLFBGraphicDriverGetImage(kernelGraphicBuffer *buffer,
				   image *getImage, int xCoord,
				   int yCoord, unsigned width,
				   unsigned height)
{
  // Draws the supplied image on the screen at the requested coordinates

  int status = 0;
  unsigned numberPixels = 0;
  unsigned lineLength = 0;
  unsigned numberLines = 0;
  unsigned lineBytes = 0;
  unsigned char *framebufferPointer = NULL;
  pixel *imageData = NULL;
  unsigned lineCounter = 0;
  unsigned pixelCounter = 0;
  int count;

  // If the supplied kernelGraphicBuffer is NULL, we draw directly to the
  // whole screen
  if (buffer == NULL)
    buffer = &wholeScreen;

  // Check params
  if ((xCoord < 0) || (xCoord >= buffer->width) ||
      (yCoord < 0) || (yCoord >= buffer->height))
    return (status = ERR_BOUNDS);

  // If the buffer goes off the right edge of the screen, only attempt to
  // grab what we're really showing
  if ((xCoord + width) < buffer->width)
    lineLength = width;
  else
    lineLength = (buffer->width - xCoord);

  // If the buffer goes off the bottom of the screen, only attempt to
  // grab what we're really showing
  if ((height + yCoord) < buffer->height)
    numberLines = height;
  else
    numberLines = (buffer->height - yCoord);
  
  // How many pixels will there be?
  numberPixels = lineLength * numberLines;
  getImage->dataLength = (numberPixels * sizeof(pixel));

  // If the image was previously holding data, release it
  if (getImage->data == NULL)
    {
      // Allocate enough memory to hold the image data
      getImage->data =
	kernelMemoryRequestBlock(getImage->dataLength, 0, "image data");
      if (getImage->data == NULL)
	{
	  // Eek, no memory
	  kernelError(kernel_error, "Unable to allocate memory for the "
		      "image");
	  return (status = ERR_MEMORY);
	}
    }

  // How many bytes in a line of data?
  lineBytes = ((adapter->bitsPerPixel * lineLength) / 8);
  if (((adapter->bitsPerPixel * lineLength) % 8))
    lineBytes += 1;

  // Figure out the starting memory location in the framebuffer
  framebufferPointer = buffer->data +
    ((((buffer->width * yCoord) + xCoord) * adapter->bitsPerPixel) / 8);

  imageData = (pixel *) getImage->data;

  // Now loop through each line of the buffer, filling the image data from
  // the screen
  for (lineCounter = 0; lineCounter < numberLines; lineCounter++)
    {
      // Do a loop through the line, copying the color values from the
      // framebuffer into the image data
      for (count = 0; count < lineBytes; )
	{
	  imageData[pixelCounter].blue = framebufferPointer[count++];
	  imageData[pixelCounter].green = framebufferPointer[count++];
	  imageData[pixelCounter].red = framebufferPointer[count++];

	  pixelCounter += 1;
	}

      // Move to the next line in the framebuffer
      framebufferPointer += ((buffer->width * adapter->bitsPerPixel) / 8);
    }

  // Fill in the image's vitals
  getImage->type = IMAGETYPE_COLOR;
  getImage->pixels = numberPixels;
  getImage->width = lineLength;
  getImage->height = numberLines;

  return (status = 0);
}


int kernelLFBGraphicDriverCopyArea(kernelGraphicBuffer *buffer, int xCoord1,
				   int yCoord1, unsigned width,
				   unsigned height, int xCoord2, int yCoord2)
{
  // Copy a clip of data from one area of the buffer to another

  int status = 0;
  unsigned char *srcPointer = NULL;
  unsigned char *destPointer = NULL;
  unsigned lineLength = 0;
  int count;

  // If the supplied kernelGraphicBuffer is NULL, we draw directly to the
  // whole screen
  if (buffer == NULL)
    buffer = &wholeScreen;

  srcPointer = buffer->data +
    ((((buffer->width * yCoord1) + xCoord1) * adapter->bitsPerPixel) / 8);
  destPointer = buffer->data +
    ((((buffer->width * yCoord2) + xCoord2) * adapter->bitsPerPixel) / 8);

  lineLength = ((width * adapter->bitsPerPixel) / 8);

  for (count = yCoord1; count <= (yCoord1 + height - 1); count ++)
    {
      kernelMemCopy(srcPointer, destPointer, lineLength);
      srcPointer += ((buffer->width * adapter->bitsPerPixel) / 8);
      destPointer += ((buffer->width * adapter->bitsPerPixel) / 8);
    }

  return (status = 0);
}


int kernelLFBGraphicDriverRenderBuffer(kernelGraphicBuffer *buffer,
				       int drawX, int drawY,
				       int clipX, int clipY,
				       unsigned width, unsigned height)
{
  // Take the supplied graphic buffer and render it onto the screen.

  int status = 0;
  unsigned bytesPerLine = 0;
  void *bufferPointer = NULL;
  void *screenPointer = NULL;

  // This function is the single instance where a NULL buffer is not
  // allowed, since we are drawing the buffer to the screen this time
  if (buffer == NULL)
    return (status = ERR_NULLPARAMETER);

  // Not allowed to specify a clip that is not fully inside the buffer
  if ((clipX < 0) || ((clipX + width) > buffer->width) ||
      (clipY < 0) || ((clipY + height) > buffer->height))
    {
      kernelError(kernel_warn, "Cannot render clip x:%d y:%d width:%d "
		  "height:%d in buffer with dimensions width:%d height:%d)",
		  clipX, clipY, width, height, buffer->width, buffer->height);

      // Render the whole buffer instead
      clipX = 0; clipY = 0; width = buffer->width; height = buffer->height;
    }

  // Get the line length of each line that we want to draw and cut them
  // off if the area will extend past the screen boundaries.
  if ((drawX + clipX) < 0)
    {
      if (-(drawX + clipX) >= width)
	return (status = 0);

      width += (drawX + clipX);
      clipX -= (drawX + clipX);
    }
  if ((drawX + clipX + width) >= wholeScreen.width)
    width = (wholeScreen.width - (drawX + clipX));
  if ((drawY + clipY) < 0)
    {
      if (-(drawY + clipY) >= height)
	return (status = 0);

      height += (drawY + clipY);
      clipY -= (drawY + clipY);
    }
  if ((drawY + clipY + height) >= wholeScreen.height)
    height = (wholeScreen.height - (drawY + clipY));

  // Don't draw if the whole area is off the screen
  if (((drawX + clipX) >= wholeScreen.width) ||
      ((drawY + clipY) >= wholeScreen.height))
    return (status = 0);

  bytesPerLine = ((width * adapter->bitsPerPixel) / 8);

  // Calculate the starting offset inside the buffer
  bufferPointer = buffer->data +
    ((((buffer->width * clipY) + clipX) * adapter->bitsPerPixel) / 8);

  // Calculate the starting offset on the screen
  screenPointer = wholeScreen.data +
    ((((wholeScreen.width * (drawY + clipY)) + drawX + clipX) *
      adapter->bitsPerPixel) / 8);

  // Start copying lines
  for ( ; height > 0; height --)
    {
      kernelMemCopy(bufferPointer, screenPointer, bytesPerLine);
      bufferPointer += ((buffer->width * adapter->bitsPerPixel) / 8);
      screenPointer += ((wholeScreen.width * adapter->bitsPerPixel) / 8);
    }

  return (status = 0);
}
