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
//  pthread_join.c
//

// This is the "pthread_join" function as found in POSIX thread libraries

#include <pthread.h>
#include <sys/api.h>


int pthread_join(pthread_t thread, void **exitCode)
{
	// Excerpted from the POSIX programming manual:
	//
	// The pthread_join() function shall suspend execution of the calling
	// thread until the target thread terminates, unless the target thread has
	// already terminated.  On return from a successful pthread_join() call
	// with a non-NULL exitCode argument, the value passed to pthread_exit()
	// by the terminating thread shall be made available in the location
	// referenced by value_ptr.  When a pthread_join() returns successfully,
	// the target thread has been terminated.
	//
	// If successful, the pthread_join() function shall return zero;
	// otherwise, an error number shall be returned to indicate the error
	//
	// The pthread_join() function shall fail if:
	//
	// EINVAL  The implementation has detected that the value specified by
	//	thread does not refer to a joinable thread
	//
	// ESRCH  No thread could be found corresponding to that specified by the
	//	given thread ID
	//
	// The pthread_join() function may fail if:
	//
	// EDEADLK  A deadlock was detected or the value of thread specifies the
	//	calling thread

	int status = 0;

	if (multitaskerProcessIsAlive(thread))
		status = multitaskerBlock(thread);

	if (exitCode)
		*exitCode = (void *) status;

	return (0);
}

