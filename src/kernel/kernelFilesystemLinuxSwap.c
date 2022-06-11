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
//  kernelFilesystemLinuxSwap.c
//

// This file contains the routines designed to interpret the Linux swap
// filesystem

#include "kernelFilesystem.h"
#include "kernelMisc.h"
#include "kernelError.h"
#include <string.h>
#include <sys/linuxswap.h>


static int initialized = 0;


static int readSwapHeader(const kernelDisk *theDisk, linuxSwapHeader *header)
{
  // This simple function will read the swap header into the supplied
  // structure.  Returns 0 on success negative on error.

  int status = 0;
  unsigned sectors = 0;
  kernelPhysicalDisk *physicalDisk = NULL;

  // Initialize the buffer we were given
  kernelMemClear(header, sizeof(linuxSwapHeader));

  physicalDisk = theDisk->physical;

  // The sector size must be non-zero
  if (physicalDisk->sectorSize == 0)
    {
      kernelError(kernel_error, "Disk sector size is zero");
      return (status = ERR_INVALID);
    }

  sectors = ((MEMORY_PAGE_SIZE / physicalDisk->sectorSize) + 
	     ((MEMORY_PAGE_SIZE % physicalDisk->sectorSize)? 1 : 0));

  // Read the $Boot file
  status = kernelDiskReadSectors((char *) theDisk->name, 0, sectors, header);
  return (status);
}


static int writeSwapHeader(const kernelDisk *theDisk, linuxSwapHeader *header)
{
  // This simple function will write the swap header from the supplied
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

  sectors = ((MEMORY_PAGE_SIZE / physicalDisk->sectorSize) + 
	     ((MEMORY_PAGE_SIZE % physicalDisk->sectorSize)? 1 : 0));

  // Read the $Boot file
  status = kernelDiskWriteSectors((char *) theDisk->name, 0, sectors, header);
  return (status);
}


static int detect(kernelDisk *theDisk)
{
  // This function is used to determine whether the data on a disk structure
  // is using a Linux swap filesystem.  Returns 1 for true, 0 for false,  and
  // negative if it encounters an error

  int status = 0;
  unsigned char sectBuff[MEMORY_PAGE_SIZE];
  linuxSwapHeader *header = (linuxSwapHeader *) sectBuff;

  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check params
  if (theDisk == NULL)
    {
      kernelError(kernel_error, "Disk structure is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  // Read the swap header
  status = readSwapHeader(theDisk, header);
  if (status < 0)
    // Not linux-swap
    return (status = 0);

  // Check for the signature
  if (!strncmp(header->magic.magic, "SWAP-SPACE", 10) ||
      !strncmp(header->magic.magic, "SWAPSPACE2", 10))
    {
      // Linux-swap
      strcpy((char *) theDisk->fsType, "linux-swap");
      return (status = 1);
    }
  else
    // Not linux-swap
    return (status = 0);
}


static int clobber(kernelDisk *theDisk)
{
  // This function destroys anything that might cause this disk to be detected
  // as having an NTFS filesystem.

  int status = 0;
  unsigned char sectBuff[MEMORY_PAGE_SIZE];
  linuxSwapHeader *header = (linuxSwapHeader *) sectBuff;

  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check params
  if (theDisk == NULL)
    {
      kernelError(kernel_error, "Disk structure is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  status = readSwapHeader(theDisk, header);
  if (status < 0)
    return (status);

  bzero(header->magic.magic, 10);

  status = writeSwapHeader(theDisk, header);
  return (status);
}


static kernelFilesystemDriver defaultLinuxSwapDriver = {
  "linux-swap", // Driver name
  detect,
  NULL,  // driverFormat
  clobber,
  NULL,  // driverCheck
  NULL,  // driverDefragment
  NULL,  // driverStat
  NULL,  // driverGetResizeConstraints
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


int kernelFilesystemLinuxSwapInitialize(void)
{
  // Initialize the driver

  int status = 0;
  
  // Register our driver
  status = kernelDriverRegister(linuxSwapDriver, &defaultLinuxSwapDriver);

  initialized = 1;

  return (status);
}
