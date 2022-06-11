//
//  Visopsys
//  Copyright (C) 1998-2003 J. Andrew McLaughlin
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

// Definitions
#define PAGEPRESENT_BIT   0x00000001
#define WRITABLE_BIT      0x00000002
#define USER_BIT          0x00000004
#define WRITETHROUGH_BIT  0x00000008
#define CACHEDISABLE_BIT  0x00000010
#define GLOBAL_BIT        0x00000100

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

// Functions simulated with preprocessor macros

#define kernelProcessorClearAddressCache(address) \
  __asm__ __volatile__ ("invlpg %0 \n\t" \
                        : : "m" (*((char *) address)))

#define kernelProcessorGetCR3(variable)       \
  __asm__ __volatile__ ("movl %%cr3, %0 \n\t" \
                        : "=r" (variable) : : "memory")

#define kernelProcessorSetCR3(variable)       \
  __asm__ __volatile__ ("movl %0, %%cr3 \n\t" \
                        : : "r" (variable))

#define kernelProcessorIntReturn() \
  __asm__ __volatile__ ("iret \n\t")

#define kernelProcessorLoadTaskReg(selector) \
  __asm__ __volatile__ ("pushfl \n\t"        \
                        "cli \n\t"           \
                        "ltr %%ax \n\t"      \
                        "popfl \n\t"         \
                        : : "a" (selector))

#define kernelProcessorInPort8(port, data)  \
  __asm__ __volatile__ ("inb %%dx, %%al \n\t" \
                        : "=a" (data) : "d" (port) : "memory")

#define kernelProcessorOutPort8(port, data)  \
  __asm__ __volatile__ ("outb %%al, %%dx \n\t" \
                        : : "a" (data), "d" (port))

#define _KERNELPROCESSORFUNCTIONS_H
#endif
