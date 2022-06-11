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
//  sin.c
//

// This is the standard "sin" function, as found in standard C libraries

#include <math.h>


double sin(double radians)
{
  // Returns the sine of x (x given in radians).  Adapted from an algorithm
  // found at http://www.dontletgo.com/planets/math.html

  double result = 0;
  double sign = 0;
  double x2nplus1 = 0;
  double factorial = 0;
  int n, count;

  for (n = 0; n < 10; n++)
    {
      sign = 1.0;
      for (count = 0; count < n; count ++)
	sign *= (double) -1;

      x2nplus1 = 1.0;
      for (count = 0; count < ((2 * n) + 1); count ++)
	x2nplus1 *= radians;

      factorial = 1.0;
      for (count = ((2 * n) + 1); count > 0; count --)
	factorial *= (double) count;

      result += (sign * (x2nplus1 / factorial));
    }

  return (result);
}
