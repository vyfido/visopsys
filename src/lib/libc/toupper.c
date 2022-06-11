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
//  toupper.c
//

// This is the standard "toupper" function, as found in standard C libraries

#include <ctype.h>


int toupper(int c)
{
  // If the argument of toupper() represents an lower-case letter, and there
  // exists a corresponding upper-case letter, the result is the
  // corresponding upper-case letter.  All other arguments are returned
  // unchanged.

  if ((c >= 97) && (c <= 122))
    return (c - 32);
  else
    return (c);
}
