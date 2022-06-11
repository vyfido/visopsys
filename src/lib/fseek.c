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
//  fseek.c
//

// This is the standard "fseek" function, as found in standard C libraries

#include <stdio.h>
#include <errno.h>
#include <sys/api.h>


int fseek(FILE *stream, long offset, int whence)
{
  // The fseek() function sets the file-position indicator for the
  // stream pointed to by stream.  The new position, measured in bytes
  // from the beginning of the file, is obtained by adding offset to the
  // position specified by whence, whose values are defined in <stdio.h>
  // as follows:
  //             SEEK_SET  Set position equal to offset bytes.
  //             SEEK_CUR  Set position to current location plus offset.
  //             SEEK_END  Set position to EOF plus offset.
  // fseek() returns 0 on success; otherwise, it returns -1 and sets errno
  // to indicate the error.

  long pos = 0;
  long new_pos = 0;

  // What is the position to which the user wants to seek?

  if (whence == SEEK_SET)
    // Seek to an absolute offset
    new_pos = offset;

  else if (whence == SEEK_CUR)
    {
      // What is the current position in the file?
      pos = ((stream->block * stream->f.blockSize) + stream->s.next);

      // Set position to current location plus offset.
      new_pos = (pos + offset);
    }

  else if (whence == SEEK_END)
    {
      // What is the current EOF of the file?  Set position to EOF plus
      // offset.
      new_pos = (stream->f.size + offset);
    }

  // Let the kernel do the rest of the work, baby.
  errno = fileStreamSeek(stream, new_pos);

  if (errno)
    return (-1);
  else
    return (0);
}
