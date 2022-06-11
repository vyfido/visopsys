// 
//  Visopsys
//  Copyright (C) 1998-2007 J. Andrew McLaughlin
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
//  loader.h
//

// This file contains definitions and structures for using the Visopsys
// kernel loader

#if !defined(_LOADER_H)

// Loader File class types
#define LOADERFILECLASS_EMPTY    0x0000
#define LOADERFILECLASS_TEXT     0x1000
#define LOADERFILECLASS_BIN      0x2000
#define LOADERFILECLASS_STATIC   0x0001
#define LOADERFILECLASS_DYNAMIC  0x0002
#define LOADERFILECLASS_EXEC     0x0004
#define LOADERFILECLASS_OBJ      0x0008
#define LOADERFILECLASS_LIB      0x0010
#define LOADERFILECLASS_CORE     0x0020
#define LOADERFILECLASS_IMAGE    0x0040
#define LOADERFILECLASS_DATA     0x0080
#define LOADERFILECLASS_CONFIG   0x0100
#define LOADERFILECLASS_BOOT     0x0200

// Symbol bindings for loader symbol structure
#define LOADERSYMBOLBIND_LOCAL   0
#define LOADERSYMBOLBIND_GLOBAL  1
#define LOADERSYMBOLBIND_WEAK    2

// Symbol types for the loader symbol structure
#define LOADERSYMBOLTYPE_NONE    0
#define LOADERSYMBOLTYPE_OBJECT  1
#define LOADERSYMBOLTYPE_FUNC    2
#define LOADERSYMBOLTYPE_SECTION 3
#define LOADERSYMBOLTYPE_FILE    4

// This structure describes the classification of the file.
typedef struct {
  char className[64];
  int flags;

} loaderFileClass;

typedef struct {
  char *name;
  int defined;
  void *value;
  unsigned size;
  int binding;
  int type;

} loaderSymbol;

typedef struct {
  int numSymbols;
  int tableSize;
  loaderSymbol symbols[];

} loaderSymbolTable;

#define _LOADER_H
#endif
