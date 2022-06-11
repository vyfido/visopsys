// 
//  Visopsys
//  Copyright (C) 1998-2011 J. Andrew McLaughlin
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
//  strnlen.c
//

// This is the standard "strnlen" function, as found in standard C libraries

#include <stdlib.h>
#include <string.h>


size_t strnlen(const char *string, size_t maxlen)
{
  size_t count = 0;

  while ((string[count] != '\0') && (count < MAXSTRINGLENGTH) &&
	 (count < maxlen))
    count ++;

  if ((count >= MAXSTRINGLENGTH) || (count >= maxlen))
    return (min(MAXSTRINGLENGTH, maxlen));
  else
    return (count);
}
