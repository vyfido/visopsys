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
//  kernelIdeDriver.h
//

// This header file contains definitions for the kernel's standard IDE/
// ATA/ATAPI driver

#if !defined(_KERNELIDEDRIVER_H)

#include "kernelLock.h"

#define MAX_IDE_DISKS 4

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
#define ATA_READSECTS_RET    0x20
#define ATA_READSECTS        0x21
#define ATA_READECC_RET      0x22
#define ATA_READECC          0x23
#define ATA_WRITESECTS_RET   0x30
#define ATA_WRITESECTS       0x31
#define ATA_WRITEECC_RET     0x32
#define ATA_WRITEECC         0x33
#define ATA_VERIFYMULT_RET   0x40
#define ATA_VERIFYMULT       0x41
#define ATA_FORMATTRACK      0x50
#define ATA_SEEK             0x70
#define ATA_DIAG             0x90
#define ATA_INITPARAMS       0x91
#define ATA_ATAPIPACKET      0xA0
#define ATA_ATAPIIDENTIFY    0xA1
#define ATA_ATAPISERVICE     0xA2
#define ATA_READMULTIPLE     0xC4
#define ATA_WRITEMULTIPLE    0xC5
#define ATA_SETMULTIMODE     0xC6
#define ATA_GETDEVINFO       0xEC
#define ATA_ATAPISETFEAT     0xEF

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
  unsigned data;
  unsigned featErr;
  unsigned sectorCount;
  unsigned sectorNumber;
  unsigned cylinderLow;
  unsigned cylinderHigh;
  unsigned driveHead;
  unsigned comStat;
  unsigned altComStat;

} idePorts;

typedef struct {
  lock controllerLock;
  int interruptReceived;

} ideController;

// Some predefined ATAPI packets
#define ATAPI_PACKET_UNLOCK \
 ((unsigned char[]) { ATAPI_PERMITREMOVAL, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } )
#define ATAPI_PACKET_LOCK \
 ((unsigned char[]) { ATAPI_PERMITREMOVAL, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0 } )
#define ATAPI_PACKET_STOP \
 ((unsigned char[]) { ATAPI_STARTSTOP, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } )
#define ATAPI_PACKET_START \
 ((unsigned char[]) { ATAPI_STARTSTOP, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0 } )
#define ATAPI_PACKET_EJECT \
 ((unsigned char[]) { ATAPI_STARTSTOP, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0 } )
#define ATAPI_PACKET_CLOSE \
 ((unsigned char[]) { ATAPI_STARTSTOP, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0 } )
#define ATAPI_PACKET_READCAPACITY \
 ((unsigned char[]) { ATAPI_READCAPACITY, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } )
#define ATAPI_PACKET_READTOC \
 ((unsigned char[]) { ATAPI_READTOC, 0, 1, 0, 0, 0, 0, 0, 12, 0x40, 0, 0 } )

#define _KERNELIDEDRIVER_
#endif
