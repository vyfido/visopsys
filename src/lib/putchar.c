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
//  putchar.c
//

// This is the standard "putchar" function, as found in standard C libraries

#include <stdio.h>
#include <errno.h>
#include <sys/api.h>


int putchar(int c)
{
  // putchar(c); is equivalent to putc(c,stdout).

  // Get a character from the text input stream
  errno = textPutc(c);

  if (errno)
    return (EOF);
  else
    return (c);
}
