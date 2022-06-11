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
//  strstr.c
//

// This is the standard "strstr" function, as found in standard C libraries

#include <string.h>
#include <errno.h>


char *strstr(const char *s1, const char *s2)
{
  // The strstr() function finds the first occurrence of the substring s2
  // in the string s1.  The terminating `\0' characters are not compared.
  // The strstr() function returns a pointer to the beginning of the
  // substring, or NULL if the substring is not found.

  int count = 0;
  char *ptr = NULL;
  int s1_length = strlen(s1);
  int s2_length = strlen(s2);


  // We don't set errno in this function
  errno = 0;

  ptr = (char *) s1;

  for (count = 0; count < s1_length; count ++)
    {
      if (!strncmp(ptr, s2, s2_length))
	return (ptr);
      else
	ptr++;
    }

  // Not found
  return (ptr = NULL);
}
