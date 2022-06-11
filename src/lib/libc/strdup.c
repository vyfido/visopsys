// 
//  Visopsys
//  Copyright (C) 1998-2014 J. Andrew McLaughlin
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
//  strdup.c
//

// This is the standard "strdup" function, as found in standard C libraries

#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/api.h>


char *strdup(const char *srcString)
{
	// I don't like this function.  Anyway, it makes a copy of the string.

	int length = 0;
	char *destString = NULL;

	// Check params
	if (srcString == NULL)
	{
		errno = ERR_NULLPARAMETER;
		return (destString = NULL);
	}

	if (visopsys_in_kernel)
		return (destString = NULL);

	length = strlen(srcString);

	destString = malloc(length + 1);
	if (destString == NULL)
	{
		errno = ERR_MEMORY;
		return (destString = NULL);
	}

	strncpy(destString, srcString, length);

	// Return success
	return (destString);
}
