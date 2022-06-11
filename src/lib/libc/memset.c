// 
//  Visopsys
//  Copyright (C) 1998-2005 J. Andrew McLaughlin
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
//  memset.c
//

// This is the standard "memset" function, as found in standard C libraries

// The description from the GNU man page reads as follows:
// The  memset() function fills the first n bytes of the memory area pointed
// to by s with the constant byte c. Returns a pointer to the memory area s.


#include <string.h>
#include <errno.h>


void *memset(void *string, int value, size_t number)
{
  unsigned count;

  // Check params
  if (string == NULL)
    {
      errno = ERR_NULLPARAMETER;
      return (NULL);
    }

  for (count = 0; count < number; count ++)
    ((char *) string)[count] = (char) value;
  
  // Return success
  return (string);
}
