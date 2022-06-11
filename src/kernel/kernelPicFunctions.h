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
//  kernelPicFunctions.h
//

#if !defined(_KERNELPICFUNCTIONS_H)

// Structures for the PIC device

typedef struct
{
  int (*driverInitialize) (void);
  void (*driverEndOfInterrupt) (int);

} kernelPicDeviceDriver;

typedef struct 
{
  kernelPicDeviceDriver *deviceDriver;

} kernelPicObject;

// Functions exported by kernelPicFunctions.c
int kernelPicRegisterDevice(kernelPicObject *);
int kernelPicInstallDriver(kernelPicDeviceDriver *);
int kernelPicInitialize(void);
int kernelPicEndOfInterrupt(int);

// Functions simulated with preprocessor macros
#define kernelPicEnableInterrupts() __asm__ __volatile__ ("sti\n")
#define kernelPicDisableInterrupts() __asm__ __volatile__ ("cli\n")
#define kernelPicInterruptStatus(variable) \
   __asm__ __volatile__ ("pushfl \n\t" \
                         "popl %0 \n\t" \
                         "shr $9, %0 \n\t" \
                         "and $1, %0 \n\t" \
                         : "=r" (variable) : : "memory")

#define _KERNELPICFUNCTIONS_H
#endif
