//
//  Visopsys
//  Copyright (C) 1998-2003 J. Andrew McLaughlin
// 
//  This program is free software; you can redistribute it and/or modify it
//  under the terms of the GNU General Public License as published by the Free
//  Software Foundation; either version 2 of the License, or (at your option)
//  any later version.
// 
//  This program is distributed in the hope that it will be useful, but
//  WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
//  or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
//  for more details.
//  
//  You should have received a copy of the GNU General Public License along
//  with this program; if not, write to the Free Software Foundation, Inc.,
//  59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
//
//  fmod.c
//

// This is the standard "fmod" function, as found in standard C libraries

#include <math.h>
#include <errno.h>


double fmod(double x, double y)
{
  // The fmod() function computes the remainder of dividing x by y.
  // The return value is x - n * y, where n is the quotient of x / y,
  // rounded towards zero to an integer.

  // In other words, the fmod() function returns the value x - n * y,
  // for some integer n such that, if y is non-zero, the result has the
  // same sign as x and magnitude less than the magnitude of y.

  double m = 0;

  if (y == 0)
    {
      m = 0;
      errno = ERR_DIVIDEBYZERO;
    }
  else
    {
      m = (x - (floor(x / y) * y));
      errno = 0;
    }

  return (x);
}
