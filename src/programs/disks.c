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
//  disks.c
//

// This command just lists all the disks registered in the system

/* This is the text that appears when a user requests help about this program
<help>

 -- disks --

Print all of the logical disks attached to the system.

Usage:
  disks

This command will print all of the disks by name, along with any device,
filesystem, or logical partition information that is appropriate.

Disk names start with certain combinations of letters which tend to indicate
the type of disk.  Examples

cd0  - First CD-ROM disk
fd1  - Second floppy disk
hd0b - Second logical partition on the first hard disk.

</help>
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/api.h>


int main(int argc, char *argv[])
{
  int status = 0;
  int availableDisks = 0;
  disk diskInfo[DISK_MAXDEVICES];
  int count;

  // We don't use argc.  This keeps the compiler happy
  argc = 0;

  // Call the kernel to give us the number of available disks
  availableDisks = diskGetCount();

  status = diskGetAll(diskInfo, (DISK_MAXDEVICES * sizeof(disk)));
  if (status < 0)
    {
      // Eek.  Problem getting disk info
      errno = status;
      perror(argv[0]);
      return (status);
    }

  printf("\nDisk  Partition");
  textSetColumn(31);
  printf("Filesystem\n");

  for (count = 0; count < availableDisks; count ++)
    {
      // Print disk info
      printf("%s: ", diskInfo[count].name);
      textSetColumn(6);
      printf("%s", diskInfo[count].partType.description);
      if (strcmp(diskInfo[count].fsType, "unknown"))
	{
	  textSetColumn(30);
	  printf(" %s", diskInfo[count].fsType);
	}
      printf("\n");
    }

  errno = 0;
  return (status = errno);
}
