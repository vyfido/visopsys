//
//  Visopsys
//  Copyright (C) 1998-2015 J. Andrew McLaughlin
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
//  kernelFont.h
//

#if !defined(_KERNELFONT_H)

#include <sys/font.h>

#define FONTS_MAX			32

// Functions exported from kernelFont.c
int kernelFontInitialize(void);
int kernelFontGetDefault(asciiFont **);
int kernelFontLoadSystem(const char *, const char *, asciiFont **, int);
int kernelFontLoadUser(const char *, asciiFont **, int);
int kernelFontGetPrintedWidth(asciiFont *, const char *);
int kernelFontGetWidth(asciiFont *);
int kernelFontGetHeight(asciiFont *);

#define _KERNELFONT_H
#endif

