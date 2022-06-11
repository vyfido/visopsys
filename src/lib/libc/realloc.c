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
//  realloc.c
//

// This is the standard "realloc" function, as found in standard C libraries

#include <stdlib.h>
#include <errno.h>


void *realloc(void *old, size_t newSize)
{
  // This is what the linux man page says about this function:
  // realloc()  changes the size of the memory block pointed to
  // by ptr to size bytes.  The contents will be  unchanged  to
  // the minimum of the old and new sizes; newly allocated memory
  // will be uninitialized.  If ptr is NULL,  the  call  is
  // equivalent  to malloc(size); if size is equal to zero, the
  // call is equivalent to free(ptr).  Unless ptr is  NULL,  it
  // must  have  been  returned by an earlier call to malloc(),
  // calloc() or realloc().
  errno = ERR_NOTIMPLEMENTED;
  return (NULL);
}
