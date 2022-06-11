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
//  cdefs.h
//

// This is the Visopsys version of the standard header file cdefs.h

#if !defined(_CDEFS_H)

#include <stdarg.h>
#include <sys/types.h>

uquad_t __div64(uquad_t, uquad_t, uquad_t *);
quad_t __divdi3(quad_t, quad_t);
int _xpndfmt(char *, int, const char *, va_list);
int _fmtinpt(const char *, const char *, va_list);
quad_t __moddi3(quad_t, quad_t);
void _num2str(unsigned long long, char *, unsigned, int);
int _numdgts(unsigned long long, unsigned, int);
unsigned long long _str2num(const char *, unsigned, int);
int _syscall(int, int, ...);
uquad_t __udivdi3(uquad_t, uquad_t);
uquad_t __umoddi3(uquad_t, uquad_t);

#define _CDEFS_H
#endif
