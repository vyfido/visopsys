// 
//  Visopsys
//  Copyright (C) 1998-2011 J. Andrew McLaughlin
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
//  fwrite.c
//

// This is the standard "fwrite" function, as found in standard C libraries

#include <stdio.h>
#include <errno.h>
#include <sys/api.h>


size_t fwrite(const void *buf, size_t size, size_t number, FILE *theStream)
{
  // Write 'size' bytes to the stream 'number' times
  
  int status = 0;
  size_t count = 0;

  if (visopsys_in_kernel)
    {
      errno = ERR_BUG;
      return (count);
    }

  while (count < number)
    {
      if ((theStream == stdout) || (theStream == stderr))
	status = textPrint((void *) (buf + (count * size)));
      else
	status =
	  fileStreamWrite(theStream, size, (void *) (buf + (count * size)));

      if (status < (int) size)
	{
	  if (status < 0)
	    errno = status;
	  break;
	}

      count += 1;
    }

  return (count);
}
