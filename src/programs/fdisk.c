//
//  Visopsys
//  Copyright (C) 1998-2005 J. Andrew McLaughlin
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

// This is a program for modifying partition tables and doing other disk
// management tasks.

/* This is the text that appears when a user requests help about this program
<help>

 -- fdisk --

Also known as the "Disk Manager".  This program can be used to perform
maintenance on fixed disks.

Usage:
  fdisk [-T] [-o <disk name>] [disk name]

The fdisk program is interactive, and can be used in either text or graphics
mode.  It provides the same functionality in both modes but graphics mode
will tend to be more fluid and user-friendly.  Development of this program
is ongoing and will be a major focus of improvements in future releases,
aiming towards providing much of the same functionality of PartitionMagic
and similar utilities.  If the disk name can be specified as the last
argument.

Options:

-T  Force text mode operation
-o <disk name>  Clear the partition table of the specified disk

</help>
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/api.h>
#include <sys/vsh.h>
#include "fdisk.h"

static const char *programName = NULL;
static int processId = 0;
static int readOnly = 1;
static char sliceListHeader[SLICESTRING_LENGTH + 1];
static int numberDisks = 0;
static disk *diskInfo = NULL;
static char *diskStringData = NULL;
static char *diskStrings[DISK_MAXDEVICES];
static int selectedDiskNumber = 0;
static disk *selectedDisk = NULL;
static unsigned cylinderSectors = 0;
static partitionTable *mainTable = NULL;
static int numberPartitions = 0;
static int numberDataPartitions = 0;
static int extendedStartSector = 0;
static slice *slices = NULL;
static int numberSlices;
static int selectedSlice = 0;
static int changesPending = 0;
static char *tmpBackupName = NULL;
static int backupAvailable = 0;
static ioThreadArgs readerArgs;
static ioThreadArgs writerArgs;
static int ioThreadsTerminate = 0;
static int ioThreadsFinished = 0;

// GUI stuff
static int graphics = 0;
static objectKey window = NULL;
static objectKey menuWrite = NULL;
static objectKey menuUndo = NULL;
static objectKey menuRestoreBackup = NULL;
static objectKey menuQuit = NULL;
static objectKey menuCopyDisk = NULL;
static objectKey menuPartOrder = NULL;
static objectKey menuSetActive = NULL;
static objectKey menuDelete = NULL;
static objectKey menuFormat = NULL;
static objectKey menuHide = NULL;
static objectKey menuInfo = NULL;
static objectKey menuListTypes = NULL;
static objectKey menuMove = NULL;
static objectKey menuNew = NULL;
static objectKey menuDeleteAll = NULL;
static objectKey menuSetType = NULL;
static objectKey diskList = NULL;
static objectKey canvas = NULL;
static objectKey sliceList = NULL;
static objectKey writeButton = NULL;
static objectKey undoButton = NULL;
static objectKey copyDiskButton = NULL;
static objectKey setActiveButton = NULL;
static objectKey deleteButton = NULL;
static objectKey deleteAllButton = NULL;
static objectKey formatButton = NULL;
static objectKey hideButton = NULL;
static objectKey infoButton = NULL;
static objectKey moveButton = NULL;
static objectKey newButton = NULL;
static objectKey setTypeButton = NULL;
static unsigned canvasWidth = 600;
static unsigned canvasHeight = 60;


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


static int quit(int force)
{
  // Shut everything down

  if (!force && changesPending && !yesOrNo("Quit without writing changes?"))
    return (0);

  errno = 0;

  if (graphics)
    {
      windowGuiStop();
      windowDestroy(window);
    }
  else
    {
      textScreenRestore();
      printf("\nQuitting.\n");
    }

  if (tmpBackupName != NULL)
    {
      fileDelete(tmpBackupName);
      tmpBackupName = NULL;
    }

  return (1);
}


static int getNumberDialog(const char *title, const char *prompt)
{
  // Creates a dialog that prompts for a number value

  int status = 0;
  char buffer[11];

  status = windowNewPromptDialog(window, title, prompt, 1, 10, buffer);
  if (status < 0)
    return (status);

  if (buffer[0] == '\0')
    return (status = ERR_NODATA);

  // Try to turn it into a number
  buffer[10] = '\0';
  status = atoi(buffer);
  return (status);
}


static char readKey(const char *choices, int allowCursor)
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

      if (allowCursor &&
	  ((character == (unsigned char) 17) ||
	   (character == (unsigned char) 20)))
	return (character);

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


static void pause(void)
{
  printf("\nPress any key to continue. ");
  getchar();
  printf("\n");
}


static void error(const char *format, ...)
{
  // Generic error message code for either text or graphics modes
  
  va_list list;
  char output[MAXSTRINGLENGTH];
  
  va_start(list, format);
  _expandFormatString(output, format, list);
  va_end(list);

  if (graphics)
    windowNewErrorDialog(window, "Error", output);
  else
    {
      printf("\n\n%s\n", output);
      pause();
    }
}


static void warning(const char *format, ...)
{
  // Generic error message code for either text or graphics modes

  va_list list;
  char output[MAXSTRINGLENGTH];
  
  va_start(list, format);
  _expandFormatString(output, format, list);
  va_end(list);

  if (graphics)
    windowNewErrorDialog(window, "Warning", output);
  else
    {
      printf("\n\nWARNING: %s\n", output);
      pause();
    }
}


static partitionTable *getPartitionTable(partitionTable *table, int sliceId)
{
  // Look for the table that the partition belongs to, starting with the
  // supplied table.  If necessary we recurse into extended partitions.

  partitionTable *returnTable = NULL;
  int count;

  for (count = 0; count < table->numberEntries; count ++)
    {
      if (table->entries[count].sliceId == sliceId)
        return (returnTable = table);

      else if (table->entries[count].entryType == partition_extended)
        {
          returnTable =
	    getPartitionTable(table->entries[count].extendedTable, sliceId);
          if (returnTable != NULL)
            return (returnTable);
        }
    }

  // If we fall through, not found.
  return (returnTable = NULL);
}


static partitionTable *findLastTable(partitionTable *table)
{
  // Look for the last table that has an extended partition and create
  // another extended partition (and table) inside it.

  int count;

  for (count = 0; count < table->numberEntries; count ++)
    {
      if (table->entries[count].entryType == partition_extended)
	return (findLastTable(table->entries[count].extendedTable));
    }

  return (table);
}


static int getPartitionEntry(int sliceId, slice *entry)
{
  // Loop, scanning for an entry with the matching partition number.

  int status = 0;
  partitionTable *table = NULL;
  int count;

  // Get the table of the entry
  table = getPartitionTable(mainTable, sliceId);
  if (table == NULL)
    return (status = ERR_NOSUCHENTRY);

  for (count = 0; count < table->numberEntries; count ++)
    {
      if (table->entries[count].sliceId == sliceId)
	{
	  memcpy(entry, &(table->entries[count]), sizeof(slice));
	  return (status = 0);
	}
    }

  // If we fall through, not found.
  return (status = ERR_NOSUCHENTRY);
}


static int setPartitionEntry(int sliceId, slice *entry)
{
  // Loop, scanning for an entry with the matching partition number.

  int status = 0;
  partitionTable *table = NULL;
  int count;

  // Get the table of the entry
  table = getPartitionTable(mainTable, sliceId);
  if (table == NULL)
    return (status = ERR_NOSUCHENTRY);

  for (count = 0; count < table->numberEntries; count ++)
    {
      if (table->entries[count].sliceId == sliceId)
	{
	  memcpy(&(table->entries[count]), entry, sizeof(slice));
	  return (status = 0);
	}
    }

  // If we fall through, not found.
  return (status = ERR_NOSUCHENTRY);
}


static void setPartitionNumbering(partitionTable *table, int reset)
{
  // Recurse down through this table and number the partitions

  int count;
  slice *extendedEntry = NULL;

  if (reset)
    {
      numberPartitions = 0;
      numberDataPartitions = 0;
    }

  for (count = 0; count < table->numberEntries; count ++)
    {
      table->entries[count].sliceId = numberPartitions++;

      if (table->entries[count].entryType == partition_extended)
	{
	  extendedEntry = &(table->entries[count]);
	  continue;
	}

      table->entries[count].partition = numberDataPartitions++;
#ifdef PARTLOGIC
      sprintf(table->entries[count].name1, "%d",
	      (table->entries[count].partition + 1));
#else
      sprintf(table->entries[count].name1, "%s%c", selectedDisk->name,
	      ('a' + table->entries[count].partition));
#endif
    }

  if (extendedEntry)
    {
      ((partitionTable *) extendedEntry->extendedTable)->parentSliceId =
	extendedEntry->sliceId;
      setPartitionNumbering(extendedEntry->extendedTable, 0);
    }
}


static inline unsigned cylsToMb(disk *theDisk, unsigned cylinders)
{
  unsigned tmpDiskSize =
    ((cylinders * (theDisk->heads * theDisk->sectorsPerCylinder)) /
     (1048576 / theDisk->sectorSize));
  if (tmpDiskSize < 1)
    return (1);
  else
    return ((unsigned) tmpDiskSize);
}


static void makeSliceString(slice *slc)
{
  int position = 0;
  partitionType sliceType;
  int count;

  for (count = 0; count < MAX_DESCSTRING_LENGTH; count ++)
    slc->string[count] = ' ';
  slc->string[MAX_DESCSTRING_LENGTH - 1] = '\0';

  if (slc->typeId == 0)
    {
      position += SLICESTRING_DISKFIELD_WIDTH;
      strcpy((slc->string + position), "Empty space");
      position += SLICESTRING_LABELFIELD_WIDTH;
    }
  else
    {
      // Disk name
      strcpy(slc->string, slc->name1);
      slc->string[strlen(slc->string)] = ' ';
      position += SLICESTRING_DISKFIELD_WIDTH;

      // Label
      diskGetPartType(slc->typeId, &sliceType);
      sprintf((slc->string + position), "%s", sliceType.description);
      slc->string[strlen(slc->string)] = ' ';
      position += SLICESTRING_LABELFIELD_WIDTH;

      // Filesystem type
      sprintf((slc->string + position), "%s", slc->fsType);
    }
  slc->string[strlen(slc->string)] = ' ';
  position += SLICESTRING_FSTYPEFIELD_WIDTH;

  sprintf((slc->string + position), "%u-%u", slc->startCylinder,
	  slc->endCylinder);
  slc->string[strlen(slc->string)] = ' ';
  position += SLICESTRING_CYLSFIELD_WIDTH;

  sprintf((slc->string + position), "%u",
	  cylsToMb(selectedDisk,
		   ((slc->endCylinder - slc->startCylinder) + 1)));
  position += SLICESTRING_SIZEFIELD_WIDTH;

  if (slc->typeId)
    {
      slc->string[strlen(slc->string)] = ' ';
      if (slc->entryType == partition_primary)
	sprintf((slc->string + position), "primary");
      else if (slc->entryType == partition_extended)
	sprintf((slc->string + position), "extended");
      else if (slc->entryType == partition_logical)
	sprintf((slc->string + position), "logical");

      if (slc->active)
	strcat(slc->string, "/active");
      else
	strcat(slc->string, "       ");
    }
}


static void makeEmptySlice(slice *emptySlice, unsigned startCylinder,
			   unsigned endCylinder)
{
  // Given a slice entry and a geometry, make a slice for it.

  bzero(emptySlice, sizeof(slice));

  emptySlice->startLogical = (startCylinder * cylinderSectors);
  emptySlice->startCylinder = startCylinder;
  emptySlice->startHead = ((emptySlice->startLogical % cylinderSectors) /
			   selectedDisk->sectorsPerCylinder);
  emptySlice->startSector = (((emptySlice->startLogical % cylinderSectors) %
			      selectedDisk->sectorsPerCylinder) + 1);

  emptySlice->sizeLogical =
    (((endCylinder - startCylinder) + 1) * cylinderSectors);

  unsigned endLogical =
    (emptySlice->startLogical + (emptySlice->sizeLogical - 1));

  emptySlice->endCylinder = endCylinder;
  emptySlice->endHead =
    ((endLogical % cylinderSectors) / selectedDisk->sectorsPerCylinder);
  emptySlice->endSector =
    (((endLogical % cylinderSectors) % selectedDisk->sectorsPerCylinder) + 1);

  makeSliceString(emptySlice);
}


static void makeSliceList(void)
{
  // This function populates the list of slices

  slice partition;
  int firstPartition = -1;
  unsigned firstSector = -1;
  int count1, count2;

  // Loop through all the partitions and put them in our list
  numberSlices = 0;
  for (count1 = 0; count1 < numberPartitions; count1 ++)
    {
      firstPartition = -1;
      firstSector = 0xFFFFFFFF;

      for (count2 = 0; count2 < numberPartitions; count2 ++)
	{
	  if (getPartitionEntry(count2, &partition) < 0)
	    break;

	  // If this is an extended partition, skip it
	  if (partition.entryType == partition_extended)
	    continue;

	  // If we have already processed this one, continue
	  if (numberSlices && (partition.startLogical <=
			       slices[numberSlices - 1].startLogical))
	    continue;

	  if (partition.startLogical < firstSector)
	    {
	      firstSector = partition.startLogical;
	      firstPartition = count2;
	    }
	}

      if (firstPartition < 0)
	break;

      // Get the partition
      if (getPartitionEntry(firstPartition, &partition) < 0)
	break;

      // If this is an extended partition, skip it
      if (partition.entryType == partition_extended)
      	continue;

      // Is there empty space between this partition and the previous slice?
      // If so, insert another slice to represent the empty space
      if (((numberSlices == 0) && (partition.startCylinder > 0)) ||
	  ((numberSlices > 0) &&
	   (partition.startCylinder >
	    (slices[numberSlices - 1].endCylinder + 1))))
	{
	  makeEmptySlice(&slices[numberSlices],
			 ((numberSlices > 0)?
			  (slices[numberSlices - 1].endCylinder + 1) : 0),
			 (partition.startCylinder - 1));
	  numberSlices += 1;
	}

      // Now add a slice for the current partition
      memcpy(&slices[numberSlices], &partition, sizeof(slice));
      makeSliceString(&slices[numberSlices]);
      numberSlices += 1;
    }

  // Is there empty space at the end of the disk?
  if ((numberPartitions == 0) || (slices[numberSlices - 1].endCylinder <
				  (selectedDisk->cylinders - 1)))
    {
      makeEmptySlice(&slices[numberSlices],
		     ((numberPartitions == 0)? 0 :
		      (slices[numberSlices - 1].endCylinder + 1)),
		     (selectedDisk->cylinders - 1));
      numberSlices += 1;
    }
}


static void pruneExtendedPartition(void)
{
  // Prune the primary extended partitions so that the 'startCylinder' and
  // 'endCylinder' values reflect the start and end of the first and last
  // logical partitions, respectively.
  
  slice *primaryExtendedEntry = NULL;
  partitionTable *table = NULL;
  slice *extendedEntry = NULL;
  unsigned startCylinder = 0xFFFFFFFF;
  unsigned endCylinder = 0;
  int count;

  extendedStartSector = 0;

  for (count = 0; count < mainTable->numberEntries; count ++)
    if (mainTable->entries[count].entryType == partition_extended)
      {
	primaryExtendedEntry = &(mainTable->entries[count]);
	break;
      }

  if (primaryExtendedEntry == NULL)
    return;

  table = primaryExtendedEntry->extendedTable;

  // Iterate through the chain of tables, collecting the start and end values
  while (table)
    {
      extendedEntry = NULL;
      for (count = 0; count < table->numberEntries; count ++)
	{
	  if (table->entries[count].entryType == partition_extended)
	    extendedEntry = &(table->entries[count]);
	  else
	    {
	      if (table->entries[count].startCylinder < startCylinder)
		startCylinder = table->entries[count].startCylinder;
	      if (table->entries[count].endCylinder > endCylinder)
		endCylinder = table->entries[count].endCylinder;
	    }
	}

      if (extendedEntry == NULL)
	break;

      table = extendedEntry->extendedTable;
    }

  primaryExtendedEntry->startLogical = (startCylinder * cylinderSectors);
  primaryExtendedEntry->sizeLogical =
    (((endCylinder - startCylinder) + 1) * cylinderSectors);  
  primaryExtendedEntry->startCylinder = startCylinder;
  primaryExtendedEntry->endCylinder = endCylinder;
  extendedStartSector = primaryExtendedEntry->startLogical;
}


static int addPartitionEntry(partitionTable *table, slice *newEntry)
{
  // Add a new entry to the supplied table, update the slice list, and return
  // the slice number of the new entry, if applicable

  int newSliceNumber = ERR_INVALID;
  int count;

  memcpy(&(table->entries[table->numberEntries]), newEntry, sizeof(slice));
  table->numberEntries += 1;

  setPartitionNumbering(mainTable, 1);
  if (table->extended)
    pruneExtendedPartition();
  makeSliceList();

  // Copy it back to the caller's structure.
  memcpy(newEntry, &(table->entries[table->numberEntries - 1]), sizeof(slice));
  
  // Find our new slice, if possible
  for (count = 0; count < numberSlices; count ++)
    if (slices[count].startLogical == newEntry->startLogical)
      {
	newSliceNumber = count;
	break;
      }

  return (newSliceNumber);
}


static int deletePartitionEntry(int sliceId)
{
  // Given a partition number, find its partition table and remove the entry
  // from the table

  int status = 0;
  partitionTable *table = NULL;
  slice deleteEntry;
  slice parentEntry;
  int count1, count2;

  // Get the table of the entry
  table = getPartitionTable(mainTable, sliceId);
  if (table == NULL)
    return (status = ERR_NOSUCHENTRY);
  
  // Loop through the table and move the entry to the end, setting its type
  // to zero
  count2 = 0;
  for (count1 = 0; count1 < table->numberEntries; count1 ++)
    {
      if (table->entries[count1].sliceId == sliceId)
	memcpy(&deleteEntry, &(table->entries[count1]), sizeof(slice));
      
      else
	memcpy(&(table->entries[count2++]), &(table->entries[count1]),
	       sizeof(slice));
    }

  deleteEntry.typeId = 0;
  table->numberEntries -= 1;
  memcpy(&(table->entries[table->numberEntries]), &deleteEntry, sizeof(slice));

  // If this is an extended partition table, and there are no more entries
  // in it, we need to delete/shrink the extended partition(s) too.
  if (table->extended)
    {
      if ((table->numberEntries == 1) &&
	  (table->entries[0].entryType == partition_extended))
	{
	  // This table only contains a further extended partition.  We need
	  // to collapse the lower table into this one.
	  memcpy(&parentEntry, &(table->entries[0]), sizeof(slice));
	  parentEntry.sliceId = table->parentSliceId;
	  if (parentEntry.extendedTable)
	    ((partitionTable *) parentEntry.extendedTable)->parentSliceId =
	      table->parentSliceId;
	  setPartitionEntry(table->parentSliceId, &parentEntry);
	}

      else if (table->numberEntries <= 0)
	{
	  // Nothing left in this table.  Delete the parent partition entry
	  status = deletePartitionEntry(table->parentSliceId);
	  if (status < 0)
	    return (status);
	}
      
      pruneExtendedPartition();
    }

  if (deleteEntry.entryType == partition_extended)
    {
      // Free the memory of the table attached to this partition.
      if (deleteEntry.extendedTable)
	free(deleteEntry.extendedTable);
      deleteEntry.extendedTable = NULL;
    }

  // Renumber the partitions
  setPartitionNumbering(mainTable, 1);

  return (status = 0);
}


static int readEntry(partitionTable *table, int entryNumber, slice *entry)
{
  int status = 0;
  unsigned char *sectorData = table->sectorData;
  unsigned char *partRecord = 0;

  bzero(entry, sizeof(slice));

  if ((entryNumber < 0) || (entryNumber > (DISK_MAX_PRIMARY_PARTITIONS - 1)))
    {
      error("No such partition entry %d", entryNumber);
      return (status = ERR_NOSUCHENTRY);
    }

  // Set this pointer to the partition record in the partition table
  partRecord = ((sectorData + 0x01BE) + (entryNumber * 16));

  entry->active = (partRecord[ENTRYOFFSET_DRV_ACTIVE] >> 7);
  entry->startHead = (unsigned) partRecord[ENTRYOFFSET_START_HEAD];
  entry->startCylinder = (unsigned) partRecord[ENTRYOFFSET_START_CYL];
  entry->startCylinder |=
    (((unsigned) partRecord[ENTRYOFFSET_START_CYLSECT] & 0xC0) << 2);
  entry->startSector = (unsigned)
    (partRecord[ENTRYOFFSET_START_CYLSECT] & 0x3F);
  entry->typeId = partRecord[ENTRYOFFSET_TYPE];
  entry->endHead = (unsigned) partRecord[ENTRYOFFSET_END_HEAD];
  entry->endCylinder = (unsigned) partRecord[ENTRYOFFSET_END_CYL];
  entry->endCylinder |=
    (((unsigned) partRecord[ENTRYOFFSET_END_CYLSECT] & 0xC0) << 2);
  entry->endSector = (unsigned) (partRecord[ENTRYOFFSET_END_CYLSECT] & 0x3F);
  entry->startLogical = (unsigned) ((unsigned *) partRecord)[2];
  entry->sizeLogical = (unsigned) ((unsigned *) partRecord)[3];

  if (PARTITION_TYPEID_IS_EXTENDED(entry->typeId))
    entry->entryType = partition_extended;
  else
    entry->entryType = partition_primary;

  // Now, check whether the start and end CHS values seem correct.
  // If they are 'maxed out' and don't correspond with the LBA values,
  // recalculate them.
  if ((entry->startCylinder == 1023) || (entry->endCylinder == 1023))
    {
      unsigned endLogical =
	(entry->startLogical + (entry->sizeLogical - 1));

      if (entry->startCylinder == 1023)
	{
	  entry->startCylinder = (entry->startLogical / cylinderSectors);

	  // If it's an extended table, adjust the cylinder value
	  if (table->extended)
	    {
	      if (entry->entryType == partition_extended)
		entry->startCylinder +=
		  (extendedStartSector / cylinderSectors);
	      else
		entry->startCylinder += (table->startSector / cylinderSectors);
	    }
	}

      if (entry->endCylinder == 1023)
	{
	  entry->endCylinder = (endLogical / cylinderSectors);

	  // If it's an extended table, adjust the cylinder value
	  if (table->extended)
	    {
	      if (entry->entryType == partition_extended)
		entry->endCylinder += (extendedStartSector / cylinderSectors);
	      else
		entry->endCylinder += (table->startSector / cylinderSectors);
	    }
	}
    }

  return (status = 0);
}


static int writeEntry(partitionTable *table, int entryNumber, slice *entry)
{
  int status = 0;
  unsigned char *sectorData = table->sectorData;
  unsigned char *partRecord = 0;
  unsigned startCyl, endCyl;

  if ((entryNumber < 0) || (entryNumber > (DISK_MAX_PRIMARY_PARTITIONS - 1)))
    {
      error("No such partition entry %d", entryNumber);
      return (status = ERR_NOSUCHENTRY);
    }

  // We might change these
  startCyl = entry->startCylinder;
  endCyl = entry->endCylinder;

  // Check whether our start or end cylinder values exceed the legal
  // maximum of 1023.  If so, set them to 1023.
  if (startCyl > 1023)
    startCyl = 1023;
  if (endCyl > 1023)
    endCyl = 1023;

  // Set this pointer to the partition record in the partition table
  partRecord = ((sectorData + 0x01BE) + (entryNumber * 16));

  // Clear it
  bzero(partRecord, 16);

  if (entry->active)
    partRecord[ENTRYOFFSET_DRV_ACTIVE] = 0x80;
  partRecord[ENTRYOFFSET_START_HEAD] = (unsigned char) entry->startHead;
  partRecord[ENTRYOFFSET_START_CYLSECT] = (unsigned char)
    (((startCyl & 0x300) >> 2) | (entry->startSector & 0x3F));
  partRecord[ENTRYOFFSET_START_CYL] = (unsigned char) (startCyl & 0x0FF);
  partRecord[ENTRYOFFSET_TYPE] = entry->typeId;
  partRecord[ENTRYOFFSET_END_HEAD] = (unsigned char) entry->endHead;
  partRecord[ENTRYOFFSET_END_CYLSECT] = (unsigned char)
    (((endCyl & 0x300) >> 2) | (entry->endSector & 0x3F));
  partRecord[ENTRYOFFSET_END_CYL] = (unsigned char) (endCyl & 0x0FF);
  ((unsigned *) partRecord)[2] = entry->startLogical;
  ((unsigned *) partRecord)[3] = entry->sizeLogical;

  return (status = 0);
}


static int getFsType(slice *entry)
{
  int status = 0;
  int tmpNumberDisks = 0;
  disk tmpDisks[DISK_MAXDEVICES];
  int count;

  // Call the kernel to give us the number of available disks
  tmpNumberDisks = diskGetCount();

  status = diskGetAll(tmpDisks, (DISK_MAXDEVICES * sizeof(disk)));
  if (status < 0)
    return (status);

  for (count = 0; count < tmpNumberDisks; count ++)
    if (!strcmp(tmpDisks[count].name, entry->name1))
      {
	if (strcmp(tmpDisks[count].fsType, "unknown"))
	  strncpy(entry->fsType, tmpDisks[count].fsType,
		  FSTYPE_MAX_NAMELENGTH);
	break;
      }

  return (status = 0);
}


static int checkTable(const disk *theDisk, partitionTable *table, int fix)
{
  // Does a series of correctness checks on the table that's currently in
  // memory, and outputs warnings for any problems it finds

  slice *entry = NULL;
  int errors = 0;
  unsigned expectCylinder = 0;
  unsigned expectHead = 0;
  unsigned expectSector = 0;
  char output[MAXSTRINGLENGTH];
  int count;

  sprintf(output, "%s table:\n", (table->extended? "Extended" : "Main"));

  if (table->extended && (table->numberEntries > table->maxEntries))
    {
      sprintf((output + strlen(output)), "Table has %d entries; max is %d",
	      table->numberEntries, table->maxEntries);
      if (fix)
	{
	  table->numberEntries = table->maxEntries;
	  changesPending += 1;
	}
      else
	errors += 1;
    }

  // For each partition entry, check that its starting/ending
  // cylinder/head/sector values match with its starting logical value
  // and logical size.  In general we expect the logical values are correct.
  for (count = 0; count < table->numberEntries; count ++)
    {
      entry = &table->entries[count];

      if (!entry->typeId)
	continue;

      unsigned endLogical = (entry->startLogical + (entry->sizeLogical - 1));

      expectCylinder = (entry->startLogical / cylinderSectors);
      if (expectCylinder != entry->startCylinder)
	{
	  sprintf((output + strlen(output)), "Partition %d starting cylinder "
		  "is %u, should be %u\n", entry->sliceId,
		  entry->startCylinder, expectCylinder);
	  if (fix)
	    {
	      entry->startCylinder = expectCylinder;
	      changesPending += 1;
	    }
	  else
	    errors += 1;
	}

      expectCylinder = (endLogical / cylinderSectors);
      if (expectCylinder != entry->endCylinder)
	{
	  sprintf((output + strlen(output)), "Partition %d ending cylinder "
		  "is %u, should be %u\n", entry->sliceId,
		  entry->endCylinder, expectCylinder);
	  if (fix)
	    {
	      entry->endCylinder = expectCylinder;
	      changesPending += 1;
	    }
	  else
	    errors += 1;
	}

      expectHead = ((entry->startLogical % cylinderSectors) /
		    theDisk->sectorsPerCylinder);
      if (expectHead != entry->startHead)
	{
	  sprintf((output + strlen(output)), "Partition %d starting head is "
		  "%u, should be %u\n", entry->sliceId, entry->startHead,
		  expectHead);
	  if (fix)
	    {
	      entry->startHead = expectHead;
	      changesPending += 1;
	    }
	  else
	    errors += 1;
	}

      expectHead =
	((endLogical % cylinderSectors) / theDisk->sectorsPerCylinder);
      if (expectHead != entry->endHead)
	{
	  sprintf((output + strlen(output)), "Partition %d ending head is %u, "
		  "should be %u\n", entry->sliceId, entry->endHead,
		  expectHead);
	  if (fix)
	    {
	      entry->endHead = expectHead;
	      changesPending += 1;
	    }
	  else
	    errors += 1;
	}

      expectSector = (((entry->startLogical % cylinderSectors) %
		       theDisk->sectorsPerCylinder) + 1);
      if (expectSector != entry->startSector)
	{
	  sprintf((output + strlen(output)), "Partition %d starting CHS "
		  "sector is %u, should be %u\n", entry->sliceId,
		  entry->startSector, expectSector);
	  if (fix)
	    {
	      entry->startSector = expectSector;
	      changesPending += 1;
	    }
	  else
	    errors += 1;
	}

      expectSector = (((endLogical % cylinderSectors) %
		       theDisk->sectorsPerCylinder) + 1);
      if (expectSector != entry->endSector)
	{
	  sprintf((output + strlen(output)), "Partition %d ending CHS sector "
		  "is %u, should be %u\n", entry->sliceId, entry->endSector,
		  expectSector);
	  if (fix)
	    {
	      entry->endSector = expectSector;
	      changesPending += 1;
	    }
	  else
	    errors += 1;
	}
    }
  
  if (errors)
    {
      sprintf((output + strlen(output)), "\nFix th%s error%s?",
	      ((errors == 1)? "is" : "ese"), ((errors == 1)? "" : "s"));
      if (yesOrNo(output))
	return (checkTable(theDisk, table, 1));
      else
	return (ERR_INVALID);
    }
  else
    return (0);
}


static void scanPartitionTable(const disk *theDisk, partitionTable *table,
			       unsigned char *buffer)
{
  // Read the partition entries from the buffer and fill out the partition
  // table structure.

  unsigned char *sectorData = NULL;
  slice *entry = NULL;
  slice *extendedEntry = NULL;
  partitionTable *extendedTable = NULL;
  int count;

  if (buffer)
    memcpy(table->sectorData, buffer, 512);

  sectorData = table->sectorData;

  // Clear the table entries
  bzero(table->entries, (sizeof(slice) * DISK_MAX_PRIMARY_PARTITIONS));
  table->numberEntries = 0;

  if (table == mainTable)
    {
      numberPartitions = 0;
      numberDataPartitions = 0;
      extendedStartSector = 0;
    }

  if (table->extended)
    // Extended partitions are only supposed to have 2 entries
    table->maxEntries = 2;
  else
    table->maxEntries = DISK_MAX_PRIMARY_PARTITIONS;

  // Is this a valid partition table?  Make sure the signature is at the end.
  if ((sectorData[510] != (unsigned char) 0x55) ||
      (sectorData[511] != (unsigned char) 0xAA))
    {
      // This does not appear to be a valid master partition table.
      warning("Invalid signature in partition table %d on disk %s.\n"
	      "Writing changes will create one.", table->extended,
	      theDisk->name);
      // Don't do the change now, record that a change will happen
      changesPending += 1;

      // For now, don't trust any data in this table.
      return;
    }

  // Loop through the partition entries and create slices for them.
  for (count = 0; count < DISK_MAX_PRIMARY_PARTITIONS; count ++)
    {
      entry = &(table->entries[count]);
      readEntry(table, count, entry);
      
      if (entry->typeId == 0)
	// The "rules" say we're supposed to be finished with this 
	// table, but that's not absolutely always the case.  We'll still
	// probably be somewhat screwed, but not as screwed as if we were
	// to quit here.
	continue;

      table->numberEntries += 1;

      if (entry->entryType == partition_extended)
	{
	  if (table->extended)
	    entry->startLogical += extendedStartSector;
	  else
	    extendedStartSector = entry->startLogical;
	  
	  // We add extended partitions after all the others
	  extendedEntry = entry;
	  continue;
	}
      else if (table->extended)
	{
	  entry->entryType = partition_logical;
	  entry->startLogical += table->startSector;
	}

      entry->sliceId = numberPartitions++;
      entry->partition = numberDataPartitions++;
      sprintf(entry->name1, "%s%c", theDisk->name, ('a' + entry->partition));
      getFsType(entry);
#ifdef PARTLOGIC
      sprintf(entry->name1, "%d", (entry->partition + 1));
#endif
    }

  if (extendedEntry)
    extendedEntry->sliceId = numberPartitions++;

  // Do a check on the table
  checkTable(theDisk, table, 0);

  // If we have an extended partition, we need to delve into the chain of
  // partition tables.
  if (extendedEntry)
    {
      // Allocate a partition table for it
      extendedTable = malloc(sizeof(partitionTable));
      if (extendedTable == NULL)
	{
	  error("Unable to allocate memory for extended table");
	  return;
	}

      extendedTable->parentSliceId = extendedEntry->sliceId;
      extendedTable->startSector = extendedEntry->startLogical;
      extendedTable->extended = (table->extended + 1);
      extendedEntry->extendedTable = extendedTable;

      if (buffer)
	scanPartitionTable(theDisk, extendedTable, (buffer + 512));

      else
	{
	  // Read the partition table sector data into the table structure.
	  if (diskReadSectors(theDisk->name, extendedTable->startSector, 1,
			      extendedTable->sectorData) < 0)
	    {
	      error("Unable to read extended table");
	      return;
	    }
	  scanPartitionTable(theDisk, extendedTable, NULL);
	}
    }
}


static int makeBackup(partitionTable *table, fileStream *backupFile)
{
  // Given a partition table and an open file, append the partition table.
  
  int status = 0;
  int count;

  if (table->extended)
    {
      status = fileStreamSeek(backupFile, (table->extended * 512));
      if (status < 0)
	warning("Error seeking extended backup partition table file");
    }

  status = fileStreamWrite(backupFile, 512, table->sectorData);
  if (status < 0)
    warning("Error writing backup partition table file");

  // Look for an extended partition
  for (count = 0; count < table->maxEntries; count ++)
    {
      if (table->entries[count].typeId == 0)
	continue;

      // We add extended partitions after all the others
      if (table->entries[count].entryType == partition_extended)
	makeBackup(table->entries[count].extendedTable, backupFile);
    }

  return (status = 0);
}


static int readPartitionTable(const disk *theDisk, unsigned sector)
{
  // Read the partition table from the physical disk

  int status = 0;
  char fileName[MAX_PATH_NAME_LENGTH];
  static char tmpBackupFileName[MAX_PATH_NAME_LENGTH];
  file backupFile;
  fileStream tmpBackupFile;

  // Clear stack data
  bzero(&backupFile, sizeof(file));
  bzero(&tmpBackupFile, sizeof(fileStream));

  // Read the first sector of the device
  status = diskReadSectors(theDisk->name, sector, 1, mainTable->sectorData);
  if (status < 0)
    return (status);
  
  scanPartitionTable(theDisk, mainTable, NULL);

  // Any backup partition table saved?  Construct the file name
  sprintf(fileName, BACKUP_MBR, theDisk->name);
  if (!fileFind(fileName, &backupFile))
    backupAvailable = 1;
  else
    backupAvailable = 0;

  if (!readOnly)
    {
      // We are not read-only, create a new temporary backup
      if (tmpBackupName != NULL)
	{
	  fileDelete(tmpBackupName);
	  tmpBackupName = NULL;
	}

      status = fileGetTemp(&backupFile);
      if (status < 0)
	{
	  warning("Can't create backup file");
	  return (status);
	}

      sprintf(tmpBackupFileName, TEMP_DIR"/%s", backupFile.name);
      fileClose(&backupFile);

      status = fileStreamOpen(tmpBackupFileName, OPENMODE_WRITE,
			      &tmpBackupFile);
      if (status < 0)
	{
	  warning("Can't open backup file %s", backupFile.name);
	  return (status);
	}

      status = makeBackup(mainTable, &tmpBackupFile);
      if (status < 0)
	return (status);

      fileStreamClose(&tmpBackupFile);
      tmpBackupName = tmpBackupFileName;
    }

  return (status = 0);
}


static int writePartitionTable(const disk *theDisk, partitionTable *table)
{
  // Write the partition table to the physical disk

  int status = 0;
  char fileName[MAX_PATH_NAME_LENGTH];
  fileStream backupFile;
  slice entry;
  int count;

  // Clear stack data
  bzero(&backupFile, sizeof(fileStream));

  if (table == mainTable)
    {
      if (readOnly && !yesOrNo("Can't create a partition table backup in\n"
			       "read-only mode.  Proceed anyway?"))
	return (status = 0);

      if (tmpBackupName != NULL)
	{
	  // Construct the backup file name
	  sprintf(fileName, BACKUP_MBR, theDisk->name);
  
	  // Copy the temporary backup file to the backup
	  fileMove(tmpBackupName, fileName);
	  tmpBackupName = NULL;
	  
	  // We now have a proper backup
	  backupAvailable = 1;
	}
    }

  // Do a check on the table
  status = checkTable(theDisk, table, 0);
  if ((status < 0) && !yesOrNo("The consistency check failed.  Write anyway?"))
    return (status);

  // Loop through the slice entries write them into the partition table
  for (count = 0; count < DISK_MAX_PRIMARY_PARTITIONS; count ++)
    {
      memcpy(&entry, &table->entries[count], sizeof(slice));

      // If this is an extended table, re-adjust the starting logical
      // starting sector value to reflect a position relative to the start
      // of the extended partition.
      if (table->extended)
	{
	  if (entry.entryType == partition_extended)
	    entry.startLogical -= extendedStartSector;
	  else
	    entry.startLogical -= table->startSector;
	}

      writeEntry(table, count, &entry);

      // If this is an extended partition, descend down and write its
      // table(s) as well.
      if (entry.typeId && (entry.entryType == partition_extended))
	{
	  status = writePartitionTable(theDisk, entry.extendedTable);
	  if (status < 0)
	    // Don't want to fail here (thus making it impossible to write
	    // any other table data).  Just make an error.
	    error("Unable to write extended partition table");
	}
    }

  // Make sure it has a valid signature
  table->sectorData[510] = (unsigned char) 0x55;
  table->sectorData[511] = (unsigned char) 0xAA;

  // Write the first sector of the device
  status =
    diskWriteSectors(theDisk->name, table->startSector, 1, table->sectorData);
  if (status < 0)
    return (status);

  diskSync();
  changesPending = 0;

  // Tell the kernel to reexamine the partition tables
  return (status = diskReadPartitions());
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
  status = diskGetAllPhysical(tmpDiskInfo, (DISK_MAXDEVICES * sizeof(disk)));
  if (status < 0)
    // Eek.  Problem getting disk info
    return (status);

  // Loop through these disks, figuring out which ones are hard disks
  // and putting them into the regular array
  for (count = 0; count < tmpNumberDisks; count ++)
    if (tmpDiskInfo[count].flags & DISKFLAG_HARDDISK)
      {
	memcpy(&diskInfo[numberDisks], &tmpDiskInfo[count], sizeof(disk));

	sprintf(diskStrings[numberDisks], "Disk %d: [%s] %u Mb, %u cyls, "
		"%u heads, %u secs/cyl, %u bytes/sec",
		diskInfo[numberDisks].deviceNumber, diskInfo[numberDisks].name,
		cylsToMb(&diskInfo[numberDisks],
			 diskInfo[numberDisks].cylinders),
		diskInfo[numberDisks].cylinders, diskInfo[numberDisks].heads, 
		diskInfo[numberDisks].sectorsPerCylinder,
		diskInfo[numberDisks].sectorSize);

	numberDisks += 1;
      }

  if (numberDisks <= 0)
    return (status = ERR_NOSUCHENTRY);
  else
    return (status = 0);
}


static inline unsigned mbToCyls(disk *theDisk, unsigned megabytes)
{
  unsigned sectors = ((1048576 / theDisk->sectorSize) * megabytes);
  unsigned cylinders = (sectors / cylinderSectors);
  if (sectors % cylinderSectors)
    cylinders += 1;

  return (cylinders);
}


static void printDisks(void)
{
  int count;

  for (count = 0; count < numberDisks; count ++)
    // Print disk info
    printf("  %s\n", diskStrings[count]);
}


static int selectDisk(const disk *theDisk)
{
  int status = 0;
  char tmpChar[80];
  int count;

  if (changesPending)
    {
      sprintf(tmpChar, "Discard changes to disk %s?", selectedDisk->name);
      if (!yesOrNo(tmpChar))
	{
	  if (graphics)
	    // Re-select the old disk in the list
	    windowComponentSetSelected(diskList, selectedDiskNumber);
	  return (status = 0);
	}
    }
  
  changesPending = 0;

  cylinderSectors = (theDisk->heads * theDisk->sectorsPerCylinder);
	
  status = readPartitionTable(theDisk, 0);
  if (status < 0)
    {
      if (selectedDisk)
	cylinderSectors =
	  (selectedDisk->heads * selectedDisk->sectorsPerCylinder);
      return (status);
    }

  for (count = 0; count < numberDisks; count ++)
    if (&diskInfo[count] == theDisk)
      {
	selectedDiskNumber = count;
	break;
      }
  selectedDisk = (disk *) theDisk;

  if (graphics)
    windowComponentSetSelected(diskList, selectedDiskNumber);

  makeSliceList();
  selectedSlice = 0;
  return (status = 0);
}


static int queryDisk(void)
{
  int status;

  status = vshCursorMenu("Please choose the disk on which to operate:",
			 numberDisks, diskStrings, selectedDiskNumber);
  if (status < 0)
    return (status);

  status = selectDisk(&diskInfo[status]);
  if (selectedDisk == NULL)
    status = ERR_INVALID;
  return (status);
}


static void drawDiagram(void)
{
  // Draw a picture of the disk layout on our 'canvas' component

  int needPixels = 0;
  int xCoord = 0;
  windowDrawParameters params;
  static color colors[DISK_MAX_PARTITIONS + 1] = {
    { 0, 255, 255 }, // 0  = Yellow
    { 255, 0, 0 },   // 1  = Blue
    { 0, 255, 0 },   // 2  = Green
    { 0, 0, 255 },   // 3  = Red
    { 255, 0, 255 }, // 4  = Purple
    { 0, 196, 255 }, // 5  = Orange
    // Need some more colors here
    { 0, 255, 255 }, // 6  = Yellow
    { 255, 0, 0 },   // 7  = Blue
    { 0, 255, 0 },   // 8  = Green
    { 0, 0, 255 },   // 9  = Red
    { 255, 0, 255 }, // 10 = Purple
    { 0, 196, 255 }, // 11  = Orange
    { 0, 255, 255 }, // 12 = Yellow
    { 255, 0, 0 },   // 13 = Blue
    { 0, 255, 0 },   // 14 = Green
    { 0, 0, 255 },   // 15 = Red
    // This one is for extended partitions
    { 255, 196, 178 }
  };
  color *extendedColor = NULL;
  int colorCounter = 0;
  int count1, count2;

  // Clear our drawing parameters
  bzero(&params, sizeof(windowDrawParameters));
  
  // Some basic drawing values for partition rectangles
  params.operation = draw_rect;
  params.mode = draw_normal;
  params.xCoord1 = 0;
  params.yCoord1 = 0;
  params.width = canvasWidth;
  params.height = canvasHeight;
  params.thickness = 1;
  params.fill = 1;

  // Draw a white background
  params.foreground.red = 255;
  params.foreground.green = 255;
  params.foreground.blue = 255;
  windowComponentSetData(canvas, &params, 1);

  // Set the pixel widths of all the slices
  for (count1 = 0; count1 < numberSlices; count1 ++)
    slices[count1].pixelWidth =
      ((((slices[count1].endCylinder - slices[count1].startCylinder) + 1) *
	canvasWidth) / selectedDisk->cylinders);

  // Now, we want to make sure each partition slice has a width of at least
  // MIN_WIDTH, so that it is visible.  If we need to, we steal the pixels
  // from any adjacent slices.
  #define MIN_WIDTH 15
  for (count1 = 0; count1 < numberSlices; count1 ++)
    if (slices[count1].pixelWidth < MIN_WIDTH)
      {
	needPixels = (MIN_WIDTH - slices[count1].pixelWidth);

	while (needPixels)
	  for (count2 = 0; count2 < numberSlices; count2 ++)
	    if ((count2 != count1) && (slices[count2].pixelWidth > MIN_WIDTH))
	      {
		slices[count1].pixelWidth += 1;
		slices[count2].pixelWidth -= 1;
		needPixels -= 1;
		if (!needPixels)
		  break;
	      }
      }

  for (count1 = 0; count1 < numberSlices; count1 ++)
    {
      slices[count1].pixelX = xCoord;

      params.mode = draw_normal;
      params.xCoord1 = slices[count1].pixelX;
      params.yCoord1 = 0;
      params.width = slices[count1].pixelWidth;
      params.height = canvasHeight;
      params.fill = 1;

      if (slices[count1].typeId)
	{
	  if (slices[count1].entryType == partition_logical)
	    extendedColor = &colors[DISK_MAX_PARTITIONS];
	  else
	    extendedColor = NULL;
	}
      
      if (extendedColor != NULL)
	{
	  if ((slices[count1].typeId) ||
	      ((count1 < (numberSlices - 1)) &&
	       (slices[count1 + 1].entryType == partition_logical)))
	    {
	      params.foreground.red = extendedColor->red;
	      params.foreground.green = extendedColor->green;
	      params.foreground.blue = extendedColor->blue;
	      windowComponentSetData(canvas, &params, 1);
	    }
	}

      // If it is a used slice, we draw a filled rectangle on the canvas to
      // represent the partition.
      if (slices[count1].typeId)
	{
	  params.foreground.red = colors[colorCounter].red;
	  params.foreground.green = colors[colorCounter].green;
	  params.foreground.blue = colors[colorCounter].blue;
	  if (slices[count1].entryType == partition_logical)
	    {
	      params.xCoord1 += 3;
	      params.yCoord1 += 3;
	      params.width -= 6;
	      params.height -= 6;
	    }
	  windowComponentSetData(canvas, &params, 1);
	  colorCounter += 1;
	}

      // If this is the selected slice, draw a border inside it
      if (count1 == selectedSlice)
	{
	  params.mode = draw_xor;
	  params.foreground.red = 200;
	  params.foreground.green = 200;
	  params.foreground.blue = 200;
	  params.xCoord1 += 2;
	  params.yCoord1 += 2;
	  params.width -= 4;
	  params.height -= 4;
	  params.fill = 0;
	  windowComponentSetData(canvas, &params, 1);
	}

      xCoord += slices[count1].pixelWidth;
    }
}


static void printBanner(void)
{
  textScreenClear();
  printf("%s\nCopyright (C) 1998-2005 J. Andrew McLaughlin\n", programName);
}


static void display(void)
{
  char *sliceStrings[MAX_SLICES];
  char lineString[SLICESTRING_LENGTH + 2];
  int slc = 0;
  int foregroundColor = textGetForeground();
  int backgroundColor = textGetBackground();
  int isHide = 0;
  int isMove = 0;
  int count;

  for (count = 0; count < numberSlices; count ++)
    // We need to pass a char *[] to the slice list component in graphics
    // mode.
    sliceStrings[count] = slices[count].string;

  if (graphics)
    {
      // Re-populate our slice list component
      windowComponentSetSelected(sliceList, 0);
      windowComponentSetData(sliceList, sliceStrings, numberSlices);
      windowComponentSetSelected(sliceList, selectedSlice);
      drawDiagram();

      // Depending on which type slice was selected (i.e. partition vs.
      // empty space) we enable/disable button choices
      if (slices[selectedSlice].typeId)
	{
	  // It's a partition

	  if (PARTITION_TYPEID_IS_HIDEABLE(slices[selectedSlice].typeId) ||
	      PARTITION_TYPEID_IS_HIDDEN(slices[selectedSlice].typeId))
	    isHide = 1;

	  if (!ISLOGICAL(&slices[selectedSlice]))
	    isMove = 1;

	  windowComponentSetEnabled(setActiveButton, 1);
	  windowComponentSetEnabled(menuSetActive, 1);
	  windowComponentSetEnabled(deleteButton, 1);
	  windowComponentSetEnabled(menuDelete, 1);
	  windowComponentSetEnabled(formatButton, 1);
	  windowComponentSetEnabled(menuFormat, 1);
	  windowComponentSetEnabled(hideButton, isHide);
	  windowComponentSetEnabled(menuHide, isHide);
	  windowComponentSetEnabled(moveButton, isMove);
	  windowComponentSetEnabled(menuMove,isMove);
	  windowComponentSetEnabled(newButton, 0);
	  windowComponentSetEnabled(menuNew, 0);
	  windowComponentSetEnabled(setTypeButton, 1);
	  windowComponentSetEnabled(menuSetType, 1);
	}
      else
	{
	  // It's empty space
	  windowComponentSetEnabled(setActiveButton, 0);
	  windowComponentSetEnabled(menuSetActive, 0);
	  windowComponentSetEnabled(deleteButton, 0);
	  windowComponentSetEnabled(menuDelete, 0);
	  windowComponentSetEnabled(formatButton, 0);
	  windowComponentSetEnabled(menuFormat, 0);
	  windowComponentSetEnabled(hideButton, 0);
	  windowComponentSetEnabled(menuHide, 0);
	  windowComponentSetEnabled(moveButton, 0);
	  windowComponentSetEnabled(menuMove, 0);
	  windowComponentSetEnabled(newButton, 1);
	  windowComponentSetEnabled(menuNew, 1);
	  windowComponentSetEnabled(setTypeButton, 0);
	  windowComponentSetEnabled(menuSetType, 0);
	}

      // Other buttons enabled/disabled...
      windowComponentSetEnabled(deleteAllButton, numberPartitions);
      windowComponentSetEnabled(menuDeleteAll, numberPartitions);
      windowComponentSetEnabled(menuRestoreBackup, backupAvailable);
      windowComponentSetEnabled(undoButton, changesPending);
      windowComponentSetEnabled(menuUndo, changesPending);
      windowComponentSetEnabled(writeButton, changesPending);
      windowComponentSetEnabled(menuWrite, changesPending);
    }
  else
    {
      printBanner();
      bzero(lineString, (SLICESTRING_LENGTH + 2));
      for (count = 0; count <= SLICESTRING_LENGTH; count ++)
	lineString[count] = 196;

      printf("\n%s\n\n  %s\n %s\n", diskStrings[selectedDiskNumber],
	     sliceListHeader, lineString);

      // Print info about the partitions
      for (slc = 0; slc < numberSlices; slc ++)
	{
	  printf(" ");
	  
	  if (slc == selectedSlice)
	    {
	      // Reverse the colors
	      textSetForeground(backgroundColor);
	      textSetBackground(foregroundColor);
	    }
	  
	  printf(" %s", slices[slc].string);
	  for (count = strlen(slices[slc].string); count < SLICESTRING_LENGTH;
	       count ++)
	    printf(" ");

	  if (slc == selectedSlice)
	    {
	      // Restore the colors
	      textSetForeground(foregroundColor);
	      textSetBackground(backgroundColor);
	    }

	  printf("\n");
	}

      printf(" %s\n", lineString);
    }
}


static void setActive(int sliceId)
{
  slice entry;
  int count;

  if (getPartitionEntry(sliceId, &entry) < 0)
    return;

  // Don't set any extended partitions
  if (entry.entryType == partition_extended)
    return;

  // Loop through the partition records
  for (count = 0; count < numberPartitions; count ++)
    {
      if (getPartitionEntry(count, &entry) < 0)
	break;

      if (entry.sliceId == sliceId)
	{
	  if (entry.active)
	    // Clear the active flag
	    entry.active = 0;
	  else if (!entry.active)
	    // Set the active flag
	    entry.active = 1;

	  changesPending += 1;
	}
      else
	// Unset active flag
	entry.active = 0;

      setPartitionEntry(count, &entry);
    }

  // Regenerate the slice list
  makeSliceList();
}


static void delete(int partition)
{
  slice deleteEntry;

  if (getPartitionEntry(partition, &deleteEntry) < 0)
    return;

  // Don't delete extended partitions this way
  if (deleteEntry.entryType == partition_extended)
    return;

  if (deleteEntry.active)
    warning("Deleting active partition.  You should set another partition "
	    "active.");

  deletePartitionEntry(partition);

  // Regenerate the slice list
  makeSliceList();

  if (selectedSlice >= numberSlices)
    selectedSlice = (numberSlices - 1);    

  changesPending += 1;
}


static void format(slice *formatSlice)
{
  // Prompt, and format partition

  int status = 0;
  objectKey formatDialog = NULL;
  objectKey bannerDialog = NULL;
  componentParameters params;
  objectKey fsTypeRadio = NULL;
  objectKey okButton = NULL;
  objectKey cancelButton = NULL;
  windowEvent event;
  char *fsTypes[] = { "FAT", "EXT2" };
  int selectedType = 0;
  char tmpChar[160];

  if (changesPending)
    {
      error("A partition format cannot be undone, and it is required that\n"
	    "you write your other changes to disk before continuing.");
      return;
    }

  if (graphics)
    {
      sprintf(tmpChar, "Format partition %s", formatSlice->name1);
      formatDialog = windowNewDialog(window, tmpChar);

      bzero(&params, sizeof(componentParameters));
      params.gridWidth = 2;
      params.gridHeight = 1;
      params.padTop = 5;
      params.padLeft = 5;
      params.padRight = 5;
      params.orientationX = orient_center;
      params.orientationY = orient_middle;
      params.useDefaultForeground = 1;
      params.useDefaultBackground = 1;
      
      windowNewTextLabel(formatDialog, "Choose the filesystem type:", &params);

      // A radio button for the filesystem type
      params.gridY = 1;
      fsTypeRadio = windowNewRadioButton(formatDialog, 2, 1, fsTypes, 2 ,
					 &params);
      windowComponentSetEnabled(fsTypeRadio, 0);
      
      // Make 'OK' and 'cancel' buttons
      params.gridY = 2;
      params.gridWidth = 1;
      params.orientationX = orient_right;
      params.padBottom = 5;
      okButton = windowNewButton(formatDialog, "OK", NULL, &params);

      params.gridX = 1;
      params.orientationX = orient_left;
      cancelButton = windowNewButton(formatDialog, "Cancel", NULL, &params);
      
      // Make the window visible
      windowSetResizable(formatDialog, 0);
      windowCenterDialog(window, formatDialog);
      windowSetVisible(formatDialog, 1);

      while(1)
	{
	  // Check for our OK button
	  status = windowComponentEventGet(okButton, &event);
	  if ((status > 0) && (event.type == EVENT_MOUSE_LEFTUP))
	    {
	      selectedType = windowComponentGetSelected(fsTypeRadio);
	      windowDestroy(formatDialog);
	      break;
	    }

	  // Check for window close, or our Cancel button
	  if (((windowComponentEventGet(formatDialog, &event) > 0) &&
	       (event.type == EVENT_WINDOW_CLOSE)) ||
	      ((windowComponentEventGet(cancelButton, &event) > 0) &&
	       (event.type == EVENT_MOUSE_LEFTUP)))
	    {
	      windowDestroy(formatDialog);
	      return;
	    }
	  
	  // Done
	  multitaskerYield();
	}
    }
  else
    {
      // Don't bother with this for the moment since we really don't offer
      // any choice other than FAT.  The graphical version above is merely
      // eye candy for the moment.
    }

  sprintf(tmpChar, "Format partition %s as %s?\n(This change cannot be "
	  "undone)", formatSlice->name1, fsTypes[selectedType]);
  if (yesOrNo(tmpChar))
    {
      if (graphics)
	bannerDialog =
	  windowNewBannerDialog(window, "Formatting", "Format in progress, "
				"please wait...");

      // Do the format
      sprintf(tmpChar, "/programs/format -s -t %s %s", fsTypes[selectedType],
	      formatSlice->name1);
      status = system(tmpChar);

      if (bannerDialog)
	windowDestroy(bannerDialog);

      if (status < 0)
	error("Error during format");
      else
	{
	  sprintf(tmpChar, "Format complete");
	  if (graphics)
	    windowNewInfoDialog(window, "Success", tmpChar);
	  else
	    {
	      printf("%s\n", tmpChar);
	      pause();
	    }
	}
    }

  return;
}


static void hide(int partition)
{
  slice entry;

  if (getPartitionEntry(partition, &entry) < 0)
    return;

  if (PARTITION_TYPEID_IS_HIDDEN(entry.typeId))
    entry.typeId -= 0x10;
  else if (PARTITION_TYPEID_IS_HIDEABLE(entry.typeId))
    entry.typeId += 0x10;
  else
    return;

  changesPending += 1;
  setPartitionEntry(partition, &entry);

  // Regenerate the slice list
  makeSliceList();
}


static void info(int sliceNumber)
{
  // Print info about a slice
  
  slice *slc = &slices[sliceNumber];
  char buff[1024];

  if (slc->typeId)
    sprintf(buff, "PARTITION %s INFO:\n\nActive : %s\nType ID : %02x\n",
	    slc->name1, (slc->active? "yes" : "no"), slc->typeId);
  else
    sprintf(buff, "EMPTY SPACE INFO:\n\n");

  sprintf((buff + strlen(buff)), "Starting Cyl/Hd/Sect: %u/%u/%u\nEnding "
	  "Cyl/Hd/Sect  : %u/%u/%u\nLogical start sector: %u\nLogical size: "
	  "%u", slc->startCylinder, slc->startHead, slc->startSector,
	  slc->endCylinder, slc->endHead, slc->endSector, slc->startLogical,
	  slc->sizeLogical);

  if (graphics)
    windowNewInfoDialog(window, "Info", buff);
  else
    {
      printf("\n%s\n", buff);
      pause();
    }
}


static void listTypes(void)
{
  int status = 0;
  partitionType *types;
  int numberTypes = 0;
  objectKey typesWindow = NULL;
  componentParameters params;
  objectKey textArea = NULL;
  objectKey oldOutput = NULL;
  objectKey dismissButton = NULL;
  windowEvent event;
  int count;

  // Get the list of types
  types = diskGetPartTypes();

  // Count them
  for (count = 0; (types[count].code != 0); count ++)
    numberTypes += 1;

  if (graphics)
    {
      // Create a new window, not a modal dialog
      typesWindow = windowNewDialog(window, PARTTYPES);
      if (typesWindow != NULL)
	{
	  bzero(&params, sizeof(componentParameters));
	  params.gridWidth = 1;
	  params.gridHeight = 1;
	  params.padTop = 5;
	  params.padLeft = 5;
	  params.padRight = 5;
	  params.orientationX = orient_center;
	  params.orientationY = orient_middle;
	  params.useDefaultForeground = 1;
	  params.useDefaultBackground = 1;

	  // Make a text area for our info
	  textArea =
	    windowNewTextArea(typesWindow, 60, ((numberTypes / 2) + 2), 0,
			      &params);

	  // Make a dismiss button
	  params.gridY = 1;
	  params.padBottom = 5;
	  dismissButton =
	    windowNewButton(typesWindow, "Dismiss", NULL, &params);
	  windowComponentFocus(dismissButton);

	  // Save old text output and set the text output to our new area
	  oldOutput = multitaskerGetTextOutput();
	  windowSetTextOutput(textArea);

	  windowSetHasCloseButton(typesWindow, 0);
	  windowCenterDialog(window, typesWindow);
	  windowSetVisible(typesWindow, 1);
	}
    }

  else
    printf("\n%s:\n", PARTTYPES);

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

  if (graphics)
    {
      // Restore old text output
      multitaskerSetTextOutput(processId, oldOutput);

      while(1)
	{
	  // Check for the dismiss button
	  status = windowComponentEventGet(dismissButton, &event);
	  if ((status > 0) && (event.type == EVENT_MOUSE_LEFTUP))
	    break;
	}
      windowDestroy(typesWindow);
    }

  else
    pause();
}


static int setType(int partition)
{
  int status = 0;
  char tmpChar[80];
  char code[8];
  int newCode = 0;
  slice entry;
  partitionType *types;
  int count;

  // Load the partition
  status = getPartitionEntry(partition, &entry);
  if (status < 0)
    return (status);

  // Don't set the type on extended partitions
  if (entry.entryType == partition_extended)
    return (status = ERR_INVALID);

  while(1)
    {
      bzero(tmpChar, 80);
      bzero(code, 8);

      strcpy(tmpChar, "Enter the hexadecimal code to set as the type");
      if (graphics)
	{
	  strcat(tmpChar, " (or 'L' to list)");
	  status = windowNewPromptDialog(window, "Partition type", tmpChar, 1,
					 8, code);
	}
      else
	{
	  printf("\n%s ('L' to list, 'Q' to quit):\n-> ", tmpChar);
	  status = readLine("0123456789AaBbCcDdEeFfLlQq", code, 8);
	  printf("\n");
	}

      if (status < 0)
	return (status);

      if ((code[0] == 'L') || (code[0] == 'l'))
	{
	  listTypes();
	  continue;
	}
      if ((code[0] == '\0') || (code[0] == 'Q') || (code[0] == 'q'))
	return (status = ERR_NODATA);

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
	  error("Unsupported partition type %x", newCode);
	  return (status = ERR_INVALID);
	}

      // Change the value
      entry.typeId = newCode;

      // Set the partition
      setPartitionEntry(partition, &entry);

      // Redo the slice strings
      makeSliceList();

      changesPending += 1;
      return (status = 0);
    }
}


static void undo(void)
{
  // Undo changes
  if (changesPending)
    {
      changesPending = 0;

      scanPartitionTable(selectedDisk, mainTable, NULL);
      
      // Regenerate the slice list
      makeSliceList();

      selectedSlice = 0;
    }
}


static void writeChanges(int confirm)
{
  int status = 0;

  if (changesPending)
    {
      if (confirm && !yesOrNo("Committing changes to disk.  Are you SURE?"))
	return;

      // Write out the partition table
      status = writePartitionTable(selectedDisk, mainTable);
      if (status < 0)
	error("Unable to write the partition table of %s.",
	      selectedDisk->name);
    }
}


static int move(int sliceId)
{
  int status = 0;
  slice entry;
  int sliceNumber = -1;
  unsigned moveRange[] = { -1, -1 };
  char number[10];
  unsigned newStartCylinder = 0;
  unsigned newStartLogical = 0;
  int moveLeft = 0;
  unsigned char *buffer = NULL;
  unsigned endLogical = 0;
  unsigned srcSector = 0;
  unsigned destSector = 0;
  unsigned sectorsToCopy = 0;
  unsigned sectorsPerOp = 0;
  char tmpChar[160];
  int count;

  // Stuff for a progress dialog when in graphics mode
  objectKey dialogWindow = NULL;
  componentParameters params;
  objectKey progressBar = NULL;

  if (sliceId < 0)
    return (status = ERR_INVALID);

  status = getPartitionEntry(sliceId, &entry);
  if (status < 0)
    return (status);

  if (changesPending)
    {
      error("A partition move cannot be undone, and must be committed\n"
	    "to disk immediately.  You need to write your other changes\n"
	    "to disk before continuing.");
      return (status = 0);
    }

  // Find out which slice it is in our list
  for (count = 0; count < numberSlices; count ++)
    if (slices[count].sliceId == sliceId)
      {
	sliceNumber = count;
	break;
      }

  if (sliceNumber < 0)
    return (status = sliceNumber);

  // If there are no empty spaces on either side, error
  if (((sliceNumber == 0) || slices[sliceNumber - 1].typeId) &&
      ((sliceNumber == (numberSlices - 1)) || slices[sliceNumber + 1].typeId))
    {
      error("No empty space on either side!");
      return (status = ERR_INVALID);
    }

  // Figure out the ranges of cylinders we can move to in both directions
  if ((sliceNumber > 0) && !(slices[sliceNumber - 1].typeId))
    {
      moveRange[0] = slices[sliceNumber - 1].startCylinder;
      moveRange[1] = slices[sliceNumber - 1].endCylinder;
    }
  if ((sliceNumber < (numberSlices - 1)) && !(slices[sliceNumber + 1].typeId))
    {
      if (moveRange[0] == (unsigned) -1)
	moveRange[0] = (entry.startCylinder + 1) ;
      moveRange[1] = (slices[sliceNumber + 1].endCylinder -
		      (entry.endCylinder - entry.startCylinder));
    }

  while (1)
    {
      sprintf(tmpChar, "Enter starting cylinder:\n(%u-%u)", moveRange[0],
	      moveRange[1]);

      if (graphics)
	{
	  newStartCylinder = getNumberDialog("Starting cylinder", tmpChar);
	  if ((int) newStartCylinder < 0)
	    return (status = (int) newStartCylinder);
	}
      else
	{
	  printf("\n%s or 'Q' to quit\n-> ", tmpChar);
	  
	  status = readLine("0123456789Qq", number, 10);
	  if (status < 0)
	    continue;
      
	  if ((number[0] == 'Q') || (number[0] == 'q'))
	    return (status = 0);
	  
	  newStartCylinder = atoi(number);
	}

      // Make sure the start cylinder is legit
      if ((newStartCylinder < moveRange[0]) ||
	  (newStartCylinder > moveRange[1]))
	{
	  error("Starting cylinder is not valid");
	  continue;
	}

      break;
    }

  // If it's not moving, quit of course
  if (newStartCylinder == entry.startCylinder)
    return (status = 0);

  // The new starting logical sector
  newStartLogical = (newStartCylinder * cylinderSectors);

  // Don't overwrite the 'boot track'
  if (newStartLogical == 0)
    newStartLogical += selectedDisk->sectorsPerCylinder;

  // Which direction?
  if (newStartLogical < entry.startLogical)
    moveLeft = 1;
 
  sprintf(tmpChar, "Moving partition from cylinder %u to cylinder %u.\n"
	  "Please use this feature with caution; it is not\nwell tested.  "
	  "Continue?", entry.startCylinder, newStartCylinder);
  if (graphics)
    {
      if (!windowNewQueryDialog(window, "New Feature", tmpChar))
	return (status = 0);
    }
  else
    {
      if (!yesOrNo(tmpChar))
	return (status = 0);
    }

  sprintf(tmpChar, "Moving %u sectors, %u Mb",
	  entry.sizeLogical, (entry.sizeLogical /
			      (1048576 / selectedDisk->sectorSize)));
  if (graphics)
    {
      dialogWindow = windowNewDialog(window, "Moving...");
	      
      bzero(&params, sizeof(componentParameters));
      params.gridWidth = 1;
      params.gridHeight = 1;
      params.padTop = 5;
      params.padLeft = 5;
      params.padRight = 5;
      params.orientationX = orient_center;
      params.orientationY = orient_middle;
      params.useDefaultForeground = 1;
      params.useDefaultBackground = 1;
      windowNewTextLabel(dialogWindow, tmpChar, &params);
      
      params.gridY = 1;
      progressBar = windowNewProgressBar(dialogWindow, &params);
      
      windowSetHasCloseButton(dialogWindow, 0);
      windowSetResizable(dialogWindow, 0);
      windowCenterDialog(window, dialogWindow);
      windowSetVisible(dialogWindow, 1);
    }
  else
    printf("\n%s\n", tmpChar);

  sectorsPerOp = cylinderSectors;
  if (moveLeft && ((entry.startLogical - newStartLogical) < sectorsPerOp))
    sectorsPerOp = (entry.startLogical - newStartLogical);
  else if (!moveLeft && (newStartLogical - entry.startLogical) < sectorsPerOp)
    sectorsPerOp = (newStartLogical - entry.startLogical);

  if (moveLeft)
    {
      srcSector = entry.startLogical;
      destSector = newStartLogical;
    }
  else
    {
      srcSector =
	(entry.startLogical + ((entry.sizeLogical - sectorsPerOp) - 1));
      destSector =
	(newStartLogical + ((entry.sizeLogical - sectorsPerOp) - 1));
    }

  sectorsToCopy = entry.sizeLogical;
  
  // Get a memory buffer to copy data to/from
  buffer = memoryGet((sectorsPerOp * selectedDisk->sectorSize),
		     "partition copy buffer");
  if (buffer == NULL)
    {
      error("Unable to allocate memory");
      return (status = ERR_MEMORY);
    }

  // Copy the data
  while (sectorsToCopy > 0)
    {
      if (graphics)
	// Update the progress indicator
	windowComponentSetData(progressBar,
			     (void *)(((entry.sizeLogical - sectorsToCopy) *
				       100) / entry.sizeLogical), 1);
      else
	{
	  textSetColumn(2);
	  printf("%d%% ", (((entry.sizeLogical - sectorsToCopy) *
                                       100) / entry.sizeLogical));
	}

      if (sectorsToCopy < sectorsPerOp)
	sectorsPerOp = sectorsToCopy;

      // Read from source
      status =
	diskReadSectors(selectedDisk->name, srcSector, sectorsPerOp, buffer);
      if (status < 0)
	{
	  error("Read error %d reading sectors %u-%u from disk %s",
		status, srcSector, ((srcSector + sectorsPerOp) - 1),
		selectedDisk->name);
	  goto out;
	}

      // Write to destination
      status =
	diskWriteSectors(selectedDisk->name, destSector, sectorsPerOp, buffer);
      if (status < 0)
	{
	  error("Write error %d writing sectors %u-%u to disk %s",
		status, destSector, ((destSector + sectorsPerOp) - 1),
		selectedDisk->name);
	  goto out;
	}

      sectorsToCopy -= sectorsPerOp;
      
      if (moveLeft)
	{
	  srcSector += min(sectorsToCopy, sectorsPerOp);
	  destSector += min(sectorsToCopy, sectorsPerOp);
	}
      else
	{
	  srcSector -= min(sectorsToCopy, sectorsPerOp);
	  destSector -= min(sectorsToCopy, sectorsPerOp);
	}
    }

  // Set the new partition data
  entry.startCylinder = newStartCylinder;
  entry.startLogical = newStartLogical;
  entry.startHead =
    ((newStartLogical % cylinderSectors) / selectedDisk->sectorsPerCylinder);
  entry.startSector = (((newStartLogical % cylinderSectors) %
			selectedDisk->sectorsPerCylinder) + 1);

  endLogical = (entry.startLogical + (entry.sizeLogical - 1));
  entry.endCylinder = (endLogical / cylinderSectors);
  entry.endHead = 
    ((endLogical % cylinderSectors) / selectedDisk->sectorsPerCylinder);
  entry.endSector =
    (((endLogical % cylinderSectors) % selectedDisk->sectorsPerCylinder) + 1); 

  setPartitionEntry(sliceId, &entry);

  // Regenerate the slice list
  makeSliceList();

  // Re-select the slice that we moved
  for (count = 0; count < numberSlices; count ++)
    if (slices[count].sliceId == entry.sliceId)
      {
	selectedSlice = count;
	break;
      }

  changesPending += 1;

  // Write everything
  writeChanges(0);

 out:
  // Release memory
  memoryRelease(buffer);

  if (graphics)
    {
      windowComponentSetData(progressBar, (void *) 100, 1);
      windowDestroy(dialogWindow);
    }
  else
    {
      textSetColumn(2);
      printf("100%%\n");
    }

  return (status);
}


static partEntryType canCreate(int sliceNumber)
{
  // This will return an entryType enumeration if, given a slice number
  // representing free space, a partition can be created there.  If not, it
  // returns error.  Otherwise it returns the enumeration representing the
  // type that can be created: primary or logical.  Primary implies that
  // a logical one can be created if preferred.

  partEntryType returnType = partition_any;
  int extended = 0;
  int count;

  if (slices[sliceNumber].typeId)
    // Not empty space
    return (returnType = ERR_INVALID);

  // Determine whether there is an extended partition in the main table
  for (count = 0; count < numberSlices; count ++)
    if (slices[count].typeId && ISLOGICAL(&slices[count]))
      {
	extended = 1;
	break;
      }

  if (extended)
    {
      // Does part of the extended partition precede or follow our empty
      // space?

      if ((sliceNumber == 0) && !ISLOGICAL(&slices[sliceNumber + 1]))
	returnType = partition_primary;

      if ((sliceNumber == (numberSlices - 1)) &&
	  !ISLOGICAL(&slices[sliceNumber - 1]))
	returnType = partition_primary;

      if (((sliceNumber < (numberSlices - 1)) &&
	   !ISLOGICAL(&slices[sliceNumber + 1])) &&
	  ((sliceNumber > 0) && !ISLOGICAL(&slices[sliceNumber - 1])))
	returnType = partition_primary;

      // For the moment, we don't allow creation of a logical partition
      // at the front of the extended partition.
      if ((sliceNumber < (numberSlices - 1)) &&
	  ISLOGICAL(&slices[sliceNumber + 1]))
	returnType = partition_primary;

      // If the preceding space is extended, is the following space as well?
      if (((sliceNumber < (numberSlices - 1)) &&
	   ISLOGICAL(&slices[sliceNumber + 1])) &&
	  ((sliceNumber > 0) && ISLOGICAL(&slices[sliceNumber - 1])))
	// We can only do logical here
	returnType = partition_logical;
    }

  // If we don't have to do logical, check whether the main table is full
  if ((returnType != partition_logical) &&
      (mainTable->numberEntries >= DISK_MAX_PRIMARY_PARTITIONS))
    {
      // If logical is possible, then logical it will be
      if (extended && (returnType == partition_any))
	returnType = partition_logical;
      else	  
	// Can't do logical, and the main table is full of entries
	returnType = ERR_NOFREE;
    }

  // Can't create a logical partition solely on cylinder 0
  if ((slices[sliceNumber].startCylinder == 0) &&
      (slices[sliceNumber].endCylinder == 0))
    {
      if ((returnType == partition_any) || (returnType == partition_primary))
	returnType = partition_primary;
      else
	returnType = ERR_NOFREE;
    }

  return (returnType);
}


static void create(int sliceNumber)
{
  enum { units_normal, units_mb, units_cylsize } units = units_normal;

  #define STARTCYL_MESSAGE "Enter starting cylinder (%u-%u)"
  #define ENDCYL_MESSAGE "Enter ending cylinder (%u-%u)\nor size in " \
   "megabytes with 'm' (1m-%um),\nor size in cylinders with 'c' (1c-%uc)"

  int status = 0;
  char startCyl[10];
  char endCyl[10];
  slice extendedEntry;
  slice newEntry;
  unsigned startCylinder, endCylinder;
  partitionTable *extendedTable = NULL;
  partitionTable *table = NULL;
  int newSliceNumber = -1;
  objectKey createDialog = NULL;
  objectKey primLogRadio = NULL;
  objectKey startCylField = NULL;
  objectKey endCylField = NULL;
  objectKey okButton = NULL;
  objectKey cancelButton = NULL;
  componentParameters params;
  windowEvent event;
  char tmpChar[256];
  int count;

  bzero(&newEntry, sizeof(slice));
  newEntry.typeId = 0x01;
  startCylinder = slices[sliceNumber].startCylinder;
  endCylinder = slices[sliceNumber].endCylinder;

  while (1)
    {
      // See if we can create a slice here, and if so, what type?
      newEntry.entryType = canCreate(sliceNumber);
      if ((int) newEntry.entryType < 0)
	{
	  // The partition table is full of entries, in its current
	  // configuration.
	  error("The partition table is full of primary partitions.  Use "
		"more\nlogical partitions in order to create more.");
	  return;
	}

      if (graphics)
	{
	  createDialog = windowNewDialog(window, "Create partition");

	  bzero(&params, sizeof(componentParameters));
	  params.gridWidth = 1;
	  params.gridHeight = 1;
	  params.padTop = 5;
	  params.padLeft = 5;
	  params.padRight = 5;
	  params.orientationX = orient_right;
	  params.orientationY = orient_middle;
	  params.useDefaultForeground = 1;
	  params.useDefaultBackground = 1;
      
	  windowNewTextLabel(createDialog, "Partition\ntype:", &params);

	  // A radio to select 'primary' or 'logical'
	  params.gridX = 1;
	  params.orientationX = orient_left;
	  primLogRadio = windowNewRadioButton(createDialog, 2, 1, (char *[])
	      { "Primary", "Logical" }, 2 , &params);
	  if (newEntry.entryType != partition_any)
	    {
	      windowComponentSetSelected(primLogRadio,
					 (int) newEntry.entryType);
	      windowComponentSetEnabled(primLogRadio, 0);
	    }

	  // A label and field for the starting cylinder
	  sprintf(tmpChar, STARTCYL_MESSAGE, startCylinder, endCylinder);
	  params.gridX = 0;
	  params.gridY = 1;
	  params.gridWidth = 2;
	  windowNewTextLabel(createDialog, tmpChar, &params);

	  params.gridY = 2;
	  params.hasBorder = 1;
	  startCylField = windowNewTextField(createDialog, 10, &params);

	  // A label and field for the ending cylinder
	  sprintf(tmpChar, ENDCYL_MESSAGE, startCylinder, endCylinder,
		  cylsToMb(selectedDisk, (endCylinder - startCylinder + 1)),
		  (endCylinder - startCylinder + 1));
	  params.gridY = 3;
	  params.hasBorder = 0;
	  windowNewTextLabel(createDialog, tmpChar, &params);

	  params.gridY = 4;
	  params.hasBorder = 1;
	  endCylField = windowNewTextField(createDialog, 10, &params);

	  // Make 'OK' and 'cancel' buttons
	  params.gridY = 5;
	  params.gridWidth = 1;
	  params.padBottom = 5;
	  params.orientationX = orient_right;
	  params.hasBorder = 0;
	  okButton = windowNewButton(createDialog, "OK", NULL, &params);

	  params.gridX = 1;
	  params.orientationX = orient_left;
	  cancelButton =
	    windowNewButton(createDialog, "Cancel", NULL, &params);
      
	  // Make the window visible
	  windowSetResizable(createDialog, 0);
	  windowCenterDialog(window, createDialog);
	  windowSetVisible(createDialog, 1);

	  while(1)
	    {
	      // Check for the OK button
	      if ((windowComponentEventGet(okButton, &event) > 0) &&
		  (event.type == EVENT_MOUSE_LEFTUP))
		break;

	      // Check for the Cancel button
	      if ((windowComponentEventGet(cancelButton, &event) > 0) &&
		  (event.type == EVENT_MOUSE_LEFTUP))
		{
		  windowDestroy(createDialog);
		  return;
		}

	      // Check for window close events
	      if ((windowComponentEventGet(createDialog, &event) > 0) &&
		  (event.type == EVENT_WINDOW_CLOSE))
		{
		  windowDestroy(createDialog);
		  return;
		}

	      // Check for keyboard events
	      if ((windowComponentEventGet(startCylField, &event) > 0) &&
		  (event.type == EVENT_KEY_DOWN) &&
		  (event.key == (unsigned char) 10))
		break;

	      if ((windowComponentEventGet(endCylField, &event) > 0) &&
		  (event.type == EVENT_KEY_DOWN) &&
		  (event.key == (unsigned char) 10))
		break;

	      // Done
	      multitaskerYield();
	    }

	  newEntry.entryType =
	    (partEntryType) windowComponentGetSelected(primLogRadio);

	  windowComponentGetData(startCylField, startCyl, 10);
	  windowComponentGetData(endCylField, endCyl, 10);
	  windowDestroy(createDialog);
	}

      else
	{
	  if (newEntry.entryType == partition_any)
	    {
	      // Does the user prefer primary or logical?
	      newEntry.entryType =
		vshCursorMenu("Choose the partition type:", 2,
			      (char *[]){ "primary", "logical" }, 0);
	      if ((int) newEntry.entryType < 0)
		return;
	    }
	  else
	    printf("\nCreating %s partition\n",
		   ((newEntry.entryType == partition_primary)? "primary" :
		    "logical"));

	  // Don't create an extended partition on the first cylinder
	  if ((newEntry.entryType == partition_logical) &&
	      (startCylinder == 0))
	    startCylinder = 1;

	  printf("\n"STARTCYL_MESSAGE", or 'Q' to quit:\n-> ", startCylinder,
		 endCylinder);
	  
	  status = readLine("0123456789Qq", startCyl, 10);
	  if (status < 0)
	    continue;
	  
	  if ((startCyl[0] == 'Q') || (startCyl[0] == 'q'))
	    return;

	  printf("\n"ENDCYL_MESSAGE", or 'Q' to quit:\n-> ",
		 atoi(startCyl), endCylinder,
		 cylsToMb(selectedDisk, (endCylinder - startCylinder + 1)),
		 (endCylinder - startCylinder + 1));
      
	  status = readLine("0123456789CcMmQq", endCyl, 10);
	  if (status < 0)
	    return;

	  if ((endCyl[0] == 'Q') || (endCyl[0] == 'q'))
	    return;
	}

      newEntry.startCylinder = atoi(startCyl);
      newEntry.startHead = 0;
      newEntry.startSector = 1;

      if (newEntry.startCylinder == 0)
	{
	  if (newEntry.entryType == partition_logical)
	    // Don't write a logical partition on the first cylinder of the
	    // disk
	    newEntry.startCylinder = 1;
	  else
	    // Don't write the first track
	    newEntry.startHead = 1;
	}

      // Make sure the start cylinder is legit
      if ((newEntry.startCylinder < startCylinder) ||
	  (newEntry.startCylinder > endCylinder))
	{
	  error("Invalid starting cylinder number");
	  continue;
	}
      
      newEntry.startLogical =
	((newEntry.startCylinder * selectedDisk->heads *
	  selectedDisk->sectorsPerCylinder) +
	 (newEntry.startHead * selectedDisk->sectorsPerCylinder));

      count = (strlen(endCyl) - 1);

      if ((endCyl[count] == 'M') || (endCyl[count] == 'm'))
	{
	  units = units_mb;
	  endCyl[count] = '\0';
	}
      else if ((endCyl[count] == 'C') || (endCyl[count] == 'c'))
	{
	  units = units_cylsize;
	  endCyl[count] = '\0';
	}

      count = atoi(endCyl);

      switch (units)
	{
	case units_mb:
	  newEntry.endCylinder =
	    (newEntry.startCylinder + mbToCyls(selectedDisk, count) - 1);
	  break;
	case units_cylsize:
	  newEntry.endCylinder = (newEntry.startCylinder + count - 1);
	  break;
	default:
	  newEntry.endCylinder = count;
	  break;
	}

      if ((newEntry.endCylinder < startCylinder) ||
	  (newEntry.endCylinder > endCylinder))
	{
	  error("Invalid ending cylinder number");
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

  if (newEntry.entryType == partition_logical)
    {
      // Create the extended partition.

      table = findLastTable(mainTable);
      if (table == NULL)
	return;

      // Create an extended partition entry in this table with the same
      // (current) parameters as the new partition.

      memcpy(&extendedEntry, &newEntry, sizeof(slice));
      extendedEntry.typeId = 0x0F;
      extendedEntry.entryType = partition_extended;
      
      extendedTable = malloc(sizeof(partitionTable));
      if (extendedTable == NULL)
	{
	  error("Unable to allocate a new extended table");
	  return;
	}
      extendedTable->startSector = extendedEntry.startLogical;
      extendedTable->maxEntries = 2;
      extendedTable->extended = (table->extended + 1);
      extendedEntry.extendedTable = extendedTable;

      addPartitionEntry(table, &extendedEntry);

      // Now we are working within our extended table
      table = extendedTable;

      // Adjust some values for the logical entry

      // Don't write the first track of the extended partition
      newEntry.startHead = 1;
      newEntry.startLogical += selectedDisk->sectorsPerCylinder;
      newEntry.sizeLogical -= selectedDisk->sectorsPerCylinder;
    }
  else
    table = mainTable;

  // Set the new entry before calling setType, since it needs the entry
  // to be in the table.
  newSliceNumber = addPartitionEntry(table, &newEntry);
  if (newSliceNumber < 0)
    {
      error("Unable to add new partition entry");
      return;
    }

  setType(slices[newSliceNumber].sliceId);

  return;
}


static void deleteAll(void)
{
  // This function completely nukes the partition entries (as opposed to
  // merely clearing type IDs, which is theoretically 'un-deletable').

  bzero(mainTable->entries, (DISK_MAX_PRIMARY_PARTITIONS * sizeof(slice)));
  mainTable->numberEntries = 0;
  numberPartitions = 0;

  // Regenerate the slice list
  makeSliceList();

  selectedSlice = 0;

  changesPending += 1;
}


static disk *chooseDiskDialog(void)
{
  // Graphical way of prompting for disk selection

  int status = 0;
  disk *retDisk = NULL;
  objectKey chooseWindow = NULL;
  componentParameters params;
  objectKey dList = NULL;
  objectKey okButton = NULL;
  objectKey cancelButton = NULL;
  windowEvent event;

  chooseWindow = windowNew(processId, "Choose Disk");

  bzero(&params, sizeof(componentParameters));
  params.gridWidth = 2;
  params.gridHeight = 1;
  params.padTop = 5;
  params.padLeft = 5;
  params.padRight = 5;
  params.orientationX = orient_center;
  params.orientationY = orient_middle;
  params.useDefaultForeground = 1;
  params.useDefaultBackground = 1;

  // Make a window list with all the disk choices
  dList = windowNewList(chooseWindow, numberDisks, 1, 0, diskStrings,
			   numberDisks, &params);

  // Make 'OK' and 'cancel' buttons
  params.gridY = 1;
  params.gridWidth = 1;
  params.padBottom = 5;
  params.orientationX = orient_right;
  okButton = windowNewButton(chooseWindow, "OK", NULL, &params);

  params.gridX = 1;
  params.orientationX = orient_left;
  cancelButton = windowNewButton(chooseWindow, "Cancel", NULL, &params);

  // Make the window visible
  windowSetHasMinimizeButton(chooseWindow, 0);
  windowSetHasCloseButton(chooseWindow, 0);
  windowSetResizable(chooseWindow, 0);
  windowSetVisible(chooseWindow, 1);

  while(1)
    {
      // Check for our OK button
      status = windowComponentEventGet(okButton, &event);
      if ((status > 0) && (event.type == EVENT_MOUSE_LEFTUP))
	{
	  retDisk = &(diskInfo[windowComponentGetSelected(dList)]);
	  break;
	}

      // Check for our Cancel button
      status = windowComponentEventGet(cancelButton, &event);
      if ((status > 0) && (event.type == EVENT_MOUSE_LEFTUP))
	break;

      // Done
      multitaskerYield();
    }

  windowDestroy(chooseWindow);

  return (retDisk);
}


static int copyDiskIoThread(int argc, char *argv[])
{
  // This thread polls for empty/full buffers and reads/writes them when
  // they're ready

  int status = 0;
  int reader = 0;
  ioThreadArgs *args = NULL;
  unsigned currentSector = 0;
  unsigned doSectors = 0;
  unsigned sectorsPerOp = 0;
  int currentBuffer = 0;
  int percentage = 0;
  int startSeconds = rtcUptimeSeconds();
  int elapsedSeconds = 0;
  int remainingSeconds = 0;
  char statusText[80];

  // Are we a reader thread or a writer thread?
  if (argc < 2)
    {
      error("IO thread argument count (%d) error", argc);
      status = ERR_ARGUMENTCOUNT;
      goto terminate;
    }

  if (!strcmp(argv[1], "reader"))
    reader = 1;
  else if (!strcmp(argv[1], "writer"))
    reader = 0;
  else
    {
      error("Invalid IO thread argument \"%s\"", argv[0]);
      status = ERR_INVALID;
      goto terminate;
    }

  if (reader)
    args = &readerArgs;
  else
    args = &writerArgs;

  currentSector = args->startSector;
  doSectors = args->numSectors;

  // Calculate the sectors per operation for the disk
  sectorsPerOp = (args->buffer->bufferSize / args->theDisk->sectorSize);

  while (doSectors && !ioThreadsTerminate)
    {
      if ((reader && args->buffer->buffer[currentBuffer].full) ||
	  (!reader && !args->buffer->buffer[currentBuffer].full))
	{
	  // For good behaviour
	  multitaskerYield();
	  continue;
	}

      // For the last op, reset the 'sectorsPerOp' values
      if (sectorsPerOp > doSectors)
	sectorsPerOp = doSectors;

      if (reader)
	status =
	  diskReadSectors(args->theDisk->name, currentSector, sectorsPerOp,
			  args->buffer->buffer[currentBuffer].data);
      else
	status =
	  diskWriteSectors(args->theDisk->name, currentSector, sectorsPerOp,
			   args->buffer->buffer[currentBuffer].data);
      if (status < 0)
	{
	  error("Error %d %s %u sectors at %u %s disk %s", status,
		(reader? "reading" : "writing"), sectorsPerOp,
		currentSector, (reader? "from" : "to"), args->theDisk->name);
	  goto terminate;
	}

      if (reader)
	args->buffer->buffer[currentBuffer].full = 1;
      else
	args->buffer->buffer[currentBuffer].full = 0;

      currentSector += sectorsPerOp;
      doSectors -= sectorsPerOp;

      if (args->showProgress)
	{
	  percentage =
	    (((currentSector - args->startSector) * 100) / args->numSectors);
	  elapsedSeconds = (rtcUptimeSeconds() - startSeconds);
	  remainingSeconds =
	    ((elapsedSeconds * (doSectors / sectorsPerOp)) /
	     ((currentSector - args->startSector) / sectorsPerOp));
	  sprintf(statusText, "Time remaining: %d:%02d:%02d",
		  (remainingSeconds / 3600), ((remainingSeconds % 3600) / 60),
		  (remainingSeconds % 60));

	  if (graphics)
	    {
	      if (args->progressBar)
		// Update the progress indicator
		windowComponentSetData(args->progressBar, (void *) percentage,
				       1);
	      if (args->statusLabel)
		// Update the status label
		windowComponentSetData(args->statusLabel, statusText,
				       strlen(statusText));
	    }
	  else
	    {
	      textSetColumn(2);
	      printf("%d%%  (%s)", percentage, statusText);
	    }
	}

      if (currentBuffer == 0)
	currentBuffer = 1;
      else
	currentBuffer = 0;
    }
 
  // Indicate that we're finished
  ioThreadsFinished += 1;

  if (!ioThreadsTerminate)
    {
      // Wait for both to be finished
      while (ioThreadsFinished < 2)
	multitaskerYield();
    }

  status = 0;

 terminate:
  multitaskerTerminate(status);
  // Compiler happy
  while(1);
}


static void clearDiskLabel(disk *theDisk)
{
  int count;

  // Find it
  for (count = 0; count < numberDisks; count ++)
    if (&diskInfo[count] == theDisk)
      {
	selectedDiskNumber = count;
	selectedDisk = theDisk;
	bzero(mainTable->entries,
	      (DISK_MAX_PRIMARY_PARTITIONS * sizeof(slice)));
	mainTable->numberEntries = 0;
	changesPending = 1;
	writeChanges(0);
	break;
      }
}


static int copyDisk(void)
{
  int status = 0;
  int diskNumber = 0;
  disk *srcDisk = NULL;
  disk *destDisk = NULL;
  char character[2];
  slice entry;
  unsigned lastUsedSector = 0;
  ioBuffer buffer;
  int readerPID = 0;
  int writerPID = 0;
  int cancelled = 0;
  char tmpChar[160];
  int count;

  // Stuff for a progress dialog when in graphics mode
  objectKey dialogWindow = NULL;
  componentParameters params;
  objectKey progressBar = NULL;
  objectKey statusLabel = NULL;
  objectKey cancelButton = NULL;
  objectKey cancelDialog = NULL;
  windowEvent event;

  if (numberDisks < 2)
    {
      error("No other disks to copy to");
      return (status = ERR_NOSUCHENTRY);
    }

  srcDisk = selectedDisk;

  // If there's only one other disk, select it automatically
  if (numberDisks == 2)
    {
      for (count = 0; count < numberDisks; count ++)
	if (&diskInfo[count] != srcDisk)
	  {
	    destDisk = &diskInfo[count];
	    break;
	  }
    }

  // Else make the user choose one
  else
    {
      while(1)
	{
	  if (graphics)
	    {
	      destDisk = chooseDiskDialog();
	      if (destDisk == NULL)
		return (status = 0);

	      diskNumber = destDisk->deviceNumber;
	    }
	  else
	    {
	      printf("\nPlease choose the disk to copy to ('Q' to quit):\n");
	      printDisks();
	      printf("\n->");
	      
	      character[0] = readKey("0123456789Qq", 0);
	      if ((character[0] == 0) ||
		  (character[0] == 'Q') || (character[0] == 'q'))
		return (status = 0);
	      character[1] = '\0';
	      
	      diskNumber = atoi(character);
	  
	      // Loop through the disks and make sure it's legit
	      for (count = 0; count < numberDisks; count ++)
		if (diskInfo[count].deviceNumber == diskNumber)
		  destDisk = &diskInfo[count];
	    }
	
	  if (destDisk == srcDisk)
	    {
	      error("Not much point in copying a disk to itself!");
	      continue;
	    }
	  
	  else if (destDisk != NULL)
	    break;
	
	  error("Invalid disk %d.", diskNumber);
	}
    }

  // We have a source disk and a destination disk.
  sprintf(tmpChar, "Copy disk %s to disk %s.\nWARNING: THIS WILL DESTROY ALL "
	  "DATA ON DISK %s.\nARE YOU SURE YOU WANT TO DO THIS?", srcDisk->name,
	  destDisk->name, destDisk->name);
  if (!yesOrNo(tmpChar))
    return (status = 0);

  // We will copy everything up to the end of the last partition (not much
  // point in copying a bunch of unused space, even though it's potentially
  // conceivable that someone, somewhere might want to do that).  Find out
  // the logical sector number of the end of the last partition.
  for (count = 0; count < numberPartitions; count ++)
    {
      if (getPartitionEntry(count, &entry) < 0)
	break;

      if ((entry.startLogical + entry.sizeLogical - 1) > lastUsedSector)
	lastUsedSector = (entry.startLogical + entry.sizeLogical - 1);
    }
  
  if (lastUsedSector == 0)
    {
      if (!yesOrNo("No partitions on the disk.  Do you want to copy the "
		   "whole\ndisk anyway?"))
	return (status = 0);
      
      lastUsedSector = (srcDisk->numSectors - 1);
    }
  
  // Make sure that the destination disk can hold the data
  if (lastUsedSector >= destDisk->numSectors)
    {
      sprintf(tmpChar, "Disk %s is smaller than the amount of data on disk "
	      "%s.\nIf you wish, you can continue and copy the data that "
	      "will\nfit.  Don't do this unless you're sure you know what "
	      "you're\ndoing.  CONTINUE?", destDisk->name, srcDisk->name);
      if (!yesOrNo(tmpChar))
	return (status = 0);
      printf("\n");
      
      lastUsedSector = (destDisk->numSectors - 1);
    }
  
  // Set up the memory buffer to copy data to/from
  bzero(&buffer, sizeof(ioBuffer));
  buffer.bufferSize = COPYBUFFER_SIZE;
  // This loop will allow us to try successively smaller memory buffer
  // allocations, so that we can start by trying to allocate a large amount
  // of memory, but not failing unless we're totally out of memory
  while((buffer.buffer[0].data == NULL) || (buffer.buffer[1].data == NULL))
    {
      buffer.buffer[0].data = memoryGet(buffer.bufferSize,
	"disk copy buffer 1");
      buffer.buffer[1].data = memoryGet(buffer.bufferSize,
	"disk copy buffer 2");

      if ((buffer.buffer[0].data == NULL) || (buffer.buffer[1].data == NULL))
	{
	  if (buffer.buffer[0].data)
	    {
	      memoryRelease(buffer.buffer[0].data);
	      buffer.buffer[0].data = NULL;
	    }
	  if (buffer.buffer[1].data)
	    {
	      memoryRelease(buffer.buffer[1].data);
	      buffer.buffer[1].data = NULL;
	    }

	  buffer.bufferSize /= 2;
	  if (buffer.bufferSize < 65535)
	    {
	      error("Unable to allocate memory buffer!");
	      return (status = ERR_MEMORY);
	    }
	}
    }

  sprintf(tmpChar, "Copying %u sectors, %u Mb", (lastUsedSector + 1),
	  ((lastUsedSector + 1) / (1048576 / srcDisk->sectorSize)));
  
  if (graphics)
    {
      dialogWindow = windowNewDialog(window, "Copying...");
      
      bzero(&params, sizeof(componentParameters));
      params.gridWidth = 1;
      params.gridHeight = 1;
      params.padTop = 5;
      params.padLeft = 5;
      params.padRight = 5;
      params.orientationX = orient_center;
      params.orientationY = orient_middle;
      params.useDefaultForeground = 1;
      params.useDefaultBackground = 1;
      windowNewTextLabel(dialogWindow, tmpChar, &params);
      
      params.gridY = 1;
      progressBar = windowNewProgressBar(dialogWindow, &params);
      
      params.gridY = 2;
      statusLabel =
	windowNewTextLabel(dialogWindow, "Time remaining: ?:??:??", &params);
      
      params.gridY = 3;
      params.padBottom = 5;
      cancelButton = windowNewButton(dialogWindow, "Cancel", NULL, &params);
      
      windowSetHasCloseButton(dialogWindow, 0);
      windowSetResizable(dialogWindow, 0);
      windowCenterDialog(window, dialogWindow);
      windowSetVisible(dialogWindow, 1);
    }
  else
    printf("\n%s\n", tmpChar);

  // Set up and start our IO threads

  bzero(&readerArgs, sizeof(ioThreadArgs));
  readerArgs.theDisk = srcDisk;
  readerArgs.startSector = 0;
  readerArgs.numSectors = (lastUsedSector + 1);
  readerArgs.buffer = &buffer;
  readerArgs.showProgress = 0;
  
  bzero(&writerArgs, sizeof(ioThreadArgs));
  writerArgs.theDisk = destDisk;
  writerArgs.startSector = 0;
  writerArgs.numSectors = (lastUsedSector + 1);
  writerArgs.buffer = &buffer;
  writerArgs.showProgress = 1;
  writerArgs.progressBar = progressBar;
  writerArgs.statusLabel = statusLabel;

  ioThreadsTerminate = 0;
  ioThreadsFinished = 0;

  readerPID = multitaskerSpawn(&copyDiskIoThread, "i/o reader thread", 1,
			       (void *[]) { "reader" });
  writerPID = multitaskerSpawn(&copyDiskIoThread, "i/o writer thread", 1,
			       (void *[]) { "writer" });
  if ((readerPID < 0) || (writerPID < 0))
    {
      if (readerPID < 0)
	return (status = readerPID);
      else
	return (status = writerPID);
    }

  while (1)
    {
      if (ioThreadsFinished == 2)
	break;

      // Now we wait for the IO threads to terminate themselves
      if (!multitaskerProcessIsAlive(readerPID) ||
	  !multitaskerProcessIsAlive(writerPID))
	{
	  cancelled = 1;
	  break;
	}

      if (graphics)
	{
	  // Check for the cancel button
	  status = windowComponentEventGet(cancelButton, &event);
	  if ((status > 0) && (event.type == EVENT_MOUSE_LEFTUP))
	    {
	      cancelled = 1;
	      break;
	    }
	}

      multitaskerYield();
    }

  if (cancelled)
    {
      cancelDialog = windowNewBannerDialog(dialogWindow, "Cancel",
					   "Terminating processes...");
      ioThreadsTerminate = 1;
      multitaskerYield();
      if (multitaskerProcessIsAlive(readerPID))
	multitaskerBlock(readerPID);
      if (multitaskerProcessIsAlive(writerPID))
	multitaskerBlock(writerPID);
    }

  // Release copy buffer data
  memoryRelease(buffer.buffer[0].data);
  memoryRelease(buffer.buffer[1].data);

  if (cancelled)
    clearDiskLabel(destDisk);

  diskSync();

  if (graphics)
    {
      windowComponentSetData(progressBar, (void *) 100, 1);
      if (cancelDialog)
	windowDestroy(cancelDialog);
      windowDestroy(dialogWindow);
    }
  else
    {
      textSetColumn(2);
      printf("100%%\n");
    }

  status = selectDisk(destDisk);
  if (status < 0)
    return (status);

  if (cancelled)
    return (status = 0);

  // Now, if any partitions are ouside (or partially outside) the 
  // bounds of the destination disk, delete (or truncate) 
  for (count = (mainTable->numberEntries - 1); count >= 0; count --)
    {
      if (getPartitionEntry(count, &entry) < 0)
	break;
      
      // Starts past the end of the disk?
      if (entry.startCylinder >= destDisk->cylinders)
	{
	  if (entry.active)
	    {
	      entry.active = 0;
	      setPartitionEntry(count, &entry);
	    }
	  delete(entry.sliceId);
	}

      // Ends past the end of the disk?
      else if (entry.endCylinder >= destDisk->cylinders)
	{
	  entry.endCylinder = (destDisk->cylinders - 1);
	  entry.endHead = (destDisk->heads - 1);
	  entry.endSector = destDisk->sectorsPerCylinder;
	  entry.sizeLogical =   
	    ((((entry.endCylinder - entry.startCylinder) + 1) *
	      (destDisk->heads * destDisk->sectorsPerCylinder)) -
	     (entry.startHead * destDisk->sectorsPerCylinder));
	  setPartitionEntry(count, &entry);
	}
    }
	
  // Write out the partition table
  status = writePartitionTable(destDisk, mainTable);

  makeSliceList();
  display();
  
  return (status);
}


static void swapEntries(slice *firstSlice, slice *secondSlice)
{
  // Given 2 slices, swap them.  This is primarily for the
  // change partition order function, below

  slice tmpSlice;
  
  // Swap the partition data
  memcpy(&tmpSlice, secondSlice, sizeof(slice));
  memcpy(secondSlice, firstSlice, sizeof(slice));
  memcpy(firstSlice, &tmpSlice, sizeof(slice));

  // See whether we need to rename them
  if ((firstSlice->entryType != partition_extended) &&
      (secondSlice->entryType != partition_extended))
    {
      int tmpPartition;
      tmpPartition = secondSlice->partition;
      secondSlice->partition = firstSlice->partition;
      firstSlice->partition = tmpPartition;

#ifdef PARTLOGIC
      sprintf(firstSlice->name1, "%d", (firstSlice->partition + 1));
      sprintf(secondSlice->name1, "%d", (secondSlice->partition + 1));
#else
      sprintf(firstSlice->name1, "%s%c", selectedDisk->name,
	      ('a' + firstSlice->partition));
      sprintf(secondSlice->name1, "%s%c", selectedDisk->name,
	      ('a' + secondSlice->partition));
#endif
      makeSliceString(firstSlice);
      makeSliceString(secondSlice);
    }
}


static void changePartitionOrder(void)
{
  // This allows the user to change the partition table ordering

  // Graphical way of prompting for disk selection

  objectKey orderDialog = NULL;
  partitionTable tempTable;
  slice *orderSlices[DISK_MAX_PRIMARY_PARTITIONS];
  int numOrderSlices = 0;
  char *orderStrings[DISK_MAX_PRIMARY_PARTITIONS];
  char lineString[SLICESTRING_LENGTH + 2];
  componentParameters params;
  objectKey orderList = NULL;
  objectKey upButton = NULL;
  objectKey downButton = NULL;
  objectKey okButton = NULL;
  objectKey cancelButton = NULL;
  int selected = 0;
  windowEvent event;
  int count1, count2;

  // Make a copy of the main partition table
  memcpy(&tempTable, mainTable, sizeof(partitionTable));

  // Put pointers to the slices in our array
  for (count1 = 0; count1 < tempTable.numberEntries; count1 ++)
    {
      if ((tempTable.entries[count1].entryType == partition_primary) ||
	  (tempTable.entries[count1].entryType == partition_extended))
	{
	  orderSlices[numOrderSlices] = &tempTable.entries[count1];
	  makeSliceString(&tempTable.entries[count1]);
	  orderStrings[numOrderSlices++] = tempTable.entries[count1].string;
	}
    }
  
  if (graphics)
    {
      orderDialog = windowNewDialog(window, "Partition Order");

      bzero(&params, sizeof(componentParameters));
      params.gridWidth = 2;
      params.gridHeight = 2;
      params.padTop = 10;
      params.padLeft = 5;
      params.padRight = 5;
      params.orientationX = orient_center;
      params.orientationY = orient_middle;
      params.useDefaultForeground = 1;
      params.useDefaultBackground = 1;
      
      // Make a window list with all the disk choices
      fontGetDefault(&params.font);
      orderList = windowNewList(orderDialog, DISK_MAX_PRIMARY_PARTITIONS,
				1, 0, orderStrings, numOrderSlices, &params);

      // Make 'up' and 'down' buttons
      params.gridX = 2;
      params.gridHeight = 1;
      params.gridWidth = 1;
      params.font = NULL;
      upButton = windowNewButton(orderDialog, "/\\", NULL, &params);

      params.gridY = 1;
      params.padTop = 0;
      downButton = windowNewButton(orderDialog, "\\/", NULL, &params);

      // Make 'OK' and 'cancel' buttons
      params.gridX = 0;
      params.gridY = 2;
      params.padTop = 10;
      params.orientationX = orient_right;
      okButton = windowNewButton(orderDialog, "OK", NULL, &params);

      params.gridX = 1;
      params.orientationX = orient_left;
      cancelButton = windowNewButton(orderDialog, "Cancel", NULL, &params);

      // Make the window visible
      windowSetHasMinimizeButton(orderDialog, 0);
      windowSetResizable(orderDialog, 0);
      windowSetVisible(orderDialog, 1);

      while(1)
	{
	  selected = windowComponentGetSelected(orderList);

	  if ((windowComponentEventGet(upButton, &event) > 0) &&
	      (event.type == EVENT_MOUSE_LEFTUP) && (selected > 0))
	    {
	      // 'Up' button
	      swapEntries(orderSlices[selected], orderSlices[selected - 1]);
	      windowComponentSetSelected(orderList, (selected - 1));
	      windowComponentSetData(orderList, orderStrings, numOrderSlices);
	    }
	  
	  if ((windowComponentEventGet(downButton, &event) > 0) &&
	      (event.type == EVENT_MOUSE_LEFTUP) &&
	      (selected < (numOrderSlices - 1)))
	    {
	      // 'Down' button
	      swapEntries(orderSlices[selected], orderSlices[selected + 1]);
	      windowComponentSetSelected(orderList, (selected + 1));
	      windowComponentSetData(orderList, orderStrings, numOrderSlices);
	    }

	  // Check for our OK button
	  if ((windowComponentEventGet(okButton, &event) > 0) &&
	      (event.type == EVENT_MOUSE_LEFTUP))
	    {
	      // Copy our temporary table back into the main one
	      memcpy(mainTable, &tempTable, sizeof(partitionTable));
	      setPartitionNumbering(mainTable, 1);
	      makeSliceList();
	      changesPending += 1;
	      break;
	    }
	  
	  // Check for our Cancel button or window close
	  if (((windowComponentEventGet(orderDialog, &event) > 0) &&
	       (event.type == EVENT_WINDOW_CLOSE)) ||
	      ((windowComponentEventGet(cancelButton, &event) > 0) &&
	       (event.type == EVENT_MOUSE_LEFTUP)))
	    break;
	  
	  // Done
	  multitaskerYield();
	}

      windowDestroy(orderDialog);
    }
  else
    {
      int foregroundColor = textGetForeground();
      int backgroundColor = textGetBackground();
      textSetCursor(0);
      textInputSetEcho(0);

      memset(lineString, 196, (SLICESTRING_LENGTH + 1));
      lineString[SLICESTRING_LENGTH + 1] = '\0';

      while (1)
	{
	  printBanner();
	  printf("\nChange Partition Order\n\n %s\n", lineString);

	  // Print the partition strings
	  for (count1 = 0; count1 < numOrderSlices; count1 ++)
	    {
	      printf(" ");
	  
	      if (count1 == selected)
		{
		  // Reverse the colors
		  textSetForeground(backgroundColor);
		  textSetBackground(foregroundColor);
		}
	  
	      printf(" %s", orderStrings[count1]);
	      for (count2 = strlen(orderStrings[count1]);
		   count2 < SLICESTRING_LENGTH; count2 ++)
		printf(" ");

	      if (count1 == selected)
		{
		  // Restore the colors
		  textSetForeground(foregroundColor);
		  textSetBackground(backgroundColor);
		}

	      printf("\n");
	    }

	  printf(" %s\n\n  [Cursor up/down to select, '-' move up, '+' move "
		 "down,\n   Enter to accept, 'Q' to quit]", lineString);

	  switch (getchar())
	    {
	    case (unsigned char) 10:
	      // Copy our temporary table back into the main one
	      memcpy(mainTable, &tempTable, sizeof(partitionTable));
	      setPartitionNumbering(mainTable, 1);
	      makeSliceList();
	      changesPending += 1;
	      textSetCursor(1);
	      textInputSetEcho(1);
	      return;

	    case (unsigned char) 17:
	      // Cursor up.
	      if (selected > 0)
		selected -= 1;
	      continue;

	    case (unsigned char) 20:
	      // Cursor down.
	      if (selected < (numOrderSlices - 1))
		selected += 1;
	      continue;
	      
	    case '-':
	      if (selected > 0)
		{
		  // Move up
		  swapEntries(orderSlices[selected],
			      orderSlices[selected - 1]);
		  windowComponentSetSelected(orderList, (selected - 1));
		  windowComponentSetData(orderList, orderStrings,
					 numOrderSlices);
		  selected -= 1;
		}
	      continue;

	    case '+':
	      if (selected < (numOrderSlices - 1))
		{
		  // Move down
		  swapEntries(orderSlices[selected],
			      orderSlices[selected + 1]);
		  windowComponentSetSelected(orderList, (selected + 1));
		  windowComponentSetData(orderList, orderStrings,
					 numOrderSlices);
		  selected += 1;
		}
	      continue;

	    case 'q':
	    case 'Q':
	      textSetCursor(1);
	      textInputSetEcho(1);
	      return;

	    default:
	      continue;
	    }
	}
    }
}


static void restoreBackup(void)
{
  // Restore the backed-up partition table from a file

  int status = 0;
  file backupFile;
  char fileName[MAX_PATH_NAME_LENGTH];
  unsigned char *buffer = NULL;

  if (!yesOrNo("Restore old partition table from backup?"))
    return;

  // Clear stack data
  bzero(&backupFile, sizeof(file));

  // Construct the file name
  sprintf(fileName, BACKUP_MBR, selectedDisk->name);

  // Read a backup copy of the partition tables
  status = fileOpen(fileName, OPENMODE_READ, &backupFile);
  if (status < 0)
    {
      error("Error opening backup partition table file %s");
      return;
    }

  buffer = malloc(backupFile.blocks * backupFile.blockSize);
  if (buffer == NULL)
    {
      error("Error allocating backup partition table file memory");
      return;
    }

  status = fileRead(&backupFile, 0, backupFile.blocks, buffer);
  if (status < 0)
    {
      error("Error reading backup partition table file");
      free(buffer);
      return;
    }
  fileClose(&backupFile);

  // Scan the backup buffer into the partition table
  scanPartitionTable(selectedDisk, mainTable, buffer);

  free(buffer);

  // Regenerate the slice list
  makeSliceList();

  // Don't write it.  The user has to do that explicitly.
  changesPending += 1;

  return;
}


static void eventHandler(objectKey key, windowEvent *event)
{
  int count;
  int selected = -1;

  // Check for the window being closed by a GUI event.
  if (((key == window) && (event->type == EVENT_WINDOW_CLOSE)) ||
      ((key == menuQuit) && (event->type == EVENT_MOUSE_LEFTUP)))
    {
      quit(0);
      return;
    }

  // Check for changes to our disk list
  else if ((key == diskList) && ((event->type == EVENT_MOUSE_LEFTDOWN) ||
				 (event->type == EVENT_KEY_DOWN)))
    {
      int newSelected = windowComponentGetSelected(diskList);
      if ((newSelected >= 0) && (newSelected != selectedDiskNumber))
	if (selectDisk(&diskInfo[newSelected]) < 0)
	  windowComponentSetSelected(diskList, selectedDiskNumber);
    }

  // Check for clicks on our canvas diagram
  else if ((key == canvas) && (event->type == EVENT_MOUSE_LEFTDOWN))
    {
      for (count = 0; count < numberSlices; count ++)
	if ((event->xPosition > slices[count].pixelX) &&
	    (event->xPosition < (slices[count].pixelX +
				 slices[count].pixelWidth)))
	  {
	    selected = count;
	    break;
	  }
      if (selected >= 0)
	selectedSlice = selected;
    }
  
  // Check for changes to our slice list
  else if ((key == sliceList) && ((event->type == EVENT_MOUSE_LEFTDOWN) ||
				  (event->type == EVENT_KEY_DOWN)))
    {
      selected = windowComponentGetSelected(sliceList);
      if (selected >= 0)
	selectedSlice = selected;
    }

  else if (((key == writeButton) || (key == menuWrite)) &&
	   (event->type == EVENT_MOUSE_LEFTUP))
    writeChanges(1);

  else if (((key == undoButton) || (key == menuUndo)) && 
	   (event->type == EVENT_MOUSE_LEFTUP))
    undo();

  else if ((key == menuRestoreBackup) && (event->type == EVENT_MOUSE_LEFTUP))
    restoreBackup();

  else if (((key == copyDiskButton) || (key == menuCopyDisk)) &&
	   (event->type == EVENT_MOUSE_LEFTUP))
    {
      if (copyDisk() < 0)
	error("Disk copy failed.");
    }

  else if ((key == menuPartOrder) && (event->type == EVENT_MOUSE_LEFTUP))
    {
      changePartitionOrder();
    }

  else if (((key == setActiveButton) || (key == menuSetActive)) &&
	   (event->type == EVENT_MOUSE_LEFTUP))
    {
      if (slices[selectedSlice].typeId)
	setActive(slices[selectedSlice].sliceId);
    }

  else if (((key == deleteButton) || (key == menuDelete)) &&
	   (event->type == EVENT_MOUSE_LEFTUP))
    {
      if (slices[selectedSlice].typeId)
	delete(slices[selectedSlice].sliceId);
    }

  else if (((key == formatButton) || (key == menuFormat)) &&
	   (event->type == EVENT_MOUSE_LEFTUP))
    format(&slices[selectedSlice]);

  else if (((key == hideButton) || (key == menuHide)) &&
	   (event->type == EVENT_MOUSE_LEFTUP))
    {
      if (slices[selectedSlice].typeId)
	hide(slices[selectedSlice].sliceId);
    }

  else if (((key == infoButton) || (key == menuInfo)) &&
	   (event->type == EVENT_MOUSE_LEFTUP))
    info(selectedSlice);

  else if ((key == menuListTypes) && (event->type == EVENT_MOUSE_LEFTUP))
    listTypes();

  else if (((key == moveButton) || (key == menuMove)) &&
	   (event->type == EVENT_MOUSE_LEFTUP))
    {
      if (slices[selectedSlice].typeId)
	move(slices[selectedSlice].sliceId);
    }

  else if (((key == newButton) || (key == menuNew)) &&
	   (event->type == EVENT_MOUSE_LEFTUP))
    create(selectedSlice);

  else if (((key == deleteAllButton) || (key == menuDeleteAll)) &&
	   (event->type == EVENT_MOUSE_LEFTUP))
    deleteAll();

  else if (((key == setTypeButton) || (key == menuSetType)) &&
	   (event->type == EVENT_MOUSE_LEFTUP))
    {
      if (slices[selectedSlice].typeId)
	setType(slices[selectedSlice].sliceId);
    }

  else
    return;

  display();
}


static void constructWindow(void)
{
  // If we are in graphics mode, make a window rather than operating on the
  // command line.

  int status = 0;
  componentParameters params;
  static image iconImage;
  objectKey container = NULL;
  objectKey imageComponent = NULL;
  objectKey textLabel = NULL;
  int count;

  // Create a new window, with small, arbitrary size and location
  window = windowNew(processId, programName);
  if (window == NULL)
    return;

  bzero(&params, sizeof(componentParameters));
  params.gridWidth = 1;
  params.gridHeight = 1;
  params.padTop = 5;
  params.padLeft = 5;
  params.padRight = 5;
  params.orientationX = orient_left;
  params.orientationY = orient_middle;
  params.useDefaultForeground = 1;
  params.useDefaultBackground = 1;

  // Create the top 'file' menu
  objectKey menuBar = windowNewMenuBar(window, &params);
  objectKey menu1 = windowNewMenu(menuBar, "File", &params);
  menuWrite = windowNewMenuItem(menu1, "Write", &params);
  windowRegisterEventHandler(menuWrite, &eventHandler);
  menuUndo = windowNewMenuItem(menu1, "Undo", &params);
  windowRegisterEventHandler(menuUndo, &eventHandler);
  menuRestoreBackup = windowNewMenuItem(menu1, "Restore backup", &params);
  windowRegisterEventHandler(menuRestoreBackup, &eventHandler);
  menuQuit = windowNewMenuItem(menu1, "Quit", &params);
  windowRegisterEventHandler(menuQuit, &eventHandler);

  // Create the top 'disk' menu
  objectKey menu2 = windowNewMenu(menuBar, "Disk", &params);
  menuCopyDisk = windowNewMenuItem(menu2, "Copy disk", &params);
  windowRegisterEventHandler(menuCopyDisk, &eventHandler);
  menuPartOrder = windowNewMenuItem(menu2, "Partition order", &params);
  windowRegisterEventHandler(menuPartOrder, &eventHandler);

  // Create the top 'partition' menu
  objectKey menu3 = windowNewMenu(menuBar, "Partition", &params);
  menuSetActive = windowNewMenuItem(menu3, "Set active", &params);
  windowRegisterEventHandler(menuSetActive, &eventHandler);
  menuDelete = windowNewMenuItem(menu3, "Delete", &params);
  windowRegisterEventHandler(menuDelete, &eventHandler);
  menuFormat = windowNewMenuItem(menu3, "Format", &params);
  windowRegisterEventHandler(menuFormat, &eventHandler);
  menuHide = windowNewMenuItem(menu3, "Hide/Unhide", &params);
  windowRegisterEventHandler(menuHide, &eventHandler);
  menuInfo = windowNewMenuItem(menu3, "Info", &params);
  windowRegisterEventHandler(menuInfo, &eventHandler);
  menuListTypes = windowNewMenuItem(menu3, "List types", &params);
  windowRegisterEventHandler(menuListTypes, &eventHandler);
  menuMove = windowNewMenuItem(menu3, "Move", &params);
  windowRegisterEventHandler(menuMove, &eventHandler);
  menuNew = windowNewMenuItem(menu3, "New", &params);
  windowRegisterEventHandler(menuNew, &eventHandler);
  menuDeleteAll = windowNewMenuItem(menu3, "Delete all", &params);
  windowRegisterEventHandler(menuDeleteAll, &eventHandler);
  menuSetType = windowNewMenuItem(menu3, "Set type", &params);
  windowRegisterEventHandler(menuSetType, &eventHandler);

  // Create a container for the disk icon image and the title label
  params.gridY = 1;
  container = windowNewContainer(window, "titleContainer", &params);
  if (container != NULL)
    {
      if (iconImage.data == NULL)
	// Try to load an icon image to go at the top of the window
	status = imageLoad("/system/icons/diskicon.bmp", 0, 0, &iconImage);
      if (status == 0)
	{
	  // Create an image component from it, and add it to the container
	  iconImage.translucentColor.red = 0;
	  iconImage.translucentColor.green = 255;
	  iconImage.translucentColor.blue = 0;
	  imageComponent = windowNewImage(container, &iconImage,
					  draw_translucent, &params);
	}

      // Put a title text label in the container
      params.gridX = 1;
      textLabel = windowNewTextLabel(container, programName, &params);
    }

  // Make a list for the disks
  params.gridX = 0;
  params.gridY = 2;
  diskList = windowNewList(window, numberDisks, 1, 0, diskStrings,
			   numberDisks, &params);
  windowRegisterEventHandler(diskList, &eventHandler);

  params.gridY = 4;
  fontGetDefault(&params.font);
  textLabel = windowNewTextLabel(window, sliceListHeader, &params);

  // Make a list for the partitions
  params.gridY = 5;
  params.padTop = 5;
  char tmpChar[SLICESTRING_LENGTH + 1];
  for (count = 0; count <= SLICESTRING_LENGTH; count ++)
    tmpChar[count] = ' ';
  tmpChar[SLICESTRING_LENGTH] = '\0';
  sliceList = windowNewList(window, 6, 1, 0, (char *[]){ tmpChar }, 1,
			    &params);
  windowRegisterEventHandler(sliceList, &eventHandler);
  canvasWidth = windowComponentGetWidth(sliceList);

  // Get a canvas for drawing the visual representation
  params.gridY = 3;
  params.padTop = 10;
  params.hasBorder = 1;
  canvas = windowNewCanvas(window, canvasWidth, canvasHeight, &params);
  windowRegisterEventHandler(canvas, &eventHandler);

  // A container for the buttons
  params.gridX = 0;
  params.gridY = 6;
  params.padTop = 5;
  params.padBottom = 5;
  params.orientationX = orient_center;
  params.hasBorder = 0;
  container = windowNewContainer(window, "buttonContainer", &params);
  if (container != NULL)
    {
      params.gridY = 0;
      params.orientationX = orient_left;
      params.padBottom = 0;
      params.font = NULL;
      newButton = windowNewButton(container, "New", NULL, &params);
      windowRegisterEventHandler(newButton, &eventHandler);

      params.gridX = 1;
      setActiveButton =
	windowNewButton(container, "Set active", NULL, &params);
      windowRegisterEventHandler(setActiveButton, &eventHandler);

      params.gridX = 2;
      moveButton = windowNewButton(container, "Move", NULL, &params);
      windowRegisterEventHandler(moveButton, &eventHandler);

      params.gridX = 3;
      copyDiskButton = windowNewButton(container, "Copy disk", NULL, &params);
      windowRegisterEventHandler(copyDiskButton, &eventHandler);

      params.gridX = 4;
      formatButton = windowNewButton(container, "Format", NULL, &params);
      windowRegisterEventHandler(formatButton, &eventHandler);

      params.gridX = 5;
      deleteAllButton =
	windowNewButton(container, "Delete all", NULL, &params);
      windowRegisterEventHandler(deleteAllButton, &eventHandler);

      params.gridX = 0;
      params.gridY = 1;
      params.padTop = 0;
      deleteButton = windowNewButton(container, "Delete", NULL, &params);
      windowRegisterEventHandler(deleteButton, &eventHandler);

      params.gridX = 1;
      hideButton = windowNewButton(container, "Hide/unhide", NULL, &params);
      windowRegisterEventHandler(hideButton, &eventHandler);

      params.gridX = 2;
      infoButton = windowNewButton(container, "Info", NULL, &params);
      windowRegisterEventHandler(infoButton, &eventHandler);

      params.gridX = 3;
      setTypeButton = windowNewButton(container, "Set type", NULL, &params);
      windowRegisterEventHandler(setTypeButton, &eventHandler);

      params.gridX = 4;
      undoButton = windowNewButton(container, "Undo", NULL, &params);
      windowRegisterEventHandler(undoButton, &eventHandler);

      params.gridX = 5;
      writeButton = windowNewButton(container, "Write changes", NULL, &params);
      windowRegisterEventHandler(writeButton, &eventHandler);
    }

  // Register an event handler to catch window close events
  windowRegisterEventHandler(window, &eventHandler);

  // Go
  windowSetVisible(window, 1);

  return;
}


static int textMenu(void)
{
  int status = 0;
  char optionString[80];
  int isPartition = 0;
  int isHide = 0;
  int isMove = 0;

  // This is the main menu bit
  while (1)
    {
      // Print out the partitions
      display();

      isPartition = 0;
      isHide = 0;
      isMove = 0;

      if (slices[selectedSlice].typeId)
	{
	  isPartition = 1;

	  if (PARTITION_TYPEID_IS_HIDEABLE(slices[selectedSlice].typeId) ||
	      PARTITION_TYPEID_IS_HIDDEN(slices[selectedSlice].typeId))
	    isHide = 1;

	  if (!ISLOGICAL(&slices[selectedSlice]))
	    isMove = 1;
	}

      // Print out the menu choices
      printf("\n%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s",
	     (isPartition? "[A] Set active\n" : ""),
	     (numberDataPartitions? "[B] Partition order\n" : ""),
	     "[C] Copy disk\n",
	     (isPartition? "[D] Delete\n" : ""),
	     (isPartition? "[F] Format\n" : ""),
	     (isHide? "[H] Hide/Unhide\n" : ""),
	     "[I] Info\n",
	     "[L] List types\n",
	     (isMove? "[M] Move\n" : ""),
	     (!isPartition? "[N] New\n" : ""),
	     (numberPartitions? "[O] Delete all\n" : ""),
	     "[Q] Quit\n",
	     (backupAvailable? "[R] Restore backup\n" : ""),
	     "[S] Select disk\n",
	     (isPartition? "[T] Set type\n" : ""),
	     (changesPending? "[U] Undo\n" : ""),
	     (changesPending? "[W] Write changes\n" : ""));

      if (changesPending)
	printf("  -== %d changes pending ==-\n", changesPending);
      printf("-> ");

      // Construct the string of allowable options, corresponding to what is
      // shown above.
      sprintf(optionString, "%s%sCc%s%s%sIiLl%s%s%sQq%sSs%s%s%s",
	      (isPartition? "Aa" : ""),
	      (numberDataPartitions? "Bb" : ""),
	      (isPartition? "Dd" : ""),
	      (isPartition? "Ff" : ""),
	      (isPartition? "Hh" : ""),
	      (isMove? "Mm" : ""),
	      (!isPartition? "Nn" : ""),
	      (numberPartitions? "Oo" : ""),
	      (backupAvailable? "Rr" : ""),
	      (isPartition? "Tt" : ""),
	      (changesPending? "Uu" : ""),
	      (changesPending? "Ww" : ""));

      switch (readKey(optionString, 1))
	{
	case (unsigned char) 17:
	  // Cursor up.
	  if (selectedSlice > 0)
	    selectedSlice -= 1;
	  continue;

	case (unsigned char) 20:
	  // Cursor down.
	  if (selectedSlice < (numberSlices - 1))
	    selectedSlice += 1;
	  continue;

	case 'a':
	case 'A':
	  setActive(slices[selectedSlice].sliceId);
	  continue;

	case 'b':
	case 'B':
	  changePartitionOrder();
	  continue;

	case 'c':
	case 'C':
	  status = copyDisk();
	  if (status < 0)
	    error("Disk copy failed.");
	  continue;

	case 'd':
	case 'D':
	  delete(slices[selectedSlice].sliceId);
	  continue;

	case 'f':
	case 'F':
	  format(&slices[selectedSlice]);
	  continue;

	case 'h':
	case 'H':
	  hide(slices[selectedSlice].sliceId);
	  continue;

	case 'i':
	case 'I':
	  info(selectedSlice);
	  continue;

	case 'l':
	case 'L':
	  listTypes();
	  continue;

	case 'm':
	case 'M':
	  move(slices[selectedSlice].sliceId);
	  continue;

	case 'n':
	case 'N':
	  create(selectedSlice);
	  continue;

	case 'o':
	case 'O':
	  deleteAll();
	  continue;

	case 'q':
	case 'Q':
	  if (quit(0))
	    return (status = 0);
	  continue;
	      
	case 'r':
	case 'R':
	  restoreBackup();
	  continue;
	      
	case 's':
	case 'S':
	  status = queryDisk();
	  if (status < 0)
	    {
	      error("No disk selected.  Quitting.");
	      quit(1);
	      return (status = ERR_CANCELLED);
	    }
	  continue;

	case 't':
	case 'T':
	  setType(slices[selectedSlice].sliceId);
	  continue;

	case 'u':
	case 'U':
	  undo();
	  continue;
	  
	case 'w':
	case 'W':
	  writeChanges(1);
	  continue;
	  
	default:
	  continue;
	}
    }
}


static void freeMemory(void)
{
  // Free any malloc'ed global memory

  if (diskInfo)
    free(diskInfo);

  if (diskStringData)
    free(diskStringData);

  if (mainTable)
    {
      // This will free anything allocated for extended partition tables, etc.
      //deleteAll();

      free(mainTable);
    }

  if (slices)
    free(slices);
}


static void makeSliceListHeader(void)
{
  // The header that goes above the slice list.  Name string in graphics
  // and text modes

  int count;

  for (count = 0; count < SLICESTRING_LENGTH; count ++)
    sliceListHeader[count] = ' ';
  count = 0;
#ifdef PARTLOGIC
  strncpy(sliceListHeader, "#", 1);
#else
  strncpy(sliceListHeader, "Disk", 4);
#endif
  count += SLICESTRING_DISKFIELD_WIDTH;
  strncpy((sliceListHeader + count), "Partition", 9);
  count += SLICESTRING_LABELFIELD_WIDTH;
  strncpy((sliceListHeader + count), "FS", 2);
  count += SLICESTRING_FSTYPEFIELD_WIDTH;
  strncpy((sliceListHeader + count), "Cylinders", 9);
  count += SLICESTRING_CYLSFIELD_WIDTH;
  strncpy((sliceListHeader + count), "Size(Mb)", 8);
  count += SLICESTRING_SIZEFIELD_WIDTH;
  strncpy((sliceListHeader + count), "Attributes", 10);
  return;
}


int main(int argc, char *argv[])
{
  int status = 0;
  char opt;
  int clearLabel = 0;
  char tmpChar[80];
  int count;

  // Are graphics enabled?
  graphics = graphicsAreEnabled();

  // Run as Partition Logic?
  #ifdef PARTLOGIC
  programName = PARTITION_LOGIC;
  #else
  programName = DISK_MANAGER;
  #endif

  while (strchr("To", (opt = getopt(argc, argv, "To"))))
    {
      // Force text mode?
      if (opt == 'T')
	graphics = 0;

      // Clear the partition table?
      if (opt == 'o')
	clearLabel = 1;
    }

  processId = multitaskerGetCurrentProcessId();

  // Check privilege level
  if (multitaskerGetProcessPrivilege(processId) != 0)
    {
      if (graphics)
	windowNewErrorDialog(NULL, "Permission Denied", PERM);
      printf("\n%s\n(Try logging in as user \"admin\")\n\n", PERM);
      return (errno = ERR_PERMISSION);
    }

  // Get memory for various things

  diskInfo = malloc(DISK_MAXDEVICES * sizeof(disk));
  diskStringData = malloc(DISK_MAXDEVICES * MAX_DESCSTRING_LENGTH);
  mainTable = malloc(sizeof(partitionTable));
  slices = malloc(MAX_SLICES * sizeof(slice));
  if ((diskInfo == NULL) || (diskStringData == NULL) || (mainTable == NULL) ||
      (slices == NULL))
    {
      freeMemory();
      return (status = errno = ERR_MEMORY);
    }
  for (count = 0; count < DISK_MAXDEVICES; count ++)
    diskStrings[count] = (diskStringData + (count * MAX_DESCSTRING_LENGTH));

  // Find out whether our temp or backup directories are on a read-only
  // filesystem
  bzero(diskInfo, sizeof(disk));
  if (!fileGetDisk(TEMP_DIR, diskInfo) && !diskInfo->readOnly)
    {
      if (!fileGetDisk(BOOT_DIR, diskInfo) && !diskInfo->readOnly)
	readOnly = 0;
    }
  bzero(diskInfo, sizeof(disk));

  // Gather the disk info
  status = scanDisks();
  if (status < 0)
    {
      if (status == ERR_NOSUCHENTRY)
	error("No hard disks registered");
      else
	error("Problem getting hard disk info");
      freeMemory();
      return (errno = status);
    }

  if (!numberDisks)
    {
      // There are no fixed disks
      error("No fixed disks to manage.  Quitting.");
      freeMemory();
      return (errno = status);
    }

  makeSliceListHeader();

  if (graphics)
    {
      constructWindow();
    }
  else
    {
      textScreenSave();
      printBanner();
    }

  // The user can specify the disk name as an argument.  Try to see
  // whether they did so.
  if (argc > 1)
    {
      for (count = 0; count < numberDisks; count ++)
	if (!strcmp(diskInfo[count].name, argv[argc - 1]))
	  {
	    if (clearLabel)
	      {
		// If the disk is specified, we can clear the label
		sprintf(tmpChar, "Clear the partition table of disk %s?",
			diskInfo[count].name);
		if (yesOrNo(tmpChar))
		  clearDiskLabel(&diskInfo[count]);
	      }
	    
	    selectDisk(&diskInfo[count]);
	    break;
	  }
    }

  if (selectedDisk == NULL)
    {
      // If we're in text mode, the user must first select a disk
      if (!graphics && (numberDisks > 1))
	{
	  status = queryDisk();
	  if (status < 0)
	    {
	      printf("\n\nNo disk selected.  Quitting.\n\n");
	      freeMemory();
	      textScreenRestore();
	      return (errno = status);
	    }
	}
      else
	{
	  status = selectDisk(&diskInfo[0]);
	  if (status < 0)
	    {
	      freeMemory();
	      return (errno = status);
	    }
	}
    }

  if (graphics)
    {
      display();
      windowGuiRun();
      status = 0;
    }
  else
    {
      status = textMenu();
      textScreenRestore();
    }

  freeMemory();
  return (status);
}
