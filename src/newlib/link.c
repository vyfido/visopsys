//
//  Visopsys
//  Copyright (C) 1998-2005 J. Andrew McLaughlin
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
//  link.c
//

// Visopsys link for Newlib

#include <errno.h>
#include <sys/api.h>

int link(char *src, char *dest)
{
  // For the moment we don't support actual links, hard or otherwise, so
  // instead we make a copy of the file
  
  errno = fileCopy(src, dest);

  if (errno)
    return (-1);
  else
    return (0);
}
