// 
//  Visopsys
//  Copyright (C) 1998-2006 J. Andrew McLaughlin
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
//  tolower.c
//

// This is the standard "tolower" function, as found in standard C libraries

#include <ctype.h>


int tolower(int c)
{
  // If the argument of tolower() represents an upper-case letter, and there
  // exists a corresponding lower-case letter, the result is the
  // corresponding lower-case letter.  All other arguments are returned
  // unchanged.

  if ((c >= 65) && (c <= 90))
    return (c + 32);
  else
    return (c);
}
