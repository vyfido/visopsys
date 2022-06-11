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
//  pthread_attr_destroy.c
//

// This is the "pthread_attr_destroy" function as found in POSIX thread
// libraries

#include <pthread.h>
#include <errno.h>
#include <string.h>


int pthread_attr_destroy(pthread_attr_t *attr)
{
	// Excerpted from the POSIX programming manual:
	//
	// The pthread_attr_destroy() function shall destroy a thread attributes
	// object.  An implementation may cause pthread_attr_destroy() to set attr
	// to an implementation-defined invalid value. A destroyed attr attributes
	// object can be reinitialized using pthread_attr_init(); the results of
	// otherwise referencing the object after it has been destroyed are
	// undefined.
	//
	// Upon successful completion, pthread_attr_destroy() shall return a value
	// of 0; otherwise, an error number shall be returned to indicate the
	// error

	// Check params
	if (!attr)
		return (errno = ERR_NULLPARAMETER);

	memset(attr, 0, sizeof(pthread_attr_t));

	return (0);
}

