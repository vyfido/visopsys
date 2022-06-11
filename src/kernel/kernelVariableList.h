//
//  Visopsys
//  Copyright (C) 1998-2003 J. Andrew McLaughlin
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

#include "kernelLock.h"

// Definitions.

typedef struct
{
  unsigned numVariables;
  unsigned maxVariables;
  unsigned usedData;
  unsigned maxData;
  unsigned totalSize;
  char **variables;
  char **values;
  char *data;
  kernelLock listLock;

} kernelVariableList;

// Functions exported by kernelVariableList.c
kernelVariableList *kernelVariableListCreate(unsigned, unsigned, const char *);
int kernelVariableListGet(kernelVariableList *, const char *, char *,
			  unsigned);
int kernelVariableListSet(kernelVariableList *, const char *, const char *);
int kernelVariableListUnset(kernelVariableList *, const char *);

#define _KERNELVARIABLELIST_H
#endif
