//
//  Visopsys
//  Copyright (C) 1998-2011 J. Andrew McLaughlin
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
//  kernelVariableList.h
//

#if !defined(_KERNELVARIABLELIST_H)

#include <sys/variable.h>
#include <sys/memory.h>

#define VARIABLE_INITIAL_MEMORY    MEMORY_PAGE_SIZE
#define VARIABLE_INITIAL_NUMBER    32
#define VARIABLE_INITIAL_DATASIZE  (VARIABLE_INITIAL_MEMORY -       \
				    (2 * VARIABLE_INITIAL_NUMBER *  \
				     sizeof(char *)))

// Functions exported by kernelVariableList.c
int kernelVariableListCreate(variableList *);
int kernelVariableListDestroy(variableList *);
int kernelVariableListGet(variableList *, const char *, char *, unsigned);
int kernelVariableListSet(variableList *, const char *, const char *);
int kernelVariableListUnset(variableList *, const char *);

#define _KERNELVARIABLELIST_H
#endif
