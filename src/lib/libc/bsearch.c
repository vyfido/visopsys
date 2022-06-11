// 
//  Visopsys
//  Copyright (C) 1998-2004 J. Andrew McLaughlin
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
//  bsearch.c
//

// This is the standard "bsearch" function, as found in standard C libraries

#include <stdlib.h>
#include <errno.h>


void *bsearch(const void *key, const void *base, size_t members,
              size_t memberSize,
	      int (*compareFunction)(const void *, const void *))
{
  // From GNU: The bsearch() function searches an array of objects, the
  // initial member of which is pointed to by base, for a member that matches
  // the object pointed to by key.  The size of each member of the array is
  // specified by size.  The contents of the array should be in ascending
  // sorted order according to the comparison function referenced by
  // compareFunction.  The compareFunction routine is expected to have two
  // arguments which point to the key object and to an array member, in that
  // order, and should return an integer less than, equal to, or greater
  // than zero if the key object is found, respectively, to be less than,
  // to match, or be greater than the array member.

  errno = ERR_NOTIMPLEMENTED;
  return (NULL);
}
