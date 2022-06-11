// 
//  Visopsys
//  Copyright (C) 1998-2006 J. Andrew McLaughlin
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
//  Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
//
//  text.h
//

// This file contains definitions and structures for using and manipulating
// text in Visopsys.

#if !defined(_TEXT_H)

// Definitions
#define TEXT_STREAMSIZE               32768
#define TEXT_DEFAULT_TAB              8
#define TEXT_DEFAULT_SCROLLBACKLINES  256

// Colours for the text console
#define TEXT_COLOR_BLACK              0
#define TEXT_COLOR_BLUE               1
#define TEXT_COLOR_GREEN              2
#define TEXT_COLOR_CYAN               3
#define TEXT_COLOR_RED                4
#define TEXT_COLOR_MAGENTA            5
#define TEXT_COLOR_BROWN              6
#define TEXT_COLOR_LIGHTGREY          7
#define TEXT_COLOR_DARKGREY           8
#define TEXT_COLOR_LIGHTBLUE          9
#define TEXT_COLOR_LIGHTGREEN         10
#define TEXT_COLOR_LIGHTCYAN          11
#define TEXT_COLOR_LIGHTRED           12
#define TEXT_COLOR_LIGHTMAGENTA       13
#define TEXT_COLOR_YELLOW             14
#define TEXT_COLOR_WHITE              15

#define TEXT_DEFAULT_FOREGROUND       TEXT_COLOR_LIGHTGREY
#define TEXT_DEFAULT_BACKGROUND       TEXT_COLOR_BLUE
#define TEXT_DEFAULT_ERRORFOREGROUND  TEXT_COLOR_BROWN

typedef struct {
  int column;
  int row;
  unsigned char *data;

} textScreen;

#define _TEXT_H
#endif
