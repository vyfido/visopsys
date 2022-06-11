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
//  kernelDmaFunctions.h
//

// These are the generic functions for DMA access.  These are above the
// level of the actual DMA driver (which will probably be external, and/or
// an assembly-language function

#if !defined(_KERNELDMAFUNCTIONS_H)

// Definitions.  

// These are 8-bit bitwise numbers sent to the controller's
// mode registers.
#define DEMANDMODE 0x00
#define SINGLEMODE 0x40
#define BLOCKMODE 0x80
#define AUTOINITMODE 0x10
#define INCRMODE 0x00
#define DECRMODE 0x20
#define READMODE 0x08
#define WRITEMODE 0x04

// Error messages

#define NULL_DMA_OBJECT "Attempt to register or use a DMA controller object which is NULL"
#define NULL_DMA_DRIVER "Attempt to issue a call to a DMA driver which has not been installed."
#define NO_INITIALIZE_FN "The installed DMA driver has a NULL initialize function"
#define NO_SETUPCHANNEL_FN "The installed DMA driver has a NULL setup-channel function"
#define NO_READDATA_FN "The installed DMA driver has a NULL read-data function"
#define NO_WRITEDATA_FN "The installed DMA driver has a NULL write-data function"
#define NO_CLOSECHANNEL_FN "The installed DMA driver has a NULL close-channel function"
#define NO_ENABLECHANNEL_FN "The installed DMA driver has a NULL enable-channel function"
#define INITIALIZATION_ERROR "The driver returned this error status when initialization was attempted."
#define CHANNELSETUP_ERROR "The driver returned this error status when setting up DMA channel."
#define DMAREAD_ERROR "The driver returned this error status when performing a DMA read."
#define DMAWRITE_ERROR "The driver returned this error status when performing a DMA write."
#define CHANNELCLOSE_ERROR "The driver returned this error status when closing a DMA channel."
#define CHANNELENABLE_ERROR "The driver returned this error status when enabling a DMA channel."

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
