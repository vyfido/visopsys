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
//  reinstall.c
//

// This is a program for reinstalling the system from another disk
// (filesystem).

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/api.h>


#define mountPoint "/tmp_install"

typedef enum 
{  
  halt, reboot

} shutdownType;


static void usage(char *name)
{
  printf("usage:\n");
  printf("%s [disk #]\n", name);
  return;
}


static int copyBootSector(int diskNumber, int rootDisk)
{
  // Overlay the boot sector from the install disk onto the boot sector of
  // the root disk

  int status = 0;
  unsigned char rootBootSector[512];
  unsigned char diskBootSector[512];
  int count;

  printf("Copying boot sector...  ");

  // Read the boot sector of the root disk
  status = diskFunctionsReadSectors(rootDisk, 0, 1, rootBootSector);
  if (status < 0)
    {
      printf("\nUnable to read the boot sector of the root disk.  "
	     "Quitting.\n");
      return (status);
    }
  
  // Read the boot sector of the install disk
  status = diskFunctionsReadSectors(diskNumber, 0, 1, diskBootSector);
  if (status < 0)
    {
      printf("\nUnable to read the boot sector of the install disk.  "
	     "Quitting.\n");
      return (status);
    }

  // Copy bytes 0-11 and 39-511 from the install disk boot sector to the
  // root boot sector
  for (count = 0; count < 12; count ++)
    rootBootSector[count] = diskBootSector[count];
  for (count = 39; count < 512; count ++)
    rootBootSector[count] = diskBootSector[count];

  // Write the boot sector of the root disk
  status = diskFunctionsWriteSectors(rootDisk, 0, 1, rootBootSector);
  if (status < 0)
    {
      printf("\nUnable to write the boot sector of the root disk.  "
	     "Quitting.\n");
      return (status);
    }

  printf("Done\n");

  return (status = 0);
}


static int mountDisk(int diskNumber)
{
  int status = 0;

  printf("Mounting install disk...  ");

  status = filesystemMount(diskNumber, mountPoint);
  if (status < 0)
    {
      printf("\nUnable to mount the filesystem of the install disk.  "
	     "Quitting.\n");
      return (status);
    }

  printf("Done\n");

  return (status = 0);
}


static int unmountDisk(void)
{
  int status = 0;

  printf("Unounting install disk...  ");

  status = filesystemUnmount(mountPoint);
  if (status < 0)
    printf("\nWARNING: Unable to unmount the filesystem of the install "
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
  status = fileCopyRecursive(tmpFileName, fileName);
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
      if (status < 0)
	return (status);
    }

  printf("Done\n");

  return (status = 0);
}


int main(int argc, char *argv[])
{
  int status = 0;
  int availableDisks = 0;
  int diskNumber = 0;
  disk diskInfo;
  unsigned char character[2];
  int rootDisk = 0;
  int count;

  if (argc > 2)
    {
      usage((argc > 0)? argv[0] : "reinstall");
      return (status = ERR_ARGUMENTCOUNT);
    }

  // Print a message
  printf("\nVisopsys Native Reinstaller\nCopyright (C) 1998-2004 J. Andrew "
	 "McLaughlin\n");

  if (argc == 2)
    {
      // The user can specify the disk number as the only argument.  Try to see
      // whether they did so.

      diskNumber = atoi(argv[1]);

      if (errno)
	{
	  // Oops, not a number?
	  usage(argv[0]);
	  return (status = errno);
	}
    }
  else
    {
      // The user has not specified a disk number.  We need to display the
      // list of available disks and prompt them.

      // Call the kernel to give us the number of available disks
      availableDisks = diskFunctionsGetCount();

      printf("\nPlease choose the volume from which to install.  Note that "
	     "the\ninstallation volume MUST be of the same filesystem type as"
	     "\nthe current root filesystem!:\n");
      
      // Loop through all the possibilities, getting the disk info and
      // displaying it
      for (count = 0; count < availableDisks; count ++)
	{
	  status = diskFunctionsGetInfo(count, &diskInfo);

	  if (status < 0)
	    {
	      // Eek.  Problem getting disk info
	      errno = status;
	      return (status);
	    }

	  // Print disk info
	  printf("%d: %s\n", count, diskInfo.description);
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
	      if (diskNumber > (availableDisks - 1))
		{
		  printf("Invalid volume number %d.\n-> ", diskNumber);
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
    }
  
  printf("\nInstalling from volume %d.  Are you SURE? (y/n): ", diskNumber);

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

  // Re-read the info for the target disk
  status = diskFunctionsGetInfo(diskNumber, &diskInfo);
  if (status < 0)
    {
      // Eek.  Problem getting disk info
      errno = status;
      return (status);
    }

  // Get the root disk
  rootDisk = diskFunctionsGetBoot();

  // Make sure it's not the same disk; that would be pointless and possibly
  // dangerous
  if (rootDisk == diskNumber)
    {
      printf("The system is currently running on disk %d.  Quitting.\n",
	     diskNumber);
      errno = ERR_ALREADY;
      return (status = errno);
    }

  // Copy the boot sector
  status = copyBootSector(diskNumber, rootDisk);
  if (status < 0)
    {
      // Couldn't copy the boot sector
      errno = status;
      return (status);
    }

  // Mount the target filesystem
  status = mountDisk(diskNumber);
  if (status < 0)
    {
      // Couldn't mount the filesystem
      errno = status;
      return (status);
    }

  // Copy the files
  status = copyFiles();
  if (status < 0)
    {
      // Couldn't copy the files
      printf("Unable to copy files.  Quitting.\n");
      errno = status;
      return (status);
    }

  // Unmount the target filesystem
  status = unmountDisk();

  shutdown(reboot, 0);

  errno = 0;
  return (status = 0);
}
