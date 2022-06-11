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
//  mbslen.c
//

// This is the standard "mbslen" function, as NOT found in standard C
// libraries, but found in gnulib and Microsoft runtime libraries

#include <string.h>
#include <stdlib.h>
#include <errno.h>


size_t mbslen(const char *string)
{
	size_t count = 0;
	size_t bytes = 0;
	int charLen = 0;

	if (!string)
	{
		errno = ERR_NULLPARAMETER;
		return (count = 0);
	}

	while (bytes <= MAXSTRINGLENGTH)
	{
		charLen = mblen((string + bytes), MB_CUR_MAX);
		if (charLen < 0)
		{
			errno = ERR_INVALID;
			return (count = 0);
		}

		if (!charLen)
			break;

		bytes += charLen;
		count += 1;
	}

	// If this is true, then we probably have an unterminated string
	// constant.  Checking for a string that exceeds MAXSTRINGLENGTH will
	// help to prevent the function from running off too far into memory.
	if (bytes > MAXSTRINGLENGTH)
	{
		errno = ERR_BOUNDS;
		return (count = 0);
	}

	return (count);
}

