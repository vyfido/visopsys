//
//  Visopsys
//  Copyright (C) 1998-2020 J. Andrew McLaughlin
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
//  wctomb.c
//

// This is the standard "wctomb" function, as found in standard C libraries

#include <stdlib.h>
#include <sys/utf.h>


int wctomb(char *s, wchar_t wc)
{
	// Here's how the Linux man page describes this function:
	//
	// If s is not NULL, the wctomb function converts the wide character wc to
	// its multibyte representation and stores it at the beginning of the
	// character array pointed to by string.  It updates the shift state,
	// which is stored in a static anonymous variable only known to the wctomb
	// function, and returns the length of said multibyte representation, i.e.
	// the number of bytes written at string. The programmer must ensure that
	// there is room for at least MB_CUR_MAX bytes at string.  If string is
	// NULL, the wctomb function resets the shift state, only known to this
	// function, to the initial state, and returns non-zero if the encoding
	// has non-trivial shift state, or zero if the encoding is stateless.
	//
	// We're going to attempt to support UTF-8 as our multibyte standard.

	int numBytes = 0;

	if (!s)
		// Stateless
		return (0);

	numBytes = utf8CodeWidth(wc);
	if (!numBytes)
		// Too large
		return (-1);

	unicodeToUtf8Char(wc, (unsigned char *) s, sizeof(wchar_t) /* assumed */);

	return (numBytes);
}

