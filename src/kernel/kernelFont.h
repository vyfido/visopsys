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
//  kernelFont.h
//

#ifndef _KERNELFONT_H
#define _KERNELFONT_H

#include <sys/font.h>
#include <sys/vis.h>

typedef struct {
	unsigned unicode;
	image img;

} kernelGlyph;

typedef struct {
	fontInfo info;
	linkedList charSet;					// e.g. ASCII, ISO-8859-15, etc.
	int numGlyphs;						// Number of glyphs in file
	int glyphWidth;						// Fixed width of all glyphs
	int glyphHeight;					// Fixed height of all glyphs
	kernelGlyph *glyphs;

} kernelFont;

// Functions exported from kernelFont.c
int kernelFontInitialize(void);
kernelFont *kernelFontGetSystem(void);
int kernelFontHasCharSet(kernelFont *, const char *);
kernelFont *kernelFontGet(const char *, unsigned, int, const char *);
int kernelFontGetPrintedWidth(kernelFont *, const char *, const char *);
int kernelFontGetWidth(kernelFont *);
int kernelFontGetHeight(kernelFont *);

#endif

