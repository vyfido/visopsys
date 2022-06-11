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
//  libvis.h
//

// This file contains definitions and structures for general library
// functionality shared between the kernel and userspace in Visopsys.

#ifndef _LIBVIS_H
#define _LIBVIS_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/api.h>
#include <sys/lock.h>
#include <sys/vis.h>

#ifndef _X_
#define _X_
#endif

#if defined(DEBUG)
#define debug(message, arg...) do { \
	if (visopsys_in_kernel) { \
		if (kernLibOps.debug) \
			kernLibOps.debug(__FILE__, __FUNCTION__, __LINE__, \
				debug_misc, message, ##arg); \
	} else { \
		printf("DEBUG %s:%s(%d): ", __FILE__, __FUNCTION__, __LINE__); \
		printf(message, ##arg); \
		printf("\n"); \
	} } while (0)
#else
	#define debug(message, arg...) do { } while (0)
#endif // defined(DEBUG)

#define error(message, arg...) do { \
	if (visopsys_in_kernel) { \
		kernLibOps.error(__FILE__, __FUNCTION__, __LINE__, kernel_error, \
			message, ##arg); \
	} else { \
		printf("Error: %s:%s(%d): ", __FILE__, __FUNCTION__, __LINE__); \
		printf(message, ##arg); \
		printf("\n"); \
	} } while (0)


static inline int lock_get(spinLock *lock)
{
	if (visopsys_in_kernel)
		return (kernLibOps.lockGet(lock));
	else
		return (lockGet(lock));
}


static inline void lock_release(spinLock *lock)
{
	if (visopsys_in_kernel)
		kernLibOps.lockRelease(lock);
	else
		lockRelease(lock);
}


static inline void memory_free(void *ptr)
{
	debug("Free dynamic memory block");
	if (visopsys_in_kernel)
		kernLibOps._free(ptr, __FUNCTION__);
	else
		free(ptr);
}


static inline void *memory_get(unsigned size, const char *desc)
{
	debug("Request memory block of size %u", size);
	if (visopsys_in_kernel)
		return (kernLibOps.memoryGet(size, desc));
	else
		return (memoryGet(size, desc));
}


static inline void *memory_get_system(unsigned size, const char *desc)
{
	debug("Request system memory block of size %u", size);
	if (visopsys_in_kernel)
		return (kernLibOps.memoryGetSystem(size, desc));
	else
		return (NULL); // Invalid
}


static inline void *memory_malloc(size_t size)
{
	debug("Request dynamic memory block of size %u", size);
	if (visopsys_in_kernel)
		return (kernLibOps._malloc(size, __FUNCTION__));
	else
		return (malloc(size));
}


static inline int memory_release(void *start)
{
	debug("Release memory block at %p", start);
	if (visopsys_in_kernel)
		return (kernLibOps.memoryRelease(start));
	else
		return (memoryRelease(start));
}


static inline int memory_release_system(void *start)
{
	debug("Release system memory block at %p", start);
	if (visopsys_in_kernel)
		return (kernLibOps.memoryReleaseSystem(start));
	else
		return (memoryRelease(start)); // Will fail, with appropriate errors
}


static inline int process_id(void)
{
	if (visopsys_in_kernel)
		return (kernLibOps.multitaskerGetCurrentProcessId());
	else
		return (multitaskerGetCurrentProcessId());
}

#endif

