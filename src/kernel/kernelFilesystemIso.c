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
//  kernelFilesystemIso.c
//

// This file contains the routines designed to interpret the ISO9660
// filesystem (commonly found on Linux disks)

#include "kernelFilesystemIso.h"
#include "kernelFile.h"
#include "kernelDriverManagement.h"
#include "kernelMalloc.h"
#include "kernelMiscFunctions.h"
#include "kernelError.h"
#include <string.h>
#include <stdlib.h>
#include <sys/errors.h>


static kernelFilesystemDriver defaultIsoDriver = {
  Iso,   // FS type
  "ISO", // Driver name
  kernelFilesystemIsoInitialize,
  kernelFilesystemIsoDetect,
  NULL, // driverFormat
  NULL, // driverCheck
  NULL, // driverDefragment
  kernelFilesystemIsoMount,
  kernelFilesystemIsoUnmount,
  kernelFilesystemIsoGetFree,
  kernelFilesystemIsoNewEntry,
  kernelFilesystemIsoInactiveEntry,
  kernelFilesystemIsoResolveLink,
  kernelFilesystemIsoReadFile,
  NULL, // driverWriteFile
  NULL, // driverCreateFile
  NULL, // driverDeleteFile
  NULL, // driverFileMoved
  kernelFilesystemIsoReadDir,
  NULL, // driverWriteDir
  NULL, // driverMakeDir
  NULL, // driverRemoveDir
  NULL // driverTimestamp
};

// These hold free private data memory
//static isoDirectoryRecord *freeDirectoryRecords = NULL;
//static unsigned numFreeDirectoryRecords = 0;

static int initialized = 0;


static int readPrimaryVolDesc(const kernelDisk *theDisk, unsigned char *buffer)
{
  // This simple function will read the primary volume descriptor into the
  // supplied buffer.  Returns 0 on success, negative on error.

  int status = 0;
  kernelPhysicalDisk *physicalDisk = theDisk->physical;

  // Do a dummy read from the CD-ROM to ensure that the TOC has been properly
  // read, and therefore the information for the last session is available.
  status = kernelDiskReadSectors((char *) theDisk->name,
				 ISO_PRIMARY_VOLDESC_SECTOR, 1, buffer);
  if (status < 0)
    return (status);
  
  // The sector size must be non-zero
  if (physicalDisk->sectorSize == 0)
    {
      kernelError(kernel_error, "Disk sector size is zero");
      return (status = ERR_INVALID);
    }

  // Initialize the buffer we were given
  kernelMemClear(buffer, physicalDisk->sectorSize);
     
  // Read the primary volume descriptor
  status = kernelDiskReadSectors((char *) theDisk->name,
				 (physicalDisk->lastSession + 
				  ISO_PRIMARY_VOLDESC_SECTOR), 1, buffer);
  if (status < 0)
    {
      kernelError(kernel_error, "Unable to read the ISO primary volume "
		  "descriptor");
	  return (status);
    }
  
  return (status = 0);
}


static void readDirRecord(unsigned char *buffer, isoDirectoryRecord *dirRec)
{
  // Reads a directory record from the on-disk structure in 'buffer' to
  // our structure

  int count;

  // Get root directory record from the buffer
  dirRec->dirRecLength = *((unsigned char *) buffer);
  dirRec->extAttrRecLength = *((unsigned char *)(buffer + 1));
  dirRec->blockNumber = *((unsigned *)(buffer + 2)); // 8 bytes - fix.
  dirRec->fileSize = *((unsigned *)(buffer + 10));   // 8 bytes - fix.
  strncpy((unsigned char *) dirRec->dateTime, (buffer + 18), 7);
  dirRec->flags = *((unsigned char *)(buffer + 25));
  dirRec->unitSize = *((unsigned char *)(buffer + 26));
  dirRec->intrGapSize = *((unsigned char *)(buffer + 27));
  dirRec->nameLength = *((unsigned char *)(buffer + 32));
  dirRec->volSeqNumber = *((unsigned *)(buffer + 28));
  strncpy((char *) dirRec->name, (buffer + 33), dirRec->nameLength);
  dirRec->name[dirRec->nameLength] = '\0';

  // Find the semicolon (if any) at the end of the name
  for (count = (dirRec->nameLength - 1); count > 0; count --)
    if (dirRec->name[count] == ';')
      {
	dirRec->name[count] = '\0';
	break;
      }

  dirRec->versionNumber =
    atoi((char *)(dirRec->name + strlen((char *) dirRec->name) + 1));
}


static isoInternalData *getIsoData(kernelFilesystem *filesystem)
{
  // This function reads the filesystem parameters from the superblock

  int status = 0;
  unsigned char *buffer = NULL;
  isoInternalData *isoData = filesystem->filesystemData;
  kernelPhysicalDisk *physicalDisk = NULL;

  // Have we already read the parameters for this filesystem?
  if (isoData != NULL)
    return (isoData);

  physicalDisk = filesystem->disk->physical;

  // Get a temporary buffer to read the primary volume descriptor
  buffer = kernelMalloc(physicalDisk->sectorSize);
  if (buffer == NULL)
    {
      kernelError(kernel_error, "Unable to allocate buffer");
      return (isoData = NULL);
    }

  // Read the superblock into our isoInternalData buffer
  status = readPrimaryVolDesc(filesystem->disk, buffer);
  if (status < 0)
    {
      kernelFree(buffer);
      return (isoData = NULL);
    }

  // Make sure it's a primary volume descriptor
  if (buffer[0] != (unsigned char) ISO_DESCRIPTORTYPE_PRIMARY)
    {
      kernelError(kernel_error, "Primary volume descriptor not found");
      kernelFree(buffer);
      return (isoData = NULL);
    }

  // We must allocate some new memory to hold information about
  // the filesystem
  isoData = kernelMalloc(sizeof(isoInternalData));
  if (isoData == NULL)
    {
      kernelError(kernel_error, "Unable to allocate ISO data memory");
      kernelFree(buffer);
      return (isoData = NULL);
    }

  // Set our filesystem data
  strncpy((char *) isoData->volumeIdentifier, (buffer + 40), 31);
  isoData->volumeIdentifier[31] = '\0';
  // MSB data
  isoData->volumeBlocks = *((unsigned *)(buffer + 80));
  isoData->blockSize = *((unsigned short *)(buffer + 128));
  isoData->pathTableSize = *((unsigned *)(buffer + 132));
  isoData->pathTableBlock = *((unsigned *)(buffer + 140));

  // Get the root directory record
  readDirRecord((buffer + 156), &(isoData->rootDirRec));

  // How many blocks for the root directory?
  unsigned rootDirBlocks = (isoData->rootDirRec.fileSize / isoData->blockSize);
  if (isoData->rootDirRec.fileSize % isoData->blockSize)
    rootDirBlocks += 1;

  // We don't need this any more.
  kernelFree(buffer);

  // Attach the disk structure to the isoData structure
  isoData->disk = filesystem->disk;

  // Attach our new FS data to the filesystem structure
  filesystem->filesystemData = (void *) isoData;

  // Specify the filesystem block size
  filesystem->blockSize = isoData->blockSize;

  return (isoData);
}


static void makeSystemTime(unsigned char *isoTime, unsigned *date,
			   unsigned *time)
{
  // This function takes an ISO date/time value and returns the equivalent in
  // packed-BCD system format.

  // The year
  *date = ((isoTime[0] + 1900) << 9);

  // The month (1-12)
  *date |= ((isoTime[1] & 0x0F) << 5);

  // Day of the month (1-31)
  *date |= (isoTime[2] & 0x1F);

  // The hour
  *time = ((isoTime[3] & 0x3F) << 12);

  // The minute
  *time |= ((isoTime[4] & 0x3F) << 6);

  // The second
  *time |= (isoTime[5] & 0x3F);

  return;
}


static int scanDirectory(isoInternalData *isoData, kernelFileEntry *dirEntry)
{
  int status = 0;
  isoDirectoryRecord *scanDirRec = NULL;
  unsigned bufferSize = 0;
  void *buffer = NULL;
  void *ptr = NULL;
  kernelFileEntry *fileEntry = NULL;
  isoDirectoryRecord entryDirRec;

  // Make sure it's really a directory, and not a regular file
  if (dirEntry->type != dirT)
    {
      kernelError(kernel_error, "Entry to scan is not a directory");
      return (status = ERR_NOTADIR);
    }

  // Manufacture some "." and ".." entries
  status = kernelFileMakeDotDirs(dirEntry->parentDirectory, dirEntry);
  if (status < 0)
    kernelError(kernel_warn, "Unable to create '.' and '..' directory "
 		"entries");

  scanDirRec = (isoDirectoryRecord *) dirEntry->driverData;

  bufferSize = (dirEntry->blocks * isoData->blockSize);
  if (bufferSize < scanDirRec->fileSize)
    {
      kernelError(kernel_error, "Wrong buffer size for directory!");
      return (status = ERR_BADDATA);
    }

  // Get a buffer for the directory
  buffer = kernelMalloc(bufferSize);
  if (buffer == NULL)
    {
      kernelError(kernel_error, "Unable to get memory for directory buffer");
      return (status = ERR_MEMORY);
    }

  status = kernelDiskReadSectors((char *) isoData->disk->name,
				 scanDirRec->blockNumber, dirEntry->blocks,
				 buffer);
  if (status < 0)
    {
      kernelFree(buffer);
      return (status);
    }

  // Loop through the contents
  ptr = buffer; 
  while (ptr < (buffer + bufferSize))
    {
      kernelMemClear((void *) &entryDirRec, sizeof(isoDirectoryRecord));
      readDirRecord(ptr, &entryDirRec);

      if (!(entryDirRec.dirRecLength))
	{
	  // This is a NULL entry.  If the next entry doesn't fit within
	  // the same logical sector, it is placed in the next one.  Thus,
	  // if we are not within the last sector we read, skip to the next
	  // one.
	  if ((((ptr - buffer) / isoData->blockSize) + 1) < dirEntry->blocks)
	    {
	      ptr += (isoData->blockSize -
		      ((ptr - buffer) % isoData->blockSize));
	      continue;
	    }
	  else
	    break;
	}

      if ((entryDirRec.name[0] < 32) || (entryDirRec.name[0] > 126))
	{
          if ((entryDirRec.name[0] != 0) && (entryDirRec.name[0] != 1))
            // Not the current directory, or the parent directory.  Warn about
            // funny ones like this.
            kernelError(kernel_warn, "Unknown directory entry type in %s",
                        dirEntry->name);
	  ptr += entryDirRec.dirRecLength;
	  continue;
	}

      // Normal entry
      fileEntry = kernelFileNewEntry(dirEntry->filesystem);
      if ((fileEntry == NULL) || (fileEntry->driverData == NULL))
        {
          kernelError(kernel_error, "Unable to get new filesystem entry or "
                          "entry has no private data");
          kernelFree(buffer);
          return (status = ERR_NOCREATE);
        }
     
      // Copy the name into the file entry
      strncpy((char *) fileEntry->name, (char *) entryDirRec.name,
              MAX_NAME_LENGTH);

      fileEntry->type = fileT;

      if (entryDirRec.flags & ISO_FLAGMASK_DIRECTORY)
        fileEntry->type = dirT;

      if (entryDirRec.flags & ISO_FLAGMASK_ASSOCIATED)
        fileEntry->type = linkT;

      makeSystemTime((unsigned char *) entryDirRec.dateTime,
                     (unsigned *) &(fileEntry->creationDate),
                     (unsigned *) &(fileEntry->creationTime));
      fileEntry->accessedTime = fileEntry->creationTime;
      fileEntry->accessedDate = fileEntry->creationDate;
      fileEntry->modifiedTime = fileEntry->creationTime;
      fileEntry->modifiedDate = fileEntry->creationDate;
      fileEntry->size = entryDirRec.fileSize;
      fileEntry->blocks = (entryDirRec.fileSize / isoData->blockSize);
      if (entryDirRec.fileSize % isoData->blockSize)
        fileEntry->blocks += 1;
      fileEntry->lastAccess = kernelSysTimerRead();

      // Copy the directory record data into the private data area
      kernelMemCopy((void *) &entryDirRec, fileEntry->driverData,
                    sizeof(isoDirectoryRecord)); 

      // Add it to the directory
      status = kernelFileInsertEntry(fileEntry, dirEntry);
      if (status < 0)
        {
          kernelFree(buffer);
          return (status);
        }

      ptr += entryDirRec.dirRecLength;
    }

  kernelFree(buffer);
  return (status = 0);
}


static int readRootDir(isoInternalData *isoData, kernelFilesystem *filesystem)
{
  // This function reads the root directory and attaches it to the
  // filesystem structure.

  int status = 0;
  isoDirectoryRecord *rootDirRec =
    (isoDirectoryRecord *) filesystem->filesystemRoot->driverData;
  if (rootDirRec == NULL)
    {
      kernelError(kernel_error, "Root entry has no private data");
      return (status = ERR_NODATA);
    }

  // Copy the directory record for the root directory from the isoData
  kernelMemCopy((void *) &(isoData->rootDirRec), (void *) rootDirRec,
		sizeof(isoDirectoryRecord));

  // Set the directory size
  filesystem->filesystemRoot->size = rootDirRec->fileSize;
  filesystem->filesystemRoot->blocks =
    (rootDirRec->fileSize / isoData->blockSize);
  if (rootDirRec->fileSize % isoData->blockSize)
    filesystem->filesystemRoot->blocks += 1;

  return (status = scanDirectory(isoData, filesystem->filesystemRoot));
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
//  Below here, the functions are exported for isoernal use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


int kernelFilesystemIsoInitialize(void)
{
  // Initialize the driver

  int status = 0;
  
  // Register our driver
  status = kernelDriverRegister(isoDriver, &defaultIsoDriver);

  initialized = 1;

  return (status);
}


int kernelFilesystemIsoDetect(const kernelDisk *theDisk)
{
  // This function is used to determine whether the data on a disk structure
  // is using an ISO filesystem.  It just looks for a 'magic number' on the
  // disk to identify ISO.  Any data that it gathers is discarded when the
  // call terminates.  It returns 1 for true, 0 for false, and negative if
  // it encounters an error

  int status = 0;
  kernelPhysicalDisk *physicalDisk = NULL;
  char *buffer = NULL;
  int isIso = 0;
  
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check params
  if (theDisk == NULL)
    {
      kernelError(kernel_error, "Disk structure is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  // Get the physical disk
  physicalDisk = (kernelPhysicalDisk *) theDisk->physical;

  // Get a temporary buffer to read the primary volume descriptor
  buffer = kernelMalloc(physicalDisk->sectorSize);
  if (buffer == NULL)
    {
      kernelError(kernel_error, "Unable to allocate buffer");
      return (status = ERR_MEMORY);
    }

  // Read the primary volume descriptor
  status = readPrimaryVolDesc(theDisk, buffer);
  if (status < 0)
    {
      kernelFree(buffer);
      return (status);
    }

  // Check for the standard identifier
  if (!strncmp((buffer + 1), ISO_STANDARD_IDENTIFIER,
	       strlen(ISO_STANDARD_IDENTIFIER)))
    isIso = 1;

  kernelFree(buffer);

  return (isIso);
}


int kernelFilesystemIsoMount(kernelFilesystem *filesystem)
{
  // This function initializes the filesystem driver by gathering all of
  // the required information from the boot sector.  In addition, it
  // dynamically allocates memory space for the "used" and "free" file and
  // directory structure arrays.

  int status = 0;
  isoInternalData *isoData = NULL;
  
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Make sure the filesystem isn't NULL
  if (filesystem == NULL)
    {
      kernelError(kernel_error, "NULL filesystem structure");
      return (status = ERR_NULLPARAMETER);
    }

  // The filesystem data cannot exist
  filesystem->filesystemData = NULL;

  // Get the ISO data for the requested filesystem.  We don't need the info
  // right now -- we just want to collect it.
  isoData = getIsoData(filesystem);
  if (isoData == NULL)
    return (status = ERR_BADDATA);

  // Read the filesystem's root directory and attach it to the filesystem
  // structure
  status = readRootDir(isoData, filesystem);
  if (status < 0)
    {
      kernelError(kernel_error, "Unable to read the filesystem's root "
                  "directory");
      return (status = ERR_BADDATA);
    }

  // Set the proper filesystem type name on the disk structure
  strcpy((char *) filesystem->disk->fsType, "iso9660");

  // Read-only
  filesystem->readOnly = 1;

  return (status = 0);
}


int kernelFilesystemIsoUnmount(kernelFilesystem *filesystem)
{
  // This function releases all of the stored information about a given
  // filesystem.

  int status = 0;
  
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check params
  if (filesystem == NULL)
    {
      kernelError(kernel_error, "NULL filesystem structure");
      return (status = ERR_NULLPARAMETER);
    }

  return (status = 0);
}


unsigned kernelFilesystemIsoGetFree(kernelFilesystem *filesystem)
{
  // This function returns the amount of free disk space, in bytes,
  // which is always zero.
  return (0);
}


int kernelFilesystemIsoNewEntry(kernelFileEntry *newEntry)
{
  // This function gets called when there's a new kernelFileEntry in the
  // filesystem (either because a file was created or because some existing
  // thing has been newly read from disk).  This gives us an opportunity
  // to attach ISO-specific data to the file entry

  int status = 0;
  //isoDirectoryRecord *directoryRecord = NULL;
  //isoDirectoryRecord *newDirectoryRecords = NULL;
  //int count;
  
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check params
  if (newEntry == NULL)
    {
      kernelError(kernel_error, "NULL file entry");
      return (status = ERR_NULLPARAMETER);
    }

  // Make sure there isn't already some sort of data attached to this
  // file entry, and that there is a filesystem attached
  if (newEntry->driverData != NULL)
    {
      kernelError(kernel_error, "Entry already has private filesystem data");
      return (status = ERR_ALREADY);
    }

  // Make sure there's an associated filesystem
  if (newEntry->filesystem == NULL)
    {
      kernelError(kernel_error, "Entry has no associated filesystem");
      return (status = ERR_NOCREATE);
    }

  /*
  if (freeDirectoryRecords == 0)
    {
      // Allocate memory for directory records
      newDirectoryRecords = kernelMalloc(sizeof(isoDirectoryRecord) *
					 MAX_BUFFERED_FILES);
      if (newDirectoryRecords == NULL)
	{
	  kernelError(kernel_error, "Error allocating memory for ISO "
		      "directory records");
	  return (status = ERR_MEMORY);
	}

      // Initialize the new isoDirectoryRecord structures.

      for (count = 0; count < (MAX_BUFFERED_FILES - 1); count ++)
	newDirectoryRecords[count].next = (void *)
	  &(newDirectoryRecords[count + 1]);

      // The free directory records are the new memory
      freeDirectoryRecords = newDirectoryRecords;

      // Add the number of new file entries
      numFreeDirectoryRecords = MAX_BUFFERED_FILES;
    }

  directoryRecord = freeDirectoryRecords;
  freeDirectoryRecords = (isoDirectoryRecord *) directoryRecord->next;
  numFreeDirectoryRecords -= 1;
  newEntry->driverData = (void *) directoryRecord;
  */

  newEntry->driverData = kernelMalloc(sizeof(isoDirectoryRecord));

  return (status = 0);
}


int kernelFilesystemIsoInactiveEntry(kernelFileEntry *inactiveEntry)
{
  // This function gets called when a kernelFileEntry is about to be
  // deallocated by the system (either because a file was deleted or because
  // the entry is simply being unbuffered).  This gives us an opportunity
  // to deallocate our ISO-specific data from the file entry

  int status = 0;
  
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check params
  if (inactiveEntry == NULL)
    {
      kernelError(kernel_error, "NULL file entry");
      return (status = ERR_NULLPARAMETER);
    }

  // Need to actually deallocate memory here.
  kernelFree(inactiveEntry->driverData);

  // Remove the reference
  inactiveEntry->driverData = NULL;

  return (status = 0);
}


int kernelFilesystemIsoResolveLink(kernelFileEntry *linkEntry)
{
  // This is called by the kernelFile.c code when we have registered a
  // file or directory as a link, but not resolved it, and now it needs
  // to be resolved.  By default this driver never resolves ISO symbolic
  // links until they are needed, so this will get called when the system
  // wants to read/write the symbolically-linked directory or file.

  int status = 0;

  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check params
  if (linkEntry == NULL)
    {
      kernelError(kernel_error, "Link entry is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  return (status = 0);
}


int kernelFilesystemIsoReadFile(kernelFileEntry *theFile, unsigned blockNum,
				unsigned blocks, unsigned char *buffer)
{
  // This function is the "read file" function that the filesystem
  // driver exports to the world.  It is mainly a wrapper for the
  // internal function of the same purpose, but with some additional
  // argument checking.  Returns 0 on success, negative otherwise.

  int status = 0;
  isoInternalData *isoData = NULL;
  isoDirectoryRecord *dirRec = NULL;

  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check params
  if ((theFile == NULL) || (buffer == NULL))
    {
      kernelError(kernel_error, "Null file or buffer parameter");
      return (status = ERR_NULLPARAMETER);
    }

  // Make sure there's a directory record  attached
  dirRec = (isoDirectoryRecord *) theFile->driverData;
  if (dirRec == NULL)
    {
      kernelError(kernel_error, "File \"%s\" has no private data",
		  theFile->name);
      return (status = ERR_NODATA);
    }

  // Get the ISO data for the filesystem.
  isoData = getIsoData(theFile->filesystem);
  if (isoData == NULL)
    return (status = ERR_BADDATA);

  status = kernelDiskReadSectors((char *) isoData->disk->name,
				 dirRec->blockNumber, theFile->blocks,
				 buffer);
  return (status);
}


int kernelFilesystemIsoReadDir(kernelFileEntry *directory)
{
  // This function receives an emtpy file entry structure, which represents
  // a directory whose contents have not yet been read.  This will fill the
  // directory structure with its appropriate contents.  Returns 0 on
  // success, negative otherwise.

  int status = 0;
  isoInternalData *isoData = NULL;
  
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check params
  if (directory == NULL)
    {
      kernelError(kernel_error, "Directory parameter is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  // Make sure there's a directory record  attached
  if (directory->driverData == NULL)
    {
      kernelError(kernel_error, "Directory \"%s\" has no private data",
		  directory->name);
      return (status = ERR_NODATA);
    }

  // Get the ISO data for the filesystem.
  isoData = getIsoData(directory->filesystem);
  if (isoData == NULL)
    return (status = ERR_BADDATA);

  return (scanDirectory(isoData, directory));
}