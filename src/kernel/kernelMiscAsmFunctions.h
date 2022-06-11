//
//  Visopsys
//  Copyright (C) 1998-2001 J. Andrew McLaughlin
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
//  kernelMiscAsmFunctions.h
//
	
#if !defined(_KERNELMISCASMFUNCTIONS_H)

// Functions exported by kernelMiscAsmFunctions.s
void kernelSuddenStop(void);
void kernelSuddenReboot(void);
void kernelMemCopy(const void *, void *, unsigned int);
void kernelMemClear(void *, unsigned int);
void kernelInstallGDT(void *, unsigned int);
void kernelInstallIDT(void *, unsigned int);
void kernelTaskJump(unsigned int);
void kernelTaskCall(unsigned int);

#define _KERNELMISCASMFUNCTIONS_H
#endif
