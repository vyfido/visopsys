//
//  Visopsys
//  Copyright (C) 1998-2021 J. Andrew McLaughlin
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
//  islower.c
//

// This is the standard "islower" function, as found in standard C libraries

#include <ctype.h>


int islower(int c)
{
	// Checks for a lower-case character.

	// We use the ISO-8859-15 character set for this determination.
	return (((c >= 'a') && (c <= 'z')) ||
		(c == 168) || (c == 184) || (c == 189) ||
		((c >= 224) && (c <= 246)) ||
		((c >= 248) && (c <= 255)));
}

