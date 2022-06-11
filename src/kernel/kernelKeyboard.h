//
//  Visopsys
//  Copyright (C) 1998-2007 J. Andrew McLaughlin
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
//  kernelKeyboard.h
//
	
#if !defined(_KERNELKEYBOARD_H)

#include "kernelDevice.h"
#include <sys/stream.h>

// A structure for holding keyboard key mappings
typedef struct {
  char name[32];
  char regMap[86];
  char shiftMap[86];
  char controlMap[86];
  char altGrMap[86];

} kernelKeyMap;

// A structure for holding information about the keyboard
typedef struct {
  unsigned flags;
  kernelKeyMap *keyMap;

} kernelKeyboard;

// A structure for holding pointers to the keyboard driver functions
typedef struct {
  void (*driverReadData) (void);

} kernelKeyboardOps;

// Functions exported by kernelKeyboard.c
int kernelKeyboardInitialize(kernelDevice *);
int kernelKeyboardGetMaps(char *, unsigned);
int kernelKeyboardSetMap(const char *);
int kernelKeyboardSetStream(stream *);
int kernelKeyboardInput(int, int);

#define _KERNELKEYBOARD_H
#endif
