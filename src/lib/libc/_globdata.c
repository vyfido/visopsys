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
//  _globdata.c
//

// This contains standard data which should be linked to all programs made
// using this standard library.

#include <locale.h>
extern struct lconv _c_locale;

// This is the global 'errno' error status variable for this program
int errno = 0;

// This allows us to ensure that kernel API functions are not called from
// within the kernel.
int visopsys_in_kernel = 0;

// Pointer to the current locale
struct lconv *_current_locale = &_c_locale;