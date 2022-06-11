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
//  kernelMouseFunctions.h
//
	
// This goes with the file kernelMouseFunctions.c

#if !defined(_KERNELMOUSEFUNCTIONS_H)

#include "kernelImageFunctions.h"

// Mouse event masks
#define MOUSE_DOWN   0x01
#define MOUSE_UP     0x02
#define MOUSE_MOVE   0x04
#define MOUSE_DRAG   0x08

// A structure for holding pointers to the mouse driver functions
typedef struct
{
  int (*driverInitialize) (void);
  void (*driverReadData) (void);

} kernelMouseDriver;

typedef struct
{
  char name[64];
  image pointerImage;

} kernelMousePointer;

// A structure for holding information about the mouse object
typedef struct
{
  kernelMouseDriver *deviceDriver;

} kernelMouseObject;

// Keeps mouse pointer size and location data
typedef struct
{
  int xPosition;
  int yPosition;
  unsigned width;
  unsigned height;
  int button1;
  int button2;
  int button3;
  int eventMask;

} kernelMouseStatus;

// Functions exported by kernelMouseFunctions.c
int kernelMouseRegisterDevice(kernelMouseObject *);
int kernelMouseInstallDriver(kernelMouseDriver *);
int kernelMouseInitialize(void);
int kernelMouseReadData(void);
int kernelMouseLoadPointer(const char *, const char *);
int kernelMouseSwitchPointer(const char *);
void kernelMouseDraw(void);
void kernelMouseMove(int, int);
void kernelMouseButtonChange(int, int);

#define _KERNELMOUSEFUNCTIONS_H
#endif
