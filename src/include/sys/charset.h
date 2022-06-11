//
//  Visopsys
//  Copyright (C) 1998-2020 J. Andrew McLaughlin
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
//  charset.h
//

// This file contains definitions for working with different character sets

#ifndef _CHARSET_H
#define _CHARSET_H

#define CHARSET_NAME_LEN			31
#define CHARSET_CTRL_CODES			32
#define CHARSET_NUM_CODES			128
#define CHARSET_IDENT_CODES			128
#define CHARSET_NAME_ASCII			"ASCII"
#define CHARSET_NAME_UTF8			"UTF-8"
#define CHARSET_NAME_ISO_8859_5		"ISO-8859-5"	// Latin/Cyrillic (Russia)
#define CHARSET_NAME_ISO_8859_9		"ISO-8859-9"	// Latin-5 (Turkish)
#define CHARSET_NAME_ISO_8859_15	"ISO-8859-15"	// Latin-9 (W Europe)
#define CHARSET_NAME_ISO_8859_16	"ISO-8859-16"	// Latin-10 (SE Europe)
#define CHARSET_NAME_DEFAULT		CHARSET_NAME_UTF8

typedef struct {
	char name[CHARSET_NAME_LEN + 1];
	struct {
		unsigned code;
		unsigned unicode;

	} codes[CHARSET_NUM_CODES];

} charset;

#endif

