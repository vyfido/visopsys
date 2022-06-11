//
//  Visopsys
//  Copyright (C) 1998-2005 J. Andrew McLaughlin
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
//  kernelDevice.h
//

// Describes the generic description/classification mechanism for hardware
// devices.

#if !defined(_KERNELDEVICE_H)

#include "kernelDriver.h"

// Hardware device classes and subclasses
#define DEVICECLASS_NONE                    0
#define DEVICECLASS_SYSTEM                  0x0100
#define DEVICECLASS_CPU                     0x0200
#define DEVICECLASS_MEMORY                  0x0300
#define DEVICECLASS_BUS                     0x0400
#define DEVICECLASS_PIC                     0x0500
#define DEVICECLASS_SYSTIMER                0x0600
#define DEVICECLASS_RTC                     0x0700
#define DEVICECLASS_DMA                     0x0800
#define DEVICECLASS_KEYBOARD                0x0900
#define DEVICECLASS_MOUSE                   0x0A00
#define DEVICECLASS_DISK                    0x0B00
#define DEVICECLASS_GRAPHIC                 0x0C00
#define DEVICECLASS_NETWORK                 0x0D00

// Device sub-classes

// Sub-classes of CPUs
#define DEVICESUBCLASS_NONE                 0

#define DEVICESUBCLASS_CPU_X86              (DEVICECLASS_CPU | 0x01)

// Sub-classes of buses
#define DEVICESUBCLASS_BUS_PCI              (DEVICECLASS_BUS | 0x01)

// Sub-classes of mice
#define DEVICESUBCLASS_MOUSE_PS2            (DEVICECLASS_MOUSE | 0x01)
#define DEVICESUBCLASS_MOUSE_SERIAL         (DEVICECLASS_MOUSE | 0x02)

// Sub-classes of disks
#define DEVICESUBCLASS_DISK_FLOPPY          (DEVICECLASS_DISK | 0x01)
#define DEVICESUBCLASS_DISK_IDE             (DEVICECLASS_DISK | 0x02)
#define DEVICESUBCLASS_DISK_SCSI            (DEVICECLASS_DISK | 0x03)

// Sub-classes of graphics adapters
#define DEVICESUBCLASS_GRAPHIC_FRAMEBUFFER  (DEVICECLASS_GRAPHIC | 0x01)

// Sub-classes of network adapters
#define DEVICESUBCLASS_NETWORK_ETHERNET     (DEVICECLASS_NETWORK | 0x01)

// For masking off class/subclass
#define DEVICECLASS_MASK                    0xFF00
#define DEVICESUBCLASS_MASK                 0x00FF

// A structure for device classes and subclasses, which just allows us to
// associate the different types with string names.
typedef struct {
  int class;
  char *name;

} kernelDeviceClass;

// The generic hardware device structure
typedef struct {
  // Device class and subclass.  Subclass optional.
  kernelDeviceClass *class;
  kernelDeviceClass *subClass;
  // Optional, vendor-specific model name
  char *model;
  // Driver
  kernelDriver *driver;
  // Device class-specific structure
  void *dev;
  // Used for our tree
  void *parent;
  void *firstChild;
  void *next;

} kernelDevice;

// Functions exported from kernelDevice.c
int kernelDeviceInitialize(void);
kernelDeviceClass *kernelDeviceGetClass(int);
int kernelDeviceFind(kernelDeviceClass *, kernelDeviceClass *,
		     kernelDevice *[], int);
int kernelDeviceAdd(kernelDevice *, kernelDevice *);

#define _KERNELDEVICE_H
#endif