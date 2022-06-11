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
//  pthread_create.c
//

// This is the "pthread_create" function as found in POSIX thread libraries

#include <pthread.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/api.h>


static void startThreadWrapper(int argc, void *argv[])
{
	int status = 0;
	void *(*start)(void *) = NULL;
	void *arg = NULL;

	if (argc < 2)
	{
		status = errno = ERR_ARGUMENTCOUNT;
		goto out;
	}

	// Convert the start string to a pointer
	start = (void *)((unsigned long) xtoll((char *) argv[1]));

	if (argc > 2)
	{
		// Convert the argument string to a pointer
		arg = (void *)((unsigned long) xtoll((char *) argv[2]));
	}

	status = (int) start(arg);

out:
	pthread_exit((void *) status);
}


int pthread_create(pthread_t *thread, const pthread_attr_t *attrs,
	void *(*start)(void *), void *arg)
{
	// Excerpted from the POSIX programming manual:
	//
	// The pthread_create() function shall create a new thread, with
	// attributes specified by attr, within a process. If attr is NULL, the
	// default attributes shall be used.  If the attributes specified by attr
	// are modified later, the thread's attributes shall not be affected.
	// Upon successful completion, pthread_create() shall store the ID of the
	// created thread in the location referenced by thread.
	//
	// If successful, the pthread_create() function shall return zero;
	// otherwise, an error number shall be returned to indicate the error
	//
	// The pthread_create() function shall fail if:
	//
	// EAGAIN  The system lacked the necessary resources to create another
	//	thread, or the system-imposed limit on the total number of threads in
	//	a process {PTHREAD_THREADS_MAX} would be exceeded
	//
	// EINVAL  The value specified by attr is invalid
	//
	// EPERM  The caller does not have appropriate permission to set the
	//	required scheduling parameters or scheduling policy

	char startPtrString[(sizeof(void *) * 2) + 3];
	char argPtrString[(sizeof(void *) * 2) + 3];
	int processId = 0;

	// Check params.  'attrs' and 'arg' are allowed to be NULL
	if (!thread || !start)
	{
		errno = ERR_NULLPARAMETER;
		return (processId = errno);
	}

	// Convert the start pointer to a string
	lltoux((unsigned long) start, startPtrString);

	if (arg)
	{
		lltoux((unsigned long) arg, argPtrString);

		processId = multitaskerSpawn(startThreadWrapper, "posix thread",
			2 /* argc */, (void *[]){ startPtrString, argPtrString },
			1 /* run */);
	}
	else
	{
		processId = multitaskerSpawn(startThreadWrapper, "posix thread",
			1 /* argc */, (void *[]){ startPtrString }, 1 /* run */);
	}

	if (processId < 0)
		return (errno = processId);

	if (attrs)
	{ }

	*thread = processId;

	return (0);
}

