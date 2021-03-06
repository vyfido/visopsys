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
//  fdisk.c
//

// This is a program for modifying partition tables and doing other disk
// management tasks.

/* This is the text that appears when a user requests help about this program
<help>

 -- fdisk --

Also known as the "Disk Manager", fdisk is a hard disk partitioning tool.
It can create, delete, format, and move partitions and modify their
attributes.  It can copy entire hard disks from one to another.

Usage:
  fdisk [-T] [-o] [disk_name]

The fdisk program is interactive, and can be used in either text or graphics
mode.  It provides the same functionality in both modes; text mode operation
is menu-driven.

The disk can be automatically selected by specifying its name (as listed by
the 'disks' command) as the last argument.

Options:
-T              : Force text mode operation
-o <disk_name>  : Clear the partition table of the specified disk

Development of this program is ongoing and will be a major focus of
improvements in future releases,  aiming towards providing much of the same
functionality of PartitionMagic and similar utilities.

</help>
*/

#include "fdisk.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/api.h>
#include <sys/vsh.h>
#include <sys/fat.h>
#include <sys/ntfs.h>

#define PERM             "You must be a privileged user to use this " \
                         "command.\n(Try logging in as user \"admin\")"
#define PARTTYPES        "Supported Partition Types"
#define STARTCYL_MESSAGE "Enter starting cylinder (%u-%u)"
#define ENDCYL_MESSAGE   "Enter ending cylinder (%u-%u)\n" \
                         "or size in megabytes with 'm' (1m-%um),\n" \
                         "or size in cylinders with 'c' (1c-%uc)"

static const char *programName = NULL;
static int processId = 0;
static int readOnly = 1;
static char sliceListHeader[SLICESTRING_LENGTH + 1];
static int numberDisks = 0;
static disk *diskInfo = NULL;
static listItemParameters *diskListParams = NULL;
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
static int checkTableAsk = 1;
static textScreen screen;
static slice clipboardSlice;
static disk *clipboardDisk = NULL;
static int clipboardSliceValid = 0;

// GUI stuff
static int graphics = 0;
static objectKey window = NULL;
static objectKey fileMenu = NULL;
static objectKey diskMenu = NULL;
static objectKey partMenu = NULL;
static objectKey diskList = NULL;
static objectKey canvas = NULL;
static objectKey sliceList = NULL;
static objectKey writeButton = NULL;
static objectKey undoButton = NULL;
static objectKey defragButton = NULL;
static objectKey setActiveButton = NULL;
static objectKey deleteButton = NULL;
static objectKey deleteAllButton = NULL;
static objectKey formatButton = NULL;
static objectKey hideButton = NULL;
static objectKey infoButton = NULL;
static objectKey moveButton = NULL;
static objectKey newButton = NULL;
static objectKey resizeButton = NULL;
static unsigned canvasWidth = 600;
static unsigned canvasHeight = 60;

// Menus

#define FILEMENU_WRITE         0
#define FILEMENU_UNDO          1
#define FILEMENU_RESTOREBACKUP 2
#define FILEMENU_QUIT          3
windowMenuContents fileMenuContents = {
  4,
  {
    { "Write", NULL },
    { "Undo", NULL },
    { "Restore backup", NULL },
    { "Quit", NULL }
  }
};

#define DISKMENU_COPYDISK  0
#define DISKMENU_PARTORDER 1
#define DISKMENU_SIMPLEMBR 2
#define DISKMENU_BOOTMENU  3
windowMenuContents diskMenuContents = {
  4,
  {
    { "Copy disk", NULL },
    { "Partition order", NULL },
    { "Write basic MBR", NULL },
    { "MBR boot menu", NULL }
  }
};
  
#define PARTMENU_COPY      0
#define PARTMENU_PASTE     1
#define PARTMENU_SETACTIVE 2
#define PARTMENU_DELETE    3
#define PARTMENU_FORMAT    4
#define PARTMENU_DEFRAG    5
#define PARTMENU_RESIZE    6
#define PARTMENU_HIDE      7
#define PARTMENU_INFO      8
#define PARTMENU_LISTTYPES 9
#define PARTMENU_MOVE      10
#define PARTMENU_CREATE    11
#define PARTMENU_DELETEALL 12
#define PARTMENU_SETTYPE   13
windowMenuContents partMenuContents = {
  14,
  {
    { "Copy", NULL },
    { "Paste", NULL },
    { "Set active", NULL },
    { "Delete", NULL },
    { "Format", NULL },
    { "Defragment", NULL },
    { "Resize", NULL },
    { "Hide/Unhide", NULL },
    { "Info", NULL },
    { "List types", NULL },
    { "Move", NULL },
    { "Create", NULL },
    { "Delete all", NULL },
    { "Set type", NULL }
  }
};

static int yesOrNo(char *question)
{
  char character;

  if (graphics)
    return (windowNewQueryDialog(window, "Confirmation", question));

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

  if (graphics)
    {
      windowGuiStop();
      windowDestroy(window);
    }
  else
    {
      textScreenRestore(&screen);
      printf("\nQuitting.\n");
    }

  if (tmpBackupName != NULL)
    {
      fileDelete(tmpBackupName);
      tmpBackupName = NULL;
    }

  errno = 0;
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


static void error(const char *, ...) __attribute__((format(printf, 1, 2)));
static void error(const char *format, ...)
{
  // Generic error message code for either text or graphics modes
  
  va_list list;
  char *output = NULL;

  output = malloc(MAXSTRINGLENGTH);
  if (output == NULL)
    return;
  
  va_start(list, format);
  vsnprintf(output, MAXSTRINGLENGTH, format, list);
  va_end(list);

  if (graphics)
    windowNewErrorDialog(window, "Error", output);
  else
    {
      printf("\n\n%s\n", output);
      pause();
    }

  free(output);
}


static void warning(const char *, ...) __attribute__((format(printf, 1, 2)));
static void warning(const char *format, ...)
{
  // Generic error message code for either text or graphics modes

  va_list list;
  char *output = NULL;
  
  output = malloc(MAXSTRINGLENGTH);
  if (output == NULL)
    return;
  
  va_start(list, format);
  vsnprintf(output, MAXSTRINGLENGTH, format, list);
  va_end(list);

  if (graphics)
    windowNewErrorDialog(window, "Warning", output);
  else
    {
      printf("\n\nWARNING: %s\n", output);
      pause();
    }

  free(output);
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
      sprintf(table->entries[count].diskName, "%s%c", selectedDisk->name,
	      ('a' + table->entries[count].partition));
#ifdef PARTLOGIC
      sprintf(table->entries[count].sliceName, "%d",
	      (table->entries[count].partition + 1));
#else
      strcpy(table->entries[count].sliceName, table->entries[count].diskName);
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

  memset(slc->string, ' ', MAX_DESCSTRING_LENGTH);
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
      strcpy(slc->string, slc->sliceName);
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
      else
	sprintf((slc->string + position), "unknown");

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


static int getFsInfo(slice *entry)
{
  int status = 0;
  disk tmpDisk;

  status = diskGet(entry->diskName, &tmpDisk);
  if (status < 0)
    return (status);

  entry->opFlags = tmpDisk.opFlags;

  if (strcmp(tmpDisk.fsType, "unknown"))
    strncpy(entry->fsType, tmpDisk.fsType, FSTYPE_MAX_NAMELENGTH);

  return (status = 0);
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
      getFsInfo(&slices[numberSlices]);
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

  if (PARTITION_TYPEID_IS_EXTD(entry->typeId))
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


static int checkTable(const disk *theDisk, partitionTable *table, int fix)
{
  // Does a series of correctness checks on the table that's currently in
  // memory, and outputs warnings for any problems it finds

  slice *entry = NULL;
  int errors = 0;
  unsigned expectCylinder = 0;
  unsigned expectHead = 0;
  unsigned expectSector = 0;
  char *output = NULL;
  int count;

  output = malloc(MAXSTRINGLENGTH);
  if (output == NULL)
    return (ERR_MEMORY);

  sprintf(output, "%s table:\n", (table->extended? "Extended" : "Main"));

  // Check that extended tables don't have too many entries
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

      if (checkTableAsk)
	{
	  if (yesOrNo(output))
	    return (checkTable(theDisk, table, 1));
	  else
	    // Don't ask about fixing stuff more than once.
	    checkTableAsk = 0;
	}

      free(output);
      return (ERR_INVALID);
    }
  else
    {
      free(output);
      return (0);
    }
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
      sprintf(entry->diskName, "%s%c", theDisk->name,
	      ('a' + entry->partition));
#ifdef PARTLOGIC
      sprintf(entry->sliceName, "%d", (entry->partition + 1));
#else
      strcpy(entry->sliceName, entry->diskName);
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

  status = fileStreamWrite(backupFile, 512, (char *) table->sectorData);
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
  char *fileName = NULL;
  static char *tmpBackupFileName = NULL;
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
  fileName = malloc(MAX_PATH_NAME_LENGTH);
  if (fileName == NULL)
    return (status = ERR_MEMORY);
  sprintf(fileName, BACKUP_MBR, theDisk->name);
  if (!fileFind(fileName, &backupFile))
    backupAvailable = 1;
  else
    backupAvailable = 0;
  free(fileName);

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

      if (tmpBackupFileName == NULL)
	{
	  tmpBackupFileName = malloc(MAX_PATH_NAME_LENGTH);
	  if (tmpBackupFileName == NULL)
	    return (status = ERR_MEMORY);
	}
      snprintf(tmpBackupFileName, MAX_PATH_NAME_LENGTH, TEMP_DIR"/%s",
	       backupFile.name);
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


static int writePartitionTable(const disk *theDisk, partitionTable *table,
			       int confirm)
{
  // Write the partition table to the physical disk

  int status = 0;
  char *fileName = NULL;
  fileStream backupFile;
  slice entry;
  int count;

  // Clear stack data
  bzero(&backupFile, sizeof(fileStream));

  if (table == mainTable)
    {
      if (readOnly && confirm &&
	  !yesOrNo("Can't create a partition table backup in\n"
		   "read-only mode.  Proceed anyway?"))
	return (status = 0);

      // Make the backup file permanent
      if (tmpBackupName != NULL)
	{
	  fileName = malloc(MAX_PATH_NAME_LENGTH);
	  if (fileName == NULL)
	    return (status = ERR_MEMORY);

	  // Construct the backup file name
	  snprintf(fileName, MAX_PATH_NAME_LENGTH, BACKUP_MBR, theDisk->name);
  
	  // Copy the temporary backup file to the backup
	  fileMove(tmpBackupName, fileName);

	  free(fileName);

	  tmpBackupName = NULL;
	  
	  // We now have a proper backup
	  backupAvailable = 1;
	}
    }

  // Do a check on the table
  status = checkTable(theDisk, table, 0);
  if ((status < 0) && !yesOrNo("Partition table consistency check failed.\n"
			       "Write anyway?"))
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
	  status = writePartitionTable(theDisk, entry.extendedTable, confirm);
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

  return (status = 0);
}


static int scanDisks(void)
{
  int status = 0;
  int tmpNumberDisks = 0;
  disk *tmpDiskInfo = NULL;
  int count;

  // Call the kernel to give us the number of available disks
  tmpNumberDisks = diskGetPhysicalCount();
  if (tmpNumberDisks <= 0)
    return (status = ERR_NOSUCHENTRY);

  tmpDiskInfo = malloc(DISK_MAXDEVICES * sizeof(disk));
  if (tmpDiskInfo == NULL)
    return (status = ERR_MEMORY);

  // Read disk info into our temporary structure
  status = diskGetAllPhysical(tmpDiskInfo, (DISK_MAXDEVICES * sizeof(disk)));
  if (status < 0)
    {
      // Eek.  Problem getting disk info.
      free(tmpDiskInfo);
      return (status);
    }

  diskListParams = malloc(DISK_MAXDEVICES * sizeof(listItemParameters));
  if (diskListParams == NULL)
    {
      free(tmpDiskInfo);
      return (status = ERR_MEMORY);
    }

  // Loop through these disks, figuring out which ones are hard disks
  // and putting them into the regular array
  for (count = 0; count < tmpNumberDisks; count ++)
    if (tmpDiskInfo[count].flags & DISKFLAG_HARDDISK)
      {
	memcpy(&diskInfo[numberDisks], &tmpDiskInfo[count], sizeof(disk));

	snprintf(diskListParams[numberDisks].text, WINDOW_MAX_LABEL_LENGTH,
		 "Disk %d: [%s] %u Mb, %u cyls, %u heads, %u secs/cyl, "
		 "%u bytes/sec", diskInfo[numberDisks].deviceNumber,
		 diskInfo[numberDisks].name,
		 cylsToMb(&diskInfo[numberDisks],
			  diskInfo[numberDisks].cylinders),
		 diskInfo[numberDisks].cylinders, diskInfo[numberDisks].heads, 
		 diskInfo[numberDisks].sectorsPerCylinder,
		 diskInfo[numberDisks].sectorSize);

	numberDisks += 1;
      }

  free(tmpDiskInfo);

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
    printf("  %s\n", diskListParams[count].text);
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
  char *diskStrings[DISK_MAXDEVICES];
  int count;

  for (count = 0; count < numberDisks; count ++)
    diskStrings[count] = diskListParams[count].text;

  status = vshCursorMenu("Please choose the disk on which to operate:",
			 diskStrings, numberDisks, selectedDiskNumber);
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
	  slices[count1].color = &colors[colorCounter++];
	  params.foreground.red = slices[count1].color->red;
	  params.foreground.green = slices[count1].color->green;
	  params.foreground.blue = slices[count1].color->blue;
	  if (slices[count1].entryType == partition_logical)
	    {
	      params.xCoord1 += 3;
	      params.yCoord1 += 3;
	      params.width -= 6;
	      params.height -= 6;
	    }
	  windowComponentSetData(canvas, &params, 1);
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
  printf("%s\nCopyright (C) 1998-2007 J. Andrew McLaughlin\n", programName);
}


static void display(void)
{
  listItemParameters *sliceListParams = NULL;
  char lineString[SLICESTRING_LENGTH + 2];
  int slc = 0;
  int isDefrag = 0;
  int isHide = 0;
  int isMove = 0;
  textAttrs attrs;
  int count;

  if (graphics)
    {
      // Re-populate our slice list component
      sliceListParams = malloc(numberSlices * sizeof(listItemParameters));
      if (sliceListParams == NULL)
	return;
      for (count = 0; count < numberSlices; count ++)
	strncpy(sliceListParams[count].text, slices[count].string,
		WINDOW_MAX_LABEL_LENGTH);
      windowComponentSetSelected(sliceList, 0);
      windowComponentSetData(sliceList, sliceListParams, numberSlices);
      free(sliceListParams);
      windowComponentSetSelected(sliceList, selectedSlice);

      drawDiagram();

      // Depending on which type slice was selected (i.e. partition vs.
      // empty space) we enable/disable button choices
      if (slices[selectedSlice].typeId)
	{
	  // It's a partition

	  if (slices[selectedSlice].opFlags & FS_OP_DEFRAG)
	    isDefrag = 1;

	  if (PARTITION_TYPEID_IS_HIDEABLE(slices[selectedSlice].typeId) ||
	      PARTITION_TYPEID_IS_HIDDEN(slices[selectedSlice].typeId))
	    isHide = 1;

	  if (!ISLOGICAL(&slices[selectedSlice]))
	    isMove = 1;

	  windowComponentSetEnabled(partMenuContents.items[PARTMENU_COPY]
				    .key, 1);
	  windowComponentSetEnabled(partMenuContents.items[PARTMENU_PASTE]
				    .key, 0);
	  windowComponentSetEnabled(setActiveButton, 1);
	  windowComponentSetEnabled(partMenuContents.items[PARTMENU_SETACTIVE]
				    .key, 1);
	  windowComponentSetEnabled(deleteButton, 1);
	  windowComponentSetEnabled(partMenuContents.items[PARTMENU_DELETE]
				    .key, 1);
	  windowComponentSetEnabled(formatButton, 1);
	  windowComponentSetEnabled(partMenuContents.items[PARTMENU_FORMAT]
				    .key, 1);
	  windowComponentSetEnabled(defragButton, isDefrag);
	  windowComponentSetEnabled(partMenuContents.items[PARTMENU_DEFRAG]
				    .key, isDefrag);
	  windowComponentSetEnabled(resizeButton, 1);
	  windowComponentSetEnabled(partMenuContents.items[PARTMENU_RESIZE]
				    .key, 1);
	  windowComponentSetEnabled(hideButton, isHide);
	  windowComponentSetEnabled(partMenuContents.items[PARTMENU_HIDE]
				    .key, isHide);
	  windowComponentSetEnabled(moveButton, isMove);
	  windowComponentSetEnabled(partMenuContents.items[PARTMENU_MOVE]
				    .key, isMove);
	  windowComponentSetEnabled(newButton, 0);
	  windowComponentSetEnabled(partMenuContents.items[PARTMENU_CREATE]
				    .key, 0);
	  windowComponentSetEnabled(partMenuContents.items[PARTMENU_SETTYPE]
				    .key, 1);
	}
      else
	{
	  // It's empty space
	  windowComponentSetEnabled(partMenuContents.items[PARTMENU_COPY]
				    .key, 0);
	  windowComponentSetEnabled(partMenuContents.items[PARTMENU_PASTE]
				    .key, clipboardSliceValid);
	  windowComponentSetEnabled(setActiveButton, 0);
	  windowComponentSetEnabled(partMenuContents.items[PARTMENU_SETACTIVE]
				    .key, 0);
	  windowComponentSetEnabled(deleteButton, 0);
	  windowComponentSetEnabled(partMenuContents.items[PARTMENU_DELETE]
				    .key, 0);
	  windowComponentSetEnabled(formatButton, 0);
	  windowComponentSetEnabled(partMenuContents.items[PARTMENU_FORMAT]
				    .key, 0);
	  windowComponentSetEnabled(defragButton, 0);
	  windowComponentSetEnabled(partMenuContents.items[PARTMENU_DEFRAG]
				    .key, 0);
	  windowComponentSetEnabled(partMenuContents.items[PARTMENU_RESIZE]
				    .key, 0);
	  windowComponentSetEnabled(hideButton, 0);
	  windowComponentSetEnabled(partMenuContents.items[PARTMENU_HIDE]
				    .key, 0);
	  windowComponentSetEnabled(moveButton, 0);
	  windowComponentSetEnabled(partMenuContents.items[PARTMENU_MOVE]
				    .key, 0);
	  windowComponentSetEnabled(newButton, 1);
	  windowComponentSetEnabled(partMenuContents.items[PARTMENU_CREATE]
				    .key, 1);
	  windowComponentSetEnabled(resizeButton, 0);
	  windowComponentSetEnabled(partMenuContents.items[PARTMENU_SETTYPE]
				    .key, 0);
	}

      // Other buttons enabled/disabled...
      windowComponentSetEnabled(deleteAllButton, numberPartitions);
      windowComponentSetEnabled(partMenuContents.items[PARTMENU_DELETEALL]
				.key, numberPartitions);
      windowComponentSetEnabled(fileMenuContents.items[FILEMENU_RESTOREBACKUP]
				.key, backupAvailable);
      windowComponentSetEnabled(undoButton, changesPending);
      windowComponentSetEnabled(fileMenuContents.items[FILEMENU_UNDO]
				.key, changesPending);
      windowComponentSetEnabled(writeButton, changesPending);
      windowComponentSetEnabled(fileMenuContents.items[FILEMENU_WRITE]
				.key, changesPending);
    }
  else
    {
      printBanner();
      bzero(&attrs, sizeof(textAttrs));
      bzero(lineString, (SLICESTRING_LENGTH + 2));
      for (count = 0; count <= SLICESTRING_LENGTH; count ++)
	lineString[count] = 196;

      printf("\n%s\n\n  %s\n %s\n", diskListParams[selectedDiskNumber].text,
	     sliceListHeader, lineString);

      // Print info about the partitions
      for (slc = 0; slc < numberSlices; slc ++)
	{
	  printf(" ");
	  
	  if (slc == selectedSlice)
	    attrs.flags = TEXT_ATTRS_REVERSE;
	  else
	    attrs.flags = 0;

	  textPrintAttrs(&attrs, " ");
	  textPrintAttrs(&attrs, slices[slc].string);
	  for (count = strlen(slices[slc].string); count < SLICESTRING_LENGTH;
	       count ++)
	    textPrintAttrs(&attrs, " ");

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


static int mountedCheck(slice *entry)
{
  // If the slice is mounted, query whether to ignore, unmount, or cancel

  int status = 0;
  int choice = 0;
  char tmpChar[160];
  disk tmpDisk;
  char character;

  status = diskGet(entry->diskName, &tmpDisk);
  if (status < 0)
    // The disk probably doesn't exist (yet).  So, it obviously can't be
    // mounted
    return (status = 0);

  if (!tmpDisk.mounted)
    return (status = 0);

  sprintf(tmpChar, "The partition is mounted as %s.  It is STRONGLY "
	  "recommended\nthat you unmount before continuing",
	  tmpDisk.mountPoint);

  if (graphics)
    choice =
      windowNewChoiceDialog(window, "Partition Is Mounted", tmpChar,
			    (char *[]) { "Ignore", "Unmount", "Cancel" },
			    3, 1);
  else
    {
      printf("\n%s (I)gnore/(U)nmount/(C)ancel?: ", tmpChar);
      textInputSetEcho(0);

      while(1)
	{
	  character = getchar();
      
	  if ((character == 'i') || (character == 'I'))
	    {
	      printf("Ignore\n");
	      choice = 0;
	      break;
	    }
	  else if ((character == 'u') || (character == 'U'))
	    {
	      printf("Unmount\n");
	      choice = 1;
	      break;
	    }
	  else if ((character == 'c') || (character == 'C'))
	    {
	      printf("Cancel\n");
	      choice = 2;
	      break;
	    }
	}

      textInputSetEcho(1);
    }

  if ((choice < 0) || (choice == 2))
    // Cancelled
    return (status = ERR_CANCELLED);

  else if (choice == 1)
    {
      // Try to unmount the filesystem
      status = filesystemUnmount(tmpDisk.mountPoint);
      if (status < 0)
	{
	  error("Unable to unmount %s", tmpDisk.mountPoint);
	  return (status);
	}
    }
  
  return (status = 0);
}


static void format(slice *formatSlice)
{
  // Prompt, and format partition

  int status = 0;
  char *fsTypes[] = { "FAT", "EXT2", "Linux-swap", "None" };
  char *fatTypes[] = { "Default", "FAT12", "FAT16", "FAT32" };
  const char *chooseString = "Choose the filesystem type:";
  const char *fatString = "Choose the FAT type:";
  int typeNum = 0;
  char typeName[16];
  char tmpChar[160];

  if (changesPending)
    {
      error("A partition format cannot be undone, and it is required that\n"
	    "you write your other changes to disk before continuing.");
      return;
    }

  status = mountedCheck(formatSlice);
  if (status < 0)
    return;

  if (graphics)
    {
      sprintf(tmpChar, "Format Partition %s", formatSlice->sliceName);
      typeNum =
	windowNewRadioDialog(window, tmpChar, chooseString, fsTypes, 4, 0);
    }
  else
    typeNum = vshCursorMenu(chooseString, fsTypes, 4, 0);

  if (typeNum < 0)
    return;

  strncpy(typeName, fsTypes[typeNum], 16);

  // If the filesystem type is FAT, additionally offer to let the user
  // choose the subtype
  if (!strncasecmp(typeName, "fat", 3))
    {
      if (graphics)
	typeNum =
	  windowNewRadioDialog(window, "FAT Type", fatString, fatTypes, 4, 0);
      else
	typeNum = vshCursorMenu(fatString, fatTypes, 4, 0);

      if (typeNum < 0)
	return;

      strncpy(typeName, fatTypes[typeNum], 16);

      if (!strcasecmp(typeName, "default"))
	strcpy(typeName, "FAT");
    }

  if (!strcasecmp(typeName, "none"))
    {
      sprintf(tmpChar, "Unformat partition %s?  (This change cannot be "
	      "undone)", formatSlice->sliceName);

      if (!yesOrNo(tmpChar))
	return;

      status = filesystemClobber(formatSlice->diskName);
    }
  else
    {
      sprintf(tmpChar, "Format partition %s as %s?\n(This change cannot be "
	      "undone)", formatSlice->sliceName, typeName);

      if (!yesOrNo(tmpChar))
	return;

      // Do the format
      sprintf(tmpChar, "/programs/format -s -t %s %s", typeName,
	      formatSlice->diskName);

      status = system(tmpChar);
    }

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

  // Regenerate the slice list
  makeSliceList();

  return;
}


static void defragment(slice *formatSlice)
{
  // Prompt, and defragment partition

  int status = 0;
  progress prog;
  objectKey progressDialog = NULL;
  char tmpChar[160];

  if (changesPending)
    {
      error("A partition defragmentation cannot be undone, and it is "
	    "required\nthat you write your other changes to disk before "
	    "continuing.");
      return;
    }

  sprintf(tmpChar, "Defragment partition %s?\n(This change cannot be "
	  "undone)", formatSlice->sliceName);

  if (!yesOrNo(tmpChar))
    return;

  status = mountedCheck(formatSlice);
  if (status < 0)
    return;

  sprintf(tmpChar, "Please use this feature with caution; it is not\n"
	  "well tested.  Continue?");
  if (graphics)
    {
      if (!windowNewQueryDialog(window, "New Feature", tmpChar))
	return;
    }
  else
    {
      if (!yesOrNo(tmpChar))
	return;
    }

  // Do the defrag

  bzero((void *) &prog, sizeof(progress));
  if (graphics)
    progressDialog =
      windowNewProgressDialog(window, "Defragmenting...", &prog);
  else
    vshProgressBar(&prog);

  status = filesystemDefragment(formatSlice->diskName, &prog);

  if (graphics && progressDialog)
    windowProgressDialogDestroy(progressDialog);
  else
    vshProgressBarDestroy(&prog);

  // Regenerate the slice list
  makeSliceList();

  if (status < 0)
    error("Error during defragmentation");

  else
    {
      sprintf(tmpChar, "Defragmentation complete");
      if (graphics)
	windowNewInfoDialog(window, "Success", tmpChar);
      else
	{
	  printf("%s\n", tmpChar);
	  pause();
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
  char *buff = NULL;

  buff = malloc(1024);
  if (buff == NULL)
    return;

  if (slc->typeId)
    sprintf(buff, "PARTITION %s INFO:\n\nActive : %s\nType ID : %02x\n",
	    slc->sliceName, (slc->active? "yes" : "no"), slc->typeId);
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

  free(buff);
}


static int getTypeListParams(listItemParameters **typeListParams)
{
  // Get the list of supported partition types as an array of
  // listItemParameters structures

  partitionType *types = NULL;
  int numberTypes = 0;
  int count;

  // Get the list of types
  types = diskGetPartTypes();
  if (types == NULL)
    return (numberTypes = ERR_NODATA);

  for (count = 0; (types[count].code != 0); count ++)
    numberTypes += 1;
  
  // Make an array of list item parameters

  *typeListParams = malloc(numberTypes * sizeof(listItemParameters));
  if (*typeListParams == NULL)
    {
      numberTypes = ERR_MEMORY;
      goto out;
    }

  for (count = 0; count < numberTypes; count ++)
    snprintf((*typeListParams)[count].text, WINDOW_MAX_LABEL_LENGTH,
	     "%02x  %s", types[count].code, types[count].description);

 out:
  memoryRelease(types);
  return (numberTypes);
}


static int typeListDialog(listItemParameters *typeListParams, int numberTypes,
			  int select)
{
  int selection = 0;
  objectKey typesDialog = NULL;
  objectKey typesList = NULL;
  objectKey selectButton = NULL;
  objectKey cancelButton = NULL;
  componentParameters params;
  windowEvent event;

  // Create a new window, not a modal dialog
  typesDialog = windowNewDialog(window, PARTTYPES);
  if (typesDialog == NULL)
    {
      error("Can't create dialog window");
      return (selection = ERR_NOCREATE);
    }

  bzero(&params, sizeof(componentParameters));
  params.gridWidth = 2;
  params.gridHeight = 1;
  params.padTop = 5;
  if (!select)
    params.padBottom = 5;
  params.padLeft = 5;
  params.padRight = 5;
  params.orientationX = orient_center;
  params.orientationY = orient_middle;
  
  // Make a list for our info
  typesList = windowNewList(typesDialog, windowlist_textonly, 10, 2, 0,
			    typeListParams, numberTypes, &params);

  if (select)
    {
      params.gridY += 1;
      params.gridWidth = 1;
      params.padBottom = 5;
      params.flags |= WINDOW_COMPFLAG_FIXEDWIDTH;
      params.orientationX = orient_right;
      selectButton = windowNewButton(typesDialog, "Select", NULL, &params);

      params.gridX += 1;
      params.orientationX = orient_left;
      cancelButton = windowNewButton(typesDialog, "Cancel", NULL, &params);
    }

  windowCenterDialog(window, typesDialog);
  windowSetVisible(typesDialog, 1);

  while(1)
    {
      // Check for window close
      if ((windowComponentEventGet(typesDialog, &event) > 0) &&
	  (event.type & EVENT_WINDOW_CLOSE))
	break;

      if (select)
	{
	  // Check for selection
	  if ((windowComponentEventGet(selectButton, &event) > 0) &&
	      (event.type & EVENT_MOUSE_LEFTUP))
	    {
	      windowComponentGetSelected(typesList, &selection);
	      break;
	    }

	  // Check for cancel
	  if ((windowComponentEventGet(cancelButton, &event) > 0) &&
	      (event.type & EVENT_MOUSE_LEFTUP))
	    {
	      selection = ERR_CANCELLED;
	      break;
	    }
	}
      
      multitaskerYield();
    }
  
  windowDestroy(typesDialog);

  return (selection);
}


static void listTypes(void)
{
  int numberTypes = 0;
  listItemParameters *typeListParams = NULL;
  int count;

  numberTypes = getTypeListParams(&typeListParams);
  if (numberTypes < 0)
    return;

  if (graphics)
    typeListDialog(typeListParams, numberTypes, 0);
  else
    {
      printf("\n%s:\n", PARTTYPES);

      for (count = 0; count <= (numberTypes / 2); count ++)
	{
	  printf("  %s", typeListParams[count].text);
	  textSetColumn(30);
	  if ((count + (numberTypes / 2)) < numberTypes)
	    printf("  %s\n", typeListParams[count + (numberTypes / 2)].text);
	}

      pause();
    }

  if (typeListParams)
    free(typeListParams);

  return;
}


static int setType(int partition)
{
  int status = 0;
  int numberTypes = 0;
  listItemParameters *typeListParams = NULL;
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

  types = diskGetPartTypes();

  if (graphics)
    {
      numberTypes = getTypeListParams(&typeListParams);
      if (numberTypes < 0)
	{
	  status = numberTypes;
	  goto out;
	}

      newCode = typeListDialog(typeListParams, numberTypes, 1);

      free(typeListParams);

      if (newCode < 0)
	{
	  status = newCode;
	  goto out;
	}

      newCode = types[newCode].code;
    }

  else {
    while(1)
      {
	bzero(code, 8);

	printf("\nEnter the hexadecimal code to set as the type ('L' to list, "
	       "'Q' to quit):\n-> ");
	status = readLine("0123456789AaBbCcDdEeFfLlQq", code, 8);
	printf("\n");

	if (status < 0)
	  goto out;

	if ((code[0] == 'L') || (code[0] == 'l'))
	  {
	    listTypes();
	    continue;
	  }
	if ((code[0] == '\0') || (code[0] == 'Q') || (code[0] == 'q'))
	  {
	    status = ERR_NODATA;
	    goto out;
	  }

	if (strlen(code) != 2)
	  continue;

	// Turn it into a number
	newCode = xtoi(code);

	// Is it a supported type?
	for (count = 0; types[count].code != 0; count ++)
	  if (types[count].code == newCode)
	    break;

	if (types[count].code == 0)
	  {
	    error("Unsupported partition type %x", newCode);
	    status = ERR_INVALID;
	    goto out;
	  }

	break;
      }
  }

  // Change the value
  entry.typeId = newCode;

  // Set the partition
  setPartitionEntry(partition, &entry);

  // Redo the slice strings
  makeSliceList();

  changesPending += 1;
  status = 0;

 out:
  memoryRelease(types);
  return (status);
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
      status = writePartitionTable(selectedDisk, mainTable, confirm);
      if (status < 0)
	error("Unable to write the partition table of %s.",
	      selectedDisk->name);

      // Tell the kernel to reexamine the partition tables
      diskReadPartitions(selectedDisk->name);
    }
}


static int move(int sliceId)
{
  int status = 0;
  slice entry;
  int sliceNumber = ERR_NOSUCHENTRY;
  unsigned moveRange[] = { -1, -1 };
  char number[10];
  unsigned newStartCylinder = 0;
  unsigned newStartLogical = 0;
  int moveLeft = 0;
  unsigned char *buffer = NULL;
  unsigned endLogical = 0;
  unsigned srcSector = 0;
  unsigned destSector = 0;
  unsigned overlapSector = 0;
  unsigned sectorsToCopy = 0;
  unsigned sectorsPerOp = 0;
  int startSeconds = 0;
  int remainingSeconds = 0;
  progress prog;
  objectKey progressDialog = NULL;
  char tmpChar[160];
  int count;

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
    if (slices[count].typeId && (slices[count].sliceId == sliceId))
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

  status = mountedCheck(&entry);
  if (status < 0)
    return (status);

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
	  newStartCylinder = getNumberDialog("Starting Cylinder", tmpChar);
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
	  "Continue?", entry.startCylinder, newStartCylinder);
  if (graphics)
    {
      if (!windowNewQueryDialog(window, "Moving", tmpChar))
	return (status = 0);
    }
  else
    {
      if (!yesOrNo(tmpChar))
	return (status = 0);
    }

  sectorsPerOp = cylinderSectors;
  if (moveLeft && ((entry.startLogical - newStartLogical) < sectorsPerOp))
    sectorsPerOp = (entry.startLogical - newStartLogical);
  else if (!moveLeft && (newStartLogical - entry.startLogical) < sectorsPerOp)
    sectorsPerOp = (newStartLogical - entry.startLogical);

  if (moveLeft)
    {
      srcSector = entry.startLogical;
      destSector = newStartLogical;

      // Will the new location overlap the old location?
      if ((newStartLogical + entry.sizeLogical) > entry.startLogical)
	overlapSector = entry.startLogical;
    }
  else
    {
      srcSector = (entry.startLogical + (entry.sizeLogical - sectorsPerOp));
      destSector = (newStartLogical + (entry.sizeLogical - sectorsPerOp));

      // Will the new location overlap the old location?
      if ((entry.startLogical + entry.sizeLogical) > newStartLogical)
	overlapSector = (entry.startLogical + (entry.sizeLogical - 1));
    }

  sectorsToCopy = entry.sizeLogical;
  
  // Get a memory buffer to copy data to/from
  buffer = malloc(sectorsPerOp * selectedDisk->sectorSize);
  if (buffer == NULL)
    {
      error("Unable to allocate memory");
      return (status = ERR_MEMORY);
    }

  bzero((void *) &prog, sizeof(progress));
  prog.total = sectorsToCopy;
  strcpy(prog.statusMessage, "Time remaining: ? hours ?? minutes");
  if (!overlapSector || !((overlapSector >= destSector) &&
			  (overlapSector < (destSector + sectorsPerOp))))
    prog.canCancel = 1;

  sprintf(tmpChar, "Moving %u Mb",
	  (entry.sizeLogical / (1048576 / selectedDisk->sectorSize)));

  if (graphics)
    progressDialog = windowNewProgressDialog(window, tmpChar, &prog);
  else
    {
      printf("\n%s\n", tmpChar);
      vshProgressBar(&prog);
    }

  startSeconds = rtcUptimeSeconds();

  // Copy the data
  while (sectorsToCopy > 0)
    {
      if (sectorsToCopy < sectorsPerOp)
	sectorsPerOp = sectorsToCopy;

      // Read from source
      status =
	diskReadSectors(selectedDisk->name, srcSector, sectorsPerOp, buffer);
      if (status < 0)
	{
	  error("Read error %d reading sectors %u-%u from disk %s",
		status, srcSector, (srcSector + (sectorsPerOp - 1)),
		selectedDisk->name);
	  goto out;
	}

      if (prog.cancel)
	goto out;

      // Write to destination
      status =
	diskWriteSectors(selectedDisk->name, destSector, sectorsPerOp, buffer);
      if (status < 0)
	{
	  error("Write error %d writing sectors %u-%u to disk %s",
		status, destSector, (destSector + (sectorsPerOp - 1)),
		selectedDisk->name);
	  goto out;
	}

      if (prog.cancel)
	goto out;

      sectorsToCopy -= sectorsPerOp;
      srcSector += (moveLeft? sectorsPerOp : -sectorsPerOp);
      destSector += (moveLeft? sectorsPerOp : -sectorsPerOp);

      if (lockGet(&(prog.lock)) >= 0)
	{
	  prog.finished += sectorsPerOp;
	  prog.percentFinished = ((prog.finished * 100) / prog.total);
	  remainingSeconds = (((rtcUptimeSeconds() - startSeconds) *
			       (sectorsToCopy / sectorsPerOp)) /
			      (prog.finished / sectorsPerOp));
	  if (remainingSeconds > 60)
	    sprintf(prog.statusMessage, "Time remaining: %d hours %02d "
		    "minutes", (remainingSeconds / 3600),
		    ((remainingSeconds % 3600) / 60));
	  else
	    sprintf(prog.statusMessage, "Time remaining: less than 1 minute");

	  // Will the next operation overwrite any of the original data?
	  if (overlapSector && (overlapSector >= destSector) &&
	      (overlapSector < (destSector + sectorsPerOp)))
	    prog.canCancel = 0;

	  lockRelease(&(prog.lock));
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
  free(buffer);

  if (graphics && progressDialog)
    windowProgressDialogDestroy(progressDialog);
  else
    vshProgressBarDestroy(&prog);

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

  if ((int) returnType == ERR_NOFREE)
    // The partition table is full of entries, in its current
    // configuration.
    error("The partition table is full of primary partitions.  Use "
	  "more\nlogical partitions in order to create more.");

  return (returnType);
}


static int doCreate(int sliceNumber, partEntryType entryType,
		    unsigned startCylinder, unsigned endCylinder)
{
  // Does the non-interactive work of creating a partition

  int newSliceNumber = ERR_NOSUCHENTRY;
  slice *emptySlice = NULL;
  slice newEntry;
  partitionTable *table = NULL;
  partitionTable *extendedTable = NULL;
  slice extendedEntry;

  emptySlice = &(slices[sliceNumber]);
  bzero(&newEntry, sizeof(slice));
  newEntry.typeId = 0x01;

  if (emptySlice->typeId)
    {
      error("Slice to create in is not empty");
      return (newSliceNumber = ERR_INVALID);
    }

  newEntry.startCylinder = startCylinder;
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

  newEntry.endCylinder = endCylinder;
  newEntry.endHead = (selectedDisk->heads - 1);
  newEntry.endSector = selectedDisk->sectorsPerCylinder;

  newEntry.startLogical =
    ((newEntry.startCylinder * selectedDisk->heads *
      selectedDisk->sectorsPerCylinder) +
     (newEntry.startHead * selectedDisk->sectorsPerCylinder));

  newEntry.sizeLogical =   
    ((((newEntry.endCylinder - newEntry.startCylinder) + 1) *
      (selectedDisk->heads * selectedDisk->sectorsPerCylinder)) -
     (newEntry.startHead * selectedDisk->sectorsPerCylinder));

  newEntry.entryType = entryType;

  if (newEntry.entryType == partition_logical)
    {
      // Create the extended partition.

      table = findLastTable(mainTable);
      if (table == NULL)
	return (newSliceNumber = ERR_BUG);

      // Create an extended partition entry in this table with the same
      // (current) parameters as the new partition.

      memcpy(&extendedEntry, &newEntry, sizeof(slice));
      extendedEntry.typeId = 0x0F;
      extendedEntry.entryType = partition_extended;
      
      extendedTable = malloc(sizeof(partitionTable));
      if (extendedTable == NULL)
	{
	  error("Unable to allocate a new extended table");
	  return (newSliceNumber = ERR_MEMORY);
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
    error("Unable to add new partition entry");

  return (newSliceNumber);
}


static partEntryType queryPrimaryLogical(objectKey primLogRadio)
{
  partEntryType retType = partition_none;
  int response = -1;

  if (graphics)
    {
      if (windowComponentGetSelected(primLogRadio, &response) < 0)
	return (retType = partition_none);
    }
  else
    {
      response = vshCursorMenu("Choose the partition type:",
			       (char *[]){ "primary", "logical" }, 2, 0);
      if (response < 0)
	return (retType = partition_none);
    }

  switch (response)
    {
    case 0:
    default:
      retType = partition_primary;
      break;
    case 1:
      retType = partition_logical;
      break;
    }
  
  return (retType);
}


static void create(int sliceNumber)
{
  // This is the interactive partition creation routine.

  enum { units_normal, units_mb, units_cylsize } units = units_normal;

  int status = 0;
  unsigned minStartCylinder = 0;
  unsigned maxEndCylinder = 0;
  partEntryType entryType = partition_none;
  char startCyl[10];
  char endCyl[10];
  objectKey createDialog = NULL;
  objectKey primLogRadio = NULL;
  objectKey startCylField = NULL;
  objectKey endCylField = NULL;
  objectKey okButton = NULL;
  objectKey cancelButton = NULL;
  componentParameters params;
  windowEvent event;
  unsigned startCylinder = 0;
  unsigned endCylinder = 0;
  int newSliceNumber = ERR_NOSUCHENTRY;
  char tmpChar[160];
  int count;

  minStartCylinder = slices[sliceNumber].startCylinder;
  maxEndCylinder = slices[sliceNumber].endCylinder;

  while (1)
    {
      // See if we can create a slice here, and if so, what type?
      entryType = canCreate(sliceNumber);
      if ((int) entryType < 0)
	return;

      if (graphics)
	{
	  createDialog = windowNewDialog(window, "Create Partition");

	  bzero(&params, sizeof(componentParameters));
	  params.gridWidth = 1;
	  params.gridHeight = 1;
	  params.padTop = 5;
	  params.padLeft = 5;
	  params.padRight = 5;
	  params.orientationX = orient_right;
	  params.orientationY = orient_middle;
      
	  windowNewTextLabel(createDialog, "Partition\ntype:", &params);

	  // A radio to select 'primary' or 'logical'
	  params.gridX = 1;
	  params.orientationX = orient_left;
	  primLogRadio = windowNewRadioButton(createDialog, 2, 1, (char *[])
	      { "Primary", "Logical" }, 2 , &params);
	  if (entryType != partition_any)
	    {
	      if (entryType == partition_logical)
		windowComponentSetSelected(primLogRadio, 1);
	      windowComponentSetEnabled(primLogRadio, 0);
	    }

	  // A label and field for the starting cylinder
	  snprintf(tmpChar, 160, STARTCYL_MESSAGE, minStartCylinder,
		   maxEndCylinder);
	  params.gridX = 0;
	  params.gridY = 1;
	  params.gridWidth = 2;
	  windowNewTextLabel(createDialog, tmpChar, &params);

	  params.gridY = 2;
	  params.flags |= WINDOW_COMPFLAG_HASBORDER;
	  startCylField = windowNewTextField(createDialog, 10, &params);

	  // A label and field for the ending cylinder
	  snprintf(tmpChar, 160, ENDCYL_MESSAGE, minStartCylinder,
		   maxEndCylinder,
		   cylsToMb(selectedDisk,
			    (maxEndCylinder - minStartCylinder + 1)),
		   (maxEndCylinder - minStartCylinder + 1));
	  params.gridY = 3;
	  params.flags &= ~WINDOW_COMPFLAG_HASBORDER;
	  windowNewTextLabel(createDialog, tmpChar, &params);

	  params.gridY = 4;
	  params.flags |= WINDOW_COMPFLAG_HASBORDER;
	  endCylField = windowNewTextField(createDialog, 10, &params);

	  // Make 'OK' and 'cancel' buttons
	  params.gridY = 5;
	  params.gridWidth = 1;
	  params.padBottom = 5;
	  params.orientationX = orient_right;
	  params.flags &= ~WINDOW_COMPFLAG_HASBORDER;
	  params.flags |= WINDOW_COMPFLAG_FIXEDWIDTH;
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

	  entryType = queryPrimaryLogical(primLogRadio);
	  if (entryType == partition_none)
	    return;

	  windowComponentGetData(startCylField, startCyl, 10);
	  windowComponentGetData(endCylField, endCyl, 10);
	  windowDestroy(createDialog);
	}

      else
	{
	  if (entryType == partition_any)
	    {
	      // Does the user prefer primary or logical?
	      entryType = queryPrimaryLogical(NULL);
	      if (entryType == partition_none)
		return;
	    }
	  else
	    printf("\nCreating %s partition\n",
		   ((entryType == partition_primary)? "primary" : "logical"));

	  // Don't create an extended partition on the first cylinder
	  if ((entryType == partition_logical) && (minStartCylinder == 0))
	    minStartCylinder = 1;

	  printf("\n"STARTCYL_MESSAGE", or 'Q' to quit:\n-> ",
		 minStartCylinder, maxEndCylinder);
	  
	  status = readLine("0123456789Qq", startCyl, 10);
	  if (status < 0)
	    continue;
	  
	  if ((startCyl[0] == 'Q') || (startCyl[0] == 'q'))
	    return;

	  printf("\n"ENDCYL_MESSAGE", or 'Q' to quit:\n-> ",
		 atoi(startCyl), maxEndCylinder,
		 cylsToMb(selectedDisk,
			  (maxEndCylinder - minStartCylinder + 1)),
		 (maxEndCylinder - minStartCylinder + 1));
      
	  status = readLine("0123456789CcMmQq", endCyl, 10);
	  if (status < 0)
	    return;

	  printf("\n");
	  
	  if ((endCyl[0] == 'Q') || (endCyl[0] == 'q'))
	    return;
	}

      startCylinder = atoi(startCyl);

      // Make sure the start cylinder is legit
      if ((startCylinder < minStartCylinder) ||
	  (startCylinder > maxEndCylinder))
	{
	  error("Invalid starting cylinder number");
	  continue;
	}
      
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
	  endCylinder = (startCylinder + mbToCyls(selectedDisk, count) - 1);
	  break;
	case units_cylsize:
	  endCylinder = (startCylinder + count - 1);
	  break;
	default:
	  endCylinder = count;
	  break;
	}

      if ((endCylinder < minStartCylinder) || (endCylinder > maxEndCylinder))
	{
	  error("Invalid ending cylinder number");
	  continue;
	}
      
      break;
    }

  newSliceNumber =
    doCreate(sliceNumber, entryType, startCylinder, endCylinder);
  if (newSliceNumber < 0)
    return;

  if (setType(slices[newSliceNumber].sliceId) < 0)
    {
      // Cancelled.  Remove it again.
      deletePartitionEntry(slices[newSliceNumber].sliceId);
      makeSliceList();
    }

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


static void resizePartition(int sliceId, slice *entry, unsigned newEndCylinder,
			    unsigned newEndHead, unsigned newEndSector,
			    unsigned newSize)
{
  // Resize the partition
  entry->endCylinder = newEndCylinder;
  entry->endHead = newEndHead;
  entry->endSector = newEndSector;
  entry->sizeLogical = newSize;
  
  // Set the partition
  setPartitionEntry(sliceId, entry);
  
  // Redo the slice strings
  makeSliceList();

  changesPending += 1;
}


static int resize(int sliceId)
{
  enum { units_normal, units_mb, units_cylsize } units = units_normal;

  int status = 0;
  slice entry;
  int sliceNumber = -1;
  int resizeFs = 0;
  unsigned minFsSectors = 0;
  unsigned maxFsSectors = 0;
  int haveResizeConstraints = 0;
  unsigned minEndCylinder = 0;
  unsigned maxEndCylinder = 0;
  objectKey resizeDialog = NULL;
  objectKey partCanvas = NULL;
  objectKey endCylField = NULL;
  objectKey okButton = NULL;
  objectKey cancelButton = NULL;
  char newEndString[10];
  unsigned newEndCylinder = 0;
  unsigned oldEndCylinder, oldEndHead, oldEndSector, oldSizeLogical;
  int didResize = 0;
  componentParameters params;
  windowDrawParameters drawParams;
  windowEvent event;
  unsigned newSize = 0;
  progress prog;
  objectKey bannerDialog = NULL;
  objectKey progressDialog = NULL;
  char tmpChar[256];
  int count;

  if (sliceId < 0)
    return (status = ERR_INVALID);

  status = getPartitionEntry(sliceId, &entry);
  if (status < 0)
    return (status);

  // Determine whether or not we can resize the filesystem
  if ((slices[selectedSlice].opFlags & FS_OP_RESIZE) ||
      !strcmp(slices[selectedSlice].fsType, "ntfs"))
    {
      // We can resize this filesystem.
      resizeFs = 1;

      char *optionStrings[] =
	{
	  "Filesystem and partition (recommended)",
	  "Partition only"
	};
      int selected = -1;
      
      // We can resize the filesystem, but does the user want to?
      strcpy(tmpChar, "Please select the type of resize operation:");
      if (graphics)
	selected = windowNewRadioDialog(window, "Resize Type",
					tmpChar, optionStrings, 2, 0);
      else
	selected = vshCursorMenu(tmpChar, optionStrings, 2, 0);

      switch (selected)
	{
	case 0:
	  break;
	case 1:
	  resizeFs = 0;
	  break;
	default:
	  // Cancelled
	  return (status = 0);
	}

      if (resizeFs)
	{
	  if (changesPending)
	    {
	      error("A filesystem resize cannot be undone, and must be "
		    "committed\nto disk immediately.  You need to write your "
		    "other changes\nto disk before continuing.");
	      return (status = 0);
	    }

	  if (slices[selectedSlice].opFlags & FS_OP_RESIZECONST)
	    {
	      strcpy(tmpChar, "Collecting filesystem resizing constraints...");
	      if (graphics)
		bannerDialog = windowNewBannerDialog(window, "Filesystem",
						     tmpChar);
	      else
		printf("\n%s\n\n", tmpChar);

	      status =
		filesystemResizeConstraints(entry.diskName, &minFsSectors,
					    &maxFsSectors);

	      if (graphics && bannerDialog)
		windowDestroy(bannerDialog);

	      if (status < 0)
		{
		  snprintf(tmpChar, 256, "Error reading filesystem "
			   "information.  However, it is\npossible to resize "
			   "the partition anyway and discard all\nof the data "
			   "it contains.  Continue?");
		  if (graphics)
		    {
		      if (!windowNewQueryDialog(window, "Can't Resize "
						"Filesystem", tmpChar))
			return (status = 0);
		    }
		  else
		    {
		      if (!yesOrNo(tmpChar))
			return (status = 0);
		    }
	      
		  resizeFs = 0;
		}
	      else
		haveResizeConstraints = 1;
	    }
	}
    }
  else
    {
      // We can't resize this filesystem, but we will offer to resize
      // the partition anyway.
      snprintf(tmpChar, 256, "Resizing the filesystem on this partition is "
	       "not supported.\n[ Currently, only Windows NTFS can be "
	       "resized ]\nHowever, it is possible to resize the partition "
	       "anyway and\ndiscard all of the data it contains.  Continue?");
      if (graphics)
	{
	  if (!windowNewQueryDialog(window, "Can't Resize Filesystem",
				    tmpChar))
	    return (status = 0);
	}
      else
	{
	  if (!yesOrNo(tmpChar))
	    return (status = 0);
	}
    }

  status = mountedCheck(&entry);
  if (status < 0)
    return (status);

  // Find out which slice it is in our list
  for (count = 0; count < numberSlices; count ++)
    if (slices[count].typeId && (slices[count].sliceId == sliceId))
      {
	sliceNumber = count;
	break;
      }

  if (sliceNumber < 0)
    return (status = sliceNumber);

  // Calculate the minimum and maximum permissable sizes.

  minEndCylinder = entry.startCylinder;
  if (haveResizeConstraints)
    minEndCylinder += (((minFsSectors / cylinderSectors) +
			((minFsSectors % cylinderSectors)? 1 : 0)) - 1);

  if ((sliceNumber < (numberSlices - 1)) && !slices[sliceNumber + 1].typeId)
    maxEndCylinder = slices[sliceNumber + 1].endCylinder;
  else
    maxEndCylinder = entry.endCylinder;
  if (haveResizeConstraints)
    maxEndCylinder = min((entry.startCylinder +
			  ((maxFsSectors / cylinderSectors) +
			   ((maxFsSectors % cylinderSectors)? 1 : 0)) - 1),
			 maxEndCylinder);

  while (1)
    {
      if (graphics)
	{
	  resizeDialog = windowNewDialog(window, "Resize Partition");

	  bzero(&params, sizeof(componentParameters));
	  params.gridWidth = 2;
	  params.gridHeight = 1;
	  params.padTop = 10;
	  params.padLeft = 5;
	  params.padRight = 5;
	  params.orientationX = orient_center;
	  params.orientationY = orient_middle;

	  if (haveResizeConstraints)
	    {
	      params.flags |= WINDOW_COMPFLAG_HASBORDER;
	      partCanvas = windowNewCanvas(resizeDialog, (canvasWidth / 2),
					   canvasHeight, &params);
	    }

	  // A label and field for the new size
	  snprintf(tmpChar, 256, "Current ending cylinder: %u\n"ENDCYL_MESSAGE,
		  entry.endCylinder, minEndCylinder, maxEndCylinder,
		  cylsToMb(selectedDisk,
			   (maxEndCylinder - minEndCylinder + 1)),
		  (maxEndCylinder - minEndCylinder + 1));
	  params.gridY++;
	  params.padTop = 5;
	  params.orientationX = orient_left;
	  params.flags &= ~WINDOW_COMPFLAG_HASBORDER;
	  windowNewTextLabel(resizeDialog, tmpChar, &params);
      
	  params.gridY++;
	  params.flags |= WINDOW_COMPFLAG_HASBORDER;
	  endCylField = windowNewTextField(resizeDialog, 10, &params);

	  // Make 'OK' and 'cancel' buttons
	  params.gridY++;
	  params.gridWidth = 1;
	  params.padBottom = 5;
	  params.orientationX = orient_right;
	  params.flags &= ~WINDOW_COMPFLAG_HASBORDER;
	  params.flags |= WINDOW_COMPFLAG_FIXEDWIDTH;
	  okButton = windowNewButton(resizeDialog, "OK", NULL, &params);

	  params.gridX = 1;
	  params.orientationX = orient_left;
	  cancelButton =
	    windowNewButton(resizeDialog, "Cancel", NULL, &params);
      
	  // Make the window visible
	  windowSetResizable(resizeDialog, 0);
	  windowCenterDialog(window, resizeDialog);
	  windowSetVisible(resizeDialog, 1);

	  if (haveResizeConstraints)
	    {
	      // Set up our drawing parameters for the canvas
	      bzero(&drawParams, sizeof(windowDrawParameters));
	      drawParams.operation = draw_rect;
	      drawParams.mode = draw_normal;
	      drawParams.width = windowComponentGetWidth(partCanvas);
	      drawParams.height = canvasHeight;
	      drawParams.thickness = 1;
	      drawParams.fill = 1;

	      // Draw a background
	      drawParams.foreground.red = slices[selectedSlice].color->red;
	      drawParams.foreground.green = slices[selectedSlice].color->green;
	      drawParams.foreground.blue  = slices[selectedSlice].color->blue;
	      windowComponentSetData(partCanvas, &drawParams, 1);

	      // Draw a shaded bit representing the used portion
	      drawParams.foreground.red =
		((drawParams.foreground.red * 2) / 3);
	      drawParams.foreground.green =
		((drawParams.foreground.green * 2) / 3);
	      drawParams.foreground.blue =
		((drawParams.foreground.blue * 2) / 3);
	      drawParams.width = ((minFsSectors * drawParams.width) /
				  slices[selectedSlice].sizeLogical);
	      windowComponentSetData(partCanvas, &drawParams, 1);
	    }	   

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
		  windowDestroy(resizeDialog);
		  return (status = 0);
		}

	      // Check for window close events
	      if ((windowComponentEventGet(resizeDialog, &event) > 0) &&
		  (event.type == EVENT_WINDOW_CLOSE))
		{
		  windowDestroy(resizeDialog);
		  return (status = 0);
		}

	      // Check for keyboard events
	      if ((windowComponentEventGet(endCylField, &event) > 0) &&
		  (event.type == EVENT_KEY_DOWN) &&
		  (event.key == (unsigned char) 10))
		break;

	      // Done
	      multitaskerYield();
	    }

	  windowComponentGetData(endCylField, newEndString, 10);
	  windowDestroy(resizeDialog);
	}

      else
	{
	  printf("\nCurrent ending cylinder: %u\n"ENDCYL_MESSAGE
		 ", or 'Q' to quit:\n-> ", entry.endCylinder,
		 minEndCylinder, maxEndCylinder,
		 cylsToMb(selectedDisk, (maxEndCylinder - minEndCylinder + 1)),
		 (maxEndCylinder - minEndCylinder + 1));
	  
	  status = readLine("0123456789CcMmQq", newEndString, 10);
	  if (status < 0)
	    continue;

	  printf("\n");
	  
	  if ((newEndString[0] == 'Q') || (newEndString[0] == 'q'))
	    return (status = 0);
	}

      count = (strlen(newEndString) - 1);

      if ((newEndString[count] == 'M') || (newEndString[count] == 'm'))
	{
	  units = units_mb;
	  newEndString[count] = '\0';
	}
      else if ((newEndString[count] == 'C') || (newEndString[count] == 'c'))
	{
	  units = units_cylsize;
	  newEndString[count] = '\0';
	}

      newEndCylinder = atoi(newEndString);

      switch (units)
	{
	case units_mb:
	  newEndCylinder = (entry.startCylinder +
			    mbToCyls(selectedDisk, newEndCylinder) - 1);
	  break;
	case units_cylsize:
	  newEndCylinder = (entry.startCylinder + newEndCylinder - 1);
	  break;
	default:
	  break;
	}

      if ((newEndCylinder < minEndCylinder) ||
	  (newEndCylinder > maxEndCylinder))
	{
	  error("Invalid ending cylinder number");
	  continue;
	}

      newSize = (((newEndCylinder + 1) * selectedDisk->heads *
		  selectedDisk->sectorsPerCylinder) - entry.startLogical);
      break;
    }

  // Before we go, warn that this is a new feature.
  snprintf(tmpChar, 256, "Resizing partition from %u to %u sectors.\n"
	   "Please use this feature with caution, and only after\n"
	   "making a backup of all important data.  Continue?",
	   entry.sizeLogical, newSize);
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

  oldEndCylinder = entry.endCylinder;
  oldEndHead = entry.endHead;
  oldEndSector = entry.endSector;
  oldSizeLogical = entry.sizeLogical;

  if (newSize >= entry.sizeLogical)
    {
      resizePartition(sliceId, &entry, newEndCylinder,
		      (selectedDisk->heads - 1),
		      selectedDisk->sectorsPerCylinder, newSize);
      didResize = 1;
    }

  // Now, if we're resizing the filesystem...
  if (resizeFs)
    {
      // Write everything
      writeChanges(0);

      // For now, we comment out the progress dialog stuff because it's not
      // playing nice with the NTFS resizing code.  Just use a simple banner
      // instead.

      bzero((void *) &prog, sizeof(progress));
      strcpy(tmpChar, "Resizing Filesystem...");
      if (graphics)
	progressDialog = windowNewProgressDialog(window, tmpChar, &prog);
      else
	vshProgressBar(&prog);

      if (!strcmp(slices[selectedSlice].fsType, "ntfs"))
	// NTFS resizing is done by our libntfs library
	status = ntfsResize(entry.diskName, newSize, &prog);
      else
	// The kernel will do the resize
	status = filesystemResize(entry.diskName, newSize, &prog);

      if (graphics && progressDialog)
	windowProgressDialogDestroy(progressDialog);
      else
	vshProgressBarDestroy(&prog);

      // Regenerate the slice list
      makeSliceList();

      if (status < 0)
	{
	  if (didResize)
	    {
	      // Undo the partition resize
	      entry.endCylinder = oldEndCylinder;
	      entry.endHead = oldEndHead;
	      entry.endSector = oldEndSector;
	      entry.sizeLogical = oldSizeLogical;
	      setPartitionEntry(sliceId, &entry);
	      changesPending += 1;
	      writeChanges(0);
	      makeSliceList();
	    }
	  if (status == ERR_CANCELLED)
	    error("Filesystem resize cancelled");
	  else
	    error("Error during filesystem resize");
	  return (status);
	}
    }

  if (!didResize)
    {
      resizePartition(sliceId, &entry, newEndCylinder,
		      (selectedDisk->heads - 1),
		      selectedDisk->sectorsPerCylinder, newSize);
      
      if (resizeFs)
	// We already resized the filesystem, so write everything
	writeChanges(0);
    }

  if (resizeFs)
    {
      snprintf(tmpChar, 256, "Filesystem resize complete");
      if (graphics)
	windowNewInfoDialog(window, "Success", tmpChar);
      else
	{
	  printf("\n%s\n", tmpChar);
	  pause();
	}
    }

  // Return success
  return (status = 0);
}


static disk *chooseDiskDialog(void)
{
  // Graphical way of prompting for disk selection

  int status = 0;
  disk *retDisk = NULL;
  int selected = 0;
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

  // Make a window list with all the disk choices
  dList = windowNewList(chooseWindow, windowlist_textonly, numberDisks, 1, 0,
			diskListParams, numberDisks, &params);

  // Make 'OK' and 'cancel' buttons
  params.gridY = 1;
  params.gridWidth = 1;
  params.padBottom = 5;
  params.orientationX = orient_right;
  params.flags |= WINDOW_COMPFLAG_FIXEDWIDTH;
  okButton = windowNewButton(chooseWindow, "OK", NULL, &params);

  params.gridX = 1;
  params.orientationX = orient_left;
  cancelButton = windowNewButton(chooseWindow, "Cancel", NULL, &params);

  // Make the window visible
  windowRemoveMinimizeButton(chooseWindow);
  windowRemoveCloseButton(chooseWindow);
  windowSetResizable(chooseWindow, 0);
  windowSetVisible(chooseWindow, 1);

  while(1)
    {
      // Check for our OK button
      status = windowComponentEventGet(okButton, &event);
      if ((status > 0) && (event.type == EVENT_MOUSE_LEFTUP))
	{
	  windowComponentGetSelected(dList, &selected);
	  retDisk = &(diskInfo[selected]);
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


static void copyIoThread(int argc, char *argv[])
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
  int startSeconds = rtcUptimeSeconds();
  int remainingSeconds = 0;

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

      if (!reader && args->prog && (lockGet(&(args->prog->lock)) >= 0))
	{
	  args->prog->finished = (currentSector - args->startSector);
	  args->prog->percentFinished =
	    ((args->prog->finished * 100) / args->numSectors);
	  remainingSeconds = (((rtcUptimeSeconds() - startSeconds) *
			       (doSectors / sectorsPerOp)) /
			      (args->prog->finished / sectorsPerOp));
	  if (remainingSeconds > 60)
	    sprintf(args->prog->statusMessage, "Time remaining: %d hours "
		    "%02d minutes", (remainingSeconds / 3600),
		    ((remainingSeconds % 3600) / 60));
	  else
	    sprintf(args->prog->statusMessage, "Time remaining: less than "
		    "1 minute");

	  lockRelease(&(args->prog->lock));
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
}


static int copyData(disk *srcDisk, unsigned srcSector, disk *destDisk,
		    unsigned destSector, unsigned numSectors)
{
  // Generic routine for raw disk I/O

  int status = 0;
  ioBuffer buffer;
  int readerPID = 0;
  int writerPID = 0;
  char tmpChar[160];
  progress prog;
  objectKey progressDialog = NULL;
  objectKey cancelDialog = NULL;

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

  sprintf(tmpChar, "Copying %u Mb...",
	  (numSectors / (1048576 / srcDisk->sectorSize)));

  bzero((void *) &prog, sizeof(progress));
  prog.total = numSectors;
  strcpy(prog.statusMessage, "Time remaining: ? hours ?? minutes");
  prog.canCancel = 1;

  if (graphics)
    progressDialog = windowNewProgressDialog(window, tmpChar, &prog);
  else
    {
      printf("\n%s (press 'Q' to cancel)\n", tmpChar);
      vshProgressBar(&prog);
    }

  // Set up and start our IO threads

  bzero(&readerArgs, sizeof(ioThreadArgs));
  readerArgs.theDisk = srcDisk;
  readerArgs.startSector = srcSector;
  readerArgs.numSectors = numSectors;
  readerArgs.buffer = &buffer;
  
  bzero(&writerArgs, sizeof(ioThreadArgs));
  writerArgs.theDisk = destDisk;
  writerArgs.startSector = destSector;
  writerArgs.numSectors = numSectors;
  writerArgs.buffer = &buffer;
  writerArgs.prog = &prog;

  ioThreadsTerminate = 0;
  ioThreadsFinished = 0;

  readerPID = multitaskerSpawn(&copyIoThread, "i/o reader thread", 1,
			       (void *[]) { "reader" });
  writerPID = multitaskerSpawn(&copyIoThread, "i/o writer thread", 1,
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
	prog.cancel = 1;

      if (prog.cancel)
	// This can be set above, or else by the progress dialog when the
	// user presses the cancel button
	break;

      multitaskerYield();
    }

  if (prog.cancel)
    {
      sprintf(tmpChar, "Terminating processes...");
      if (graphics)
	cancelDialog =
	  windowNewBannerDialog(progressDialog, "Cancel", tmpChar);
      else
	printf("\n%s\n", tmpChar);

      ioThreadsTerminate = 1;
      multitaskerYield();
      if (multitaskerProcessIsAlive(readerPID))
	multitaskerBlock(readerPID);
      if (multitaskerProcessIsAlive(writerPID))
	multitaskerBlock(writerPID);

      if (cancelDialog)
	windowDestroy(cancelDialog);

      status = ERR_CANCELLED;
    }
  else
    status = 0;

  // Release copy buffer data
  memoryRelease(buffer.buffer[0].data);
  memoryRelease(buffer.buffer[1].data);

  diskSync();

  if (graphics && progressDialog)
    windowProgressDialogDestroy(progressDialog);
  else
    vshProgressBarDestroy(&prog);

  return (status);
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


static int setFatGeometry(disk *theDisk, int partition)
{
  // Given a disk and a partition number, make sure the FAT disk geometry
  // fields are correct.

  int status = 0;
  slice entry;
  unsigned char *bootSector = NULL;
  fatBPB *bpb = NULL;

  // Load the partition
  status = getPartitionEntry(partition, &entry);
  if (status < 0)
    return (status);

  bootSector = malloc(theDisk->sectorSize);
  if (bootSector == NULL)
    return (status = ERR_MEMORY);
  bpb = (fatBPB *) bootSector;

  // Read the boot sector
  status = diskReadSectors(theDisk->name, entry.startLogical, 1, bootSector);
  if (status < 0)
    {
      free(bootSector);
      return (status);
    }

  // Set the values
  bpb->sectsPerTrack = theDisk->sectorsPerCylinder;
  bpb->numHeads = theDisk->heads;

  if (!strcmp(entry.fsType, "fat32"))
    bpb->fat32.biosDriveNum = (0x80 + theDisk->deviceNumber);
  else
    bpb->fat.biosDriveNum = (0x80 + theDisk->deviceNumber);

  // Write back the boot sector
  status = diskWriteSectors(theDisk->name, entry.startLogical, 1, bootSector);

  free(bootSector);
  return (status);
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
  char tmpChar[160];
  progress prog;
  objectKey progressDialog = NULL;
  objectKey cancelDialog = NULL;
  int count;

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

  sprintf(tmpChar, "Copying %u Mb...", ((lastUsedSector + 1) /
					(1048576 / srcDisk->sectorSize)));
  bzero((void *) &prog, sizeof(progress));
  prog.total = (lastUsedSector + 1);
  strcpy(prog.statusMessage, "Time remaining: ? hours ?? minutes");
  prog.canCancel = 1;

  if (graphics)
    progressDialog = windowNewProgressDialog(window, tmpChar, &prog);
  else
    {
      printf("\n%s (press 'Q' to cancel)\n", tmpChar);
      vshProgressBar(&prog);
    }

  // Set up and start our IO threads

  bzero(&readerArgs, sizeof(ioThreadArgs));
  readerArgs.theDisk = srcDisk;
  readerArgs.startSector = 0;
  readerArgs.numSectors = (lastUsedSector + 1);
  readerArgs.buffer = &buffer;
  
  bzero(&writerArgs, sizeof(ioThreadArgs));
  writerArgs.theDisk = destDisk;
  writerArgs.startSector = 0;
  writerArgs.numSectors = (lastUsedSector + 1);
  writerArgs.buffer = &buffer;
  writerArgs.prog = &prog;

  ioThreadsTerminate = 0;
  ioThreadsFinished = 0;

  readerPID = multitaskerSpawn(&copyIoThread, "i/o reader thread", 1,
			       (void *[]) { "reader" });
  writerPID = multitaskerSpawn(&copyIoThread, "i/o writer thread", 1,
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
	prog.cancel = 1;

      if (prog.cancel)
	// This can be set above, or else by the progress dialog when the
	// user presses the cancel button
	break;

      multitaskerYield();
    }

  if (prog.cancel)
    {
      sprintf(tmpChar, "Terminating processes...");
      if (graphics)
	cancelDialog =
	  windowNewBannerDialog(progressDialog, "Cancel", tmpChar);
      else
	printf("\n%s\n", tmpChar);

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

  if (prog.cancel)
    clearDiskLabel(destDisk);

  diskSync();

  if (graphics)
    {
      if (cancelDialog)
	windowDestroy(cancelDialog);
      if (progressDialog)
	windowProgressDialogDestroy(progressDialog);
    }
  else
    vshProgressBarDestroy(&prog);

  status = selectDisk(destDisk);
  if (status < 0)
    return (status);

  if (prog.cancel)
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
  status = writePartitionTable(destDisk, mainTable, 0);

  if (status >= 0)
    // Make sure the disk geometries of any FAT partitions are correct for
    // the new disk.
    for (count = 0; count < mainTable->numberEntries; count ++)
      {
	if (!strncmp(entry.fsType, "fat", 3))
	  setFatGeometry(destDisk, entry.sliceId);
      }

  makeSliceList();
  display();

  // Tell the kernel to reexamine the partition tables
  diskReadPartitions(destDisk->name);

  return (status);
}


static void copyPartition(int sliceNumber)
{
  // 'Copy' a partition to our partition 'clipboard'
  memcpy(&clipboardSlice, &(slices[sliceNumber]), sizeof(slice));
  clipboardDisk = selectedDisk;
  clipboardSliceValid = 1;
}


static int pastePartition(int sliceNumber)
{
  // 'Paste' a partition from our partition 'clipboard' to the supplied
  // empty space slice.  This is really where a partition copying operation
  // takes place.

  int status = 0;
  slice *emptySlice = NULL;
  partEntryType newType = partition_none;
  unsigned newEndCylinder = 0;
  int newSliceNumber = 0;
  char tmpChar[160];

  if (!clipboardSliceValid)
    {
      error("No partition copied to the clipboard");
      return (status = ERR_NODATA);
    }

  emptySlice = &(slices[sliceNumber]);

  if (emptySlice->typeId)
    // Not empty space
    return (status = ERR_INVALID);

  // See if we can create a slice here, and if so, what type?
  newType = canCreate(sliceNumber);
  if ((int) newType < 0)
    return (status = newType);

  // Check whether the empty slice is big enough
  if (emptySlice->sizeLogical < clipboardSlice.sizeLogical)
    {
      error("Partition %s is too big (%u sectors) to fit in the\nselected "
	    "empty space (%u sectors)", clipboardSlice.sliceName,
	    clipboardSlice.sizeLogical, emptySlice->sizeLogical);
      return (status = ERR_NOSUCHENTRY);
    }

  // Everything seems OK.  Confirm.
  sprintf(tmpChar, "Paste partition %s to selected empty space on disk %s?",
	  clipboardSlice.sliceName, selectedDisk->name);
  if (!yesOrNo(tmpChar))
    return (status = 0);

  status = copyData(clipboardDisk, clipboardSlice.startLogical,
		    selectedDisk, emptySlice->startLogical,
		    clipboardSlice.sizeLogical);
  if (status < 0)
    return (status);

  newEndCylinder =
    (emptySlice->startCylinder +
     (clipboardSlice.sizeLogical / cylinderSectors) +
     ((clipboardSlice.sizeLogical % cylinderSectors) != 0) - 1);

  newSliceNumber =
    doCreate(sliceNumber, newType, emptySlice->startCylinder, newEndCylinder);
  if (newSliceNumber < 0)
    return (status);

  // If it's a FAT filesystem, make sure the disk geometry stuff in it
  // is correct for the new disk.
  if (!strncmp(slices[newSliceNumber].fsType, "fat", 3))
    setFatGeometry(selectedDisk, slices[newSliceNumber].sliceId);

  changesPending += 1;

  return (status = 0);
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

      sprintf(firstSlice->diskName, "%s%c", selectedDisk->name,
	      ('a' + firstSlice->partition));
      sprintf(secondSlice->diskName, "%s%c", selectedDisk->name,
	      ('a' + secondSlice->partition));
#ifdef PARTLOGIC
      sprintf(firstSlice->sliceName, "%d", (firstSlice->partition + 1));
      sprintf(secondSlice->sliceName, "%d", (secondSlice->partition + 1));
#else
      strcpy(firstSlice->sliceName, firstSlice->diskName);
      strcpy(secondSlice->sliceName, secondSlice->diskName);
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
  listItemParameters orderListParams[DISK_MAX_PRIMARY_PARTITIONS];
  char lineString[SLICESTRING_LENGTH + 2];
  componentParameters params;
  objectKey orderList = NULL;
  objectKey upButton = NULL;
  objectKey downButton = NULL;
  objectKey okButton = NULL;
  objectKey cancelButton = NULL;
  int selected = 0;
  windowEvent event;
  textAttrs attrs;
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
	  strncpy(orderListParams[numOrderSlices++].text,
		  tempTable.entries[count1].string, WINDOW_MAX_LABEL_LENGTH);
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
      
      // Make a window list with all the disk choices
      fontGetDefault(&params.font);
      orderList = windowNewList(orderDialog, windowlist_textonly,
				DISK_MAX_PRIMARY_PARTITIONS, 1, 0,
				orderListParams, numOrderSlices, &params);

      // Make 'up' and 'down' buttons
      params.gridX = 2;
      params.gridHeight = 1;
      params.gridWidth = 1;
      params.font = NULL;
      upButton = windowNewButton(orderDialog, "Up", NULL, &params);

      params.gridY = 1;
      params.padTop = 5;
      downButton = windowNewButton(orderDialog, "Down", NULL, &params);

      // Make 'OK' and 'cancel' buttons
      params.gridX = 0;
      params.gridY = 2;
      params.padBottom = 5;
      params.orientationX = orient_right;
      params.flags |= WINDOW_COMPFLAG_FIXEDWIDTH;
      okButton = windowNewButton(orderDialog, "OK", NULL, &params);

      params.gridX = 1;
      params.orientationX = orient_left;
      cancelButton = windowNewButton(orderDialog, "Cancel", NULL, &params);

      // Make the window visible
      windowRemoveMinimizeButton(orderDialog);
      windowSetResizable(orderDialog, 0);
      windowSetVisible(orderDialog, 1);

      while(1)
	{
	  windowComponentGetSelected(orderList, &selected);

	  if ((windowComponentEventGet(upButton, &event) > 0) &&
	      (event.type == EVENT_MOUSE_LEFTUP) && (selected > 0))
	    {
	      // 'Up' button
	      swapEntries(orderSlices[selected], orderSlices[selected - 1]);
	      windowComponentSetSelected(orderList, (selected - 1));
	      strncpy(orderListParams[selected].text,
		      orderSlices[selected]->string,
		      WINDOW_MAX_LABEL_LENGTH);
	      strncpy(orderListParams[selected - 1].text,
		      orderSlices[selected - 1]->string,
		      WINDOW_MAX_LABEL_LENGTH);
	      windowComponentSetData(orderList, orderListParams,
				     numOrderSlices);
	    }
	  
	  if ((windowComponentEventGet(downButton, &event) > 0) &&
	      (event.type == EVENT_MOUSE_LEFTUP) &&
	      (selected < (numOrderSlices - 1)))
	    {
	      // 'Down' button
	      swapEntries(orderSlices[selected], orderSlices[selected + 1]);
	      windowComponentSetSelected(orderList, (selected + 1));
	      strncpy(orderListParams[selected].text,
		      orderSlices[selected]->string,
		      WINDOW_MAX_LABEL_LENGTH);
	      strncpy(orderListParams[selected + 1].text,
		      orderSlices[selected + 1]->string,
		      WINDOW_MAX_LABEL_LENGTH);
	      windowComponentSetData(orderList, orderListParams,
				     numOrderSlices);
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
      bzero(&attrs, sizeof(textAttrs));
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
		attrs.flags = TEXT_ATTRS_REVERSE;
	      else
		attrs.flags = 0;

	      textPrintAttrs(&attrs, " ");
	      textPrintAttrs(&attrs, orderListParams[count1].text);
	      for (count2 = strlen(orderListParams[count1].text);
		   count2 < SLICESTRING_LENGTH; count2 ++)
		textPrintAttrs(&attrs, " ");

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
		  strncpy(orderListParams[selected].text,
			  orderSlices[selected]->string,
			  WINDOW_MAX_LABEL_LENGTH);
		  strncpy(orderListParams[selected - 1].text,
			  orderSlices[selected - 1]->string,
			  WINDOW_MAX_LABEL_LENGTH);
		  selected -= 1;

		}
	      continue;

	    case '+':
	      if (selected < (numOrderSlices - 1))
		{
		  // Move down
		  swapEntries(orderSlices[selected],
			      orderSlices[selected + 1]);
		  strncpy(orderListParams[selected].text,
			  orderSlices[selected]->string,
			  WINDOW_MAX_LABEL_LENGTH);
		  strncpy(orderListParams[selected + 1].text,
			  orderSlices[selected + 1]->string,
			  WINDOW_MAX_LABEL_LENGTH);
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


static int writeSimpleMbr(void)
{
  // Put simple MBR code into the main partition table.

  int status = 0;
  fileStream mbrFile;

  if (!yesOrNo("After you write changes, the \"active\" partition will\n"
	       "always boot automatically.  Proceed?"))
    return (status = 0);

  // Read the MBR file
  bzero(&mbrFile, sizeof(fileStream));
  status = fileStreamOpen(SIMPLE_MBR_FILE, OPENMODE_READ, &mbrFile);
  if (status < 0)
    {
      error("Can't locate simple MBR file %s", SIMPLE_MBR_FILE);
      return (status);
    }

  // Read bytes 0-445 into the main table
  status = fileStreamRead(&mbrFile, 446, (char *) mainTable->sectorData);
  if (status < 0)
    {
      error("Can't read simple MBR file %s", SIMPLE_MBR_FILE);
      return (status);
    }

  changesPending += 1;

  return (status = 0);
}


static int mbrBootMenu(void)
{
  // Call the 'bootmenu' program to install a boot menu

  int status = 0;
  char command[80];

  if (changesPending)
    {
      error("This operation cannot be undone, and it is required that\n"
	    "you write your other changes to disk before continuing.");
      return (status = ERR_BUSY);
    }

  sprintf(command, "/programs/bootmenu %s", selectedDisk->name);

  status = system(command);
  if (status < 0)
    error("Error %d running bootmenu command", status);

  // Need to re-read the partition table
  return (selectDisk(selectedDisk));
}


static void restoreBackup(void)
{
  // Restore the backed-up partition table from a file

  int status = 0;
  file backupFile;
  char *fileName = NULL;
  unsigned char *buffer = NULL;

  if (!yesOrNo("Restore old partition table from backup?"))
    return;

  // Clear stack data
  bzero(&backupFile, sizeof(file));

  // Construct the file name
  fileName = malloc(MAX_PATH_NAME_LENGTH);
  if (fileName == NULL)
    return;
  sprintf(fileName, BACKUP_MBR, selectedDisk->name);

  // Read a backup copy of the partition tables
  status = fileOpen(fileName, OPENMODE_READ, &backupFile);

  free(fileName);

  if (status < 0)
    {
      error("Error opening backup partition table file");
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
  objectKey selectedItem = NULL;
  int selected = -1;
  
  if (key == window)
    {
      // Check for the window being closed by a GUI event.
      if (event->type == EVENT_WINDOW_CLOSE)
	{
	  quit(0);
	  return;
	}

      // Check for window resize
      else if (event->type == EVENT_WINDOW_RESIZE)
	{
	  // Get the canvas sizes
	  canvasWidth = windowComponentGetWidth(canvas);
	  canvasHeight = windowComponentGetHeight(canvas);
	}

      else
	// Mouse enter and leave events, for example, we don't want to redraw
	// everything for those, so return.
	return;
    }
      
  // Check for menu events

  // File menu
  else if ((key == fileMenu) && (event->type & EVENT_SELECTION))
    {
      windowComponentGetData(fileMenu, &selectedItem, 1);
      if (selectedItem)
	{
	  if (selectedItem == fileMenuContents.items[FILEMENU_WRITE].key)
	    writeChanges(1);

	  else if (selectedItem == fileMenuContents.items[FILEMENU_UNDO].key)
	    undo();

	  else if (selectedItem ==
		   fileMenuContents.items[FILEMENU_RESTOREBACKUP].key)
	    restoreBackup();

	  else if (selectedItem == fileMenuContents.items[FILEMENU_QUIT].key)
	    {
	      quit(0);
	      return;
	    }
	}
    }

  // Disk menu
  else if ((key == diskMenu) && (event->type & EVENT_SELECTION))
    {
      windowComponentGetData(diskMenu, &selectedItem, 1);
      if (selectedItem)
	{
	  if (selectedItem == diskMenuContents.items[DISKMENU_COPYDISK].key)
	    {
	      if (copyDisk() < 0)
		error("Disk copy failed.");
	    }

	  else if (selectedItem ==
		   diskMenuContents.items[DISKMENU_PARTORDER].key)
	    changePartitionOrder();

	  else if (selectedItem ==
		   diskMenuContents.items[DISKMENU_SIMPLEMBR].key)
	    writeSimpleMbr();

	  else if (selectedItem ==
		   diskMenuContents.items[DISKMENU_BOOTMENU].key)
	    mbrBootMenu();
	}
    }

  // Partition menu
  else if ((key == partMenu) && (event->type & EVENT_SELECTION))
    {
      windowComponentGetData(partMenu, &selectedItem, 1);
      if (selectedItem)
	{
	  if (selectedItem == partMenuContents.items[PARTMENU_COPY].key)
	    copyPartition(selectedSlice);

	  else if (selectedItem == partMenuContents.items[PARTMENU_PASTE].key)
	    pastePartition(selectedSlice);

	  else if (selectedItem ==
		   partMenuContents.items[PARTMENU_SETACTIVE].key)
	    setActive(slices[selectedSlice].sliceId);

	  else if (selectedItem == partMenuContents.items[PARTMENU_DELETE].key)
	    delete(slices[selectedSlice].sliceId);

	  else if (selectedItem == partMenuContents.items[PARTMENU_FORMAT].key)
	    format(&slices[selectedSlice]);

	  else if (selectedItem == partMenuContents.items[PARTMENU_DEFRAG].key)
	    defragment(&slices[selectedSlice]);

	  else if (selectedItem == partMenuContents.items[PARTMENU_RESIZE].key)
	    resize(slices[selectedSlice].sliceId);

	  else if (selectedItem == partMenuContents.items[PARTMENU_HIDE].key)
	    hide(slices[selectedSlice].sliceId);

	  else if (selectedItem == partMenuContents.items[PARTMENU_INFO].key)
	    info(selectedSlice);

	  else if (selectedItem ==
		   partMenuContents.items[PARTMENU_LISTTYPES].key)
	    listTypes();

	  else if (selectedItem == partMenuContents.items[PARTMENU_MOVE].key)
	    move(slices[selectedSlice].sliceId);

	  else if (selectedItem == partMenuContents.items[PARTMENU_CREATE].key)
	    create(selectedSlice);

	  else if (selectedItem ==
		   partMenuContents.items[PARTMENU_DELETEALL].key)
	    deleteAll();

	  else if (selectedItem ==
		   partMenuContents.items[PARTMENU_SETTYPE].key)
	    setType(slices[selectedSlice].sliceId);
	}
    }

  // Check for changes to our disk list
  else if ((key == diskList) && (event->type & EVENT_MOUSE_DOWN))
    {
      windowComponentGetSelected(diskList, &selected);
      if ((selected >= 0) && (selected != selectedDiskNumber))
	{
	  if (selectDisk(&diskInfo[selected]) < 0)
	    windowComponentSetSelected(diskList, selectedDiskNumber);
	}
    }

  // Check for clicks or cursor keys on our canvas diagram
  else if ((key == canvas) &&
	   ((event->type & EVENT_MOUSE_DOWN) ||
	    (event->type == EVENT_KEY_DOWN)))
    {
      if (event->type & EVENT_MOUSE_DOWN)
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

      else if (event->type == EVENT_KEY_DOWN)
	{
	  // Respond to cursor left or right
	  switch (event->key)
	    {
	    case 18:
	      // LEFT cursor key
	      if (selectedSlice)
		selectedSlice -= 1;
	      break;

	    case 19:
	      // RIGHT cursor key
	      if (selectedSlice < (numberSlices - 1))
		selectedSlice += 1;
	      break;

	    default:
	      break;
	    }
	}
    }
  
  // Check for changes to our slice list
  else if ((key == sliceList) && (event->type & EVENT_MOUSE_DOWN))
    {
      windowComponentGetSelected(sliceList, &selected);
      if (selected >= 0)
	selectedSlice = selected;
    }

  // Check for button clicks

  else if ((key == writeButton) && (event->type == EVENT_MOUSE_LEFTUP))
    writeChanges(1);

  else if ((key == undoButton) && (event->type == EVENT_MOUSE_LEFTUP))
    undo();

  else if ((key == setActiveButton) && (event->type == EVENT_MOUSE_LEFTUP))
    setActive(slices[selectedSlice].sliceId);

  else if ((key == deleteButton) && (event->type == EVENT_MOUSE_LEFTUP))
    delete(slices[selectedSlice].sliceId);

  else if ((key == formatButton) && (event->type == EVENT_MOUSE_LEFTUP))
    format(&slices[selectedSlice]);

  else if ((key == defragButton) && (event->type == EVENT_MOUSE_LEFTUP))
    defragment(&slices[selectedSlice]);

  else if ((key == hideButton) && (event->type == EVENT_MOUSE_LEFTUP))
    hide(slices[selectedSlice].sliceId);

  else if ((key == infoButton) && (event->type == EVENT_MOUSE_LEFTUP))
    info(selectedSlice);

  else if ((key == moveButton) && (event->type == EVENT_MOUSE_LEFTUP))
    move(slices[selectedSlice].sliceId);

  else if ((key == newButton) && (event->type == EVENT_MOUSE_LEFTUP))
    create(selectedSlice);

  else if ((key == deleteAllButton) && (event->type == EVENT_MOUSE_LEFTUP))
    deleteAll();

  else if ((key == resizeButton) && (event->type == EVENT_MOUSE_LEFTUP))
    resize(slices[selectedSlice].sliceId);

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

  // Create the top 'file' menu
  objectKey menuBar = windowNewMenuBar(window, &params);

  fileMenu = windowNewMenu(menuBar, "File", &fileMenuContents, &params);
  windowRegisterEventHandler(fileMenu, &eventHandler);

  // Create the top 'disk' menu
  diskMenu = windowNewMenu(menuBar, "Disk", &diskMenuContents, &params);
  windowRegisterEventHandler(diskMenu, &eventHandler);

  // Create the top 'partition' menu
  partMenu = windowNewMenu(menuBar, "Partition", &partMenuContents, &params);
  windowRegisterEventHandler(partMenu, &eventHandler);

  params.gridWidth = 1;
  params.gridHeight = 1;
  params.padTop = 5;
  params.padLeft = 5;
  params.padRight = 5;
  params.orientationX = orient_left;
  params.orientationY = orient_middle;
  params.flags |= WINDOW_COMPFLAG_FIXEDWIDTH;

  // Create a container for the disk icon image and the title label
  params.gridY++;
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
      params.gridX++;
      textLabel = windowNewTextLabel(container, programName, &params);
    }

  // Make a list for the disks
  params.gridX = 0;
  params.gridY++;
  params.flags &= ~WINDOW_COMPFLAG_FIXEDWIDTH;
  diskList = windowNewList(window, windowlist_textonly, numberDisks, 1, 0,
			   diskListParams, numberDisks, &params);
  windowRegisterEventHandler(diskList, &eventHandler);
  windowContextSet(diskList, diskMenu);

  // Get a canvas for drawing the visual representation
  params.gridY++;
  params.padTop = 10;
  params.flags |= (WINDOW_COMPFLAG_HASBORDER | WINDOW_COMPFLAG_CANFOCUS);
  canvas = windowNewCanvas(window, canvasWidth, canvasHeight, &params);
  windowRegisterEventHandler(canvas, &eventHandler);
  windowContextSet(canvas, partMenu);

  params.gridY++;
  params.flags &= ~(WINDOW_COMPFLAG_HASBORDER | WINDOW_COMPFLAG_CANFOCUS);
  fontGetDefault(&params.font);
  textLabel = windowNewTextLabel(window, sliceListHeader, &params);

  // Make a list for the partitions
  params.gridY++;
  params.padTop = 5;
  listItemParameters tmpListParams;
  for (count = 0; count < WINDOW_MAX_LABEL_LENGTH; count ++)
    tmpListParams.text[count] = ' ';
  tmpListParams.text[WINDOW_MAX_LABEL_LENGTH - 1] = '\0';
  sliceList = windowNewList(window, windowlist_textonly, 6, 1, 0,
			    &tmpListParams, 1, &params);
  windowRegisterEventHandler(sliceList, &eventHandler);
  windowContextSet(sliceList, partMenu);

  // Adjust the canvas width so that it matches the width of the partition
  // list.
  canvasWidth = windowComponentGetWidth(sliceList);
  windowComponentSetWidth(canvas, canvasWidth);

  // A container for the buttons
  params.gridY++;
  params.padTop = 5;
  params.padBottom = 5;
  params.orientationX = orient_center;
  container = windowNewContainer(window, "buttonContainer", &params);
  if (container != NULL)
    {
      params.gridY = 0;
      params.orientationX = orient_left;
      params.padBottom = 0;
      params.font = NULL;
      newButton = windowNewButton(container, "Create", NULL, &params);
      windowRegisterEventHandler(newButton, &eventHandler);

      params.gridX++;
      setActiveButton =
	windowNewButton(container, "Set active", NULL, &params);
      windowRegisterEventHandler(setActiveButton, &eventHandler);

      params.gridX++;
      moveButton = windowNewButton(container, "Move", NULL, &params);
      windowRegisterEventHandler(moveButton, &eventHandler);

      params.gridX++;
      defragButton = windowNewButton(container, "Defragment", NULL, &params);
      windowRegisterEventHandler(defragButton, &eventHandler);

      params.gridX++;
      formatButton = windowNewButton(container, "Format", NULL, &params);
      windowRegisterEventHandler(formatButton, &eventHandler);

      params.gridX++;
      deleteAllButton =
	windowNewButton(container, "Delete all", NULL, &params);
      windowRegisterEventHandler(deleteAllButton, &eventHandler);

      params.gridX = 0;
      params.gridY++;
      params.padTop = 0;
      deleteButton = windowNewButton(container, "Delete", NULL, &params);
      windowRegisterEventHandler(deleteButton, &eventHandler);

      params.gridX++;
      hideButton = windowNewButton(container, "Hide/unhide", NULL, &params);
      windowRegisterEventHandler(hideButton, &eventHandler);

      params.gridX++;
      infoButton = windowNewButton(container, "Info", NULL, &params);
      windowRegisterEventHandler(infoButton, &eventHandler);

      params.gridX++;
      resizeButton = windowNewButton(container, "Resize", NULL, &params);
      windowRegisterEventHandler(resizeButton, &eventHandler);

      params.gridX++;
      undoButton = windowNewButton(container, "Undo", NULL, &params);
      windowRegisterEventHandler(undoButton, &eventHandler);

      params.gridX++;
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
  int isDefrag = 0;
  int isHide = 0;
  int isMove = 0;
  int topRow, bottomRow;

  // This is the main menu bit
  while (1)
    {
      // Print out the partitions
      display();

      isPartition = 0;
      isDefrag = 0;
      isHide = 0;
      isMove = 0;

      if (slices[selectedSlice].typeId)
	{
	  isPartition = 1;

	  if (slices[selectedSlice].opFlags & FS_OP_DEFRAG)
	    isDefrag = 1;

	  if (PARTITION_TYPEID_IS_HIDEABLE(slices[selectedSlice].typeId) ||
	      PARTITION_TYPEID_IS_HIDDEN(slices[selectedSlice].typeId))
	    isHide = 1;

	  if (!ISLOGICAL(&slices[selectedSlice]))
	    isMove = 1;
	}

      // Print out the menu choices.  First column.
      printf("\n");
      topRow = textGetRow();
      printf("%s%s%s%s%s%s%s%s%s%s%s%s%s",
	     (isPartition? "[A] Set active\n" : ""),
	     (numberDataPartitions? "[B] Partition order\n" : ""),
	     "[C] Copy disk\n",
	     (isPartition? "[D] Delete\n" : ""),
	     (isPartition? "[E] Copy partition\n" : ""),
	     (isPartition? "[F] Format\n" : ""),
	     (isDefrag? "[G] Defragment\n" : ""),
	     (isHide? "[H] Hide/Unhide\n" : ""),
	     "[I] Info\n",
	     "[L] List types\n",
	     (isMove? "[M] Move\n" : ""),
	     (!isPartition? "[N] Create new\n" : ""),
	     (numberPartitions? "[O] Delete all\n" : ""));
      bottomRow = textGetRow();

      // Second column
      textSetRow(topRow);
      #define COL 24
      textSetColumn(COL);
      printf((!isPartition && clipboardSliceValid)?
	     "[P] Paste partition\n" : "");
      textSetColumn(COL);
      printf("[Q] Quit\n");
      textSetColumn(COL);
      printf(backupAvailable? "[R] Restore backup\n" : "");
      textSetColumn(COL);
      printf("[S] Select disk\n");
      textSetColumn(COL);
      printf(isPartition? "[T] Set type\n" : "");
      textSetColumn(COL);
      printf(changesPending? "[U] Undo\n" : "");
      textSetColumn(COL);
      printf(changesPending? "[W] Write changes\n" : "");
      textSetColumn(COL);
      printf("[X] Write basic MBR\n");
      textSetColumn(COL);
      printf("[Y] MBR boot menu\n");
      textSetColumn(COL);
      printf(isPartition? "[Z] Resize\n" : "");
      if (bottomRow > textGetRow())
	textSetRow(bottomRow);
      textSetColumn(0);

      if (changesPending)
	printf("  -== %d changes pending ==-\n", changesPending);
      printf("-> ");

      // Construct the string of allowable options, corresponding to what is
      // shown above.
      sprintf(optionString, "%s%sCc%s%s%s%s%sIiLl%s%s%s%sQq%sSs%s%s%sXxYyZz",
	      (isPartition? "Aa" : ""),
	      (numberDataPartitions? "Bb" : ""),
	      (isPartition? "Dd" : ""),
	      (isPartition? "Ee" : ""),
	      (isPartition? "Ff" : ""),
	      (isDefrag? "Gg" : ""),
	      (isPartition? "Hh" : ""),
	      (isMove? "Mm" : ""),
	      (!isPartition? "Nn" : ""),
	      (numberPartitions? "Oo" : ""),
	      ((!isPartition && clipboardSliceValid)? "Pp" : ""),
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

	case 'e':
	case 'E':
	  copyPartition(selectedSlice);
	  continue;

	case 'f':
	case 'F':
	  format(&slices[selectedSlice]);
	  continue;

	case 'g':
	case 'G':
	  defragment(&slices[selectedSlice]);
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

	case 'p':
	case 'P':
	  pastePartition(selectedSlice);
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
	  
	case 'x':
	case 'X':
	  writeSimpleMbr();
	  continue;

	case 'y':
	case 'Y':
	  mbrBootMenu();
	  continue;

	case 'z':
	case 'Z':
	  resize(slices[selectedSlice].sliceId);
	  continue;

	default:
	  continue;
	}
    }
}


static void freeMemory(void)
{
  // Free any malloc'ed global memory

  if (screen.data)
    memoryRelease(screen.data);

  if (diskInfo)
    free(diskInfo);

  if (diskListParams)
    free(diskListParams);

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
  strncpy((sliceListHeader + count), "Filesystem", 10);
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
	error(PERM);
      else
	printf("\n%s\n\n", PERM);
      return (errno = ERR_PERMISSION);
    }

  // Get memory for various things

  diskInfo = malloc(DISK_MAXDEVICES * sizeof(disk));
  mainTable = malloc(sizeof(partitionTable));
  slices = malloc(MAX_SLICES * sizeof(slice));
  if ((diskInfo == NULL) || (mainTable == NULL) ||
      (slices == NULL))
    {
      freeMemory();
      return (status = errno = ERR_MEMORY);
    }

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
      textScreenSave(&screen);
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
	      textScreenRestore(&screen);
	      freeMemory();
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
      textScreenRestore(&screen);
    }

  freeMemory();
  return (status);
}
