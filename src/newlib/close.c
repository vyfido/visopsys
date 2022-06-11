//
//  Visopsys
//  Copyright (C) 1998-2014 J. Andrew McLaughlin
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
//  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
//
//  close.c
//

// Visopsys close for Newlib

#include <errno.h>
#include <sys/api.h>

extern int __numberOpenFiles;
extern fileStream **__openFiles;  


int close(int fd)
{
  // Visopsys does not use typical UNIX file descriptors, but rather file
  // structures and file streams.  So, we try to find the supplied descriptor
  // in the list we're keeping, close the stream, and remove it from the
  // list
  
  int count;
  fileStream *str = NULL;

  for (count = 0; count < __numberOpenFiles; count ++)
    if (__openFiles[count] == (fileStream *) fd)
      {
	str = __openFiles[count];

	// Close it
	errno = fileStreamClose(str);

	if (errno)
	  return (-1);

	// Pre-decrement the number of open files
	__numberOpenFiles--;

	// Remove it from the list.  If this is not the last one in the list,
	// and if it is not the only one in the list, copy the last one into
	// its spot.
	if ((__numberOpenFiles > 0) && (count < __numberOpenFiles))
	  __openFiles[count] = __openFiles[__numberOpenFiles];

	return (0);
      }
  
  // If we fall through, we never found the file descriptor
  errno = ENOENT;
  return (-1);
}
