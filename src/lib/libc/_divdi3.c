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
//  _divdi3.c
//

// This function is the libc builtin function for signed 64-bit divs

#include <sys/cdefs.h>


quad_t __divdi3(quad_t a, quad_t b)
{
  uquad_t unsignedA = a;
  uquad_t unsignedB = b;
  uquad_t quotient = 0;
  int neg = 0;
  
  if (a < 0)
    {
      unsignedA = -((uquad_t) a);
      neg = 1;
    }
  if (b < 0)
    {
      unsignedB = -((uquad_t) b);
      neg ^= 1;
    }

  quotient = __div64(unsignedA, unsignedB, NULL);
  return (neg? -quotient : quotient);
}
