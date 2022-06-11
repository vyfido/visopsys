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
//  difftime.c
//

// This is the standard "difftime" function, as found in standard C libraries

#include <time.h>
#include <errno.h>


double difftime(time_t time1, time_t time0)
{
  // The difftime() function returns the difference (time1 - time0)
  // expressed in seconds as a double.  The difftime() function is
  // provided because there are no general arithmetic properties defined
  // for type time_t.

  // We don't set errno
  errno = 0;

  return ((double) (time1 - time0));
}
