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
//  format.c
//

// This is a program for formatting a disk

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/api.h>
#include <sys/vsh.h>

static int graphics = 0;
static int processId = 0;
static disk diskInfo[DISK_MAXDEVICES];
static int numberDisks = 0;


static int yesOrNo(char *question)
{
  char character;

  if (graphics)
    {
      if (windowNewQueryDialog(NULL, "Confirmation", question))
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
    windowNewErrorDialog(NULL, "Error", output);
  else
    {
      printf("\n\n%s\n", output);
      pause();
    }
}


static int chooseDisk(void)
{
  // This is where the user chooses the disk on which to install

  int status = 0;
  int diskNumber = -1;
  objectKey chooseWindow = NULL;
  componentParameters params;
  objectKey diskList = NULL;
  objectKey okButton = NULL;
  objectKey cancelButton = NULL;
  char *diskStrings[DISK_MAXDEVICES];
  windowEvent event;
  int count;

  #define CHOOSEDISK_STRING "Please choose the disk to format:"

  bzero(&params, sizeof(componentParameters));
  params.gridX = 0;
  params.gridY = 0;
  params.gridWidth = 2;
  params.gridHeight = 1;
  params.padTop = 5;
  params.padLeft = 5;
  params.padRight = 5;
  params.orientationX = orient_center;
  params.orientationY = orient_middle;
  params.useDefaultForeground = 1;
  params.useDefaultBackground = 1;

  char *tmp = malloc(numberDisks * 80);
  for (count = 0; count < numberDisks; count ++)
    {
      diskStrings[count] = (tmp + (count * 80));
      sprintf(diskStrings[count], "%s  [ %s ]", diskInfo[count].name,
	      diskInfo[count].partType.description);
    }

  if (graphics)
    {
      chooseWindow = windowNew(processId, "Choose Disk");
      windowNewTextLabel(chooseWindow, CHOOSEDISK_STRING, &params);

      // Make a window list with all the disk choices
      params.gridY = 1;
      diskList = windowNewList(chooseWindow, 5, 1, 0, diskStrings,
			       numberDisks, &params);
      free(diskStrings[0]);

      // Make 'OK' and 'cancel' buttons
      params.gridY = 2;
      params.gridWidth = 1;
      params.padBottom = 5;
      params.padRight = 0;
      params.orientationX = orient_right;
      okButton = windowNewButton(chooseWindow, "OK", NULL, &params);

      params.gridX = 1;
      params.padRight = 5;
      params.padLeft = 0;
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
	  if ((status < 0) || ((status > 0) &&
	      (event.type == EVENT_MOUSE_LEFTUP)))
	    {
	      diskNumber = windowComponentGetSelected(diskList);
	      break;
	    }

	  // Check for our Cancel button
	  status = windowComponentEventGet(cancelButton, &event);
	  if ((status < 0) || ((status > 0) &&
	      (event.type == EVENT_MOUSE_LEFTUP)))
	    break;

	  // Done
	  multitaskerYield();
	}

      windowDestroy(chooseWindow);
      chooseWindow = NULL;
    }

  else
    diskNumber = vshCursorMenu(CHOOSEDISK_STRING, numberDisks, diskStrings, 0);

  free(diskStrings[0]);
  return (diskNumber);
}


static int copyBootSector(const char *destDisk)
{
  // Overlay the boot sector from /system/boot/bootsect.fatnoboot onto the
  // boot sector of the target disk

  int status = 0;
  file bootSectFile;
  unsigned char *bootSectData = NULL;
  unsigned char destBootSector[512];
  int count;

  // Try to read a boot sector file from the system directory
  bootSectData = loaderLoad("/system/boot/bootsect.fatnoboot",
			    &bootSectFile);  
  if (bootSectData == NULL)
    return (status = ERR_NOSUCHFILE);
  
  // Read the boot sector of the target disk
  status = diskReadSectors(destDisk, 0, 1, destBootSector);
  if (status < 0)
    return (status);

  // Copy bytes 0-2 and 62-511 from the root disk boot sector to the
  // target boot sector
  for (count = 0; count < 3; count ++)
    destBootSector[count] = bootSectData[count];
  for (count = 62; count < 512; count ++)
    destBootSector[count] = bootSectData[count];

  memoryRelease(bootSectData);

  // Write the boot sector of the target disk
  status = diskWriteSectors(destDisk, 0, 1, destBootSector);
  if (status < 0)
    return (status);

  diskSync();

  return (status = 0);
}


int main(int argc, char *argv[])
{
  int status = 0;
  int silentMode = 0;
  char opt;
  int diskNumber = -1;
  char *diskName = NULL;
  char rootDisk[DISK_MAX_NAMELENGTH];
  char type[16];
  char tmpChar[240];
  int count;

  // Are graphics enabled?
  graphics = graphicsAreEnabled();

  // Check for options
  while (strchr("st:", (opt = getopt(argc, argv, "st:"))))
    {
      // Operate in silent/script mode?
      if (opt == 's')
	silentMode = 1;

      if (opt == 't')
	{
	  if (!optarg)
	    {
	      if (!silentMode)
		error("Missing type argument to '-t' option");
	      return (errno = ERR_NULLPARAMETER);
	    }
	  strcpy(type, optarg);
	}

      // Force text mode?
      if (opt == 'T')
	graphics = 0;
    }

  // Call the kernel to give us the number of available disks
  numberDisks = diskGetCount();

  status = diskGetInfo(diskInfo);
  if (status < 0)
    {
      // Eek.  Problem getting disk info
      errno = status;
      return (status);
    }

  if (!graphics && !silentMode)
    // Print a message
    printf("\nVisopsys FORMAT Utility\nCopyright (C) 1998-2004 J. Andrew "
	   "McLaughlin\n\n");

  // By default, we do 'generic' (i.e. let the driver make decisions) FAT.
  strcpy(type, "fat");

  if (argc > 1)
    {
      // The user can specify the disk name as an argument.  Try to see
      // whether they did so.
      for (count = 0; count < numberDisks; count ++)
	if (!strcmp(diskInfo[count].name, argv[argc - 1]))
	  {
	    diskNumber = count;
	    break;
	  }
    }

  processId = multitaskerGetCurrentProcessId();

  // Check privilege level
  if (multitaskerGetProcessPrivilege(processId) != 0)
    {
      if (!silentMode)
	error("You must be a privileged user to use this command.\n(Try "
	      "logging in as user \"admin\")");
      return (errno = ERR_PERMISSION);
    }

  if (diskNumber == -1)
    {
      if (silentMode)
	// Can't prompt for a disk in silent mode
	return (errno = ERR_INVALID);

      // The user has not specified a disk name.  We need to display the
      // list of available disks and prompt them.

      diskNumber = chooseDisk();
      if (diskNumber < 0)
	{
	  error("No disk selected.  Quitting.");
	  return (0);
	}
      
      sprintf(tmpChar, "Formatting disk %s as %s.  All data currenly on the "
	      "disk will be lost.\nAre you sure?",
	      diskInfo[diskNumber].name, type);
      if (!yesOrNo(tmpChar))
	{
	  printf("\nQuitting.\n");
	  return (status = 0);
	}
    }
  
  diskName = diskInfo[diskNumber].name;

  // Get the root disk
  status = diskGetBoot(rootDisk);
  if (status >= 0)
    if (!strcmp(rootDisk, diskName))
      {
	if (!silentMode)
	  {
	    sprintf(tmpChar, "\nYOU HAVE REQUESTED TO FORMAT YOUR ROOT DISK.  "
		    "I probably shouldn't let you\ndo this.  After format is "
		    "complete, you should shut down the computer.\nAre you "
		    "SURE you want to proceed?");
	    if (!yesOrNo(tmpChar))
	      {
		printf("\nQuitting.\n");
		return (status = 0);
	      }
	  }
      }

  status = filesystemFormat(diskName, type, "", 0);
  if (status < 0)
    {
      errno = status;
      return (status);
    }

  // The kernel's format code creates a 'dummy' boot sector.  If we have
  // a proper one stored in the /system/boot directory, copy it to the
  // disk.
  copyBootSector(diskName);

  if (!silentMode)
    {
      sprintf(tmpChar, "Format complete");
      if (graphics)
	windowNewInfoDialog(NULL, "Success", tmpChar);
      else
	printf("%s\n", tmpChar);
    }

  errno = 0;
  return (status = 0);
}
