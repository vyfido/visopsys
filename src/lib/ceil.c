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
//  ceil.c
//

// This is the standard "ceil" function, as found in standard C libraries

#include <math.h>
#include <errno.h>


double ceil(double d)
{
  // The ceil() function computes the smallest integral value not
  // less than x.

  int c = (int) d;
    
  // We don't set errno
  errno = 0;

  if (d > 0)
    d = (double) (c + 1);

  else if (d < 0)
    // ??? what to do ???  Is ceil(-5.5) == -6.0, or is
    // ceil(-5.5) == -5.0?  -5.0 according to the description.
    d = (double) c;

  return (d);
}
