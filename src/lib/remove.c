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
//  remove.c
//

// This is the standard "remove" function, as found in standard C libraries

#include <stdio.h>
#include <errno.h>
#include <sys/api.h>


int remove(const char *pathname)
{
  // The remove() function causes the file or empty directory whose name
  // is the string pointed to by pathname to be removed from the filesystem.
  // It calls delete() for files, and removeDir() for directories.  On
  // success, zero is returned.  On error, -1 is returned, and errno is
  // set appropriately.

  file f;

  // Figure out whether the file exists
  errno = fileFind(pathname, &f);

  if (errno)
    return (-1);

  // Now we should have some info about the file.  Is it a file or a 
  // directory?
  if (f.type == fileT)
    {
      // This is a regular file.
      errno = fileDelete(pathname);
    }
  else if (f.type == dirT)
    {
      // This is a directory
      errno = fileRemoveDir(pathname);
    }
  else
    {
      // Eek.  What kind of file is this?
      errno = ERR_INVALID;
      return (-1);
    }

  if (errno)
    return (-1);
  else
    return (0);
}
