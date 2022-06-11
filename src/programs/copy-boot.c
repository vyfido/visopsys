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
//  copy-boot.c
//

// This is a program for writing Visopsys boot sectors.  It is meant to
// work both as part of a Visopsys installation and as a part of the build
// system, so, it's a little tricky like that and needs to be compiled for
// each.

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

#ifdef VISOPSYS
#include <sys/api.h>
#include <sys/vsh.h>
#define OSLOADER  "/vloader"
#else
#define OSLOADER  "../build/vloader"
#endif

//#define DEBUG(message, arg...) printf(message, ##arg)
#define DEBUG(message, arg...) do { } while (0)

// FAT structures.  We pad out the bits we don't care about.

typedef struct {
  unsigned char pad1[11];
  unsigned short bytesPerSector;
  unsigned char sectorsPerCluster;
  unsigned short reservedSectors;
  unsigned char numberOfFats;
  unsigned short rootDirEntries;
  unsigned char pad2[3];
  unsigned short fatSectors;
  unsigned char pad3[12];
} __attribute__((packed)) fatCommonBSHeader1;

typedef struct {
  unsigned char pad[18];
  unsigned char fsSignature[8];
} __attribute__((packed)) fatCommonBSHeader2;

typedef struct {
  fatCommonBSHeader1 common1;
  fatCommonBSHeader2 common2;
} __attribute__((packed)) fatBSHeader;

typedef struct {
  fatCommonBSHeader1 common1;
  unsigned fatSectors;
  unsigned char pad1[4];
  unsigned rootDirCluster;
  unsigned short fsInfoSector;
  unsigned short backupBootSector;
  unsigned char pad2[12];
  fatCommonBSHeader2 common2;
} __attribute__((packed)) fat32BSHeader;

typedef struct {
  unsigned char pad1[492];
  unsigned firstFreeCluster;
  unsigned char pad2[16];
} __attribute__((packed)) fat32FsInfo;


static void usage(char *name)
{
  printf("usage:\n%s <boot image> <output file|device>\n", name);
  return;
}


static int readBootsect(const char *inputName, unsigned char *bootSect)
{
  int fd = 0;
  int status = 0;

  DEBUG("Read boot sector from %s\n", inputName);

#ifdef VISOPSYS
  // Is the input source a Visopsys disk name?
  if (inputName[0] != '/')
    {
      status = diskReadSectors(inputName, 0, 1, bootSect);
      if (status < 0)
	{
	  DEBUG("Error reading disk %s\n", inputName);
	  return (errno = status);
	}
    }
  else
#endif
    {
      // Try to open it
      fd = open(inputName, O_RDONLY);
      if (fd < 0)
	{
	  DEBUG("Error opening file %s\n", inputName);
	  return (fd);
	}

      // Read 512 bytes of it
      status = read(fd, bootSect, 512);

      close(fd);

      if (status < 0)
	{
	  DEBUG("Error reading file %s\n", inputName);
	  return (status);
	}

      if (status < 512)
	{
	  DEBUG("Could only read %d bytes from %s\n", status, inputName);
	  errno = EIO;
	  return (-1);
	}
    }

  return (errno = status = 0);
}


static int merge(unsigned char *oldBootsect, unsigned char *newBootsect)
{
  // Merge the old and new boot sectors.
  fatBSHeader *fatHeader = (fatBSHeader *) oldBootsect;
  fat32BSHeader *fat32Header = (fat32BSHeader *) oldBootsect;

  DEBUG("Merge boot sectors\n");

  if (!strncmp(fatHeader->common2.fsSignature, "FAT12   ", 8) ||
      !strncmp(fatHeader->common2.fsSignature, "FAT16   ", 8))
    memcpy((newBootsect + 3), (oldBootsect + 3), (sizeof(fatBSHeader) - 3));
  else if (!strncmp(fat32Header->common2.fsSignature, "FAT32   ", 8))
    memcpy((newBootsect + 3), (oldBootsect + 3), (sizeof(fat32BSHeader) - 3));
  else
    {
      printf("File system type is not supported\n");
      errno = EINVAL;
      return (-1);
    }

  return (errno = 0);
}


static int setOsLoaderParams(const char *outputName,
			     unsigned char *newBootsect, const char *osLoader)
{
  // Our new, generic boot sector keeps the logical sector and length of the
  // OS loader in the 3rd and 2nd last dwords, respectively.  This allows the
  // boot sector code to be simpler and requires little-to-no understanding
  // of the filesystem, which is good.
  // 
  // So, given a pointer to the OS loader program and the contents of the
  // new boot sector, calculate the starting logical sector the boot sector
  // will occupy (the first user-data sector) and its length in sectors, and
  // write them into the boot sector.

  int status = 0;
  struct stat statBuff;
  fatBSHeader *fatHeader = (fatBSHeader *) newBootsect;
  fat32BSHeader *fat32Header = (fat32BSHeader *) newBootsect;
  int fd = 0;
  unsigned char buffer[512];
  fat32FsInfo *fat32Info = (fat32FsInfo *) buffer;
  unsigned fatSectors = 0;
  unsigned *firstUserSector = (unsigned *) (newBootsect + 502);
  unsigned *osLoaderSectors = (unsigned *) (newBootsect + 506);
  int count;

  DEBUG("Set OS loader parameters\n");

  if (!strncmp(fatHeader->common2.fsSignature, "FAT12   ", 8) ||
      !strncmp(fatHeader->common2.fsSignature, "FAT16   ", 8))
    {
      if (!strncmp(fatHeader->common2.fsSignature, "FAT12   ", 8))
	DEBUG("Target filesystem is FAT12\n");
      else if (!strncmp(fatHeader->common2.fsSignature, "FAT16   ", 8))
	DEBUG("Target filesystem is FAT16\n");
      DEBUG("%u reserved\n", (unsigned) fatHeader->common1.reservedSectors);
      DEBUG("%u FATs of %u\n", (unsigned) fatHeader->common1.numberOfFats,
	    (unsigned) fatHeader->common1.fatSectors);
      DEBUG("%u root dir sectors\n",
	    (((unsigned) fatHeader->common1.rootDirEntries * 32) /
	     fatHeader->common1.bytesPerSector));

      *firstUserSector =
	((unsigned) fatHeader->common1.reservedSectors +
	 ((unsigned) fatHeader->common1.numberOfFats *
	  (unsigned) fatHeader->common1.fatSectors) +
	 (((unsigned) fatHeader->common1.rootDirEntries * 32) /
	  fatHeader->common1.bytesPerSector));
    }
  else if (!strncmp(fat32Header->common2.fsSignature, "FAT32   ", 8))
    {
      fatSectors = (unsigned) fat32Header->common1.fatSectors;
      if (fat32Header->fatSectors)
	fatSectors = (unsigned) fat32Header->fatSectors;

      DEBUG("Target filesystem is FAT32\n");
      DEBUG("%u reserved\n", (unsigned) fat32Header->common1.reservedSectors);
      DEBUG("%u FATs of %u\n", (unsigned) fat32Header->common1.numberOfFats,
	    fatSectors);
      DEBUG("Root dir cluster %u\n", (unsigned) fat32Header->rootDirCluster);

      // Read the FAT32 FSInfo sector

#ifdef VISOPSYS
      // Is the destination a Visopsys disk name?
      if (outputName[0] != '/')
	{
	  status =
	    diskReadSectors(outputName, fat32Header->fsInfoSector, 1, buffer);
	  if (status < 0)
	    {
	      DEBUG("Error reading disk %s\n", outputName);
	      return (errno = status);
	    }
	}
      else
#endif
	{
	  fd = open(outputName, O_RDONLY);
	  if (fd < 0)
	    return (fd);

	  status = lseek(fd, (fat32Header->fsInfoSector *
			      fat32Header->common1.bytesPerSector), SEEK_SET);
	  if (status < 0)
	    return (status);

	  // Read 512 bytes of it
	  status = read(fd, buffer, 512);
	  if (status < 0)
	    return (status);
	  if (status < 512)
	    {
	      printf("Can't read FAT32 FSInfo sector %u\n",
		     fat32Header->fsInfoSector);
	      errno = EIO;
	      return (-1);
	    }
	}

      DEBUG("First free cluster %u\n", (unsigned) fat32Info->firstFreeCluster);

      // mkdosfs seems to make a broken FSInfo firstFreeCluster value
      // that doesn't take the root directory usage into account.  Try to
      // catch it.
      if ((unsigned) fat32Info->firstFreeCluster ==
	  (unsigned) fat32Header->rootDirCluster)
	{
	  // Read the first FAT sector and try to find an unused cluster

	  unsigned buffer2[128];

#ifdef VISOPSYS
	  // Is the destination a Visopsys disk name?
	  if (outputName[0] != '/')
	    {
	      status = diskReadSectors(outputName, fat32Header->common1
				       .reservedSectors, 1, buffer2);
	      if (status < 0)
		{
		  DEBUG("Error reading disk %s\n", outputName);
		  return (errno = status);
		}
	    }
	  else
#endif
	    {
	      status =
		lseek(fd, (fat32Header->common1.reservedSectors *
			   fat32Header->common1.bytesPerSector), SEEK_SET);
	      if (status < 0)
		{
		  printf("Can't seek to FAT sector\n");
		  close(fd);
		  return (status);
		}

	      status = read(fd, buffer2, 512);
	      if (status < 0)
		return (status);
	      if (status < 512)
		{
		  printf("Can't read FAT sector\n");
		  close(fd);
		  errno = EIO;
		  return (-1);
		}

	      close(fd);
	    }

	  for (count = 2; count < 128; count ++)
	    {
	      if (buffer2[count] == 0)
		{
		  DEBUG("Correct first unused cluster is %d, not %d\n", count,
			fat32Info->firstFreeCluster);
		  fat32Info->firstFreeCluster = count;
		  break;
		}
	    }
	}

      *firstUserSector = ((unsigned) fat32Header->common1.reservedSectors +
			  ((unsigned) fat32Header->common1.numberOfFats *
			   fatSectors) +
			  ((((unsigned) fat32Info->firstFreeCluster) - 2) *
			   (unsigned) fat32Header->common1.sectorsPerCluster));
    }

  DEBUG("First user sector is %u\n", *firstUserSector);
  
  bzero(&statBuff, sizeof(struct stat));
  status = stat(osLoader, &statBuff);
  if (status < 0)
    return (status);

  *osLoaderSectors =
    ((unsigned) statBuff.st_size / fatHeader->common1.bytesPerSector);
  *osLoaderSectors +=
    (((unsigned) statBuff.st_size % fatHeader->common1.bytesPerSector) != 0);

  DEBUG("OS loader sectors are %u\n", *osLoaderSectors);

  return (errno = status = 0);
}


static int writeBootsect(const char *outputName, unsigned char *bootSect)
{
  int status = 0;
  int fd = 0;
  fat32BSHeader *fat32Header = (fat32BSHeader *) bootSect;

  DEBUG("Write boot sector to %s\n", outputName);

#ifdef VISOPSYS
  // Is the detination a Visopsys disk name?
  if (outputName[0] != '/')
    {
      status = diskWriteSectors(outputName, 0, 1, bootSect);
      if (status < 0)
	{
	  DEBUG("Error writing disk %s\n", outputName);
	  return (errno = status);
	}
    }
  else
#endif
    {
      // Try to open it
      fd = open(outputName, (O_WRONLY | O_SYNC));
      if (fd < 0)
	return (fd);

      // Write 512 bytes of it
      status = write(fd, bootSect, 512);
      if (status < 0)
	return (status);

      if (status < 512)
	{
	  DEBUG("Could not write 512 bytes of \"%s\"\n", outputName);
	  errno = EIO;
	  return (-1);
	}

      close(fd);
    }

  // If we think this is a FAT32 boot sector, we need to write the backup
  // one as well.
  if (!strncmp(fat32Header->common2.fsSignature, "FAT32   ", 8))
    {
#ifdef VISOPSYS
      // Is the detination a Visopsys disk name?
      if (outputName[0] != '/')
	{
	  status = diskWriteSectors(outputName, fat32Header->backupBootSector,
				    1, bootSect);
	  if (status < 0)
	    {
	      DEBUG("Error writing disk %s\n", outputName);
	      return (errno = status);
	    }
	}
      else
#endif
	{
	  // Try to open it
	  fd = open(outputName, (O_WRONLY | O_SYNC));
	  if (fd < 0)
	    return (fd);

	  status = lseek(fd, (fat32Header->backupBootSector *
			      fat32Header->common1.bytesPerSector), SEEK_SET);
	  if (status < 0)
	    return (status);

	  status = write(fd, bootSect, 512);
	  if (status < 0)
	    return (status);

	  close(fd);
	}
    }

  return (errno = status = 0);
}


int main(int argc, char *argv[])
{
  int status = 0;
  char sourceName[1024];
  char destName[1024];
  unsigned char oldBootsect[512];
  unsigned char newBootsect[512];

  // Check that we're compiled the way we expect
  if (sizeof(fatBSHeader) != 0x3E)
    {
      printf("fatBSHeader size is 0x%02x instead of 0x3E\n",
	     sizeof(fatBSHeader));
      errno = EINVAL;
      return (-1);
    }
  if (sizeof(fat32BSHeader) != 0x5A)
    {
      printf("fat32BSHeader size is 0x%02x instead of 0x5A\n",
	     sizeof(fat32BSHeader));
      errno = EINVAL;
      return (-1);
    }
  if (sizeof(fat32FsInfo) != 512)
    {
      printf("fat32FsInfo size is %d instead of 512\n", sizeof(fat32FsInfo));
      errno = EINVAL;
      return (-1);
    }

  if (argc != 3)
    {
      usage(argv[0]);
      errno = EINVAL;
      return (status = -1);
    }

#ifdef VISOPSYS
  file tmpFile;
  vshMakeAbsolutePath(argv[1], sourceName);
  if (fileFind(sourceName, &tmpFile) < 0)
#endif
    strncpy(sourceName, argv[1], 1024);
#ifdef VISOPSYS
  vshMakeAbsolutePath(argv[2], destName);
  if (fileFind(destName, &tmpFile) < 0)
#endif
    strncpy(destName, argv[2], 1024);

  // Read the new boot sector from the source file
  status = readBootsect(sourceName, newBootsect);
  if (status < 0)
    {
      perror(argv[0]);
      return (status);
    }

  // Read the old boot sector from the target device
  status = readBootsect(destName, oldBootsect);
  if (status < 0)
    {
      perror(argv[0]);
      return (status);
    }

  status = merge(oldBootsect, newBootsect);
  if (status < 0)
    {
      perror(argv[0]);
      return (status);
    }

  status = setOsLoaderParams(destName, newBootsect, OSLOADER);
  if (status < 0)
    {
      perror(argv[0]);
      return (status);
    }

  // Write the new boot sector
  status = writeBootsect(destName, newBootsect);
  if (status < 0)
    {
      perror(argv[0]);
      return (status);
    }

  // Return success
  return (errno = status = 0);
}
