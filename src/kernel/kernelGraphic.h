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
#define DEFAULT_FOREGROUND_RED    40
#define DEFAULT_FOREGROUND_GREEN  93
#define DEFAULT_FOREGROUND_BLUE   171
#define DEFAULT_BACKGROUND_RED    200
#define DEFAULT_BACKGROUND_GREEN  200
#define DEFAULT_BACKGROUND_BLUE   200
#define DEFAULT_DESKTOP_RED       40
#define DEFAULT_DESKTOP_GREEN     93
#define DEFAULT_DESKTOP_BLUE      171
#define DEFAULT_SPLASH            "/system/visopsys.bmp"

// Structure to represent a drawing area
typedef volatile struct
{
  unsigned width;
  unsigned height;
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
  int (*driverDrawMonoImage) (kernelGraphicBuffer *, image *, drawMode,
			      color *, color *, int, int);
  int (*driverDrawImage) (kernelGraphicBuffer *, image *, drawMode, int, int,
			  unsigned, unsigned, unsigned, unsigned);
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
  void *framebuffer;
  int mode;
  unsigned xRes;
  unsigned yRes;
  unsigned bitsPerPixel;
  unsigned bytesPerPixel;
  int numberModes;
  videoMode supportedModes[MAXVIDEOMODES];
  kernelGraphicDriver *driver;

} kernelGraphicAdapter;

// The default driver initialization
int kernelFramebufferGraphicDriverInitialize(void);

// Functions exported by kernelGraphic.c
int kernelGraphicRegisterDevice(kernelGraphicAdapter *);
int kernelGraphicInitialize(void);
int kernelGraphicsAreEnabled(void);
int kernelGraphicGetModes(videoMode *, unsigned);
int kernelGraphicGetMode(videoMode *);
int kernelGraphicSetMode(videoMode *);
unsigned kernelGraphicGetScreenWidth(void);
unsigned kernelGraphicGetScreenHeight(void);
unsigned kernelGraphicCalculateAreaBytes(unsigned, unsigned);
int kernelGraphicClearScreen(color *);
int kernelGraphicGetColor(const char *, color *);
int kernelGraphicSetColor(const char *, color *);
int kernelGraphicDrawPixel(kernelGraphicBuffer *, color *, drawMode, int, int);
int kernelGraphicDrawLine(kernelGraphicBuffer *, color *, drawMode, int, int,
			  int, int);
int kernelGraphicDrawRect(kernelGraphicBuffer *, color *, drawMode, int, int,
			  unsigned, unsigned, unsigned, int);
int kernelGraphicDrawOval(kernelGraphicBuffer *, color *, drawMode, int, int,
			  unsigned, unsigned, unsigned, int);
int kernelGraphicNewImage(image *, unsigned, unsigned);
int kernelGraphicDrawImage(kernelGraphicBuffer *, image *, drawMode, int, int,
			   unsigned, unsigned, unsigned, unsigned);
int kernelGraphicGetImage(kernelGraphicBuffer *, image *, int, int,
			  unsigned, unsigned);
int kernelGraphicDrawText(kernelGraphicBuffer *, color *, color *,
			  kernelAsciiFont *, const char *, drawMode, int, int);
int kernelGraphicCopyArea(kernelGraphicBuffer *, int, int, unsigned, unsigned,
			  int, int);
int kernelGraphicClearArea(kernelGraphicBuffer *, color *, int, int,
			   unsigned, unsigned);
int kernelGraphicRenderBuffer(kernelGraphicBuffer *, int, int, int, int,
			      unsigned, unsigned);
void kernelGraphicDrawGradientBorder(kernelGraphicBuffer *, int, int,
				     unsigned, unsigned, int, int, drawMode);

#define _KERNELGRAPHIC_H
#endif
