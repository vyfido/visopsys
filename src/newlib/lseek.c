//
//  Visopsys
//  Copyright (C) 1998-2004 J. Andrew McLaughlin
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
//  lseek.c
//

// Visopsys lseek for Newlib

#include <unistd.h>      // Defines SEEK_SET, SEEK_CUR, SEEK_END etc.
#include <sys/api.h>
#include <sys/stream.h>
#include <sys/types.h>
#include <sys/errno.h>

extern int __numberOpenFiles;
extern fileStream **__openFiles;

off_t lseek(int fd, off_t pos, int whence)
{
  // Visopsys does not use typical UNIX file descriptors, but rather file
  // structures and file streams.  So, what we do is look through the list
  // of open files and find the pointer that matches
  
  int count;
  fileStream *str = NULL;

  for (count = 0; count < __numberOpenFiles; count ++)
    if (__openFiles[count] == (fileStream *) fd)
      {
	str = __openFiles[count];
	
	switch (whence)
	  {
	  case SEEK_SET:
	    break;
	  case SEEK_CUR:
	    // Not supported at the moment
	    errno = EINVAL;
	    return ((off_t) -1);
	  case SEEK_END:
	    pos = str->f.size;
	    break;
	  default:
	    errno = EINVAL;
	    return ((off_t) -1);
	  }

	errno = fileStreamSeek(str, (int) pos);

	if (errno)
	  return ((off_t) -1);
	else
	  return (pos);
      }
  
  // If we fall through, we never found the file descriptor
  errno = ENOENT;
  return ((off_t) -1);
}
