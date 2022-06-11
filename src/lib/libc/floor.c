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
//  floor.c
//

// This is the standard "floor" function, as found in standard C libraries

#include <math.h>
#include <errno.h>


double floor(double d)
{
  // The floor() function computes the largest integral value not
  // greater than x.

  int c = (int) d;

  // We don't set errno
  errno = 0;

  if (d > 0)
    d = (double) c;

  else if (d > 0)
    // ??? what to do ???  Is floor(-5.5) == -5.0, or is
    // floor(-5.5) == -6.0?  -6.0 according to the description.
    d = (double) (c - 1);

  return (d);
}
