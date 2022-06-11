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
//  _xpndfmt.c
//

// This function does all the work of expanding the format strings used
// by the printf family of functions (and others, if desired)

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/api.h>
#include <sys/cdefs.h>


static int numDigits(unsigned long long foo, unsigned base)
{
  int digits = 1;
  while (foo >= base)
    {
      digits++;
      foo /= base;
    }
  return (digits);
}


int _expandFormatString(char *output, const char *format, va_list list)
{
  int inCount = 0;
  int outCount = 0;
  int formatlen = 0;
  long long argument = NULL;
  int zeroPad = 0;
  int leftJust = 0;
  int fieldWidth = 0; 
  int isLong = 0;
  int digits = 0;

  // How long is the format string?
  formatlen = strlen(format);
  if (formatlen > MAXSTRINGLENGTH)
    return (errno = ERR_BOUNDS);

  // The argument list must already have been initialized using va_start

  // Loop through all of the characters in the format string
  for (inCount = 0; ((inCount < formatlen) &&
		     (outCount < (MAXSTRINGLENGTH - 1))); )
    {
      if (format[inCount] != '%')
	{
	  output[outCount++] = format[inCount++];
	  continue;
	}
      else if ((format[inCount] == '%') && (format[inCount + 1] == '%'))
	{
	  // Copy it, but skip the next one
	  output[outCount++] = format[inCount];
	  inCount += 2;
	  continue;
	}

      // Move to the next character
      inCount += 1;

      // Look for a zero digit, which indicates that any field width argument
      // is to be zero-padded
      if (format[inCount] == '0')
	{
	  zeroPad = 1;
	  inCount++;
	}
      else
	zeroPad = 0;

      // Look for left-justification (applicable if there's a field-width
      // specifier to follow
      if (format[inCount] == '-')
	{
	  leftJust = 1;
	  inCount += 1;
	}
      else
	leftJust = 0;

      // Look for field length indicator
      if ((format[inCount] >= '1') && (format[inCount] <= '9'))
	{
	  fieldWidth = atoi(format + inCount);
	  while ((format[inCount] >= '0') && (format[inCount] <= '9'))   
	    inCount++;
        }
      else
	fieldWidth = 0;

      // If there's a 'll' qualifier for long values, make note of it
      if (format[inCount] == 'l')
	{
	  inCount += 1;
	  if (format[inCount] == 'l')
	    {
	      isLong = 1;
	      inCount += 1;
	    }
	}
      else
	isLong = 0;

      // We have some format characters.  Get the corresponding argument.
      if (isLong)
	{
	  argument = va_arg(list, unsigned);
	  argument |= ((long long) va_arg(list, unsigned) << 32);
	}
      else
	argument = va_arg(list, unsigned);

      // What is it?
      switch(format[inCount])
	{
	case 'd':
	case 'i':
	  // This is an integer.  Put the characters for the integer
	  // into the destination string
          if (fieldWidth)
            {
	      if (isLong)
		digits = numDigits(argument, 10);
	      else
		digits = numDigits((unsigned) argument, 10);
	      if (!leftJust)
		while (digits++ < fieldWidth)
		  output[outCount++] = (zeroPad? '0' : ' ');
            }
	  if (isLong)
	    lltoa(argument, (output + outCount));
	  else
	    itoa((int) argument, (output + outCount));
	  outCount = strlen(output);
	  if (fieldWidth && leftJust)
	    while (digits++ < fieldWidth)
	      output[outCount++] = ' ';
	  break;

	case 'u':
	  // This is an unsigned integer.  Put the characters for
	  // the integer into the destination string
	  if (fieldWidth)
	    {
	      if (isLong)
		digits = numDigits(argument, 10);
	      else
		digits = numDigits((unsigned) argument, 10);
	      if (!leftJust)
		while (digits++ < fieldWidth)
		  output[outCount++] = (zeroPad? '0' : ' ');
	    }
	  if (isLong)
	    ulltoa((unsigned long long) argument, (output + outCount));
	  else
	    utoa((unsigned) argument, (output + outCount));
	  outCount = strlen(output);
	  if (fieldWidth && leftJust)
	    while (digits++ < fieldWidth)
	      output[outCount++] = ' ';
	  break;

	case 'b':
	  if (fieldWidth)
	    {
	      if (isLong)
		digits = numDigits(argument, 2);
	      else
		digits = numDigits((unsigned) argument, 2);
	      if (!leftJust)
		while (digits++ < fieldWidth)
		  output[outCount++] = (zeroPad? '0' : ' ');
	    }
	  if (isLong)
	    lltob(argument, (output + outCount));
	  else
	    itob((int) argument, (output + outCount));
	  outCount = strlen(output);
	  if (fieldWidth && leftJust)
	    while (digits++ < fieldWidth)
	      output[outCount++] = ' ';
	  break;

	case 'c':
	  // A character.
	  output[outCount++] = (char) ((unsigned int) argument);
	  break;

	case 's':
	  // This is a string.  Copy the string from the next argument
	  // to the destnation string and increment outCount appropriately
	  if (argument)
	    {
	      strcpy((output + outCount), (char *) ((unsigned) argument));
	      outCount += strlen((char *) ((unsigned) argument));
	    }
	  else
	    {
	      // Eek.
	      strncpy((output + outCount), "(NULL)", 7);
	      outCount += 6;
	    }
	  break;

	case 'x':
	case 'X':
	  if (fieldWidth)
	    {
	      if (isLong)
		digits = numDigits(argument, 16);
	      else
		digits = numDigits((unsigned) argument, 16);
	      if (!leftJust)
		while (digits++ < fieldWidth)
		  output[outCount++] = (zeroPad? '0' : ' ');
	    }
	  if (isLong)
	    lltox(argument, (output + outCount));
	  else
	    itox((int) argument, (output + outCount));
	  outCount = strlen(output);
	  if (fieldWidth && leftJust)
	    while (digits++ < fieldWidth)
	      output[outCount++] = ' ';
	  break;

	default:
	  // Umm, we don't know what this is.  Just copy the '%' and
	  // the format character to the output stream
	  output[outCount++] = format[inCount - 1];
	  output[outCount++] = format[inCount];
	  break;
	}

      inCount += 1;
    }

  output[outCount] = '\0';

  // Return the number of characters we wrote
  return (outCount);
}
