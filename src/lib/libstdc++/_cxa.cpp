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
//  _cxa.c
//

// This contains standard startup and shutdown code which should be linked to
// all programs made for Visopsys using the C++ language

extern "C" {

#include <stdlib.h>

void *__dso_handle = NULL;

typedef struct {
	void (*function)(void *);
	void *arg;
	void *dso;

} exitDestructor;

static exitDestructor *destructors = NULL;
static int numDestructors = 0;

int __cxa_atexit(void (*)(void *), void *, void *);
void __cxa_pure_virtual();


int __cxa_atexit(void (*function)(void *), void *arg, void *dso)
{
	exitDestructor *newDestructors = NULL;

	newDestructors = (exitDestructor *) realloc(destructors,
		((numDestructors + 1) * sizeof(exitDestructor)));
	if (!newDestructors)
		return (-1);

	destructors = newDestructors;

	destructors[numDestructors].function = function;
	destructors[numDestructors].arg = arg;
	destructors[numDestructors].dso = dso;

	numDestructors += 1;

	return (0);
}


void __cxa_pure_virtual()
{
}


} // extern "C"

