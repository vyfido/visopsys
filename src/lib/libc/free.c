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
//  free.c
//

// This is the standard "free" function, as found in standard C libraries

#include <stdlib.h>
#include <errno.h>
#include <sys/api.h>


void free(void *oldMemory)
{
  // This is what the linux man page says about this function:
  // free()  frees  the  memory  space pointed to by ptr, which
  // must have been returned by a previous  call  to  malloc(),
  // calloc()  or  realloc().   Otherwise,  or if free(ptr) has
  // already been called before,  undefined  behaviour  occurs.
  // If ptr is NULL, no operation is performed.
  
  int status = memoryRelease(oldMemory);
  if (status < 0)
    errno = status; 

  return;
}
