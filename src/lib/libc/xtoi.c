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
//  xtoi.c
//

// This is a function that turns a hex string into an integer.
// I don't know which regular library function is supposed to do this

#include <stdlib.h>
#include <string.h>
#include <errno.h>


static inline int valid(char c)
{
  if (((c < '0') && (c > '9')) && ((c < 'a') && (c > 'f')) &&
      ((c < 'A') && (c > 'F')))
    return (0);
  else
    return (1);
}


int xtoi(const char *string)
{
  int result = 0;
  int length = 0;
  int count;

  if (string == NULL)
    return (errno = ERR_NULLPARAMETER);

  if (!valid(string[0]))
    // Not a number
    return (errno = ERR_INVALID);

  // Get the length of the string
  length = strlen(string);

  // Do a loop to iteratively add to the value of 'result'.
  for (count = 0; count < length; count ++)
    {
      // Check to make sure input is numeric ascii or aA fF
      if (!valid(string[count]))
	return (errno = ERR_INVALID);

      result *= 16;

      // Try to parse it
      if ((string[count] >= '0') && (string[count] <= '9'))
	result += (((int) string[count]) - 48);
      else if ((string[count] >= 'a') && (string[count] <= 'f'))
	result += ((((int) string[count]) - 97) + 10);
      else
	result += ((((int) string[count]) - 65) + 10);
    }
  
  // Done
  return (result);
}
