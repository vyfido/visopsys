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
//  div.c
//

// This is the standard "div" function, as found in standard C libraries

#include <stdlib.h>
#include <errno.h>


div_t div(int numer, int denom)
{
  // The div() function computes the value numer/denom and returns the
  // quotient and remainder in a structure named div_t that contains two
  // integer members named quot and rem.

  div_t result;

  if (denom != 0)
    {
      result.quot = (numer / denom);
      result.rem = (numer % denom);
      errno = 0;
    }
  else
    {
      result.quot = 0;
      result.rem = 0;
      errno = ERR_DIVIDEBYZERO;
    }

  return (result);
}
