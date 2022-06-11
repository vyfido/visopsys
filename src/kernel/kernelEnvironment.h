//
//  Visopsys
//  Copyright (C) 1998-2021 J. Andrew McLaughlin
//
//  This program is free software; you can redistribute it and/or modify it
//  under the terms of the GNU General Public License as published by the Free
//  Software Foundation; either version 2 of the License, or (at your option)
//  any later version.
//
//  This program is distributed in the hope that it will be useful, but
//  WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
//  or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
//  for more details.
//
//  You should have received a copy of the GNU General Public License along
//  with this program; if not, write to the Free Software Foundation, Inc.,
//  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
//
//  kernelEnvironment.h
//

#ifndef _KERNELENVIRONMENT_H
#define _KERNELENVIRONMENT_H

#include <sys/env.h>
#include <sys/vis.h>

// Definitions.

// Functions exported by kernelEnvironment.c
int kernelEnvironmentCreate(int, variableList *, variableList *);
int kernelEnvironmentLoad(const char *, int);
int kernelEnvironmentGet(const char *, char *, unsigned);
int kernelEnvironmentProcessGet(int, const char *, char *, unsigned);
int kernelEnvironmentSet(const char *, const char *);
int kernelEnvironmentProcessSet(int, const char *, const char *);
int kernelEnvironmentUnset(const char *);
int kernelEnvironmentClear(void);
void kernelEnvironmentDump(void);

#endif

