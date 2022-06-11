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
//  _div64.c
//
//  Adapted from Linux div64.h

// This function is the libc builtin function for 64-bit divs

#include <sys/cdefs.h>


uquad_t __div64(uquad_t a, uquad_t b, uquad_t *rem)
{
  unsigned upper = 0;
  unsigned low = 0;
  unsigned high = 0;
  unsigned mod = 0;
  unsigned base = b;

  __asm__ __volatile__ ("" : "=a" (low), "=d" (high) : "A" (a));
  upper = high;

  if (high)
    {
      upper = (high % base);
      high = (high / base);
    }

  __asm__ __volatile__ ("divl %2" : "=a" (low), "=d" (mod)
			: "rm" (base), "0" (low), "1" (upper));
  __asm__ __volatile__ ("" : "=A" (a) : "a" (low), "d" (high));

  if (rem)
    *rem = (uquad_t) mod;

  return (a);
}
