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
//  fflush.c
//

// This is the standard "fflush" function, as found in standard C libraries

#include <stdio.h>
#include <errno.h>
#include <sys/api.h>


int fflush(FILE *theStream __attribute__((unused)))
{
  // This is currently not implemented in Visopsys; generally a file stream
  // is only fully flushed when it is closed.  If it's a text stream (like
  // writing to stdout or stderr) the output is always done synchronously
  // anyway.

  if (visopsys_in_kernel)
    return (errno = ERR_BUG);

  return (0);
}
