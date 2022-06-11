//
//  Visopsys
//  Copyright (C) 1998-2001 J. Andrew McLaughlin
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
//  kernelKeyboardFunctions.h
//
	
#if !defined(_KERNELKEYBOARDFUNCTIONS_H)

#include <sys/stream.h>

// Error messages
#define NULL_KBRD_MESS "The keyboard object passed or referenced was NULL"
#define NULL_KBRD_DRIVER_MESS "The keyboard driver passed or referenced was NULL"
#define NULL_KBRD_FUNC_MESS "The keyboard driver function requested was NULL"


// A structure for holding pointers to the keyboard driver functions
typedef struct
{
  int (*driverInitialize) (stream *, int (*)(stream *, ...));
  void (*driverReadData) (void);

} kernelKeyboardDriver;

// A structure for holding information about the keyboard object
typedef struct
{
  kernelKeyboardDriver *deviceDriver;

} kernelKeyboardObject;


// Functions exported by kernelKeyboardFunctions.c
int kernelKeyboardRegisterDevice(kernelKeyboardObject *);
int kernelKeyboardInstallDriver(kernelKeyboardDriver *);
int kernelKeyboardInitialize(void);
int kernelKeyboardReadData(void);

#define _KERNELKEYBOARDFUNCTIONS_H
#endif
