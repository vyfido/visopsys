// 
//  Visopsys
//  Copyright (C) 1998-2006 J. Andrew McLaughlin
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
//  setlocale.c
//

// This is the standard "setlocale" function, as found in standard C libraries

#include <locale.h>
#include <limits.h>
#include <errno.h>

// This is the default 'C' locale
struct lconv _c_locale =
{
  "",         // currency_symbol
  ".",        // decimal_point
  "",         // grouping
  "",         // int_curr_symbol
  "",         // mon_decimal_point
  "",         // mon_grouping
  "",         // mon_thousands_sep
  "",         // negative_sign
  "",         // positive_sign
  "",         // thousands_sep
  CHAR_MAX,   // frac_digits
  CHAR_MAX,   // int_frac_digits
  CHAR_MAX,   // n_cs_precedes
  CHAR_MAX,   // n_sep_by_space
  CHAR_MAX,   // n_sign_posn
  CHAR_MAX,   // p_cs_precedes
  CHAR_MAX,   // p_sep_by_space
  CHAR_MAX,   // p_sign_posn
};
