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
//  kernelProcessorFunctions.h
//

#if !defined(_KERNELPROCESSORFUNCTIONS_H)

// Error messages
#define NULL_PROC_MESS "The processor object passed or referenced was NULL"
#define NULL_PROC_DRIVER_MESS "The processor driver passed or referenced was NULL"
#define NULL_PROC_FUNC_MESS "The processor driver function requested was NULL"

// A structure for holding pointers to the processor driver functions
typedef struct
{

  int (*driverInitialize) (void);
  unsigned long *(*driverReadTimestamp) (void);

} kernelProcessorDriver;

// A structure for holding information about the processor object
typedef struct
{

  kernelProcessorDriver *deviceDriver;

} kernelProcessorObject;


// Functions exported by kernelProcessorFunctions.c
int kernelProcessorRegisterDevice(kernelProcessorObject *);
int kernelProcessorInstallDriver(kernelProcessorDriver *);
int kernelProcessorInitialize(void);
unsigned long *kernelProcessorReadTimestamp(void);

// Functions simulated with preprocessor macros

#define kernelProcessorGetCR3(variable)       \
  __asm__ __volatile__ ("movl %%cr3, %0 \n\t" \
                        : "=r" (variable) : : "memory")

#define kernelProcessorSetCR3(variable)       \
  __asm__ __volatile__ ("movl %0, %%cr3 \n\t" \
                        : : "r" (variable))

#define kernelProcessorGetEFlags(variable) \
  __asm__ __volatile__ ("pushfl \n\t"      \
                        "popl %0 \n\t"     \
                        : "=r" (variable) : : "memory")

#define kernelProcessorSetEFlags(variable) \
  __asm__ __volatile__ ("pushl %0 \n\t"    \
                        "popfl \n\t"       \
                        : : "r" (variable))

#define kernelProcessorLoadTaskReg(selector) \
  __asm__ __volatile__ ("pushfl \n\t"        \
                        "cli \n\t"           \
                        "ltr %%ax \n\t"      \
                        "popfl \n\t"         \
                        : : "a" (selector))

#define kernelProcessorIntReturn() \
  __asm__ __volatile__ ("iret \n\t")

#define _KERNELPROCESSORFUNCTIONS_H
#endif
