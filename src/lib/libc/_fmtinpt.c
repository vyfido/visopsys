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
//  _fmtinpt.c
//

// This function does all the work of filling data values from input
// based on the format strings used by the scanf family of functions
// (and others, if desired)

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/cdefs.h>


int _formatInput(const char *input, const char *format, va_list list)
{
  int inputLen = 0;
  int formatLen = 0;
  // int inputCount = 0;
  int formatCount = 0;
  char formatAs;
  int matchItems = 0;
  void *argument = NULL;
  int isLong = 0;

  // How long are the input and format strings?
  inputLen = strlen(input);
  formatLen = strlen(format);

  if ((inputLen > MAXSTRINGLENGTH) || (formatLen > MAXSTRINGLENGTH))
    {
      errno = ERR_BOUNDS;
      return (matchItems = ERR_BOUNDS);
    }

  // The argument list must already have been initialized using va_start

  // Loop through all of the characters in the format string.  Ignore anything
  // that isn't a format character
  for (formatCount = 0; formatCount < formatLen; )
    {
      if ((format[formatCount] != '%') ||
	  ((format[formatCount] == '%') && (format[formatCount + 1] == '%')))
	continue;

      // Move to the next character
      formatCount += 1;

      // We have some format characters.  Get the corresponding argument,
      // move to the next format character.
      argument = va_arg(list, void *);

      // If there's a 'l' qualifier for long values, make note of it
      if (format[formatCount] == 'l')
	{
	  isLong = 1;
	  formatCount += 1;
	}
      else
	isLong = 0;

      // Remember the argument type, as we need to peek a little further
      // ahead
      formatAs = format[formatCount++];

      // What separates this argument from the next one, if any?

      // What is it?
      switch(format[formatCount])
	{
	case 'd':
	case 'i':
	  // This is an integer.
	  break;

	case 'u':
	  // This is an unsigned integer.
	  break;

	case 'c':
	  // A character.
	  break;

	case 's':
	  // This is a string.
	  break;

	default:
	  // Umm, we don't know what this is.  Chicken out.
	  return (errno = EINVAL);
	}

      formatCount += 1;
    }

  // Return the number of items we matched
  return (matchItems);
}

