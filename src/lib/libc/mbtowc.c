//
//  Visopsys
//  Copyright (C) 1998-2021 J. Andrew McLaughlin
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
//  mbtowc.c
//

// This is the standard "mbtowc" function, as found in standard C libraries

#include <stdlib.h>
#include <sys/utf.h>


int mbtowc(wchar_t *wc, const char *bytes, size_t n)
{
	// Here's how the Linux man page describes this function:
	//
	// The main case for this function is when bytes is not NULL and wc is not
	// NULL.  In this case, the mbtowc function inspects at most n bytes of
	// the multibyte string starting at bytes, extracts the next complete
	// multibyte character, converts it to a wide character and stores it at
	// *wc.  It updates an internal shift state only known to the mbtowc
	// function. If bytes does not point to a '\0' byte, it returns the number
	// of bytes that were consumed from bytes, otherwise it returns 0.
	//
	// If the n bytes starting at bytes do not contain a complete multibyte
	// character, or if they contain an invalid multibyte sequence, mbtowc
	// returns -1.  This can happen even if n >= MB_CUR_MAX, if the multibyte
	// string contains redundant shift sequences.
	//
	// A different case is when bytes is not NULL but wc is NULL.  In this
	// case the mbtowc function behaves as above, excepts that it does not
	// store the converted wide character in memory.
	//
	// A third case is when bytes is NULL.  In this case, wc and n are
	// ignored.  The mbtowc function resets the shift state, only known to
	// this function, to the initial state, and returns non-zero if the
	// encoding has non-trivial shift state, or zero if the encoding is
	// stateless.
	//
	// We're going to attempt to support UTF-8 as our multibyte standard.

	int numBytes = 0;

	if (!bytes)
		// Stateless
		return (0);

	numBytes = mblen(bytes, n);
	if (numBytes < 0)
		return (-1);

	if (wc)
		*wc = (wchar_t) utf8CharToUnicode(bytes, n);

	return (numBytes);
}

