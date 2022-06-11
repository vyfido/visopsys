// 
//  Visopsys
//  Copyright (C) 1998-2005 J. Andrew McLaughlin
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
//  file.h
//

// This file contains definitions and structures for using and manipulating
// files in Visopsys.

#if !defined(_FILE_H)

// File open modes
#define OPENMODE_READ        0x01
#define OPENMODE_WRITE       0x02
#define OPENMODE_CREATE      0x04
#define OPENMODE_TRUNCATE    0x08
#define OPENMODE_READWRITE   (OPENMODE_READ | OPENMODE_WRITE)

// Pathname limits
#define MAX_NAME_LENGTH      512
#define MAX_PATH_LENGTH      512
#define MAX_PATH_NAME_LENGTH (MAX_PATH_LENGTH + MAX_NAME_LENGTH)

// Typedef a file handle
typedef void* fileHandle;

typedef enum
{
  fileT, dirT, linkT, volT, unknownT
} fileType;

// This is the structure used to store universal information about a file
typedef struct
{
  fileHandle handle;
  char name[MAX_NAME_LENGTH];
  fileType type;
  char filesystem[MAX_PATH_LENGTH];
  unsigned creationDate;
  unsigned creationTime;
  unsigned accessedTime;
  unsigned accessedDate;
  unsigned modifiedDate;
  unsigned modifiedTime;
  unsigned size;
  unsigned blocks;
  unsigned blockSize;
  int openMode;

} file;

#define _FILE_H
#endif
