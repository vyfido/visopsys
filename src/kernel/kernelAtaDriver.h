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
//  kernelAtaDriver.h
//

// This header file contains definitions for the ATA family of disk drivers

#if !defined(_KERNELATADRIVER_H)

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
#define ATA_MEDIASTATUS      0xDA
#define ATA_FLUSHCACHE       0xE7
#define ATA_FLUSHCACHE_EXT   0xEA
#define ATA_IDENTIFY         0xEC
#define ATA_SETFEATURES      0xEF

// ATAPI commands
#define ATAPI_TESTREADY      0x00
#define ATAPI_REQUESTSENSE   0x03
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

#define _KERNELATADRIVER_H
#endif
