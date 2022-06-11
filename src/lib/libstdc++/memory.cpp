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
//  memory.cpp
//

// This is the standard "memory" definition, as found in standard C++
// libraries

#include <memory>
#include <cstdlib>

namespace std {

	template <class T>
	allocator<T>::allocator()
	{
	}


	template <class T>
	allocator<T>::allocator(const allocator<T>&) throw()
	{
	}


	template <class T>
	allocator<T>::~allocator()
	{
	}


	template <class T>
	typename allocator<T>::pointer allocator<T>::address(reference ref) const
	{
		return (&ref);
	}


	template <class T>
	typename allocator<T>::const_pointer allocator<T>::address(
		const_reference ref) const
	{
		return (&ref);
	}


	template <class T>
	typename allocator<T>::pointer allocator<T>::allocate(size_type size,
		allocator<void>::const_pointer hint) const
	{
		if (hint) { /* not implemented */ }

		return (new T[size]);
	}


	template <class T>
	void allocator<T>::deallocate(pointer ptr, size_type size) const
	{
		if (size)
			return (delete[] ptr);
	}


	template <class T>
	typename allocator<T>::size_type allocator<T>::max_size() const throw()
	{
		return (static_cast<size_type>(-1));
	}


	template class allocator<char>;
}

