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
//  vbf.h
//

#if !defined(_VBF_H)

#include <sys/font.h>

#define VBF_MAGIC			"VBF"
#define VBF_MAGIC_LEN		4
#define VBF_VERSION			0x00010000

typedef struct {
	char magic[4];					// VBF_MAGIC
	int version;					// VBF_VERSION (bcd VBF_VERSION)
	char name[32];					// Font name
	int points;						// Size in points (e.g. 10, 12, 20)
	char codePage[16];				// e.g. ISO-8859-15
	int numGlyphs;					// Number of glyphs in file
	int glyphWidth;					// Fixed width of all glyphs
	int glyphHeight;				// Fixed height of all glyphs
	int codes[];					// List of codepage values
	// unsigned char data[];		// Bitmap follows codes.  Each glyph is
	// padded to a byte boundary, so the size of the bitmap is:
	// numGlyphs * (((glyphWidth * glyphHeight) + 7) / 8)

} __attribute__((packed)) vbfFileHeader;

#define _VBF_H
#endif

