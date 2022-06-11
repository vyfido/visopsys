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
//  kernelIdeDriver.h
//

// This header file contains definitions for the kernel's standard IDE/
// ATA/ATAPI driver

#if !defined(_KERNELIDEDRIVER_H)

#include "kernelLock.h"

#define IDE_MAX_DISKS        4

// IDE feature flags.  These don't represent all possible features; just the
// ones we [plan to] support.
#define IDE_FEATURE_48BIT    0x40
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
#define IDE_TRANSMODE_PIO    0x01

// Status register bits
#define IDE_CTRL_BSY         0x80
#define IDE_DRV_RDY          0x40
#define IDE_DRV_WRTFLT       0x20
#define IDE_DRV_SKCOMP       0x10
#define IDE_DRV_DRQ          0x08
#define IDE_DRV_CORDAT       0x04
#define IDE_DRV_IDX          0x02
#define IDE_DRV_ERR          0x01

// ATA commands
#define ATA_NOP              0x00
#define ATA_ATAPIRESET       0x08
#define ATA_RECALIBRATE      0x10
#define ATA_READSECTS        0x20
#define ATA_READECC          0x22
#define ATA_READSECTS_EXT    0x24
#define ATA_READDMA_EXT      0x25
#define ATA_READMULTI_EXT    0x29
#define ATA_WRITESECTS       0x30
#define ATA_WRITEECC         0x32
#define ATA_WRITESECTS_EXT   0x34
#define ATA_WRITEDMA_EXT     0x35
#define ATA_WRITEMULTI_EXT   0x39
#define ATA_VERIFYMULTI      0x40
#define ATA_FORMATTRACK      0x50
#define ATA_SEEK             0x70
#define ATA_DIAG             0x90
#define ATA_INITPARAMS       0x91
#define ATA_ATAPIPACKET      0xA0
#define ATA_ATAPIIDENTIFY    0xA1
#define ATA_ATAPISERVICE     0xA2
#define ATA_READMULTI        0xC4
#define ATA_WRITEMULTI       0xC5
#define ATA_SETMULTIMODE     0xC6
#define ATA_READDMA          0xC8
#define ATA_WRITEDMA         0xCA
#define ATA_IDENTIFY         0xEC
#define ATA_SETFEATURES      0xEF

// ATAPI commands
#define ATAPI_TESTREADY      0x00
#define ATAPI_REQESTSENSE    0x03
#define ATAPI_INQUIRY        0x12
#define ATAPI_STARTSTOP      0x1B
#define ATAPI_PERMITREMOVAL  0x1E
#define ATAPI_READCAPACITY   0x25
#define ATAPI_READ10         0x28
#define ATAPI_SEEK           0x2B
#define ATAPI_READSUBCHAN    0x42
#define ATAPI_READTOC        0x43
#define ATAPI_READHEADER     0x44
#define ATAPI_PLAYAUDIO      0x45
#define ATAPI_PLAYAUDIOMSF   0x47
#define ATAPI_PAUSERESUME    0x4B
#define ATAPI_STOPPLAYSCAN   0x4E
#define ATAPI_MODESELECT     0x55
#define ATAPI_MODESENSE      0x5A
#define ATAPI_LOADUNLOAD     0xA6
#define ATAPI_READ12         0xA8
#define ATAPI_SCAN           0xBA
#define ATAPI_SETCDSPEED     0xBB
#define ATAPI_PLAYCD         0xBC
#define ATAPI_MECHSTATUS     0xBD
#define ATAPI_READCD         0xBE
#define ATAPI_READCDMSF      0xB9

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
  int identByte;
  unsigned short supportedMask;
  unsigned short enabledMask;
  int feature;

} ideDmaMode;

typedef struct {
  int featureFlags;
  char *dmaMode;

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
  idePorts ports;
  int interrupt;
  ideDisk disk[2];
  idePrd *prd;
  void *prdPhysical;
  int prdEntries;
  int gotInterrupt;
  lock lock;

} ideChannel;

typedef volatile struct {
  ideChannel channel[2];
  int busMaster;
  unsigned busMasterIo;

} ideController;

// Some predefined ATAPI packets
#define ATAPI_PACKET_UNLOCK						\
  ((unsigned char[]) { ATAPI_PERMITREMOVAL, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } )
#define ATAPI_PACKET_LOCK						\
  ((unsigned char[]) { ATAPI_PERMITREMOVAL, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0 } )
#define ATAPI_PACKET_STOP						\
  ((unsigned char[]) { ATAPI_STARTSTOP, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } )
#define ATAPI_PACKET_START						\
  ((unsigned char[]) { ATAPI_STARTSTOP, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0 } )
#define ATAPI_PACKET_EJECT						\
  ((unsigned char[]) { ATAPI_STARTSTOP, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0 } )
#define ATAPI_PACKET_CLOSE						\
  ((unsigned char[]) { ATAPI_STARTSTOP, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0 } )
#define ATAPI_PACKET_READCAPACITY					\
  ((unsigned char[]) { ATAPI_READCAPACITY, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } )
#define ATAPI_PACKET_READTOC						\
  ((unsigned char[]) { ATAPI_READTOC, 0, 1, 0, 0, 0, 0, 0, 12, 0x40, 0, 0 } )

#define _KERNELIDEDRIVER_
#endif
