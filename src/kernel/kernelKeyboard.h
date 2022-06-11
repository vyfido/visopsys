//
//  Visopsys
//  Copyright (C) 1998-2013 J. Andrew McLaughlin
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
//  kernelKeyboard.h
//
	
#if !defined(_KERNELKEYBOARD_H)

#include "kernelDevice.h"
#include <sys/keyboard.h>
#include <sys/stream.h>

extern keyMap *kernelKeyMap;

// An enum for 'special' keyboard events such as CTRL/ALT/DEL and anything
// else we care about that isn't translatable into an ASCII code.
typedef enum {
  keyboardEvent_altPress,
  keyboardEvent_altRelease,
  keyboardEvent_altTab,
  keyboardEvent_ctrlAltDel,
  keyboardEvent_printScreen,
  keyboardEvent_f1,
  keyboardEvent_f2

} kernelKeyboardEvent;

// Functions exported by kernelKeyboard.c
int kernelKeyboardInitialize(void);
int kernelKeyboardGetMap(keyMap *);
int kernelKeyboardSetMap(const char *);
int kernelKeyboardSetStream(stream *);
int kernelKeyboardSpecial(kernelKeyboardEvent);
int kernelKeyboardInput(int, int);

#define _KERNELKEYBOARD_H
#endif
