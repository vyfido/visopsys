//
//  Visopsys
//  Copyright (C) 1998-2021 J. Andrew McLaughlin
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
//  mouse.h
//

// This file contains mouse-related definitions.

#ifndef _MOUSE_H
#define _MOUSE_H

#include <sys/paths.h>

#define MOUSE_MAX_POINTERS				16
#define MOUSE_POINTER_NAMELEN			31

// Pointer names
#define MOUSE_POINTER_DEFAULT			"default"
#define MOUSE_POINTER_BUSY				"busy"
#define MOUSE_POINTER_RESIZEH			"resizeh"
#define MOUSE_POINTER_RESIZEV			"resizev"

// Pointer images
#define MOUSE_DEFAULT_POINTER_DEFAULT	PATH_SYSTEM_MOUSE "/default.bmp"
#define MOUSE_DEFAULT_POINTER_BUSY		PATH_SYSTEM_MOUSE "/busy.ico"
#define MOUSE_DEFAULT_POINTER_RESIZEH	PATH_SYSTEM_MOUSE "/resizeh.bmp"
#define MOUSE_DEFAULT_POINTER_RESIZEV	PATH_SYSTEM_MOUSE "/resizev.bmp"

#endif

