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
//  gai_strerror.c
//

// This is the standard "gai_strerror" function, as found in standard C
// libraries

#include <netdb.h>

struct {
	int code;
	const char *string;

} errors[] = {
	{ EAI_ADDRFAMILY,	"no addresses in the requested address family" },
	{ EAI_AGAIN, 		"temporary failure" },
	{ EAI_BADFLAGS,		"invalid flags" },
	{ EAI_FAIL,			"name server failure" },
	{ EAI_FAMILY,		"address family not supported" },
	{ EAI_MEMORY,		"out of memory" },
	{ EAI_NODATA,		"host does not have any network addresses" },
	{ EAI_NONAME,		"node or service is unknown" },
	{ EAI_SERVICE,		"service not available for socket type" },
	{ EAI_SOCKTYPE,		"socket type not supported" },
	{ EAI_SYSTEM,		"other system error" },
	{ 0, NULL }
};


const char *gai_strerror(int code)
{
	const char *string = "unknown";
	int count;

	for (count = 0; errors[count].string; count ++)
	{
		if (errors[count].code == code)
		{
			string = errors[count].string;
			break;
		}
	}

	return (string);
}

