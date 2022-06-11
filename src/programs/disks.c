//
//  Visopsys
//  Copyright (C) 1998-2020 J. Andrew McLaughlin
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
//  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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

Note in the third example above, the physical device is the first hard disk,
hd0.  Logical partitions are specified with letters, in partition table order
(a = first partition, b = second partition, etc.).

</help>
*/

#include <errno.h>
#include <libintl.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/api.h>
#include <sys/env.h>

#define _(string) gettext(string)

#define COL_PART	11
#define COL_FS		37
#define COL_MOUNT	49


int main(void)
{
	int status = 0;
	int availableDisks = 0;
	disk *diskInfo = NULL;
	int count;

	setlocale(LC_ALL, getenv(ENV_LANG));
	textdomain("disks");

	// Call the kernel to give us the number of available disks
	availableDisks = diskGetCount();
	if (availableDisks < 0)
	{
		errno = availableDisks;
		perror("diskGetCount");
		return (status = availableDisks);
	}

	if (!availableDisks)
	{
		printf("\n%s\n", _("No disks"));
		return (status = 0);
	}

	diskInfo = malloc(DISK_MAXDEVICES * sizeof(disk));
	if (!diskInfo)
	{
		status = errno;
		perror("malloc");
		return (status);
	}

	status = diskGetAll(diskInfo, (DISK_MAXDEVICES * sizeof(disk)));
	if (status < 0)
	{
		errno = status;
		perror("diskGetAll");
		free(diskInfo);
		return (status);
	}

	printf("\n%s", _("Disk name"));
	textSetColumn(COL_PART);
	printf("%s", _("Partition"));
	textSetColumn(COL_FS);
	printf("%s", _("Filesystem"));
	textSetColumn(COL_MOUNT);
	printf("%s\n", _("Mount"));

	for (count = 0; count < availableDisks; count ++)
	{
		// Print disk info

		printf("%s", diskInfo[count].name);

		textSetColumn(COL_PART);
		printf("%s", diskInfo[count].partType);

		if (strcmp(diskInfo[count].fsType, "unknown"))
		{
			textSetColumn(COL_FS);
			printf("%s", diskInfo[count].fsType);
		}

		if (diskInfo[count].mounted)
		{
			textSetColumn(COL_MOUNT);
			printf("%s", diskInfo[count].mountPoint);
		}

		printf("\n");
	}

	free(diskInfo);
	return (status = 0);
}

