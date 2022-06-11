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
//  kernelDmaFunctions.h
//

// These are the generic functions for DMA access.  These are above the
// level of the actual DMA driver (which will probably be external, and/or
// an assembly-language function

#if !defined(_KERNELDMAFUNCTIONS_H)

// Definitions.  

// These are 8-bit bitwise numbers sent to the controller's
// mode registers.
#define READMODE 0x08
#define WRITEMODE 0x04

// A structure used to register a DMA driver.

typedef struct
{
  int (*driverInitialize) (void);
  int (*driverSetupChannel) (int, int, int, int);
  int (*driverSetMode) (int, int);
  int (*driverEnableChannel) (int);
  int (*driverCloseChannel) (int);

} kernelDmaDeviceDriver;

// A structure to represent the DMA controller array

typedef struct
{
  kernelDmaDeviceDriver *deviceDriver;

} kernelDmaObject;

// Functions exported from kernelDmaFunctions.c
int kernelDmaRegisterDevice(kernelDmaObject *);
int kernelDmaInitialize(void);
int kernelDmaFunctionsInstallDriver(kernelDmaDeviceDriver *);
int kernelDmaFunctionsSetupChannel(int, void *, int);
int kernelDmaFunctionsReadData(int, int);
int kernelDmaFunctionsWriteData(int, int);
int kernelDmaFunctionsEnableChannel(int);
int kernelDmaFunctionsCloseChannel(int);

#define _KERNELDMAFUNCTIONS_H
#endif
