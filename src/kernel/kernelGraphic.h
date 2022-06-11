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
//  kernelGraphic.h
//
	
// This goes with the file kernelGraphic.c

#if !defined(_KERNELGRAPHIC_H)

#include "kernelFont.h"

// Definitions
#define MAX_GRAPHICBUFFER_UPDATES 128

// Structure to represent a drawing area
typedef volatile struct
{
  unsigned width;
  unsigned height;
  int numUpdates;
  unsigned updates[MAX_GRAPHICBUFFER_UPDATES][4];
  void *data;

} kernelGraphicBuffer;

// Structures for the graphic adapter device

typedef struct
{
  int (*driverInitialize) (void);
  int (*driverRegisterDevice) (void *);
  int (*driverClearScreen) (color *);
  int (*driverDrawPixel) (kernelGraphicBuffer *, color *, drawMode, int, int);
  int (*driverDrawLine) (kernelGraphicBuffer *, color *, drawMode, int, int,
			 int, int);
  int (*driverDrawRect) (kernelGraphicBuffer *, color *, drawMode, int, int,
			 unsigned, unsigned, unsigned, int);
  int (*driverDrawOval) (kernelGraphicBuffer *, color *, drawMode,
			 int, int, unsigned, unsigned, unsigned, int);
  int (*driverDrawMonoImage) (kernelGraphicBuffer *, image *,
			      color *, color *, int, int);
  int (*driverDrawImage) (kernelGraphicBuffer *, image *, int, int, unsigned,
			  unsigned, unsigned, unsigned);
  int (*driverGetImage) (kernelGraphicBuffer *, image *, int, int,
			 unsigned, unsigned);
  int (*driverCopyArea) (kernelGraphicBuffer *, int, int, unsigned, unsigned,
			 int, int);
  int (*driverRenderBuffer) (kernelGraphicBuffer *, int, int, int, int,
			     unsigned, unsigned);

} kernelGraphicDriver;

typedef struct 
{
  unsigned videoMemory;
  int mode;
  void *framebuffer;
  unsigned xRes;
  unsigned yRes;
  unsigned bitsPerPixel;
  unsigned bytesPerPixel;
  kernelGraphicDriver *driver;

} kernelGraphicAdapter;

// The default driver initialization
int kernelLFBGraphicDriverInitialize(void);

// Functions exported by kernelGraphic.c
int kernelGraphicRegisterDevice(kernelGraphicAdapter *);
int kernelGraphicInitialize(void);
int kernelGraphicsAreEnabled(void);
unsigned kernelGraphicGetScreenWidth(void);
unsigned kernelGraphicGetScreenHeight(void);
unsigned kernelGraphicCalculateAreaBytes(unsigned, unsigned);
int kernelGraphicClearScreen(color *);
int kernelGraphicDrawPixel(kernelGraphicBuffer *, color *, drawMode, int, int);
int kernelGraphicDrawLine(kernelGraphicBuffer *, color *, drawMode, int, int,
			  int, int);
int kernelGraphicDrawRect(kernelGraphicBuffer *, color *, drawMode, int, int,
			  unsigned, unsigned, unsigned, int);
int kernelGraphicDrawOval(kernelGraphicBuffer *, color *, drawMode, int, int,
			  unsigned, unsigned, unsigned, int);
int kernelGraphicDrawImage(kernelGraphicBuffer *, image *, int, int, unsigned,
			   unsigned, unsigned, unsigned);
int kernelGraphicGetImage(kernelGraphicBuffer *, image *, int, int,
			  unsigned, unsigned);
int kernelGraphicDrawText(kernelGraphicBuffer *, color *, kernelAsciiFont *,
			  const char *, drawMode, int, int);
int kernelGraphicCopyArea(kernelGraphicBuffer *, int, int, unsigned, unsigned,
			  int, int);
int kernelGraphicClearArea(kernelGraphicBuffer *, color *, int, int,
			   unsigned, unsigned);
int kernelGraphicRenderBuffer(kernelGraphicBuffer *, int, int, int, int,
			      unsigned, unsigned); 

#define _KERNELGRAPHIC_H
#endif
