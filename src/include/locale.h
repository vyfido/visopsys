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
//  locale.h
//

// This is the Visopsys version of the standard header file locale.h

#if !defined(_LOCALE_H)

struct lconv
{
    char *currency_symbol;
    char *decimal_point;
    char *grouping;
    char *int_curr_symbol;
    char *mon_decimal_point;
    char *mon_grouping;
    char *mon_thousands_sep;
    char *negative_sign;
    char *positive_sign;
    char *thousands_sep;
    char frac_digits;
    char int_frac_digits;
    char n_cs_precedes;
    char n_sep_by_space;
    char n_sign_posn;
    char p_cs_precedes;
    char p_sep_by_space;
    char p_sign_posn;
};

#define LC_COLLATE  0x01
#define LC_CTYPE    0x02
#define LC_MONETARY 0x04
#define LC_NUMERIC  0x08
#define LC_TIME     0x10
#define LC_ALL      (LC_COLLATE & LC_CTYPE & LC_MONETARY & LC_NUMERIC & \
                     LC_TIME)

#ifndef NULL
#define NULL 0
#endif

// Functions
struct lconv *localeconv(void);
char *setlocale(int, const char *);

#define _LOCALE_H
#endif
