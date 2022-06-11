// 
//  Visopsys
//  Copyright (C) 1998-2004 J. Andrew McLaughlin
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
//  stdarg.h
//

// This is the Visopsys version of the standard header file stdarg.h

#if !defined(_STDDEF_H)

typedef int ptrdiff_t;
typedef unsigned int size_t;

#if !defined(__cplusplus)
typedef int wchar_t;
#endif

#ifndef NULL
#define NULL 0
#endif

#define offsetof(str, mbr) (&(str.mbr) - &str)

#define _STDDEF_H
#endif
