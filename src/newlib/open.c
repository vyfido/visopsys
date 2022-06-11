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
//  open.c
//

// Visopsys open for Newlib

#include <stdio.h>       // defines FOPEN_MAX, stdin, stdout, stderr, etc.
#include <fcntl.h>       // defines O_RDONLY, O_WRONLY, etc.
#include <stdlib.h>      // defines malloc
#include <errno.h>
#include <sys/api.h>
#include <sys/stream.h>

#ifndef FOPEN_MAX
#define FOPEN_MAX 128   // Number of max files is arbitrary here
#endif

int __numberOpenFiles = 0;
fileStream *__openFiles[FOPEN_MAX];  

int open(const char *name, int flags, ...)
{
  // Visopsys does not use typical UNIX file descriptors, but rather file
  // structures and file streams.  So, when we open the file we return its
  // fileStream pointer as the file descriptor.

  int mode = 0;
  fileStream *newStream = NULL;

  // Match up the UNIXy mode stuff with Visopsys stuff.  One of these first
  // three must be true
  if (flags & O_RDONLY)
    mode = OPENMODE_READ;
  else if (flags & O_WRONLY)
    mode = OPENMODE_WRITE;
  else if (flags & O_RDWR)
    mode = OPENMODE_READWRITE;
  else
    {
      errno = EINVAL;
      return (-1);
    }

  // Additional mode flags
  if (flags & O_CREAT)
    mode |= OPENMODE_CREATE;
  if (flags & O_TRUNC)
    mode |= OPENMODE_TRUNCATE;

  // Get some memory for the fileStream structure
  newStream = malloc(sizeof(fileStream));
  
  if (newStream == NULL)
    // Presumably errno is already set
    return (-1);

  errno = fileStreamOpen(name, mode, newStream);
    
  if (errno)
    return (-1);
  else
    {
      __openFiles[__numberOpenFiles++] = newStream;
      return ((int) newStream);
    }

  return (0);
}
