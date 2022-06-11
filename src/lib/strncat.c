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
//  strncat.c
//

// This is the standard "strncat" function, as found in standard C libraries

#include <string.h>
#include <errno.h>


char *strncat(char *destString, const char *sourceString, size_t maxLength)
{
  int count1, count2;
  int endFlag = 0;
  char sourceChar;
  
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

  // Now copy the source string into the dest.  If source is shorter than
  // maxLength, pad dest with NULL characters.
  for (count2 = 0; count2 < maxLength; )
    {
      if ((sourceString[count2] == (char) NULL) ||
	  (endFlag == 1))
	{
	  endFlag = 1;
	  sourceChar = (char) NULL;
	}
      
      else
	sourceChar = sourceString[count2];

      destString[count1] = sourceChar;
      count1++; count2++;
    }

  // Make sure there's a NULL at the end
  destString[count1] = NULL;

  // Return success
  errno = 0;
  return (destString);
}
