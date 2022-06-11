//
//  Visopsys
//  Copyright (C) 1998-2015 J. Andrew McLaughlin
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
//  _str2num.c
//

// This is a generic function to interpret a string as a number and return
// the value.

#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <sys/cdefs.h>


unsigned long long _str2num(const char *string, unsigned base, int sign)
{
	unsigned long long result = 0;
	int length = 0;
	int negative = 0;
	int count = 0;

	if (!string)
	{
		errno = ERR_NULLPARAMETER;
		return (0);
	}

	// Get the length of the string
	length = strlen(string);

	if (sign && (string[0] == '-'))
	{
		negative = 1;
		count += 1;
	}

	// Do a loop to iteratively add to the value of 'result'.
	for ( ; count < length; count ++)
	{
		switch (base)
		{
			case 10:
				if (!isdigit(string[count]))
				{
					errno = ERR_INVALID;
					goto out;
				}
				result *= base;
				result += (string[count] - '0');
				break;

			case 16:
				if (!isxdigit(string[count]))
				{
					errno = ERR_INVALID;
					goto out;
				}
				result *= base;
				if ((string[count] >= '0') && (string[count] <= '9'))
					result += (string[count] - '0');
				else if ((string[count] >= 'a') && (string[count] <= 'f'))
					result += ((string[count] - 'a') + 10);
				else
					result += ((string[count] - 'A') + 10);
				break;

			default:
				errno = ERR_NOTIMPLEMENTED;
				goto out;
		}
	}

out:
	if (negative)
		result = ((long long) result * -1);

	return (result);
}

