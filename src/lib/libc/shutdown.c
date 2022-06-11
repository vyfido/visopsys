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
//  shutdown.c
//

// This is the standard "shutdown" function, as found in standard C libraries

#include <sys/socket.h>
#include <errno.h>
#include <sys/api.h>
#include <sys/cdefs.h>


int shutdown(int sockFd, int how __attribute__((unused)))
{
	// Given a file descriptor, close the socket and free the file descriptor.

	int status = 0;
	fileDescType type = filedesc_unknown;
	void *data = NULL;

	if (visopsys_in_kernel)
	{
		errno = ERR_BUG;
		return (status = -1);
	}

	// Look up the file descriptor
	status = _fdget(sockFd, &type, &data);
	if (status < 0)
	{
		errno = status;
		return (status = -1);
	}

	if (data)
	{
		switch (type)
		{
			case filedesc_socket:
				status = networkClose(data);
				break;

			default:
				status = ERR_NOTIMPLEMENTED;
				break;
		}

		if (status < 0)
		{
			errno = status;
			return (status = -1);
		}
	}

	// Free the file descriptor
	_fdfree(sockFd);

	return (status = 0);
}

