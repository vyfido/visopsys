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
//  kernelInterruptsInit.h
//
	
// This is the header file to go with the kernel's descriptor manager

#if !defined(_KERNELINTERRUPTSINIT_H)

// Here is a list of all of the external interrupt handlers we will be
// installing into the descriptor table
extern void kernelExceptionHandler0(void);
extern void kernelExceptionHandler1(void);
extern void kernelExceptionHandler2(void);
extern void kernelExceptionHandler3(void);
extern void kernelExceptionHandler4(void);
extern void kernelExceptionHandler5(void);
extern void kernelExceptionHandler6(void);
extern void kernelExceptionHandler7(void);
extern void kernelExceptionHandler8(void);
extern void kernelExceptionHandler9(void);
extern void kernelExceptionHandlerA(void);
extern void kernelExceptionHandlerB(void);
extern void kernelExceptionHandlerC(void);
extern void kernelExceptionHandlerD(void);
extern void kernelExceptionHandlerE(void);
extern void kernelExceptionHandlerF(void);
extern void kernelExceptionHandler10(void);
extern void kernelExceptionHandler11(void);
extern void kernelExceptionHandler12(void);
extern void kernelInterruptHandler20(void);
extern void kernelInterruptHandler21(void);
extern void kernelInterruptHandler25(void);
extern void kernelInterruptHandler26(void);
extern void kernelInterruptHandler27(void);
extern void kernelInterruptHandler28(void);
extern void kernelInterruptHandler29(void);
extern void kernelInterruptHandler2A(void);
extern void kernelInterruptHandler2B(void);
extern void kernelInterruptHandler2C(void);
extern void kernelInterruptHandler2D(void);
extern void kernelInterruptHandler2E(void);
extern void kernelInterruptHandler2F(void);
extern void kernelInterruptHandlerUnimp(void);


// The list of functions exported from kernelInterruptsInit.c
int kernelInterruptVectorsInstall(void);

#define _KERNELINTERRUPTSINIT_H
#endif
