//
//  Visopsys
//  Copyright (C) 1998-2014 J. Andrew McLaughlin
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
//  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
//
//  gpt.h
//

// This is the header for the handling of GPT disk labels

#if !defined(_GPT_H)

#include <sys/guid.h>
#include <sys/types.h>

#define GPT_SIG		"EFI PART"

// The header for the disk label
typedef struct {
	char signature[8];
	unsigned revision;
	unsigned headerBytes;
	unsigned headerCRC32;
	unsigned reserved1;
	uquad_t myLBA;
	uquad_t altLBA;
	uquad_t firstUsableLBA;
	uquad_t lastUsableLBA;
	guid diskGUID;
	uquad_t partEntriesLBA;
	unsigned numPartEntries;
	unsigned partEntryBytes;
	unsigned partEntriesCRC32;
	char reserved2[512 - 92];

} __attribute__((packed)) gptHeader;

// An individual partition entry
typedef struct {
	guid typeGuid;
	guid partGuid;
	uquad_t startingLBA;
	uquad_t endingLBA;
	uquad_t attributes;
	char partName[72];

} __attribute__((packed)) gptEntry;

// Function to get the disk label structure
diskLabel *getLabelGpt(void);

#define _GPT_H
#endif
