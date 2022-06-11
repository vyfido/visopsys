// 
//  Visopsys
//  Copyright (C) 1998-2007 J. Andrew McLaughlin
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
//  strlen.c
//

// This is the standard "strlen" function, as found in standard C libraries

#include <string.h>
#include <errno.h>


size_t strlen (const char *theInputString)
{
  int count = 0;

  while ((theInputString[count] != '\0') &&
	 (count < MAXSTRINGLENGTH))
    count ++;

  // If this is true, then we probably have an unterminated string
  // constant.  Checking for a string that exceeds MAXSTRINGLENGTH will
  // help to prevent the routine from running off too far into memory.
  if (count >= MAXSTRINGLENGTH)
    {
      errno = ERR_BOUNDS;
      return (count = ERR_BOUNDS);
    }

  return (count);
}
