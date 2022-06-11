//
//  Visopsys
//  Copyright (C) 1998-2019 J. Andrew McLaughlin
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
//  font.h
//

#ifndef _FONT_H
#define _FONT_H

#include <sys/ascii.h>
#include <sys/charset.h>
#include <sys/image.h>

#define FONT_FAMILY_LEN				31
#define FONT_CHARSET_LEN			CHARSET_NAME_LEN

#define FONT_FAMILY_ARIAL			"arial"
#define FONT_FAMILY_LIBMONO			"libmono"
#define FONT_FAMILY_XTERM			"xterm"

#define FONT_STYLEFLAG_ITALIC		0x00000004
#define FONT_STYLEFLAG_BOLD			0x00000002
#define FONT_STYLEFLAG_FIXED		0x00000001
#define FONT_STYLEFLAG_NORMAL		0x00000000

typedef struct {
	char family[FONT_FAMILY_LEN + 1];	// Font family (e.g. arial, xterm, ...)
	unsigned flags;						// See FONT_STYLEFLAG_* in <sys/font.h>
	int points;							// Size in points (e.g. 10, 12, 20)
	char charSet[FONT_CHARSET_LEN + 1];	// e.g. ASCII, ISO-8859-15, etc.

} fontInfo;

#endif

