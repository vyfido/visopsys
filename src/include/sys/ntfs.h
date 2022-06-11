// 
//  Visopsys
//  Copyright (C) 1998-2007 J. Andrew McLaughlin
//  
//  This library is free software; you can redistribute it and/or modify it
//  under the terms of the GNU Lesser General Public License as published by
//  the Free Software Foundation; either version 2.1 of the License, or (at
//  your option) any later version.
//
//  This library is distributed in the hope that it will be useful, but
//  WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
//  General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public License
//  along with this library; if not, write to the Free Software Foundation,
//  Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
//
//  ntfs.h
//

// This file contains definitions and structures for using and manipulating
// the Microsoft(R) NTFS filesystem in Visopsys.

#if !defined(_NTFS_H)

#include <sys/disk.h>
#include <sys/progress.h>

typedef struct {
  unsigned char jmpBoot[3];             // 00 - 02 Jmp to boot code
  char oemName[8];		        // 03 - 0A OEM Name
  unsigned short bytesPerSect;		// 0B - 0C Bytes per sector
  unsigned char sectsPerClust;		// 0D - 0D Sectors per cluster
  unsigned char unused1[7];             // 0D - 14 Unused
  unsigned char media;			// 15 - 15 Media descriptor byte
  unsigned char unused2[2];             // 16 - 17 Unused
  unsigned short sectsPerTrack;		// 18 - 19 Sectors per track
  unsigned short numHeads;		// 1A - 1B Number of heads
  unsigned char unused3[8];             // 1C - 23 Unused
  unsigned biosDriveNum;	        // 24 - 27 BIOS drive number
  unsigned long long sectorsPerVolume;  // 28 - 2F Sectors per volume
  unsigned long long mftStart;          // 30 - 37 LCN of VCN 0 of the $MFT
  unsigned long long mftMirrStart;      // 38 - 3F LCN of VCN 0 of the $MFTMirr
  unsigned clustersPerMft;              // 40 - 43 Clusters per MFT Record
  unsigned clustersPerIndex;            // 44 - 47 Clusters per Index Record
  unsigned long long volSerial;         // 48 - 4F Volume serial number
  unsigned char pad[432];

} __attribute__((packed)) ntfsBootFile;

// Functions in libntfs
int ntfsGetResizeConstraints(const char *, unsigned *, unsigned *);
int ntfsResize(const char *, unsigned, progress *);

#define _NTFS_H
#endif
