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
//  kernelDriver.h
//

// Describes the generic interface for hardware device drivers.

#if !defined(_KERNELDRIVER_H)

// The generic device driver structure
typedef struct {
  int class;
  int subClass;
  // The registration and detection functions, which all drivers must implement
  void (*driverRegister) (void *);
  int (*driverDetect) (void *);
  // Device class-specific operations
  void *ops;

} kernelDriver;

// An enumeration of driver types
typedef enum {
  extDriver, fatDriver, isoDriver, linuxSwapDriver, ntfsDriver,
  textConsoleDriver, graphicConsoleDriver
} kernelDriverType;

// Structures

// Functions exported by kernelDriver.c
int kernelTextDriversInitialize(void);
int kernelFilesystemDriversInitialize(void);
int kernelDriverRegister(kernelDriverType type, void *);
void *kernelDriverGetExt(void);
void *kernelDriverGetFat(void);
void *kernelDriverGetIso(void);
void *kernelDriverGetLinuxSwap(void);
void *kernelDriverGetNtfs(void);
void *kernelDriverGetTextConsole(void);
void *kernelDriverGetGraphicConsole(void);

// Registration routines for our built-in drivers
void kernelCpuDriverRegister(void *);
void kernelMemoryDriverRegister(void *);
void kernelPicDriverRegister(void *);
void kernelSysTimerDriverRegister(void *);
void kernelRtcDriverRegister(void *);
void kernelDmaDriverRegister(void *);
void kernelKeyboardDriverRegister(void *);
void kernelFloppyDriverRegister(void *);
void kernelIdeDriverRegister(void *);
void kernelFramebufferGraphicDriverRegister(void *);
void kernelPS2MouseDriverRegister(void *);
void kernelPciDriverRegister(void *);
void kernelLanceDriverRegister(void *);

#define _KERNELDRIVER_H
#endif
