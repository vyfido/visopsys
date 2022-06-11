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
//  strcat.c
//

// This is the standard "strcat" function, as found in standard C libraries

#include <string.h>
#include <errno.h>


char *strcat(char *destString, const char *sourceString)
{
  int count1, count2;
  
  // Find the end of the first String
  for (count1 = 0; count1 < MAXSTRINGLENGTH; )
    {
      if (destString[count1] == (char) NULL) break;
      else count1++;
    }

  // If this is true, then we probably have an unterminated string
  // constant.  Checking for a string that exceeds MAXSTRINGLENGTH will
  // help to prevent the routine from running off too far into memory.
  if (count1 >= MAXSTRINGLENGTH)
    {
      errno = ERR_BOUNDS;
      return (destString = NULL);
    }

  // Now copy the source string into the dest until the source is a 
  // NULL character.  
  for (count2 = 0; count2 < MAXSTRINGLENGTH; )
    {
      destString[count1] = sourceString[count2];
      
      if (sourceString[count2] == (char) NULL)
	break;

      else
	count1++; count2++;
    }

  errno = 0;
  return (destString);
}
