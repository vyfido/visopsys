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
//  memcmp.c
//

// This is the standard "memcmp" function, as found in standard C libraries

#include <string.h>
#include <errno.h>


int memcmp(const void *first, const void *second, size_t length)
{
  int result = 0;

  // We don't set errno in this function
  errno = 0;

  // We trip through the strings, counting as we go.  If we get to
  // "length" and everything matches, we return 0.  Otherwise, we return
  // whether first is less than or greater than second

  for (result = 0; result < length; result ++)
    if (((unsigned char *) first)[result] !=
	((unsigned char *) second)[result])
      {
	if (((unsigned char *) first)[result] <
	    ((unsigned char *) second)[result])
	  return (result *= -1);
	
	else
	  return (result);
      }		
  
  // If we fall through to here, we matched as many as we could
  return (result = 0);
}
