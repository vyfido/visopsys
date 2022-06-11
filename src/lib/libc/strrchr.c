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
//  strrchr.c
//

// This is the standard "strrchr" function, as found in standard C libraries

// The strrchr() function returns a pointer to the last occurrence of the
// character in the string.


#include <string.h>
#include <errno.h>

char *strrchr(const char *string, int character)
{
  char *strptr = NULL;

  // Don't set errno in this function
  errno = 0;

  // Check params
  if (string == NULL)
    return ((char *) string);

  strptr = (char *) (string + strlen(string) - 1);

  while (strptr >= string)
    {
      if (strptr[0] == (char) character)
	return (strptr);
      strptr -= 1;
    }
  
  // Return failure
  return (NULL);
}
