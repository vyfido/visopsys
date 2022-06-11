//
//  Visopsys
//  Copyright (C) 1998-2006 J. Andrew McLaughlin
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
//  sbrk.c
//

// Visopsys sbrk for Newlib

#include <sys/api.h>
#include <sys/types.h>
#include <sys/errno.h>

void *sbrk(size_t size)
{
  // Get some new memory.  We don't really follow the GNU definition of this
  // to the letter, since the heap memory returned may well be unrelated to
  // the previous one

  void *new = memoryRequestBlock(size, 0 /* no alignment */,
				 "process heap memory");

  if (new == NULL)
    errno = ENOMEM;

  return (new);
}
