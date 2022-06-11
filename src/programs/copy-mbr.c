//
//  Visopsys
//  Copyright (C) 1998-2013 J. Andrew McLaughlin
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
//  copy-mbr.c
//

// This is a program for writing Visopsys MBR sectors.

/* This is the text that appears when a user requests help about this program
<help>

 -- copy-mbr --

Write a Visopsys MBR sector.

Usage:
  copy-mbr <image> <disk>

The copy-mbr command copies the MBR (master boot record) image to the
named physical disk.  Not useful to most users under normal circumstances;
Rather more useful in a system rescue situation, where a DOS-like automatic
booting of the 'active' partition is desired.

Example:
  copy-mbr /system/boot/mbr.simple hd0

</help>
*/

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/api.h>
#include <sys/vsh.h>

#ifdef DEBUG
  #define DEBUGMSG(message, arg...) printf(message, ##arg)
#else
  #define DEBUGMSG(message, arg...) do { } while (0)
#endif


static void usage(char *name)
{
  printf("usage:\n%s <MBR image> <output device>\n", name);
  return;
}


static int readMbrSect(const char *inputName, unsigned char *mbrSect)
{
  int status = 0;
  int fd = 0;

  DEBUGMSG("Read MBR sector from %s\n", inputName);

  // Is the input source a Visopsys disk name?
  if (inputName[0] != '/')
    {
      status = diskReadSectors(inputName, 0, 1, mbrSect);
      if (status < 0)
	{
	  DEBUGMSG("Error reading disk %s\n", inputName);
	  return (errno = status);
	}
    }
  else
    {
      // Try to open it
      fd = open(inputName, O_RDONLY);
      if (fd < 0)
	{
	  DEBUGMSG("Error opening file %s\n", inputName);
	  return (fd);
	}
      
      // Read 512 bytes of it
      status = read(fd, mbrSect, 512);
      
      close(fd);
      
      if (status < 0)
	{
	  DEBUGMSG("Error reading file %s\n", inputName);
	  return (status);
	}
      
      if (status < 512)
	{
	  DEBUGMSG("Could only read %d bytes from %s\n", status, inputName);
	  errno = EIO;
	  return (-1);
	}
    }

  return (errno = status = 0);
}


int main(int argc, char *argv[])
{
  int status = 0;
  char *sourceName = NULL;
  char *destName = NULL;
  unsigned char oldMbrSect[512];
  unsigned char newMbrSect[512];
  time_t t = 0;

  if (argc != 3)
    {
      usage(argv[0]);
      errno = EINVAL;
      return (status = -1);
    }

  sourceName = argv[1];
  destName = argv[2];

  // Read the new MBR sector from the source file
  status = readMbrSect(sourceName, newMbrSect);
  if (status < 0)
    {
      perror(argv[0]);
      return (status);
    }

  // Read the old MBR sector from the target device
  status = readMbrSect(destName, oldMbrSect);
  if (status < 0)
    {
      perror(argv[0]);
      return (status);
    }

  DEBUGMSG("Add disk signature to new MBR\n");
  time(&t);
  memcpy((newMbrSect + 440), &t, 4);

  DEBUGMSG("Copy partition table to new MBR\n");
  memcpy((newMbrSect + 446), (oldMbrSect + 446), 64);

  // Write the new MBR sector
  DEBUGMSG("Write MBR sector to %s\n", destName);
  status = diskWriteSectors(destName, 0, 1, newMbrSect);
  if (status < 0)
    {
      DEBUGMSG("Error writing disk %s\n", destName);
      perror(argv[0]);
      return (status);
    }

  // Return success
  return (errno = status = 0);
}
