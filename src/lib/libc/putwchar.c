//
//  Visopsys
//  Copyright (C) 1998-2020 J. Andrew McLaughlin
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
//  putwchar.c
//

// This is the standard "putwchar" function, as found in standard C libraries

#include <wchar.h>
#include <stddef.h>
#include <errno.h>
#include <sys/api.h>


wint_t putwchar(wchar_t wc)
{
	// This is the wide-character equivalent of the putchar function

	int status = 0;

	if (visopsys_in_kernel)
	{
		errno = ERR_BUG;
		return (WEOF);
	}

	// Output a character to the text output stream
	status = textPutc(wc);
	if (status < 0)
	{
		errno = status;
		return (WEOF);
	}

	return (wc);
}

