//
//  Visopsys
//  Copyright (C) 1998-2004 J. Andrew McLaughlin
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

#define BACKUP_MBR "/system/boot/backup-%s.mbr"
#define PERM       "You must be a privileged user to use this command."
#define PARTTYPES  "Supported partition types"

#define ENTRYOFFSET_DRV_ACTIVE    0
#define ENTRYOFFSET_START_HEAD    1
#define ENTRYOFFSET_START_CYLSECT 2
#define ENTRYOFFSET_START_CYL     3
#define ENTRYOFFSET_TYPE          4
#define ENTRYOFFSET_END_HEAD      5
#define ENTRYOFFSET_END_CYLSECT   6
#define ENTRYOFFSET_END_CYL       7
#define ENTRYOFFSET_START_LBA     8
#define ENTRYOFFSET_SIZE_LBA      12

#define COPYBUFFER_SIZE           1048576 // 1 Meg

typedef struct {
  int partition;
  int active;
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
  char string[128];
  int pixelWidth;
} slice;

static int processId = 0;
static int numberDisks = 0;
static disk diskInfo[DISK_MAXDEVICES];
static char *diskStrings[DISK_MAXDEVICES];
static disk *selectedDisk = NULL;
static unsigned char diskMBR[512];
static unsigned char originalMBR[512];
static int numberSlices;
static slice slices[9];
static int selectedSlice = 0;
static int numberPartitions = 0;
static int changesPending = 0;
static int backupAvailable = 0;
static unsigned canvasWidth = 500;
static unsigned canvasHeight = 50;

// GUI stuff
static int graphics = 0;
static objectKey window = NULL;
static objectKey menuSetActive = NULL;
static objectKey menuCopyDisk = NULL;
static objectKey menuDelete = NULL;
static objectKey menuListTypes = NULL;
static objectKey menuUndo = NULL;
static objectKey menuNew = NULL;
static objectKey menuDeleteAll = NULL;
static objectKey menuSetType = NULL;
static objectKey menuWrite = NULL;
static objectKey menuRestoreBackup = NULL;
static objectKey menuQuit = NULL;
static objectKey diskList = NULL;
static objectKey canvas = NULL;
static objectKey sliceList = NULL;
static objectKey setActiveButton = NULL;
static objectKey copyDiskButton = NULL;
static objectKey deleteButton = NULL;
static objectKey listTypesButton = NULL;
static objectKey undoButton = NULL;
static objectKey newButton = NULL;
static objectKey deleteAllButton = NULL;
static objectKey setTypeButton = NULL;
static objectKey writeButton = NULL;
static objectKey restoreBackupButton = NULL;


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
    printf("\n\n%s\n\n", output);
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
    printf("\nWARNING: %s\n", output);
}


static void getPartitionEntry(int partition, slice *entry)
{
  unsigned char *partRecord = 0;

  bzero(entry, sizeof(slice));

  // Set this pointer to the partition record in the master boot record
  partRecord = ((diskMBR + 0x01BE) + (partition * 16));

  entry->active = (partRecord[ENTRYOFFSET_DRV_ACTIVE] >> 7);
  entry->drive = (partRecord[ENTRYOFFSET_DRV_ACTIVE] & 0x7F);
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


static void setPartitionEntry(int partition, slice *entry)
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

  // Clear it
  bzero(partRecord, 16);

  partRecord[ENTRYOFFSET_DRV_ACTIVE] =
    (unsigned char) ((entry->active << 7) | (entry->drive & 0x7F));
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


static inline unsigned cylsToMb(disk *theDisk, unsigned cylinders)
{
  unsigned long tmpDiskSize = ((theDisk->heads * theDisk->sectorsPerCylinder *
				cylinders) / (1048576 / theDisk->sectorSize));
				 
  if (tmpDiskSize < 1)
    return (1);
  else
    return ((unsigned) tmpDiskSize);
}


static void makeSliceString(slice *slc)
{
  partitionType sliceType;
  int count;

  for (count = 0; count < 128; count ++)
    slc->string[count] = ' ';

  if (slc->type == 0)
    strcpy(slc->string, "  Empty space");
  else
    {
      diskGetPartType(slc->type, &sliceType);
      strcpy(slc->string, sliceType.description);
    }

  slc->string[strlen(slc->string)] = ' ';
  if (slc->active)
    sprintf((slc->string + 24), " (active)");
  slc->string[strlen(slc->string)] = ' ';
  sprintf((slc->string + 33), " %u-%u", slc->startCylinder, slc->endCylinder);
  slc->string[strlen(slc->string)] = ' ';
  sprintf((slc->string + 45), " %u",
	  cylsToMb(selectedDisk,
		   (slc->endCylinder - slc->startCylinder) + 1));
}


static void makeSliceList(void)
{
  // This function populates the list of slices

  slice partition;
  int firstPartition = 0;
  unsigned firstCylinder;
  int count1, count2;

  numberSlices = 0;

  // Loop through all the partitions and put them in our list
  for (count1 = 0; count1 < numberPartitions; count1 ++)
    {
      firstCylinder = 0xFFFFFFFF;

      for (count2 = 0; count2 < numberPartitions; count2 ++)
	{
	  getPartitionEntry(count2, &partition);
      
	  // If we have already processed this one, continue
	  if ((numberSlices > 0) &&
	      (partition.startCylinder <=
	       slices[numberSlices - 1].endCylinder))
	    continue;
	  
	  if (partition.startCylinder < firstCylinder)
	    {
	      firstPartition = count2;
	      firstCylinder = partition.startCylinder;
	    }
	}

      // Get the first partition
      getPartitionEntry(firstPartition, &partition);

      // Is there empty space between this partition and the previous slice?
      // If so, insert another slice to represent the empty space
      if (((numberSlices == 0) && (partition.startCylinder > 0)) ||
	  ((numberSlices > 0) &&
	   (partition.startCylinder >
	    (slices[numberSlices - 1].endCylinder + 1))))
	{
	  bzero(&slices[numberSlices], sizeof(slice));
	  if (numberSlices > 0)
	    slices[numberSlices].startCylinder =
	      (slices[numberSlices - 1].endCylinder + 1);
	  else
	    slices[numberSlices].startCylinder = 0;
	  slices[numberSlices].endCylinder = (partition.startCylinder - 1);
	  makeSliceString(&slices[numberSlices]);
	  numberSlices += 1;
	}

      // Now add a slice for the current partition
      bzero(&slices[numberSlices], sizeof(slice));
      slices[numberSlices].active = partition.active;
      slices[numberSlices].type = partition.type;
      slices[numberSlices].partition = firstPartition;
      slices[numberSlices].startCylinder = partition.startCylinder;
      slices[numberSlices].endCylinder = partition.endCylinder;
      makeSliceString(&slices[numberSlices]);
      numberSlices += 1;
    }

  // Is there empty space at the end of the disk?
  if ((numberPartitions == 0) ||
      (slices[numberSlices - 1].endCylinder <
       (selectedDisk->cylinders - 1)))
    {
      bzero(&slices[numberSlices], sizeof(slice));
      if (numberPartitions == 0)
	slices[numberSlices].startCylinder = 0;
      else
	slices[numberSlices].startCylinder =
	  (slices[numberSlices - 1].endCylinder + 1);
      slices[numberSlices].endCylinder = (selectedDisk->cylinders - 1);
      makeSliceString(&slices[numberSlices]);
      numberSlices += 1;
    }
}


static void scanPartitions(void)
{
  slice entry;
  int count;

  // Figure out how many partitions are present.  Loop through the partition
  // records, looking for non-zero entries
  numberPartitions = 0;
  for (count = 0; count < 4; count ++)
    {
      getPartitionEntry(count, &entry);
      
      if (entry.type == 0)
	// The "rules" say we're supposed to be finished with this physical
	// device, but that's not absolutely always the case.  We'll still
	// probably be screwed, but not as screwed as if we were to quit here.
	continue;
      
      numberPartitions++;
    }
}


static int readMBR(const char *diskName)
{
  // Read the MBR from the physical disk

  int status = 0;
  char fileName[MAX_PATH_NAME_LENGTH];
  file backupFile;

  // Clear stack data
  bzero(&backupFile, sizeof(file));

  // Read the first sector of the device
  status = diskReadAbsoluteSectors(diskName, 0, 1, diskMBR);
  if (status < 0)
    return (status);

  // Is this a valid MBR?
  if ((diskMBR[511] != (unsigned char) 0xAA) ||
      (diskMBR[510] != (unsigned char) 0x55))
    // This is not a valid master boot record.
    warning("Invalid MBR on hard disk %s.", diskName);

  // Save a copy of the original
  memcpy(originalMBR, diskMBR, 512);

  scanPartitions();
  makeSliceList();
  selectedSlice = 0;

  changesPending = 0;

  // Any backup MBR saved?  Construct the file name
  sprintf(fileName, BACKUP_MBR, diskName);
  if (!fileFind(fileName, &backupFile))
    backupAvailable = 1;
  else
    backupAvailable = 0;

  return (status = 0);
}


static int writeMBR(const char *diskName)
{
  // Write the MBR to the physical disk

  int status = 0;
  char fileName[MAX_PATH_NAME_LENGTH];
  fileStream backupFile;

  // Make sure the signature is at the end
  diskMBR[510] = (unsigned char) 0x55;
  diskMBR[511] = (unsigned char) 0xAA;

  // Clear stack data
  bzero(&backupFile, sizeof(fileStream));

  // Construct the file name
  sprintf(fileName, BACKUP_MBR, diskName);

  // Write a backup copy of the original MBR
  status = fileStreamOpen(fileName, (OPENMODE_WRITE | OPENMODE_CREATE |
				     OPENMODE_TRUNCATE), &backupFile);
  if (status >= 0)
    {
      status = fileStreamWrite(&backupFile, 512, originalMBR);
      if (status < 0)
	warning("Error writing backup MBR file");
      else
	backupAvailable = 1;
      fileStreamClose(&backupFile);
    }
  else
    warning("Error opening backup MBR file");

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
    // Print disk info
    printf("  %s\n", diskStrings[count]);
}


static int selectDisk(int diskNumber)
{
  int status = 0;
  char tmpChar[80];
  int count;

  if (changesPending)
    {
      sprintf(tmpChar, "Discard changes to disk %d?",
	      selectedDisk->deviceNumber);
      if (!yesOrNo(tmpChar))
	{
	  if (graphics)
	    {
	      // Re-select the old disk
	      for (count = 0; count < numberDisks; count ++)
		if (&diskInfo[count] == selectedDisk)
		  {
		    windowComponentSetSelected(diskList, count);
		    break;
		  }
	    }
	  return (status = 0);
	}
    }

  selectedDisk = NULL;

  for (count = 0; count < numberDisks; count ++)
    if (diskInfo[count].deviceNumber == diskNumber)
      {
	selectedDisk = &diskInfo[count];
	windowComponentSetSelected(diskList, count);
	break;
      }

  if (selectedDisk == NULL)
    return (status = ERR_NOSUCHENTRY);

  status = readMBR(selectedDisk->name);
  if (status < 0)
    return (status);

  selectedSlice = 0;

  return (status = 0);
}


static int queryDisk(void)
{
  int diskNumber = 0;
  char character[2];

  printf("\n");
  printDisks();

  while(1)
    {
      printf("Please choose the disk on which to operate ('Q' to quit):\n-> ");
      
      character[0] = readKey("0123456789Qq", 0);
      if ((character[0] == 0) ||
	  (character[0] == 'Q') || (character[0] == 'q'))
	return (ERR_INVALID);
      character[1] = '\0';
	
      diskNumber = atoi(character);
      
      selectDisk(diskNumber);
      if (selectedDisk != NULL)
	break;
      
      printf("%d is not a valid disk number.\n", diskNumber);
    }

  return (0);
}


static void drawDiagram(void)
{
  // Draw a picture of the disk layout on our 'canvas' component

  int needPixels = 0;
  int xCoord = 0;
  windowDrawParameters params;
  color colors[] = {
    { 255, 0, 0 },   // Blue
    { 0, 255, 0 },   // Green
    { 0, 255, 255 }, // Yellow
    { 0, 0, 255 }    // Red
  };
  int count1, count2;

  // Clear our drawing parameters
  bzero(&params, sizeof(windowDrawParameters));
  
  // Some basic drawing values which are the same for all partition rectangles
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
  #define MIN_WIDTH 10
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
      // If it is a used slice, we draw a filled rectangle on the canvas to
      // represent the partition.
      if (slices[count1].type)
	{
	  // Draw the rectangle to represent this partition.
	  params.mode = draw_normal;
	  params.foreground.red = colors[slices[count1].partition].red;
	  params.foreground.green = colors[slices[count1].partition].green;
	  params.foreground.blue = colors[slices[count1].partition].blue;
	  params.xCoord1 = xCoord;
	  params.yCoord1 = 0;
	  params.width = slices[count1].pixelWidth;
	  params.height = canvasHeight;
	  params.fill = 1;
	  windowComponentSetData(canvas, &params, 1);
	}

      // If this is the selected slice, draw a border inside it
      if (count1 == selectedSlice)
	{
	  params.mode = draw_xor;
	  params.foreground.red = 200;
	  params.foreground.green = 200;
	  params.foreground.blue = 200;
	  params.xCoord1 = (xCoord + 2);
	  params.yCoord1 = 2;
	  params.width = (slices[count1].pixelWidth - 4);
	  params.height = (canvasHeight - 4);
	  params.fill = 0;
	  windowComponentSetData(canvas, &params, 1);
	}

      xCoord += slices[count1].pixelWidth;
    }
}


static void printBanner(void)
{
  // Print a message
  textScreenClear();
  printf("Visopsys FDISK Utility\nCopyright (C) 1998-2004 J. Andrew "
	 "McLaughlin\n");
}


static void display(void)
{
  char *sliceStrings[9];
  int slc = 0;
  int foregroundColor = textGetForeground();
  int backgroundColor = textGetBackground();
  int count;

  if (!graphics)
    {
      printBanner();
      printf("\nPartitions on disk %d:              cylinders   size (Mb) \n",
	     selectedDisk->deviceNumber);
    }

  for (count = 0; count < numberSlices; count ++)
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
      if (slices[selectedSlice].type)
	{
	  // It's a partition
	  windowComponentSetEnabled(setActiveButton, 1);
	  windowComponentSetEnabled(menuSetActive, 1);
	  windowComponentSetEnabled(deleteButton, 1);
	  windowComponentSetEnabled(menuDelete, 1);
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
	  windowComponentSetEnabled(newButton, 1);
	  windowComponentSetEnabled(menuNew, 1);
	  windowComponentSetEnabled(setTypeButton, 0);
	  windowComponentSetEnabled(menuSetType, 0);
	}

      // Other buttons enabled/disabled...
      windowComponentSetEnabled(deleteAllButton, numberPartitions);
      windowComponentSetEnabled(menuDeleteAll, numberPartitions);
      windowComponentSetEnabled(restoreBackupButton, backupAvailable);
      windowComponentSetEnabled(menuRestoreBackup, backupAvailable);
      windowComponentSetEnabled(undoButton, changesPending);
      windowComponentSetEnabled(menuUndo, changesPending);
      windowComponentSetEnabled(writeButton, changesPending);
      windowComponentSetEnabled(menuWrite, changesPending);
    }
  else
    {
      // Print info about the partitions
      for (slc = 0; slc < numberSlices; slc ++)
	{
	  if (slc == selectedSlice)
	    {
	      // Reverse the colors
	      textSetForeground(backgroundColor);
	      textSetBackground(foregroundColor);
	    }
	  
	  printf(" %s\n", sliceStrings[slc]);
	  
	  if (slc == selectedSlice)
	    {
	      // Restore the colors
	      textSetForeground(foregroundColor);
	      textSetBackground(backgroundColor);
	    }
	}
    }
}


static void setActive(int newActive)
{
  slice entry;
  int partition = 0;

  if (newActive < 0)
    return;

  // Loop through the partition records
  for (partition = 0; partition < numberPartitions; partition ++)
    {
      getPartitionEntry(partition, &entry);

      if (entry.type == 0)
	// The "rules" say we must be finished with this physical device.
	break;

      if (partition == newActive)
	{
	  if (!(entry.active))
	    {
	      // Set active flag
	      entry.active = 1;
	      changesPending++;
	    }
	}
      else
	// Unset active flag
	entry.active = 0;

      setPartitionEntry(partition, &entry);
    }

  // Regenerate the slice list
  makeSliceList();
}


static void delete(int partition)
{
  slice entries[4];
  int count1, count2;

  if (partition < 0)
    return;

  // Get all the entries
  for (count1 = 0; count1 < numberPartitions; count1 ++)
    getPartitionEntry(count1, &entries[count1]);

  if (entries[partition].active)
    warning("Deleting active partition.  You should set another partition "
	    "active.");

  // Shift the remaining ones forward
  count2 = 0;
  for (count1 = 0; count1 < numberPartitions; count1 ++)
    if (count1 != partition)
      setPartitionEntry(count2++, &entries[count1]);
  numberPartitions -= 1;

  // For the deleted one, set the type to 0 and put it at the end
  entries[partition].type = 0;
  entries[partition].active = 0;
  setPartitionEntry(numberPartitions, &entries[partition]);

  // Regenerate the slice list
  makeSliceList();

  if (selectedSlice > (numberSlices - 1))
    selectedSlice = (numberSlices - 1);    

  changesPending++;
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
	    windowNewTextArea(typesWindow, 60, ((numberTypes / 2) + 2), NULL,
			      &params);

	  // Make a dismiss button
	  params.gridY = 1;
	  params.padBottom = 5;
	  dismissButton =
	    windowNewButton(typesWindow, "Dismiss", NULL, &params);
	  windowComponentFocus(dismissButton);

	  // Save old text output
	  oldOutput = multitaskerGetTextOutput();

	  // Set the text output to our new area
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
	  if ((status > 0) && (event.type == EVENT_MOUSE_UP))
	    break;
	}
      windowDestroy(typesWindow);
    }

  else
    {
      printf("\nPress any key to continue. ");
      getchar();
    }
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

  if (partition < 0)
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
	  error("Unsupported partition type %x", newCode);
	  return (status = ERR_INVALID);
	}

      // Load the partition
      getPartitionEntry(partition, &entry);

      // Change the value
      entry.type = newCode;

      // Set the partition
      setPartitionEntry(partition, &entry);

      changesPending++;
      return (status = 0);
    }
}


static void undo(void)
{
  // Undo changes
  if (changesPending)
    {
      memcpy(diskMBR, originalMBR, 512);

      selectedSlice = 0;

      scanPartitions();

      // Regenerate the slice list
      makeSliceList();

      changesPending = 0;
    }
}


static void write(void)
{
  int status = 0;
  static char *message = "CHANGES WRITTEN.";

  if (changesPending)
    {
      if (!yesOrNo("Committing changes to disk.  Are you SURE?"))
	return;

      // Write out the MBR
      status = writeMBR(selectedDisk->name);

      diskSync();

      if (status < 0)
	error("Unable to write the MBR sector of the device.");

      else
	{
	  if (graphics)
	    windowNewInfoDialog(window, "Success", message);
	  else
	    printf("\n%s\n\n", message);
	}
    }
}


static void create(void)
{
  enum { units_normal, units_mb, units_cylsize } units = units_normal;

  int status = 0;
  char tmpChar[256];
  char number[10];
  slice newEntry;
  int count;

  if (numberPartitions >= 4)
    {
      error("The partition table is full");
      return;
    }

  bzero(&newEntry, sizeof(slice));
  newEntry.drive = selectedDisk->deviceNumber;
  newEntry.type = 0x01;

  while (1)
    {
      sprintf(tmpChar, "Enter starting cylinder:\n(%u-%u)",
	      slices[selectedSlice].startCylinder,
	      slices[selectedSlice].endCylinder);

      if (graphics)
	{
	  newEntry.startCylinder =
	    getNumberDialog("Starting cylinder", tmpChar);
	  if ((int) newEntry.startCylinder < 0)
	    return;
	}
      else
	{
	  printf("\n%s or 'Q' to quit\n-> ", tmpChar);
	  
	  status = readLine("0123456789Qq", number, 10);
	  if (status < 0)
	    continue;
	  
	  if ((number[0] == 'Q') || (number[0] == 'q'))
	    return;
	  
	  newEntry.startCylinder = atoi(number);
	}

      // Make sure the start cylinder is legit
      if ((newEntry.startCylinder < slices[selectedSlice].startCylinder) ||
	  (newEntry.startCylinder > slices[selectedSlice].endCylinder))
	{
	  error("Starting cylinder is not in unallocated space");
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
      sprintf(tmpChar, "Enter ending cylinder:\n(%u-%u), or size in "
	      "megabytes with 'm' (1m-%um),\nor size in cylinders with 'c' "
	      "(1c-%uc)", newEntry.startCylinder,
	      slices[selectedSlice].endCylinder,
	      cylsToMb(selectedDisk,
		       (slices[selectedSlice].endCylinder -
			newEntry.startCylinder + 1)),
	      (slices[selectedSlice].endCylinder -
	       newEntry.startCylinder + 1));

      if (graphics)
	{
	  status = windowNewPromptDialog(window, "Ending cylinder", tmpChar,
					 1, 10, number);
	  if ((status < 0) || (number[0] == '\0'))
	    return;
	}
      else
	{
	  printf("\n%s, or 'Q' to quit:\n-> ", tmpChar);
      
	  status = readLine("0123456789CcMmQq", number, 10);
	  if (status < 0)
	    return;
	}

      if ((number[0] == 'Q') || (number[0] == 'q'))
	return;

      count = (strlen(number) - 1);

      if ((number[count] == 'M') || (number[count] == 'm'))
	{
	  units = units_mb;
	  number[count] = '\0';
	}
      else if ((number[count] == 'C') || (number[count] == 'c'))
	{
	  units = units_cylsize;
	  number[count] = '\0';
	}

      count = atoi(number);

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

      if ((newEntry.endCylinder < newEntry.startCylinder) ||
	  (newEntry.endCylinder > slices[selectedSlice].endCylinder))
	{
	  error("Invalid cylinder number");
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

      // Regenerate the slice list
      makeSliceList();

      // Select our new slice
      for (count = 0; count < numberSlices; count ++)
	if (slices[count].partition == (numberPartitions - 1))
	  {
	    selectedSlice = count;
	    break;
	  }
    }
  else
    {
      newEntry.type = 0;
      setPartitionEntry(numberPartitions, &newEntry);
    }

  return;
}


static void deleteAll(void)
{
  slice delEntry;
  int count;

  // Delete by setting the type = 0
  for (count = 0; count < numberPartitions; count ++)
    {
      getPartitionEntry(count, &delEntry);
      delEntry.type = 0;
      delEntry.active = 0;
      setPartitionEntry(count, &delEntry);
    }

  numberPartitions = 0;

  // Regenerate the slice list
  makeSliceList();

  selectedSlice = 0;

  changesPending++;
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
  unsigned char *buffer = NULL;
  unsigned srcSectorsPerOp = 0;
  unsigned destSectorsPerOp = 0;
  unsigned srcSector = 0;
  unsigned destSector = 0;
  char tmpChar[160];
  int count1, count2;

  // Stuff for a progress dialog when in graphics mode
  objectKey dialogWindow = NULL;
  componentParameters params;
  objectKey textLabel = NULL;
  objectKey progressBar = NULL;

  bzero(&params, sizeof(componentParameters));

  if (numberDisks < 2)
    {
      error("No other disks to copy to");
      return (status = ERR_NOSUCHENTRY);
    }

  srcDisk = selectedDisk;

  while(1)
    {
      strcpy(tmpChar, "Please choose the disk to copy to");
      if (graphics)
	{
	  diskNumber = getNumberDialog("Destination disk", tmpChar);
	  if (diskNumber < 0)
	    return (diskNumber);
	}
      else
	{
	  printf("\n%s ('Q' to quit):\n", tmpChar);
	  printDisks();
	  printf("\n->");
      
	  character[0] = readKey("0123456789Qq", 0);
	  if ((character[0] == 0) ||
	      (character[0] == 'Q') || (character[0] == 'q'))
	    return (status = 0);
	  character[1] = '\0';
      
	  diskNumber = atoi(character);
	}

      // Loop through the disks and make sure it's legit
      for (count1 = 0; count1 < numberDisks; count1 ++)
	if (diskInfo[count1].deviceNumber == diskNumber)
	  destDisk = &diskInfo[count1];
      
      if (destDisk == srcDisk)
	{
	  error("Not much point in copying a disk to itself!");
	  continue;
	}

      else if (destDisk != NULL)
	break;
      
      error("Invalid disk %d.", diskNumber);
    }

  // We have a source disk and a destination disk.
  sprintf(tmpChar, "Copy disk %s to disk %s.\nWARNING: THIS WILL DESTROY ALL "
	  "DATA ON DISK %s.\nARE YOU SURE YOU WANT TO DO THIS?", srcDisk->name,
	  destDisk->name, destDisk->name);
  if (!yesOrNo(tmpChar))
    return (status = 0);
  printf("\n");

  // We will copy everything up to the end of the last partition (not much
  // point in copying a bunch of unused space, even though it's potentially
  // conceivable that someone, somewhere might want to do that).  Find out
  // the logical sector number of the end of the last partition.
  for (count1 = 0; count1 < numberPartitions; count1 ++)
    {
      getPartitionEntry(count1, &entry);

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
	      "%s.\nIf you wish, you can continue and copy the data that "
	      "will\nfit.  Don't do this unless you're sure you know what "
	      "you're\ndoing.  CONTINUE?", destDisk->name, srcDisk->name);
      if (!yesOrNo(tmpChar))
	return (status = 0);
      printf("\n");

      lastUsedSector = (destDisk->numSectors - 1);
    }

  // Get a decent memory buffer to copy data to/from
  buffer = memoryGet(COPYBUFFER_SIZE, "disk copy buffer");
  if (buffer == NULL)
    {
      error("Unable to allocate memory buffer!");
      return (status = ERR_MEMORY);
    }

  // Calculate the sectors per operation for each disk
  srcSectorsPerOp = (COPYBUFFER_SIZE / srcDisk->sectorSize);
  destSectorsPerOp = (COPYBUFFER_SIZE / destDisk->sectorSize);

  sprintf(tmpChar, "Copying %u sectors, %u Mb", (lastUsedSector + 1),
	  ((lastUsedSector + 1) / (1048576 / srcDisk->sectorSize)));

  if (graphics)
    {
      dialogWindow = windowNewDialog(window, "Copying...");
      if (dialogWindow != NULL)
	{
	  params.gridWidth = 1;
	  params.gridHeight = 1;
	  params.padTop = 5;
	  params.padLeft = 5;
	  params.padRight = 5;
	  params.orientationX = orient_center;
	  params.orientationY = orient_middle;
	  params.useDefaultForeground = 1;
	  params.useDefaultBackground = 1;
	  textLabel = windowNewTextLabel(dialogWindow, NULL, tmpChar, &params);

	  params.gridY = 1;
	  params.padBottom = 5;
	  progressBar = windowNewProgressBar(dialogWindow, &params);

	  windowSetHasCloseButton(dialogWindow, 0);
	  windowSetResizable(dialogWindow, 0);
	  windowCenterDialog(window, dialogWindow);
	  windowSetVisible(dialogWindow, 1);
	}
    }
  else
    printf("\n%s\n", tmpChar);

  // Copy the data
  while (srcSector < lastUsedSector)
    {
      // Show a progress indicator
      if (graphics)
	windowComponentSetData(progressBar,
			       (void *)((srcSector * 100) / lastUsedSector),
			       1);
      else
	{
	  textSetColumn(2);
	  printf("%d%% ", ((srcSector * 100) / lastUsedSector));
	}

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
	  error("Read error %d reading sectors %u-%u from disk %s",
		status, srcSector, (srcSector + srcSectorsPerOp - 1),
		srcDisk->name);
	  memoryRelease(buffer);
	  return (status);
	}

      // Write to destination
      status = diskWriteAbsoluteSectors(destDisk->name, destSector, 
					destSectorsPerOp, buffer);
      if (status < 0)
	{
	  error("Write error %d writing sectors %u-%u to disk %s",
		status, destSector, (destSector + destSectorsPerOp - 1),
		destDisk->name);
	  memoryRelease(buffer);
	  return (status);
	}

      srcSector += srcSectorsPerOp;
      destSector += destSectorsPerOp;
    }

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

  // Release temporary memory
  memoryRelease(buffer);

  // Now, if any partitions  are ouside (or partially outside) the 
  // bounds of the destination disk, delete (or truncate) 
  for (count1 = 0; count1 < numberDisks; count1 ++)
    {
      if (&diskInfo[count1] == destDisk)
	{
	  selectDisk(diskInfo[count1].deviceNumber);

	  for (count2 = 0; count2 < numberPartitions; count2 ++)
	    {
	      getPartitionEntry(count2, &entry);
    
	      // Starts past the end of the disk
	      if (entry.startCylinder >= destDisk->cylinders)
		{
		  delete(count2);
		  count2 -= 1;
		}
	      else if (entry.endCylinder >= destDisk->cylinders)
		{
		  entry.endCylinder = (destDisk->cylinders - 1);
		  entry.endHead = (destDisk->heads - 1);
		  entry.endSector = destDisk->sectorsPerCylinder;
		  entry.sizeLogical =   
		    ((((entry.endCylinder - entry.startCylinder) + 1) *
		      (destDisk->heads * destDisk->sectorsPerCylinder)) -
		     (entry.startHead * destDisk->sectorsPerCylinder));
		  setPartitionEntry(count2, &entry);
		}
	    }

	  // Write out the MBR
	  status = writeMBR(destDisk->name);
	  diskSync();
	  if (status < 0)
	    error("Unable to write the MBR sector of the device.");
	  display();

	  break;
	}
    }
  
  return (status = 0);
}


static void restoreBackup(const char *diskName)
{
  // Restore the backed-up MBR from a file

  int status = 0;
  fileStream backupFile;
  char fileName[MAX_PATH_NAME_LENGTH];

  if (!yesOrNo("Restore old MBR from backup?"))
    return;

  // Clear stack data
  bzero(&backupFile, sizeof(fileStream));

  // Construct the file name
  sprintf(fileName, BACKUP_MBR, diskName);

  // Read a backup copy of the MBR
  status = fileStreamOpen(fileName, OPENMODE_READ, &backupFile);
  if (status >= 0)
    {
      status = fileStreamRead(&backupFile, 512, diskMBR);
      if (status < 0)
	warning("Error reading backup MBR file");

      fileStreamClose(&backupFile);
    }
  else
    warning("Error opening backup MBR file");

  // Regenerate the slice list
  scanPartitions();
  makeSliceList();

  // Don't write it.  The user has to do that explicitly.
  changesPending++;

  return;
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
      windowDestroy(window);
    }
  else
    {
      printf("\nQuitting.\n");
      //textScreenRestore();
    }

  exit(0);
}


static void eventHandler(objectKey key, windowEvent *event)
{
  // Check for the window being closed by a GUI event.
  if (((key == window) && (event->type == EVENT_WINDOW_CLOSE)) ||
      ((key == menuQuit) && (event->type == EVENT_MOUSE_UP)))
    {
      quit();
      return;
    }

  // Check for changes to our disk list
  else if ((key == diskList) && (event->type == EVENT_MOUSE_DOWN))
    {
      int newSelected = windowComponentGetSelected(diskList);
      if (newSelected >= 0) 
	selectDisk(diskInfo[newSelected].deviceNumber);
    }

  // Check for changes to our slice list
  else if ((key == sliceList) && (event->type == EVENT_MOUSE_DOWN))
    {
      int selected = windowComponentGetSelected(sliceList);
      if (selected >= 0)
	selectedSlice = selected;
    }
  
  else if (((key == setActiveButton) || (key == menuSetActive)) &&
	   (event->type == EVENT_MOUSE_UP))
    {
      if (slices[selectedSlice].type)
	setActive(slices[selectedSlice].partition);
    }

  else if (((key == copyDiskButton) || (key == menuCopyDisk)) &&
	   (event->type == EVENT_MOUSE_UP))
    {
      if (copyDisk() < 0)
	error("Disk copy failed.");
    }

  else if (((key == deleteButton) || (key == menuDelete)) &&
	   (event->type == EVENT_MOUSE_UP))
    {
      if (slices[selectedSlice].type)
	delete(slices[selectedSlice].partition);
    }

  else if (((key == listTypesButton) || (key == menuListTypes)) &&
	   (event->type == EVENT_MOUSE_UP))
    listTypes();

  else if (((key == undoButton) || (key == menuUndo)) && 
	   (event->type == EVENT_MOUSE_UP))
    undo();

  else if (((key == newButton) || (key == menuNew)) &&
	   (event->type == EVENT_MOUSE_UP))
    create();

  else if (((key == deleteAllButton) || (key == menuDeleteAll)) &&
	   (event->type == EVENT_MOUSE_UP))
    deleteAll();

  else if (((key == setTypeButton) || (key == menuSetType)) &&
	   (event->type == EVENT_MOUSE_UP))
    {
      if (slices[selectedSlice].type)
	setType(slices[selectedSlice].partition);
    }

  else if (((key == writeButton) || (key == menuWrite)) &&
	   (event->type == EVENT_MOUSE_UP))
    write();

  else if (((key == restoreBackupButton) || (key == menuRestoreBackup)) &&
	   (event->type == EVENT_MOUSE_UP))
    restoreBackup(selectedDisk->name);

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
  objectKey titleContainer = NULL;
  objectKey imageComponent = NULL;
  objectKey textLabel = NULL;
  objectKey systemFont = NULL;

  // Create a new window, with small, arbitrary size and location
  window = windowNew(processId, "Disk Manager");
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

  // Create the top 'partition' menu
  objectKey menu3 = windowNewMenu(menuBar, "Partition", &params);
  menuNew = windowNewMenuItem(menu3, "New", &params);
  windowRegisterEventHandler(menuNew, &eventHandler);
  menuSetActive = windowNewMenuItem(menu3, "Set active", &params);
  windowRegisterEventHandler(menuSetActive, &eventHandler);
  menuSetType = windowNewMenuItem(menu3, "Set type", &params);
  windowRegisterEventHandler(menuSetType, &eventHandler);
  menuListTypes = windowNewMenuItem(menu3, "List types", &params);
  windowRegisterEventHandler(menuListTypes, &eventHandler);
  menuDelete = windowNewMenuItem(menu3, "Delete", &params);
  windowRegisterEventHandler(menuDelete, &eventHandler);
  menuDeleteAll = windowNewMenuItem(menu3, "Delete all", &params);
  windowRegisterEventHandler(menuDeleteAll, &eventHandler);

  // Create a container for the disk icon image and the title label
  params.gridY = 1;
  params.gridWidth = 5;
  titleContainer = windowNewContainer(window, "titleContainer", &params);
  if (titleContainer != NULL)
    {
      if (iconImage.data == NULL)
	// Try to load an icon image to go at the top of the window
	status = imageLoadBmp("/system/diskicon.bmp", &iconImage);
      if (status == 0)
	{
	  // Create an image component from it, and add it to the container
	  iconImage.translucentColor.red = 0;
	  iconImage.translucentColor.green = 255;
	  iconImage.translucentColor.blue = 0;
	  params.gridWidth = 1;
	  imageComponent = windowNewImage(titleContainer, &iconImage,
					  draw_translucent, &params);
	}

      // Put a title text label in the container
      params.gridX = 1;
      textLabel = windowNewTextLabel(titleContainer, NULL,
				     "Visopsys Disk Manager", &params);
    }

  // Make a list for the disks
  params.gridX = 0;
  params.gridY = 2;
  params.gridWidth = 5;
  diskList = windowNewList(window, NULL, numberDisks, 1, 0, diskStrings,
			   numberDisks, &params);
  windowRegisterEventHandler(diskList, &eventHandler);

  // Get a canvas for drawing the visual representation
  params.gridY = 3;
  params.hasBorder = 1;
  canvas = windowNewCanvas(window, canvasWidth, canvasHeight, &params);

  fontGetDefault(&systemFont);

  params.gridY = 4;
  params.hasBorder = 0;
  textLabel = windowNewTextLabel(window, systemFont, "Partitions           "
				 "             cylinders   size (Mb)",
				 &params);

  // Make a list for the partitions
  params.gridY = 5;
  params.padTop = 0;
  sliceList = windowNewList(window, systemFont, 5, 1, 0, (char *[])
      { "                                                                 " },
			    1, &params);
  windowRegisterEventHandler(sliceList, &eventHandler);
  // Set the canvas width, above, to have the same width as this component.
  // canvasWidth = windowComponentGetWidth(sliceList);
  // windowComponentSetWidth(canvas, canvasWidth);

  params.gridX = 0;
  params.gridY = 6;
  params.gridWidth = 1;
  params.padTop = 5;
  params.padBottom = 0;
  newButton = windowNewButton(window, "New", NULL, &params);
  windowRegisterEventHandler(newButton, &eventHandler);

  params.gridX = 1;
  setTypeButton = windowNewButton(window, "Set type", NULL, &params);
  windowRegisterEventHandler(setTypeButton, &eventHandler);

  params.gridX = 2;
  copyDiskButton = windowNewButton(window, "Copy disk", NULL, &params);
  windowRegisterEventHandler(copyDiskButton, &eventHandler);

  params.gridX = 3;
  deleteAllButton = windowNewButton(window, "Delete all", NULL, &params);
  windowRegisterEventHandler(deleteAllButton, &eventHandler);

  params.gridX = 4;
  writeButton = windowNewButton(window, "Write changes", NULL, &params);
  windowRegisterEventHandler(writeButton, &eventHandler);

  params.gridX = 0;
  params.gridY = 7;
  params.padTop = 0;
  params.padBottom = 5;
  setActiveButton = windowNewButton(window, "Set active", NULL, &params);
  windowRegisterEventHandler(setActiveButton, &eventHandler);

  params.gridX = 1;
  listTypesButton = windowNewButton(window, "List types", NULL, &params);
  windowRegisterEventHandler(listTypesButton, &eventHandler);

  params.gridX = 2;
  deleteButton = windowNewButton(window, "Delete", NULL, &params);
  windowRegisterEventHandler(deleteButton, &eventHandler);

  params.gridX = 3;
  undoButton = windowNewButton(window, "Undo", NULL, &params);
  windowRegisterEventHandler(undoButton, &eventHandler);

  params.gridX = 4;
  restoreBackupButton = windowNewButton(window, "Restore backup", NULL,
					&params);
  windowRegisterEventHandler(restoreBackupButton, &eventHandler);


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
  char character;

  // This is the main menu bit
  while (1)
    {
      // Print out the partitions
      display();

      // Print out the menu choices
      printf("\n%s%s%s%s%s%s%s%s%s%s%s%s",
	     ((slices[selectedSlice].type && !slices[selectedSlice].active)?
	      "[A] Set active\n" : ""),
	     "[C] Copy disk\n",
	     (slices[selectedSlice].type? "[D] Delete\n" : ""),
	     "[L] List types\n",
	     (!slices[selectedSlice].type? "[N] New\n" : ""),
	     (numberPartitions? "[O] Delete all\n" : ""),
	     "[Q] Quit\n",
	     "[S] Select disk\n",
	     (slices[selectedSlice].type? "[T] Set type\n" : ""),
	     (changesPending? "[U] Undo\n" : ""),
	     (changesPending? "[W] Write changes\n" : ""),
	     (backupAvailable? "[R] Restore backup\n" : ""));

      if (changesPending)
	printf("  -== %d changes pending ==-\n", changesPending);
      printf("-> ");

      // Construct the string of allowable options, corresponding to what is
      // shown above.
      sprintf(optionString, "%sCc%sLl%s%s%sSsQq%s%s%s",
	      ((slices[selectedSlice].type && !slices[selectedSlice].active)?
	      "Aa" : ""),
	      (slices[selectedSlice].type? "Dd" : ""),
	      (!slices[selectedSlice].type? "Nn" : ""),
	      (numberPartitions? "Oo" : ""),
	      (backupAvailable? "Rr" : ""),
	      (slices[selectedSlice].type? "Tt" : ""),
	      (changesPending? "Uu" : ""),
	      (changesPending? "Ww" : ""));

      character = readKey(optionString, 1);
      if (character == 0)
	continue;

      switch (character)
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
	  setActive(slices[selectedSlice].partition);
	  continue;

	case 'c':
	case 'C':
	  status = copyDisk();
	  if (status < 0)
	    error("Disk copy failed.");
	  continue;

	case 'd':
	case 'D':
	  delete(slices[selectedSlice].partition);
	  continue;

	case 'l':
	case 'L':
	  listTypes();
	  continue;

	case 'n':
	case 'N':
	  create();
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
	  restoreBackup(selectedDisk->name);
	  continue;
	      
	case 's':
	case 'S':
	  status = queryDisk();
	  if (status < 0)
	    {
	      error("No disk selected.  Quitting.");
	      quit();
	    }
	  continue;

	case 't':
	case 'T':
	  setType(slices[selectedSlice].partition);
	  continue;

	case 'u':
	case 'U':
	  undo();
	  continue;
	  
	case 'w':
	case 'W':
	  write();
	  continue;
	  
	default:
	  continue;
	}
    }
}


int main(int argc, char *argv[])
{
  int status = 0;
  int count;

  processId = multitaskerGetCurrentProcessId();

  // Are graphics enabled?
  graphics = graphicsAreEnabled();

  // Get memory for strings about disks and partitions
  char *tmp = malloc(DISK_MAXDEVICES * 128);
  for (count = 0; count < DISK_MAXDEVICES; count ++)
    diskStrings[count] = (tmp + (count * 128));

  // Check privilege level
  if (multitaskerGetProcessPrivilege(processId) != 0)
    {
      printf("\n%s\n(Try logging in as user \"admin\")\n\n", PERM);
      if (graphics)
	windowNewErrorDialog(NULL, "Permission Denied", PERM);
      return (errno = ERR_PERMISSION);
    }

  // Gather the disk info
  status = scanDisks();
  if (status < 0)
    {
      if (status == ERR_NOSUCHENTRY)
	error("No hard disks registered");
      else
	error("Problem getting hard disk info");
      return (errno = status);
    }

  if (!numberDisks)
    {
      // There are no fixed disks
      error("No fixed disks to manage.  Quitting.");
      errno = status;
      return (status);
    }

  if (!graphics)
    {
      //textScreenSave();
      printBanner();
    }

  // If we're in text mode, the user must first select a disk
  if (!graphics && (numberDisks > 1))
    {
      status = queryDisk();
      if (status < 0)
	{
	  printf("\n\nNo disk selected.  Quitting.\n\n");
	  return (errno = status);
	}
    }
  else
    selectedDisk = &diskInfo[0];

  // Read the MBR of the device
  status = readMBR(selectedDisk->name);
  if (status < 0)
    {
      error("Unable to read the MBR sector of the device.  Quitting.");
      errno = status;
      return (status);
    }

  if (graphics)
    {
      constructWindow();
      display();
      windowGuiRun();
      return (status = 0);
    }
  else
    return (status = textMenu());
}
