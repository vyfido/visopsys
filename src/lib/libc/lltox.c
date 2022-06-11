// 
//  Visopsys
//  Copyright (C) 1998-2006 J. Andrew McLaughlin
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
//  lltox.c
//

// This is a function that turns a long long into a hex string.  I don't know
// which regular library function is supposed to do this

#include <stdlib.h>
#include <errno.h>


void lltox(long long number, char *string)
{
  unsigned remainder;
  int leadZero = 1;
  int place = 0;
  int count;

  if (string == NULL)
    {
      errno = ERR_NULLPARAMETER;
      return;
    }

  for (count = 0; count < 16; count ++)
    {
      remainder = (unsigned) ((number & 0xF000000000000000LL) >> 60);
      number <<= 4;

      if (remainder || !leadZero || (count == 15))
	{	
	  if (remainder <= 9)
	    string[place++] = (char) ('0' + remainder);
	  else
	    string[place++] = (char) ('a' + (remainder - 10));
	  leadZero = 0;
	}
    }

  string[place] = '\0';

  // Done
  return;
}
