//
//  Visopsys
//  Copyright (C) 1998-2004 J. Andrew McLaughlin
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
//  kernelFilesystemIso.h
//

#if !defined(_KERNELFILESYSTEMISO_H)

#include "kernelDisk.h"
#include "kernelFilesystem.h"

// Definitions
#define ISO_PRIMARY_VOLDESC_SECTOR        16
#define ISO_STANDARD_IDENTIFIER           "CD001"
#define ISO_DESCRIPTORTYPE_BOOT           0
#define ISO_DESCRIPTORTYPE_PRIMARY        1
#define ISO_DESCRIPTORTYPE_SUPPLEMENTARY  2
#define ISO_DESCRIPTORTYPE_PARTITION      3
#define ISO_DESCRIPTORTYPE_TERMINATOR     255

#define ISO_FLAGMASK_HIDDEN          0x01
#define ISO_FLAGMASK_DIRECTORY       0x02
#define ISO_FLAGMASK_ASSOCIATED      0x04
#define ISO_FLAGMASK_EXTENDEDSTRUCT  0x08
#define ISO_FLAGMASK_EXTENDEDPERM    0x10
#define ISO_FLAGMASK_LINKS           0x80

// Structures

typedef volatile struct {

  unsigned char dirIdentLength;
  unsigned char extAttrLength;
  unsigned blockNumber;
  unsigned parentDirRecord;
  char name[255];

} isoPathTableRecord;

typedef volatile struct {

  unsigned blockNumber;
  unsigned flags;
  unsigned unitSize;
  unsigned intrGapSize;
  unsigned volSeqNumber;
  unsigned versionNumber;

} isoFileData;

// Global filesystem data
typedef volatile struct {

  char volumeIdentifier[32];
  unsigned volumeBlocks;
  unsigned blockSize;
  unsigned pathTableSize;
  unsigned pathTableBlock;
  const kernelDisk *disk;

} isoInternalData;

// Functions exported by kernelFileSystemIso.c, not defined elsewhere.
int kernelFilesystemIsoDetect(const kernelDisk *);
int kernelFilesystemIsoMount(kernelFilesystem *);
int kernelFilesystemIsoUnmount(kernelFilesystem *);
unsigned kernelFilesystemIsoGetFree(kernelFilesystem *);
int kernelFilesystemIsoNewEntry(kernelFileEntry *);
int kernelFilesystemIsoInactiveEntry(kernelFileEntry *);
int kernelFilesystemIsoResolveLink(kernelFileEntry *);
int kernelFilesystemIsoReadFile(kernelFileEntry *, unsigned, unsigned,
				unsigned char *);
int kernelFilesystemIsoReadDir(kernelFileEntry *);

#define _KERNELFILESYSTEMISO_H
#endif
