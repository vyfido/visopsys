//
//  Visopsys
//  Copyright (C) 1998-2011 J. Andrew McLaughlin
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
//  kernelIdeDriver.h
//

// This header file contains definitions for the kernel's standard IDE/
// ATA/ATAPI driver

#if !defined(_KERNELIDEDRIVER_H)

#include "kernelDisk.h"
#include "kernelLock.h"

#define IDE_MAX_DISKS        4
#define IDE_MAX_CONTROLLERS  (DISK_MAXDEVICES / IDE_MAX_DISKS)

// IDE feature flags.  These don't represent all possible features; just the
// ones we [plan to] support.
#define IDE_FEATURE_48BIT    0x80
#define IDE_FEATURE_MEDSTAT  0x40
#define IDE_FEATURE_WCACHE   0x20
#define IDE_FEATURE_RCACHE   0x10
#define IDE_FEATURE_SMART    0x08
#define IDE_FEATURE_DMA      (IDE_FEATURE_UDMA | IDE_FEATURE_MWDMA)
#define IDE_FEATURE_UDMA     0x04
#define IDE_FEATURE_MWDMA    0x02
#define IDE_FEATURE_MULTI    0x01

// IDE transfer modes.
#define IDE_TRANSMODE_UDMA6  0x46
#define IDE_TRANSMODE_UDMA5  0x45
#define IDE_TRANSMODE_UDMA4  0x44
#define IDE_TRANSMODE_UDMA3  0x43
#define IDE_TRANSMODE_UDMA2  0x42
#define IDE_TRANSMODE_UDMA1  0x41
#define IDE_TRANSMODE_UDMA0  0x40
#define IDE_TRANSMODE_DMA2   0x22
#define IDE_TRANSMODE_DMA1   0x21
#define IDE_TRANSMODE_DMA0   0x20
#define IDE_TRANSMODE_PIO    0x00

// Status register bits
#define IDE_CTRL_BSY         0x80
#define IDE_DRV_RDY          0x40
#define IDE_DRV_WRTFLT       0x20
#define IDE_DRV_SKCOMP       0x10
#define IDE_DRV_DRQ          0x08
#define IDE_DRV_CORDAT       0x04
#define IDE_DRV_IDX          0x02
#define IDE_DRV_ERR          0x01

// Error codes
#define IDE_ADDRESSMARK      0
#define IDE_CYLINDER0        1
#define IDE_INVALIDCOMMAND   2
#define IDE_MEDIAREQ         3
#define IDE_SECTNOTFOUND     4
#define IDE_MEDIACHANGED     5
#define IDE_BADDATA          6
#define IDE_BADSECTOR        7
#define IDE_UNKNOWN          8
#define IDE_TIMEOUT          9

typedef struct {
  char *name;
  unsigned char val;
  unsigned char identByte;
  unsigned short suppMask;
  unsigned short enabledMask;
  int featureFlag;

} ideDmaMode;

typedef struct {
  char *name;
  unsigned char suppByte;
  unsigned short suppMask;
  unsigned char featureCode;
  unsigned char enabledByte;
  unsigned short enabledMask;
  int featureFlag;

} ideFeature;

typedef struct {
  int featureFlags;
  int packetMaster;
  char *dmaMode;
  kernelPhysicalDisk physical;

} ideDisk;

typedef struct {
  unsigned data;
  unsigned featErr;
  unsigned sectorCount;
  unsigned lbaLow;
  unsigned lbaMid;
  unsigned lbaHigh;
  unsigned device;
  unsigned comStat;
  unsigned altComStat;

} idePorts;

typedef volatile struct {
  void *physicalAddress;
  unsigned short count;
  unsigned short EOT;

} __attribute__((packed)) idePrd;

typedef volatile struct {
  unsigned char error;
  unsigned char segNum;
  unsigned char senseKey;
  unsigned info;
  unsigned char addlLength;
  unsigned commandSpecInfo;
  unsigned char addlSenseCode;
  unsigned char addlSenseCodeQual;
  unsigned char unitCode;
  unsigned char senseKeySpec[3];
  unsigned char addlSenseBytes[];

} __attribute__((packed)) ideSenseData;

typedef volatile struct {
  idePorts ports;
  int interrupt;
  unsigned char intStatus;
  ideDisk disk[2];
  idePrd *prd;
  void *prdPhysical;
  int prdEntries;
  int expectInterrupt;
  int gotInterrupt;
  int ints, acks;
  lock lock;

} ideChannel;

typedef volatile struct {
  ideChannel channel[2];
  int busMaster;
  int pciInterrupt;
  unsigned busMasterIo;

} ideController;

#define _KERNELIDEDRIVER_H
#endif
