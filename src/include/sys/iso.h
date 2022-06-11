// 
//  Visopsys
//  Copyright (C) 1998-2005 J. Andrew McLaughlin
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
//  iso.h
//

// This file contains definitions and structures for using and manipulating
// ISO 9660 filesystems in Visopsys.

#if !defined(_ISO_H)

// Definitions
#define ISO_PRIMARY_VOLDESC_SECTOR        16
#define ISO_STANDARD_IDENTIFIER           "CD001"
#define ISO_DESCRIPTORTYPE_BOOT           0
#define ISO_DESCRIPTORTYPE_PRIMARY        1
#define ISO_DESCRIPTORTYPE_SUPPLEMENTARY  2
#define ISO_DESCRIPTORTYPE_PARTITION      3
#define ISO_DESCRIPTORTYPE_TERMINATOR     255

#define ISO_FLAGMASK_HIDDEN               0x01
#define ISO_FLAGMASK_DIRECTORY            0x02
#define ISO_FLAGMASK_ASSOCIATED           0x04
#define ISO_FLAGMASK_EXTENDEDSTRUCT       0x08
#define ISO_FLAGMASK_EXTENDEDPERM         0x10
#define ISO_FLAGMASK_LINKS                0x80

// Structures

typedef struct {
  unsigned char recordLength;
  unsigned char extAttrLength;
  unsigned long long blockNumber;
  unsigned long long size;
  unsigned char date[7];
  unsigned char flags;
  unsigned char unitSize;
  unsigned char intrGapSize;
  unsigned volSeqNumber;
  unsigned char nameLength;
  char name[];

} __attribute__((packed)) isoDirectoryRecord;

typedef struct {
  unsigned char type;
  char identifier[5];
  unsigned char version;
  unsigned char unused1;
  char systemIdentifier[32];
  char volumeIdentifier[32];
  unsigned char unused2[8];
  unsigned long long volumeBlocks;
  unsigned char unused3[32];
  unsigned volumeSetSize;
  unsigned volumeSequenceNum;
  unsigned blockSize;
  unsigned long long pathTableSize;
  unsigned pathTableBlock;
  unsigned optTypeLPathTable;
  unsigned typeMPathTable;
  unsigned optTypeMPathTable;
  isoDirectoryRecord rootDirectoryRecord;
  char __rootDirRecNamePadding__;
  char volumeSetId[128];
  char publisherId[128];
  char preparerId[128];
  char applicationId[128];
  char copyrightFileId[37];
  char abstractFileId[37];
  char biblioFileId[37];
  unsigned char creationDate[17];
  unsigned char modificationDate[17];
  unsigned char expirationDate[17];
  unsigned char effectiveDate[17];
  unsigned char fileStructVersion;
  unsigned char unused4;
  unsigned char applicationData[512];
  unsigned char unused5[653];

} __attribute__((packed)) isoPrimaryDescriptor;


#define _ISO_H
#endif
