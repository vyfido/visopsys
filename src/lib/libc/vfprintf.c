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
//  vfprintf.c
//

// This is the standard "vfprintf" function, as found in standard C libraries

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/api.h>
#include <sys/cdefs.h>


int vfprintf(FILE *theStream, const char *format, va_list list)
{
	int status = 0;
	int len = 0;
	char output[MAXSTRINGLENGTH + 1];

	if (visopsys_in_kernel)
		return (errno = ERR_BUG);

	if ((theStream == stdout) || (theStream == stderr))
	{
		status = vprintf(format, list);
		return (status);
	}

	// Fill out the output line
	len = _xpndfmt(output, MAXSTRINGLENGTH, format, list);

	if (len < 0)
	{
		errno = len;
		return (0);
	}

	status = fileStreamWrite((fileStream *) theStream, len, output);
	if (status < 0)
	{
		errno = status;
		return (0);
	}

	return (len);
}

