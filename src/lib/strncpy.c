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
//  strncpy.c
//

// This is the standard "strncpy" function, as found in standard C libraries

#include <string.h>
#include <errno.h>


char *strncpy(char *destString, const char *sourceString, size_t maxLength)
{
  int count;

  // Make sure neither of the pointers are NULL
  if ((destString == (char *) NULL) ||
      (sourceString == (char *) NULL))
    {
      errno = ERR_NULLPARAMETER;
      return (destString = NULL);
    }

  for (count = 0; count < maxLength; count ++)
    {
      destString[count] = sourceString[count];
      
      if ((sourceString[count] == (char) NULL) ||
	  (count >= MAXSTRINGLENGTH))
	break;
    }

  // Make sure there's a NULL at the end
  destString[count] = NULL;

  // If this is true, then we probably have an unterminated string
  // constant.  Checking for a string that exceeds MAXSTRINGLENGTH will
  // help to prevent the routine from running off too far into memory.
  if (count >= MAXSTRINGLENGTH)
    {
      errno = ERR_BOUNDS;
      return (destString = NULL);
    }

  // Return success
  errno = 0;
  return (destString);
}
