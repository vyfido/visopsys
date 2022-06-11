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
//  rewind.c
//

// This is the standard "rewind" function, as found in standard C libraries

#include <stdio.h>
#include <errno.h>
#include <sys/api.h>


void rewind(FILE *stream)
{
  // The rewind function sets the file position indicator for the stream
  // pointed to by stream to the beginning of the file.  It is equivalent
  // to:
  //      (void)fseek(stream, 0L, SEEK_SET)
  // except that the error indicator for the stream is also cleared.  The
  // rewind function returns no value.

  // Let the kernel do all the work, baby.
  errno = fileStreamSeek(stream, 0);
  return;
}
