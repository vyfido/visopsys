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
//  pthread_cancel.c
//

// This is the "pthread_cancel" function as found in POSIX thread libraries

#include <pthread.h>
#include <sys/api.h>


int pthread_cancel(pthread_t thread)
{
	// Excerpted from the POSIX programming manual:
	//
	// The pthread_cancel() function shall request that thread be canceled.
	// The cancellation processing in the target thread shall run
	// asynchronously with respect to the calling thread returning from
	// pthread_cancel().
	//
	// If successful, the pthread_cancel() function shall return zero;
	// otherwise, an error number shall be returned to indicate the error
	//
	// The pthread_cancel() function may fail if:
	//
	// ESRCH  No thread could be found corresponding to that specified by the
	//	given thread ID

	return (multitaskerKillProcess(thread));
}

