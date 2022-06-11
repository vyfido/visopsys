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
//  strncmp.c
//

// This is the standard "strncmp" function, as found in standard C libraries

#include <string.h>
#include <errno.h>


int strncmp(const char *firstString, const char *secondString, size_t length)
{
  int result = 0;

  // We don't set errno in this function
  errno = 0;

  // We trip through the strings, counting as we go.  If we get to
  // the end, or "length" and everything matches, we return 0.  
  // Otherwise, if thestrings match partially, we return the count at which 
  // they diverge.  If they don't match at all, we return -1

  for (result = 0; ((result < MAXSTRINGLENGTH) && (result < length)); 
       result ++)
    {
      if ((firstString[result] == '\0') && (secondString[result] == '\0'))
	return (result = 0);

      else if (firstString[result] != secondString[result])
	{
	  if (result == 0) 
	    return (result = -1);

	  else
	    return (result);
	}
    }

  // If we fall through to here, we matched as many as we could
  return (result = 0);
}
