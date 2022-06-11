//
//  Visopsys
//  Copyright (C) 1998-2007 J. Andrew McLaughlin
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
//  59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
//
//  kernelEnvironment.h
//

#if !defined(_KERNELENVIRONMENT_H)

#include <sys/variable.h>

// Definitions.

// Functions exported by kernelEnvironment.c
int kernelEnvironmentCreate(int, variableList *, variableList *);
int kernelEnvironmentGet(const char *, char *, unsigned);
int kernelEnvironmentSet(const char *, const char *);
int kernelEnvironmentUnset(const char *);
void kernelEnvironmentDump(void);

#define _KERNELENVIRONMENT_H
#endif
