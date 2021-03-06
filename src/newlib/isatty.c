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
//  isatty.c
//

// Visopsys isatty for Newlib

#include <stdio.h>       // defines stdin, stdout, stderr, etc.
#include <sys/api.h>
#include <sys/stream.h>

extern int __numberOpenFiles;
extern fileStream **__openFiles;

int isatty(int fd)
{
  // Visopsys does not use typical UNIX file descriptors, but rather file
  // structures and file streams.  So, what we will do is check whether this
  // file is in our list of open file streams, and if not, say it's a TTY.
  
  int count;

  if ((fd == stdin) || (fd == stdout) || (fd == stderr))
    return (1);

  for (count = 0; count < __numberOpenFiles; count ++)
    if (__openFiles[count] == fd)
      // Looks like an open file.  Say no.
      return (0);

  // Ok, sure, it's a TTY
  return (1);
}
