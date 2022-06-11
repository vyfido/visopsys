// 
//  Visopsys
//  Copyright (C) 1998-2003 J. Andrew McLaughlin
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
//  signal.h
//

// This is the Visopsys version of the standard header file signal.h

#if !defined(_SIGNAL_H)

#include <stddef.h>

// Compatible with solaris codes
#define SIGABRT   6
#define SIGFPE    8
#define SIGILL    4
#define SIGINT    2
#define SIGSEGV   11
#define SIGTERM   15
#define SIG_DFL   (void(*)())0
#define SIG_ERR   (void(*)())0
#define SIG_IGN   (void(*)())0

int raise(int);
void (*signal(int, void (*)(int)))(int); 

#define _SIGNAL_H
#endif
