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
//  wcscpy.c
//

// This is the standard "wcscpy" function, as found in standard C libraries

#include <wchar.h>
#include <errno.h>
#include <string.h>


wchar_t *wcscpy(wchar_t *dest, const wchar_t *src)
{
	// The wcscpy() function is the wide-character equivalent of the strcpy()
	// function.

	int count;

	// Make sure neither of the pointers are NULL
	if (!dest || !src)
	{
		errno = ERR_NULLPARAMETER;
		return (dest = NULL);
	}

	for (count = 0; count < MAXSTRINGLENGTH; count ++)
	{
		dest[count] = src[count];

		if (!src[count] || (count >= MAXSTRINGLENGTH))
			break;
	}

	// If this is true, then we probably have an unterminated string
	// constant.  Checking for a string that exceeds MAXSTRINGLENGTH will
	// help to prevent the function from running off too far into memory.
	if (count >= MAXSTRINGLENGTH)
	{
		errno = ERR_BOUNDS;
		return (dest = NULL);
	}

	// Return success
	return (dest);
}

