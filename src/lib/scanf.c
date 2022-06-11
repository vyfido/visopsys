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
//  scanf.c
//

// This is the standard "printf" function, as found in standard C libraries

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <readline.h>
#include <errno.h>


int scanf(const char *format, ...)
{
  va_list list;
  int formatLength = 0;
  int matchItems = 0;
  char *input = NULL;
  char tmpChar;
  int count = 0;

  // We don't set errno in this function
  errno = 0;

  // Go through the format string until we reach the first formatting
  // argument.  Everything before it will be used as our prompt that we
  // send to readline.
  formatLength = strlen(format);
  for (count = 0; count < formatLength; )
    if ((format[count] != '%') ||
	// Make sure it's not "escaped"
	((format[count] == '%') && (format[count + 1] == '%')))
      {
	count++;
	continue;
      }

  // Temporarily place a NULL at the current position so that we don't
  // need to allocate a new string for the readline prompt
  tmpChar = format[count];
  ((char *) format)[count] = NULL;

  // Read the line
  input = readline(format);

  if (errno)
    // We matched zero items
    return (matchItems = 0);

  // Replace the NULL
  ((char *) format) = tmpChar;

  // Initialize the argument list
  va_start(list, format);

  // Now assign the input values based on the input data and the format
  // string
  matchItems = _formatInput(input, format, list);

  va_end(list);
  
  // This gets malloc'd by readline, but we're finished with it.
  free(input);

  return (matchItems);
}
