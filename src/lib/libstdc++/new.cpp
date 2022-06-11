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
//  new.cpp
//

// This containst the standard "new" and "delete" functions, as found in
// standard C++ libraries

#include <new>
#include <cstddef>
#include <cstdlib>


void *operator new(std::size_t size)
{
	return (std::malloc(size));
}


void *operator new[](std::size_t size)
{
	return (std::malloc(size));
}


void operator delete(void *ptr)
{
	std::free(ptr);
}


void operator delete[](void *ptr)
{
	std::free(ptr);
}

