//
//  Visopsys
//  Copyright (C) 1998-2006 J. Andrew McLaughlin
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
//  kernelFilesystemNtfs.c
//

// This file contains the routines designed to interpret the NTFS filesystem
// (commonly found on Windows 2000 and Windows XP+)

#include "kernelFilesystem.h"
#include "kernelMisc.h"
#include "kernelError.h"
#include <string.h>
#include <sys/ntfs.h>


static int initialized = 0;


static int readBootFile(const kernelDisk *theDisk, ntfsBootFile *bootFile)
{
  // This simple function will read the $Boot file into the supplied
  // structure.  Returns 0 on success negative on error.

  int status = 0;
  unsigned sectors = 0;
  kernelPhysicalDisk *physicalDisk = NULL;

  // Initialize the buffer we were given
  kernelMemClear(bootFile, sizeof(ntfsBootFile));

  physicalDisk = theDisk->physical;

  // The sector size must be non-zero
  if (physicalDisk->sectorSize == 0)
    {
      kernelError(kernel_error, "Disk sector size is zero");
      return (status = ERR_INVALID);
    }

  sectors = ((sizeof(ntfsBootFile) / physicalDisk->sectorSize) + 
	     ((sizeof(ntfsBootFile) % physicalDisk->sectorSize)? 1 : 0));

  // Read the $Boot file
  status = kernelDiskReadSectors((char *) theDisk->name, 0, sectors, bootFile);
  return (status);
}


static int writeBootFile(const kernelDisk *theDisk, ntfsBootFile *bootFile)
{
  // This simple function will write the $Boot file from the supplied
  // structure.  Returns 0 on success negative on error.

  int status = 0;
  unsigned sectors = 0;
  kernelPhysicalDisk *physicalDisk = NULL;

  physicalDisk = theDisk->physical;

  // The sector size must be non-zero
  if (physicalDisk->sectorSize == 0)
    {
      kernelError(kernel_error, "Disk sector size is zero");
      return (status = ERR_INVALID);
    }

  sectors = ((sizeof(ntfsBootFile) / physicalDisk->sectorSize) + 
	     ((sizeof(ntfsBootFile) % physicalDisk->sectorSize)? 1 : 0));

  // Write the $Boot file
  status =
    kernelDiskWriteSectors((char *) theDisk->name, 0, sectors, bootFile);
  return (status);
}


static int detect(kernelDisk *theDisk)
{
  // This function is used to determine whether the data on a disk structure
  // is using an NTFS filesystem.  Returns 1 for true, 0 for false,  and
  // negative if it encounters an error

  int status = 0;
  ntfsBootFile bootFile;

  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check params
  if (theDisk == NULL)
    {
      kernelError(kernel_error, "Disk structure is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  status = readBootFile(theDisk, &bootFile);
  if (status < 0)
    // Not NTFS
    return (status);

  // Check for the NTFS OEM text
  if (!strncmp(bootFile.oemName, "NTFS    ", 8))
    {
      // NTFS
      strcpy((char *) theDisk->fsType, FSNAME_NTFS);
      return (status = 1);
    }
  else
    // Not NTFS
    return (status = 0);
}


static int clobber(kernelDisk *theDisk)
{
  // This function destroys anything that might cause this disk to be detected
  // as having an NTFS filesystem.

  int status = 0;
  ntfsBootFile bootFile;

  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check params
  if (theDisk == NULL)
    {
      kernelError(kernel_error, "Disk structure is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  status = readBootFile(theDisk, &bootFile);
  if (status < 0)
    return (status);

  bzero(bootFile.oemName, 8);

  status = writeBootFile(theDisk, &bootFile);
  return (status);
}


static kernelFilesystemDriver defaultNtfsDriver = {
  FSNAME_NTFS, // Driver name
  detect,
  NULL,  // driverFormat
  clobber,
  NULL,  // driverCheck
  NULL,  // driverDefragment
  NULL,  // driverStat
  NULL,  // driverResizeConstraints
  NULL,  // driverResize
  NULL,  // driverMount
  NULL,  // driverUnmount
  NULL,  // driverGetFree
  NULL,  // driverNewEntry
  NULL,  // driverInactiveEntry
  NULL,  // driverResolveLink
  NULL,  // driverReadFile
  NULL,  // driverWriteFile
  NULL,  // driverCreateFile
  NULL,  // driverDeleteFile,
  NULL,  // driverFileMoved,
  NULL,  // driverReadDir
  NULL,  // driverWriteDir
  NULL,  // driverMakeDir
  NULL,  // driverRemoveDir
  NULL   // driverTimestamp
};


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
//  Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


int kernelFilesystemNtfsInitialize(void)
{
  // Initialize the driver

  int status = 0;
  
  // Register our driver
  status = kernelDriverRegister(ntfsDriver, &defaultNtfsDriver);

  initialized = 1;

  return (status);
}
