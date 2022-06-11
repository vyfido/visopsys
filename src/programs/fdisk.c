//
//  Visopsys
//  Copyright (C) 1998-2003 J. Andrew McLaughlin
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
//  fdisk.c
//

// This is a program for modifying the partition record.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/api.h>

static unsigned char diskMBR[512];


static int readMBR(int deviceNumber)
{
  // Read the MBR from the physical disk

  int status = 0;

  // Read the first sector of the device
  status = diskFunctionsReadAbsoluteSectors(deviceNumber, 0, 1, diskMBR);
  if (status < 0)
    return (status);

  return (status = 0);
}


static int writeMBR(int deviceNumber)
{
  // Read the MBR from the physical disk

  int status = 0;

  // Read the first sector of the device
  status = diskFunctionsWriteAbsoluteSectors(deviceNumber, 0, 1, diskMBR);
  if (status < 0)
    return (status);

  return (status = 0);
}


int main(int argc, char *argv[])
{
  int status = 0;
  int availableDisks = 0;
  int firstDisk = -1;
  int lastDisk = -1;
  int diskNumber = -1;
  unsigned long diskSize = 0;
  disk diskInfo;
  unsigned char character[2];
  unsigned char *partitionRecord = 0;
  unsigned char partitionType = 0;
  char *partitionDescription = NULL;
  unsigned char *activePartition = NULL;
  int availablePartitions = 0;
  int partition = 0;
  int count;

  // This structure is used to describe a known partition type
  typedef struct
  {
    unsigned char index;
    char *description;
  } partType;   

  // This is a table for keeping partition types
  static partType partitionTypes[] =
  {
    { 0x01, "FAT12"},
    { 0x04, "FAT16"},
    { 0x05, "Extended partition"},
    { 0x06, "FAT16"},
    { 0x07, "OS/2 HPFS, or NTFS"},
    { 0x0A, "OS/2 Boot Manager"},
    { 0x0B, "FAT32"},
    { 0x0C, "FAT32 (LBA)"},
    { 0x0E, "FAT16 (LBA)"},
    { 0x0F, "Extended partition (LBA)"},
    { 0x63, "GNU HURD"},
    { 0x80, "Minix"},
    { 0x81, "Linux or Minix"},
    { 0x82, "Linux swap or Solaris"},
    { 0x83, "Linux ext2"},
    { 0x87, "HPFS mirrored"},
    { 0xBE, "Solaris boot"},
    { 0xC7, "HPFS mirrored"},
    { 0xEB, "BeOS BFS"},
    { 0xF2, "DOS 3.3+ second partition"},
    { 0, 0 }
  };

  // Print a message
  printf("\nVisopsys FDISK Utility\nCopyright (C) 1998-2003 J. Andrew "
	 "McLaughlin\n\n");

  // Loop through the logical disks to get info about the physical devices

  printf("Please choose the disk on which to operate:\n");

  // Call the kernel to give us the number of available disks
  availableDisks = diskFunctionsGetCount();

  for (count = 0; count < availableDisks; count ++)
    {
      status = diskFunctionsGetInfo(count, &diskInfo);

      if (status < 0)
	{
	  // Eek.  Problem getting disk info
	  errno = status;
	  return (status);
	}

      if ((diskInfo.fixedRemovable == fixed) &&
	  (diskInfo.physicalDevice != lastDisk))
	{
	  if (firstDisk == -1)
	    firstDisk = diskInfo.physicalDevice;
	  lastDisk = diskInfo.physicalDevice;
	  
	  diskSize = (diskInfo.cylinders * diskInfo.heads * diskInfo.sectors *
		      diskInfo.sectorSize) / 1048576;

	  // Print disk info
	  printf("%d: %u Mbytes, cylinders: %u, heads: %u, sectors: %u\n",
		 diskInfo.physicalDevice, (unsigned) diskSize, 
		 diskInfo.cylinders, diskInfo.heads, diskInfo.sectors);
	}
    }

  printf("-> ");

  while(1)
    {
      character[0] = getchar();

      if (errno)
	{
	  // Eek.  We can't get input.  Quit.
	  perror(argv[0]);
	  return (status = errno);
	}
      
      if ((character[0] >= '0') && (character[0] <= '9'))
	{
	  character[1] = '\0';
	  printf("\n");
	  diskNumber = atoi(character);
	  if ((diskNumber < firstDisk) || (diskNumber > lastDisk))
	    {
	      printf("Invalid disk %d.\n-> ", diskNumber);
	      continue;
	    }
	  break;
	}
      else
	{
	  printf("\nNo disk selected.  Quitting.\n");
	  return (0);
	}
    }
  
  printf("\nOperating on disk %d.\n", diskNumber);

  // Read the MBR of the device
  status = readMBR(diskNumber);

  if (status < 0)
    {
      printf("\nUnable to read the MBR sector of the device.  Quitting.\n");
      return (status);
    }

  // Is this a valid MBR?
  if ((diskMBR[511] != (unsigned char) 0xAA) ||
      (diskMBR[510] != (unsigned char) 0x55))
    {
      // This is not a valid master boot record.
      printf("Invalid MBR on hard disk %d.  Quitting\n", diskNumber);
      return (status = ERR_INVALID);
    }
      
  // Set this pointer to the first partition record in the master
  // boot record
  partitionRecord = (diskMBR + 0x01BE);
  
  printf("\nPlease choose the partition to set active:\n");

  // Loop through the partition records, looking for non-zero entries
  for (partition = 0; partition < 4; partition ++)
    {
      partitionType = partitionRecord[4];
	  
      if (partitionType == 0)
	// The "rules" say we must be finished with this physical
	// device.
	break;

      partitionDescription = "Unsupported partition type";
      for (count = 0; partitionTypes[count].index != 0; count ++)
	if (partitionTypes[count].index == partitionType)
	  partitionDescription = partitionTypes[count].description;

      availablePartitions++;
 
      // Print info about the partition
      printf("%d: %s", partition, partitionDescription);

      if (partitionRecord[0] & 0x80)
	{
	  activePartition = partitionRecord;
	  printf(" (active)\n");
	}
      else
	printf("\n");

      // Move to the next partition record
      partitionRecord += 16;
    }
  
  printf("-> ");

  while(1)
    {
      character[0] = getchar();

      if (errno)
	{
	  // Eek.  We can't get input.  Quit.
	  perror(argv[0]);
	  return (status = errno);
	}
      
      if ((character[0] >= '0') && (character[0] <= '9'))
	{
	  character[1] = '\0';
	  printf("\n");
	  partition = atoi(character);
	  if (partition > (availablePartitions - 1))
	    {
	      printf("Invalid partition number %d.\n-> ", partition);
	      continue;
	    }
	  break;
	}
      else
	{
	  printf("\n\nNo partition selected.  Quitting.\n");
	  return (0);
	}
    }
  
  printf("\nSetting partition %d to active.  Are you SURE? (y/n): ",
	 partition);

  while(1)
    {
      character[0] = getchar();

      if (errno)
	{
	  // Eek.  We can't get input.  Quit.
	  perror(argv[0]);
	  return (status = errno);
	}

      if (character[0] == 'y')
	{
	  printf("\n");
	  break;
	}
      else if (character[0] == 'n')
	{
	  printf("\n\nQuitting.\n");
	  return (status = 0);
	}
      else
	{
	  textBackSpace();
	  continue;
	}
    }

  // Unset previous active partition
  activePartition[0] &= 0x7F;

  // Set new active partition
  partitionRecord = ((diskMBR + 0x01BE) + (16 * partition));
  partitionRecord[0] |= 0x80;

  // Write out the MBR
  status = writeMBR(diskNumber);

  if (status < 0)
    {
      printf("\nUnable to write the MBR sector of the device.  Quitting.\n");
      return (status);
    }

  printf("\nPartition %d now active.\n", partition);

  errno = 0;
  return (status = 0);
}
