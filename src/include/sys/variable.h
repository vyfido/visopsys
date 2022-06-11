// 
//  Visopsys
//  Copyright (C) 1998-2011 J. Andrew McLaughlin
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
//  Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
//
//  variable.h
//

// This file contains definitions and structures for using variable lists
// in Visopsys.

#if !defined(_VARIABLE_H)

#include <sys/lock.h>

typedef struct {
  int numVariables;
  int maxVariables;
  int usedData;
  int maxData;
  void *memory;
  unsigned memorySize;
  char **variables;
  char **values;
  char *data;
  lock listLock;

} variableList;

#define _VARIABLE_H
#endif
