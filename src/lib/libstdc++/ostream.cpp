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
//  ostream.cpp
//

// This is the standard "ostream" definition, as found in standard C++
// libraries

#include <ostream>

using namespace std;

namespace visopsys {
	extern "C" {
		#include <sys/api.h>
	}
}

namespace std {

	template <class charT, class traits>
	basic_ostream<charT, traits>::basic_ostream(
		basic_streambuf<charT, traits> *)
	{
	}


	template <class charT, class traits>
	basic_ostream<charT, traits>::~basic_ostream()
	{
	}


	template <class charT, class traits>
	basic_ostream<charT, traits>& basic_ostream<charT, traits>::operator<<(
		basic_ostream<charT, traits>& (*pf)(basic_ostream<charT, traits>&))
	{
		return (pf(*this));
	}


	template <>
	ostream& ostream::operator<<(char c)
	{
		visopsys::textPutc(static_cast<int>(c));
		return (*this);
	}


	template <>
	ostream& ostream::operator<<(const char *s)
	{
		visopsys::textPrint(s);
		return (*this);
	}


	ostream& endl(ostream& os)
	{
		visopsys::textNewline();
		return (os);
	}


	template class basic_ostream<char, char_traits<char> >;
}

