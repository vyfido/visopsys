// 
//  Visopsys
//  Copyright (C) 1998-2014 J. Andrew McLaughlin
//  
//  This library is free software; you can redistribute it and/or modify it
//  under the terms of the GNU Lesser General Public License as published by
//  the Free Software Foundation; either version 2.1 of the License, or (at
//  your option) any later version.
//
//  This library is distributed in the hope that it will be useful, but
//  WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
//  General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public License
//  along with this library; if not, write to the Free Software Foundation,
//  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
//
//  image.h
//

// This file contains definitions and structures for using and manipulating
// images in Visopsys.

#if !defined(_IMAGE_H)

#include <sys/color.h>

// Definitions
#define IMAGETYPE_MONO   1
#define IMAGETYPE_COLOR  2

#define IMAGEFORMAT_BMP  1
#define IMAGEFORMAT_JPG  2

#define MAXVIDEOMODES    20

#define PIXELS_EQ(p1, p2)                                       \
  (((p1)->red == (p2)->red) && ((p1)->green == (p2)->green) &&  \
   ((p1)->blue == (p2)->blue))
#define PIXEL_COPY(src, dest) do { \
    (dest)->red = (src)->red;	   \
    (dest)->green = (src)->green;  \
    (dest)->blue = (src)->blue;	   \
} while (0)

// An enumeration for different drawing modes.
typedef enum {
  draw_normal, draw_reverse, draw_or, draw_xor, draw_translucent,
  draw_alphablend

} drawMode;

typedef enum {
  shade_fromtop, shade_frombottom, shade_fromleft, shade_fromright

} shadeType;

// Structures for manipulating generic images.

typedef color pixel;

typedef struct {
  int type;
  color transColor;
  unsigned pixels;
  unsigned width;
  unsigned height;
  unsigned dataLength;
  void *data;
  float *alpha;

} image;

// A data structure to describe a graphics mode
typedef struct {
  int mode;
  int xRes;
  int yRes;
  int bitsPerPixel;

} videoMode;

#define _IMAGE_H
#endif
