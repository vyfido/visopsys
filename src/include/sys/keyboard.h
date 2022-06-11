// 
//  Visopsys
//  Copyright (C) 1998-2014 J. Andrew McLaughlin
//  
//  This library is free software; you can redistribute it and/or modify it
//  under the terms of the GNU Lesser General Public License as published by
//  the Free Software Foundation; either version 2.1 of the License, or (at
//  your option) any later version.
//
//  This library is distributed in the hope that it will be useful, but
//  WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
//  General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public License
//  along with this library; if not, write to the Free Software Foundation,
//  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
//
//  keyboard.h
//

// This file contains definitions and structures for using and manipulating
// keyboards and keymaps in Visopsys.

#if !defined(_KEYBOARD_H)

#define KEYSCAN_CODES	86
#define KEYMAP_DIR		"/system/keymaps"
#define KEYMAP_MAGIC	"keymap"
#define KEYMAP_NAMELEN	32

// A structure for holding keyboard key mappings
typedef struct {
	char magic[8];
	char name[KEYMAP_NAMELEN];
	char regMap[KEYSCAN_CODES];
	char shiftMap[KEYSCAN_CODES];
	char controlMap[KEYSCAN_CODES];
	char altGrMap[KEYSCAN_CODES];

} keyMap;

#define _KEYBOARD_H
#endif
