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
//  malloc.c
//

// This is the standard "malloc" function, as found in standard C libraries

#include <stdlib.h>
#include <errno.h>
#include <sys/api.h>


void *malloc(size_t requestedSize)
{
  // This is what the linux man page says about this function:
  // malloc() allocates size bytes and returns a pointer to the
  // allocated memory.  The memory is not cleared.

  void *memoryPointer = NULL;


  // Make sure the requested size is reasonable
  if (!requestedSize)
    {
      errno = ERR_NODATA;
      return NULL;
    }

  memoryPointer = memoryRequestBlock(requestedSize, 0, "user heap");

  if (memoryPointer == NULL)
    errno = ERR_BADADDRESS;

  // Return this value, whether or not we were successful
  return (memoryPointer);
}
