//
//  Visopsys
//  Copyright (C) 1998-2011 J. Andrew McLaughlin
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
#include "kernelDebug.h"
#include "kernelError.h"
#include "kernelMalloc.h"
#include "kernelMisc.h"
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

  kernelMemClear(bootFile.oemName, 8);

  status = writeBootFile(theDisk, &bootFile);
  return (status);
}


#if 0
static int mount(kernelDisk *theDisk)
{
  int status = 0;
  ntfsBootFile bootFile;
  int sectorsPerFileRecord = 0;
  ntfsFileRecord *fileRecord = NULL;
  ntfsAttributeHeader *attrHeader = NULL;
  char *attrTypeName = NULL;
  ntfsFilenameAttribute *filenameAttribute = NULL;
  unsigned long long count1;
  unsigned count2; //, count3;

  status = readBootFile(theDisk, &bootFile);
  if (status < 0)
    return (status);

  bootFile.oemName[7] = '\0';
  kernelDebug(debug_fs, "oemName=\"%s\"\n", bootFile.oemName);
  kernelDebug(debug_fs, "bytesPerSect=%d\n", bootFile.bytesPerSect);
  kernelDebug(debug_fs, "sectsPerClust=%d\n", bootFile.sectsPerClust);
  kernelDebug(debug_fs, "media=%02x\n", bootFile.media);
  kernelDebug(debug_fs, "sectsPerTrack=%d\n", bootFile.sectsPerTrack);
  kernelDebug(debug_fs, "numHeads=%d\n", bootFile.numHeads);
  kernelDebug(debug_fs, "biosDriveNum=%04x\n", bootFile.biosDriveNum);
  kernelDebug(debug_fs, "sectsPerVolume=%llu\n", bootFile.sectsPerVolume);
  kernelDebug(debug_fs, "mftStart=%llu\n", bootFile.mftStart);
  kernelDebug(debug_fs, "mftMirrStart=%llu\n", bootFile.mftMirrStart);
  kernelDebug(debug_fs, "clustersPerMftRec=%u\n", bootFile.clustersPerMftRec);
  kernelDebug(debug_fs, "clustersPerIndexRec=%u\n",
	      bootFile.clustersPerIndexRec);
  kernelDebug(debug_fs, "volSerial=%llu\n", bootFile.volSerial);

  sectorsPerFileRecord = (bootFile.sectsPerClust * bootFile.clustersPerMftRec);

  // Now we can alloc a buffer for file records based on the number of
  // sectors per cluster and clusters per MFT record
  fileRecord = kernelMalloc(bootFile.bytesPerSect * sectorsPerFileRecord);
  if (!fileRecord)
    {
      status = ERR_MEMORY;
      goto out;
    }

  // Start reading the MFT
  for (count1 = (bootFile.mftStart * bootFile.sectsPerClust); ;
       count1 += bootFile.sectsPerClust)
    {
      kernelDebug(debug_fs, "FILE record %llu\n",
	     (count1 - (bootFile.mftStart * bootFile.sectsPerClust)));

      status = kernelDiskReadSectors((char *) theDisk->name, count1,
				     sectorsPerFileRecord, fileRecord);
      if (status < 0)
	goto out;

      if (fileRecord->magic != NTFS_MAGIC_FILERECORD)
	{
	  kernelDebugError("File record magic is not FILE");
	  break;
	}

      attrHeader = (((void *) fileRecord) + fileRecord->attrSeqOffset);
      for (count2 = 0; (attrHeader->type != NTFS_ATTR_TERMINATE); count2 ++)
	{
	  switch (attrHeader->type)
	    {
	    case NTFS_ATTR_STANDARDINFO:
	      attrTypeName = "NTFS_ATTR_STANDARDINFO";
	      break;
	    case NTFS_ATTR_ATTRLIST:
	      attrTypeName = "NTFS_ATTR_ATTRLIST";
	      break;
	    case NTFS_ATTR_FILENAME:
	      attrTypeName = "NTFS_ATTR_FILENAME";
	      break;
	    case NTFS_ATTR_OBJECTID:
	      attrTypeName = "NTFS_ATTR_OBJECTID";
	      break;
	    case NTFS_ATTR_SECURITYDESC:
	      attrTypeName = "NTFS_ATTR_SECURITYDESC";
	      break;
	    case NTFS_ATTR_VOLUMENAME:
	      attrTypeName = "NTFS_ATTR_VOLUMENAME";
	      break;
	    case NTFS_ATTR_VOLUMEINFO:
	      attrTypeName = "NTFS_ATTR_VOLUMEINFO";
	      break;
	    case NTFS_ATTR_DATA:
	      attrTypeName = "NTFS_ATTR_DATA";
	      break;
	    case NTFS_ATTR_INDEXROOT:
	      attrTypeName = "NTFS_ATTR_INDEXROOT";
	      break;
	    case NTFS_ATTR_INDEXALLOC:
	      attrTypeName = "NTFS_ATTR_INDEXALLOC";
	      break;
	    case NTFS_ATTR_BITMAP:
	      attrTypeName = "NTFS_ATTR_BITMAP";
	      break;
	    case NTFS_ATTR_REPARSEPOINT:
	      attrTypeName = "NTFS_ATTR_REPARSEPOINT";
	      break;
	    case NTFS_ATTR_EAINFO:
	      attrTypeName = "NTFS_ATTR_EAINFO";
	      break;
	    case NTFS_ATTR_EA:
	      attrTypeName = "NTFS_ATTR_EA";
	      break;
	    case NTFS_ATTR_PROPERTYSET:
	      attrTypeName = "NTFS_ATTR_PROPERTYSET";
	      break;
	    case NTFS_ATTR_LOGGEDUTILSTR:
	      attrTypeName = "NTFS_ATTR_LOGGEDUTILSTR";
	      break;
	    default:
	      attrTypeName = "(unknown)";
	      break;
	    }

	  kernelDebug(debug_fs, "  ATTR type %02x %s, %sresident\n",
		      attrHeader->type, attrTypeName,
		      (attrHeader->nonResident? "non" : ""));

	  if (!attrHeader->nonResident &&
	      (attrHeader->type == NTFS_ATTR_FILENAME))
	    {
	      filenameAttribute = (((void *) attrHeader) +
				   attrHeader->res.yes.attributeOffset);

	      /*
	      kernelDebug(debug_fs, "    filename: ");
	      for (count3 = 0; count3 < filenameAttribute->filenameLength;
		   count3 ++)
		kernelDebug(debug_fs, "%c",
			    filenameAttribute->filename[count3]);
	      kernelDebug(debug_fs, "\n");
	      */
	    }

	  attrHeader = (((void *) attrHeader) + attrHeader->length);
	}
    }

  status = 0;

 out:
  if (fileRecord)
    kernelFree(fileRecord);

  return (status);
}
#endif


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
  NULL,  // driverTimestamp
  NULL   // driverSetBlocks
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
