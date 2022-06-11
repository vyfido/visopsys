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
//  fdisk_gpt.c
//

// This is a companion to fdisk.c, and contains routines specific to EFI
// GPT disk labels.

#include "fdisk_gpt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Some pre-defined, static GUID definitions
static gptGuid GUID_UNUSED = { 0, 0, 0, 0, 0, { 0, 0, 0, 0, 0, 0 } };
static gptGuid GUID_FAT32 = { 0xc12a7328, 0xf81f, 0x11d2, 0xba, 0x4b,
			   { 0x00, 0xa0, 0xc9, 0x3e, 0xc9, 0x3b } };
static gptGuid GUID_EXT2 = { 0xebd0a0a2, 0xb9e5, 0x4433, 0x87, 0xc0,
			  { 0x68, 0xb6, 0xb7, 0x26, 0x99, 0xc7 } };
static gptGuid GUID_LINUX_SWAP = { 0x0657fd6d, 0xa4ab, 0x43c4, 0x84, 0xe5,
				{ 0x09, 0x33, 0xc8, 0x4b, 0x4f, 0x4f } };

static unsigned long  gptCRCTable[256] = {
  0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F,
  0xE963A535, 0x9E6495A3, 0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988,
  0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91, 0x1DB71064, 0x6AB020F2,
  0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
  0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9,
  0xFA0F3D63, 0x8D080DF5, 0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172,
  0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B, 0x35B5A8FA, 0x42B2986C,
  0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
  0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423,
  0xCFBA9599, 0xB8BDA50F, 0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924,
  0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D, 0x76DC4190, 0x01DB7106,
  0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
  0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D,
  0x91646C97, 0xE6635C01, 0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E,
  0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457, 0x65B0D9C6, 0x12B7E950,
  0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
  0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7,
  0xA4D1C46D, 0xD3D6F4FB, 0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0,
  0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9, 0x5005713C, 0x270241AA,
  0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
  0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81,
  0xB7BD5C3B, 0xC0BA6CAD, 0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A,
  0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683, 0xE3630B12, 0x94643B84,
  0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
  0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB,
  0x196C3671, 0x6E6B06E7, 0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC,
  0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5, 0xD6D6A3E8, 0xA1D1937E,
  0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
  0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55,
  0x316E8EEF, 0x4669BE79, 0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236,
  0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F, 0xC5BA3BBE, 0xB2BD0B28,
  0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
  0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F,
  0x72076785, 0x05005713, 0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38,
  0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21, 0x86D3D2D4, 0xF1D4E242,
  0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
  0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69,
  0x616BFFD3, 0x166CCF45, 0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2,
  0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB, 0xAED16A4A, 0xD9D65ADC,
  0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
  0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD70693,
  0x54DE5729, 0x23D967BF, 0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94,
  0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
};


static void printGUID(gptGuid *guid)
{
  printf("%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
	 guid->timeLow,  guid->timeMid, guid->timeHiAndVersion,
	 guid->clockHiAndReserved, guid->clockLow, guid->node[0],
	 guid->node[1], guid->node[2], guid->node[3], guid->node[4],
	 guid->node[5]);
}


static unsigned gptCRC(void *ptr, int len, unsigned *checksum)
{
  register char *p = ptr;
  register unsigned crc;

  if (checksum == NULL)
    crc = ~0U;
  else
    crc = *checksum;

  while (len-- > 0)
    crc = (unsigned) gptCRCTable[(crc ^ *p++) & 0xFFL] ^ (unsigned)(crc >> 8);

  if (checksum != NULL)
    *checksum = crc;

  return (crc ^ ~0U);
}


static unsigned gptHeaderChecksum(gptHeader *header)
{
  // Given a gptHeader structure, compute the checksum

  gptHeader *headerCopy = NULL;
  unsigned checksum = 0;

  // Allocate and make a copy of the header because we have to modify it
  headerCopy = malloc(sizeof(gptHeader));
  if (!headerCopy)
    return (-1);

  memcpy(headerCopy, header, sizeof(gptHeader));
  header = headerCopy;

  // Zero the checksum field
  header->headerCRC32 = 0;

  // Get the checksum
  checksum = gptCRC(header, header->headerBytes, NULL);

  free(header);
  return (checksum);
}


static unsigned gptEntriesChecksum(gptEntry *entries, int bytes)
{
  // Given an array of gptEntry structures and its size, compute the
  // checksum
  return (gptCRC(entries, bytes, NULL));
}


static gptHeader *gptGetHeader(int fd)
{
  // Given a file descriptor for the open disk, return a malloc-ed buffer
  // containing * a GPT header.

  int status = 0;
  gptHeader *header = NULL;

  // Seek past the 'dummy' MS-DOS table in the first sector
  status = (int) lseek64(fd, 512ULL, SEEK_SET);
  if (status == -1)
    {
      printf("Can't seek device to header\n");
      return (header = NULL);
    }

  // Get a sector's worth of memory for the buffer
  header = calloc(1, sizeof(gptHeader));
  if (header == NULL)
    {
      printf("Can't get memory for a GPT header\n");
      return (header = NULL);
    }

  // Read the next sector
  status = read(fd, header, 512);
  if (status < 512)
    {
      printf("Can't read device\n");
      free(header);
      return (header = NULL);
    }

  // Check the header checksum 
  if (gptHeaderChecksum(header) != header->headerCRC32)
    printf("GPT header checksum mismatch (%x != %x)\n",
	   gptHeaderChecksum(header), header->headerCRC32);
  else
    printf("GPT header checksum OK\n");

  return (header);
}


static int gptWriteHeader(int fd, gptHeader *header)
{
  // Given a file descriptor for the open disk and a GPT header structure,
  // write the header to disk.
 
  int status = 0;
  gptHeader *headerCopy = NULL;

  // Seek past the 'dummy' MS-DOS table in the first sector 
  status = (int) lseek64(fd, (header->myLBA * 512), SEEK_SET);
  if (status == -1)
    {
      printf("Can't seek device\n");
      return (status);
    }

  // Compute the new header checksum 
  header->headerCRC32 = gptHeaderChecksum(header);

  // Write the data 
  status = (int) write(fd, header, header->headerBytes);
  if (status !=  header->headerBytes)
    {
      printf("Can't write device\n");
      return (-1);
    }

  // Seek to the alternate header sector (should be the last one on
  // the disk
   
  status = (int) lseek64(fd, (header->altLBA * 512), SEEK_SET);
  if (status == -1)
    {
      printf("Can't seek device\n");
      return (status);
    }

  // Make a copy of the header so we can modify it (other than just
  // the checksum
   
  headerCopy = calloc(1, header->headerBytes);
  if (!headerCopy)
    {
      printf("Can't get memory for a GPT header copy\n");
      return (-1);
    }
  memcpy(headerCopy, header, header->headerBytes);

  // Adjust the 'my LBA' field and recompute the checksum 
  headerCopy->myLBA = header->altLBA;
  headerCopy->headerCRC32 = gptHeaderChecksum(headerCopy);

  // Write the backup partition table header 
  status = (int) write(fd, headerCopy, headerCopy->headerBytes);
  if (status != headerCopy->headerBytes)
    {
      printf("Can't write device\n");
      free(headerCopy);
      return (-1);
    }

  return (0);
}


static int isGptDisk(char *fullpath)
{
  // Given a disk, return 1 if it has a GPT partition table signature

  int status = 0;
  gptHeader *header = NULL;
  int fd = 0;

  // Open the disk device 
  fd = open(fullpath, O_RDWR);
  if (fd == -1)
    {
      printf("Can't open device %s\n", fullpath);
      return (0);
    }

  // Read the header 
  header = gptGetHeader(fd);
  if (!header)
    {
      close(fd);
      return (0);
    }

  // Check for the GPT signature 
  if (!memcmp(header->signature, "EFI PART", 8))
    status = 1;
  
  free(header);
  close(fd);
  return (status);
}


static gptEntry *gptGetEntries(int fd, gptHeader *header)
{
  // Given a GPT header pointer, return the malloc-ed array of partition
  // entries.
 
  int status = 0;
  gptEntry *entries = NULL;

  // Get the memory 
  entries = malloc(header->numPartEntries * header->partEntryBytes);
  if (entries == NULL)
    {
      printf("Can't get memory for a GPT entry array\n");
      return (entries = NULL);
    }

  // Seek to the start of the partition entries 
  status = (int) lseek64(fd, (header->partEntriesLBA * 512), SEEK_SET);
  if (status == -1)
    {
      printf("Can't seek device to entries\n");
      free(entries);
      return (entries = NULL);
    }

  // Read the data 
  status = read(fd, entries, (header->numPartEntries *
			      header->partEntryBytes));
  if (status < (header->numPartEntries * header->partEntryBytes))
    {
      printf("Can't read device\n");
      free(entries);
      return (entries = NULL);
    }

  return (entries);
}


static int gptWriteEntries(int fd, gptHeader *header, gptEntry *entries)
{
  // Given a file descriptor for the open disk, and GPT header and entries
  // pointers, write the partition entries to disk.
 
  int status = 0;

  // Seek to the start of the partition entries 
  status = (int) lseek64(fd, (header->partEntriesLBA * 512), SEEK_SET);
  if (status == -1)
    {
      printf("Can't seek device\n");
      return (-1);
    }

  // Write the data 
  status = (int) write(fd, entries, (header->partEntryBytes *
				     header->numPartEntries));
  if (status != (header->partEntryBytes * header->numPartEntries))
    {
      printf("Can't write device\n");
      return (-1);
    }

  // Update the entries checksum in the header 
  header->partEntriesCRC32 =
    gptEntriesChecksum(entries, (header->partEntryBytes *
				 header->numPartEntries));

  return (0);
}


static inline int gptEntryUsed(gptEntry *entry)
{
  // A GPT entry is empty if the partition type GIUD is all zeros
 
  if (memcmp(&entry->partTypeGUID, &GUID_UNUSED, sizeof(gptGuid)))
    return (1);
  else
    return (0);
}


static int gptGetPart(char *fullpath, struct diskent *ent, int *maxp)
{
  // Given a *full path* disk name, get the partition info from a
  // GPT-labelled disk
 
  int status = 0;
  int fd = 0;
  gptHeader *header = NULL;
  gptEntry *entries = NULL;
  int count;

  // Open the disk device 
  fd = open(fullpath, O_RDWR);
  if (fd == -1)
    {
      printf("Can't open device %s\n", fullpath);
      return (-1);
    }

  // Read the header 
  header = gptGetHeader(fd);
  if (!header)
    {
      close(fd);
      return (-1);
    }

  // Read the partition entries 
  entries = gptGetEntries(fd, header);

  close(fd);

  if (!entries)
    {
      free(header);
      return (-1);	
    }

  // Fill in the partition entries 
  for (count = 0; ((count < MAX_PARTITIONS) &&
		   (count < header->numPartEntries)); count ++)
    {
      if (gptEntryUsed(&entries[count]))
	{
	  ent->part[count + 1].part_num = (count + 1);
	  ent->part[count + 1].part_start = entries[count].startingLBA;
	  ent->part[count + 1].part_size = (entries[count].endingLBA -
					    entries[count].startingLBA + 1);
	  ent->part[count + 1].part_tag = gpt_map_guid_to_tag(&entries[count]
							      .partTypeGUID);
	  memcpy(&(ent->part[count + 1].part_type_guid),
		 &entries[count].partTypeGUID, sizeof(gptGuid));
	  memcpy(&(ent->part[count + 1].part_guid), &entries[count].partGUID,
		 sizeof(gptGuid));
	  ent->part[count + 1].part_flags = entries[count].attributes;
	  memcpy(ent->part[count + 1].part_name, entries[count].partName, 72);
	  
	  printf("%d: start %llu end %llu attrib %llx %s", count,
		 entries[count].startingLBA, entries[count].endingLBA, 
		 entries[count].attributes, entries[count].partName);
	  printGUID(&entries[count].partTypeGUID);
	  printf("\n");
	}
    }

  free(header);
  free(entries);
  return (0);
}


static int gptFindEntry(gptHeader *header, gptEntry *entries,
			unsigned long long start, unsigned long long size)
{
  // Given an array of GPT partition entries, try to locate one with the
  // supplied start and size values.  If found, return the index of the
  // entry.  Otherwise, return -1
 
  int count;

  for (count = 0; count < header->numPartEntries; count ++)
    {
      if (gptEntryUsed(&entries[count]) &&
	  (entries[count].startingLBA == start) &&
	  (entries[count].endingLBA == (start + size - 1)))
	return (count);
    }

  // Not found 
  return (-1);
}


static int gptWritePart(char *fullpath, struct diskent *ent, int flag)
{
  // Given a *full path* disk name pointing to a GPT-labelled disk, write a
  // new partition scheme to disk.
 
  int status = 0;
  int fd = 0;
  gptHeader *header = NULL;
  gptEntry *orig_entries = NULL;
  gptEntry *entries = NULL;
  int count;

  // Open the disk device 
  fd = open(fullpath, O_RDWR);
  if (fd == -1)
    {
      printf("Can't open device %s\n", fullpath);
      return (-1);
    }

  // Read the header 
  header = gptGetHeader(fd);
  if (!header)
    {
      close(fd);
      return (-1);
    }

  // Read the existing partition entries 
  orig_entries = gptGetEntries(fd, header);
  if (!orig_entries)
    {
      free(header);
      close(fd);
      return (-1);
    }

  // Get some memory to hold our array of new entries 
  entries = calloc(header->numPartEntries, header->partEntryBytes);
  if (!entries)
    {
      free(header);
      free(orig_entries);
      close(fd);
      return (-1);
    }

  // Loop for each partition 
  for (count = 1; ((count <= MAX_PARTITIONS) &&
		   (count <= header->numPartEntries)); count ++)
    {
      if (ent->part[count].part_size)
	{
	  // Is there already a matching entry? 
	  int old_index = gptFindEntry(header, orig_entries,
				       ent->part[count].part_start,
				       ent->part[count].part_size);
	  if (old_index >= 0)
	    {
	      // A matching entry already exists (i.e. one with the same start
	      // and end positions).  Because our system would cause all kinds
	      // of information to be lost, we copy the existing entry first,
	      // and then overwrite the fields we've been provided.
	      memcpy(&entries[count - 1], &orig_entries[old_index],
		     header->partEntryBytes);
	    }
	  else
	    {
	      memcpy(&entries[count - 1].partGUID,
		     &GUID_EXT2, sizeof(gptGuid));
	      entries[count - 1].attributes = 0;
	      //entries[count - 1].partName[72];
	    }

	  entries[count - 1].startingLBA = ent->part[count].part_start;
	  entries[count - 1].endingLBA = (ent->part[count].part_start +
					  (ent->part[count].part_size - 1));
	  memcpy(&entries[count - 1].partTypeGUID,
		 gpt_map_tag_to_guid(ent->part[count].part_tag),
		 sizeof(gptGuid));

	  // Make sure the start and end are legal 
	  if ((entries[count - 1].startingLBA < header->firstUsableLBA) ||
	      (entries[count - 1].endingLBA > header->lastUsableLBA)) {
	    printf("Partition parameters (%llu->%llu) are outside legal range "
		   "(%llu->%llu)", entries[count - 1].startingLBA,
		   entries[count - 1].endingLBA, header->firstUsableLBA,
		   header->lastUsableLBA);
	    continue;
	  }

	  printf("Wrote entry %d %llu-%llu\n", (count - 1),
		 entries[count - 1].startingLBA, entries[count - 1].endingLBA);
	}

      else
	printf("Entry %d size %llu\n", (count - 1),
	       ent->part[count].part_size);
    }

  // Write out the entries 
  status = gptWriteEntries(fd, header, entries);
  if (status)
    printf("Error %d writing GPT entries\n", status);

  // Write out the header 
  status = gptWriteHeader(fd, header);
  if (status)
    printf("Error %d writing GPT header\n", status);

  // Just to make sure, partition information updated on the disk. 
  fsync(fd);
  sleep(1);

  // Have the kernel re-read the partitions. 
  ioctl(fd, BLKRRPART);
  close(fd);

  free(header);
  free(orig_entries);
  free(entries);
  return (status);
}
