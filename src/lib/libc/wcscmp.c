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
//  wcscmp.c
//

// This is the standard "wcscmp" function, as found in standard C libraries

#include <wchar.h>
#include <errno.h>
#include <string.h>


int wcscmp(const wchar_t *s1, const wchar_t *s2)
{
	// The wcscmp() function is the wide-character equivalent of the strcmp()
	// function

	int count = 0;

	// The spec doesn't really make it clear what to do with NULL parameters
	// here
	if (!s1 || !s2)
	{
		if (!s1 && s2)
		{
			return -1;
		}
		if (s1 && !s2)
		{
			return (1);
		}
		else
		{
			// Both NULL.  Fine.
			return (0);
		}
	}

	for (count = 0; count < MAXSTRINGLENGTH; count ++)
	{
		if (!s1[count] && !s2[count])
			// The strings are identical
			return (0);

		else if (s1[count] != s2[count])
		{
			// The strings stop matching here.  Is the s1 character in question
			// 'less than' or 'greater than' the s2 character?
			return ((s1[count] > s2[count])? 1 : -1);
		}
	}

	// EEK, we have an overflow, but the strings match up to this point.
	// I'm not sure what the 'correct' thing to do is, but return 'equal'
	// anyway, whilst setting errno
	errno = ERR_BOUNDS;
	return (0);
}

