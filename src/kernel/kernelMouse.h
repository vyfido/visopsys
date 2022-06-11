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
//  kernelMouse.h
//
	
// This goes with the file kernelMouse.c

#if !defined(_KERNELMOUSE_H)

#include "kernelImage.h"

// A structure for holding pointers to the mouse driver functions
typedef struct
{
  int (*driverInitialize) (void);
  int (*driverRegisterDevice) (void *);
  void (*driverReadData) (void);

} kernelMouseDriver;

typedef struct
{
  char name[64];
  image pointerImage;

} kernelMousePointer;

// A structure for holding information about the mouse
typedef struct
{
  kernelMouseDriver *driver;

} kernelMouse;

// The default driver initialization
int kernelPS2MouseDriverInitialize(void);

// Functions exported by kernelMouse.c
int kernelMouseRegisterDevice(kernelMouse *);
int kernelMouseInitialize(void);
int kernelMouseReadData(void);
int kernelMouseLoadPointer(const char *, const char *);
int kernelMouseSwitchPointer(const char *);
void kernelMouseDraw(void);
void kernelMouseMove(int, int);
void kernelMouseButtonChange(int, int);

#define _KERNELMOUSE_H
#endif