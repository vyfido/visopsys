//
//  Visopsys
//  Copyright (C) 1998-2003 J. Andrew McLaughlin
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
//  itox.c
//

// This is a function that turns an int into a string.  I don't know which
// regular library function is supposed to do this

#include <stdlib.h>
#include <errno.h>


void itox(int number, char *string)
{
  unsigned int remainder;
  int count;

  if (string == NULL)
    {
      errno = ERR_NULLPARAMETER;
      return;
    }

  errno = 0;

  for (count = 9; count > 1; count --)
    {
      remainder = (number % 16);
      number /= 16;

      if (remainder <= 9)
	string[count] = (char) ('0' + remainder);
      else
	string[count] = (char) ('a' + (remainder - 10));
    }

  string[0] = '0';
  string[1] = 'x';
  string[10] = '\0';

  // Done
  return;
}
