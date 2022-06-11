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
//  fsetpos.c
//

// This is the standard "fsetpos" function, as found in standard C libraries

#include <stdio.h>
#include <errno.h>
#include <sys/api.h>


long ftell(FILE *stream)
{
  // The ftell() function obtains the current value of the file-position
  // indicator for the stream pointed to by stream.  Upon successful
  // completion, the ftell() function returns the current value of the
  // file-position indicator for the stream measured in bytes from the
  // beginning of the file.  Otherwise, they return -1 and sets errno to
  // indicate the error.

  // We don't set errno in this function
  errno = 0;
  
  return ((stream->block * stream->f.blockSize) + stream->s.next);
}
