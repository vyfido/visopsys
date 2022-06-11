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
//  format.c
//

// This is a program for formatting a disk

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/api.h>


static int yesOrNo(void)
{
  char character;

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
  int availableDisks = 0;
  int diskNumber = -1;
  char *diskName = NULL;
  disk diskInfo[DISK_MAXDEVICES];
  char rootDisk[DISK_MAX_NAMELENGTH];
  char type[16];
  unsigned char character[2];
  int count1, count2;

  // Call the kernel to give us the number of available disks
  availableDisks = diskGetCount();

  status = diskGetInfo(diskInfo);
  if (status < 0)
    {
      // Eek.  Problem getting disk info
      errno = status;
      return (status);
    }

  // By default, we do 'generic' (i.e. let the driver make decisions) FAT.
  strcpy(type, "fat");

  if (argc >= 2)
    {
      for (count1 = 1; count1 < argc; count1 ++)
	{
	  if (!strncmp(argv[count1], "-t", 2))
	    {
	      if (argv[count1][2] == '\0')
		{
		  count1++;
		  strcpy(type, argv[count1]);
		}
	      else
		strcpy(type, (argv[count1] + 2));
	      continue;
	    }
	  else
	    {
	      // The user can specify the disk name as an argument.  Try to see
	      // whether they did so.
	      for (count2 = 0; count2 < availableDisks; count2 ++)
		if (!strcmp(diskInfo[count2].name, argv[count1]))
		  {
		    diskNumber = count2;
		    break;
		  }
	      if (diskNumber > 0)
		continue;
	    }
	}
    }

  // Print a message
  printf("\nVisopsys FORMAT Utility\nCopyright (C) 1998-2003 J. Andrew "
	 "McLaughlin\n\n");

  // Check privilege level
  if (multitaskerGetProcessPrivilege(multitaskerGetCurrentProcessId()) != 0)
    {
      printf("You must be a privileged user to use this command.\n(Try "
	     "logging in as user \"admin\")\n\n");
      return (errno = ERR_PERMISSION);
    }

  if (diskNumber == -1)
    {
      // The user has not specified a disk name.  We need to display the
      // list of available disks and prompt them.

      printf("Please choose the disk to format:\n");
      
      // Loop through all the possibilities, getting the disk info and
      // displaying it
      for (count1 = 0; count1 < availableDisks; count1 ++)
	// Print disk info
	printf("%d: %s  [ %s ]\n", count1, diskInfo[count1].name,
	       diskInfo[count1].partType.description);

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
	      if (diskNumber > (availableDisks - 1))
		{
		  printf("Invalid volume number %d.\n-> ", diskNumber);
		  continue;
		}
	      printf("\n");
	      break;
	    }
	  else
	    {
	      printf("\nNo disk selected.  Quitting.\n");
	      return (0);
	    }
	}

      printf("Formatting disk %s as %s.  All data currenly on the disk will "
	     "be lost.\nAre you sure? (y/n): ", diskInfo[diskNumber].name,
	     type);

      if (!yesOrNo())
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
	printf("\nYOU HAVE REQUESTED TO FORMAT YOUR ROOT DISK.  I probably "
	       "shouldn't let you\ndo this.  After format is complete, you "
	       "should shut down the computer.\nAre you SURE you want to "
	       "proceed? (y/n): ");

	if (!yesOrNo())
	  {
	    printf("\nQuitting.\n");
	    return (status = 0);
	  }
      }

  printf("\n");

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

  errno = 0;
  return (status = 0);
}
