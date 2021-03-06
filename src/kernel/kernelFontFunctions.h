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
//  kernelFontFunctions.h
//

#if !defined(_KERNELFONTFUNCTIONS_H)

#include "kernelImageFunctions.h"

#define ASCII_PRINTABLES 95
#define MAX_FONTS 16

// The font data structure for ascii-oriented fonts
typedef struct
{
  char name[32];
  int charWidth;
  int charHeight;
  image chars[ASCII_PRINTABLES]; // printable ascii characters

} kernelAsciiFont;

// Functions exported from kernelFontFunctions.c
int kernelFontInitialize(void);
int kernelFontGetDefault(kernelAsciiFont **);
int kernelFontSetDefault(const char *);
int kernelFontLoad(const char*, const char*, kernelAsciiFont **);
unsigned kernelFontGetPrintedWidth(kernelAsciiFont *, const char *);

#define _KERNELFONTFUNCTIONS_H
#endif
