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
#include "msdos.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/api.h>
#include <sys/fat.h>
#include <sys/ntfs.h>
#include <sys/vsh.h>

#define PERM             "You must be a privileged user to use this "	\
  "command.\n(Try logging in as user \"admin\")"
#define PARTTYPES        "Supported Partition Types"
#define STARTCYL_MESSAGE "Enter starting cylinder (%u-%u)"
#define ENDCYL_MESSAGE   "Enter ending cylinder (%u-%u)\n"	\
  "or size in megabytes with 'm' (1m-%um),\n"			\
  "or size in cylinders with 'c' (1c-%uc)"

static const char *programName = NULL;
static int processId = 0;
static int readOnly = 1;
static diskLabel *msdosLabel = NULL;
static int numberDisks = 0;
static disk *disks = NULL;
partitionTable *table = NULL;
static textScreen screen;
static char *tmpBackupName = NULL;
static char sliceListHeader[SLICESTRING_LENGTH + 1];
static listItemParameters *diskListParams = NULL;
static ioThreadArgs readerArgs;
static ioThreadArgs writerArgs;
static int ioThreadsTerminate = 0;
static int ioThreadsFinished = 0;
static int checkTableAsk = 1;
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


static void quit(int status, int force)
{
  // Shut everything down

  if (!force && table->changesPending &&
      !yesOrNo("Quit without writing changes?"))
    return;

  if (graphics)
    {
      windowGuiStop();
      if (window)
	windowDestroy(window);
    }
  else
    {
      if (screen.data)
	textScreenRestore(&screen);
    }

  if (tmpBackupName)
    {
      fileDelete(tmpBackupName);
      tmpBackupName = NULL;
    }

  // Free any malloc'ed global memory

  if (screen.data)
    memoryRelease(screen.data);

  if (disks)
    free(disks);

  if (diskListParams)
    free(diskListParams);

  if (table)
    free(table);

  exit(errno = status);
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


void pause(void)
{
  printf("\nPress any key to continue. ");
  getchar();
  printf("\n");
}


void error(const char *format, ...)
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


void warning(const char *format, ...)
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


static inline unsigned cylsToMb(disk *theDisk, unsigned cylinders)
{
  unsigned tmpDiskSize =
    ((cylinders * CYLSECTS(theDisk)) / (1048576 / theDisk->sectorSize));
  return (max(1, tmpDiskSize));
}


static inline unsigned mbToCyls(disk *theDisk, unsigned megabytes)
{
  unsigned sectors = ((1048576 / theDisk->sectorSize) * megabytes);
  unsigned cylinders = (sectors / CYLSECTS(theDisk));
  if (sectors % CYLSECTS(theDisk))
    cylinders += 1;

  return (cylinders);
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
	memcpy(&disks[numberDisks], &tmpDiskInfo[count], sizeof(disk));

	snprintf(diskListParams[numberDisks].text, WINDOW_MAX_LABEL_LENGTH,
		 "Disk %d: [%s] %u Mb, %u cyls, %u heads, %u secs/cyl, "
		 "%u bytes/sec", numberDisks, disks[numberDisks].name,
		 (disks[numberDisks].numSectors /
		  (1048576 / disks[numberDisks].sectorSize)),
		 disks[numberDisks].cylinders, disks[numberDisks].heads,
		 disks[numberDisks].sectorsPerCylinder,
		 disks[numberDisks].sectorSize);

	numberDisks += 1;
      }

  free(tmpDiskInfo);

  if (numberDisks <= 0)
    return (status = ERR_NOSUCHENTRY);
  else
    return (status = 0);
}


static int checkTable(partitionTable *t, int fix)
{
  // Does a series of correctness checks on the table, and outputs warnings
  // for any problems it finds

  slice *checkSlice = NULL;
  int errors = 0;
  unsigned endLogical = 0;
  unsigned expectCylinder = 0;
  unsigned expectHead = 0;
  unsigned expectSector = 0;
  char *output = NULL;
  int count;

  output = malloc(MAXSTRINGLENGTH);
  if (output == NULL)
    return (ERR_MEMORY);

  // For each partition entry, check that its starting/ending
  // cylinder/head/sector values match with its starting logical value
  // and logical size.  In general we expect the logical values are correct.
  for (count = 0; count < t->numSlices; count ++)
    {
      checkSlice = &t->slices[count];

      if (!checkSlice->raw.tag)
	continue;

      endLogical =
	((checkSlice->raw.startLogical + checkSlice->raw.sizeLogical) - 1);

      expectCylinder = (checkSlice->raw.startLogical / CYLSECTS(t->disk));
      if (expectCylinder != checkSlice->raw.geom.startCylinder)
	{
	  sprintf((output + strlen(output)), "Partition %s starting cylinder "
		  "is %u, should be %u\n", checkSlice->showSliceName,
		  checkSlice->raw.geom.startCylinder, expectCylinder);
	  if (fix)
	    {
	      checkSlice->raw.geom.startCylinder = expectCylinder;
	      t->changesPending += 1;
	    }
	  else
	    errors += 1;
	}

      expectCylinder = (endLogical / CYLSECTS(t->disk));
      if (expectCylinder != checkSlice->raw.geom.endCylinder)
	{
	  sprintf((output + strlen(output)), "Partition %s ending cylinder "
		  "is %u, should be %u\n", checkSlice->showSliceName,
		  checkSlice->raw.geom.endCylinder, expectCylinder);
	  if (fix)
	    {
	      checkSlice->raw.geom.endCylinder = expectCylinder;
	      t->changesPending += 1;
	    }
	  else
	    errors += 1;
	}

      expectHead = ((checkSlice->raw.startLogical % CYLSECTS(t->disk)) /
		    t->disk->sectorsPerCylinder);
      if (expectHead != checkSlice->raw.geom.startHead)
	{
	  sprintf((output + strlen(output)), "Partition %s starting head is "
		  "%u, should be %u\n", checkSlice->showSliceName,
		  checkSlice->raw.geom.startHead, expectHead);
	  if (fix)
	    {
	      checkSlice->raw.geom.startHead = expectHead;
	      t->changesPending += 1;
	    }
	  else
	    errors += 1;
	}

      expectHead =
	((endLogical % CYLSECTS(t->disk)) / t->disk->sectorsPerCylinder);
      if (expectHead != checkSlice->raw.geom.endHead)
	{
	  sprintf((output + strlen(output)), "Partition %s ending head is %u, "
		  "should be %u\n", checkSlice->showSliceName,
		  checkSlice->raw.geom.endHead, expectHead);
	  if (fix)
	    {
	      checkSlice->raw.geom.endHead = expectHead;
	      t->changesPending += 1;
	    }
	  else
	    errors += 1;
	}

      expectSector = (((checkSlice->raw.startLogical % CYLSECTS(t->disk)) %
		       t->disk->sectorsPerCylinder) + 1);
      if (expectSector != checkSlice->raw.geom.startSector)
	{
	  sprintf((output + strlen(output)), "Partition %s starting CHS "
		  "sector is %u, should be %u\n", checkSlice->showSliceName,
		  checkSlice->raw.geom.startSector, expectSector);
	  if (fix)
	    {
	      checkSlice->raw.geom.startSector = expectSector;
	      t->changesPending += 1;
	    }
	  else
	    errors += 1;
	}

      expectSector =
	(((endLogical % CYLSECTS(t->disk)) % t->disk->sectorsPerCylinder) + 1);
      if (expectSector != checkSlice->raw.geom.endSector)
	{
	  sprintf((output + strlen(output)), "Partition %s ending CHS sector "
		  "is %u, should be %u\n", checkSlice->showSliceName,
		  checkSlice->raw.geom.endSector, expectSector);
	  if (fix)
	    {
	      checkSlice->raw.geom.endSector = expectSector;
	      t->changesPending += 1;
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
	    {
	      free(output);
	      return (checkTable(t, 1));
	    }
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


static int readPartitionTable(disk *theDisk, partitionTable *t)
{
  // Read the partition table from the physical disk

  int status = 0;
  char *fileName = NULL;
  static char *tmpBackupFileName = NULL;
  file backupFile;
  fileStream tmpBackupFile;
  int count;

  // Clear stack data
  bzero(&backupFile, sizeof(file));
  bzero(&tmpBackupFile, sizeof(fileStream));

  // Clear any existing partition table data
  bzero(t, sizeof(partitionTable));

  t->disk = theDisk;
  for (count = 0; count < numberDisks; count ++)
    if (&disks[count] == t->disk)
      {
	t->diskNumber = count;
	break;
      }

  // Detect the disk label
  if (msdosLabel->detect(t->disk) == 1)
    t->label = msdosLabel;

  if (t->label)
    {
      status = t->label->read(t->disk, t->rawSlices, &t->numRawSlices);
      if (status < 0)
	warning("Error %d reading partition table, data may be incorrect.\n"
		"Proceed with caution.", status);
    }
  else
    {
      warning("Unknown disk label.  Writing changes will create an MS-DOS "
	      "label.");
      t->label = msdosLabel;
    }

  // Any backup partition table saved?  Construct the file name
  fileName = malloc(MAX_PATH_NAME_LENGTH);
  if (fileName == NULL)
    return (status = ERR_MEMORY);
  sprintf(fileName, BACKUP_MBR, t->disk->name);
  if (!fileFind(fileName, &backupFile))
    t->backupAvailable = 1;
  else
    t->backupAvailable = 0;
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

      status = fileStreamWrite(&tmpBackupFile, sizeof(int),
			       (char *) &t->numRawSlices);
      if (status < 0)
	warning("Error writing backup partition table file");
      status = fileStreamWrite(&tmpBackupFile,
			       (t->numRawSlices * sizeof(rawSlice)),
			       (char *) t->rawSlices);
      if (status < 0)
	warning("Error writing backup partition table file");
      
      fileStreamClose(&tmpBackupFile);
      tmpBackupName = tmpBackupFileName;
    }

  return (status = 0);
}


static int writePartitionTable(partitionTable *t)
{
  // Write the partition table to the physical disk

  int status = 0;
  char *fileName = NULL;
  int count1, count2;

  bzero(t->rawSlices, (DISK_MAX_PARTITIONS * sizeof(rawSlice)));
  t->numRawSlices = 0;

  // Copy the 'raw' data from the data slices into the raw slice list.
  for (count1 = 0; count1 < DISK_MAX_PARTITIONS; count1 ++)
    for (count2 = 0; count2 < t->numSlices; count2 ++)
      if (t->slices[count2].raw.tag &&
	  (t->slices[count2].raw.order == count1))
	{
	  bcopy(&t->slices[count2].raw, &t->rawSlices[count1],
		sizeof(rawSlice));
	  t->numRawSlices += 1;
	  break;
	}

  // Do a check on the table
  status = checkTable(t, 0);
  if ((status < 0) && !yesOrNo("Partition table consistency check failed.\n"
			       "Write anyway?"))
    return (status);

  status = t->label->write(t->disk, t->rawSlices);
  if (status < 0)
    return (status);

  // Make the backup file permanent
  if (tmpBackupName != NULL)
    {
      fileName = malloc(MAX_PATH_NAME_LENGTH);
      if (fileName == NULL)
	return (status = ERR_MEMORY);

      // Construct the backup file name
      snprintf(fileName, MAX_PATH_NAME_LENGTH, BACKUP_MBR, t->disk->name);
  
      // Copy the temporary backup file to the backup
      fileMove(tmpBackupName, fileName);

      free(fileName);

      tmpBackupName = NULL;
	  
      // We now have a proper backup
      t->backupAvailable = 1;
    }

  diskSync();
  t->changesPending = 0;

  return (status = 0);
}


static void insertSliceAt(partitionTable *t, int sliceNumber)
{
  // Just moves part of the slice list to accommodate an insertion.
  
  int count;

  for (count = t->numSlices; count > sliceNumber; count --)
    memcpy(&t->slices[count], &t->slices[count - 1], sizeof(slice));

  table->numSlices += 1;
}


static void removeSliceAt(partitionTable *t, int sliceNumber)
{
  // Just moves part of the slice list to accommodate a removal.
  
  int count;

  for (count = (sliceNumber + 1); count < t->numSlices; count ++)
    memcpy(&t->slices[count - 1], &t->slices[count], sizeof(slice));

  table->numSlices -= 1;
}


static void makeEmptySlice(partitionTable *t, int sliceNumber,
			   unsigned startCylinder, unsigned endCylinder)
{
  // Given a slice entry and a geometry, make a slice for it.

  slice *emptySlice = &t->slices[sliceNumber];

  bzero(emptySlice, sizeof(slice));

  emptySlice->raw.startLogical = (startCylinder * CYLSECTS(t->disk));
  emptySlice->raw.geom.startCylinder = startCylinder;
  emptySlice->raw.geom.startHead =
    ((emptySlice->raw.startLogical % CYLSECTS(t->disk)) /
     t->disk->sectorsPerCylinder);
  emptySlice->raw.geom.startSector =
    (((emptySlice->raw.startLogical % CYLSECTS(t->disk)) %
      t->disk->sectorsPerCylinder) + 1);

  emptySlice->raw.sizeLogical =
    (((endCylinder - startCylinder) + 1) * CYLSECTS(t->disk));

  unsigned endLogical =
    (emptySlice->raw.startLogical + (emptySlice->raw.sizeLogical - 1));

  emptySlice->raw.geom.endCylinder = endCylinder;
  emptySlice->raw.geom.endHead =
    ((endLogical % CYLSECTS(t->disk)) / t->disk->sectorsPerCylinder);
  emptySlice->raw.geom.endSector =
    (((endLogical % CYLSECTS(t->disk)) % t->disk->sectorsPerCylinder) + 1);
}


static void updateEmptySlices(partitionTable *t)
{
  // Make all the empty slices reflect the actual empty spaces on the disk.

  int count;

  // First remove any existing empty slices
  for (count = 0; count < t->numSlices; count ++)
    if (!t->slices[count].raw.tag)
      {
	removeSliceAt(t, count);
	count -= 1;
      }

  // Now loop through the real slices and insert empty slices where appropriate
  for (count = 0; count < t->numSlices; count ++)
    {
      // Is there empty space between this slice and the previous slice?
      if ((!count && (t->slices[count].raw.geom.startCylinder > 0)) ||
	  (count && (t->slices[count].raw.geom.startCylinder >
		     (t->slices[count - 1].raw.geom.endCylinder + 1))))
	{
	  insertSliceAt(t, count);
	  makeEmptySlice(t, count, (count? (t->slices[count - 1]
					    .raw.geom.endCylinder + 1) : 0),
			 (t->slices[count].raw.geom.startCylinder - 1));
	  count += 1;
	}
    }

  // Is there empty space at the end of the disk?
  if ((t->numSlices == 0) ||
      (t->slices[t->numSlices - 1].raw.geom.endCylinder <
       (t->disk->cylinders - 1)))
    {
      makeEmptySlice(t, t->numSlices,
		     (t->numSlices? (t->slices[t->numSlices - 1]
				     .raw.geom.endCylinder + 1) : 0),
		     (t->disk->cylinders - 1));
      t->numSlices += 1;
    }
}


static void getFsInfo(partitionTable *t, int sliceNumber)
{
  slice *slc = &t->slices[sliceNumber];
  disk tmpDisk;

  strcpy(slc->fsType, "unknown");

  if (!slc->diskName[0])
    return;

  if (diskGet(slc->diskName, &tmpDisk) < 0)
    return;

  slc->opFlags = tmpDisk.opFlags;

  if (strcmp(tmpDisk.fsType, "unknown"))
    strncpy(slc->fsType, tmpDisk.fsType, FSTYPE_MAX_NAMELENGTH);

  return;
}


static void makeSliceString(disk *theDisk, slice *slc)
{
  int position = 0;
  partitionType partType;

  memset(slc->string, ' ', MAX_DESCSTRING_LENGTH);
  slc->string[MAX_DESCSTRING_LENGTH - 1] = '\0';

  if (slc->raw.tag)
    {
      // Slice/disk name
      strcpy(slc->string, slc->showSliceName);
      slc->string[strlen(slc->string)] = ' ';
      position += SLICESTRING_DISKFIELD_WIDTH;

      // Label
      diskGetPartType(slc->raw.tag, &partType);
      sprintf((slc->string + position), "%s", partType.description);
      slc->string[strlen(slc->string)] = ' ';
      position += SLICESTRING_LABELFIELD_WIDTH;

      // Filesystem type
      sprintf((slc->string + position), "%s", slc->fsType);
    }
  else
    {
      position += SLICESTRING_DISKFIELD_WIDTH;
      strcpy((slc->string + position), "Empty space");
      position += SLICESTRING_LABELFIELD_WIDTH;
    }

  slc->string[strlen(slc->string)] = ' ';
  position += SLICESTRING_FSTYPEFIELD_WIDTH;

  sprintf((slc->string + position), "%u-%u", slc->raw.geom.startCylinder,
	  slc->raw.geom.endCylinder);
  slc->string[strlen(slc->string)] = ' ';
  position += SLICESTRING_CYLSFIELD_WIDTH;

  sprintf((slc->string + position), "%u",
	  cylsToMb(theDisk, ((slc->raw.geom.endCylinder -
			      slc->raw.geom.startCylinder) + 1)));
  position += SLICESTRING_SIZEFIELD_WIDTH;

  if (slc->raw.tag)
    {
      slc->string[strlen(slc->string)] = ' ';
      if (!ISLOGICAL(slc))
	sprintf((slc->string + position), "primary");
      else
	sprintf((slc->string + position), "logical");

      if (slc->raw.flags & SLICEFLAG_BOOTABLE)
	strcat(slc->string, "/active");
      else
	strcat(slc->string, "       ");
    }
}


static void updateSliceList(partitionTable *t)
{
  int count;

  // Update the empty slices
  updateEmptySlices(t);

  // Update the slice strings
  for (count = 0; count < t->numSlices; count ++)
    {
      if (t->slices[count].raw.tag)
	{
#ifdef PARTLOGIC
	  snprintf(t->slices[count].showSliceName, 6, "%d",
		   (t->slices[count].raw.order + 1));
#else
	  snprintf(t->slices[count].showSliceName, 6, "%s%c",
		   t->disk->name, ('a' + t->slices[count].raw.order));
#endif
	  getFsInfo(t, count);
	}

      makeSliceString(t->disk, &t->slices[count]);
    }
}


static void makeSliceList(partitionTable *t)
{
  // This function populates the list of slices using the 'raw' slices list
  // in the partitionTable structure.

  rawSlice *raw = NULL;
  int firstPartition = -1;
  unsigned firstSector = -1;
  int count1, count2;

  bzero(t->slices, (MAX_SLICES * sizeof(slice)));
  t->numSlices = 0;
  
  // Loop through all the raw partitions and put them in our list
  for (count1 = 0; count1 < t->numRawSlices; count1 ++)
    {
      firstPartition = -1;
      firstSector = 0xFFFFFFFF;

      for (count2 = 0; count2 < t->numRawSlices; count2 ++)
	{
	  raw = &t->rawSlices[count2];

	  // If we have already processed this one, continue
	  if (t->numSlices && (raw->startLogical <=
			       t->slices[t->numSlices - 1].raw.startLogical))
	    continue;

	  if (raw->startLogical < firstSector)
	    {
	      firstSector = raw->startLogical;
	      firstPartition = count2;
	    }
	}

      if (firstPartition < 0)
	break;

      raw = &t->rawSlices[firstPartition];

      // Now add a slice for the current partition
      memcpy(&t->slices[t->numSlices].raw, raw, sizeof(rawSlice));

      snprintf(t->slices[t->numSlices].diskName, 6, "%s%c",
	       t->disk->name, ('a' + t->slices[t->numSlices].raw.order));

      t->numSlices += 1;
    }
  
  updateSliceList(t);
}


static int selectDisk(disk *theDisk)
{
  int status = 0;
  char tmpChar[80];

  if (table->changesPending)
    {
      sprintf(tmpChar, "Discard changes to disk %s?", table->disk->name);
      if (!yesOrNo(tmpChar))
	{
	  if (graphics)
	    // Re-select the old disk in the list
	    windowComponentSetSelected(diskList, table->diskNumber);
	  return (status = 0);
	}
  
      table->changesPending = 0;
    }

  status = readPartitionTable(theDisk, table);
  if (status < 0)
    return (status);
	
  if (graphics)
    windowComponentSetSelected(diskList, table->diskNumber);

  // Make the slice list
  makeSliceList(table);

  table->selectedSlice = 0;
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
			 diskStrings, numberDisks, table->diskNumber);
  if (status < 0)
    return (status);

  status = selectDisk(&disks[status]);
  if (table->disk == NULL)
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
    // These standard shades can fill out the rest.
    COLOR_BLUE,
    COLOR_GREEN,
    COLOR_CYAN,
    COLOR_RED,
    COLOR_MAGENTA,
    COLOR_BROWN,
    COLOR_LIGHTBLUE,
    COLOR_LIGHTGREEN,
    COLOR_LIGHTCYAN,
    COLOR_LIGHTRED,
    // This one is for extended partitions
    { 255, 196, 178 }
  };
  color *extendedColor = NULL;
  int count1, count2;

  // Clear our drawing parameters
  bzero(&params, sizeof(windowDrawParameters));
  
  // Some basic drawing values for slice rectangles
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
  for (count1 = 0; count1 < table->numSlices; count1 ++)
    table->slices[count1].pixelWidth =
      ((((table->slices[count1].raw.geom.endCylinder -
	  table->slices[count1].raw.geom.startCylinder) + 1) *
	canvasWidth) / table->disk->cylinders);

  // Now, we want to make sure each slice has a width of at least MIN_WIDTH,
  // so that it is visible.  If we need to, we steal the pixels from any
  // adjacent slices.
  #define MIN_WIDTH 15
  for (count1 = 0; count1 < table->numSlices; count1 ++)
    if (table->slices[count1].pixelWidth < MIN_WIDTH)
      {
	needPixels = (MIN_WIDTH - table->slices[count1].pixelWidth);

	while (needPixels)
	  for (count2 = 0; count2 < table->numSlices; count2 ++)
	    if ((count2 != count1) &&
		(table->slices[count2].pixelWidth > MIN_WIDTH))
	      {
		table->slices[count1].pixelWidth += 1;
		table->slices[count2].pixelWidth -= 1;
		needPixels -= 1;
		if (!needPixels)
		  break;
	      }
      }

  for (count1 = 0; count1 < table->numSlices; count1 ++)
    {
      table->slices[count1].pixelX = xCoord;

      params.mode = draw_normal;
      params.xCoord1 = table->slices[count1].pixelX;
      params.yCoord1 = 0;
      params.width = table->slices[count1].pixelWidth;
      params.height = canvasHeight;
      params.fill = 1;

      if (table->slices[count1].raw.tag)
	{
	  if (ISLOGICAL(&table->slices[count1]))
	    extendedColor = &colors[DISK_MAX_PARTITIONS];
	  else
	    extendedColor = NULL;
	}
      
      if (extendedColor != NULL)
	{
	  if ((table->slices[count1].raw.tag) ||
	      ((count1 < (table->numSlices - 1)) &&
	       ISLOGICAL(&table->slices[count1 + 1])))
	    {
	      memcpy(&params.foreground, extendedColor, sizeof(color));
	      windowComponentSetData(canvas, &params, 1);
	    }
	}

      // If it is a used slice, we draw a filled rectangle on the canvas to
      // represent the slice.
      if (table->slices[count1].raw.tag)
	{
	  table->slices[count1].color =
	    &colors[table->slices[count1].raw.order];
	  memcpy(&params.foreground, table->slices[count1].color,
		 sizeof(color));
	  if (ISLOGICAL(&table->slices[count1]))
	    {
	      params.xCoord1 += 3;
	      params.yCoord1 += 3;
	      params.width -= 6;
	      params.height -= 6;
	    }
	  windowComponentSetData(canvas, &params, 1);
	}

      // If this is the selected slice, draw a border inside it
      if (count1 == table->selectedSlice)
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

      xCoord += table->slices[count1].pixelWidth;
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
  textAttrs attrs;
  int count;

  if (graphics)
    {
      // Re-populate our slice list component
      sliceListParams = malloc(table->numSlices * sizeof(listItemParameters));
      if (sliceListParams == NULL)
	return;
      for (count = 0; count < table->numSlices; count ++)
	strncpy(sliceListParams[count].text, table->slices[count].string,
		WINDOW_MAX_LABEL_LENGTH);
      windowComponentSetSelected(sliceList, 0);
      windowComponentSetData(sliceList, sliceListParams, table->numSlices);
      free(sliceListParams);
      windowComponentSetSelected(sliceList, table->selectedSlice);

      drawDiagram();

      // Depending on which slice type is selected (i.e. partition vs. empty
      // space) we enable/disable button choices
      if (table->slices[table->selectedSlice].raw.tag)
	{
	  // It's a partition

	  if (table->slices[table->selectedSlice].opFlags & FS_OP_DEFRAG)
	    isDefrag = 1;

	  if (PARTITION_TYPEID_IS_HIDEABLE(table->slices[table->selectedSlice]
					   .raw.tag) ||
	      PARTITION_TYPEID_IS_HIDDEN(table->slices[table->selectedSlice]
					 .raw.tag))
	    isHide = 1;

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
	  windowComponentSetEnabled(moveButton, 1);
	  windowComponentSetEnabled(partMenuContents.items[PARTMENU_MOVE]
				    .key, 1);
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
      windowComponentSetEnabled(deleteAllButton, table->numRawSlices);
      windowComponentSetEnabled(partMenuContents.items[PARTMENU_DELETEALL]
				.key, table->numRawSlices);
      windowComponentSetEnabled(fileMenuContents.items[FILEMENU_RESTOREBACKUP]
				.key, table->backupAvailable);
      windowComponentSetEnabled(undoButton, table->changesPending);
      windowComponentSetEnabled(fileMenuContents.items[FILEMENU_UNDO]
				.key, table->changesPending);
      windowComponentSetEnabled(writeButton, table->changesPending);
      windowComponentSetEnabled(fileMenuContents.items[FILEMENU_WRITE]
				.key, table->changesPending);
    }
  else
    {
      printBanner();
      bzero(&attrs, sizeof(textAttrs));
      bzero(lineString, (SLICESTRING_LENGTH + 2));
      for (count = 0; count <= SLICESTRING_LENGTH; count ++)
	lineString[count] = 196;

      printf("\n%s\n\n  %s\n %s\n", diskListParams[table->diskNumber].text,
	     sliceListHeader, lineString);

      // Print info about the slices
      for (slc = 0; slc < table->numSlices; slc ++)
	{
	  printf(" ");
	  
	  if (slc == table->selectedSlice)
	    attrs.flags = TEXT_ATTRS_REVERSE;
	  else
	    attrs.flags = 0;

	  textPrintAttrs(&attrs, " ");
	  textPrintAttrs(&attrs, table->slices[slc].string);
	  for (count = strlen(table->slices[slc].string);
	       count < SLICESTRING_LENGTH; count ++)
	    textPrintAttrs(&attrs, " ");

	  printf("\n");
	}

      printf(" %s\n", lineString);
    }
}


static void setActive(int sliceNumber)
{
  // Toggle the 'bootable' flag of the supplied slice number, and if necessary
  // clear the flag of any existing bootable slice.

  int count;

  // Loop through the slices
  for (count = 0; count < table->numSlices; count ++)
    if (table->slices[count].raw.tag)
      {
	if (count == sliceNumber)
	  {
	    if (table->slices[count].raw.flags & SLICEFLAG_BOOTABLE)
	      // Clear the bootable flag
	      table->slices[count].raw.flags &= ~SLICEFLAG_BOOTABLE;
	    else
	      // Set the bootable flag
	      table->slices[count].raw.flags |= SLICEFLAG_BOOTABLE;
	  }
	else
	  table->slices[count].raw.flags &= ~SLICEFLAG_BOOTABLE;
      }

  table->changesPending += 1;

  // Update the slice list
  updateSliceList(table);
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
      params.flags |=
	(WINDOW_COMPFLAG_FIXEDWIDTH | WINDOW_COMPFLAG_FIXEDHEIGHT);
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


static int setType(int sliceNumber)
{
  int status = 0;
  int numberTypes = 0;
  listItemParameters *typeListParams = NULL;
  char code[8];
  int newTag = 0;
  partitionType *types;
  int count;

  types = diskGetPartTypes();

  if (graphics)
    {
      numberTypes = getTypeListParams(&typeListParams);
      if (numberTypes < 0)
	{
	  status = numberTypes;
	  goto out;
	}

      newTag = typeListDialog(typeListParams, numberTypes, 1);

      free(typeListParams);

      if (newTag < 0)
	{
	  status = newTag;
	  goto out;
	}

      newTag = types[newTag].code;
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
	newTag = xtoi(code);

	// Is it a supported type?
	for (count = 0; types[count].code != 0; count ++)
	  if (types[count].code == newTag)
	    break;

	if (types[count].code == 0)
	  {
	    error("Unsupported partition type %x", newTag);
	    status = ERR_INVALID;
	    goto out;
	  }

	break;
      }
  }

  // Change the value
  table->slices[sliceNumber].raw.tag = newTag;
  table->changesPending += 1;

  // Update the slice list
  updateSliceList(table);

  status = 0;

 out:
  memoryRelease(types);
  return (status);
}


static void doDelete(int sliceNumber)
{
  int order = table->slices[sliceNumber].raw.order;
  int count;

  removeSliceAt(table, sliceNumber);

  // Reduce the order numbers of all slices that occur after the deleted
  // slice.
  for (count = 0; count < table->numSlices; count ++)
    if (table->slices[count].raw.tag &&
	(table->slices[count].raw.order > order))
      table->slices[count].raw.order -= 1;

  // Update the slice list
  updateSliceList(table);
}


static void delete(int sliceNumber)
{
  if (table->slices[sliceNumber].raw.flags & SLICEFLAG_BOOTABLE)
    warning("Deleting active partition.  You should set another partition "
	    "active.");

  doDelete(sliceNumber);

  if (table->selectedSlice >= table->numSlices)
    table->selectedSlice = (table->numSlices - 1);    

  table->changesPending += 1;
}


static sliceType queryPrimaryLogical(objectKey primLogRadio)
{
  sliceType retType = partition_none;
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


static int createSliceOrder(int sliceNumber, sliceType type)
{
  // Given a slice number which currently contains empty space, determine the
  // correct table order for a new slice which will reside there (and re-order
  // others, if appropriate).

  int order = 0;
  int count1, count2;

  // Determine the partition table order of this new slice.  First, just find
  // the first empty primary slot, which will be the order number if the new
  // slice is primary, and will also be the order number if the new slice is
  // logical but there are no existing logical slices already.
  for (count1 = 0; count1 < DISK_MAX_PARTITIONS; count1 ++)
    for (count2 = 0; count2 < table->numSlices; count2 ++)
      {
	if (table->slices[count2].raw.tag &&
	    !ISLOGICAL(&table->slices[count2]) &&
	    (table->slices[count2].raw.order == order))
	  {
	    order += 1;
	    break;
	  }
      }

  if (type == partition_primary)
    {
      // Any logical slices will have their order numbers increased.
      for (count1 = 0; count1 < table->numSlices; count1 ++)
	if (ISLOGICAL(&table->slices[count1]))
	  table->slices[count1].raw.order += 1;
    }

  else if (type == partition_logical)
    {
      // Logical slices' order in the table should always correspond with
      // their on-disk order, so if any previous slice is logical, the new
      // slice will follow it in the order.
      if (sliceNumber && ISLOGICAL(&table->slices[sliceNumber - 1]))
	order = (table->slices[sliceNumber - 1].raw.order + 1);

      // Otherwise if any following slice is logical, the new slice will
      // precede it in the order.
      else if ((sliceNumber < (table->numSlices - 1)) &&
	       ISLOGICAL(&table->slices[sliceNumber + 1]))
	order = table->slices[sliceNumber + 1].raw.order;

      // Any logical slices that follow this one will have their order
      // numbers increased.
      for (count1 = (sliceNumber + 1); count1 < table->numSlices; count1 ++)
	if (ISLOGICAL(&table->slices[count1]))
	  table->slices[count1].raw.order += 1;
    }

  return (order);
}


static int doCreate(int sliceNumber, sliceType type, unsigned startCylinder,
		    unsigned endCylinder)
{
  // Does the non-interactive work of creating a partition

  slice *newSlice = &(table->slices[sliceNumber]);
  int count;

  bzero(newSlice, sizeof(slice));

  newSlice->raw.order = createSliceOrder(sliceNumber, type);
  newSlice->raw.type = type;
  newSlice->raw.tag = 0x01;

  newSlice->raw.geom.startCylinder = startCylinder;
  newSlice->raw.geom.startHead = 0;
  newSlice->raw.geom.startSector = 1;
  
  if (newSlice->raw.geom.startCylinder == 0)
    {
      if (type == partition_logical)
	// Don't write a logical slice on the first cylinder.
	newSlice->raw.geom.startCylinder += 1;
      else
	// Don't write the first track of the first cylinder
	newSlice->raw.geom.startHead += 1;
    }

  if (type == partition_logical)
    // Don't write the first track of the extended partition
    newSlice->raw.geom.startHead += 1;

  newSlice->raw.geom.endCylinder = endCylinder;
  newSlice->raw.geom.endHead = (table->disk->heads - 1);
  newSlice->raw.geom.endSector = table->disk->sectorsPerCylinder;

  newSlice->raw.startLogical =
    ((newSlice->raw.geom.startCylinder * CYLSECTS(table->disk)) +
     (newSlice->raw.geom.startHead * table->disk->sectorsPerCylinder));

  newSlice->raw.sizeLogical =
    ((((newSlice->raw.geom.endCylinder -
	newSlice->raw.geom.startCylinder) + 1) * CYLSECTS(table->disk)) -
     (newSlice->raw.geom.startHead * table->disk->sectorsPerCylinder));

  // Update the slice list
  updateSliceList(table);

  // Find our new slice in the list
  for (count = 0; count < table->numSlices; count ++)
    if (table->slices[count].raw.geom.startCylinder == startCylinder)
      {
	sliceNumber = count;
	break;
      }

  return (sliceNumber);
}


static void create(int sliceNumber)
{
  // This is the interactive partition creation routine.

  enum { units_normal, units_mb, units_cylsize } units = units_normal;

  int status = 0;
  unsigned minStartCylinder = 0;
  unsigned maxEndCylinder = 0;
  sliceType type = partition_none;
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

  minStartCylinder = table->slices[sliceNumber].raw.geom.startCylinder;
  maxEndCylinder = table->slices[sliceNumber].raw.geom.endCylinder;

  while (1)
    {
      // See if we can create a slice here, and if so, what type?
      type =
	table->label->canCreate(table->slices, table->numSlices, sliceNumber);
      if ((int) type < 0)
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
	  primLogRadio =
	    windowNewRadioButton(createDialog, 2, 1, (char *[])
				 { "Primary", "Logical" }, 2 , &params);
	  if (type != partition_any)
	    {
	      if (type == partition_logical)
		windowComponentSetSelected(primLogRadio, 1);
	      windowComponentSetEnabled(primLogRadio, 0);
	    }

	  // Don't create a logical slice on the first cylinder
	  if ((type == partition_logical) && (minStartCylinder == 0))
	    minStartCylinder = 1;

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
		   cylsToMb(table->disk,
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

	  type = queryPrimaryLogical(primLogRadio);
	  if (type == partition_none)
	    return;

	  windowComponentGetData(startCylField, startCyl, 10);
	  windowComponentGetData(endCylField, endCyl, 10);
	  windowDestroy(createDialog);
	}

      else
	{
	  if (type == partition_any)
	    {
	      // Does the user prefer primary or logical?
	      type = queryPrimaryLogical(NULL);
	      if (type == partition_none)
		return;
	    }
	  else
	    printf("\nCreating %s partition\n",
		   ((type == partition_primary)? "primary" : "logical"));

	  // Don't create a logical slice on the first cylinder
	  if ((type == partition_logical) && (minStartCylinder == 0))
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
		 cylsToMb(table->disk,
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
	  endCylinder = (startCylinder + mbToCyls(table->disk, count) - 1);
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

  newSliceNumber = doCreate(sliceNumber, type, startCylinder, endCylinder);
  if (newSliceNumber < 0)
    return;

  if (setType(newSliceNumber) < 0)
    // Cancelled.  Remove it again.
    doDelete(newSliceNumber);
  else
    // The setType() will increase the 'changes pending' if it succeeded,
    // so we don't do it here.
    table->selectedSlice = newSliceNumber;

  return;
}


static int mountedCheck(slice *entry)
{
  // If the slice is mounted, query whether to ignore, unmount, or cancel

  int status = 0;
  int choice = 0;
  char tmpChar[160];
  disk tmpDisk;
  char character;

  if (entry->diskName[0] && (diskGet(entry->diskName, &tmpDisk) >= 0))
    {
      if (!tmpDisk.mounted)
	// Not mounted
	return (status = 0);

      // Mounted.  Prompt.
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

      if (choice == 0)
	// Ignore
	return (status = 0);

      if ((choice < 0) || (choice == 2))
	// Cancelled
	return (status = ERR_CANCELLED);

      if (choice == 1)
	{
	  // Try to unmount the filesystem
	  status = filesystemUnmount(tmpDisk.mountPoint);
	  if (status < 0)
	    error("Unable to unmount %s", tmpDisk.mountPoint);
	  return (status);
	}
    }

  // The disk probably doesn't exist (yet).  So, it obviously can't be
  // mounted
  return (status = 0);
}


static void format(int sliceNumber)
{
  // Prompt, and format a slice

  int status = 0;
  slice *formatSlice = &(table->slices[sliceNumber]);
  char *fsTypes[] = { "FAT", "EXT2", "Linux-swap", "None" };
  char *fatTypes[] = { "Default", "FAT12", "FAT16", "FAT32" };
  const char *chooseString = "Choose the filesystem type:";
  const char *fatString = "Choose the FAT type:";
  int typeNum = 0;
  char typeName[16];
  char tmpChar[160];

  if (table->changesPending)
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
      sprintf(tmpChar, "Format Partition %s", formatSlice->showSliceName);
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
	      "undone)", formatSlice->showSliceName);

      if (!yesOrNo(tmpChar))
	return;

      status = filesystemClobber(formatSlice->diskName);
    }
  else
    {
      sprintf(tmpChar, "Format partition %s as %s?\n(This change cannot be "
	      "undone)", formatSlice->showSliceName, typeName);

      if (!yesOrNo(tmpChar))
	return;

      // Do the format
      sprintf(tmpChar, "/programs/format %s -s -t %s %s",
	      (graphics? "" : "-T"), typeName, formatSlice->diskName);

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

  // Make the slice list
  makeSliceList(table);

  return;
}


static void defragment(int sliceNumber)
{
  // Prompt, and defragment a slice

  int status = 0;
  slice *defragSlice = &(table->slices[sliceNumber]);
  progress prog;
  objectKey progressDialog = NULL;
  char tmpChar[160];

  if (table->changesPending)
    {
      error("A partition defragmentation cannot be undone, and it is "
	    "required\nthat you write your other changes to disk before "
	    "continuing.");
      return;
    }

  sprintf(tmpChar, "Defragment partition %s?\n(This change cannot be "
	  "undone)", defragSlice->showSliceName);

  if (!yesOrNo(tmpChar))
    return;

  status = mountedCheck(defragSlice);
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

  status = filesystemDefragment(defragSlice->diskName, &prog);

  if (graphics && progressDialog)
    windowProgressDialogDestroy(progressDialog);
  else
    vshProgressBarDestroy(&prog);

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


static void hide(int sliceNumber)
{
  if (PARTITION_TYPEID_IS_HIDDEN(table->slices[sliceNumber].raw.tag))
    table->slices[sliceNumber].raw.tag -= 0x10;
  else if (PARTITION_TYPEID_IS_HIDEABLE(table->slices[sliceNumber].raw.tag))
    table->slices[sliceNumber].raw.tag += 0x10;
  else
    return;

  table->changesPending += 1;

  // Update the slice list
  updateSliceList(table);
}


static void info(int sliceNumber)
{
  // Print info about a slice
  
  slice *infoSlice = &table->slices[sliceNumber];
  char *buff = NULL;

  buff = malloc(1024);
  if (buff == NULL)
    return;

  if (infoSlice->raw.tag)
    sprintf(buff, "PARTITION %s INFO:\n\nActive : %s\nType ID : %02x\n",
	    infoSlice->showSliceName,
	    (infoSlice->raw.flags & SLICEFLAG_BOOTABLE? "yes" : "no"),
	    infoSlice->raw.tag);
  else
    sprintf(buff, "EMPTY SPACE INFO:\n\n");

  sprintf((buff + strlen(buff)), "Starting Cyl/Hd/Sect: %u/%u/%u\nEnding "
	  "Cyl/Hd/Sect  : %u/%u/%u\nLogical start sector: %u\nLogical size: "
	  "%u", infoSlice->raw.geom.startCylinder,
	  infoSlice->raw.geom.startHead, infoSlice->raw.geom.startSector,
	  infoSlice->raw.geom.endCylinder, infoSlice->raw.geom.endHead,
	  infoSlice->raw.geom.endSector, infoSlice->raw.startLogical,
	  infoSlice->raw.sizeLogical);

  if (graphics)
    windowNewInfoDialog(window, "Info", buff);
  else
    {
      printf("\n%s\n", buff);
      pause();
    }

  free(buff);
}


static void undo(void)
{
  // Undo changes
  if (table->changesPending)
    {
      // Re-scan from the original raw slices.
      makeSliceList(table);

      table->selectedSlice = 0;
      table->changesPending = 0;
    }
}


static void writeChanges(partitionTable *t, int confirm)
{
  int status = 0;

  if (t->changesPending)
    {
      if (confirm && !yesOrNo("Committing changes to disk.  Are you SURE?"))
	return;

      // Write out the partition table
      status = writePartitionTable(t);
      if (status < 0)
	error("Unable to write the partition table of %s.", t->disk->name);

      // Tell the kernel to reexamine the partition tables
      diskReadPartitions(t->disk->name);

      // Make the slice list.
      makeSliceList(t);
    }
}


static void formatTime(char *string, unsigned seconds)
{
  strcpy(string, "Time remaining: ");
  if (seconds > 3600)
    sprintf((string + strlen(string)), "%d hour%s %d minute%s",
	    (seconds / 3600), ((seconds > 7200)? "s" : ""),
	    ((seconds % 3600) / 60), ((seconds > 120)? "s" : ""));
  else if (seconds > 60)
    sprintf((string + strlen(string)), "%d minute%s",
	    (seconds / 60), ((seconds > 120)? "s" : ""));
  else
    sprintf((string + strlen(string)), "less than 1 minute");
}


static int doMove(int sliceNumber, unsigned newStartCylinder)
{
  int status = 0;
  unsigned newStartLogical = 0;
  unsigned newEndLogical = 0;
  slice *moveSlice = &table->slices[sliceNumber];
  int moveLeft = 0;
  unsigned sectorsPerOp = 0;
  unsigned srcSector = 0;
  unsigned destSector = 0;
  unsigned overlapSector = 0;
  unsigned sectorsToCopy = 0;
  unsigned char *buffer = NULL;
  int startSeconds = 0;
  int remainingSeconds = 0;
  progress prog;
  objectKey progressDialog = NULL;
  char tmpChar[160];
  int count;

  // The new starting logical sector
  newStartLogical = (newStartCylinder * CYLSECTS(table->disk));

  if (newStartCylinder == 0)
    {
      if (ISLOGICAL(moveSlice))
	{
	  // Don't write a logical slice on the first cylinder
	  newStartCylinder += 1;
	  newStartLogical += CYLSECTS(table->disk);
	}
      else
	// Don't write the first track of the first cylinder
	newStartLogical += table->disk->sectorsPerCylinder;
    }

  if (ISLOGICAL(moveSlice))
    // Don't write the first track of the extended partition
    newStartLogical += table->disk->sectorsPerCylinder;

  // Which direction?
  if (newStartLogical < moveSlice->raw.startLogical)
    moveLeft = 1;
 
  sectorsPerOp = CYLSECTS(table->disk);
  if (moveLeft &&
      ((moveSlice->raw.startLogical - newStartLogical) < sectorsPerOp))
    sectorsPerOp = (moveSlice->raw.startLogical - newStartLogical);
  else if (!moveLeft &&
	   ((newStartLogical - moveSlice->raw.startLogical) < sectorsPerOp))
    sectorsPerOp = (newStartLogical - moveSlice->raw.startLogical);

  if (moveLeft)
    {
      srcSector = moveSlice->raw.startLogical;
      destSector = newStartLogical;

      // Will the new location overlap the old location?
      if ((newStartLogical + moveSlice->raw.sizeLogical) >
	  moveSlice->raw.startLogical)
	overlapSector = moveSlice->raw.startLogical;
    }
  else
    {
      srcSector = (moveSlice->raw.startLogical +
		   (moveSlice->raw.sizeLogical - sectorsPerOp));
      destSector =
	(newStartLogical + (moveSlice->raw.sizeLogical - sectorsPerOp));

      // Will the new location overlap the old location?
      if ((moveSlice->raw.startLogical + moveSlice->raw.sizeLogical) >
	  newStartLogical)
	overlapSector =
	  (moveSlice->raw.startLogical + (moveSlice->raw.sizeLogical - 1));
    }

  sectorsToCopy = moveSlice->raw.sizeLogical;
  
  // Get a memory buffer to copy data to/from
  buffer = malloc(sectorsPerOp * table->disk->sectorSize);
  if (buffer == NULL)
    {
      error("Unable to allocate memory");
      return (status = ERR_MEMORY);
    }

  bzero((void *) &prog, sizeof(progress));
  prog.total = sectorsToCopy;
  strcpy((char *) prog.statusMessage, "Time remaining: ?? hours ?? minutes");
  if (!overlapSector || !((overlapSector >= destSector) &&
			  (overlapSector < (destSector + sectorsPerOp))))
    prog.canCancel = 1;

  sprintf(tmpChar, "Moving %u Mb",
	  (moveSlice->raw.sizeLogical / (1048576 / table->disk->sectorSize)));

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
	diskReadSectors(table->disk->name, srcSector, sectorsPerOp, buffer);
      if (status < 0)
	{
	  error("Read error %d reading sectors %u-%u from disk %s",
		status, srcSector, (srcSector + (sectorsPerOp - 1)),
		table->disk->name);
	  goto out;
	}

      if (prog.cancel)
	goto out;

      // Write to destination
      status =
	diskWriteSectors(table->disk->name, destSector, sectorsPerOp, buffer);
      if (status < 0)
	{
	  error("Write error %d writing sectors %u-%u to disk %s",
		status, destSector, (destSector + (sectorsPerOp - 1)),
		table->disk->name);
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

	  formatTime((char *) prog.statusMessage, remainingSeconds);

	  // Will the next operation overwrite any of the original data?
	  if (overlapSector && (overlapSector >= destSector) &&
	      (overlapSector < (destSector + sectorsPerOp)))
	    prog.canCancel = 0;

	  lockRelease(&(prog.lock));
	}
    }

  // Set the new slice data
  moveSlice->raw.startLogical = newStartLogical;
  moveSlice->raw.geom.startCylinder =
    (newStartLogical / CYLSECTS(table->disk));
  moveSlice->raw.geom.startHead = ((newStartLogical % CYLSECTS(table->disk)) /
				   table->disk->sectorsPerCylinder);
  moveSlice->raw.geom.startSector =
    (((newStartLogical % CYLSECTS(table->disk)) %
      table->disk->sectorsPerCylinder) + 1);
  newEndLogical =
    (moveSlice->raw.startLogical + (moveSlice->raw.sizeLogical - 1));
  moveSlice->raw.geom.endCylinder = (newEndLogical / CYLSECTS(table->disk));
  moveSlice->raw.geom.endHead = ((newEndLogical % CYLSECTS(table->disk)) /
				 table->disk->sectorsPerCylinder);
  moveSlice->raw.geom.endSector = (((newEndLogical % CYLSECTS(table->disk)) %
				    table->disk->sectorsPerCylinder) + 1); 

  table->changesPending += 1;

  // Write everything
  writeChanges(table, 0);

  // Find our moved slice in the list
  for (count = 0; count < table->numSlices; count ++)
    if (table->slices[count].raw.startLogical == newStartLogical)
      {
	sliceNumber = count;
	break;
      }

 out:
  // Release memory
  free(buffer);

  if (graphics && progressDialog)
    windowProgressDialogDestroy(progressDialog);
  else
    vshProgressBarDestroy(&prog);

  return (sliceNumber);
}


static int move(int sliceNumber)
{
  int status = 0;
  slice *moveSlice = &table->slices[sliceNumber];
  unsigned moveRange[] = { -1, -1 };
  char number[10];
  unsigned newStartCylinder = 0;
  char tmpChar[160];

  if (table->changesPending)
    {
      error("A partition move cannot be undone, and must be committed\n"
	    "to disk immediately.  You need to write your other changes\n"
	    "to disk before continuing.");
      return (status = 0);
    }

  // If there are no empty spaces on either side, error
  if (((sliceNumber == 0) || table->slices[sliceNumber - 1].raw.tag) &&
      ((sliceNumber == (table->numSlices - 1)) ||
       table->slices[sliceNumber + 1].raw.tag))
    {
      error("No empty space on either side!");
      return (status = ERR_INVALID);
    }

  status = mountedCheck(moveSlice);
  if (status < 0)
    return (status);

  // Figure out the ranges of cylinders we can move to in both directions
  if ((sliceNumber > 0) && !table->slices[sliceNumber - 1].raw.tag)
    {
      moveRange[0] = table->slices[sliceNumber - 1].raw.geom.startCylinder;
      moveRange[1] = table->slices[sliceNumber - 1].raw.geom.endCylinder;
    }
  if ((sliceNumber < (table->numSlices - 1)) &&
      !table->slices[sliceNumber + 1].raw.tag)
    {
      if (moveRange[0] == (unsigned) -1)
	moveRange[0] = (moveSlice->raw.geom.startCylinder + 1);
      moveRange[1] = (table->slices[sliceNumber + 1].raw.geom.endCylinder -
		      (moveSlice->raw.geom.endCylinder -
		       moveSlice->raw.geom.startCylinder));
    }

  if (ISLOGICAL(moveSlice) && !moveRange[0])
    // Don't write a logical slice on the first cylinder
    moveRange[0] += 1;

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
  if (newStartCylinder == moveSlice->raw.geom.startCylinder)
    return (status = 0);

  sprintf(tmpChar, "Moving partition from cylinder %u to cylinder %u.\n"
	  "Continue?", moveSlice->raw.geom.startCylinder,
	  newStartCylinder);
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

  status = doMove(sliceNumber, newStartCylinder);
  if (status >= 0)
    table->selectedSlice = status;

  return (status);
}


static void deleteAll(void)
{
  // This function completely nukes the slice entries (as opposed to
  // merely clearing type IDs, so it is theoretically 'un-deletable').

  bzero(table->slices, (MAX_SLICES * sizeof(slice)));
  table->numSlices = 0;

  table->selectedSlice = 0;
  table->changesPending += 1;

  // Update the slice list.
  updateSliceList(table);
}


static void resizeSlice(slice *rszSlice, unsigned newEndCylinder,
			unsigned newEndHead, unsigned newEndSector,
			unsigned newSize)
{
  // Resize the slice
  rszSlice->raw.geom.endCylinder = newEndCylinder;
  rszSlice->raw.geom.endHead = newEndHead;
  rszSlice->raw.geom.endSector = newEndSector;
  rszSlice->raw.sizeLogical = newSize;
  
  // Update the slice list
  updateSliceList(table);

  table->changesPending += 1;
}


static int doResize(int sliceNumber, unsigned newEndCylinder, int resizeFs)
{
  int status = 0;
  unsigned newSize = 0;
  unsigned oldEndCylinder, oldEndHead, oldEndSector, oldSizeLogical;
  int didResize = 0;
  progress prog;
  objectKey progressDialog = NULL;

  newSize = (((newEndCylinder + 1) * CYLSECTS(table->disk)) -
	     table->slices[sliceNumber].raw.startLogical);

  oldEndCylinder = table->slices[sliceNumber].raw.geom.endCylinder;
  oldEndHead = table->slices[sliceNumber].raw.geom.endHead;
  oldEndSector = table->slices[sliceNumber].raw.geom.endSector;
  oldSizeLogical = table->slices[sliceNumber].raw.sizeLogical;

  if (newSize >= table->slices[sliceNumber].raw.sizeLogical)
    {
      resizeSlice(&table->slices[sliceNumber], newEndCylinder,
		  (table->disk->heads - 1), table->disk->sectorsPerCylinder,
		  newSize);
      didResize = 1;
    }

  // Now, if we're resizing the filesystem...
  if (resizeFs)
    {
      // Write everything
      writeChanges(table, 0);

      bzero((void *) &prog, sizeof(progress));
      if (graphics)
	progressDialog =
	  windowNewProgressDialog(window, "Resizing Filesystem...", &prog);
      else
	vshProgressBar(&prog);

      if (!strcmp(table->slices[sliceNumber].fsType, "ntfs"))
	// NTFS resizing is done by our libntfs library
	status =
	  ntfsResize(table->slices[sliceNumber].diskName, newSize, &prog);
      else
	// The kernel will do the resize
	status = filesystemResize(table->slices[sliceNumber].diskName,
				  newSize, &prog);

      if (graphics && progressDialog)
	windowProgressDialogDestroy(progressDialog);
      else
	vshProgressBarDestroy(&prog);

      // Update the slice list
      updateSliceList(table);

      if (status < 0)
	{
	  if (didResize)
	    {
	      // Undo the slice resize
	      resizeSlice(&table->slices[sliceNumber], oldEndCylinder,
			  oldEndHead, oldEndSector, oldSizeLogical);
	      writeChanges(table, 0);
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
      resizeSlice(&table->slices[sliceNumber], newEndCylinder,
		  (table->disk->heads - 1),
		  table->disk->sectorsPerCylinder, newSize);
      
      if (resizeFs)
	// We already resized the filesystem, so write everything
	writeChanges(table, 0);
    }

  return (status = 0);
}


static int resize(int sliceNumber)
{
  enum { units_normal, units_mb, units_cylsize } units = units_normal;

  int status = 0;
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
  componentParameters params;
  windowDrawParameters drawParams;
  windowEvent event;
  unsigned newSize = 0;
  objectKey bannerDialog = NULL;
  char tmpChar[256];
  int count;

  // Determine whether or not we can resize the filesystem
  if ((table->slices[sliceNumber].opFlags & FS_OP_RESIZE) ||
      !strcmp(table->slices[sliceNumber].fsType, "ntfs"))
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
	  if (table->changesPending)
	    {
	      error("A filesystem resize cannot be undone, and must be "
		    "committed\nto disk immediately.  You need to write your "
		    "other changes\nto disk before continuing.");
	      return (status = 0);
	    }

	  if (table->slices[sliceNumber].opFlags & FS_OP_RESIZECONST)
	    {
	      strcpy(tmpChar, "Collecting filesystem resizing constraints...");
	      if (graphics)
		bannerDialog = windowNewBannerDialog(window, "Filesystem",
						     tmpChar);
	      else
		printf("\n%s\n\n", tmpChar);

	      status =
		filesystemResizeConstraints(table->slices[sliceNumber]
					    .diskName, &minFsSectors,
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
      // the slice anyway.
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

  status = mountedCheck(&table->slices[sliceNumber]);
  if (status < 0)
    return (status);

  // Calculate the minimum and maximum permissable sizes.

  minEndCylinder = table->slices[sliceNumber].raw.geom.startCylinder;
  if (haveResizeConstraints)
    minEndCylinder += (((minFsSectors / CYLSECTS(table->disk)) +
			((minFsSectors % CYLSECTS(table->disk))? 1 : 0)) - 1);

  if ((sliceNumber < (table->numSlices - 1)) &&
      !table->slices[sliceNumber + 1].raw.tag)
    maxEndCylinder = table->slices[sliceNumber + 1].raw.geom.endCylinder;
  else
    maxEndCylinder = table->slices[sliceNumber].raw.geom.endCylinder;
  if (haveResizeConstraints)
    maxEndCylinder =
      min((table->slices[sliceNumber].raw.geom.startCylinder +
	   ((maxFsSectors / CYLSECTS(table->disk)) +
	    ((maxFsSectors % CYLSECTS(table->disk))? 1 : 0)) - 1),
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
		   table->slices[sliceNumber].raw.geom.endCylinder,
		   minEndCylinder, maxEndCylinder,
		   cylsToMb(table->disk,
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
	      memcpy(&drawParams.foreground, table->slices[sliceNumber].color,
		     sizeof(color));
	      windowComponentSetData(partCanvas, &drawParams, 1);

	      // Draw a shaded bit representing the used portion
	      drawParams.foreground.red =
		((drawParams.foreground.red * 2) / 3);
	      drawParams.foreground.green =
		((drawParams.foreground.green * 2) / 3);
	      drawParams.foreground.blue =
		((drawParams.foreground.blue * 2) / 3);
	      drawParams.width = ((minFsSectors * drawParams.width) /
				  table->slices[sliceNumber].raw.sizeLogical);
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
		 ", or 'Q' to quit:\n-> ",
		 table->slices[sliceNumber].raw.geom.endCylinder,
		 minEndCylinder, maxEndCylinder,
		 cylsToMb(table->disk, (maxEndCylinder - minEndCylinder + 1)),
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
	  newEndCylinder = (table->slices[sliceNumber].raw.geom.startCylinder +
			    mbToCyls(table->disk, newEndCylinder) - 1);
	  break;
	case units_cylsize:
	  newEndCylinder = (table->slices[sliceNumber].raw.geom.startCylinder +
			    newEndCylinder - 1);
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

      newSize = (((newEndCylinder + 1) * CYLSECTS(table->disk)) -
		 table->slices[sliceNumber].raw.startLogical);
      break;
    }

  // Before we go, warn about backups and such.
  snprintf(tmpChar, 256, "Resizing partition from %u to %u sectors.\n"
	   "Please use this feature with caution, and only after\n"
	   "making a backup of all important data.  Continue?",
	   table->slices[sliceNumber].raw.sizeLogical, newSize);
  if (graphics)
    {
      if (!windowNewQueryDialog(window, "Resizing", tmpChar))
	return (status = 0);
    }
  else
    {
      if (!yesOrNo(tmpChar))
	return (status = 0);
    }

  status = doResize(sliceNumber, newEndCylinder, resizeFs);
  if (status < 0)
    return (status);

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

	  formatTime((char *) args->prog->statusMessage, remainingSeconds);

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
      buffer.buffer[0].data = memoryGet(buffer.bufferSize, "disk copy buffer");
      buffer.buffer[1].data = memoryGet(buffer.bufferSize, "disk copy buffer");

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
  strcpy((char *) prog.statusMessage, "Time remaining: ?? hours ?? minutes");
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

  // Flush data.
  diskSync();

  if (graphics && progressDialog)
    windowProgressDialogDestroy(progressDialog);
  else
    vshProgressBarDestroy(&prog);

  return (status);
}


static void clearDiskLabel(disk *theDisk, diskLabel *label)
{
  partitionTable t;
  int count;

  bzero(&t, sizeof(partitionTable));

  // Find it
  for (count = 0; count < numberDisks; count ++)
    if (&disks[count] == theDisk)
      {
	t.disk = theDisk;
	t.diskNumber = count;
	t.label = label;
	t.changesPending = 1;
	writeChanges(&t, 0);
	break;
      }
}


static int setFatGeometry(partitionTable *t, int sliceNumber)
{
  // Given a slice, make sure the FAT disk geometry fields are correct.

  int status = 0;
  slice *slc = &t->slices[sliceNumber];
  unsigned char *bootSector = NULL;
  fatBPB *bpb = NULL;

  bootSector = malloc(t->disk->sectorSize);
  if (bootSector == NULL)
    return (status = ERR_MEMORY);
  bpb = (fatBPB *) bootSector;

  // Read the boot sector
  status =
    diskReadSectors(t->disk->name, slc->raw.startLogical, 1, bootSector);
  if (status < 0)
    {
      free(bootSector);
      return (status);
    }

  // Set the values
  bpb->sectsPerTrack = t->disk->sectorsPerCylinder;
  bpb->numHeads = t->disk->heads;

  if (!strcmp(slc->fsType, "fat32"))
    bpb->fat32.biosDriveNum = (0x80 + t->disk->deviceNumber);
  else
    bpb->fat.biosDriveNum = (0x80 + t->disk->deviceNumber);

  // Write back the boot sector
  status =
    diskWriteSectors(t->disk->name, slc->raw.startLogical, 1, bootSector);

  free(bootSector);
  return (status);
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
	  retDisk = &(disks[selected]);
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


static void copyDisk(void)
{
  int status = 0;
  char *diskStrings[DISK_MAXDEVICES];
  int diskNumber = 0;
  disk *srcDisk = NULL;
  disk *destDisk = NULL;
  unsigned lastUsedSector = 0;
  char tmpChar[160];
  int count;

  if (numberDisks < 2)
    {
      error("No other disks to copy to");
      return;
    }

  srcDisk = table->disk;

  // If there's only one other disk, select it automatically
  if (numberDisks == 2)
    {
      for (count = 0; count < numberDisks; count ++)
	if (&disks[count] != srcDisk)
	  {
	    destDisk = &disks[count];
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
		return;
	    }
	  else
	    {
	      for (count = 0; count < numberDisks; count ++)
		diskStrings[count] = diskListParams[count].text;

	      diskNumber =
		vshCursorMenu("Please choose the disk to copy to:",
			      diskStrings, numberDisks, 0);
	      if (diskNumber < 0)
		return;

	      destDisk = &disks[diskNumber];
	    }
	
	  if (destDisk == srcDisk)
	    {
	      error("Not much point in copying a disk to itself!");
	      continue;
	    }

	  break;
	}
    }

  // We have a source disk and a destination disk.
  sprintf(tmpChar, "Copy disk %s to disk %s.\nWARNING: THIS WILL DESTROY ALL "
	  "DATA ON DISK %s.\nARE YOU SURE YOU WANT TO DO THIS?", srcDisk->name,
	  destDisk->name, destDisk->name);
  if (!yesOrNo(tmpChar))
    return;

  // We will copy everything up to the end of the last slice (not much
  // point in copying a bunch of unused space, even though it's potentially
  // conceivable that someone, somewhere might want to do that).  Find out
  // the logical sector number of the end of the last slice.
  for (count = 0; count < table->numSlices; count ++)
    {
      if (table->slices[count].raw.tag &&
	  ((table->slices[count].raw.startLogical +
	    table->slices[count].raw.sizeLogical - 1) > lastUsedSector))
	lastUsedSector = (table->slices[count].raw.startLogical +
			  table->slices[count].raw.sizeLogical - 1);
    }
  
  if (lastUsedSector == 0)
    {
      if (!yesOrNo("No partitions on the disk.  Do you want to copy the "
		   "whole\ndisk anyway?"))
	return;
      
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
	return;
      printf("\n");
      
      lastUsedSector = (destDisk->numSectors - 1);
    }

  // Go
  status = copyData(srcDisk, 0, destDisk, 0, (lastUsedSector + 1));

  // If it was cancelled, clear the disk label
  if (status == ERR_CANCELLED)
    clearDiskLabel(destDisk, table->label);

  status = selectDisk(destDisk);
  if (status < 0)
    return;

  // Now, if any slices are ouside (or partially outside) the bounds of the
  // destination disk, delete (or truncate) 
  for (count = (table->numSlices - 1); count >= 0; count --)
    {
      // Starts past the end of the disk?
      if (table->slices[count].raw.geom.startCylinder >= destDisk->cylinders)
	{
	  table->numSlices -= 1;
	  table->changesPending += 1;
	}

      // Ends past the end of the disk?
      else if (table->slices[count].raw.geom.endCylinder >=
	       destDisk->cylinders)
	{
	  table->slices[count].raw.geom.endCylinder =
	    (destDisk->cylinders - 1);
	  table->slices[count].raw.geom.endHead = (destDisk->heads - 1);
	  table->slices[count].raw.geom.endSector =
	    destDisk->sectorsPerCylinder;
	  table->slices[count].raw.sizeLogical =   
	    ((((table->slices[count].raw.geom.endCylinder -
		table->slices[count].raw.geom.startCylinder) + 1) *
	      CYLSECTS(destDisk)) - (table->slices[count].raw.geom.startHead *
				     destDisk->sectorsPerCylinder));
	  table->changesPending += 1;
	}
    }

  // Write out the partition table
  writeChanges(table, 0);

  // Make sure the disk geometries of any FAT slices are correct for
  // the new disk.
  for (count = 0; count < table->numSlices; count ++)
    if (table->slices[count].raw.tag &&
	!strncmp(table->slices[count].fsType, "fat", 3))
      setFatGeometry(table, count);

  return;
}


static void copyPartition(int sliceNumber)
{
  // 'Copy' a slice to our slice 'clipboard'
  memcpy(&clipboardSlice, &(table->slices[sliceNumber]), sizeof(slice));
  clipboardDisk = table->disk;
  clipboardSliceValid = 1;
}


static int pastePartition(int sliceNumber)
{
  // 'Paste' a slice from our partition 'clipboard' to the supplied
  // empty space slice.  This is really where a slice copying operation
  // takes place.

  int status = 0;
  slice *emptySlice = NULL;
  sliceType newType = partition_none;
  unsigned newEndCylinder = 0;
  int newSliceNumber = 0;
  char tmpChar[160];

  if (!clipboardSliceValid)
    {
      error("No partition copied to the clipboard");
      return (status = ERR_NODATA);
    }

  emptySlice = &(table->slices[sliceNumber]);

  if (emptySlice->raw.tag)
    // Not empty space
    return (status = ERR_INVALID);

  // See if we can create a slice here, and if so, what type?
  newType =
    table->label->canCreate(table->slices, table->numSlices, sliceNumber);
  if ((int) newType < 0)
    return (status = newType);

  // Check whether the empty slice is big enough
  if (emptySlice->raw.sizeLogical < clipboardSlice.raw.sizeLogical)
    {
      error("Partition %s is too big (%u sectors) to fit in the\nselected "
	    "empty space (%u sectors)", clipboardSlice.showSliceName,
	    clipboardSlice.raw.sizeLogical, emptySlice->raw.sizeLogical);
      return (status = ERR_NOSUCHENTRY);
    }

  // Everything seems OK.  Confirm.
  sprintf(tmpChar, "Paste partition %s to selected empty space on disk %s?",
	  clipboardSlice.showSliceName, table->disk->name);
  if (!yesOrNo(tmpChar))
    return (status = 0);

  status = copyData(clipboardDisk, clipboardSlice.raw.startLogical,
		    table->disk, emptySlice->raw.startLogical,
		    clipboardSlice.raw.sizeLogical);
  if (status < 0)
    return (status);

  newEndCylinder =
    (emptySlice->raw.geom.startCylinder +
     (clipboardSlice.raw.sizeLogical / CYLSECTS(table->disk)) +
     ((clipboardSlice.raw.sizeLogical % CYLSECTS(table->disk)) != 0) - 1);

  newSliceNumber =
    doCreate(sliceNumber, newType, emptySlice->raw.geom.startCylinder,
	     newEndCylinder);
  if (newSliceNumber < 0)
    return (status = newSliceNumber);

  // If it's a FAT filesystem, make sure the disk geometry stuff in it
  // is correct for the new disk.
  if (!strncmp(table->slices[newSliceNumber].fsType, "fat", 3))
    setFatGeometry(table, newSliceNumber);

  table->selectedSlice = newSliceNumber;
  table->changesPending += 1;

  return (status = 0);
}


static void swapSlices(disk *theDisk, slice *firstSlice, slice *secondSlice)
{
  // Given 2 slices, swap them.  This is primarily for the change partition
  // order function, below

  int tmpOrder = 0;
  slice tmpSlice;

  // Swap the partition data
  memcpy(&tmpSlice, secondSlice, sizeof(slice));
  memcpy(secondSlice, firstSlice, sizeof(slice));
  memcpy(firstSlice, &tmpSlice, sizeof(slice));

  tmpOrder = secondSlice->raw.order;
  secondSlice->raw.order = firstSlice->raw.order;
  firstSlice->raw.order = tmpOrder;

#ifdef PARTLOGIC
  sprintf(firstSlice->showSliceName, "%d", (firstSlice->raw.order + 1));
  sprintf(secondSlice->showSliceName, "%d", (secondSlice->raw.order + 1));
#else
  sprintf(firstSlice->showSliceName, "%s%c", theDisk->name,
	  ('a' + firstSlice->raw.order));
  sprintf(secondSlice->showSliceName, "%s%c", theDisk->name,
	  ('a' + secondSlice->raw.order));
#endif

  makeSliceString(theDisk, firstSlice);
  makeSliceString(theDisk, secondSlice);
}


static void changePartitionOrder(void)
{
  // This allows the user to change the ordering of primary partitions

  objectKey orderDialog = NULL;
  slice orderSlices[DISK_MAX_PRIMARY_PARTITIONS];
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

  // Copy primary slices in our array, in order
  for (count1 = 0; count1 < DISK_MAX_PRIMARY_PARTITIONS; count1 ++)
    for (count2 = 0; count2 < table->numSlices; count2 ++)
      if (table->slices[count2].raw.tag &&
	  (table->slices[count2].raw.order == count1) &&
	  !ISLOGICAL(&table->slices[count2]))
	{
	  memcpy(&orderSlices[numOrderSlices], &table->slices[count2],
		 sizeof(slice));
	  strncpy(orderListParams[numOrderSlices++].text,
		  table->slices[count2].string, WINDOW_MAX_LABEL_LENGTH);
	  break;
	}

  if (numOrderSlices < 2)
    {
      error("Must be more than one primary partition to reorder!");
      return;
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
	      swapSlices(table->disk, &orderSlices[selected],
			 &orderSlices[selected - 1]);
	      strncpy(orderListParams[selected].text,
		      orderSlices[selected].string,
		      WINDOW_MAX_LABEL_LENGTH);
	      strncpy(orderListParams[selected - 1].text,
		      orderSlices[selected - 1].string,
		      WINDOW_MAX_LABEL_LENGTH);
	      windowComponentSetData(orderList, orderListParams,
				     numOrderSlices);
	      windowComponentSetSelected(orderList, (selected - 1));
	    }
	  
	  if ((windowComponentEventGet(downButton, &event) > 0) &&
	      (event.type == EVENT_MOUSE_LEFTUP) &&
	      (selected < (numOrderSlices - 1)))
	    {
	      // 'Down' button
	      swapSlices(table->disk, &orderSlices[selected],
			 &orderSlices[selected + 1]);
	      strncpy(orderListParams[selected].text,
		      orderSlices[selected].string,
		      WINDOW_MAX_LABEL_LENGTH);
	      strncpy(orderListParams[selected + 1].text,
		      orderSlices[selected + 1].string,
		      WINDOW_MAX_LABEL_LENGTH);
	      windowComponentSetData(orderList, orderListParams,
				     numOrderSlices);
	      windowComponentSetSelected(orderList, (selected + 1));
	    }

	  // Check for our OK button
	  if ((windowComponentEventGet(okButton, &event) > 0) &&
	      (event.type == EVENT_MOUSE_LEFTUP))
	    {
	      windowDestroy(orderDialog);
	      goto commit;
	    }

	  // Check for our Cancel button or window close
	  if (((windowComponentEventGet(orderDialog, &event) > 0) &&
	       (event.type == EVENT_WINDOW_CLOSE)) ||
	      ((windowComponentEventGet(cancelButton, &event) > 0) &&
	       (event.type == EVENT_MOUSE_LEFTUP)))
	    {
	      windowDestroy(orderDialog);
	      return;
	    }

	  // Done
	  multitaskerYield();
	}
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
	      textSetCursor(1);
	      textInputSetEcho(1);
	      goto commit;

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
		  swapSlices(table->disk, &orderSlices[selected],
			     &orderSlices[selected - 1]);
		  strncpy(orderListParams[selected].text,
			  orderSlices[selected].string,
			  WINDOW_MAX_LABEL_LENGTH);
		  strncpy(orderListParams[selected - 1].text,
			  orderSlices[selected - 1].string,
			  WINDOW_MAX_LABEL_LENGTH);
		  selected -= 1;

		}
	      continue;

	    case '+':
	      if (selected < (numOrderSlices - 1))
		{
		  // Move down
		  swapSlices(table->disk, &orderSlices[selected],
			     &orderSlices[selected + 1]);
		  strncpy(orderListParams[selected].text,
			  orderSlices[selected].string,
			  WINDOW_MAX_LABEL_LENGTH);
		  strncpy(orderListParams[selected + 1].text,
			  orderSlices[selected + 1].string,
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

 commit:
  // If we fall through to here, we are making the changes.  Copy the slices
  // from our (re)-ordered list back into the partition table.
  for (count1 = 0; count1 < numOrderSlices; count1 ++)
    for (count2 = 0; count2 < table->numSlices; count2 ++)
      if (orderSlices[count1].raw.startLogical ==
	  table->slices[count2].raw.startLogical)
	{
	  memcpy(&table->slices[count2], &orderSlices[count1], sizeof(slice));
	  break;
	}
  updateSliceList(table);
  table->changesPending += 1;
  return;
}


static int writeSimpleMbr(void)
{
  // Put simple MBR code into the main partition table.

  int status = 0;
  void *sectorData = NULL;
  fileStream mbrFile;

  if (table->changesPending)
    {
      error("This operation cannot be undone, and it is required that\n"
	    "you write your other changes to disk before continuing.");
      return (status = ERR_BUSY);
    }

  if (!yesOrNo("After you write changes, the \"active\" partition will\n"
	       "always boot automatically.  Proceed?"))
    return (status = 0);

  // Open the MBR file
  bzero(&mbrFile, sizeof(fileStream));
  status = fileStreamOpen(SIMPLE_MBR_FILE, OPENMODE_READ, &mbrFile);
  if (status < 0)
    {
      error("Can't locate simple MBR file %s", SIMPLE_MBR_FILE);
      return (status);
    }

  // Get memory to hold the MBR sector
  sectorData = malloc(table->disk->sectorSize);
  if (sectorData == NULL)
    {
      error("Error getting memory");
      return (status = ERR_MEMORY);
    }

  // Read the current MBR sector.
  status = diskReadSectors(table->disk->name, 0, 1, sectorData);
  if (status < 0)
    {
      error("Couldn't read MBR sector");
      return (status);
    }

  // Read bytes 0-445 into the sector data
  status = fileStreamRead(&mbrFile, 446, sectorData);
  if (status < 0)
    {
      error("Can't read simple MBR file %s", SIMPLE_MBR_FILE);
      return (status);
    }

  // Write back the MBR sector.
  status = diskWriteSectors(table->disk->name, 0, 1, sectorData);
  if (status < 0)
    {
      error("Couldn't write MBR sector");
      return (status);
    }

  return (status = 0);
}


static int mbrBootMenu(void)
{
  // Call the 'bootmenu' program to install a boot menu

  int status = 0;
  char command[80];

  if (table->changesPending)
    {
      error("This operation cannot be undone, and it is required that\n"
	    "you write your other changes to disk before continuing.");
      return (status = ERR_BUSY);
    }

  sprintf(command, "/programs/bootmenu %s", table->disk->name);

  status = system(command);
  if (status < 0)
    error("Error %d running bootmenu command", status);

  // Need to re-read the partition table
  return (selectDisk(table->disk));
}


static void restoreBackup(void)
{
  // Restore the backed-up partition table from a file

  int status = 0;
  char *fileName = NULL;
  fileStream backupFile;

  if (!yesOrNo("Restore old partition table from backup?"))
    return;

  // Clear stack data
  bzero(&backupFile, sizeof(fileStream));

  // Construct the file name
  fileName = malloc(MAX_PATH_NAME_LENGTH);
  if (fileName == NULL)
    return;
  sprintf(fileName, BACKUP_MBR, table->disk->name);

  // Read a backup copy of the partition tables
  status = fileStreamOpen(fileName, OPENMODE_READ, &backupFile);

  free(fileName);

  if (status < 0)
    {
      error("Error opening backup partition table file");
      return;
    }

  // Clear the raw slices in the partition table.
  bzero(table->rawSlices, (DISK_MAX_PARTITIONS * sizeof(rawSlice)));
  table->numRawSlices = 0;

  // Get the number of raw slices
  status =
    fileStreamRead(&backupFile, sizeof(int), (char *) &table->numRawSlices); 
  if (status < 0)
    {
      error("Error reading backup partition table file");
      fileStreamClose(&backupFile);
      return;
    }

  status =
    fileStreamRead(&backupFile, (table->numRawSlices * sizeof(rawSlice)),
		   (char *) table->rawSlices);

  fileStreamClose(&backupFile);

  if (status < 0)
    {
      error("Error reading backup partition table file");
      return;
    }

  // Generate the slice list from our raw slices
  makeSliceList(table);

  // Don't write it.  The user has to do that explicitly.
  table->changesPending += 1;

  return;
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


static void eventHandler(objectKey key, windowEvent *event)
{
  int count;
  objectKey selectedItem = NULL;
  int selected = -1;
  
  if (key == window)
    {
      // Check for the window being closed by a GUI event.
      if (event->type == EVENT_WINDOW_CLOSE)
	quit(0, 0);

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
	    writeChanges(table, 1);

	  else if (selectedItem == fileMenuContents.items[FILEMENU_UNDO].key)
	    undo();

	  else if (selectedItem ==
		   fileMenuContents.items[FILEMENU_RESTOREBACKUP].key)
	    restoreBackup();

	  else if (selectedItem == fileMenuContents.items[FILEMENU_QUIT].key)
	    quit(0, 0);
	}
    }

  // Disk menu
  else if ((key == diskMenu) && (event->type & EVENT_SELECTION))
    {
      windowComponentGetData(diskMenu, &selectedItem, 1);
      if (selectedItem)
	{
	  if (selectedItem == diskMenuContents.items[DISKMENU_COPYDISK].key)
	    copyDisk();

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
	    copyPartition(table->selectedSlice);

	  else if (selectedItem == partMenuContents.items[PARTMENU_PASTE].key)
	    pastePartition(table->selectedSlice);

	  else if (selectedItem ==
		   partMenuContents.items[PARTMENU_SETACTIVE].key)
	    setActive(table->selectedSlice);

	  else if (selectedItem == partMenuContents.items[PARTMENU_DELETE].key)
	    delete(table->selectedSlice);

	  else if (selectedItem == partMenuContents.items[PARTMENU_FORMAT].key)
	    format(table->selectedSlice);

	  else if (selectedItem == partMenuContents.items[PARTMENU_DEFRAG].key)
	    defragment(table->selectedSlice);

	  else if (selectedItem == partMenuContents.items[PARTMENU_RESIZE].key)
	    resize(table->selectedSlice);

	  else if (selectedItem == partMenuContents.items[PARTMENU_HIDE].key)
	    hide(table->selectedSlice);

	  else if (selectedItem == partMenuContents.items[PARTMENU_INFO].key)
	    info(table->selectedSlice);

	  else if (selectedItem ==
		   partMenuContents.items[PARTMENU_LISTTYPES].key)
	    listTypes();

	  else if (selectedItem == partMenuContents.items[PARTMENU_MOVE].key)
	    move(table->selectedSlice);

	  else if (selectedItem == partMenuContents.items[PARTMENU_CREATE].key)
	    create(table->selectedSlice);

	  else if (selectedItem ==
		   partMenuContents.items[PARTMENU_DELETEALL].key)
	    deleteAll();

	  else if (selectedItem ==
		   partMenuContents.items[PARTMENU_SETTYPE].key)
	    setType(table->selectedSlice);
	}
    }

  // Check for changes to our disk list
  else if ((key == diskList) && ((event->type & EVENT_MOUSE_DOWN) ||
				 (event->type & EVENT_KEY_DOWN)))
    {
      windowComponentGetSelected(diskList, &selected);
      if ((selected >= 0) && (selected != table->diskNumber))
	{
	  if (selectDisk(&disks[selected]) < 0)
	    windowComponentSetSelected(diskList, table->diskNumber);
	}
    }

  // Check for clicks or cursor keys on our canvas diagram
  else if ((key == canvas) &&
	   ((event->type & EVENT_MOUSE_DOWN) ||
	    (event->type & EVENT_KEY_DOWN)))
    {
      if (event->type & EVENT_MOUSE_DOWN)
	{
	  for (count = 0; count < table->numSlices; count ++)
	    if ((event->xPosition > table->slices[count].pixelX) &&
		(event->xPosition < (table->slices[count].pixelX +
				     table->slices[count].pixelWidth)))
	      {
		selected = count;
		break;
	      }

	  if (selected >= 0)
	    table->selectedSlice = selected;
	}

      else if (event->type & EVENT_KEY_DOWN)
	{
	  // Respond to cursor left or right
	  switch (event->key)
	    {
	    case 18:
	      // LEFT cursor key
	      if (table->selectedSlice)
		table->selectedSlice -= 1;
	      break;

	    case 19:
	      // RIGHT cursor key
	      if (table->selectedSlice < (table->numSlices - 1))
		table->selectedSlice += 1;
	      break;

	    default:
	      break;
	    }
	}
    }
  
  // Check for changes to our slice list
  else if ((key == sliceList) && ((event->type & EVENT_MOUSE_DOWN) ||
				  (event->type & EVENT_KEY_DOWN)))
    {
      windowComponentGetSelected(sliceList, &selected);
      if (selected >= 0)
	table->selectedSlice = selected;
    }

  // Check for button clicks

  else if ((key == writeButton) && (event->type == EVENT_MOUSE_LEFTUP))
    writeChanges(table, 1);

  else if ((key == undoButton) && (event->type == EVENT_MOUSE_LEFTUP))
    undo();

  else if ((key == setActiveButton) && (event->type == EVENT_MOUSE_LEFTUP))
    setActive(table->selectedSlice);

  else if ((key == deleteButton) && (event->type == EVENT_MOUSE_LEFTUP))
    delete(table->selectedSlice);

  else if ((key == formatButton) && (event->type == EVENT_MOUSE_LEFTUP))
    format(table->selectedSlice);

  else if ((key == defragButton) && (event->type == EVENT_MOUSE_LEFTUP))
    defragment(table->selectedSlice);

  else if ((key == hideButton) && (event->type == EVENT_MOUSE_LEFTUP))
    hide(table->selectedSlice);

  else if ((key == infoButton) && (event->type == EVENT_MOUSE_LEFTUP))
    info(table->selectedSlice);

  else if ((key == moveButton) && (event->type == EVENT_MOUSE_LEFTUP))
    move(table->selectedSlice);

  else if ((key == newButton) && (event->type == EVENT_MOUSE_LEFTUP))
    create(table->selectedSlice);

  else if ((key == deleteAllButton) && (event->type == EVENT_MOUSE_LEFTUP))
    deleteAll();

  else if ((key == resizeButton) && (event->type == EVENT_MOUSE_LEFTUP))
    resize(table->selectedSlice);

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
  params.orientationX = orient_left;
  params.orientationY = orient_middle;

  // Create a container for the disk icon image and the disk list
  container = windowNewContainer(window, "titleContainer", &params);

  params.padBottom = 5;
  params.padLeft = 5;
  params.padRight = 5;

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
	  params.flags |=
	    (WINDOW_COMPFLAG_FIXEDWIDTH | WINDOW_COMPFLAG_FIXEDHEIGHT);
	  imageComponent = windowNewImage(container, &iconImage,
					  draw_translucent, &params);
	}

      // Make a list for the disks
      params.gridX++;
      params.flags &=
	~(WINDOW_COMPFLAG_FIXEDWIDTH | WINDOW_COMPFLAG_FIXEDHEIGHT);
      diskList = windowNewList(container, windowlist_textonly, numberDisks, 1,
			       0, diskListParams, numberDisks, &params);
      windowRegisterEventHandler(diskList, &eventHandler);
      windowContextSet(diskList, diskMenu);
    }

  // Get a canvas for drawing the visual representation
  params.gridX = 0;
  params.gridY++;
  params.flags |= (WINDOW_COMPFLAG_HASBORDER | WINDOW_COMPFLAG_CANFOCUS);
  canvas = windowNewCanvas(window, canvasWidth, canvasHeight, &params);
  windowRegisterEventHandler(canvas, &eventHandler);
  windowContextSet(canvas, partMenu);

  // Put a header label over the slice list
  params.gridY++;
  params.flags &= ~(WINDOW_COMPFLAG_HASBORDER | WINDOW_COMPFLAG_CANFOCUS);
  params.flags |= WINDOW_COMPFLAG_FIXEDHEIGHT;
  params.padBottom = 0;
  fontGetDefault(&params.font);
  textLabel = windowNewTextLabel(window, sliceListHeader, &params);

  // Make a list for the slices
  params.gridY++;
  params.flags &= ~WINDOW_COMPFLAG_FIXEDHEIGHT;
  listItemParameters tmpListParams;
  for (count = 0; count < WINDOW_MAX_LABEL_LENGTH; count ++)
    tmpListParams.text[count] = ' ';
  tmpListParams.text[WINDOW_MAX_LABEL_LENGTH - 1] = '\0';
  sliceList = windowNewList(window, windowlist_textonly, 6, 1, 0,
			    &tmpListParams, 1, &params);
  windowRegisterEventHandler(sliceList, &eventHandler);
  windowContextSet(sliceList, partMenu);

  // Adjust the canvas width so that it matches the width of the slice
  // list.
  canvasWidth = windowComponentGetWidth(sliceList);
  windowComponentSetWidth(canvas, canvasWidth);

  // A container for the buttons
  params.gridY++;
  params.padBottom = 5;
  params.orientationX = orient_center;
  params.flags |= WINDOW_COMPFLAG_FIXEDHEIGHT;
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
  int topRow, bottomRow;

  // This is the main menu bit
  while (1)
    {
      // Print out the partitions
      display();

      isPartition = 0;
      isDefrag = 0;
      isHide = 0;

      if (table->slices[table->selectedSlice].raw.tag)
	{
	  isPartition = 1;

	  if (table->slices[table->selectedSlice].opFlags & FS_OP_DEFRAG)
	    isDefrag = 1;

	  if (PARTITION_TYPEID_IS_HIDEABLE(table->slices[table->selectedSlice]
					   .raw.tag) ||
	      PARTITION_TYPEID_IS_HIDDEN(table->slices[table->selectedSlice]
					 .raw.tag))
	    isHide = 1;
	}

      // Print out the menu choices.  First column.
      printf("\n");
      topRow = textGetRow();
      printf("%s%s%s%s%s%s%s%s%s%s%s%s%s",
	     (isPartition? "[A] Set active\n" : ""),
	     (table->numRawSlices? "[B] Partition order\n" : ""),
	     "[C] Copy disk\n",
	     (isPartition? "[D] Delete\n" : ""),
	     (isPartition? "[E] Copy partition\n" : ""),
	     (isPartition? "[F] Format\n" : ""),
	     (isDefrag? "[G] Defragment\n" : ""),
	     (isHide? "[H] Hide/Unhide\n" : ""),
	     "[I] Info\n",
	     "[L] List types\n",
	     (isPartition? "[M] Move\n" : ""),
	     (!isPartition? "[N] Create new\n" : ""),
	     (table->numRawSlices? "[O] Delete all\n" : ""));
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
      printf(table->backupAvailable? "[R] Restore backup\n" : "");
      textSetColumn(COL);
      printf("[S] Select disk\n");
      textSetColumn(COL);
      printf(isPartition? "[T] Set type\n" : "");
      textSetColumn(COL);
      printf(table->changesPending? "[U] Undo\n" : "");
      textSetColumn(COL);
      printf(table->changesPending? "[W] Write changes\n" : "");
      textSetColumn(COL);
      printf("[X] Write basic MBR\n");
      textSetColumn(COL);
      printf("[Y] MBR boot menu\n");
      textSetColumn(COL);
      printf(isPartition? "[Z] Resize\n" : "");
      if (bottomRow > textGetRow())
	textSetRow(bottomRow);
      textSetColumn(0);

      if (table->changesPending)
	printf("  -== %d changes pending ==-\n", table->changesPending);
      printf("-> ");

      // Construct the string of allowable options, corresponding to what is
      // shown above.
      sprintf(optionString, "%s%sCc%s%s%s%s%sIiLl%s%s%s%sQq%sSs%s%s%sXxYyZz",
	      (isPartition? "Aa" : ""),
	      (table->numRawSlices? "Bb" : ""),
	      (isPartition? "Dd" : ""),
	      (isPartition? "Ee" : ""),
	      (isPartition? "Ff" : ""),
	      (isDefrag? "Gg" : ""),
	      (isPartition? "Hh" : ""),
	      (isPartition? "Mm" : ""),
	      (!isPartition? "Nn" : ""),
	      (table->numRawSlices? "Oo" : ""),
	      ((!isPartition && clipboardSliceValid)? "Pp" : ""),
	      (table->backupAvailable? "Rr" : ""),
	      (isPartition? "Tt" : ""),
	      (table->changesPending? "Uu" : ""),
	      (table->changesPending? "Ww" : ""));

      switch (readKey(optionString, 1))
	{
	case (unsigned char) 17:
	  // Cursor up.
	  if (table->selectedSlice > 0)
	    table->selectedSlice -= 1;
	  continue;

	case (unsigned char) 20:
	  // Cursor down.
	  if (table->selectedSlice < (table->numSlices - 1))
	    table->selectedSlice += 1;
	  continue;

	case 'a':
	case 'A':
	  setActive(table->selectedSlice);
	  continue;

	case 'b':
	case 'B':
	  changePartitionOrder();
	  continue;

	case 'c':
	case 'C':
	  copyDisk();
	  continue;

	case 'd':
	case 'D':
	  delete(table->selectedSlice);
	  continue;

	case 'e':
	case 'E':
	  copyPartition(table->selectedSlice);
	  continue;

	case 'f':
	case 'F':
	  format(table->selectedSlice);
	  continue;

	case 'g':
	case 'G':
	  defragment(table->selectedSlice);
	  continue;

	case 'h':
	case 'H':
	  hide(table->selectedSlice);
	  continue;

	case 'i':
	case 'I':
	  info(table->selectedSlice);
	  continue;

	case 'l':
	case 'L':
	  listTypes();
	  continue;

	case 'm':
	case 'M':
	  move(table->selectedSlice);
	  continue;

	case 'n':
	case 'N':
	  create(table->selectedSlice);
	  continue;

	case 'o':
	case 'O':
	  deleteAll();
	  continue;

	case 'p':
	case 'P':
	  pastePartition(table->selectedSlice);
	  continue;

	case 'q':
	case 'Q':
	  return (status = 0);
	      
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
	      quit(ERR_CANCELLED, 1);
	    }
	  continue;

	case 't':
	case 'T':
	  setType(table->selectedSlice);
	  continue;

	case 'u':
	case 'U':
	  undo();
	  continue;
	  
	case 'w':
	case 'W':
	  writeChanges(table, 1);
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
	  resize(table->selectedSlice);
	  continue;

	default:
	  continue;
	}
    }
}


int main(int argc, char *argv[])
{
  int status = 0;
  char opt;
  int clearLabel = 0;
  char tmpChar[80];
  int count;

  bzero(&screen, sizeof(textScreen));

  // Are graphics enabled?
  graphics = graphicsAreEnabled();

  // Run as Partition Logic?
#ifdef PARTLOGIC
  programName = "Partition Logic";
#else
  programName = "Visopsys Disk Manager";
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
      quit(ERR_PERMISSION, 1);
    }

  // Get memory for various things

  disks = malloc(DISK_MAXDEVICES * sizeof(disk));
  table = malloc(sizeof(partitionTable));
  if ((disks == NULL) || (table == NULL))
    quit(ERR_MEMORY, 1);

  // Find out whether our temp or backup directories are on a read-only
  // filesystem
  if (!fileGetDisk(TEMP_DIR, disks) && !disks->readOnly)
    if (!fileGetDisk(BOOT_DIR, disks) && !disks->readOnly)
      readOnly = 0;

  msdosLabel = getLabelMsdos();

  // Gather the disk info
  status = scanDisks();
  if (status < 0)
    {
      if (status == ERR_NOSUCHENTRY)
	error("No hard disks registered");
      else
	error("Problem getting hard disk info");
      quit(status, 1);
    }

  if (!numberDisks)
    {
      // There are no fixed disks
      error("No fixed disks to manage.  Quitting.");
      quit(status, 1);
    }

  makeSliceListHeader();

  if (graphics)
    constructWindow();
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
	if (!strcmp(disks[count].name, argv[argc - 1]))
	  {
	    if (clearLabel)
	      {
		// If the disk is specified, we can clear the label
		sprintf(tmpChar, "Clear the partition table of disk %s?",
			disks[count].name);
		if (yesOrNo(tmpChar))
		  clearDiskLabel(&disks[count], msdosLabel);
	      }
	    
	    selectDisk(&disks[count]);
	    break;
	  }
    }

  if (table->disk == NULL)
    {
      // If we're in text mode, the user must first select a disk
      if (!graphics && (numberDisks > 1))
	{
	  status = queryDisk();
	  if (status < 0)
	    {
	      printf("\n\nNo disk selected.  Quitting.\n\n");
	      quit(status, 1);
	    }
	}
      else
	{
	  status = selectDisk(&disks[0]);
	  if (status < 0)
	    quit(status, 1);
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

  quit(status, 1);
  // Keep the compiler happy
  return(status);
}
