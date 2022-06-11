// 
//  Visopsys
//  Copyright (C) 1998-2004 J. Andrew McLaughlin
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
//  Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
//
//  image.h
//

// This file contains definitions and structures for using and manipulating
// images in Visopsys.

#if !defined(_IMAGE_H)

// Definitions
#define IMAGETYPE_MONO  1
#define IMAGETYPE_COLOR 2

#define MAXVIDEOMODES   20

// An enumeration for different drawing modes.
typedef enum {
  draw_normal, draw_reverse, draw_or, draw_xor, draw_translucent
} drawMode;

// Structures for manipulating generic images.

typedef struct
{
  unsigned char blue;
  unsigned char green;
  unsigned char red;

} color;

typedef color pixel;

typedef struct
{
  int type;
  color translucentColor;
  unsigned pixels;
  unsigned width;
  unsigned height;
  unsigned dataLength;
  void *data;

} image;

// A data structure to describe a graphics mode
typedef struct
{
  int mode;
  int xRes;
  int yRes;
  int bitsPerPixel;

} videoMode;

#define _IMAGE_H
#endif
