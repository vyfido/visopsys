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
//  kernelFramebufferGraphicDriver.c
//

// This is the simple graphics driver for a LFB (Linear Framebuffer)
// -equipped graphics adapter

#include "kernelDriverManagement.h" // Contains my prototypes
#include "kernelMemoryManager.h"
#include "kernelMalloc.h"
#include "kernelMiscFunctions.h"
#include "kernelProcessorX86.h"
#include "kernelError.h"
#include <sys/errors.h>


int kernelLFBGraphicDriverRegisterDevice(void *);
int kernelLFBGraphicDriverClearScreen(color *);
int kernelLFBGraphicDriverDrawPixel(kernelGraphicBuffer *, color *, drawMode,
				    int, int);
int kernelLFBGraphicDriverDrawLine(kernelGraphicBuffer *, color *, drawMode,
				   int, int, int, int);
int kernelLFBGraphicDriverDrawRect(kernelGraphicBuffer *, color *, drawMode,
				   int, int, unsigned, unsigned, unsigned,
				   int);
int kernelLFBGraphicDriverDrawOval(kernelGraphicBuffer *, color *, drawMode,
				   int, int, unsigned, unsigned, unsigned,
				   int);
int kernelLFBGraphicDriverDrawMonoImage(kernelGraphicBuffer *, image *,
					drawMode, color *, color *, int, int);
int kernelLFBGraphicDriverDrawImage(kernelGraphicBuffer *, image *, drawMode,
				    int, int, unsigned, unsigned,
				    unsigned, unsigned);
int kernelLFBGraphicDriverGetImage(kernelGraphicBuffer *, image *, int, int,
				   unsigned, unsigned);
int kernelLFBGraphicDriverCopyArea(kernelGraphicBuffer *, int, int, unsigned,
				   unsigned, int, int);
int kernelLFBGraphicDriverRenderBuffer(kernelGraphicBuffer *, int, int, int,
				       int, unsigned, unsigned);

static kernelGraphicDriver defaultGraphicDriver =
{
  kernelLFBGraphicDriverInitialize,
  kernelLFBGraphicDriverRegisterDevice,
  kernelLFBGraphicDriverClearScreen,
  kernelLFBGraphicDriverDrawPixel,
  kernelLFBGraphicDriverDrawLine,
  kernelLFBGraphicDriverDrawRect,
  kernelLFBGraphicDriverDrawOval,
  kernelLFBGraphicDriverDrawMonoImage,
  kernelLFBGraphicDriverDrawImage,
  kernelLFBGraphicDriverGetImage,
  kernelLFBGraphicDriverCopyArea,
  kernelLFBGraphicDriverRenderBuffer
};

static kernelGraphicAdapter *adapter = NULL;
static kernelGraphicBuffer wholeScreen;


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


int kernelLFBGraphicDriverInitialize(void)
{
  // The standard initialization stuff
  return (kernelDriverRegister(graphicDriver, &defaultGraphicDriver));
}


int kernelLFBGraphicDriverRegisterDevice(void *device)
{
  // Save the reference to the device information
  adapter = (kernelGraphicAdapter *) device;

  // Set up the kernelGraphicBuffer that represents the whole screen
  wholeScreen.width = adapter->xRes;
  wholeScreen.height = adapter->yRes;
  wholeScreen.data = adapter->framebuffer;

  return (0);
}


int kernelLFBGraphicDriverClearScreen(color *background)
{
  // Resets the whole screen to the background color
  
  int status = 0;
  int pixels = (adapter->xRes * adapter->yRes);
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
    for (count = 0; count < pixels; count ++)
      {
	short pixel = (((background->red >> 3) << 11) |
		       ((background->green >> 2) << 5) |
		       (background->blue >> 3));
	((short *) adapter->framebuffer)[count] = pixel;
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
	short pixel = (((foreground->red >> 3) << 11) |
		       ((foreground->green >> 2) << 5) |
		       (foreground->blue >> 3));
	if (mode == draw_normal)
	  *((short *) framebufferPointer) = pixel;
	else if (mode == draw_or)
	  *((short *) framebufferPointer) |= pixel;
	else if (mode == draw_xor)
	  *((short *) framebufferPointer) ^= pixel;
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

  int tmp;
  #define SWAP(a, b) do { tmp = a; a = b; b = tmp; } while (0)

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
      if (endX < buffer->width)
	lineLength = (endX - startX + 1);
      else
	lineLength = (buffer->width - startX);
      
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
	for (count = 0; count < lineLength; count ++)
	  {
	    short pixel = (((foreground->red >> 3) << 11) |
			   ((foreground->green >> 2) << 5) |
			   (foreground->blue >> 3));
	    if (mode == draw_normal)
	      ((short *) framebufferPointer)[count] = pixel;
	    else if (mode == draw_or)
	      ((short *) framebufferPointer)[count] |= pixel;
	    else if (mode == draw_xor)
	      ((short *) framebufferPointer)[count] ^= pixel;
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
      if (endY < buffer->height)
	lineLength = (endY - startY + 1);
      else
	lineLength = (buffer->height - startY);
      
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
	for (count = 0; count < lineLength; count ++)
	  {
	    short pixel = (((foreground->red >> 3) << 11) |
			   ((foreground->green >> 2) << 5) |
			   (foreground->blue >> 3));
	    if (mode == draw_normal)
	      *((short *) framebufferPointer) = pixel;
	    else if (mode == draw_or)
	      *((short *) framebufferPointer) |= pixel;
	    else if (mode == draw_xor)
	      *((short *) framebufferPointer) ^= pixel;

	    framebufferPointer += (buffer->width * adapter->bytesPerPixel);
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
	    kernelLFBGraphicDriverDrawPixel(buffer, foreground, mode, var,
					    count);
	  else
	    kernelLFBGraphicDriverDrawPixel(buffer, foreground, mode, count,
					    var);

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


int kernelLFBGraphicDriverDrawRect(kernelGraphicBuffer *buffer,
				   color *foreground, drawMode mode,
				   int startX, int startY,
				   unsigned width, unsigned height,
				   unsigned thickness, int fill)
{
  // Draws a rectangle on the screen using the preset foreground color

  int status = 0;
  int endX, endY;
  unsigned lineBytes = 0;
  unsigned char *lineBuffer = NULL;
  void *framebufferPointer = NULL;
  int count;

  // If the supplied kernelGraphicBuffer is NULL, we draw directly to the
  // whole screen
  if (buffer == NULL)
    buffer = &wholeScreen;

  // Out of the buffer entirely?
  if ((startX >= buffer->width) || (startY >= buffer->height))
    return (status = ERR_BOUNDS);

  // Off the left edge of the buffer?
  if (startX < 0)
    {
      width += startX;
      startX = 0;
    }
  // Off the top of the buffer?
  if (startY < 0)
    {
      height += startY;
      startY = 0;
    }
  // Off the right edge of the buffer?
  if ((startX + width) >= buffer->width)
    width = (buffer->width - startX);
  // Off the bottom of the buffer?
  if ((startY + height) >= buffer->height)
    height = (buffer->height - startY);
	  
  endX = (startX + (width - 1));
  endY = (startY + (height - 1));

  if (fill)
    {
      if ((mode == draw_or) || (mode == draw_xor))
	// Just draw a series of lines, since every pixel needs to be dealt
	// with individually and we can't really do that better than the
	// line drawing routine does already.
	for (count = startY; count <= endY; count ++)
	  kernelLFBGraphicDriverDrawLine(buffer, foreground, mode, startX,
					 count, endX, count);
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
	    for (count = 0; count < width; count ++)
	      {
		short pixel = (((foreground->red >> 3) << 11) |
			       ((foreground->green >> 2) << 5) |
			       (foreground->blue >> 3));
		((short *) lineBuffer)[count] = pixel;
	      }

	  // Point to the starting place
	  framebufferPointer = buffer->data +
	    (((buffer->width * startY) + startX) * adapter->bytesPerPixel);
	  
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
	  kernelLFBGraphicDriverDrawPixel(buffer, foreground, mode,
				  (centerX + outerX), (centerY + outerY));
	  kernelLFBGraphicDriverDrawPixel(buffer, foreground, mode,
				  (centerX + outerX), (centerY - outerY));
	  kernelLFBGraphicDriverDrawPixel(buffer, foreground, mode,
				  (centerX - outerX), (centerY + outerY));
	  kernelLFBGraphicDriverDrawPixel(buffer, foreground, mode,
				  (centerX - outerX), (centerY - outerY));
	  kernelLFBGraphicDriverDrawPixel(buffer, foreground, mode,
				  (centerX + outerY), (centerY + outerX));
	  kernelLFBGraphicDriverDrawPixel(buffer, foreground, mode,
				  (centerX + outerY), (centerY - outerX));
	  kernelLFBGraphicDriverDrawPixel(buffer, foreground, mode,
				  (centerX - outerY), (centerY + outerX));
	  kernelLFBGraphicDriverDrawPixel(buffer, foreground, mode,
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
	    kernelLFBGraphicDriverDrawLine(buffer, foreground, mode,
					   (centerX - outerBitmap[outerY]),
					   (centerY - outerY),
					   (centerX + outerBitmap[outerY]),
					   (centerY - outerY));
	    kernelLFBGraphicDriverDrawLine(buffer, foreground, mode,
					   (centerX - outerBitmap[outerY]),
					   (centerY + outerY),
					   (centerX + outerBitmap[outerY]),
					   (centerY + outerY));
	  }
	else
	  {
	    kernelLFBGraphicDriverDrawLine(buffer, foreground, mode,
					   (centerX - outerBitmap[outerY]),
					   (centerY - outerY),
					   (centerX - innerBitmap[outerY]),
					   (centerY - outerY));
	    kernelLFBGraphicDriverDrawLine(buffer, foreground, mode,
					   (centerX + innerBitmap[outerY]),
					   (centerY - outerY),
					   (centerX + outerBitmap[outerY]),
					   (centerY - outerY));
	    kernelLFBGraphicDriverDrawLine(buffer, foreground, mode,
					   (centerX - outerBitmap[outerY]),
					   (centerY + outerY),
					   (centerX - innerBitmap[outerY]),
					   (centerY + outerY));
	    kernelLFBGraphicDriverDrawLine(buffer, foreground, mode,
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


int kernelLFBGraphicDriverDrawMonoImage(kernelGraphicBuffer *buffer,
					image *drawImage, drawMode mode,
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
	for (count = 0; count < lineLength; count ++)
	  {
	    // Isolate the bit from the bitmap
	    if ((monoImageData[pixelCounter / 8] &
		 (0x80 >> (pixelCounter % 8))) != 0)
	      {
		// 'on' bit.
		short pixel = (((foreground->red >> 3) << 11) |
			       ((foreground->green >> 2) << 5) |
			       (foreground->blue >> 3));
		((short *) framebufferPointer)[count] = pixel;
	      }
	    else
	      {
		// 'off' bit
		short pixel = (((background->red >> 3) << 11) |
			       ((background->green >> 2) << 5) |
			       (background->blue >> 3));
		((short *) framebufferPointer)[count] = pixel;
	      }

	    pixelCounter += 1;
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


int kernelLFBGraphicDriverDrawImage(kernelGraphicBuffer *buffer,
				    image *drawImage, drawMode mode,
				    int xCoord, int yCoord,
				    unsigned xOffset, unsigned yOffset,
				    unsigned width, unsigned height)
{
  // Draws the requested width and height of the supplied image on the screen
  // at the requested coordinates, with the requested offset

  int status = 0;
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
  if ((xCoord >= (int) buffer->width) || (yCoord >= (int) buffer->height))
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
  if ((xCoord + lineLength) >= buffer->width)
    lineLength -= ((xCoord + lineLength) - buffer->width);
  if ((xOffset + lineLength) >= drawImage->width)
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
  if ((yOffset + numberLines) >= drawImage->height)
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
	for (count = 0; count < lineLength; count ++)
	  {
	    if ((mode != draw_translucent) ||
		((imageData[pixelCounter].red !=
		  drawImage->translucentColor.red) ||
		 (imageData[pixelCounter].green !=
		  drawImage->translucentColor.green) ||
		 (imageData[pixelCounter].blue !=
		  drawImage->translucentColor.blue)))
	      {
		short pixel = (((imageData[pixelCounter].red >> 3) << 11) |
			       ((imageData[pixelCounter].green >> 2) << 5) |
			       (imageData[pixelCounter].blue >> 3));
		((short *) framebufferPointer)[count] = pixel;
	      }

	    pixelCounter += 1;
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
  getImage->dataLength = (numberPixels * sizeof(pixel));

  // If the image was previously holding data, release it
  if (getImage->data == NULL)
    {
      // Allocate enough memory to hold the image data
      getImage->data = kernelMemoryGet(getImage->dataLength, "image data");
      if (getImage->data == NULL)
	// Eek, no memory
	return (status = ERR_MEMORY);
    }

  // How many bytes in a line of data?
  lineBytes = (adapter->bytesPerPixel * lineLength);

  // Figure out the starting memory location in the framebuffer
  framebufferPointer = buffer->data +
    (((buffer->width * yCoord) + xCoord) * adapter->bytesPerPixel);

  imageData = (pixel *) getImage->data;

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
	for (count = 0; count < lineLength; count ++)
	  {
	    short pixel = ((short *) framebufferPointer)[count];

	    imageData[pixelCounter].red = ((pixel & 0xF800) >> 8);
	    imageData[pixelCounter].green = ((pixel & 0x07E0) >> 3);
	    imageData[pixelCounter].blue = ((pixel & 0x001F) << 3);

	    pixelCounter += 1;
	  }

	// Move to the next line in the framebuffer
	framebufferPointer += (buffer->width * adapter->bytesPerPixel);
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


int kernelLFBGraphicDriverRenderBuffer(kernelGraphicBuffer *buffer,
				       int drawX, int drawY,
				       int clipX, int clipY,
				       unsigned width, unsigned height)
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
