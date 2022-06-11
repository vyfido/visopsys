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
//  kernelFramebufferGraphicDriver.c
//

// This is the simple graphics driver for a LFB (Linear Framebuffer)
// -equipped graphics adapter

#include "kernelGraphic.h"
#include "kernelMemoryManager.h"
#include "kernelMalloc.h"
#include "kernelMain.h"
#include "kernelMiscFunctions.h"
#include "kernelPageManager.h"
#include "kernelParameters.h"
#include "kernelProcessorX86.h"
#include "kernelError.h"
#include <string.h>

static kernelGraphicAdapter *adapter = NULL;
static kernelGraphicBuffer wholeScreen;


static int driverClearScreen(color *background)
{
  // Resets the whole screen to the background color
  
  int status = 0;
  int pixels = (adapter->xRes * adapter->yRes);
  short pix = 0;
  int count;
  
  // Set everything to the background color

  if (adapter->bitsPerPixel == 32)
    {
      unsigned tmp = ((background->red << 16) | (background->green << 8) |
		      background->blue);
      kernelProcessorWriteDwords(tmp, adapter->framebuffer, pixels);
    }

  else if (adapter->bitsPerPixel == 24)
    for (count = 0; count < (pixels * adapter->bytesPerPixel); )
      {
	((char *) adapter->framebuffer)[count++] = background->blue;
	((char *) adapter->framebuffer)[count++] = background->green;
	((char *) adapter->framebuffer)[count++] = background->red;
      }

  else if ((adapter->bitsPerPixel == 16) || (adapter->bitsPerPixel == 15))
    {
      if (adapter->bitsPerPixel == 16)
	pix = (((background->red >> 3) << 11) |
	       ((background->green >> 2) << 5) |
	       (background->blue >> 3));
      else
	pix = (((background->red >> 3) << 10) |
	       ((background->green >> 3) << 5) |
	       (background->blue >> 3));

      for (count = 0; count < pixels; count ++)
	((short *) adapter->framebuffer)[count] = pix;
    }

  return (status = 0);
}


static int driverDrawPixel(kernelGraphicBuffer *buffer, color *foreground,
			   drawMode mode, int xCoord, int yCoord)
{
  // Draws a single pixel to the graphic buffer using the preset foreground
  // color

  int status = 0;
  unsigned char *framebufferPointer = NULL;
  short pix = 0;

  // If the supplied kernelGraphicBuffer is NULL, we draw directly to the
  // whole screen
  if (buffer == NULL)
    buffer = &wholeScreen;

  // Make sure the pixel is in the buffer
  if ((xCoord >= buffer->width) || (yCoord >= buffer->height))
    // Don't make an error condition, just skip it
    return (status = 0);

  // Make sure we're not drawing off the screen
  if ((xCoord < 0) || (xCoord >= buffer->width) ||
      (yCoord < 0) || (yCoord >= buffer->height))
    return (status = 0);

  // Draw the pixel using the supplied color
  framebufferPointer = buffer->data +
    (((buffer->width * yCoord) + xCoord) * adapter->bytesPerPixel);
	
  if ((adapter->bitsPerPixel == 32) || (adapter->bitsPerPixel == 24))
    {
      if (mode == draw_normal)
	{
	  framebufferPointer[0] = foreground->blue;
	  framebufferPointer[1] = foreground->green;
	  framebufferPointer[2] = foreground->red;
	}
      else if (mode == draw_or)
	{
	  framebufferPointer[0] |= foreground->blue;
	  framebufferPointer[1] |= foreground->green;
	  framebufferPointer[2] |= foreground->red;
	}
      else if (mode == draw_xor)
	{
	  framebufferPointer[0] ^= foreground->blue;
	  framebufferPointer[1] ^= foreground->green;
	  framebufferPointer[2] ^= foreground->red;
	}
    }

  else if ((adapter->bitsPerPixel == 16) || (adapter->bitsPerPixel == 15))
    {
      if (adapter->bitsPerPixel == 16)
	pix = (((foreground->red >> 3) << 11) |
	       ((foreground->green >> 2) << 5) |
	       (foreground->blue >> 3));
      else
	pix = (((foreground->red >> 3) << 10) |
	       ((foreground->green >> 3) << 5) |
	       (foreground->blue >> 3));
      
      if (mode == draw_normal)
	*((short *) framebufferPointer) = pix;
      else if (mode == draw_or)
	*((short *) framebufferPointer) |= pix;
      else if (mode == draw_xor)
	*((short *) framebufferPointer) ^= pix;
    }

  return (status = 0);
}


static int driverDrawLine(kernelGraphicBuffer *buffer, color *foreground,
			  drawMode mode, int startX, int startY,
			  int endX, int endY)
{
  // Draws a line on the screen using the preset foreground color

  int status = 0;
  int lineLength = 0;
  int lineBytes = 0;
  unsigned char *framebufferPointer = NULL;
  short pix = 0;
  int count;

#define SWAP(a, b) do { int tmp = a; a = b; b = tmp; } while (0)

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

      // Make sure startX < endX
      if (startX > endX)
	SWAP(startX, endX);

      // If the line goes off the edge of the screen, only attempt to
      // display what will fit
      if (startX < 0)
	startX = 0;
      if (endX >= buffer->width)
	endX = (buffer->width - 1);
      lineLength = ((endX - startX) + 1);

      // Nothing to do?
      if (lineLength <= 0)
	return (status = 0);
      
      // How many bytes in the line?
      lineBytes = (adapter->bytesPerPixel * lineLength);

      framebufferPointer = buffer->data +
	(((buffer->width * startY) + startX) * adapter->bytesPerPixel);

      // Do a loop through the line, copying the color values consecutively

      if ((adapter->bitsPerPixel == 32) || (adapter->bitsPerPixel == 24))
	{
	  if ((adapter->bitsPerPixel == 24) || (mode == draw_or) ||
	      (mode == draw_xor))
	    for (count = 0; count < lineBytes; )
	      {
		if (mode == draw_normal)
		  {
		    framebufferPointer[count] = foreground->blue;
		    framebufferPointer[count + 1] = foreground->green;
		    framebufferPointer[count + 2] = foreground->red;
		  }
		else if (mode == draw_or)
		  {
		    framebufferPointer[count] |= foreground->blue;
		    framebufferPointer[count + 1] |= foreground->green;
		    framebufferPointer[count + 2] |= foreground->red;
		  }
		else if (mode == draw_xor)
		  {
		    framebufferPointer[count] ^= foreground->blue;
		    framebufferPointer[count + 1] ^= foreground->green;
		    framebufferPointer[count + 2] ^= foreground->red;
		  }
		count += 3;
		if (adapter->bitsPerPixel == 32)
		  count++;
	      }
	  else
	    {
	      unsigned tmp = ((foreground->red << 16) |
	    		      (foreground->green << 8) | foreground->blue);
	      kernelProcessorWriteDwords(tmp, framebufferPointer, lineLength);
	    }
	}

      else if ((adapter->bitsPerPixel == 16) || (adapter->bitsPerPixel == 15))
	{
	  if (adapter->bitsPerPixel == 16)
	    pix = (((foreground->red >> 3) << 11) |
		   ((foreground->green >> 2) << 5) |
		   (foreground->blue >> 3));
	  else
	    pix = (((foreground->red >> 3) << 10) |
		   ((foreground->green >> 3) << 5) |
		   (foreground->blue >> 3));
	  
	  for (count = 0; count < lineLength; count ++)
	    {
	      if (mode == draw_normal)
		((short *) framebufferPointer)[count] = pix;
	      else if (mode == draw_or)
		((short *) framebufferPointer)[count] |= pix;
	      else if (mode == draw_xor)
		((short *) framebufferPointer)[count] ^= pix;
	    }
	}
    }

  // Is it a vertical line?
  else if (startX == endX)
    {
      // This is an easy line to draw.

      // If the X location is off the screen, skip it
      if ((startX < 0) || (startX >= buffer->width))
	return (status = 0);

      // Make sure startY < endY
      if (startY > endY)
	SWAP(startY, endY);

      // If the line goes off the bottom edge of the screen, only attempt to
      // display what will fit
      if (startY < 0)
	startY = 0;
      if (endY >= buffer->height)
	endY = (buffer->height - 1);
      lineLength = ((endY - startY) + 1);
      
      // Nothing to do?
      if (lineLength <= 0)
	return (status = 0);
      
      framebufferPointer = buffer->data +
	(((buffer->width * startY) + startX) * adapter->bytesPerPixel);

      // Do a loop through the line, copying the color values
      // into each row

      if ((adapter->bitsPerPixel == 32) || (adapter->bitsPerPixel == 24))
	for (count = 0; count < lineLength; count ++)
	  {
	    if (mode == draw_normal)
	      {
		framebufferPointer[0] = foreground->blue;
		framebufferPointer[1] = foreground->green;
		framebufferPointer[2] = foreground->red;
	      }
	    else if (mode == draw_or)
	      {
		framebufferPointer[0] |= foreground->blue;
		framebufferPointer[1] |= foreground->green;
		framebufferPointer[2] |= foreground->red;
	      }
	    else if (mode == draw_xor)
	      {
		framebufferPointer[0] ^= foreground->blue;
		framebufferPointer[1] ^= foreground->green;
		framebufferPointer[2] ^= foreground->red;
	      }
	      
	    framebufferPointer +=
	      ((buffer->width * adapter->bitsPerPixel) / 8);
	  }

      else if ((adapter->bitsPerPixel == 16) || (adapter->bitsPerPixel == 15))
	{
	  if (adapter->bitsPerPixel == 16)
	    pix = (((foreground->red >> 3) << 11) |
		   ((foreground->green >> 2) << 5) |
		   (foreground->blue >> 3));
	  else
	    pix = (((foreground->red >> 3) << 10) |
		   ((foreground->green >> 3) << 5) |
		   (foreground->blue >> 3));
	  
	  for (count = 0; count < lineLength; count ++)
	    {
	      if (mode == draw_normal)
		*((short *) framebufferPointer) = pix;
	      else if (mode == draw_or)
		*((short *) framebufferPointer) |= pix;
	      else if (mode == draw_xor)
		*((short *) framebufferPointer) ^= pix;
	      
	      framebufferPointer += (buffer->width * adapter->bytesPerPixel);
	    }
	}
    }

  // It's not horizontal or vertical.  We will use a Bresenham algorithm
  // to make the line
  else
    {
      // Since I didn't feel like dragging out my old computer science
      // textbooks and re-implementing the Bresenham algorithm, this is a
      // heavily customized version of the code published here:
      // http://www.funducode.com/freec/graphics/graphics2.htm
      // The original author of which seems to be Abhijit Roychoudhuri

      int dx, dy, e = 0, e_inc = 0, e_noinc = 0, incdec = 0,
	start = 0, end = 0, var = 0;

      if (startX > endX)
	{
	  SWAP(startX, endX);
	  SWAP(startY, endY);
	}

      dx = (endX - startX);
      dy = (endY - startY);
      
      /* 0 < m <= 1 */
      if ((dy <= dx) && (dy > 0))
	{
	  e_noinc = (dy << 1);
	  e = ((dy << 1) - dx);
	  e_inc = ((dy - dx) << 1);
	  start = startX;
	  end = endX;
	  var = startY;
	  incdec = 1;
	}

      /* 1 < m < infinity */
      if ((dy > dx) && (dy > 0))
	{
	  e_noinc = (dx << 1);
	  e = ((dx << 1) - dy);
	  e_inc = ((dx - dy) << 1);
	  start = startY;
	  end = endY;
	  var = startX;
	  incdec = 1;
	}

      /* 0 > m > -1, or m = -1 */
      if (((-dy < dx) && (dy < 0)) ||
	  ((dy == -dx) && (dy < 0)))
	{
	  dy = -dy;
	  e_noinc = (dy << 1);
	  if ((-dy < dx) && (dy < 0))
	    e = ((dy - dx) << 1);
	  else
	    e = ((dy << 1) - dx);
	  e_inc = ((dy - dx) << 1);
	  start = startX;
	  end = endX;
	  var = startY;
	  incdec = -1;
	}

      /* -1 > m > 0 */
      if ((-dy > dx) && (dy < 0))
	{
	  dx = -dx;
	  e_noinc = -(dx << 1);
	  e = ((dx - dy) << 1);
	  e_inc = -((dx - dy) << 1);
	  SWAP(startX, endX);
	  SWAP(startY, endY);
	  start = startY;
	  end = endY;
	  var = startX;
	  incdec = -1;
	}
  
      for (count = start; count <= end; count++)
	{
	  if (start == startY)
	    driverDrawPixel(buffer, foreground, mode,
		      var, count);
	  else
	    driverDrawPixel(buffer, foreground, mode,
		      count, var);

	  if (e < 0)
	    e += e_noinc;
	  else
	    {
	      var += incdec;
	      e += e_inc;
	    }
	}
    }
  
  return (status = 0);
}


static int driverDrawRect(kernelGraphicBuffer *buffer, color *foreground,
			  drawMode mode, int xCoord, int yCoord, int width,
			  int height, int thickness, int fill)
{
  // Draws a rectangle on the screen using the preset foreground color

  int status = 0;
  int endX = (xCoord + (width - 1));
  int endY = (yCoord + (height - 1));
  int lineBytes = 0;
  unsigned char *lineBuffer = NULL;
  void *framebufferPointer = NULL;
  short pix = 0;
  int count;

  // If the supplied kernelGraphicBuffer is NULL, we draw directly to the
  // whole screen
  if (buffer == NULL)
    buffer = &wholeScreen;

  // Out of the buffer entirely?
  if ((xCoord >= buffer->width) || (yCoord >= buffer->height))
    return (status = ERR_BOUNDS);

  if (fill)
    {
      // Off the left edge of the buffer?
      if (xCoord < 0)
	{
	  width += xCoord;
	  xCoord = 0;
	}
      // Off the top of the buffer?
      if (yCoord < 0)
	{
	  height += yCoord;
	  yCoord = 0;
	}
      // Off the right edge of the buffer?
      if ((xCoord + width) >= buffer->width)
	width = (buffer->width - xCoord);
      // Off the bottom of the buffer?
      if ((yCoord + height) >= buffer->height)
	height = (buffer->height - yCoord);
	  
      // Re-set these values
      endX = (xCoord + (width - 1));
      endY = (yCoord + (height - 1));

      if ((mode == draw_or) || (mode == draw_xor))
	// Just draw a series of lines, since every pixel needs to be dealt
	// with individually and we can't really do that better than the
	// line drawing routine does already.
	for (count = yCoord; count <= endY; count ++)
	  driverDrawLine(buffer, foreground, mode, xCoord, count, endX, count);
      else
	{
	  // Draw the box manually
	  
	  lineBytes = (width * adapter->bytesPerPixel);

	  // Make one line of the buffer
	  lineBuffer = kernelMalloc(lineBytes);
	  if (lineBuffer == NULL)
	    return (status = ERR_MEMORY);

	  // Do a loop through the line, copying the color values
	  // consecutively

	  if ((adapter->bitsPerPixel == 32) || (adapter->bitsPerPixel == 24))
	    for (count = 0; count < lineBytes; )
	      {
		lineBuffer[count++] = foreground->blue;
		lineBuffer[count++] = foreground->green;
		lineBuffer[count++] = foreground->red;
		if (adapter->bitsPerPixel == 32)
		  count++;
	      }

	  else if ((adapter->bitsPerPixel == 16) ||
		   (adapter->bitsPerPixel == 15))
	    {
	      if (adapter->bitsPerPixel == 16)
		pix = (((foreground->red >> 3) << 11) |
		       ((foreground->green >> 2) << 5) |
		       (foreground->blue >> 3));
	      else
		pix = (((foreground->red >> 3) << 10) |
		       ((foreground->green >> 3) << 5) |
		       (foreground->blue >> 3));
	  
	      for (count = 0; count < width; count ++)
		((short *) lineBuffer)[count] = pix;
	    }

	  // Point to the starting place
	  framebufferPointer = buffer->data +
	    (((buffer->width * yCoord) + xCoord) * adapter->bytesPerPixel);
	  
	  // Copy the line 'height' times
	  for (count = 0; count < height; count ++)
	    {
	      kernelProcessorCopyBytes(lineBuffer, framebufferPointer,
				       lineBytes);
	      framebufferPointer += (buffer->width * adapter->bytesPerPixel);
	    }

	  // Free linebuffer memory
	  kernelFree(lineBuffer);
	}
    }

  else
    {
      // Draw the top line 'thickness' times
      for (count = (yCoord + thickness - 1); count >= yCoord; count --)
	driverDrawLine(buffer, foreground, mode, xCoord, count, endX, count);

      // Draw the left line 'thickness' times
      for (count = (xCoord + thickness - 1); count >= xCoord; count --)
	driverDrawLine(buffer, foreground, mode, count, (yCoord + thickness),
		       count, (endY - thickness));

      // Draw the bottom line 'thickness' times
      for (count = (endY - thickness + 1); count <= endY; count ++)
	driverDrawLine(buffer, foreground, mode, xCoord, count, endX, count);

      // Draw the right line 'thickness' times
      for (count = (endX - thickness + 1); count <= endX; count ++)
	driverDrawLine(buffer, foreground, mode, count, (yCoord + thickness),
		       count, (endY - thickness));
    }

  return (status = 0);
}


static int driverDrawOval(kernelGraphicBuffer *buffer, color *foreground,
			  drawMode mode, int centerX, int centerY, int width,
			  int height, int thickness, int fill)
{
  // Draws an oval on the screen using the preset foreground color.  We use
  // a version of the Bresenham circle algorithm, but in the case of an
  // (unfilled) oval with (thickness > 1) we calculate inner and outer ovals,
  // and draw lines between the inner and outer X coordinates of both, for
  // any given Y coordinate.

  int status = 0;
  int outerRadius = (width >> 1);
  int outerD = (3 - (outerRadius << 1));
  int outerX = 0;
  int outerY = outerRadius;
  int innerRadius = 0, innerD = 0, innerX = 0, innerY = 0;
  int *outerBitmap = NULL;
  int *innerBitmap = NULL;

  // If the supplied kernelGraphicBuffer is NULL, we draw directly to the
  // whole screen
  if (buffer == NULL)
    buffer = &wholeScreen;

  // For now, we only support circles
  if (width != height)
    {
      kernelError(kernel_error, "The framebuffer driver only supports "
		  "circular ovals");
      return (status = ERR_NOTIMPLEMENTED);
    }

  outerBitmap = kernelMalloc((outerRadius + 1) * sizeof(int));
  if (outerBitmap == NULL)
    return (status = ERR_MEMORY);

  if ((thickness > 1) && !fill)
    {
      innerRadius = (outerRadius - thickness + 1);
      innerD = (3 - (innerRadius << 1));
      innerY = innerRadius;

      innerBitmap = kernelMalloc((innerRadius + 1) * sizeof(int));
      if (innerBitmap == NULL)
	return (status = ERR_MEMORY);
    }

  while (outerX <= outerY)
    {
      if (!fill && (thickness == 1))
	{
	  driverDrawPixel(buffer, foreground, mode,
			  (centerX + outerX), (centerY + outerY));
	  driverDrawPixel(buffer, foreground, mode,
			  (centerX + outerX), (centerY - outerY));
	  driverDrawPixel(buffer, foreground, mode,
			  (centerX - outerX), (centerY + outerY));
	  driverDrawPixel(buffer, foreground, mode,
			  (centerX - outerX), (centerY - outerY));
	  driverDrawPixel(buffer, foreground, mode,
			  (centerX + outerY), (centerY + outerX));
	  driverDrawPixel(buffer, foreground, mode,
			  (centerX + outerY), (centerY - outerX));
	  driverDrawPixel(buffer, foreground, mode,
			  (centerX - outerY), (centerY + outerX));
	  driverDrawPixel(buffer, foreground, mode,
			  (centerX - outerY), (centerY - outerX));
	}
      
      if (outerY > outerBitmap[outerX])
	outerBitmap[outerX] = outerY;
      if (outerX > outerBitmap[outerY])
	outerBitmap[outerY] = outerX;
      
      if (outerD < 0)
	outerD += ((outerX << 2) + 6);
      else
	{
	  outerD += (((outerX - outerY) << 2) + 10);
	  outerY -= 1;
	}
      outerX += 1;

      if ((thickness > 1) && !fill)
	{
	  if (!innerBitmap[innerX] || (innerY < innerBitmap[innerX]))
	    innerBitmap[innerX] = innerY;
	  if (!innerBitmap[innerY] || (innerX < innerBitmap[innerY]))
	    innerBitmap[innerY] = innerX;

	  if (innerD < 0)
	    innerD += ((innerX << 2) + 6);
	  else
	    {
	      innerD += (((innerX - innerY) << 2) + 10);
	      innerY -= 1;
	    }
	  innerX += 1;
	}
    }

  if ((thickness > 1) || fill)
    for (outerY = 0; outerY <= outerRadius; outerY ++)
      {
	if ((outerY > innerRadius) || fill)
	  {
	    driverDrawLine(buffer, foreground, mode,
			   (centerX - outerBitmap[outerY]),
			   (centerY - outerY),
			   (centerX + outerBitmap[outerY]),
			   (centerY - outerY));
	    driverDrawLine(buffer, foreground, mode,
			   (centerX - outerBitmap[outerY]),
			   (centerY + outerY),
			   (centerX + outerBitmap[outerY]),
			   (centerY + outerY));
	  }
	else
	  {
	    driverDrawLine(buffer, foreground, mode,
			   (centerX - outerBitmap[outerY]),
			   (centerY - outerY),
			   (centerX - innerBitmap[outerY]),
			   (centerY - outerY));
	    driverDrawLine(buffer, foreground, mode,
			   (centerX + innerBitmap[outerY]),
			   (centerY - outerY),
			   (centerX + outerBitmap[outerY]),
			   (centerY - outerY));
	    driverDrawLine(buffer, foreground, mode,
			   (centerX - outerBitmap[outerY]),
			   (centerY + outerY),
			   (centerX - innerBitmap[outerY]),
			   (centerY + outerY));
	    driverDrawLine(buffer, foreground, mode,
			   (centerX + innerBitmap[outerY]),
			   (centerY + outerY),
			   (centerX + outerBitmap[outerY]),
			   (centerY + outerY));
	  }
      }

  kernelFree(outerBitmap);
  if ((thickness > 1) && !fill)
    kernelFree(innerBitmap);

  return (status = 0);
}


static int driverDrawMonoImage(kernelGraphicBuffer *buffer, image *drawImage,
			       drawMode mode,color *foreground,
			       color *background, int xCoord, int yCoord)
{
  // Draws the supplied image on the screen at the requested coordinates

  int status = 0;
  unsigned lineLength = 0;
  unsigned lineBytes = 0;
  int numberLines = 0;
  unsigned char *framebufferPointer = NULL;
  unsigned char *monoImageData = NULL;
  int lineCounter = 0;
  unsigned pixelCounter = 0;
  short onPixel, offPixel;
  unsigned count;

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
  if ((int)(xCoord + drawImage->width) < buffer->width)
    lineLength = drawImage->width;
  else
    lineLength = (buffer->width - xCoord);

  // If the image goes off the bottom of the screen, only show the
  // lines that will fit
  if ((int)(yCoord + drawImage->height) < buffer->height)
    numberLines = drawImage->height;
  else
    numberLines = (buffer->height - yCoord);
  
  // images are lovely little data structures that give us image
  // data in the most convenient form we can imagine.

  // How many bytes in a line of data?
  lineBytes = (adapter->bytesPerPixel * lineLength);

  framebufferPointer = buffer->data +
    (((buffer->width * yCoord) + xCoord) * adapter->bytesPerPixel);

  // A mono image has a bitmap of 'on' bits and 'off' bits.  We will
  // draw all 'on' bits using the current foreground color.
  monoImageData = (unsigned char *) drawImage->data;

  // Loop for each line

  for (lineCounter = 0; lineCounter < numberLines; lineCounter++)
    {
      // Do a loop through the line, copying either the foreground color
      // value or the background color into framebuffer

      if ((adapter->bitsPerPixel == 32) || (adapter->bitsPerPixel == 24))
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
		if (adapter->bitsPerPixel == 32)
		  count++;
	      }
	    else
	      {
		if (mode == draw_translucent)
		  count += adapter->bytesPerPixel;
		else
		  {
		    // 'off' bit.
		    framebufferPointer[count++] = background->blue;
		    framebufferPointer[count++] = background->green;
		    framebufferPointer[count++] = background->red;
		    if (adapter->bitsPerPixel == 32)
		      count++;
		  }
	      }

	    pixelCounter += 1;
	  }

      else if ((adapter->bitsPerPixel == 16) || (adapter->bitsPerPixel == 15))
	{
	  if (adapter->bitsPerPixel == 16)
	    {
	      onPixel = (((foreground->red >> 3) << 11) |
			 ((foreground->green >> 2) << 5) |
			 (foreground->blue >> 3));
	      offPixel = (((background->red >> 3) << 11) |
			  ((background->green >> 2) << 5) |
			  (background->blue >> 3));
	    }
	  else
	    {
	      onPixel = (((foreground->red >> 3) << 10) |
			  ((foreground->green >> 3) << 5) |
			  (foreground->blue >> 3));
	      offPixel = (((background->red >> 3) << 10) |
			  ((background->green >> 3) << 5) |
			  (background->blue >> 3));
	    }

	for (count = 0; count < lineLength; count ++)
	  {
	    // Isolate the bit from the bitmap
	    if ((monoImageData[pixelCounter / 8] &
		 (0x80 >> (pixelCounter % 8))) != 0)
	      // 'on' bit.
	      ((short *) framebufferPointer)[count] = onPixel;

	    else if (mode != draw_translucent)
	      // 'off' bit
	      ((short *) framebufferPointer)[count] = offPixel;

	    pixelCounter += 1;
	  }
	}

      // Move to the next line in the framebuffer
      framebufferPointer += (buffer->width * adapter->bytesPerPixel);
      
      // Are we skipping any because it's off the screen?
      if (drawImage->width > lineLength)
	pixelCounter += (drawImage->width - lineLength);
    }


  // Success
  return (status = 0);
}


static int driverDrawImage(kernelGraphicBuffer *buffer, image *drawImage,
			   drawMode mode, int xCoord, int yCoord, int xOffset,
			   int yOffset, int width, int height)
{
  // Draws the requested width and height of the supplied image on the screen
  // at the requested coordinates, with the requested offset

  int status = 0;
  unsigned lineLength = 0;
  unsigned lineBytes = 0;
  int numberLines = 0;
  unsigned char *framebufferPointer = NULL;
  pixel *imageData = NULL;
  int lineCounter = 0;
  unsigned pixelCounter = 0;
  short pix = 0;
  unsigned count;

  // If the supplied kernelGraphicBuffer is NULL, we draw directly to the
  // whole screen
  if (buffer == NULL)
    buffer = &wholeScreen;

  // If the image is outside the buffer entirely, skip it
  if ((xCoord >= buffer->width) || (yCoord >= buffer->height))
    return (status = ERR_BOUNDS);

  // Make sure it's a color image
  if (drawImage->type == IMAGETYPE_MONO)
    return (status = ERR_INVALID);

  if (width)
    lineLength = width;
  else
    lineLength = drawImage->width;

  // If the image goes off the sides of the screen, only attempt to display
  // the pixels that will fit
  if (xCoord < 0)
    {
      lineLength += xCoord;
      xOffset += -xCoord;
      xCoord = 0;
    }
  if ((int)(xCoord + lineLength) >= buffer->width)
    lineLength -= ((xCoord + lineLength) - buffer->width);
  if ((unsigned)(xOffset + lineLength) >= drawImage->width)
    lineLength -= ((xOffset + lineLength) - drawImage->width);

  if (height)
    numberLines = height;
  else
    numberLines = drawImage->height;

  // If the image goes off the top or bottom of the screen, only show the
  // lines that will fit
  if (yCoord < 0)
    {
      numberLines += yCoord;
      yOffset += -yCoord;
      yCoord = 0;
    }
  if ((yCoord + numberLines) >= buffer->height)
    numberLines -= ((yCoord + numberLines) - buffer->height);
  if ((unsigned)(yOffset + numberLines) >= drawImage->height)
    numberLines -= ((yOffset + numberLines) - drawImage->height);

  // Images are lovely little data structures that give us image data in the
  // most convenient form we can imagine.

  // How many bytes in a line of data?
  lineBytes = (adapter->bytesPerPixel * lineLength);

  framebufferPointer = buffer->data +
    (((buffer->width * yCoord) + xCoord) * adapter->bytesPerPixel);

  imageData = (pixel *)
    (drawImage->data + (((yOffset * drawImage->width) + xOffset) * 3));
  
  // Loop for each line

  for (lineCounter = 0; lineCounter < numberLines; lineCounter++)
    {	  
      // Do a loop through the line, copying the color values from the
      // image into the framebuffer
      
      if ((adapter->bitsPerPixel == 32) || (adapter->bitsPerPixel == 24))
	for (count = 0; count < lineBytes; )
	  {
	    if ((mode == draw_translucent) &&
		(imageData[pixelCounter].red ==
		 drawImage->translucentColor.red) &&
		(imageData[pixelCounter].green ==
		 drawImage->translucentColor.green) &&
		(imageData[pixelCounter].blue ==
		 drawImage->translucentColor.blue))
	      count += adapter->bytesPerPixel;
	    else
	      {
		framebufferPointer[count++] = imageData[pixelCounter].blue;
		framebufferPointer[count++] = imageData[pixelCounter].green;
		framebufferPointer[count++] = imageData[pixelCounter].red;
		if (adapter->bitsPerPixel == 32)
		  count++;
	      }

	    pixelCounter += 1;
	  }

      else if ((adapter->bitsPerPixel == 16) || (adapter->bitsPerPixel == 15))
	{
	  for (count = 0; count < lineLength; count ++)
	    {
	      if (adapter->bitsPerPixel == 16)
		pix = (((imageData[pixelCounter].red >> 3) << 11) |
		       ((imageData[pixelCounter].green >> 2) << 5) |
		       (imageData[pixelCounter].blue >> 3));
	      else
		pix = (((imageData[pixelCounter].red >> 3) << 10) |
		       ((imageData[pixelCounter].green >> 3) << 5) |
		       (imageData[pixelCounter].blue >> 3));

	      if ((mode != draw_translucent) ||
		  ((imageData[pixelCounter].red !=
		    drawImage->translucentColor.red) ||
		   (imageData[pixelCounter].green !=
		    drawImage->translucentColor.green) ||
		   (imageData[pixelCounter].blue !=
		    drawImage->translucentColor.blue)))
		((short *) framebufferPointer)[count] = pix;
	      
	      pixelCounter += 1;
	    }
	}

      // Move to the next line in the framebuffer
      framebufferPointer += (buffer->width * adapter->bytesPerPixel);
      
      // Are we skipping any of this line because it's off the screen?
      if (drawImage->width > lineLength)
	pixelCounter += (drawImage->width - lineLength);
    }

  // Success
  return (status = 0);
}


static int driverGetImage(kernelGraphicBuffer *buffer, image *theImage,
			  int xCoord, int yCoord, int width, int height)
{
  // Draws the supplied image on the screen at the requested coordinates

  int status = 0;
  unsigned numberPixels = 0;
  int lineLength = 0;
  int numberLines = 0;
  int lineBytes = 0;
  unsigned char *framebufferPointer = NULL;
  pixel *imageData = NULL;
  int lineCounter = 0;
  unsigned pixelCounter = 0;
  int count;

  // If the supplied kernelGraphicBuffer is NULL, we read directly from the
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
  theImage->dataLength = (numberPixels * sizeof(pixel));

  // If the image was previously holding data, release it
  if (theImage->data == NULL)
    {
      // Allocate enough memory to hold the image data
      theImage->data = kernelMemoryGet(theImage->dataLength, "image data");
      if (theImage->data == NULL)
	// Eek, no memory
	return (status = ERR_MEMORY);
    }

  // How many bytes in a line of data?
  lineBytes = (adapter->bytesPerPixel * lineLength);

  // Figure out the starting memory location in the framebuffer
  framebufferPointer = buffer->data +
    (((buffer->width * yCoord) + xCoord) * adapter->bytesPerPixel);

  imageData = (pixel *) theImage->data;

  // Now loop through each line of the buffer, filling the image data from
  // the screen

  for (lineCounter = 0; lineCounter < numberLines; lineCounter++)
    {
      // Do a loop through the line, copying the color values from the
      // framebuffer into the image data

      if ((adapter->bitsPerPixel == 32) || (adapter->bitsPerPixel == 24))
	for (count = 0; count < lineBytes; )
	  {
	    imageData[pixelCounter].blue = framebufferPointer[count++];
	    imageData[pixelCounter].green = framebufferPointer[count++];
	    imageData[pixelCounter].red = framebufferPointer[count++];
	    if (adapter->bitsPerPixel == 32)
	      count++;
	    
	    pixelCounter += 1;
	  }

      else if ((adapter->bitsPerPixel == 16) || (adapter->bitsPerPixel == 15))
	{
	  for (count = 0; count < lineLength; count ++)
	    {
	      short pix = ((short *) framebufferPointer)[count];
	      
	      if (adapter->bitsPerPixel == 16)
		{
		  imageData[pixelCounter].red = ((pix & 0xF800) >> 8);
		  imageData[pixelCounter].green = ((pix & 0x07E0) >> 3);
		  imageData[pixelCounter].blue = ((pix & 0x001F) << 3);
		}
	      else
		{
		  imageData[pixelCounter].red = ((pix & 0x7C00) >> 7);
		  imageData[pixelCounter].green = ((pix & 0x03E0) >> 2);
		  imageData[pixelCounter].blue = ((pix & 0x001F) << 3);
		}

	      pixelCounter += 1;
	    }
	}

      // Move to the next line in the framebuffer
      framebufferPointer += (buffer->width * adapter->bytesPerPixel);
    }

  // Fill in the image's vitals
  theImage->type = IMAGETYPE_COLOR;
  theImage->pixels = numberPixels;
  theImage->width = lineLength;
  theImage->height = numberLines;

  return (status = 0);
}


static int driverCopyArea(kernelGraphicBuffer *buffer,
			  int xCoord1, int yCoord1, int width, int height,
			  int xCoord2, int yCoord2)
{
  // Copy a clip of data from one area of the buffer to another

  int status = 0;
  unsigned char *srcPointer = NULL;
  unsigned char *destPointer = NULL;
  int count;

  // If the supplied kernelGraphicBuffer is NULL, we draw directly to the
  // whole screen
  if (buffer == NULL)
    buffer = &wholeScreen;

  srcPointer = buffer->data +
    (((buffer->width * yCoord1) + xCoord1) * adapter->bytesPerPixel);
  destPointer = buffer->data +
    (((buffer->width * yCoord2) + xCoord2) * adapter->bytesPerPixel);

  for (count = yCoord1; count <= (yCoord1 + height - 1); count ++)
    {
      kernelMemCopy(srcPointer, destPointer, (width * adapter->bytesPerPixel));
      srcPointer += (buffer->width * adapter->bytesPerPixel);
      destPointer += (buffer->width * adapter->bytesPerPixel);
    }

  return (status = 0);
}


static int driverRenderBuffer(kernelGraphicBuffer *buffer,
			      int drawX, int drawY, int clipX, int clipY,
			      int width, int height)
{
  // Take the supplied graphic buffer and render it onto the screen.

  int status = 0;
  void *bufferPointer = NULL;
  void *screenPointer = NULL;

  // This function is the single instance where a NULL buffer is not
  // allowed, since we are drawing the buffer to the screen this time
  if (buffer == NULL)
    return (status = ERR_NULLPARAMETER);

  // Not allowed to specify a clip that is not fully inside the buffer
  if ((clipX < 0) || ((clipX + width) > buffer->width) ||
      (clipY < 0) || ((clipY + height) > buffer->height))
    return (status = ERR_RANGE);

  // Get the line length of each line that we want to draw and cut them
  // off if the area will extend past the screen boundaries.
  if ((drawX + clipX) < 0)
    {
      width += (drawX + clipX);
      clipX -= (drawX + clipX);
    }
  if ((drawX + clipX + width) >= wholeScreen.width)
    width = (wholeScreen.width - (drawX + clipX));
  if ((drawY + clipY) < 0)
    {
      height += (drawY + clipY);
      clipY -= (drawY + clipY);
    }
  if ((drawY + clipY + height) >= wholeScreen.height)
    height = (wholeScreen.height - (drawY + clipY));

  // Don't draw if the whole area is off the screen
  if (((drawX + clipX) >= wholeScreen.width) ||
      ((drawY + clipY) >= wholeScreen.height))
    return (status = 0);

  // Calculate the starting offset inside the buffer
  bufferPointer = buffer->data +
    (((buffer->width * clipY) + clipX) * adapter->bytesPerPixel);

  // Calculate the starting offset on the screen
  screenPointer = wholeScreen.data +
    (((wholeScreen.width * (drawY + clipY)) + drawX + clipX) *
     adapter->bytesPerPixel);

  // Start copying lines
  for ( ; height > 0; height --)
    {
      kernelMemCopy(bufferPointer, screenPointer,
		    (width * adapter->bytesPerPixel));
      bufferPointer += (buffer->width * adapter->bytesPerPixel);
      screenPointer += (wholeScreen.width * adapter->bytesPerPixel);
    }

  return (status = 0);
}


static int driverFilter(kernelGraphicBuffer *buffer, color *filterColor,
			int xCoord, int yCoord, int width, int height)
{
  // Take an area of a buffer and average it with the supplied color
  
  int status = 0;
  int lineBytes = 0;
  unsigned char *framebufferPointer = NULL;
  int red, green, blue;
  int lineCount, count;

  // If the supplied kernelGraphicBuffer is NULL, we draw directly to the
  // whole screen
  if (buffer == NULL)
    buffer = &wholeScreen;

  // Out of the buffer entirely?
  if ((xCoord >= buffer->width) || (yCoord >= buffer->height))
    return (status = ERR_BOUNDS);

  // Off the left edge of the buffer?
  if (xCoord < 0)
    {
      width += xCoord;
      xCoord = 0;
    }
  // Off the top of the buffer?
  if (yCoord < 0)
    {
      height += yCoord;
      yCoord = 0;
    }
  // Off the right edge of the buffer?
  if ((xCoord + width) >= buffer->width)
    width = (buffer->width - xCoord);
  // Off the bottom of the buffer?
  if ((yCoord + height) >= buffer->height)
    height = (buffer->height - yCoord);

  // How many bytes in the line?
  lineBytes = (adapter->bytesPerPixel * width);

  framebufferPointer = buffer->data +
    (((buffer->width * yCoord) + xCoord) * adapter->bytesPerPixel);

  // Do a loop through each line, copying the color values consecutively
  for (lineCount = 0; lineCount < height; lineCount ++)
    {
      if ((adapter->bitsPerPixel == 32) || (adapter->bitsPerPixel == 24))
	{
	  for (count = 0; count < lineBytes; )
	    {
	      framebufferPointer[count] =
		((framebufferPointer[count] + filterColor->blue) / 2);
	      framebufferPointer[count + 1] =
		((framebufferPointer[count + 1] + filterColor->green) / 2);
	      framebufferPointer[count + 2] =
		((framebufferPointer[count + 2] + filterColor->red) / 2);

	      count += 3;
	      if (adapter->bitsPerPixel == 32)
		count++;
	    }
	}

      else if ((adapter->bitsPerPixel == 16) || (adapter->bitsPerPixel == 15))
	{
	  for (count = 0; count < width; count ++)
	    {
	      short *ptr = (short *) framebufferPointer;

	      blue = ((((ptr[count] & 0x001F) +
			(filterColor->blue >> 3)) >> 1) & 0x001F);

	      if (adapter->bitsPerPixel == 16)
		{
		  red = (((((ptr[count] & 0xF800) >> 11) +
			   (filterColor->red >> 3)) >> 1) & 0x001F);
		  green = (((((ptr[count] & 0x07E0) >> 5) +
			     (filterColor->green >> 2)) >> 1) & 0x003F);
		  ptr[count] = (short) ((red << 11) | (green << 5) | blue);
		}
	      else
		{
		  red = (((((ptr[count] & 0x7C00) >> 10) +
			   (filterColor->red >> 3)) >> 1) & 0x001F);
		  green = (((((ptr[count] & 0x03E0) >> 5) +
			     (filterColor->green >> 3)) >> 1) & 0x001F);
		  ptr[count] = (short) ((red << 10) | (green << 5) | blue);
		}
	    }
	}

      framebufferPointer += (buffer->width * adapter->bytesPerPixel);
    }

  return (status = 0);
}


static int driverDetect(void *driver)
{
  // This routine is used to detect and initialize each device, as well as
  // registering each one with any higher-level interfaces

  int status = 0;
  kernelDevice *dev = NULL;

  // Allocate memory for the device
  dev = kernelMalloc(sizeof(kernelDevice) + sizeof(kernelGraphicAdapter));
  if (dev == NULL)
    return (status = 0);

  adapter = ((void *) dev + sizeof(kernelDevice));

  // Set up the device parameters
  adapter->videoMemory = kernelOsLoaderInfo->graphicsInfo.videoMemory;
  adapter->framebuffer = kernelOsLoaderInfo->graphicsInfo.framebuffer;
  adapter->mode = kernelOsLoaderInfo->graphicsInfo.mode;
  adapter->xRes = kernelOsLoaderInfo->graphicsInfo.xRes;
  adapter->yRes = kernelOsLoaderInfo->graphicsInfo.yRes;
  adapter->bitsPerPixel = kernelOsLoaderInfo->graphicsInfo.bitsPerPixel;
  if (adapter->bitsPerPixel == 15)
    adapter->bytesPerPixel = 2;
  else
    adapter->bytesPerPixel = (adapter->bitsPerPixel / 8);
  adapter->numberModes = kernelOsLoaderInfo->graphicsInfo.numberModes;
  kernelMemCopy(&(kernelOsLoaderInfo->graphicsInfo.supportedModes),
		&(adapter->supportedModes),
		(sizeof(videoMode) * MAXVIDEOMODES));

  dev->device.class = kernelDeviceGetClass(DEVICECLASS_GRAPHIC);
  dev->device.subClass =
    kernelDeviceGetClass(DEVICESUBCLASS_GRAPHIC_FRAMEBUFFER);
  dev->driver = driver;
  dev->data = adapter;
  
  // If we are in a graphics mode, initialize the graphics functions
  if (adapter->mode != 0)
    {
      // Map the supplied physical linear framebuffer address into kernel
      // memory
      status = kernelPageMapToFree(KERNELPROCID, adapter->framebuffer,
				   &(adapter->framebuffer),
				   (adapter->xRes * adapter->yRes *
				    adapter->bytesPerPixel));
      if (status < 0)
	{
	  kernelError(kernel_error, "Unable to map linear framebuffer");
	  return (status);
	}

      status = kernelGraphicInitialize(dev);
      if (status < 0)
	return (status);
    }

  // Set up the kernelGraphicBuffer that represents the whole screen
  wholeScreen.width = adapter->xRes;
  wholeScreen.height = adapter->yRes;
  wholeScreen.data = adapter->framebuffer;

  return (status = kernelDeviceAdd(NULL, dev));
}


static kernelGraphicOps framebufferOps = {
  driverClearScreen,
  driverDrawPixel,
  driverDrawLine,
  driverDrawRect,
  driverDrawOval,
  driverDrawMonoImage,
  driverDrawImage,
  driverGetImage,
  driverCopyArea,
  driverRenderBuffer,
  driverFilter,
};


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


void kernelFramebufferGraphicDriverRegister(void *driverData)
{
   // Device driver registration.

  kernelDriver *driver = (kernelDriver *) driverData;

  driver->driverDetect = driverDetect;
  driver->ops = &framebufferOps;

  return;
}
