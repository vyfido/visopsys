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
//  calloc.c
//

// This is the standard "calloc" function, as found in standard C libraries

#include <stdlib.h>
#include <errno.h>
#include <sys/api.h>


void *calloc(size_t items, size_t itemSize)
{
  // This is what the linux man page says about this function:
  // calloc() allocates memory for an array of  nmemb  elements
  // of  size bytes each and returns a pointer to the allocated
  // memory.  The memory is set to zero.

  size_t totalSize = 0;
  void *memoryPointer = NULL;
  int count; 


  // Total size is (items * itemSize)
  totalSize = (items * itemSize);

  // Make sure the requested size is reasonable (not zero)
  if (!totalSize)
    {
      errno = ERR_NODATA;
      return NULL;
    }

  memoryPointer = memoryRequestBlock(totalSize, 0, "user heap");

  if (memoryPointer == NULL)
    errno = ERR_BADADDRESS;
  else
    {
      // We must clear the memory
      for (count = 0; count < (totalSize / sizeof(int)); count ++)
	((int *) memoryPointer)[count] = 0;
    }

  // Return this value, whether or not we were successful
  return (memoryPointer);
}
