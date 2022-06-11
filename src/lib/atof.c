// 
//  Visopsys
//  Copyright (C) 1998-2003 J. Andrew McLaughlin
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
//  atof.c
//

// This is the standard "atof" function, as found in standard C libraries

#include <stdlib.h>
#include <errno.h>


double atof(const char *string)
{
  // From GNU: The atof() function  converts the initial portion of the
  // string pointed to by string to double.  The behaviour is the same as
  // strtod(string,(char **) NULL) except that atof() does not detect errors.

  errno = ERR_NOTIMPLEMENTED;
  return ((double) errno);
}
