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
//  vbf.h
//

#if !defined(_VBF_H)

#include <sys/font.h>

#define VBF_MAGIC    "VBF"
#define VBF_VERSION  0x00010000

typedef struct {
  char magic[4];  // VBF_MAGIC
  int version;    // VBF_VERSION (bcd)
  char name[32];  // Font name
  int points;
  char codePage[16];
  int numGlyphs;
  int glyphWidth;
  int glyphHeight;
  int codes[];

} __attribute__((packed)) vbfFileHeader;

#define _VBF_H
#endif
