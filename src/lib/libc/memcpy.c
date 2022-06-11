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
//  memcpy.c
//

// This is the standard "memcpy" function, as found in standard C libraries

#include <string.h>
#include <errno.h>

#define copyBytes(src, dest, count)		\
	__asm__ __volatile__ ("pushal \n\t"	\
		"pushfl \n\t"					\
		"cld \n\t"						\
		"rep movsb \n\t"				\
		"popfl \n\t"					\
		"popal"							\
		: : "S" (src), "D" (dest), "c" (count))

#define copyDwords(src, dest, count)	\
	__asm__ __volatile__ ("pushal \n\t"	\
		"pushfl \n\t"					\
		"cld \n\t"						\
		"rep movsl \n\t"				\
		"popfl \n\t"					\
		"popal"							\
		: : "S" (src), "D" (dest), "c" (count))


void *memcpy(void *dest, const void *src, size_t bytes)
{
	// The memcpy() function copies len bytes from memory area src to memory
	// area dest.  The memory areas may not overlap.  Use memmove if the
	// memory areas do overlap.

	unsigned dwords = (bytes >> 2);

	// Check params
	if ((src == NULL) || (dest == NULL))
	{
		errno = ERR_NULLPARAMETER;
		return (NULL);
	}

	if (((unsigned) src % 4) || ((unsigned) dest % 4) || (bytes % 4))
		copyBytes(src, dest, bytes);
	else
		copyDwords(src, dest, dwords);

	return (dest);
}
