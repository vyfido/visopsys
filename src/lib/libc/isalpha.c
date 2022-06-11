// 
//  Visopsys
//  Copyright (C) 1998-2007 J. Andrew McLaughlin
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
//  isalpha.c
//

// This is the standard "isalpha" function, as found in standard C libraries

// These functions check whether c, which must have the value of an
// unsigned char or EOF, falls into a certain character class according
// to the current locale.  Ok, right now they don't look at the current
// locale.

#include <ctype.h>


int isalpha (int c)
{
  // checks for an alphabetic character; in the standard "C"  locale, it is
  // equivalent  to (isupper(c) || islower(c)).  In some locales, there may
  // be additional characters for which isalpha() is true -- letters which
  // are neither upper case nor lower case.
  return (((c >= 'a') && (c <= 'z')) ||
	  ((c >= 'A') && (c <= 'Z')));
}
