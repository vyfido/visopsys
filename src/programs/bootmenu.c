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
//  bootmenu.c
//

// This is a program for writing the Visopsys boot menu (and corresponding
// MBR sector) to the first track of the disk.

/* This is the text that appears when a user requests help about this program
<help>

 -- bootmenu --

This program will install or edit the boot loader menu.

Usage:
  bootmenu <physical disk name>

The 'bootmenu' program is interactive, and operates in both text and
graphics modes.  It allows the 'admin' user to install the Visopsys boot
loader program on a hard disk, and edit the menu options that appear.

Example:
  bootmenu hd0

This example will launch the bootmenu program to install or edit the boot
choices for the first hard disk, hd0.

</help>
*/

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/api.h>
#include <sys/vsh.h>

#define MBR_FILENAME        "/system/boot/mbr.bootmenu"
#define BOOTMENU_FILENAME   "/system/boot/bootmenu"
#define SLICESTRING_LENGTH  60
#define TITLE               "Visopsys Boot Menu Installer\n" \
                            "Copyright (C) 1998-2007 J. Andrew McLaughlin"
#define PERMISSION          "You must be a privileged user to use this " \
                             "command."
#define PARTITIONS          "Partitions on the disk:"
#define ENTRIES             "Chain-loadable entries for the boot menu:"
#define WRITTEN             "Boot menu written."
#define DEFAULTTIMEOUT      10 // Seconds

// Structures we write into the bootmenu
typedef struct {
  char string[SLICESTRING_LENGTH];
  unsigned startSector;
} entryStruct;

typedef struct {
  entryStruct entries[DISK_MAX_PRIMARY_PARTITIONS];
  int numberEntries;
  int defaultEntry;
  int timeoutSeconds;
} entryStructArray;

static int graphics = 0;
static int processId = 0;
static disk *logicalDisks = NULL;
static int numberLogical = 0;
static unsigned char *buffer = NULL;
static entryStructArray *entryArray = NULL;
static objectKey window = NULL;
static objectKey textArea = NULL;
static objectKey entryList = NULL;
static objectKey defaultButton = NULL;
static objectKey editButton = NULL;
static objectKey deleteButton = NULL;
static objectKey timeoutCheckbox = NULL;
static objectKey timeoutValueField = NULL;
static objectKey okButton = NULL;
static objectKey cancelButton = NULL;


static void usage(char *name)
{
  printf("usage:\n%s <disk name>\n", name);
  return;
}


__attribute__((format(printf, 1, 2)))
static void error(const char *format, ...)
{
  // Generic error message code for either text or graphics modes
  
  va_list list;
  char output[MAXSTRINGLENGTH];
  
  va_start(list, format);
  vsnprintf(output, MAXSTRINGLENGTH, format, list);
  va_end(list);

  if (graphics)
    windowNewErrorDialog(window, "Error", output);
  else
    printf("\n\n%s\n", output);
}


static void quit(void)
{
  // Shut everything down

  errno = 0;

  if (graphics && window)
    {
      windowGuiStop();
      // Crashing?  Eh?
      windowDestroy(window);
    }

  // Try to deallocate any memory we've allocated

  if (logicalDisks)
    free(logicalDisks);
  if (buffer)
    free(buffer);

  return;
}


static void setEntryString(int entryNumber, const char *string)
{
  int length = strlen(string);

  memset(entryArray->entries[entryNumber].string, (int) ' ',
	 SLICESTRING_LENGTH);
  entryArray->entries[entryNumber].string[SLICESTRING_LENGTH - 1] = '\0';

  strncpy(entryArray->entries[entryNumber].string, string, length);
}


static void refreshList(void)
{
  listItemParameters entryParams[DISK_MAX_PRIMARY_PARTITIONS];
  int count;

  if (graphics)
    {
      for (count = 0; count < entryArray->numberEntries; count ++)
	sprintf(entryParams[count].text, "%s%s",
		((count == entryArray->defaultEntry)? " * " : "   "),
		entryArray->entries[count].string);

      windowComponentSetData(entryList, entryParams,
			     entryArray->numberEntries);
      windowComponentSetEnabled(deleteButton, (entryArray->numberEntries > 1));
    }
}


static void editEntryLabel(int entryNumber)
{
  // Let the user edit the label string.

  objectKey dialogWindow = NULL;
  componentParameters params;
  objectKey newLabelField = NULL;
  objectKey okBut = NULL;
  objectKey cancelBut = NULL;
  char string[SLICESTRING_LENGTH];
  windowEvent event;
  int count;

  if (graphics)
    {
      // Create the dialog
      dialogWindow = windowNewDialog(window, "Edit entry label");
      if (dialogWindow == NULL)
	return;

      bzero(&params, sizeof(componentParameters));
      params.gridWidth = 2;
      params.gridHeight = 1;
      params.padLeft = 5;
      params.padRight = 5;
      params.padTop = 5;
      params.orientationX = orient_left;
      params.orientationY = orient_middle;
      windowNewTextLabel(dialogWindow, entryArray->entries[entryNumber].string,
			 &params);

      params.gridY = 1;
      params.flags = WINDOW_COMPFLAG_HASBORDER;
      newLabelField =
	windowNewTextField(dialogWindow, (SLICESTRING_LENGTH - 1), &params);

      // Create the OK button
      params.gridY = 2;
      params.gridWidth = 1;
      params.padBottom = 5;
      params.padRight = 0;
      params.orientationX = orient_right;
      params.flags |= WINDOW_COMPFLAG_FIXEDWIDTH;
      okBut = windowNewButton(dialogWindow, "OK", NULL, &params);

      // Create the Cancel button
      params.gridX = 1;
      params.padRight = 5;
      params.orientationX = orient_left;
      cancelBut = windowNewButton(dialogWindow, "Cancel", NULL, &params);

      windowCenterDialog(window, dialogWindow);
      windowSetVisible(dialogWindow, 1);
  
      while(1)
	{
	  // Check for the OK button
	  if (((windowComponentEventGet(newLabelField, &event) > 0) &&
	       (event.key == (unsigned char) 10) &&
	       (event.type == EVENT_KEY_DOWN)) ||
	      ((windowComponentEventGet(okBut, &event) > 0) &&
	       (event.type == EVENT_MOUSE_LEFTUP)))
	    break;
	  
	  // Check for window close or cancel button events
	  if (((windowComponentEventGet(dialogWindow, &event) > 0) &&
	       (event.type == EVENT_WINDOW_CLOSE)) ||
	      ((windowComponentEventGet(cancelBut, &event) > 0) &&
	       (event.type == EVENT_MOUSE_LEFTUP)))
	    {
	      windowDestroy(dialogWindow);
	      return;
	    }

	  // Done
	  multitaskerYield();
	}
  
      windowComponentGetData(newLabelField, string, (SLICESTRING_LENGTH - 1));

      windowDestroy(dialogWindow);
    }

  else
    {
      printf("Enter new label (%d characters max.):\n [%s]\n [",
	     (SLICESTRING_LENGTH - 1),
	     entryArray->entries[entryNumber].string);
      int column = textGetColumn();
      for (count = 0; count < (SLICESTRING_LENGTH - 1); count ++)
	printf(" ");
      printf("]");
      textSetColumn(column);

      textInputSetEcho(0);

      for (count = 0; count < SLICESTRING_LENGTH; count ++)
	{
	  string[count] = getchar();
      
	  if (string[count] == 10) // Newline
	    {
	      string[count] = '\0';
	      textInputSetEcho(1);

	      if (count == 0)
		// Assume they want to quit
		return;

	      break;
	    }
	
	  if (string[count] == 8) // Backspace
	    {
	      if (count > 0)
		{
		  textBackSpace();
		  count -= 2;
		}
	      else
		count -= 1;
	      continue;
	    }

	  else
	    printf("%c", string[count]);
	}

      // Reached the end of the buffer.
      string[SLICESTRING_LENGTH - 1] = '\0';
    }

  setEntryString(entryNumber, string);
  
  refreshList();

  return;
}


static void deleteEntryLabel(int entryNumber)
{
  int count;

  bzero(&entryArray->entries[entryNumber], sizeof(entryStruct));

  entryArray->numberEntries -= 1;

  if (entryNumber < entryArray->numberEntries)
    {
      for (count = entryNumber; count < entryArray->numberEntries; count ++)
	memcpy(&entryArray->entries[count], &entryArray->entries[count + 1],
	       sizeof(entryStruct));
    }
  
  refreshList();
}


static int getLogicalDisks(disk *physicalDisk)
{
  int status = 0;
  int tmpNumberLogical = diskGetCount();
  int count;

  logicalDisks = malloc(tmpNumberLogical * sizeof(disk));
  if (logicalDisks == NULL)
    {
      error("Can't get memory for the logical disk list");
      return (status = ERR_MEMORY);
    }

  status = diskGetAll(logicalDisks, (tmpNumberLogical * sizeof(disk)));
  if (status < 0)
    {
      error("Can't get the logical disk list\n");
      return (status);
    }

  // Now we need to get the list of logical disks that reside on our
  // physical disk which are also primary partitions.
  for (count = 0; ((count < tmpNumberLogical) &&
		   (numberLogical < DISK_MAX_PRIMARY_PARTITIONS)); count ++)
    if (!strncmp(logicalDisks[count].name, physicalDisk->name,
		 strlen(physicalDisk->name)))
      {
	if (logicalDisks[count].flags & DISKFLAG_PRIMARY)
	  // This logical resides on our physical disk
	  memcpy(&logicalDisks[numberLogical++], &logicalDisks[count],
		 sizeof(disk));
      }

  return (status = 0);
}


static int readBootMenu(file *theFile)
{
  int status = 0;

  // Open the boot menu program file and read it into memory
  status = fileOpen(BOOTMENU_FILENAME, OPENMODE_READ, theFile);
  if (status < 0)
    {
      error("Can't open %s file\n", BOOTMENU_FILENAME);
      return (status);
    }

  buffer = malloc(theFile->blocks * theFile->blockSize);
  if (buffer == NULL)
    {
      error("Can't get buffer memory\n");
      return (status = ERR_MEMORY);
    }

  status = fileRead(theFile, 0, theFile->blocks, buffer);
  if (status < 0)
    {
      error("Can't read %s file\n", BOOTMENU_FILENAME);
      return (status);
    }

  fileClose(theFile);
  return (status = 0);
}


static int bootable(const char *diskName)
{
  // Return 1 if the disk seems bootable
  
  int status = 0;
  unsigned char buf[512];

  status = diskReadSectors(diskName, 0, 1, &buf);
  if (status < 0)
    // Can't read the boot sector?  Fooey.
    return (0);

  // Check for boot sector signature
  if ((buf[510] == 0x55) && (buf[511] == 0xAA))
    return (1);
  else
    return (0);
}


static void printPartitions(void)
{
  int count;

  // Print out the primary partitions and info about them
  for (count = 0; count < numberLogical; count ++)
    printf("Disk %s\n  Label: %s\n  Filesystem: %s\n  Chain-loadable: "
	   "%s\n\n", logicalDisks[count].name,
	   logicalDisks[count].partType.description,
	   logicalDisks[count].fsType,
	   (bootable(logicalDisks[count].name)? "yes" : "no"));
}


static void eventHandler(objectKey key, windowEvent *event)
{
  int selected = 0;

  // Check for the window being closed by a GUI event.
  if (((key == window) && (event->type == EVENT_WINDOW_CLOSE)) ||
      ((key == cancelButton) && (event->type == EVENT_MOUSE_LEFTUP)))
    {
      quit();
      exit(0);
    }

  if ((key == timeoutCheckbox) && (event->type & EVENT_SELECTION))
    {
      windowComponentGetSelected(timeoutCheckbox, &selected);
      windowComponentSetEnabled(timeoutValueField, selected);
    }

  if ((key == editButton) && (event->type == EVENT_MOUSE_LEFTUP))
    {
      windowComponentGetSelected(entryList, &selected);
      if (selected >= 0)
	editEntryLabel(selected);
    }

  if ((key == defaultButton) && (event->type == EVENT_MOUSE_LEFTUP))
    {
      windowComponentGetSelected(entryList, &selected);
      entryArray->defaultEntry = selected;
      refreshList();
    }

  if ((key == deleteButton) && (event->type == EVENT_MOUSE_LEFTUP))
    {
      windowComponentGetSelected(entryList, &selected);
      if (selected >= 0)
	deleteEntryLabel(selected);
    }

  if ((key == okButton) && (event->type == EVENT_MOUSE_LEFTUP))
    windowGuiStop();
}


static void constructWindow(void)
{
  // If we are in graphics mode, make a window rather than operating on the
  // command line.

  componentParameters params;
  listItemParameters entryParams[DISK_MAX_PRIMARY_PARTITIONS];
  objectKey timeoutContainer = NULL;
  objectKey buttonContainer = NULL;
  char tmp[40];
  int count;

  // Create a new window, with small, arbitrary size and location
  window = windowNew(processId, "Boot Menu Installer");
  if (window == NULL)
    return;

  bzero(&params, sizeof(componentParameters));
  params.gridWidth = 1;
  params.gridHeight = 1;
  params.padTop = 10;
  params.padLeft = 5;
  params.padRight = 5;
  params.orientationX = orient_left;
  params.orientationY = orient_middle;
  windowNewTextLabel(window, PARTITIONS, &params);

  params.gridY = 1;
  params.padTop = 5;
  params.flags = WINDOW_COMPFLAG_HASBORDER;
  textArea = windowNewTextArea(window, 45, 20, 200, &params);

  // Use the text area for all our input and output
  windowSetTextOutput(textArea);
  textSetCursor(0);

  params.gridY = 2;
  params.flags = 0;
  windowNewTextLabel(window, ENTRIES, &params);

  params.gridY = 3;
  params.gridHeight = 3;
  bzero(&entryParams, (DISK_MAX_PRIMARY_PARTITIONS *
		       sizeof(listItemParameters)));
  for (count = 0; count < entryArray->numberEntries; count ++)
    sprintf(entryParams[count].text, "%s%s",
	    ((count == entryArray->defaultEntry)? " * " : "   "),
	    entryArray->entries[count].string);
  entryList =
    windowNewList(window, windowlist_textonly, DISK_MAX_PRIMARY_PARTITIONS,
		  1, 0, entryParams, entryArray->numberEntries, &params);

  params.gridX = 1;
  params.gridHeight = 1;
  editButton = windowNewButton(window, "Edit", NULL, &params);
  windowRegisterEventHandler(editButton, &eventHandler);

  params.gridY = 4;
  defaultButton = windowNewButton(window, "Default", NULL, &params);
  windowRegisterEventHandler(defaultButton, &eventHandler);

  params.gridY = 5;
  deleteButton = windowNewButton(window, "Delete", NULL, &params);
  windowRegisterEventHandler(deleteButton, &eventHandler);
  windowComponentSetEnabled(deleteButton, (entryArray->numberEntries > 1));

  params.gridX = 0;
  params.gridY = 6;
  params.flags = WINDOW_COMPFLAG_FIXEDWIDTH;
  timeoutContainer = windowNewContainer(window, "timeout container", &params);

  params.gridX = 0;
  params.gridY = 0;
  params.gridWidth = 1;
  timeoutCheckbox =
    windowNewCheckbox(timeoutContainer, "Automatically boot default selection "
		      "after (seconds)", &params);
  windowComponentSetSelected(timeoutCheckbox, 1);
  windowRegisterEventHandler(timeoutCheckbox, &eventHandler);

  params.gridX = 1;
  params.flags |= WINDOW_COMPFLAG_HASBORDER;
  timeoutValueField = windowNewTextField(timeoutContainer, 4, &params);
  sprintf(tmp, "%d", entryArray->timeoutSeconds);
  windowComponentSetData(timeoutValueField, tmp, strlen(tmp));

  params.gridX = 0;
  params.gridY = 7;
  params.gridWidth = 2;
  params.padBottom = 5;
  params.orientationX = orient_center;
  params.flags &= ~WINDOW_COMPFLAG_HASBORDER;
  buttonContainer = windowNewContainer(window, "button container", &params);

  params.gridY = 0;
  params.gridWidth = 1;
  params.padTop = 0;
  params.padBottom = 0;
  params.orientationX = orient_right;
  okButton = windowNewButton(buttonContainer, "OK", NULL, &params);
  windowRegisterEventHandler(okButton, &eventHandler);

  params.gridX = 1;
  params.orientationX = orient_left;
  cancelButton = windowNewButton(buttonContainer, "Cancel", NULL, &params);
  windowRegisterEventHandler(cancelButton, &eventHandler);

  // Register an event handler to catch window close events
  windowRegisterEventHandler(window, &eventHandler);

  // Go
  windowSetVisible(window, 1);

  return;
}


static int writeOut(unsigned numSectors, disk *physicalDisk)
{
  int status = 0;
  int selected = 0;
  char tmpChar[80];

  if (graphics)
    {
      windowComponentGetSelected(timeoutCheckbox, &selected);

      if (selected)
	{
	  windowComponentGetData(timeoutValueField, tmpChar, 5);
	  entryArray->timeoutSeconds = atoi(tmpChar);
	  if (errno || (entryArray->timeoutSeconds < 0) ||
	      (entryArray->timeoutSeconds > 999))
	    // User is a dummy.  Ignore them; no timeout.
	    entryArray->timeoutSeconds = 0;
	}
      else
	entryArray->timeoutSeconds = 0;
    }

  status = diskWriteSectors(physicalDisk->name, 1, numSectors , buffer);
  if (status < 0)
    {
      error("Can't write boot menu\n");
      return (status);
    }

  // Copy the boot menu boot sector to the MBR

  sprintf(tmpChar, "/programs/copy-mbr %s %s", MBR_FILENAME,
	  physicalDisk->name);

  status = system(tmpChar);
  if (status < 0)
    {
      error("Can't write MBR %s to %s\n", MBR_FILENAME, physicalDisk->name);
      return (status);
    }

  return (status = 0);
}


int main(int argc, char *argv[])
{
  int status = 0;
  disk theDisk;
  file theFile;
  char string[SLICESTRING_LENGTH];
  textAttrs attrs;
  int count;

  if (argc != 2)
    {
      usage(argv[0]);
      errno = EINVAL;
      return (status = -1);
    }

  bzero(&attrs, sizeof(textAttrs));

  // Are graphics enabled?
  graphics = graphicsAreEnabled();

  processId = multitaskerGetCurrentProcessId();

  // Check privilege level
  if (multitaskerGetProcessPrivilege(processId) != 0)
    {
      if (graphics)
	windowNewErrorDialog(NULL, "Permission Denied", PERMISSION);
      printf("\n%s\n(Try logging in as user \"admin\")\n\n", PERMISSION);
      return (errno = ERR_PERMISSION);
    }

  // Get the disk specified by the name
  bzero(&theDisk, sizeof(disk));
  status = diskGet(argv[1], &theDisk);
  if (status < 0)
    {
      error("Can't get disk %s\n", argv[1]);
      return (errno = status);
    }

  // Make sure it's a physical hard disk device, and not a logical disk,
  // floppy, CD-ROM, etc.
  if (!(theDisk.flags & DISKFLAG_PHYSICAL) ||
      !(theDisk.flags & DISKFLAG_HARDDISK))
    {
      error("Disk %s is not a physical hard disk device\n", theDisk.name);
      return (errno = ERR_INVALID);
    }

  // Get all of the logical disks
  status = getLogicalDisks(&theDisk);
  if (status < 0)
    {
      error("Can't get the list of logical disks");
      return (errno = status);
    }

  // Read the boot menu file into memory
  bzero(&theFile, sizeof(file));
  status = readBootMenu(&theFile);
  if (status < 0)
    {
      quit();
      return (errno = status);
    }

  // The entries come at this offset
  entryArray = (entryStructArray *)(buffer + 4);
  bzero(entryArray, sizeof(entryStructArray));
  entryArray->timeoutSeconds = DEFAULTTIMEOUT;

  // Make strings for each of the logical disks
  for (count = 0; count < numberLogical; count ++)
    if (bootable(logicalDisks[count].name))
      {
	// Make sure we move up logical disks in our list for after
	memcpy(&logicalDisks[entryArray->numberEntries],
	       &logicalDisks[count], sizeof(disk));

	sprintf(string, "\"%s\" [Filesystem: %s]",
		logicalDisks[count].partType.description,
		logicalDisks[count].fsType);

	// Set the string
	setEntryString(entryArray->numberEntries, string);

	// Set the start sector
	entryArray->entries[entryArray->numberEntries]
	  .startSector = logicalDisks[count].startSector;

	entryArray->numberEntries += 1;
      }

  if (!entryArray->numberEntries)
    {
      error("\nThere are no chain-loadable operating systems on this "
	     "disk.\n\n");
      quit();
      return (errno = ERR_NODATA);
    }

  if (graphics)
    {
      constructWindow();
      printPartitions();
      windowGuiRun();
    }
  else
    {
      textSetCursor(0);
      textInputSetEcho(0);
      int selected = 0;

      while(1)
	{
	  textScreenClear();
	  printf("%s\n", TITLE);
	  printf("\n%s\n\n", PARTITIONS);
	  printPartitions();
	  printf("\n%s\n\n", ENTRIES);
	  
	  for (count = 0; count < entryArray->numberEntries; count ++)
	    {
	      printf(" ");

	      if (count == selected)
		attrs.flags = TEXT_ATTRS_REVERSE;
	      else
		attrs.flags = 0;

	      if (count == entryArray->defaultEntry)
		textPrintAttrs(&attrs, " * ");
	      else
		textPrintAttrs(&attrs, "   ");
	      textPrintAttrs(&attrs, entryArray->entries[count].string);

	      printf("\n");
	    }

	  printf("\n  [Cursor up/down to select, 'e' edit, '*' default\n"
		 "   'd' delete, Enter to accept, 'Q' to quit]");

	  char character = getchar();

	  switch (character)
	    {
	    case (unsigned char) 10:
	      // Accept
	      textInputSetEcho(1);
	      textSetCursor(1);
	      printf("\n\n");
	      break;

	    case (unsigned char) 17:
	      // Cursor up.
	      if (selected > 0)
		selected -= 1;
	      continue;

	    case (unsigned char) 20:
	      // Cursor down.
	      if (selected < (entryArray->numberEntries - 1))
		selected += 1;
	      continue;
	      
	    case 'e':
	      textInputSetEcho(1);
	      textSetCursor(1);
	      printf("\n\n\n");
	      editEntryLabel(selected);
	      textInputSetEcho(0);
	      textSetCursor(0);
	      continue;

	    case '*':
	      entryArray->defaultEntry = selected;
	      continue;

	    case 'd':
	      if (entryArray->numberEntries > 1)
		{
		  deleteEntryLabel(selected);
		  if (selected >= entryArray->numberEntries)
		    selected -= 1;
		}
	      continue;

	    case 'q':
	    case 'Q':
	      textInputSetEcho(1);
	      textSetCursor(1);
	      printf("\n\n");
	      quit();
	      return (errno = 0);
	      
	    default:
	      continue;
	    }

	  break;
	}
    }

  // Now write the boot menu and MBR sector
  status = writeOut((theFile.blocks * (theFile.blockSize / 512)), &theDisk);
  if (status < 0)
    {
      error("Can't write boot menu or MBR\n");
      quit();
      return (errno = status);
    }

  if (graphics)
    {
      windowSetVisible(window, 0);
      windowNewInfoDialog(NULL, "Done", WRITTEN);
    }
  else
    printf("%s\n\n", WRITTEN);

  quit();

  return (0);
}
