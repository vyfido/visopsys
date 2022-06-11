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
//  memmove.c
//

// This is the standard "memmove" function, as found in standard C libraries

#include <errno.h>
#include <string.h>


void *memmove(void *dest, const void *src, size_t len)
{
	// The memmove() function copies n bytes from memory area src to memory
	// area dest.  The memory areas may overlap.

	size_t count = 0;

	if ((dest == NULL) || (src == NULL))
	{
		errno = ERR_NULLPARAMETER;
		return (dest);
	}

	if (len)
	{
		// In case the memory areas overlap, we will copy the data differently
		// depending on the position of the src and dest pointers
		if (dest < src)
		{
			for (count = 0; count < len; count ++)
				((char *) dest)[count] = ((char *) src)[count];
		}
		else if (dest > src)
		{
			for (count = (len - 1); ; count --)
			{
				((char *) dest)[count] = ((char *) src)[count];

				// Can't do this easily in the 'for' above because it's unsigned
				if (!count)
					break;
			}
		}
	}
		
	return (dest);
}
