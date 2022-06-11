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
//  utoa.c
//

// This is a function that turns an unsigned int into a string.  I don't
// know which regular library function is supposed to do this

#include <stdlib.h>
#include <errno.h>


void utoa(unsigned int number, char *string)
{
  unsigned int place = 1000000000;  // Decimal - 1 billion
  int leadZero = 1;
  unsigned int remainder = 0;
  int count = 0;

  if (string == NULL)
    {
      errno = ERR_NULLPARAMETER;
      return;
    }

  errno = 0;

  while (1)
    {
      remainder = (number % place);
      number = (number / place);
      
      if (number || !leadZero || (place == 1))
	{
	  leadZero = 0;
	  
	  string[count++] = (char) ('0' + number);
	}

      number = remainder;

      if (place > 1)
	place /= 10;
      else
	break;
    }

  string[count] = '\0';

  // Done
  return;
}
