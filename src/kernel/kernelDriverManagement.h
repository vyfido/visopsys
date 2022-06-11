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
//  kernelDriverManagement.h
//

// Prototypes for the assembler routines associated with the 
// default device drivers.  The names are pretty much self-explanatory.

#if !defined(_KERNELDRIVERMANAGEMENT_H)

#include "kernelSysTimer.h"
#include "kernelRtc.h"
#include "kernelPic.h"
#include "kernelSerial.h"
#include "kernelDma.h"
#include "kernelKeyboard.h"
#include "kernelMouse.h"
#include "kernelDisk.h"
#include "kernelGraphic.h"
#include "kernelFilesystem.h"
#include "kernelText.h"


// An enumeration of driver types
typedef enum {
  picDriver, sysTimerDriver, rtcDriver, dmaDriver, keyboardDriver,
  mouseDriver, floppyDriver, ideDriver, graphicDriver, fatDriver,
  textConsoleDriver, graphicConsoleDriver

} kernelDriverType;

// Structures

typedef struct
{
  // This structure contains a vector for each
  // possible kind of driver
  
  kernelPicDriver *picDriver;
  kernelSysTimerDriver *sysTimerDriver;
  kernelRtcDriver *rtcDriver;
  kernelSerialDriver *serialDriver;
  kernelDmaDriver *dmaDriver;
  kernelKeyboardDriver *keyboardDriver;
  kernelMouseDriver *mouseDriver;
  kernelDiskDriver *floppyDriver;
  kernelDiskDriver *ideDriver;
  kernelGraphicDriver *graphicDriver;
  kernelFilesystemDriver *fatDriver;
  kernelTextOutputDriver *textConsoleDriver;
  kernelTextOutputDriver *graphicConsoleDriver;

} kernelDriverManager;

// Functions exported by kernelDriverManagement.c
int kernelDriversInitialize(void);
int kernelDriverRegister(kernelDriverType type, void *);
void kernelInstallPicDriver(kernelPic *);
void kernelInstallSysTimerDriver(kernelSysTimer *);
void kernelInstallRtcDriver(kernelRtc *);
void kernelInstallDmaDriver(kernelDma *);
void kernelInstallKeyboardDriver(kernelKeyboard *);
void kernelInstallMouseDriver(kernelMouse *);
void kernelInstallFloppyDriver(kernelPhysicalDisk *);
void kernelInstallHardDiskDriver(kernelPhysicalDisk *);
void kernelInstallGraphicDriver(kernelGraphicAdapter *);
kernelFilesystemDriver *kernelDriverGetFat(void);
kernelTextOutputDriver *kernelDriverGetTextConsole(void);
kernelTextOutputDriver *kernelDriverGetGraphicConsole(void);

#define _KERNELDRIVERMANAGEMENT_H
#endif
