// 
//  Visopsys
//  Copyright (C) 1998-2013 J. Andrew McLaughlin
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
//  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
//
//  ntfs.h
//

// This file contains definitions and structures for using and manipulating
// the Microsoft(R) NTFS filesystem in Visopsys.

#if !defined(_NTFS_H)

#include <sys/disk.h>
#include <sys/progress.h>

#define NTFS_MAGIC_FILERECORD    0x454C4946 // 'FILE'

#define NTFS_ATTR_STANDARDINFO   0x10
#define NTFS_ATTR_ATTRLIST       0x20
#define NTFS_ATTR_FILENAME       0x30
#define NTFS_ATTR_VOLUMEVERSION  0x40
#define NTFS_ATTR_OBJECTID       0x40
#define NTFS_ATTR_SECURITYDESC   0x50
#define NTFS_ATTR_VOLUMENAME     0x60
#define NTFS_ATTR_VOLUMEINFO     0x70
#define NTFS_ATTR_DATA           0x80
#define NTFS_ATTR_INDEXROOT      0x90
#define NTFS_ATTR_INDEXALLOC     0xA0
#define NTFS_ATTR_BITMAP         0xB0
#define NTFS_ATTR_SYMBOLICLINK   0xC0
#define NTFS_ATTR_REPARSEPOINT   0xC0
#define NTFS_ATTR_EAINFO         0xD0
#define NTFS_ATTR_EA             0xE0
#define NTFS_ATTR_PROPERTYSET    0xF0
#define NTFS_ATTR_LOGGEDUTILSTR  0x100
#define NTFS_ATTR_TERMINATE      0xFFFFFFFF

typedef struct {
  unsigned magic;                    // 00 - 03  Magic number 'FILE'
  unsigned short updateSeqOffset;    // 04 - 05  Update sequence array offset
  unsigned short updateSeqLength;    // 06 - 08  Update sequence array length
  unsigned char unused[8];           // 09 - 0F  Unused
  unsigned short seqNumber;          // 10 - 11  Sequence number
  unsigned short refCount;           // 12 - 13  Reference count
  unsigned short attrSeqOffset;      // 14 - 15  Attributes sequence offset
  unsigned short flags;              // 16 - 17  Flags
  unsigned recordRealLength;         // 18 - 1B  Real file record size
  unsigned recordAllocLength;        // 1C - 1F  Allocated file record size
  unsigned long long baseFileRecord; // 20 - 27  Base record file reference
  unsigned short maxAttrId;          // 28 - 29  Max attribute identifier + 1
  unsigned short updateSeq;          // 2A - 2B  Update sequence number
  unsigned char updateSeqArray[];    // 2C -     Update sequence array

} __attribute__((packed)) ntfsFileRecord;

typedef struct {
  unsigned type;                     // 00 - 03  Attribute type
  unsigned length;                   // 04 - 07  Length
  unsigned char nonResident;         // 08 - 08  Non-resident flag
  unsigned char nameLength;          // 09 - 09  Name length
  unsigned short nameOffset;         // 0A - 0B  Name Offset
  unsigned short flags;              // 0C - 0C  Flags
  unsigned short attributeId;        // 0E - 0F  Attribute ID
  union {
    struct {
      unsigned attributeLength;      // 10 - 13  Attribute length
      unsigned char attributeOffset; // 14 - 15  Attribute offset
      unsigned char indexedFlag;     // 16 - 16  Indexed flag
      unsigned char unused;          // 17 - 17  Unused
      unsigned char attribute[];     // 18 -     Attribute - Starts with name
    } yes;                           //          if named attribute.
    struct {
      unsigned long long startVcn;   // 10 - 17  Starting VCN
      unsigned long long lastVcn;    // 18 - 1F  Ending VCN
      unsigned short dataRunsOffset; // 20 - 21  Data runs offset
      unsigned short compUnitSize;   // 22 - 23  Compression unit size
      unsigned char unused[4];       // 24 - 27  Unused
      unsigned long long allocLength;// 28 - 2F  Attribute allocated length
      unsigned long long realLength; // 30 - 37  Attribute real length
      unsigned long long streamLength;//38 - 3F  Initialized data stream len
      unsigned char dataRuns[];      // 40 -     Data runs - Starts with name
    } no;                            //          if named attribute.
  } res;
} __attribute__((packed)) ntfsAttributeHeader;

typedef struct {
  unsigned long long parentDirRef;   // 00 - 07  File reference of parent dir
  unsigned long long cTime;          // 08 - 0F  File creation
  unsigned long long aTime;          // 10 - 17  File altered
  unsigned long long mTime;          // 18 - 1F  MFT record changed
  unsigned long long rTime;          // 20 - 27  File read (accessed) time
  unsigned long long allocLength;    // 28 - 2F  File allocated length
  unsigned long long realLength;     // 30 - 37  File real length
  unsigned flags;                    // 38 - 3B  Flags (eg. dir, comp, hidden)
  unsigned EaFlags;                  // 3C - 3F  Used by EAs and reparse
  unsigned char filenameLength;      // 40 - 40  Filename length in characters
  unsigned char filenameNamespace;   // 41 - 41  Filename namespace
  unsigned short filename[];         // 42 -     Filename in Unicode (not NULL
                                     //          terminated)
} __attribute__((packed)) ntfsFilenameAttribute;

typedef struct {
  unsigned magic;                    // 00 - 03  Magic number 'INDX'
  unsigned short updateSeqOffset;    // 04 - 05  Update sequence array offset
  unsigned short updateSeqLength;    // 06 - 08  Update sequence array length
  unsigned char unused1[8];          // 09 - 0F  Unused
  unsigned long long indexBufferVcn; // 10 - 17  VCN of the index buffer
  unsigned short entriesStartOffset; // 18 - 19  Entries starting offset - 0x18
  unsigned char unused2[2];          // 1A - 1B  Unused
  unsigned entriesEndOffset;         // 1C - 1F  Entries ending offset - 0x18
  unsigned bufferEndOffset;          // 20 - 23  Buffer ending offset - 0x18
  unsigned rootNode;                 // 24 - 27  1 if not leaf node
  unsigned short updateSeq;          // 28 - 29  Update sequence number
  unsigned char updateSeqArray[];    // 2A -     Update sequence array

} __attribute__((packed)) ntfsIndexBuffer;

typedef struct {
  unsigned long long fileReference;  // 00 - 07  File reference
  unsigned short entryLength;        // 08 - 09  Index entry length
  unsigned short streamLength;       // 0A - 0B  Length of the stream
  unsigned char flags;               // 0C - 0C  Flags
  unsigned char unused[3];           // 0D - 0F  Unused
  unsigned char stream[];            // 10 -     Data

} __attribute__((packed)) ntfsIndexEntry;

typedef struct {
  unsigned char jmpBoot[3];          // 00  - 02   Jmp to boot code
  char oemName[8];		     // 03  - 0A   OEM Name
  unsigned short bytesPerSect;	     // 0B  - 0C   Bytes per sector
  unsigned char sectsPerClust;	     // 0D  - 0D   Sectors per cluster
  unsigned char unused1[7];          // 0E  - 14   Unused
  unsigned char media;		     // 15  - 15   Media descriptor byte
  unsigned char unused2[2];          // 16  - 17   Unused
  unsigned short sectsPerTrack;	     // 18  - 19   Sectors per track
  unsigned short numHeads;	     // 1A  - 1B   Number of heads
  unsigned char unused3[8];          // 1C  - 23   Unused
  unsigned biosDriveNum;	     // 24  - 27   BIOS drive number
  unsigned long long sectsPerVolume; // 28  - 2F   Sectors per volume
  unsigned long long mftStart;       // 30  - 37   LCN of VCN 0 of the $MFT
  unsigned long long mftMirrStart;   // 38  - 3F   LCN of VCN 0 of the $MFTMirr
  unsigned clustersPerMftRec;        // 40  - 43   Clusters per MFT Record
  unsigned clustersPerIndexRec;      // 44  - 47   Clusters per Index Record
  unsigned long long volSerial;      // 48  - 4F   Volume serial number
  unsigned char unused4[13];         // 50  - 5C   Unused
  unsigned char bootCode[417];       // 5D  - 1FD  Boot loader code
  unsigned short magic;              // 1FE - 1FF  Magic number
  unsigned char moreCode[];          // 200 -      More code of some sort

} __attribute__((packed)) ntfsBootFile;

// Functions in libntfs
int ntfsFormat(const char *, const char *, int, progress *);
int ntfsGetResizeConstraints(const char *, uquad_t *, uquad_t *, progress *);
int ntfsResize(const char *, uquad_t, progress *);

#define _NTFS_H
#endif
