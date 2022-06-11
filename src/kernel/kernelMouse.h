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
//  kernelMouse.h
//
	
// This goes with the file kernelMouse.c

#if !defined(_KERNELMOUSE_H)

#include "kernelImage.h"
#include "kernelDevice.h"

#define MOUSE_MAX_POINTERS             16
#define MOUSE_POINTER_NAMELEN          64
#define MOUSE_DEFAULT_POINTER_DEFAULT  "/system/mouse/default.bmp"
#define MOUSE_DEFAULT_POINTER_BUSY     "/system/mouse/busy.bmp"
#define MOUSE_DEFAULT_POINTER_RESIZEH  "/system/mouse/resizeh.bmp"
#define MOUSE_DEFAULT_POINTER_RESIZEV  "/system/mouse/resizev.bmp"

// A structure for holding pointers to the mouse driver functions
typedef struct {
} kernelMouseOps;

typedef struct {
  char name[MOUSE_POINTER_NAMELEN];
  image pointerImage;

} kernelMousePointer;

// Functions exported by kernelMouse.c
int kernelMouseInitialize(void);
int kernelMouseShutdown(void);
int kernelMouseLoadPointer(const char *, const char *);
kernelMousePointer *kernelMouseGetPointer(const char *);
int kernelMouseSetPointer(kernelMousePointer *);
void kernelMouseDraw(void);
void kernelMouseMove(int, int);
void kernelMouseButtonChange(int, int);
void kernelMouseScroll(int);
int kernelMouseGetX(void);
int kernelMouseGetY(void);

#define _KERNELMOUSE_H
#endif
