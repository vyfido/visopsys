//
//  Visopsys
//  Copyright (C) 1998-2007 J. Andrew McLaughlin
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
//  kernelScsiDiskDriver.h
//
	
#if !defined(_KERNELSCSIDISKDRIVER_H)

#include "kernelBus.h"
#include "kernelUsbDriver.h"
#include "kernelScsiDriver.h"

#define SCSI_MAX_DISKS 16

typedef struct {
  kernelBusType busType;
  int target;
  kernelDevice *dev;
  char vendorId[9];
  char productId[17];
  char vendorProductId[26];
  unsigned numSectors;
  unsigned sectorSize;
  struct {
    usbDevice usbDev;
    usbEndpointDesc *bulkInDesc;
    unsigned char bulkInEndpoint;
    usbEndpointDesc *bulkOutDesc;
    unsigned char bulkOutEndpoint;
    usbEndpointDesc *intrInDesc;
    unsigned char intrInEndpoint;
  } usb;
} kernelScsiDisk;

typedef struct {
  scsiModeParamHeader header;
  unsigned char code;
  unsigned char length;
  unsigned char cylinders[3];
  unsigned char heads;
  unsigned char pad[18];
  
} __attribute__((packed)) scsiDiskGeomPage;

#define _KERNELSCSIDISKDRIVER_H
#endif
