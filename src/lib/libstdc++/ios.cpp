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
//  ios.cpp
//

// This is the standard "ios" definition, as found in standard C++ libraries

#include <iostream>

namespace std {

	int ios_base::Init::refCount;


	ios_base::Init::Init()
	{
		if (refCount <= 0)
		{
			streambuf sb;

			cin = istream(&sb);
			cout = ostream(&sb);
			cerr = ostream(&sb);
			clog = ostream(&sb);
		}

		refCount += 1;
	}


	ios_base::Init::~Init()
	{
		if (refCount <= 0)
		{
		}

		refCount -= 1;
	}


	ios_base::ios_base()
	{
	}


	template <class charT, class traits>
	basic_ios<charT, traits>::basic_ios()
	{
	}


	template class basic_ios<char, char_traits<char> >;
}

