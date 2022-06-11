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
//  xtoi.c
//

// This is a function that turns a hex string into an integer.
// I don't know which regular library function is supposed to do this

#include <stdlib.h>
#include <string.h>
#include <errno.h>


int xtoi(const char *string)
{
  int places = 1;
  int result = 0;
  int stringLength = 0;
  int count;

  if (string == NULL)
    {
      errno = ERR_NULLPARAMETER;
      return (0);
    }

  errno = 0;

  // Get the length of the string
  stringLength = strlen(string);

  // Do a loop to iteratively add to the value of 'result'.  Go from right
  // to left, setting 'places' based on the position in the string
  for (count = (stringLength - 1); count >= 0; count --)
    {
      // If we hit an 'x' or 'X', stop
      if ((string[count] == 'x') || (string[count] == 'X'))
	break;

      // Check to make sure input is numeric ascii or aA fF
      if (((string[count] < '0') && (string[count] > '9')) &&
	  ((string[count] < 'a') && (string[count] > 'f')) &&
	  ((string[count] < 'A') && (string[count] > 'F')))
	{
	  errno = ERR_INVALID;
	  return (0);
	}

      // Try to parse it
      if ((string[count] >= '0') && (string[count] <= '9'))
	result += places * (((int) string[count]) - 48);
      else if ((string[count] >= 'a') && (string[count] <= 'f'))
	result += places * ((((int) string[count]) - 97) + 10);
      else
	result += places * ((((int) string[count]) - 65) + 10);
      
      // update everything else
      places *= 16;
    }
  
  // Done
  return (result);
}
