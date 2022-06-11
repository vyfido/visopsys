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
//  rename.c
//

// This is the standard "rename" function, as found in standard C libraries

#include <stdio.h>
#include <errno.h>
#include <sys/api.h>


int rename(const char *old, const char *new)
{
  // The rename() function changes the name of a file.  The old argument
  // points to the pathname of the file to be renamed.  The new argument
  // points to the new pathname of the file.  Upon successful completion,
  // 0 is returned. Otherwise, -1 is returned and errno is set to indicate
  // an error.

  // Let the kernel do all the work, baby.
  errno = fileMove(old, new);

  if (errno)
    return (-1);
  else
    return (0);
}
