// 
//  Visopsys
//  Copyright (C) 1998-2005 J. Andrew McLaughlin
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
//  fprintf.c
//

// This is the standard "fprintf" function, as found in standard C libraries

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <sys/api.h>


int fprintf(FILE *theStream, const char *format, ...)
{
  int status = 0;
  va_list list;
  int outputLen = 0;
  char output[MAXSTRINGLENGTH];
  
  // Initialize the argument list
  va_start(list, format);

  // Fill out the output line
  outputLen = _expandFormatString(output, format, list);

  va_end(list);

  if (outputLen < 0)
    return (0);

  status = fileStreamWrite((fileStream *) theStream, outputLen, output);
  if (status < 0)
    {
      errno = status;
      return (0);
    }
  
  return (outputLen);
}