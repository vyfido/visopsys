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
//  atoi.c
//

// This is the standard "atoi" function, as found in standard C libraries

#include <stdlib.h>
#include <string.h>
#include <errno.h>


int atoi(const char *theString)
{
  // Here's how the Linux man page describes this function:
  // The  atoi()  function  converts the initial portion of the
  // string pointed to by nptr to int.  The  behaviour  is  the
  // same as
  //          strtol(nptr, (char **)NULL, 10);
  // 
  // except that atoi() does not detect errors.
  //
  // This one detects errors.

  int result = 0;
  int places = 1;
  int stringLength = 0;
  int count;


  // Get the length of the string
  stringLength = strlen(theString);

  // Do a loop to iteratively add to the value of 'result'.  Go from right
  // to left, setting 'places' based on the position in the string
  for (count = (stringLength - 1); count >= 0; count --)
    {
      // Check to make sure input is numeric ascii
      if ((theString[count] < 48) || (theString[count] > 57))
	{
	  errno = ERR_INVALID;
	  return (result = 0);
	}

      // Try to parse it
      result += places * (((int) theString[count]) - 48);
      
      // update everything else
      places *= 10;
    }
  
  return (result);
}
