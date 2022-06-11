//
//  Visopsys
//  Copyright (C) 1998-2006 J. Andrew McLaughlin
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
#include "kernelDevice.h"

// Definitions
#define DEFAULT_FOREGROUND_RED    40
#define DEFAULT_FOREGROUND_GREEN  93
#define DEFAULT_FOREGROUND_BLUE   171
#define DEFAULT_BACKGROUND_RED    200
#define DEFAULT_BACKGROUND_GREEN  200
#define DEFAULT_BACKGROUND_BLUE   200
#define DEFAULT_DESKTOP_RED       40
#define DEFAULT_DESKTOP_GREEN     93
#define DEFAULT_DESKTOP_BLUE      171

// Types of borders
typedef enum {
  border_top = 1, border_left = 2, border_bottom = 4, border_right = 8,
  border_all = (border_top | border_left | border_bottom | border_right)
} borderType;

// Structure to represent a drawing area
typedef volatile struct {
  int width;
  int height;
  void *data;

} kernelGraphicBuffer;

// Structures for the graphic adapter device

typedef struct {
  int (*driverClearScreen) (color *);
  int (*driverDrawPixel) (kernelGraphicBuffer *, color *, drawMode, int, int);
  int (*driverDrawLine) (kernelGraphicBuffer *, color *, drawMode, int, int,
			 int, int);
  int (*driverDrawRect) (kernelGraphicBuffer *, color *, drawMode, int, int,
			 int, int, int, int);
  int (*driverDrawOval) (kernelGraphicBuffer *, color *, drawMode, int, int,
			 int, int, int, int);
  int (*driverDrawMonoImage) (kernelGraphicBuffer *, image *, drawMode,
			      color *, color *, int, int);
  int (*driverDrawImage) (kernelGraphicBuffer *, image *, drawMode, int, int,
			  int, int, int, int);
  int (*driverGetImage) (kernelGraphicBuffer *, image *, int, int, int, int);
  int (*driverCopyArea) (kernelGraphicBuffer *, int, int, int, int, int, int);
  int (*driverRenderBuffer) (kernelGraphicBuffer *, int, int, int, int, int,
			     int);
  int (*driverFilter) (kernelGraphicBuffer *, color *, int, int, int, int);

} kernelGraphicOps;

typedef struct {
  unsigned videoMemory;
  void *framebuffer;
  int mode;
  int xRes;
  int yRes;
  int bitsPerPixel;
  int bytesPerPixel;
  int numberModes;
  videoMode supportedModes[MAXVIDEOMODES];
  kernelDriver *driver;

} kernelGraphicAdapter;

// Functions exported by kernelGraphic.c
int kernelGraphicInitialize(kernelDevice *);
int kernelGraphicsAreEnabled(void);
int kernelGraphicGetModes(videoMode *, unsigned);
int kernelGraphicGetMode(videoMode *);
int kernelGraphicSetMode(videoMode *);
int kernelGraphicGetScreenWidth(void);
int kernelGraphicGetScreenHeight(void);
int kernelGraphicCalculateAreaBytes(int, int);
int kernelGraphicClearScreen(color *);
int kernelGraphicGetColor(const char *, color *);
int kernelGraphicSetColor(const char *, color *);
int kernelGraphicDrawPixel(kernelGraphicBuffer *, color *, drawMode, int, int);
int kernelGraphicDrawLine(kernelGraphicBuffer *, color *, drawMode, int, int,
			  int, int);
int kernelGraphicDrawRect(kernelGraphicBuffer *, color *, drawMode, int, int,
			  int, int, int, int);
int kernelGraphicDrawOval(kernelGraphicBuffer *, color *, drawMode, int, int,
			  int, int, int, int);
int kernelGraphicImageToKernel(image *);
int kernelGraphicNewImage(image *, int, int);
int kernelGraphicNewKernelImage(image *, int, int);
int kernelGraphicDrawImage(kernelGraphicBuffer *, image *, drawMode, int, int,
			   int, int, int, int);
int kernelGraphicGetImage(kernelGraphicBuffer *, image *, int, int, int, int);
int kernelGraphicGetKernelImage(kernelGraphicBuffer *, image *, int, int, int,
				int);
int kernelGraphicDrawText(kernelGraphicBuffer *, color *, color *,
			  kernelAsciiFont *, const char *, drawMode, int, int);
int kernelGraphicCopyArea(kernelGraphicBuffer *, int, int, int, int, int, int);
int kernelGraphicClearArea(kernelGraphicBuffer *, color *, int, int, int, int);
int kernelGraphicRenderBuffer(kernelGraphicBuffer *, int, int, int, int, int,
			      int);
int kernelGraphicFilter(kernelGraphicBuffer *, color *, int, int, int, int);
void kernelGraphicDrawGradientBorder(kernelGraphicBuffer *, int, int, int, int,
				     int, color *, int, drawMode, borderType);

#define _KERNELGRAPHIC_H
#endif
