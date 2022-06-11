// 
//  Visopsys
//  Copyright (C) 1998-2004 J. Andrew McLaughlin
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
//  Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
//
//  memcpy.c
//

// This is the standard "memcpy" function, as found in standard C libraries

#include <string.h>
#include <errno.h>


void *memcpy(void *dest, const void *src, size_t len)
{
  // The memcpy() function copies len bytes from memory area src to memory
  // area dest.  The memory areas may not overlap.  Use memmove if the
  // memory areas do overlap.

  size_t count = 0;

  // We don't set errno
  errno = 0;

  for (count = 0; count < len; count ++)
    ((char *) dest)[count] = ((char *) src)[count];

  return (dest);
}
