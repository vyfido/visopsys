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
//  cdrom.c
//

// This is a program for controlling aspects of a CD-ROM device, such
// as opening and closing the drawer.

/* This is the text that appears when a user requests help about this program
<help>

 -- cdrom --

This command can be used to control operations of CD-ROM devices.

Usage:
  cdrom [disk_name] [operation]

    where 'operation' is one of: open, eject, close, lock, unlock 

The first (optional) parameter is the name of a CD-ROM disk.  If no disk
name is specified, the cdrom command will attempt to guess the most likely
device (the first CD-ROM device identified by the system). The second
(optional) argument tells the CD-ROM which operation to perform.

If no disk name or operation are specified, the program prints out the names
of CD-ROM devices it thinks are available to the system.  Use the 'disks'
command to print out the names of all disks.

</help>
*/

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/api.h>


static int numberDisks = 0;
static disk diskInfo[DISK_MAXDEVICES];
static disk *selectedDisk = NULL;


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

  // Loop through these disks, figuring out which ones are CD-ROMS
  // and putting them into the regular array
  for (count = 0; count < tmpNumberDisks; count ++)
    if (tmpDiskInfo[count].type & DISKTYPE_CDROM)
      {
	memcpy(&diskInfo[numberDisks], &tmpDiskInfo[count], sizeof(disk));
	numberDisks ++;
      }

  return (status = 0);
}


static void printDisks(void)
{
  // Print disk info

  int count;
  
  for (count = 0; count < numberDisks; count ++)
    printf("%s\n", diskInfo[count].name);
}


int main(int argc, char *argv[])
{
  int status = 0;
  int count;

  // Gather the disk info
  status = scanDisks();
  if (status < 0)
    {
      printf("\n\nProblem getting CD-ROM info\n\n");
      errno = status;
      return (status);
    }

  if (argc < 2)
    {
      printDisks();
      errno = 0;
      return (status = 0);
    }

  // There needs to be at least one CD-ROM to continue
  if (numberDisks < 1)
    {
      printf("\n\nNo CD-ROMS registered\n\n");
      errno = ERR_NOSUCHENTRY;
      return (status = errno);
    }

  // Determine which disk is requested.  If there's only one it's easy
  if ((numberDisks > 1) && (argc > 2))
    {
      // Did the user specify which cdrom to use?
      for (count = 0; count < numberDisks; count ++)
	if (!strcmp(argv[1], diskInfo[count].name))
	  {
	    selectedDisk = &diskInfo[count];
	    break;
	  }
    }

  if (selectedDisk == NULL)
    selectedDisk = &(diskInfo[0]);

  if (!strcasecmp(argv[argc - 1], "open") ||
      !strcasecmp(argv[argc - 1], "eject"))
    status = diskSetDoorState(selectedDisk->name , 1);

  else if (!strcasecmp(argv[argc - 1], "lock"))
    status = diskSetLockState(selectedDisk->name , 1);

  else if (!strcasecmp(argv[argc - 1], "unlock"))
    status = diskSetLockState(selectedDisk->name , 0);

  else if (!strcasecmp(argv[argc - 1], "close"))
    status = diskSetDoorState(selectedDisk->name , 0);

  else
    {
      printf("\n\nUnknown command \"%s\"\n\n", argv[argc - 1]);
      status = ERR_INVALID;
    }

  errno = status;

  if (status < 0)
    perror(argv[0]);

  return (status);
}
