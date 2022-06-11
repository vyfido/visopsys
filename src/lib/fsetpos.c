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


int fsetpos(FILE *stream, fpos_t *pos)
{
  // The fsetpos() function sets the file position indicator for the
  // stream pointed to by stream according to the value of the object
  // pointed to by pos, which must be a value obtained from an earlier
  // call to fgetpos() on the same stream.  Upon successful completion,
  // fsetpos returns 0.  Otherwise, -1 is returned and the global variable
  // errno is set to indicate the error.

  // Let the kernel do the rest of the work, baby.
  errno = fileStreamSeek(stream, *pos);

  if (errno)
    return (-1);
  else
    return (0);
}
