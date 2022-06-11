//
//  Visopsys
//  Copyright (C) 1998-2020 J. Andrew McLaughlin
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
//  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
//
//  graphic.h
//

// This file contains definitions and structures for using and manipulating
// graphics in Visopsys.

#ifndef _GRAPHIC_H
#define _GRAPHIC_H

#define MAXVIDEOMODES		100

#define PIXELS_EQ(p1, p2) \
	(((p1)->red == (p2)->red) && ((p1)->green == (p2)->green) && \
	((p1)->blue == (p2)->blue))
#define PIXEL_COPY(src, dest) do { \
	(dest)->red = (src)->red; \
	(dest)->green = (src)->green; \
	(dest)->blue = (src)->blue; \
} while (0)

// An enumeration for different drawing modes.
typedef enum {
	draw_normal, draw_reverse, draw_or, draw_xor, draw_translucent,
	draw_alphablend

} drawMode;

typedef enum {
	shade_fromtop, shade_frombottom, shade_fromleft, shade_fromright

} shadeType;

#ifdef __cplusplus
	#define __VOLATILE
#else
	#define __VOLATILE volatile
#endif

// Structure to represent a drawing area
typedef __VOLATILE struct {
	int width;
	int height;
	void *data;

} graphicBuffer;

#undef __VOLATILE

// A data structure to describe a graphics mode
typedef struct {
	int mode;
	int xRes;
	int yRes;
	int bitsPerPixel;

} videoMode;

#endif

