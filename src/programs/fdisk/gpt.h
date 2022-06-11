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
//  gpt.h
//

// This is a header for the Visopsys Disk Manager program, that describes
// things related to EFI GPT disk labels.

#if !defined(_GPT_H)

// EFI GPT GUID.
typedef struct {
  unsigned timeLow;
  unsigned short timeMid;
  unsigned short timeHiAndVersion;
  unsigned char clockHiAndReserved;
  unsigned char clockLow;
  unsigned char node[6];

} __attribute__((packed)) gptGuid;

// EFI GPT disk label.
typedef struct {
  char signature[8];
  unsigned revision;
  unsigned headerBytes;
  unsigned headerCRC32;
  unsigned reserved1;
  unsigned long long myLBA;
  unsigned long long altLBA;
  unsigned long long firstUsableLBA;
  unsigned long long lastUsableLBA;
  gptGuid diskGUID;
  unsigned long long partEntriesLBA;
  unsigned numPartEntries;
  unsigned partEntryBytes;
  unsigned partEntriesCRC32;
  char reserved2[512 - 92];

} __attribute__((packed)) gptHeader;

// EFI GPT partition table entry */
typedef struct {
  gptGuid partTypeGUID;
  gptGuid partGUID;
  unsigned long long startingLBA;
  unsigned long long endingLBA;
  unsigned long long attributes;
  char partName[72];

} __attribute__((packed)) gptEntry;	

#define _GPT_H
#endif
