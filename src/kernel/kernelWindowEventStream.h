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
//  kernelWindowEventStream.h
//

// This file describes the kernel's facilities for reading and writing
// files using a 'streams' abstraction.  It's a convenience for dealing
// with files.

#if !defined(_KERNELWINDOWEVENTSTREAM_H)

#include "kernelWindow.h"

// Functions exported by kernelWindowEventStream.c
int kernelWindowEventStreamNew(volatile windowEventStream *);
int kernelWindowEventStreamPeek(volatile windowEventStream *);
int kernelWindowEventStreamRead(volatile windowEventStream *, windowEvent *);
int kernelWindowEventStreamWrite(volatile windowEventStream *, windowEvent *);

#define _KERNELWINDOWEVENTSTREAM_H
#endif
