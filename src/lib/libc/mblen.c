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
//  mblen.c
//

// This is the standard "mblen" function, as found in standard C libraries

#include <stdlib.h>
#include <sys/utf.h>


int mblen(const char *s, size_t n)
{
	// Here's how the Linux man page describes this function:
	//
	// If s is not NULL, the mblen() function inspects at most n bytes of the
	// multibyte string starting at s and extracts the next complete multibyte
	// character.  It uses a static anonymous shift state known only to the
	// mblen() function.  If the multibyte character is not the null wide
	// character, it returns the number of bytes that were consumed from s.
	// If the multibyte character is the null wide character, it returns 0.
	//
	// If the n bytes starting at s do not contain a complete multibyte
	// character, mblen() returns -1.  This can happen even if n is greater
	// than or equal to MB_CUR_MAX, if the multibyte string contains redundant
	// shift sequences.
	//
	// If the multibyte string starting at s contains an invalid multibyte
	// sequence before the next complete character, mblen() also returns -1.
	//
	// If s is NULL, the mblen() function resets the shift state, known to
	// only this function, to the initial state, and returns nonzero if the
	// encoding has nontrivial shift state, or zero if the encoding is
	// stateless.
	//
	// We're going to attempt to support UTF-8 as our multibyte standard.

	if (!s)
		// Stateless
		return (0);

	if ((n >= 1) && UTF8_IS_1BYTE(s))
	{
		if (s[0])
			return (1);
		else
			return (0);
	}
	else if ((n >= 2) && UTF8_IS_2BYTE(s))
	{
		return (2);
	}
	else if ((n >= 3) && UTF8_IS_3BYTE(s))
	{
		return (3);
	}
	else if ((n >= 4) && UTF8_IS_4BYTE(s))
	{
		return (4);
	}
	else
	{
		return (-1);
	}
}

