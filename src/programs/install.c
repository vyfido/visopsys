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
//  install.c
//

// This is a program for installing the system on a target disk (filesystem).

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/api.h>


#define mountPoint "/tmp_install"


static void usage(char *name)
{
  printf("usage:\n");
  printf("%s [disk]\n", name);
  return;
}


static int copyBootSector(const char *rootDisk, const char *destDisk)
{
  // Overlay the boot sector from the root disk onto the boot sector of
  // the target disk

  int status = 0;
  file bootSectFile;
  unsigned char *bootSectData = NULL;
  unsigned char rootBootSector[512];
  unsigned char destBootSector[512];
  int count;

  printf("Copying boot sector...  ");

  // Try to read a boot sector file from the system directory
  bootSectData = loaderLoad("/system/boot/bootsect.fat12", &bootSectFile);  
  
  if (bootSectData == NULL)
    {
      // Try to read the boot sector of the root disk instead
      status = diskReadSectors(rootDisk, 0, 1, rootBootSector);
      if (status < 0)
	{
	  printf("\nUnable to read the boot sector of the root disk.  "
		 "Quitting.\n");
	  return (status);
	}

      bootSectData = rootBootSector;
    }
  
  // Read the boot sector of the target disk
  status = diskReadSectors(destDisk, 0, 1, destBootSector);
  if (status < 0)
    {
      printf("\nUnable to read the boot sector of the target disk.  "
	     "Quitting.\n");
      return (status);
    }

  // Copy bytes 0-2 and 62-511 from the root disk boot sector to the
  // target boot sector
  for (count = 0; count < 3; count ++)
    destBootSector[count] = bootSectData[count];
  for (count = 62; count < 512; count ++)
    destBootSector[count] = bootSectData[count];

  if (bootSectData != rootBootSector)
    memoryRelease(bootSectData);

  // Write the boot sector of the target disk
  status = diskWriteSectors(destDisk, 0, 1, destBootSector);
  if (status < 0)
    {
      printf("\nUnable to write the boot sector of the target disk.  "
	     "Quitting.\n");
      return (status);
    }

  diskSync();

  printf("Done\n");

  return (status = 0);
}


static int mountDisk(const char *diskName)
{
  int status = 0;

  printf("Mounting target disk...  ");

  status = filesystemMount(diskName, mountPoint);
  if (status < 0)
    {
      printf("\nUnable to mount the filesystem of the target disk.  "
	     "Quitting.\n");
      return (status);
    }

  printf("Done\n");

  return (status = 0);
}


static int unmountDisk(void)
{
  int status = 0;

  printf("Unounting target disk...  ");

  status = filesystemUnmount(mountPoint);
  if (status < 0)
    printf("\nWARNING: Unable to unmount the filesystem of the target "
	   "disk.\n");
  else
    printf("Done\n");

  return (status = 0);
}


static int copyFile(const char *fileName)
{
  int status = 0;
  char tmpFileName[128];

  printf("%s\n", fileName);
  strcpy(tmpFileName, mountPoint);
  strcat(tmpFileName, fileName);
  status = fileCopyRecursive(fileName, tmpFileName);
  return (status);
}


static int copyFiles(void)
{
  int status = 0;
  int count;

  char *filesToCopy[] = 
    {
      "/vloader",
      "/visopsys",
      "/programs",
      "/system",
      ""
    };

  printf("Copying files...\n");

  for (count = 0; (filesToCopy[count][0] != '\0'); count ++)
    {
      status = copyFile(filesToCopy[count]);

      diskSync();

      if (status < 0)
	return (status);
    }

  // Delete anything we don't want in the installation
  char tmp[1024];
  sprintf(tmp, "%s/system/kernel.log", mountPoint);
  fileDelete(tmp);

  printf("Done\n");

  return (status = 0);
}


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


int main(int argc, char *argv[])
{
  int status = 0;
  int availableDisks = 0;
  int diskNumber = -1;
  char *diskName = NULL;
  disk diskInfo[DISK_MAXDEVICES];
  unsigned char character[2];
  char rootDisk[DISK_MAX_NAMELENGTH];
  int count;

  if (argc > 2)
    {
      usage((argc > 0)? argv[0] : "install");
      return (status = ERR_ARGUMENTCOUNT);
    }

  // Print a message
  printf("\nVisopsys Native Installer\nCopyright (C) 1998-2003 J. Andrew "
	 "McLaughlin\n\n");

  // Check privilege level
  if (multitaskerGetProcessPrivilege(multitaskerGetCurrentProcessId()) != 0)
    {
      printf("You must be a privileged user to use this command.\n(Try "
	     "logging in as user \"admin\")\n\n");
      return (errno = ERR_PERMISSION);
    }

  // Call the kernel to give us the number of available disks
  availableDisks = diskGetCount();

  status = diskGetInfo(diskInfo);
  if (status < 0)
    {
      // Eek.  Problem getting disk info
      errno = status;
      return (status);
    }

  if (argc == 2)
    {
      // The user can specify the disk name as an argument.  Try to see
      // whether they did so.

      for (count = 0; count < availableDisks; count ++)
	if (!strcmp(diskInfo[count].name, argv[1]))
	  {
	    diskNumber = count;
	    break;
	  }

      if (diskNumber < 0)
	{
	  // Oops, not a valid disk name
	  usage(argv[0]);
	  return (status = ERR_NOSUCHENTRY);
	}
    }
  else
    {
      // The user has not specified a disk number.  We need to display the
      // list of available disks and prompt them.

      printf("Please choose the volume on which to install.  Note that the "
	     "installation\nvolume MUST be of the same filesystem type as "
	     "the current root filesystem!:\n");
      
      // Loop through all the possibilities, getting the disk info and
      // displaying it
      for (count = 0; count < availableDisks; count ++)
	// Print disk info
	printf("%d: %s  [ %s ]\n", count, diskInfo[count].name,
	       diskInfo[count].partType.description);

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
    }
  
  diskName = diskInfo[diskNumber].name;

  printf("Installing on disk %s.  Are you SURE? (y/n): ", diskName);

  if (!yesOrNo())
    {
      printf("\n\nQuitting.\n");
      return (status = 0);
    }
  printf("\n");

  // Get the root disk
  status = diskGetBoot(rootDisk);
  if (status < 0)
    {
      // Couldn't get the root disk name
      errno = status;
      return (status);
    }

  // Make sure it's not the same disk; that would be pointless and possibly
  // dangerous
  if (!strcmp(rootDisk, diskName))
    {
      printf("The system is currently running on disk %s.  Quitting.\n",
	     diskName);
      errno = ERR_ALREADY;
      return (status = errno);
    }

  printf("Format disk %s? (destroys all data!) (y/n): ", diskName);

  if (yesOrNo())
    {
      printf("\n");

      status = filesystemFormat(diskName, "fat12", "Visopsys", 0);
      if (status < 0)
	{
	  printf("\n\nErrors during format.  Quitting.\n");
	  errno = status;
	  return (status);
	}
    }
  else
    printf("\n");

  // Copy the boot sector
  status = copyBootSector(rootDisk, diskName);
  if (status < 0)
    {
      // Couldn't copy the boot sector
      errno = status;
      return (status);
    }

  // Mount the target filesystem
  status = mountDisk(diskName);
  if (status < 0)
    {
      // Couldn't mount the filesystem
      errno = status;
      return (status);
    }

  // Copy the files
  status = copyFiles();

  // Unmount the target filesystem
  unmountDisk();

  if (status < 0)
    {
      // Couldn't copy the files
      printf("Unable to copy files.  Quitting.\n");
    }
  
  errno = status;
  return (status);
}
