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
//  ffs.c
//

// This is the standard "ffs" function, as found in standard C libraries

#include <string.h>
#include <values.h>


int ffs(int i)
{
  // Returns the least significant bit set in the word.  Lame.

  int count = 1;

  for (count = 1; !(i & 1) && (count <= (int) INTBITS); count ++)
    i >>= 1;

  if (count > (int) INTBITS)
    count = 0;

  return (count);
}