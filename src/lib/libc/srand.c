//
//  Visopsys
//  Copyright (C) 1998-2015 J. Andrew McLaughlin
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
//  srand.c
//

// This is the standard "srand" function, as found in standard C libraries

#include <stdlib.h>

extern unsigned __random_seed;


void srand(unsigned seed)
{
	// The srand() function sets its argument as the seed for a new sequence
	// of pseudo-random integers to be returned by rand().  These sequences
	// are repeatable by calling srand() with the same seed value.
	__random_seed = seed;
	return;
}

