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
//  pthread_self.c
//

// This is the "pthread_self" function as found in POSIX thread libraries

#include <pthread.h>
#include <sys/api.h>


pthread_t pthread_self(void)
{
	// Excerpted from the POSIX programming manual:
	//
	// The pthread_self() function shall return the thread ID of the calling
	// thread.  No errors are defined.

	return (multitaskerGetCurrentProcessId());
}

