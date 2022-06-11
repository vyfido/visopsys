//
//  Visopsys
//  Copyright (C) 1998-2001 J. Andrew McLaughlin
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
//  modf.c
//

// This is the standard "modf" function, as found in standard C libraries

#include <math.h>
#include <errno.h>
#include <stddef.h>


double modf(double x, double *pint)
{
  // The modf() function breaks the argument x into an integral part and
  // a fractional  part, each of which has the same sign as x.  The
  // integral part is stored in iptr.

  int i;

  if (pint == NULL)
    {
      errno = ERR_NULLPARAMETER;
      return (0);
    }

  i = (int) x;
  *pint = (double) i;

  errno = 0;
  return (x - ((double) i));
}
