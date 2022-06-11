//
//  Visopsys
//  Copyright (C) 1998-2006 J. Andrew McLaughlin
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
//  kernelRandom.h
//

#if !defined(_KERNELRANDOM_H)

// Functions exported by kernelRandom.c
int kernelRandomInitialize(void);
unsigned kernelRandomUnformatted(void);
unsigned kernelRandomFormatted(unsigned, unsigned);
unsigned kernelRandomSeededUnformatted(unsigned);
unsigned kernelRandomSeededFormatted(unsigned, unsigned, unsigned);

#define _KERNELRANDOM_H
#endif
