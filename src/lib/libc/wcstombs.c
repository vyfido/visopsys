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
//  wcstombs.c
//

// This is the standard "wcstombs" function, as found in standard C libraries

#include <stdlib.h>
#include <sys/utf.h>


size_t wcstombs(char *dest, const wchar_t *src, size_t n)
{
	// Here's how the Linux man page describes this function (oh boy):
	//
	// If dest is not NULL, the wcstombs() function converts the wide-
	// character string src to a multibyte string starting at dest.  At most n
	// bytes are written to dest. The conversion starts in the initial state.
	// The conversion can stop for three reasons:
	//
	// 1. A wide character has been encountered that can not be represented
	// as a multibyte sequence (according to the current locale).  In this
	// case, (size_t) -1 is returned.
	//
	// 2. The length limit forces a stop.  In this case, the number of bytes
	// written to dest is returned, but the shift state at this point is lost.
	//
	// 3. The wide-character string has been completely converted, including
	// the terminating null wide character (L'\0').  In this case, the
	// conversion ends in the initial state.  The number of bytes written to
	// dest, excluding the terminating null byte ('\0'), is returned.
	//
	// The programmer must ensure that there is room for at least n bytes at
	// dest.
	//
	// If dest is NULL, n is ignored, and the conversion proceeds as above,
	// except that the converted bytes are not written out to memory, and no
	// length limit exists.
	//
	// In order to avoid the case 2 above, the programmer should make sure n
	// is greater than or equal to wcstombs(NULL, src, 0) + 1.
	//
	// We're going to attempt to support UTF-8 as our multibyte standard.

	size_t outBytes = 0;
	int numBytes = 0;

	while (!dest || (outBytes < n))
	{
		if (dest)
			numBytes = wctomb(dest, *src);
		else
			numBytes = utf8CodeWidth(*src);

		if (numBytes <= 0)
			return (-1);

		if (*src == L'\0')
			break;

		dest += numBytes;
		src += 1;
		outBytes += numBytes;
	}

	return (outBytes);
}

