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
//  gpt.c
//

// This code does operations specific to GPT-labelled disks.

#include "fdisk.h"
#include "gpt.h"
#include "msdos.h"
#include <libintl.h>
#include <stdlib.h>
#include <sys/api.h>

#define _(string) gettext(string)


static unsigned headerChecksum(gptHeader *header)
{
	// Given a gptHeader structure, compute the checksum

	gptHeader *headerCopy = NULL;
	unsigned checksum = 0;

	// Allocate and make a copy of the header because we have to modify it
	headerCopy = malloc(sizeof(gptHeader));
	if (headerCopy == NULL)
		return (-1);

	memcpy(headerCopy, header, sizeof(gptHeader));
	header = headerCopy;

	// Zero the checksum field
	header->headerCRC32 = 0;

	// Get the checksum
	checksum = crc32(header, header->headerBytes, NULL);

	free(header);
	return (checksum);
}


static gptHeader *readHeader(const disk *theDisk)
{
	// Return a malloc-ed buffer containing the GPT header.

	int status = 0;
	gptHeader *header = NULL;

	// Get memory for the header
	header = malloc(theDisk->sectorSize);
	if (header == NULL)
	{
		error("%s", _("Can't get memory for a GPT header"));
		return (header = NULL);
	}

	// The guard MS-DOS table in the first sector.  Read the second sector.
	status = diskReadSectors(theDisk->name, 1, 1, header);
	if (status < 0)
	{
		error("%s", _("Can't read GPT header"));
		free(header);
		return (header = NULL);
	}

	// Check for the GPT signature 
	if (memcmp(header->signature, GPT_SIG, 8))
	{
		// No signature.
		free(header);
		return (header = NULL);
	}

	// Check the header checksum.  For the moment we only warn if it's not
	// correct.  Later we should be trying the backup header.
	if (headerChecksum(header) != header->headerCRC32)
		error(_("GPT header checksum mismatch (%x != %x)"), headerChecksum(header),
			header->headerCRC32);

	return (header);
}


static int writeHeader(const disk *theDisk, gptHeader *header)
{
	// Given a file descriptor for the open disk and a GPT header structure,
	// write the header to disk.
	 
	int status = 0;
	gptHeader *headerCopy = NULL;

	// Make a copy of the header so we can modify it (other than just the
	// checksum
	headerCopy = malloc(header->headerBytes);
	if (!headerCopy)
	{
		error("%s", _("Can't get memory for a GPT header copy"));
		return (status = ERR_MEMORY);
	}
	memcpy(headerCopy, header, header->headerBytes);

	// Compute the header checksum
	headerCopy->headerCRC32 = headerChecksum(headerCopy);

	// The guard MS-DOS table in the first sector.  Write the second sector.
	status = diskWriteSectors(theDisk->name, 1, 1, headerCopy);
	if (status < 0)
	{
		error("%s", _("Can't write GPT header"));
		free(headerCopy);
		return (status);
	}

	// Reset the header copy
	memcpy(headerCopy, header, header->headerBytes);

	// Adjust the 'my LBA' field and recompute the checksum
	headerCopy->myLBA = header->altLBA;
	headerCopy->altLBA = header->myLBA;

	headerCopy->headerCRC32 = headerChecksum(headerCopy);

	// Write the backup partition table header
	status = diskWriteSectors(theDisk->name, header->altLBA, 1, headerCopy);
	if (status < 0)
		warning("%s", _("Can't write backup GPT header"));

	free(headerCopy);

	return (0);
}


static unsigned entriesChecksum(gptEntry *entries, unsigned bytes)
{
	// Given an array of gptEntry structures and its size, compute the
	// checksum
	return (crc32(entries, bytes, NULL));
}


static gptEntry *readEntries(const disk *theDisk, gptHeader *header)
{
	// Given a GPT header, return a malloc-ed array of partition entries.
	 
	int status = 0;
	unsigned entryBytes = 0;
	unsigned entrySectors = 0;
	gptEntry *entries = NULL;

	// Calculate the number of sectors we need to read
	entryBytes = (header->numPartEntries * header->partEntryBytes);
	entrySectors = ((entryBytes / theDisk->sectorSize) +
		((entryBytes % theDisk->sectorSize)? 1 : 0));

	// Get memory for the entries
	entries = malloc(entrySectors * theDisk->sectorSize);
	if (entries == NULL)
	{
		error("%s", _("Can't get memory for a GPT entry array"));
		return (entries = NULL);
	}

	// Read the entries
	status = diskReadSectors(theDisk->name, header->partEntriesLBA,
		entrySectors, entries);
	if (status < 0)
	{
		error("%s", _("Can't read GPT entries"));
		free(entries);
		return (entries = NULL);
	}

	// Check the entries checksum.  For the moment we only warn if it's not
	// correct.  Later we should be trying the backup entries.
	if (entriesChecksum(entries, entryBytes) != header->partEntriesCRC32)
		error(_("GPT entries checksum mismatch (%x != %x)"),
			entriesChecksum(entries, entryBytes), header->partEntriesCRC32);

	return (entries);
}


static int writeEntries(const disk *theDisk, gptHeader *header,
	gptEntry *entries)
{
	// Given a GPT header and entries, write the partition entries to disk.
	 
	int status = 0;
	unsigned entryBytes = 0;
	unsigned entrySectors = 0;

	// Calculate the number of sectors we need to read
	entryBytes = (header->numPartEntries * header->partEntryBytes);
	entrySectors = ((entryBytes / theDisk->sectorSize) +
		((entryBytes % theDisk->sectorSize)? 1 : 0));

	// Read the primary entries
	status = diskWriteSectors(theDisk->name, header->partEntriesLBA,
		entrySectors, entries);
	if (status < 0)
	{
		error("%s", _("Can't write GPT entries"));
		return (status);
	}

	// Write the backup entries
	status = diskWriteSectors(theDisk->name, (header->lastUsableLBA + 1),
		entrySectors, entries);
	if (status < 0)
	{
		error("%s", _("Can't write GPT backup entries"));
		return (status);
	}

	// Update the entries checksum in the header 
	header->partEntriesCRC32 = entriesChecksum(entries, entryBytes);

	return (0);
}


static inline int isEntryUsed(guid *g)
{
	// A GPT entry is empty if the partition type GUID is all NULLs
	 
	if (memcmp(g, &GUID_UNUSED, sizeof(guid)))
		return (1);
	else
		return (0);
}


static int detect(const disk *theDisk)
{
	// Checks for the presense of a GPT disk label.

	int isGpt = 0;
	unsigned char *sectorData = NULL;
	msdosTable *table = NULL;
	int foundMsdosProtective = 0;
	gptHeader *header = NULL;
	int count;

	// A GPT disk must have a "guard" MS-DOS table, so a call to the MS-DOS
	// detect() function must succeed first.
	if (getLabelMsdos()->detect(theDisk) != 1)
		// Not a GPT label
		return (isGpt = 0);

	// Make sure it has the GPT protective partition
	sectorData = malloc(theDisk->sectorSize);
	if (sectorData == NULL)
		return (isGpt = 0);

	// Read the MS-DOS table
	if (diskReadSectors(theDisk->name, 0, 1, sectorData) < 0)
	{
		free(sectorData);
		return (isGpt = 0);
	}

	// Make sure it has the GPT protective partition
	table = (msdosTable *)(sectorData + MSDOS_TABLE_OFFSET);
	for (count = 0; count < MSDOS_TABLE_ENTRIES; count ++)
	{
		if (table->entries[count].tag == MSDOSTAG_EFI_GPT_PROT)
		{
			foundMsdosProtective = 1;
			break;
		}
	}

	free(sectorData);

	if (!foundMsdosProtective)
	{
		// Say it's not a valid GPT label
		return (isGpt = 0);
	}

	// Read the header 
	header = readHeader(theDisk);
	if (header)
	{
		// Call this a GPT label.
		free(header);
		isGpt = 1;
	}

	return (isGpt);
}


static int readTable(const disk *theDisk, rawSlice *slices, int *numSlices)
{
	// Read the partition table.

	int status = 0;
	gptHeader *header = NULL;
	gptEntry *entries = NULL;
	unsigned count;

	// Read the header
	header = readHeader(theDisk);
	if (header == NULL)
		return (status = ERR_INVALID);

	// Read the partition entries
	entries = readEntries(theDisk, header);
	if (entries == NULL)
		return (status = ERR_INVALID);

	// Fill in the partition entries 
	for (count = 0; ((count < DISK_MAX_PARTITIONS) &&
		(count < header->numPartEntries)); count ++)
	{
		if (isEntryUsed(&entries[count].typeGuid))
		{
			// Assign the data fields in the appropriate slice

			slices[*numSlices].order = *numSlices;
			slices[*numSlices].type = partition_primary;
			slices[*numSlices].tag = 1;

			// The logical (LBA) start sector and number of sectors.
			slices[*numSlices].startLogical = entries[count].startingLBA;
			slices[*numSlices].sizeLogical =
				((entries[count].endingLBA - entries[count].startingLBA) + 1);

			// Calculate some (fictitious) partition geometry.
			slices[*numSlices].geom.startCylinder =
				(entries[count].startingLBA / CYLSECTS(theDisk));
			slices[*numSlices].geom.startHead =
				((entries[count].startingLBA % CYLSECTS(theDisk)) /
					theDisk->sectorsPerCylinder);
			slices[*numSlices].geom.startSector =
				(((entries[count].startingLBA % CYLSECTS(theDisk)) %
					theDisk->sectorsPerCylinder) + 1);
			slices[*numSlices].geom.endCylinder = 
				(entries[count].endingLBA / CYLSECTS(theDisk));
			slices[*numSlices].geom.endHead =
				((entries[count].endingLBA % CYLSECTS(theDisk)) /
					theDisk->sectorsPerCylinder);
			slices[*numSlices].geom.endSector =
				(((entries[count].endingLBA % CYLSECTS(theDisk)) %
					theDisk->sectorsPerCylinder) + 1);

			// The partition type GUID.
			memcpy(&slices[*numSlices].typeGuid, &entries[count].typeGuid,
				sizeof(guid));
			// The partition GUID.
			memcpy(&slices[*numSlices].partGuid, &entries[count].partGuid,
				sizeof(guid));
			// The attributes
			slices[*numSlices].attributes = entries[count].attributes;

			*numSlices += 1;
		}
	}

	free(header);
	free(entries);
	return (status = 0);
}


static int writeTable(const disk *theDisk, rawSlice *slices, int numSlices)
{
	// Write the partition table.

	int status = 0;
	gptHeader *header = NULL;
	gptEntry *entries = NULL;
	int numEntries = 0;
	int count;

	// Read the header
	header = readHeader(theDisk);
	if (header == NULL)
		return (status = ERR_INVALID);

	// Read the partition entries
	entries = readEntries(theDisk, header);
	if (entries == NULL)
		return (status = ERR_INVALID);

	// Clear the partition entries
	bzero(entries, (header->numPartEntries * header->partEntryBytes));

	// Fill in the partition entries 
	for (count = 0; ((count < DISK_MAX_PARTITIONS) && (count < numSlices));
		count ++)
	{
		if (isEntryUsed(&slices[count].typeGuid))
		{
			// Assign the data fields in the appropriate slice

			// The GPT partition type GUID.
			memcpy(&entries[numEntries].typeGuid, &slices[count].typeGuid,
				sizeof(guid));

			// The GPT partition GUID.  If this is a newly-created partition
			// then it won't have one yet.
			if (!isEntryUsed(&slices[count].partGuid))
				guidGenerate(&slices[count].partGuid);
			memcpy(&entries[numEntries].partGuid, &slices[count].partGuid,
				sizeof(guid));

			// The logical (LBA) start sector and end sectors.
			entries[numEntries].startingLBA = slices[count].startLogical;
			entries[numEntries].endingLBA =
				(slices[count].startLogical + slices[count].sizeLogical - 1);

			// The GPT attributes
			entries[numEntries].attributes = slices[count].attributes;

			numEntries += 1;
		}
	}

	// Write the entries
	status = writeEntries(theDisk, header, entries);
	if (status < 0)
		return (status);

	// Write the header
	status = writeHeader(theDisk, header);
	if (status < 0)
		return (status);

	return (status = 0);
}


static int getSliceDesc(rawSlice *slc, char *string)
{
	// Given a pointer to a raw slice, return a string description based on
	// the partition type.  For GPT we call the kernel to ask for the GUID
	// description, if known.

	int status = 0;
	gptPartType type;

	status = diskGetGptPartType(&slc->typeGuid, &type);
	if (status < 0)
		return (status);

	strncpy(string, type.description, FSTYPE_MAX_NAMELENGTH);
	return (status = 0);
}


static sliceType canCreate(slice *slices __attribute__((unused)),
	int numSlices __attribute__((unused)),
	int sliceNumber __attribute__((unused)))
{
	// This will return a sliceType enumeration if, given a slice number
	// representing free space, a partition can be created there.  With GPT,
	// however, if there's empty space, a primary partition (the only type we
	// do) can always be created there.  This function is a lot more
	// "interesting" for MS-DOS labels ;^)
	return (partition_primary);
}


static int getTypes(listItemParameters **typeListParams)
{
	// Get the list of supported partition types as an array of
	// listItemParameters structures

	gptPartType *types = NULL;
	int numberTypes = 0;
	int count;

	// Get the list of types
	types = diskGetGptPartTypes();
	if (types == NULL)
		return (numberTypes = ERR_NODATA);

	for (count = 0; isEntryUsed(&types[count].typeGuid); count ++)
		numberTypes += 1;
	
	// Make an array of list item parameters

	*typeListParams = malloc(numberTypes * sizeof(listItemParameters));
	if (*typeListParams == NULL)
	{
		numberTypes = ERR_MEMORY;
		goto out;
	}

	for (count = 0; count < numberTypes; count ++)
		strncpy((*typeListParams)[count].text, types[count].description,
			WINDOW_MAX_LABEL_LENGTH);

out:
	memoryRelease(types);
	return (numberTypes);
}


static int setType(slice *slc, int typeNum)
{
	// Given a slice and the number of a type (returned in the list by the
	// function getTypes()), set the type.

	int status = 0;
	gptPartType *types = NULL;

	// Get the list of types
	types = diskGetGptPartTypes();
	if (types == NULL)
		return (status = ERR_NODATA);

	memcpy(&slc->raw.typeGuid, &types[typeNum].typeGuid, sizeof(guid));

	memoryRelease(types);
	return (status = 0);
}


diskLabel gptLabel = {
	label_gpt, // type
	(LABELFLAG_PRIMARYPARTS | // flags
	 LABELFLAG_USEGUIDS),
	// Functions
	&detect,
	&readTable,
	&writeTable,
	&getSliceDesc,
	&canCreate,
	NULL, // canHide
	NULL, // hide
	&getTypes,
	&setType
};


diskLabel *getLabelGpt(void)
{
	// Called at initialization, returns a pointer to the disk label structure.
	return (&gptLabel);
}
