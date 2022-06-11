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
//  strcmp.c
//

// This is the standard "strcmp" function, as found in standard C libraries

#include <string.h>
#include <errno.h>


int strcmp(const char *s1, const char *s2)
{
  // The  strcmp() function compares the two strings s1 and s2.
  // It returns an integer less than, equal to, or greater than zero
  // if s1 is found, respectively, to be less than, to match, or be
  // greater than s2.

  int count = 0;


  // We don't normally set errno in this function
  errno = 0;

  for (count = 0; count < MAXSTRINGLENGTH; count ++)
    {
      if ((s1[count] == '\0') && (s2[count] == '\0'))
	// The strings match
	return (0);

      else if (s1[count] != s2[count])
	{
	  // The strings stop matching here.

	  // Is one of the characters a NULL character?  If so, that string
	  // is 'less than' the other
	  if (s1[count] == '\0')
	    return (-1);
	  else if (s2[count] == '\0')
	    return (1);

	  // Otherwise, is the s1 character in question 'less than' or
	  // 'greater than' the s2 character?  This will return a positive
	  // number if the s1 character is greater than s2, else a negative
	  // number.
	  return (s1[count] - s2[count]);
	}
    }

  // EEK, we have an overflow, but the strings match up to this point.
  // I'm not sure what the 'correct' thing to do is, but return 'equal'
  // anyway, whilst setting errno
  errno = ERR_BOUNDS;
  return (0);
}
