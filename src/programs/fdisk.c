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

// This is a program for modifying the master boot record (MBR) and doing
// other disk management tasks.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/api.h>

#define BACKUP_MBR "/system/boot/backup.mbr"
#define PERM "You must be a privileged user to use this command."

#define ENTRYOFFSET_DRV_BOOTABLE  0
#define ENTRYOFFSET_START_HEAD    1
#define ENTRYOFFSET_START_CYLSECT 2
#define ENTRYOFFSET_START_CYL     3
#define ENTRYOFFSET_TYPE          4
#define ENTRYOFFSET_END_HEAD      5
#define ENTRYOFFSET_END_CYLSECT   6
#define ENTRYOFFSET_END_CYL       7
#define ENTRYOFFSET_START_LBA     8
#define ENTRYOFFSET_SIZE_LBA      12

static int processId = 0;
static int numberDisks = 0;
static disk diskInfo[DISK_MAXDEVICES];
static disk *selectedDisk = NULL;
static unsigned char diskMBR[512];
static unsigned char originalMBR[512];
static int numberPartitions = 0;
static int changesPending = 0;
static int backupAvailable = 0;

// GUI stuff
static int graphics = 0;
static objectKey window = NULL;
static objectKey quitButton = NULL;

typedef struct {
  int bootable;
  int drive;
  unsigned startCylinder;
  unsigned startHead;
  unsigned startSector;
  int type;
  unsigned endCylinder;
  unsigned endHead;
  unsigned endSector;
  unsigned startLogical;
  unsigned sizeLogical;
} partitionEntry;


static int yesOrNo(char *question)
{
  char character;

  if (graphics)
    {
      if (windowNewQueryDialog(window, "Confirmation", question))
	return (1);
      else
	return (0);
    }
  else
    {
      printf("\n%s (y/n): ", question);
      textInputSetEcho(0);

      while(1)
	{
	  character = getchar();
	  if (errno)
	    {
	      // Eek.  We can't get input.  Quit.
	      textInputSetEcho(1);
	      return (0);
	    }
      
	  if ((character == 'y') || (character == 'Y'))
	    {
	      printf("Yes\n");
	      textInputSetEcho(1);
	      return (1);
	    }
	  else if ((character == 'n') || (character == 'N'))
	    {
	      printf("No\n");
	      textInputSetEcho(1);
	      return (0);
	    }
	}
    }
}


static char readKey(const char *choices)
{
  int stringLength = strlen(choices);
  char character;
  int count;

  textInputSetEcho(0);

  while(1)
    {
      character = getchar();
      
      if (errno)
	{
	  // Eek.  We can't get input.  Quit.
	  textInputSetEcho(1);
	  return (0);
	}

      for (count = 0; count < stringLength; count ++)
	if (character == choices[count])
	  {
	    printf("%c\n", character);
	    textInputSetEcho(1);
	    return (character);
	  }
    }
}


static int readLine(const char *choices, char *buffer, int length)
{
  int choicesLength = strlen(choices);
  int count1, count2;

  textInputSetEcho(0);

  for (count1 = 0; count1 < (length - 1); count1 ++)
    {
      buffer[count1] = getchar();
      
      if (errno)
	{
	  // Eek.  We can't get input.  Quit.
	  textInputSetEcho(1);
	  return (ERR_INVALID);
	}

      if (buffer[count1] == 10) // Newline
	{
	  buffer[count1] = '\0';
	  textInputSetEcho(1);
	  return (count1);
	}
	
      if (buffer[count1] == 8) // Backspace
	{
	  if (count1 > 0)
	    {
	      textBackSpace();
	      count1 -= 2;
	    }
	  else
	    count1 -= 1;
	  continue;
	}

      for (count2 = 0; count2 < choicesLength; count2 ++)
	if (buffer[count1] == choices[count2])
	  {
	    printf("%c", buffer[count1]);
	    break;
	  }

      if (buffer[count1] != choices[count2])
	count1--;
    }

  // Reached the end of the buffer.
  buffer[length - 1] = '\0';
  return (0);
}


static int readMBR(const char *diskName)
{
  // Read the MBR from the physical disk

  int status = 0;
  char tmpChar[80];

  // Read the first sector of the device
  status = diskReadAbsoluteSectors(diskName, 0, 1, diskMBR);
  if (status < 0)
    return (status);

  // Is this a valid MBR?
  if ((diskMBR[511] != (unsigned char) 0xAA) ||
      (diskMBR[510] != (unsigned char) 0x55))
    {
      // This is not a valid master boot record.
      sprintf(tmpChar, "Invalid MBR on hard disk %s.", diskName);
      if (graphics)
	windowNewErrorDialog(window, "Warning", tmpChar);
      else
	printf("WARNING: %s\n", tmpChar);
    }

  // Save a copy of the original
  memcpy(originalMBR, diskMBR, 512);

  changesPending = 0;
  return (status = 0);
}


static int writeMBR(const char *diskName)
{
  // Write the MBR to the physical disk

  int status = 0;
  fileStream backupFile;

  // Make sure the signature is at the end
  diskMBR[510] = (unsigned char) 0x55;
  diskMBR[511] = (unsigned char) 0xAA;

  // Write a backup copy of the original MBR
  status = fileStreamOpen(BACKUP_MBR, (OPENMODE_WRITE | OPENMODE_CREATE |
				       OPENMODE_TRUNCATE), &backupFile);
  if (status >= 0)
    {
      status = fileStreamWrite(&backupFile, 512, originalMBR);
      if (status < 0)
	printf("WARNING: Error writing backup MBR file");
      else
	backupAvailable = 1;
      fileStreamClose(&backupFile);
    }
  else
    printf("WARNING: Error opening backup MBR file");

  // Write the first sector of the device
  status = diskWriteAbsoluteSectors(diskName, 0, 1, diskMBR);
  if (status < 0)
    return (status);

  // Tell the kernel to reexamine the partition tables
  status = diskReadPartitions();

  changesPending = 0;
  return (status = 0);
}


static int scanDisks(void)
{
  int status = 0;
  int tmpNumberDisks = 0;
  disk tmpDiskInfo[DISK_MAXDEVICES];
  int count;

  // Call the kernel to give us the number of available disks
  tmpNumberDisks = diskGetPhysicalCount();

  if (tmpNumberDisks <= 0)
    return (status = ERR_NOSUCHENTRY);

  // Read disk info into our temporary structure
  status = diskGetPhysicalInfo(tmpDiskInfo);
  if (status < 0)
    // Eek.  Problem getting disk info
    return (status);

  // Loop through these disks, figuring out which ones are hard disks
  // and putting them into the regular array
  for (count = 0; count < tmpNumberDisks; count ++)
    if (tmpDiskInfo[count].fixedRemovable == fixed)
      {
	memcpy(&diskInfo[numberDisks], &tmpDiskInfo[count], sizeof(disk));
	numberDisks ++;
      }

  if (numberDisks <= 0)
    return (status = ERR_NOSUCHENTRY);
  else
    return (status = 0);
}


static inline unsigned cylsToMb(disk *theDisk, unsigned cylinders)
{
  unsigned long tmpDiskSize = ((theDisk->heads * theDisk->sectorsPerCylinder *
				cylinders) / (1048576 / theDisk->sectorSize));
				 
  if (tmpDiskSize < 1)
    return (1);
  else
    return ((unsigned) tmpDiskSize);
}


static inline unsigned mbToCyls(disk *theDisk, unsigned megabytes)
{
  unsigned long tmpDiskSize = 0;

  tmpDiskSize = (((megabytes * 1048576) / theDisk->sectorSize) /
		 (theDisk->heads * theDisk->sectorsPerCylinder));

  if (tmpDiskSize < 1)
    return (1);
  else
    return ((unsigned) tmpDiskSize);
}


static void printDisks(void)
{
  int count;

  for (count = 0; count < numberDisks; count ++)
    {
      // Print disk info
      printf("  Disk %d: [%s] %u Mb, %u cyls, %u heads, %u secs/cyl, %u "
	     "bytes/sec\n", diskInfo[count].deviceNumber, diskInfo[count].name,
	     cylsToMb(&diskInfo[count], diskInfo[count].cylinders),
	     diskInfo[count].cylinders, diskInfo[count].heads, 
	     diskInfo[count].sectorsPerCylinder, diskInfo[count].sectorSize);
    }
}


static int selectDisk(void)
{
  int status = 0;
  int diskNumber = 0;
  char character[2];
  char tmpChar[80];
  int count;

  selectedDisk = NULL;
  
  printf("\n");
  printDisks();

  if (numberDisks > 1)
    while(1)
      {
	printf("\nPlease choose the disk on which to operate ('Q' to quit):"
	       "\n-> ");
	
	character[0] = readKey("0123456789Qq");
	if ((character[0] == 0) ||
	    (character[0] == 'Q') || (character[0] == 'q'))
	  return (ERR_INVALID);
	character[1] = '\0';
	
	diskNumber = atoi(character);
	
	// Loop through the disks and make sure it's legit
	for (count = 0; count < numberDisks; count ++)
	  if (diskInfo[count].deviceNumber == diskNumber)
	    selectedDisk = &diskInfo[count];
	
	if (selectedDisk != NULL)
	  break;

	printf("Invalid disk %d.\n", diskNumber);
      }
  else
    selectedDisk = &diskInfo[0];

  // Read the MBR of the device
  status = readMBR(selectedDisk->name);
  if (status < 0)
    {
      sprintf(tmpChar, "Unable to read the MBR sector of the device.  "
	      "Quitting.");
      if (graphics)
	windowNewErrorDialog(window, "Error", tmpChar);
      else
	printf("\n%s\n", tmpChar);
      errno = status;
      return (status);
    }

  return (status = 0);
}


static void getPartitionEntry(int partition, partitionEntry *entry)
{
  unsigned char *partRecord = 0;

  // Set this pointer to the partition record in the master boot record
  partRecord = ((diskMBR + 0x01BE) + (partition * 16));

  entry->bootable = (partRecord[ENTRYOFFSET_DRV_BOOTABLE] >> 7);
  entry->drive = (partRecord[ENTRYOFFSET_DRV_BOOTABLE] & 0x7F);
  entry->startHead = (unsigned) partRecord[ENTRYOFFSET_START_HEAD];
  entry->startCylinder = (unsigned) partRecord[ENTRYOFFSET_START_CYL];
  entry->startCylinder |=
    (((unsigned) partRecord[ENTRYOFFSET_START_CYLSECT] & 0xC0) << 2);
  entry->startSector = (unsigned)
    (partRecord[ENTRYOFFSET_START_CYLSECT] & 0x3F);
  entry->type = partRecord[ENTRYOFFSET_TYPE];
  entry->endHead = (unsigned) partRecord[ENTRYOFFSET_END_HEAD];
  entry->endCylinder = (unsigned) partRecord[ENTRYOFFSET_END_CYL];
  entry->endCylinder |=
    (((unsigned) partRecord[ENTRYOFFSET_END_CYLSECT] & 0xC0) << 2);
  entry->endSector = (unsigned) (partRecord[ENTRYOFFSET_END_CYLSECT] & 0x3F);
  entry->startLogical = (unsigned) ((unsigned *) partRecord)[2];
  entry->sizeLogical = (unsigned) ((unsigned *) partRecord)[3];

  // Now, check whether the start and end cylinder values seem correct.
  // If they are 'maxed out' and don't correspond with the LBA values,
  // recalculate them.
  if ((entry->startCylinder == 1023) || (entry->endCylinder == 1023))
    {
      entry->startCylinder =
	(entry->startLogical / (selectedDisk->heads *
				selectedDisk->sectorsPerCylinder));
      entry->endCylinder =
	((entry->startLogical + entry->sizeLogical - 1) /
	 (selectedDisk->heads * selectedDisk->sectorsPerCylinder));
    }
}


static void setPartitionEntry(int partition, partitionEntry *entry)
{
  unsigned char *partRecord = 0;
  unsigned startCyl, endCyl;

  startCyl = entry->startCylinder;
  endCyl = entry->endCylinder;

  // Check whether our start or end cylinder values exceed the legal
  // maximum of 1023.  If so, make them 1023
  if (startCyl > 1023)
    startCyl = 1023;
  if (endCyl > 1023)
    endCyl = 1023;

  // Set this pointer to the partition record in the master boot record
  partRecord = ((diskMBR + 0x01BE) + (partition * 16));

  partRecord[ENTRYOFFSET_DRV_BOOTABLE] =
    (unsigned char) ((entry->bootable << 7) | (entry->drive & 0x7F));
  partRecord[ENTRYOFFSET_START_HEAD] = (unsigned char) entry->startHead;
  partRecord[ENTRYOFFSET_START_CYLSECT] = (unsigned char)
    (((startCyl & 0x300) >> 2) | (entry->startSector & 0x3F));
  partRecord[ENTRYOFFSET_START_CYL] = (unsigned char) (startCyl & 0x0FF);
  partRecord[ENTRYOFFSET_TYPE] = entry->type;
  partRecord[ENTRYOFFSET_END_HEAD] = (unsigned char) entry->endHead;
  partRecord[ENTRYOFFSET_END_CYLSECT] = (unsigned char)
    (((endCyl & 0x300) >> 2) | (entry->endSector & 0x3F));
  partRecord[ENTRYOFFSET_END_CYL] = (unsigned char) (endCyl & 0x0FF);
  ((unsigned *) partRecord)[2] = entry->startLogical;
  ((unsigned *) partRecord)[3] = entry->sizeLogical;
}


static void printPartitions(void)
{
  int partition = 0;
  partitionEntry entry;
  partitionType partType;
  unsigned long partSize = 0;

  numberPartitions = 0;

  printf("\nPartitions on disk %d:              cylinders   size      "
	 "logical\n", selectedDisk->deviceNumber);

  // Loop through the partition records, looking for non-zero entries
  for (partition = 0; partition < 4; partition ++)
    {
      getPartitionEntry(partition, &entry);

      if (entry.type == 0)
	// The "rules" say we're supposed to be finished with this physical
	// device, but that's not absolutely always the case.  We'll still
	// probably be screwed, but not as screwed as if we were to quit here.
	continue;

      numberPartitions++;
 
      diskGetPartType(entry.type, &partType);
      
      partSize = (entry.sizeLogical / (1048576 / selectedDisk->sectorSize));

      // Print info about the partition
      printf("  %d: %s", partition, partType.description);
      textSetColumn(25);
      if (entry.bootable)
	printf(" (active)");
      textSetColumn(34);
      printf(" %u-%u", entry.startCylinder, entry.endCylinder);
      textSetColumn(46);
      printf(" %u Mb", (unsigned ) partSize);
      textSetColumn(56);
      printf(" %u-%u\n", entry.startLogical,
	     (entry.startLogical + entry.sizeLogical - 1));
    }

  if (numberPartitions == 0)
    printf("  (NO PARTITIONS)\n");
}


static int selectPartition(void)
{
  char character[2];
  int partition;

  if (numberPartitions > 1)
    {
      printf("Please select the partition for this operation (0-%d):\n-> ",
	     (numberPartitions - 1));

      textInputSetEcho(0);
      
      while(1)
	{
	  character[0] = getchar();
	  
	  if (errno)
	    {
	      // Eek.  We can't get input.  Quit.
	      textInputSetEcho(1);
	      return (errno);
	    }

	  if ((character[0] >= '0') && (character[0] <= '9'))
	    {
	      character[1] = '\0';
	      printf("%s\n", character);
	      partition = atoi(character);
	      if (partition > (numberPartitions - 1))
		{
		  printf("Invalid partition number %d.\n-> ", partition);
		  continue;
		}
	      textInputSetEcho(1);
	      return (partition);
	    }
	  else
	    {
	      textInputSetEcho(1);
	      return (ERR_INVALID);
	    }
	}
    }
  else
    return (partition = 0);
}


static void setActive(int newActive)
{
  partitionEntry entry;
  int partition = 0;

  // Loop through the partition records
  for (partition = 0; partition < 4; partition ++)
    {
      getPartitionEntry(partition, &entry);

      if (entry.type == 0)
	// The "rules" say we must be finished with this physical device.
	break;

      if (partition == newActive)
	// Set active flag
	entry.bootable = 1;
      else
	// Unset active flag
	entry.bootable = 0;
	
      setPartitionEntry(partition, &entry);
    }

  changesPending++;
}


static void delete(int partition)
{
  partitionEntry entries[4];
  char tmpChar[120];
  int count1, count2;

  // Get all the entries
  for (count1 = 0; count1 < numberPartitions; count1 ++)
    getPartitionEntry(count1, &entries[count1]);

  if (entries[partition].bootable)
    {
      sprintf(tmpChar, "Deleting active partition.  You should set another "
	      "partition active.");
      if (graphics)
	windowNewErrorDialog(window, "Warning", tmpChar);
      else
	printf("\n\n%s\n\n", tmpChar);
    }

  // Shift the remaining ones forward
  count2 = 0;
  for (count1 = 0; count1 < numberPartitions; count1 ++)
    if (count1 != partition)
      setPartitionEntry(count2++, &entries[count1]);
  numberPartitions -= 1;

  // For the deleted one, set the type to 0 and put it at the end
  entries[partition].type = 0;
  setPartitionEntry(numberPartitions, &entries[partition]);

  changesPending++;
}


static void printTypes(void)
{
  partitionType *types;
  int numberTypes = 0;
  int count;

  printf("\nSupported partition types:\n");

  // Get the list of types
  types = diskGetPartTypes();

  // Count them
  for (count = 0; (types[count].code != 0); count ++)
    numberTypes += 1;

  for (count = 0; count <= (numberTypes / 2); count ++)
    {
      printf("  %s%x  %s", (types[count].code < 16)? "0" : "",
	     types[count].code, types[count].description);
      textSetColumn(30);
      if ((count + (numberTypes / 2)) < numberTypes)
	printf("  %s%x  %s\n",
	       (types[count + (numberTypes / 2)].code < 16)? "0" : "",
	       types[count + (numberTypes / 2)].code,
	       types[count + (numberTypes / 2)].description);
    }
  printf("\n");
}


static int setType(int partition)
{
  int status = 0;
  char code[8];
  int newCode = 0;
  partitionEntry entry;
  partitionType *types;
  char tmpChar[80];
  int count;

  while(1)
    {
      printf("\nEnter the hexadecimal code to set as the type ('L' to "
	     "list, 'Q' to quit):\n-> ");

      status = readLine("0123456789AaBbCcDdEeFfLlQq", code, 8);
      if (status < 0)
	return (status);

      if ((code[0] == 'L') || (code[0] == 'l'))
	{
	  printTypes();
	  continue;
	}
      if ((code[0] == 'Q') || (code[0] == 'q'))
	return (status = ERR_NODATA);

      printf("\n");

      if (strlen(code) != 2)
	continue;

      // Turn it into a number
      newCode = xtoi(code);

      // Is it a supported type?
      types = diskGetPartTypes();

      for (count = 0; types[count].code != 0; count ++)
	if (types[count].code == newCode)
	  break;
      if (types[count].code == 0)
	{
	  sprintf(tmpChar, "Unsupported partition type %x", newCode);
	  if (graphics)
	    windowNewErrorDialog(window, "Error", tmpChar);
	  else
	    printf("\n%s", tmpChar);
	  return (status = ERR_INVALID);
	}

      // Load the partition
      getPartitionEntry(partition, &entry);

      // Change the value
      entry.type = newCode;

      // Set the partition
      setPartitionEntry(partition, &entry);

      // Done
      changesPending++;
      return (status = 0);
    }
}


static int create(void)
{
  typedef struct {
    unsigned startCyl;
    unsigned endCyl;
  } emptySpace;

  enum { units_normal, units_mb, units_cylsize } units = units_normal;

  int status = 0;
  char number[10];
  emptySpace empties[5];
  int numberEmpties = 0;
  int realNumberEmpties = 0;
  emptySpace *selectedEmpty = NULL;
  partitionEntry newEntry;
  char tmpChar[80];
  int count1, count2;

  if (numberPartitions >= 4)
    {
      sprintf(tmpChar, "The partition table is full");
      if (graphics)
	windowNewErrorDialog(window, "Error", tmpChar);
      else
	printf("\n%s", tmpChar);
      return (status = ERR_NOFREE);
    }

  for (count1 = 0; count1 < (sizeof(emptySpace) * 5); count1 ++)
    ((char *) empties)[count1] = '\0';

  empties[0].startCyl = 0;
  empties[0].endCyl = (selectedDisk->cylinders - 1);
  numberEmpties = 1;

  // Loop through the partitions and find empty spaces
  for (count1 = 0; count1 < numberPartitions; count1 ++)
    {
      getPartitionEntry(count1, &newEntry);

      for (count2 = 0; count2 < numberEmpties; count2 ++)
	if ((newEntry.startCylinder >= empties[count2].startCyl) &&
	    (newEntry.endCylinder <= empties[count2].endCyl))
	  {
	    if ((newEntry.startCylinder > empties[count2].startCyl) &&
		(newEntry.endCylinder < empties[count2].endCyl))
	      {
		// The entry is right inside the empty space
		empties[numberEmpties].startCyl = (newEntry.endCylinder + 1);
		empties[numberEmpties].endCyl = empties[count2].endCyl;
		numberEmpties++;
		empties[count2].endCyl = (newEntry.startCylinder - 1);
	      }
	    else
	      {
		// The entry starts and/or ends at the same spot(s) as the
		// empty space
		if (newEntry.startCylinder == empties[count2].startCyl)
		  {
		    if (newEntry.startCylinder < empties[count2].endCyl)
		      empties[count2].startCyl = (newEntry.endCylinder + 1);
		    else
		      {
			empties[count2].startCyl = -1;
			empties[count2].endCyl = -1;
		      }
		  }
		if (newEntry.endCylinder == empties[count2].endCyl)
		  {
		    if (newEntry.startCylinder > empties[count2].startCyl)
		      empties[count2].endCyl = (newEntry.startCylinder - 1);
		    else
		      {
			empties[count2].startCyl = -1;
			empties[count2].endCyl = -1;
		      }
		  }
	      }
	  }
    }

  // Print the empty spaces
  printf("\nUnallocated spaces on the disk:\n");
  for (count1 = 0; count1 < numberEmpties; count1 ++)
    if ((empties[count1].startCyl != -1) && (empties[count1].endCyl != -1))
      {
	printf("  cylinder %u -> cylinder %u  (%u Mb)\n",
	       empties[count1].startCyl, empties[count1].endCyl,
	       cylsToMb(selectedDisk, (empties[count1].endCyl -
				       empties[count1].startCyl + 1)));
	realNumberEmpties++;
    }

  if (realNumberEmpties == 0)
    {
      sprintf(tmpChar, "No free space on the disk");
      if (graphics)
	windowNewErrorDialog(window, "Error", tmpChar);
      else
	printf("\n%s", tmpChar);
      return (status = ERR_NOFREE);
    }

  newEntry.bootable = 0;
  newEntry.drive = selectedDisk->deviceNumber;
  newEntry.type = 0x01;

  while (1)
    {
      printf("\nEnter starting cylinder (");
      for (count1 = 0; count1 < numberEmpties; count1 ++)
	if ((empties[count1].startCyl != -1) && (empties[count1].endCyl != -1))
	  printf("%u-%u; ", empties[count1].startCyl, empties[count1].endCyl);
      printf("'Q' to quit):\n-> ");

      status = readLine("0123456789Qq", number, 10);
      if (status < 0)
	continue;

      if ((number[0] == 'Q') || (number[0] == 'q'))
	return (status = ERR_INVALID);

      newEntry.startCylinder = atoi(number);

      // Find the empty space that this resides in
      for (count1 = 0; count1 < numberEmpties; count1++)
	if ((newEntry.startCylinder >= empties[count1].startCyl) &&
	    (newEntry.startCylinder <= empties[count1].endCyl))
	  {
	    selectedEmpty = &empties[count1];
	    break;
	  }

      if (selectedEmpty == NULL)
	{
	  sprintf(tmpChar, "Starting cylinder is not in unallocated space");
	  if (graphics)
	    windowNewErrorDialog(window, "Error", tmpChar);
	  else
	    printf("\n%s\n", tmpChar);
	  continue;
	}

      // Don't write cylinder 0, track 0
      if (newEntry.startCylinder == 0)
	newEntry.startHead = 1;
      else
	newEntry.startHead = 0;

      newEntry.startSector = 1;
      newEntry.startLogical =
	((newEntry.startCylinder * selectedDisk->heads *
	  selectedDisk->sectorsPerCylinder) +
	 (newEntry.startHead * selectedDisk->sectorsPerCylinder));
      break;
    }

  while (1)
    {
      printf("\nEnter ending cylinder (%u-%u),\n"
	     "or size in megabytes with 'm' (1m-%um),\n"
	     "or size in cylinders with 'c' (1c-%uc),\n"
	     "or 'Q' to quit:\n-> ",
	     newEntry.startCylinder, selectedEmpty->endCyl,
	     cylsToMb(selectedDisk,
		      (selectedEmpty->endCyl - newEntry.startCylinder + 1)),
	     (selectedEmpty->endCyl - newEntry.startCylinder + 1));
      
      status = readLine("0123456789CcMmQq", number, 10);
      if (status < 0)
	return (status);

      if ((number[0] == 'Q') || (number[0] == 'q'))
	return (status = ERR_INVALID);

      count1 = (strlen(number) - 1);

      if ((number[count1] == 'M') || (number[count1] == 'm'))
	{
	  units = units_mb;
	  number[count1] = '\0';
	}
      else if ((number[count1] == 'C') || (number[count1] == 'c'))
	{
	  units = units_cylsize;
	  number[count1] = '\0';
	}

      count1 = atoi(number);

      switch (units)
	{
	case units_mb:
	  newEntry.endCylinder =
	    (newEntry.startCylinder + mbToCyls(selectedDisk, count1) - 1);
	  break;
	case units_cylsize:
	  newEntry.endCylinder = (newEntry.startCylinder + count1 - 1);
	  break;
	default:
	  newEntry.endCylinder = count1;
	  break;
	}

      if ((newEntry.endCylinder < newEntry.startCylinder) ||
	  (newEntry.endCylinder > selectedEmpty->endCyl))
	{
	  sprintf(tmpChar, "Invalid cylinder number");
	  if (graphics)
	    windowNewErrorDialog(window, "Error", tmpChar);
	  else
	    printf("\n%s\n", tmpChar);
	  continue;
	}

      newEntry.endHead = (selectedDisk->heads - 1);
      newEntry.endSector = selectedDisk->sectorsPerCylinder;
      newEntry.sizeLogical =   
	((((newEntry.endCylinder - newEntry.startCylinder) + 1) *
	  (selectedDisk->heads * selectedDisk->sectorsPerCylinder)) -
	 (newEntry.startHead * selectedDisk->sectorsPerCylinder));
      break;
    }

  // Set the new entry before calling setType, since it needs the entry
  // to be in the table.
  setPartitionEntry(numberPartitions, &newEntry);

  status = setType(numberPartitions);
  if (status >= 0)
    {
      numberPartitions++;
      return (status = 0);
    }
  else
    {
      newEntry.type = 0;
      setPartitionEntry(numberPartitions, &newEntry);
      return (status);
    }
}


static void deleteAll(void)
{
  partitionEntry blankEntry;
  int count;

  for (count = 0; count < sizeof(partitionEntry); count ++)
    ((char *) &blankEntry)[count] = '\0';

  for (count = 0; count < 4; count ++)
    setPartitionEntry(count, &blankEntry);

  changesPending++;
}


static int copyDisk(void)
{
  int status = 0;
  int diskNumber = 0;
  disk *srcDisk = NULL;
  disk *destDisk = NULL;
  char character[2];
  partitionEntry entry;
  unsigned lastUsedSector = 0;
  unsigned char *buffer = NULL;
  unsigned srcSectorsPerOp = 0;
  unsigned destSectorsPerOp = 0;
  unsigned srcSector = 0;
  unsigned destSector = 0;
  char tmpChar[160];
  int count;

#define COPYBUFFER_SIZE 1048576 // 1 Meg

  if (numberDisks < 2)
    {
      sprintf(tmpChar, "No other disks to copy to");
      if (graphics)
	windowNewErrorDialog(window, "Error", tmpChar);
      else
	printf("\n%s\n", tmpChar);
      return (status = ERR_NOSUCHENTRY);
    }

  srcDisk = selectedDisk;

  while(1)
    {
      printf("\nPlease choose the disk to copy to ('Q' to quit):\n");
      printDisks();
      printf("\n->");
      
      character[0] = readKey("0123456789Qq");
      if ((character[0] == 0) ||
	  (character[0] == 'Q') || (character[0] == 'q'))
	return (status = 0);
      character[1] = '\0';
      
      diskNumber = atoi(character);
      
      // Loop through the disks and make sure it's legit
      for (count = 0; count < numberDisks; count ++)
	if (diskInfo[count].deviceNumber == diskNumber)
	  destDisk = &diskInfo[count];
      
      if (destDisk == srcDisk)
	{
	  sprintf(tmpChar, "Not much point in copying a disk to itself!");
	  if (graphics)
	    windowNewErrorDialog(window, "Error", tmpChar);
	  else
	    printf("\n%s\n", tmpChar);
	  continue;
	}

      else if (destDisk != NULL)
	break;
      
      sprintf(tmpChar, "Invalid disk %d.", diskNumber);
      if (graphics)
	windowNewErrorDialog(window, "Error", tmpChar);
      else
	printf("\n%s\n", tmpChar);
    }

  // We have a source disk and a destination disk.
  sprintf(tmpChar, "Copy disk %s to disk %s.  WARNING: THIS WILL DESTROY ALL "
	  "DATA ON DISK %s.\nARE YOU SURE YOU WANT TO DO THIS?", srcDisk->name,
	  destDisk->name, destDisk->name);
  if (!yesOrNo(tmpChar))
    return (status = 0);
  printf("\n");

  // We will copy everything up to the end of the last partition (not much
  // point in copying a bunch of unused space, even though it's potentially
  // conceivable that someone, somewhere might want to do that).  Find out
  // the logical sector number of the end of the last partition.
  for (count = 0; count < numberPartitions; count ++)
    {
      getPartitionEntry(count, &entry);

      if ((entry.startLogical + entry.sizeLogical - 1) > lastUsedSector)
	lastUsedSector = (entry.startLogical + entry.sizeLogical - 1);
    }

  if (lastUsedSector == 0)
    {
      if (!yesOrNo("No partitions on the disk.  Do you want to copy the "
		   "whole\ndisk anyway?"))
	return (status = 0);
      printf("\n");

      lastUsedSector = (srcDisk->numSectors - 1);
    }

  // Make sure that the destination disk can hold the data
  if (lastUsedSector >= destDisk->numSectors)
    {
      sprintf(tmpChar, "Disk %s is smaller than the amount of data on disk "
	      "%s.  If you wish, you\ncan continue and copy the data that "
	      "will fit.  Don't do this unless you're\npretty sure you know "
	      "what you're doing.  CONTINUE?", destDisk->name, srcDisk->name);
      if (!yesOrNo(tmpChar))
	return (status = 0);
      printf("\n");

      lastUsedSector = (destDisk->numSectors - 1);
    }

  // Get a decent memory buffer to copy data to/from
  buffer = memoryGet(COPYBUFFER_SIZE, "disk copy buffer");
  if (buffer == NULL)
    {
      sprintf(tmpChar, "Unable to allocate memory buffer!");
      if (graphics)
	windowNewErrorDialog(window, "Error", tmpChar);
      else
	printf("%s\n", tmpChar);
      return (status = ERR_MEMORY);
    }

  // Calculate the sectors per operation for each disk
  srcSectorsPerOp = (COPYBUFFER_SIZE / srcDisk->sectorSize);
  destSectorsPerOp = (COPYBUFFER_SIZE / destDisk->sectorSize);

  printf("\nCopying %u sectors, %u Mb\n", (lastUsedSector + 1),
	 ((lastUsedSector + 1) / (1048576 / srcDisk->sectorSize)));

  // Copy the data
  while (srcSector < lastUsedSector)
    {
      textSetColumn(2);
      printf("%d%% ", ((srcSector * 100) / lastUsedSector));

      // For the last op, reset the 'sectorsPerOp' values
      if (((srcSector + srcSectorsPerOp) - 1) > lastUsedSector)
	{
	  srcSectorsPerOp = ((lastUsedSector - srcSector) + 1);
	  destSectorsPerOp = ((srcSectorsPerOp * srcDisk->sectorSize) /
			      destDisk->sectorSize);
	}

      // Read from source
      status = diskReadAbsoluteSectors(srcDisk->name, srcSector, 
				       srcSectorsPerOp, buffer);
      if (status < 0)
	{
	  sprintf(tmpChar, "Read error %d reading sectors %u-%u from disk %s",
		  status, srcSector, (srcSector + srcSectorsPerOp - 1),
		  srcDisk->name);
	  if (graphics)
	    windowNewErrorDialog(window, "Error", tmpChar);
	  else
	    printf("%s\n", tmpChar);
	  memoryRelease(buffer);
	  return (status);
	}

      // Write to destination
      status = diskWriteAbsoluteSectors(destDisk->name, destSector, 
					destSectorsPerOp, buffer);
      if (status < 0)
	{
	  sprintf(tmpChar, "Write error %d writing sectors %u-%u to disk %s",
		  status, destSector, (destSector + destSectorsPerOp - 1),
		  destDisk->name);
	  if (graphics)
	    windowNewErrorDialog(window, "Error", tmpChar);
	  else
	    printf("%s\n", tmpChar);
	  memoryRelease(buffer);
	  return (status);
	}

      srcSector += srcSectorsPerOp;
      destSector += destSectorsPerOp;
    }

  textSetColumn(2);
  printf("100%%\n");

  memoryRelease(buffer);
  return (status = 0);
}


static int restoreBackup(const char *fileName)
{
  // Restore the backed-up MBR from a file

  int status = 0;
  fileStream backupFile;
  char tmpChar[80];

  // Read a backup copy of the MBR
  status = fileStreamOpen(fileName, OPENMODE_READ, &backupFile);
  if (status >= 0)
    {
      status = fileStreamRead(&backupFile, 512, diskMBR);
      if (status < 0)
	{
	  sprintf(tmpChar, "Error reading backup MBR file");
	  if (graphics)
	    windowNewErrorDialog(window, "Warning", tmpChar);
	  else
	    printf("WARNING: %s", tmpChar);
	}
      fileStreamClose(&backupFile);
    }
  else
    {
      sprintf(tmpChar, "Error opening backup MBR file");
      if (graphics)
	windowNewErrorDialog(window, "Warning", tmpChar);
      else
      printf("WARNING: %s", tmpChar);
    }

  // Don't write it.  The user has to do that explicitly.

  changesPending++;

  return (status = 0);
}


static void quit(void)
{
  // Shut everything down

  if (changesPending && !yesOrNo("Quit without writing changes?"))
    return;

  errno = 0;

  if (graphics)
    {
      windowGuiStop();
      windowManagerDestroyWindow(window);
    }
  else
    printf("\nQuitting.\n");

  exit(0);
}


static void eventHandler(objectKey key, windowEvent *event)
{
  // Check for the window being closed by a GUI event.
  if (((key == window) && (event->type == EVENT_WINDOW_CLOSE)) ||
      ((key == quitButton) && (event->type == EVENT_MOUSE_UP)))
    {
      if (changesPending && !yesOrNo("Quit without writing changes?"))
	return;
      windowManagerDestroyWindow(window);
      multitaskerKillProcess(processId, 0 /* no force */);
    }
}


static void constructWindow(void)
{
  // If we are in graphics mode, make a window rather than operating on the
  // command line.

  int status = 0;
  componentParameters params;
  static image iconImage;
  objectKey imageComponent = NULL;
  objectKey textLabel = NULL;
  objectKey textArea = NULL;

  // Create a new window, with small, arbitrary size and location
  window = windowManagerNewWindow(processId, "Disk Manager", 0, 0, 400, 400);
  if (window == NULL)
    return;

  params.gridX = 0;
  params.gridY = 0;
  params.gridHeight = 1;
  params.padLeft = 5;
  params.padRight = 5;
  params.padTop = 5;
  params.padBottom = 0;
  params.orientationY = orient_middle;
  params.hasBorder = 0;
  params.useDefaultForeground = 0;
  params.foreground.red = 40;
  params.foreground.green = 93;
  params.foreground.blue = 171;
  params.useDefaultBackground = 1;

  if (iconImage.data == NULL)
    // Try to load an icon image to go at the top of the window
    status = imageLoadBmp("/system/diskicon.bmp", &iconImage);

  if (status == 0)
    {
      // Create an image component from it, and add it to the window
      params.gridX = 0;
      params.gridWidth = 1;
      params.orientationX = orient_right;
      iconImage.isTranslucent = 1;
      iconImage.translucentColor.red = 0;
      iconImage.translucentColor.green = 255;
      iconImage.translucentColor.blue = 0;
      imageComponent = windowNewImage(window, &iconImage);
      windowAddClientComponent(window, imageComponent, &params);
    }

  // Put text labels in the window
  textLabel = windowNewTextLabel(window, NULL, "Visopsys Disk Manager");
  if (textLabel != NULL)
    {
      // Put it in the client area of the window
      params.gridX = 1;
      params.gridWidth = 1;
      params.orientationX = orient_left;
      windowAddClientComponent(window, textLabel, &params);
    }

  // Put a text area where we do all our interaction
  textArea = windowNewTextArea(window, 80, 20, NULL /*font*/);
  if (textArea != NULL)
    {
      // Put it in the client area of the window
      params.gridX = 0;
      params.gridY = 1;
      params.gridWidth = 2;
      params.orientationX = orient_center;
      params.padTop = 5;
      params.padBottom = 5;
      params.hasBorder = 1;
      params.useDefaultBackground = 0;
      params.background.red = 255;
      params.background.green = 255;
      params.background.blue = 255;
      windowAddClientComponent(window, textArea, &params);
      windowRegisterEventHandler(textArea, &eventHandler);
    }

  // Create a 'quit' button
  quitButton = windowNewButton(window, 20, 20,
			       windowNewTextLabel(window, NULL, "Quit"), NULL);
  if (quitButton != NULL)
    {
      // Put it in the client area of the window
      params.gridY = 2;
      params.gridWidth = 2;
      params.padTop = 5;
      params.padBottom = 5;
      params.orientationX = orient_center;
      params.hasBorder = 0;
      params.foreground.red = 40;
      params.foreground.green = 93;
      params.foreground.blue = 171;
      params.useDefaultBackground = 1;
      windowAddClientComponent(window, quitButton, &params);
      windowRegisterEventHandler(quitButton, &eventHandler);
    }

  // Lay out and autosize the window to fit
  windowLayout(window);
  windowAutoSize(window);
  windowCenter(window);

  // Set the text area to be our input and output stream
  windowManagerSetTextOutput(textArea);

  // Register an event handler to catch window close events
  windowRegisterEventHandler(window, &eventHandler);

  // Run the GUI as a thread
  windowGuiThread();

  // Go
  windowSetVisible(window, 1);

  return;
}


int main(int argc, char *argv[])
{
  int status = 0;
  char character;
  int partition = -1;
  char tmpChar[160];

  processId = multitaskerGetCurrentProcessId();

  // Are graphics enabled?
  graphics = graphicsAreEnabled();
  if (graphics)
    constructWindow();

  else
    // Print a message
    printf("\nVisopsys FDISK Utility\nCopyright (C) 1998-2003 J. Andrew "
	   "McLaughlin\n");
  
  // Check privilege level
  if (multitaskerGetProcessPrivilege(processId) != 0)
    {
      printf("\n%s\n(Try logging in as user \"admin\")\n\n", PERM);
      if (graphics)
	{
	  windowGuiStop();
	  windowNewErrorDialog(window, "Permission Denied", PERM);
	  windowManagerDestroyWindow(window);
	}
      return (errno = ERR_PERMISSION);
    }

  // Any backup MBR saved?
  file tmpFile;
  if (!fileFind(BACKUP_MBR, &tmpFile))
    backupAvailable = 1;

  // Gather the disk info
  status = scanDisks();
  if (status < 0)
    {
      if (status == ERR_NOSUCHENTRY)
	sprintf(tmpChar, "No hard disks registered");
      else
	sprintf(tmpChar, "Problem getting hard disk info");
      if (graphics)
	windowNewErrorDialog(window, "Error", tmpChar);
      else
	printf("\n\n%s\n\n", tmpChar);
      errno = status;
      return (status);
    }

  status = selectDisk();
  if (status < 0)
    {
      sprintf(tmpChar, "No disk selected.  Quitting.");
      if (graphics)
	windowNewErrorDialog(window, "Error", tmpChar);
      else
	printf("\n\n%s\n\n", tmpChar);
      quit();
    }

  // This is the main menu bit
  while (1)
    {
      // Print out the partitions
      printPartitions();

      // Print out the menu choices
      printf(
"\n[A] Set active     [C] Copy disk  [D] Delete       [L] List types  [N] New"
"\n[O] Delete all     [Q] Quit       [S] Select disk  [T] Set type    [U] Undo"
"\n[W] Write changes  %s\n", (backupAvailable? "[R] Restore backup" : ""));
      if (changesPending)
	printf("  -== %d changes pending ==-\n", changesPending);
      printf("-> ");

      if (backupAvailable)
	character = readKey("AacCDdLlNnOoRrSsQqTtUuWw");
      else
	character = readKey("AacCDdLlNnOoSsQqTtUuWw");
      if (character == 0)
	continue;

      switch (character)
	{
	case 'a':
	case 'A':
	  partition = selectPartition();
	  if (partition >= 0)
	    {
	      setActive(partition);
	      sprintf(tmpChar, "Partition %d now active.", partition);
	      if (graphics)
		windowNewInfoDialog(window, "Success", tmpChar);
	      else
		printf("\n%s\n\n", tmpChar);
	    }
	  else
	    {
	      sprintf(tmpChar, "No partition selected.");
	      if (graphics)
		windowNewErrorDialog(window, "Error", tmpChar);
	      else
		printf("\n\n%s\n\n", tmpChar);
	    }
	  continue;

	case 'c':
	case 'C':
	  status = copyDisk();
	  if (status < 0)
	    {
	      sprintf(tmpChar, "Disk copy failed.");
	      if (graphics)
		windowNewErrorDialog(window, "Error", tmpChar);
	      else
		printf("\n\n%s\n\n", tmpChar);
	    }
	  continue;

	case 'd':
	case 'D':
	  partition = selectPartition();
	  if (partition >= 0)
	    {
	      delete(partition);
	      sprintf(tmpChar, "Partition %d deleted.", partition);
	      if (graphics)
		windowNewInfoDialog(window, "Success", tmpChar);
	      else
		printf("\n%s\n\n", tmpChar);
	    }
	  else
	    {
	      sprintf(tmpChar, "No partition selected.");
	      if (graphics)
		windowNewErrorDialog(window, "Error", tmpChar);
	      else
		printf("\n\n%s\n\n", tmpChar);
	    }
	  continue;

	case 'l':
	case 'L':
	  printTypes();
	  continue;

	case 'n':
	case 'N':
	  status = create();
	  if (status < 0)
	    {
	      sprintf(tmpChar, "Error creating partition.");
	      if (graphics)
		windowNewErrorDialog(window, "Error", tmpChar);
	      else
		printf("\n\n%s\n\n", tmpChar);
	    }
	  continue;

	case 'o':
	case 'O':
	  deleteAll();
	  continue;

	case 'q':
	case 'Q':
	  quit();
	  continue;
	      
	case 'r':
	case 'R':
	  if (!yesOrNo("Restore old MBR from backup?"))
	    continue;
	  status = restoreBackup(BACKUP_MBR);
	  if (status < 0)
	    {
	      sprintf(tmpChar, "Error restoring backup.");
	      if (graphics)
		windowNewErrorDialog(window, "Error", tmpChar);
	      else
		printf("\n\n%s\n\n", tmpChar);
	    }
	  continue;
	      
	case 's':
	case 'S':
	  if (changesPending)
	    {
	      sprintf(tmpChar, "Discard changes to disk %d?",
		      selectedDisk->deviceNumber);
	      if (!yesOrNo(tmpChar))
		continue;
	    }
	  status = selectDisk();
	  if (status < 0)
	    {
	      sprintf(tmpChar, "No disk selected.  Quitting.");
	      if (graphics)
		windowNewErrorDialog(window, "Error", tmpChar);
	      else
		printf("\n\n%s\n\n", tmpChar);
	      quit();
	    }
	  continue;

	case 't':
	case 'T':
	  partition = selectPartition();
	  if (partition >= 0)
	    {
	      status = setType(partition);
	      if (status < 0)
		{
		  if (status == ERR_NODATA)
		    printf("\n\n");
		  else
		    {
		      sprintf(tmpChar, "Invalid type entered");
		      if (graphics)
			windowNewErrorDialog(window, "Error", tmpChar);
		      else
			printf("\n\n%s\n\n", tmpChar);
		    }
		}
	      else
		{
		  sprintf(tmpChar, "Partition %d changed.", partition);
		  if (graphics)
		    windowNewInfoDialog(window, "Success", tmpChar);
		  else
		    printf("\n%s\n\n", tmpChar);
		}
	    }
	  else
	    {
	      sprintf(tmpChar, "No partition selected.");
	      if (graphics)
		windowNewErrorDialog(window, "Error", tmpChar);
	      else
		printf("\n\n%s\n\n", tmpChar);
	    }
	  continue;

	case 'u':
	case 'U':
	  if (changesPending)
	    {
	      memcpy(diskMBR, originalMBR, 512);
	      changesPending = 0;
	    }
	  continue;
	  
	case 'w':
	case 'W':
	  if (changesPending)
	    {
	      if (!yesOrNo("Committing changes to disk.  Are you SURE?"))
		continue;
	      // Write out the MBR
	      status = writeMBR(selectedDisk->name);

	      diskSync();

	      if (status < 0)
		{
		  sprintf(tmpChar, "Unable to write the MBR sector of the "
			  "device.");
		  if (graphics)
		    windowNewErrorDialog(window, "Error", tmpChar);
		  else
		    printf("\n\n%s\n\n", tmpChar);
		}
	      else
		{
		  sprintf(tmpChar, "CHANGES WRITTEN.");
		  if (graphics)
		    windowNewInfoDialog(window, "Success", tmpChar);
		  else
		  printf("\n%s\n\n", tmpChar);
		}
	    }
	  continue;
	  
	default:
	  continue;
	}
    }
}
