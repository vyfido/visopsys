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
//  bzero.c
//

// This is the standard "bzero" function, as found in standard C libraries

// The description from the GNU man page reads as follows:
// The bzero() function sets the first n bytes of the byte string to zero.
// The bzero() function returns no value.


#include <string.h>
#include <errno.h>


void bzero(void *string, size_t number)
{
  int count;

  // Check params
  if (string == NULL)
    {
      errno = ERR_NULLPARAMETER;
      return;
    }

  for (count = 0; count < number; count ++)
    ((char *) string)[count] = '\0';
  
  // Return success
  errno = 0;
  return;
}
