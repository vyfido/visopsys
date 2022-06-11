// 
//  Visopsys
//  Copyright (C) 1998-2013 J. Andrew McLaughlin
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
//  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
//
// isgraph.c
//

// This is the standard "isgraph" function, as found in standard C libraries

// These functions check whether c, which must have the value of an
// unsigned char or EOF, falls into a certain character class according
// to the current locale.  Ok, right now they don't look at the current
// locale.

#include <ctype.h>


int isgraph(int c)
{
  // checks for any printable character except space.
  return (((c >= 1) && (c <= 7)) ||
	  (c == 11) || (c == 12) ||
	  ((c >= 14) && (c <= 25)) ||
	  ((c >= 27) && (c <= 31)) ||
	  ((c >= 33) && (c <= 255)));
}
