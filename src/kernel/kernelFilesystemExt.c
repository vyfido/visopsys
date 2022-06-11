//
//  Visopsys
//  Copyright (C) 1998-2003 J. Andrew McLaughlin
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
//  kernelFilesystemExt.c
//

// This file contains the routines designed to interpret the EXT2 filesystem
// (commonly found on Linux disks)

#include "kernelFilesystemExt.h"
#include "kernelFilesystem.h"
#include "kernelFile.h"
#include "kernelDriverManagement.h"
#include "kernelMalloc.h"
#include "kernelMultitasker.h"
#include "kernelMiscFunctions.h"
#include "kernelError.h"
#include <stdio.h>
#include <string.h>
#include <sys/errors.h>


static kernelFilesystemDriver defaultExtDriver = {
  Ext,   // FS type
  "EXT", // Driver name
  kernelFilesystemExtInitialize,
  kernelFilesystemExtDetect,
  kernelFilesystemExtFormat,
  kernelFilesystemExtCheck,
  kernelFilesystemExtDefragment,
  kernelFilesystemExtMount,
  kernelFilesystemExtUnmount,
  kernelFilesystemExtGetFreeBytes,
  kernelFilesystemExtNewEntry,
  kernelFilesystemExtInactiveEntry,
  kernelFilesystemExtResolveLink,
  kernelFilesystemExtReadFile,
  kernelFilesystemExtWriteFile,
  kernelFilesystemExtCreateFile,
  kernelFilesystemExtDeleteFile,
  kernelFilesystemExtFileMoved,
  kernelFilesystemExtReadDir,
  kernelFilesystemExtWriteDir,
  kernelFilesystemExtMakeDir,
  kernelFilesystemExtRemoveDir,
  kernelFilesystemExtTimestamp
};

// These hold free private data memory
static extInodeData *freeInodeDatas = NULL;
static unsigned numFreeInodeDatas = 0;

static int initialized = 0;


static int readSuperblock(const kernelDisk *theDisk, unsigned char *buffer)
{
  // This simple function will read the superblock into the supplied buffer
  // and ensure that it is (at least trivially) valid.  Returns 0 on success,
  // negative on error.

  int status = 0;
  kernelPhysicalDisk *physicalDisk = NULL;

  // Initialize the buffer we were given
  kernelMemClear(buffer, EXT_SUPERBLOCK_SIZE);

  physicalDisk = theDisk->physical;

  // Read the superblock
  status =
    kernelDiskReadSectors((char *) theDisk->name, EXT_SUPERBLOCK_SECTOR,
			  (EXT_SUPERBLOCK_SIZE / physicalDisk->sectorSize),
			  buffer);
  if (status < 0)
    {
      kernelError(kernel_error, "Unable to read the EXT superblock");
      return (status);
    }

  // Check for the EXT magic number
  if (*((unsigned short *)(buffer + EXT_MAGICNUMBER_OFFSET)) !=
      (unsigned short) EXT_MAGICNUMBER)
    // Not EXT2
    return (status = ERR_BADDATA);

  return (status = 0);
}


static inline unsigned getSectorNumber(extInternalData *extData,
                                       unsigned blockNum)
{
  // Given a filesystem and a block number, calculate the sector number
  // (which is dependent on filesystem block size)
  return (blockNum * extData->sectorsPerBlock); 
}


static extInternalData *getExtData(kernelFilesystem *filesystem)
{
  // This function reads the filesystem parameters from the superblock

  int status = 0;
  unsigned char *buffer = NULL;
  extInternalData *extData = filesystem->filesystemData;
  kernelPhysicalDisk *physicalDisk = NULL;
  unsigned groupDescriptorBlocks = 0;

  // Have we already read the parameters for this filesystem?
  if (extData != NULL)
    return (extData);

  // Get a temporary buffer to read the superblock
  buffer = kernelMalloc(EXT_SUPERBLOCK_SIZE);
  if (buffer == NULL)
    {
      kernelError(kernel_error, "Unable to allocate superblock buffer");
      return (extData = NULL);
    }

  // Read the superblock into our extInternalData buffer
  status = readSuperblock(filesystem->disk, buffer);
  if (status < 0)
    {
      kernelFree(buffer);
      return (extData = NULL);
    }

  // We must allocate some new memory to hold information about
  // the filesystem
  extData = kernelMalloc(sizeof(extInternalData));
  if (extData == NULL)
    {
      kernelError(kernel_error, "Unable to allocate EXT data memory");
      kernelFree(buffer);
      return (extData = NULL);
    }

  // Copy the data part of the superblock into our internal data structure,
  // which matches the superblock
  kernelMemCopy(buffer, (void *) extData, EXT_SUPERBLOCK_DATASIZE);

  kernelFree(buffer);

  // Check that the inode size is the same size as our structure
  if (extData->inode_size != sizeof(extInode))
    kernelError(kernel_warn, "Inode size (%u) does not match structure "
                "size (%u)", extData->inode_size, sizeof(extInode));

  physicalDisk = filesystem->disk->physical;

  extData->blockSize = (1024 << extData->log_block_size);

  // Save the sectors per block so we don't have to keep calculating it
  extData->sectorsPerBlock = (extData->blockSize / physicalDisk->sectorSize);

  // Calculate the number of block groups
  extData->numGroups = (extData->blocks_count / extData->blocks_per_group);

  // Attach the disk structure to the extData structure
  extData->disk = filesystem->disk;

  groupDescriptorBlocks = ((extData->numGroups * sizeof(extGroupDescriptor)) /
			   extData->blockSize);
  if ((extData->numGroups * sizeof(extGroupDescriptor)) % extData->blockSize)
    groupDescriptorBlocks++;

  // Get a new temporary buffer to read the group descriptors block
  buffer = kernelMalloc(groupDescriptorBlocks * extData->blockSize);
  if (buffer == NULL)
    {
      kernelError(kernel_error, "Unable to allocate superblock buffer");
      kernelFree((void *) extData);
      return (extData = NULL);
    }

  // Read the group descriptors starting at block 2 into our structures
  status = kernelDiskReadSectors((char *) extData->disk->name,
                                 extData->sectorsPerBlock,
                                 (groupDescriptorBlocks *
                                  extData->sectorsPerBlock), buffer);
  if (status < 0)
    {
      kernelError(kernel_error, "Unable to read EXT group descriptors");
      kernelFree((void *) extData);
      kernelFree(buffer);
      return (extData = NULL);
    }

  // Get some memory for our array of group descriptors
  extData->groups =
    kernelMalloc(extData->numGroups * sizeof(extGroupDescriptor));
  if (extData->groups == NULL)
    {
      kernelError(kernel_error, "Unable to allocate EXT group descriptors "
                  "memory");
      kernelFree((void *) extData);
      kernelFree(buffer);
      return (extData = NULL);
    }

  // Copy the group descriptor data into our internal data structure
  kernelMemCopy(buffer, (void *) extData->groups,
                (extData->numGroups * sizeof(extGroupDescriptor)));

  kernelFree(buffer);

  // Attach our new FS data to the filesystem structure
  filesystem->filesystemData = (void *) extData;

  // Specify the filesystem block size
  filesystem->blockSize = extData->blockSize;

  return (extData);
}


static int readInode(extInternalData *extData, unsigned number,
		     extInode *inode)
{
  // Reads the requested inode structure from disk

  int status = 0;
  unsigned groupNumber = 0;
  unsigned inodeTableBlock = 0;
  unsigned char *buffer = NULL;

  if ((number < 1) || (number > extData->inodes_count))
    {
      kernelError(kernel_error, "Invalid inode number %u", number);
      return (status = ERR_BOUNDS);
    }

  // We use the number as a base-zero but the filesystem counts from 1.
  number--;

  // Calculate the group number
  groupNumber = (number / extData->inodes_per_group);

  // Calculate the relevant sector of the inode table
  inodeTableBlock = (((number % extData->inodes_per_group) *
		      sizeof(extInode)) / extData->blockSize); 

  // Get a new temporary buffer to read the inode table block
  buffer = kernelMalloc(extData->blockSize);
  if (buffer == NULL)
    {
      kernelError(kernel_error, "Unable to allocate inode table buffer");
      return (status = ERR_MEMORY);
    }

  // Read the inode table block for the group
  status = kernelDiskReadSectors((char *) extData->disk->name,
	 getSectorNumber(extData, (extData->groups[groupNumber].inode_table +
				   inodeTableBlock)),
				 extData->sectorsPerBlock, buffer);
  if (status < 0)
    {
      kernelError(kernel_error, "Unable to read inode table for group %u",
                  groupNumber);
      kernelFree(buffer);
      return (status);
    }

  // Copy the inode structure.
  kernelMemCopy((buffer + (((number % extData->inodes_per_group) *
			    sizeof(extInode)) % extData->blockSize)),
		(void *) inode, sizeof(extInode));

  kernelFree(buffer);

  return (status = 0);
}


static int readIndirectBlocks(extInternalData *extData, unsigned indirectBlock,
			      unsigned *numBlocks, void **buffer,
			      int indirectionLevel)
{
  // This function will read indirect blocks, starting with startBlock.
  // the indirectionLevel parameter being greater than 1 causes a recursion.

  int status = 0;
  unsigned *indexBuffer = NULL;
  int count;

  // Get memory to hold a block
  indexBuffer = kernelMalloc(extData->blockSize);
  if (indexBuffer == NULL)
    {
      kernelError(kernel_error, "Unable to get block buffer memory");
      return (status = ERR_MEMORY);
    }

  // Read the indirect block number we've been passed
  status = kernelDiskReadSectors((char *) extData->disk->name,
				 getSectorNumber(extData, indirectBlock),
				 extData->sectorsPerBlock, indexBuffer);
  if (status < 0)
    {
      kernelFree(indexBuffer);
      return (status);
    }

  // Now, if the indirection level is 1, this is an index of blocks to read.
  // Otherwise, it is an index of indexes, and we need to recurse
  if (indirectionLevel > 1)
    {
      // Do a recursion for every index in this block
      for (count = 0; ((*numBlocks > 0) &&
		       (count < (extData->blockSize / sizeof(unsigned))));
           count++)
        {
	  if (indexBuffer[count] < 2)
	    return (status = 0);

          status = readIndirectBlocks(extData, indexBuffer[count], numBlocks,
				      buffer, (indirectionLevel - 1));
          if (status < 0)
            {
              kernelFree(indexBuffer);
              return (status);
            }
        }
    }
  else
    {
      // Read the blocks in our index block into the buffer
      for (count = 0; ((*numBlocks > 0) &&
		       (count < (extData->blockSize / sizeof(unsigned))));
           count++)
        {
	  if (indexBuffer[count] < 2)
	    return (status = 0);

          status = kernelDiskReadSectors((char *) extData->disk->name,
			 getSectorNumber(extData, indexBuffer[count]),
					 extData->sectorsPerBlock, *buffer);
          if (status < 0)
            {
              kernelFree(indexBuffer);
              return (status);
            }

          *numBlocks -= 1;
          *buffer += extData->blockSize;
        }
    }

  kernelFree(indexBuffer);
  return (status = 0);
}

static int readFile(extInternalData *extData, kernelFileEntry *fileEntry,
		    unsigned startBlock, unsigned numBlocks, void *buffer)
{
  // Read numBlocks blocks of a file (or directory) starting at startBlock
  // into buffer.

  int status = 0;
  extInode *inode = NULL;
  void *dataPointer = NULL;
  int count;
  
  inode = &(((extInodeData *) fileEntry->fileEntryData)->inode);

  // If numBlocks is zero, that means read the whole file
  if (numBlocks == 0)
    {
      if (fileEntry->size)
	{
	  numBlocks = (fileEntry->size / extData->blockSize);
	  if (fileEntry->size % extData->blockSize)
	    numBlocks += 1;
	}
      else
	numBlocks = (inode->blocks / extData->sectorsPerBlock);
    }

  dataPointer = buffer;

  // Read (up to) the first 12 direct blocks
  for (count = 0; ((numBlocks > 0) && (count < 12) &&
		   (dataPointer < (buffer + inode->size))); count ++)
    {
      if (inode->block[count] < 2)
	return (status = 0);

      status = kernelDiskReadSectors((char *) extData->disk->name,
		     getSectorNumber(extData, inode->block[count]),
				     extData->sectorsPerBlock, dataPointer);
      if (status < 0)
	return (status);

      numBlocks -= 1;
      dataPointer += extData->blockSize;
    }

  // Now if there are any indirect blocks...
  if (numBlocks && inode->block[12])
    {
      status = readIndirectBlocks(extData, inode->block[12], &numBlocks,
				  &dataPointer, 1);
      if (status < 0)
        return (status);
    }

  // Double-indirect blocks...
  if (numBlocks && inode->block[13])
    {
      status = readIndirectBlocks(extData, inode->block[13], &numBlocks,
				  &dataPointer, 2);
      if (status < 0)
        return (status);
    }

  // Triple-indirect blocks
  if (numBlocks && inode->block[14])
    {
      status = readIndirectBlocks(extData, inode->block[14], &numBlocks,
				  &dataPointer, 3);
      if (status < 0)
        return (status);
    }

  return (status = 0);
}


static unsigned makeSystemTime(unsigned time)
{
  // This function takes a UNIX time value and returns the equivalent in
  // packed-BCD system format.

  unsigned temp = 0;
  unsigned returnedTime = 0;

  // Unix time is seconds since 00:00:00 January 1, 1970

  // Remove all but the current day
  time %= 86400;

  // The hour
  temp = (time / 3600);
  returnedTime |= ((temp & 0x0000003F) << 12);
  time %= 3600;

  // The minute
  temp = (time / 60);
  returnedTime |= ((temp & 0x0000003F) << 6);
  time %= 60;

  // The second
  returnedTime |= (temp & 0x0000003F);

  return (returnedTime);
}


static unsigned makeSystemDate(unsigned date)
{
  // This function takes a UNIX time value and returns the equivalent in
  // packed-BCD system format.

  unsigned temp = 0;
  unsigned returnedDate = 0;

  // Unix time is seconds since 00:00:00 January 1, 1970

  // Figure out the year
  temp = (date / 31536000);
  returnedDate |= ((temp + 1970) << 9);
  date %= 31536000;

  // The month (1-12)
  temp = (date / 2678400);
  returnedDate |= (((temp + 1) & 0x0000000F) << 5);
  date %= 2678400;

  // Day of the month (1-31)
  temp = (date / 86400);
  returnedDate |= ((temp + 1) & 0x0000001F);

  return (returnedDate);
}


static int scanDirectory(extInternalData *extData, kernelFileEntry *dirEntry)
{
  int status = 0;
  extInode *dirInode = NULL;
  unsigned bufferSize = 0;
  void *buffer = NULL;
  void *entry = NULL;
  extDirectoryEntry realEntry;
  kernelFileEntry *fileEntry = NULL;
  extInode *inode = NULL;

  // Make sure it's really a directory, and not a regular file
  if (dirEntry->type != dirT)
    {
      kernelError(kernel_error, "Entry to scan is not a directory");
      return (status = ERR_NOTADIR);
    }

  dirInode = &(((extInodeData *) dirEntry->fileEntryData)->inode);

  // Check for unsupported directory types
  if ((dirInode->flags & EXT_BTREE_FL) || (dirInode->flags & EXT_INDEX_FL))
    {
      kernelError(kernel_error, "B-tree and hash-indexed directories not yet "
                  "supported");
      return (status = ERR_NOTIMPLEMENTED);
    }

  // Make sure it's not zero-length.  Shouldn't ever happen.
  if (dirInode->blocks == 0)
    {
      kernelError(kernel_error, "Directory has no data");
      return (status = ERR_NODATA);
    }

  bufferSize = ((dirInode->blocks / extData->sectorsPerBlock)
		* extData->blockSize);

  if (bufferSize < dirEntry->size)
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

  status = readFile(extData, dirEntry, dirInode->block[0], 0, buffer);
  if (status < 0)
    {
      kernelError(kernel_error, "Unable to read directory data");
      kernelFree(buffer);
      return (status);
    }

  entry = buffer;

  while ((unsigned) entry < (unsigned) (buffer + bufferSize - 1))
    {
      realEntry.inode = *((unsigned *) entry);
      realEntry.rec_len = *((unsigned short *)(entry + 4));
      realEntry.name_len = *((unsigned char *)(entry + 6));
      realEntry.file_type = *((unsigned char *)(entry + 7));
      strncpy((char *) realEntry.name, (entry + 8), realEntry.name_len);
      realEntry.name[realEntry.name_len] = '\0';

      fileEntry = kernelFileNewEntry(dirEntry->filesystem);
      if (fileEntry == NULL)
	{
	  kernelError(kernel_error, "Unable to get new filesystem entry");
	  kernelFree(buffer);
	  return (status = ERR_NOCREATE);
	}
      
      inode = &(((extInodeData *) fileEntry->fileEntryData)->inode);
      if (inode == NULL)
	{
	  kernelError(kernel_error, "New entry has no private data");
	  kernelFree(buffer);
	  return (status = ERR_BUG);
	}

      // Read the inode
      status = readInode(extData, realEntry.inode, inode);
      if (status < 0)
	{
	  kernelError(kernel_error, "Unable to read inode");
	  kernelFree(buffer);
	  return (status);  
	}
      
      strncpy((char *) fileEntry->name, (char *) realEntry.name,
	      MAX_NAME_LENGTH);

      switch (realEntry.file_type)
	{
        case EXT_FT_UNKNOWN:
          fileEntry->type = unknownT;
          break;
	case EXT_FT_DIR:
	  fileEntry->type = dirT;
	  break;
        case EXT_FT_SYMLINK:
          fileEntry->type = linkT;
          break; 
        default:
          fileEntry->type = fileT;
          break;
	}

      fileEntry->creationTime = makeSystemTime(inode->ctime);
      fileEntry->creationDate = makeSystemDate(inode->ctime);
      fileEntry->accessedTime = makeSystemTime(inode->atime);
      fileEntry->accessedDate = makeSystemDate(inode->atime);
      fileEntry->modifiedTime = makeSystemTime(inode->mtime);
      fileEntry->modifiedDate = makeSystemDate(inode->mtime);
      fileEntry->size = inode->size;
      fileEntry->blocks = (inode->blocks / extData->sectorsPerBlock);
      fileEntry->lastAccess = kernelSysTimerRead();

      // Add it to the directory
      status = kernelFileInsertEntry(fileEntry, dirEntry);
      if (status < 0)
	{
          kernelFree(buffer);
	  return (status);
	}

      // Prevent a situation of getting into a bad loop if the rec_len field
      // isn't some positive number.
      if (realEntry.rec_len <= 0)
        {
          kernelError(kernel_error, "Corrupt directory record \"%s\" in "
                      "directory \"%s\" has a NULL record length",
                      fileEntry->name, dirEntry->name);
          kernelFree(buffer);
          return (status = ERR_BADDATA);
        }
      entry += realEntry.rec_len;
    }

  kernelFree(buffer);
  return (status = 0);
}


static int readRootDir(extInternalData *extData, kernelFilesystem *filesystem)
{
  // This function reads the root directory (which uses a reserved inode
  // number) and attaches the root directory kernelFileEntry pointer to the
  // filesystem structure.

  int status = 0;
  extInode *rootInode =
    &(((extInodeData *) filesystem->filesystemRoot->fileEntryData)->inode);

  if (rootInode == NULL)
    {
      kernelError(kernel_error, "Root entry has no private data");
      return (status = ERR_NODATA);
    }

  // Read the inode for the root directory
  status = readInode(extData, EXT_ROOT_INO, rootInode);
  if (status < 0)
    {
      kernelError(kernel_error, "Unable to read root inode");
      return (status);
    }

  return (status = scanDirectory(extData, filesystem->filesystemRoot));
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
//  Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


int kernelFilesystemExtInitialize(void)
{
  // Initialize the driver

  int status = 0;
  
  // Register our driver
  status = kernelDriverRegister(extDriver, &defaultExtDriver);

  initialized = 1;

  return (status);
}


int kernelFilesystemExtDetect(const kernelDisk *theDisk)
{
  // This function is used to determine whether the data on a disk structure
  // is using a EXT filesystem.  It uses a simple test or two to determine
  // simply whether this is a EXT volume.  Any data that it gathers is
  // discarded when the call terminates.  It returns 1 for true, 0 for false, 
  // and negative if it encounters an error

  int status = 0;
  char *buffer = NULL;
  
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Get a temporary buffer to read the superblock
  buffer = kernelMalloc(EXT_SUPERBLOCK_SIZE);
  if (buffer == NULL)
    {
      kernelError(kernel_error, "Unable to allocate superblock buffer");
      return (status = ERR_MEMORY);
    }

  // Try to load the superblock
  status = readSuperblock(theDisk, buffer);

  kernelFree(buffer);

  if (status == 0)
    // EXT2
    return (status = 1);
  else
    // Not EXT2
    return (status = 0);
}


int kernelFilesystemExtFormat(kernelDisk *theDisk, const char *type,
			      const char *label, int longFormat)
{
  // Format the supplied disk as a EXT volume.

  int status = 0;
  
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  return (status = ERR_NOTIMPLEMENTED);
}


int kernelFilesystemExtCheck(kernelFilesystem *checkFilesystem, int force,
			     int repair)
{
  // This function performs a check of the EXT filesystem structure supplied.
  // Assumptions: the filesystem is REALLY a EXT filesystem, the filesystem
  // is not currently mounted anywhere, and the filesystem driver structure
  // for the filesystem is installed.

  int status = 0;
  
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  return (status = ERR_NOTIMPLEMENTED);
}


int kernelFilesystemExtDefragment(kernelFilesystem *filesystem)
{
  // Defragment the EXT filesystem

  int status = 0;
  
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  return (status = ERR_NOTIMPLEMENTED);
}

int kernelFilesystemExtMount(kernelFilesystem *filesystem)
{
  // This function initializes the filesystem driver by gathering all of
  // the required information from the boot sector.  In addition, it
  // dynamically allocates memory space for the "used" and "free" file and
  // directory structure arrays.

  int status = 0;
  extInternalData *extData = NULL;
  
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

  // Get the EXT data for the requested filesystem.  We don't need the info
  // right now -- we just want to collect it.
  extData = getExtData(filesystem);
  if (extData == NULL)
    return (status = ERR_BADDATA);

  // Read the filesystem's root directory and attach it to the filesystem
  // structure
  status = readRootDir(extData, filesystem);
  if (status < 0)
    {
      kernelError(kernel_error, "Unable to read the filesystem's root "
                  "directory");
      return (status = ERR_BADDATA);
    }

  // Set the proper filesystem type name on the disk structure
  strcpy((char *) filesystem->disk->fsType, "ext2");

  // Read-only for now
  filesystem->readOnly = 1;

  return (status = 0);
}


int kernelFilesystemExtUnmount(kernelFilesystem *filesystem)
{
  // This function releases all of the stored information about a given
  // filesystem.

  int status = 0;
  extInternalData *extData = NULL;
  
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check params
  if (filesystem == NULL)
    {
      kernelError(kernel_error, "NULL filesystem structure");
      return (status = ERR_NULLPARAMETER);
    }

  // Get the EXT data for the requested filesystem
  extData = getExtData(filesystem);
  if (extData == NULL)
    return (status = ERR_BADDATA);

  // Deallocate any global filesystem memory
  kernelFree((void *) extData->groups);
  kernelFree((void *) extData);
  
  // Finally, remove the reference from the filesystem structure
  filesystem->filesystemData = NULL;

  return (status = 0);
}


unsigned kernelFilesystemExtGetFreeBytes(kernelFilesystem *filesystem)
{
  // This function returns the amount of free disk space, in bytes.

  extInternalData *extData = NULL;

  if (!initialized)
    return (0);

  // Check params
  if (filesystem == NULL)
    {
      kernelError(kernel_error, "NULL file entry");
      return (0);
    }

  // Get the EXT data for the requested filesystem
  extData = getExtData(filesystem);
  if (extData == NULL)
    return (0);
  
  return (extData->free_blocks_count * extData->blockSize);
}


int kernelFilesystemExtNewEntry(kernelFileEntry *newEntry)
{
  // This function gets called when there's a new kernelFileEntry in the
  // filesystem (either because a file was created or because some existing
  // thing has been newly read from disk).  This gives us an opportunity
  // to attach EXT-specific data to the file entry

  int status = 0;
  extInodeData *inodeData = NULL;
  extInodeData *newInodeDatas = NULL;
  int count;
  
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
  if (newEntry->fileEntryData != NULL)
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

  if (freeInodeDatas == 0)
    {
      // Allocate memory for file entries
      newInodeDatas = kernelMalloc(sizeof(extInodeData) * MAX_BUFFERED_FILES);
      if (newInodeDatas == NULL)
	{
	  kernelError(kernel_error, "Error allocating memory for EXT inode "
		      "data");
	  return (status = ERR_MEMORY);
	}

      // Initialize the new extInodeData structures.

      for (count = 0; count < (MAX_BUFFERED_FILES - 1); count ++)
	newInodeDatas[count].next = (void *) &(newInodeDatas[count + 1]);

      // The free file entries are the new memory
      freeInodeDatas = newInodeDatas;

      // Add the number of new file entries
      numFreeInodeDatas = MAX_BUFFERED_FILES;
    }

  inodeData = freeInodeDatas;
  freeInodeDatas = (extInodeData *) inodeData->next;
  numFreeInodeDatas -= 1;
  newEntry->fileEntryData = (void *) inodeData;

  return (status = 0);
}


int kernelFilesystemExtInactiveEntry(kernelFileEntry *inactiveEntry)
{
  // This function gets called when a kernelFileEntry is about to be
  // deallocated by the system (either because a file was deleted or because
  // the entry is simply being unbuffered).  This gives us an opportunity
  // to deallocate our EXT-specific data from the file entry

  int status = 0;
  extInodeData *inodeData = NULL;
  
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check params
  if (inactiveEntry == NULL)
    {
      kernelError(kernel_error, "NULL file entry");
      return (status = ERR_NULLPARAMETER);
    }

  inodeData = (extInodeData *) inactiveEntry->fileEntryData;
  if (inodeData == NULL)
    {
      kernelError(kernel_error, "File entry has no private filesystem data");
      return (status = ERR_ALREADY);
    }

  // Erase all of the data in this entry
  kernelMemClear((void *) inodeData, sizeof(extInodeData));

  // Release the inode data structure attached to this file entry.  Put the
  // inode data back into the pool of free ones.
  inodeData->next = (void *) freeInodeDatas;
  freeInodeDatas = inodeData;
  numFreeInodeDatas += 1;

  // Remove the reference
  inactiveEntry->fileEntryData = NULL;

  return (status = 0);
}


int kernelFilesystemExtResolveLink(kernelFileEntry *linkEntry)
{
  // This is called by the kernelFile.c code when we have registered a
  // file or directory as a link, but not resolved it, and now it needs
  // to be resolved.  By default this driver never resolves EXT symbolic
  // links until they are needed, so this will get called when the system
  // wants to read/write the symbolically-linked directory or file.

  int status = 0;
  extInternalData *extData = NULL;
  extInode *inode = NULL;
  char fileName[MAX_PATH_NAME_LENGTH];
  char tmpName[MAX_PATH_NAME_LENGTH];
  kernelFileEntry *targetEntry = NULL;
  char *buffer = NULL;

  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check params
  if (linkEntry == NULL)
    {
      kernelError(kernel_error, "Link entry is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  // Get the EXT data for the requested filesystem
  extData = getExtData(linkEntry->filesystem);
  if (extData == NULL)
    return (0);
  
  inode = &(((extInodeData *) linkEntry->fileEntryData)->inode);
  if (inode == NULL)
    {
      kernelError(kernel_error, "Link entry has no private data");
      return (status = ERR_NODATA);
    }

  // If the file size is 64 bytes or less, the name will be stored right
  // in the inode.
  if (inode->size <= 64)
    strncpy(fileName, (char *) inode->block, inode->size);

  else
    {
      // We have to read the first data block in order to get the filename.

      buffer = kernelMalloc(extData->blockSize);
      if (buffer == NULL)
	{
	  kernelError(kernel_error, "No buffer for link data");
	  return (status = ERR_MEMORY);
	}

      status = kernelDiskReadSectors((char *) extData->disk->name,
				     getSectorNumber(extData, inode->block[0]),
				     extData->sectorsPerBlock, buffer);
      if (status < 0)
	{
	  kernelFree(buffer);
	  return (status);
	}

      // Copy the data into the fileName buffer
      strncpy(fileName, buffer, inode->size);
      kernelFree(buffer);
    }

  fileName[inode->size] = '\0';

  // Need to make sure it's an absolute pathname.
  if (fileName[0] != '/')
    {
      char tmpPath[MAX_PATH_LENGTH];
      kernelFileGetFullName(linkEntry->parentDirectory, tmpPath);
      sprintf(tmpName, "%s/%s", tmpPath, fileName);
      kernelFileFixupPath(tmpName, fileName);
    }

  // Try to get the entry for the specified pathname
  targetEntry = kernelFileLookup(fileName);
  if (targetEntry == NULL)
    {
      // Not found.  We'll try one more thing.  If the fileName *is* an
      // absolute path, we will try prepending the mount point in case
      // it's supposed to be relative to the start of the filesystem.

      if (fileName[0] != '/')
	return (status = ERR_NOSUCHFILE); 

      sprintf(tmpName, "%s/%s", ((kernelFilesystem *) linkEntry->filesystem)
	      ->mountPoint, fileName);
      kernelFileFixupPath(tmpName, fileName);

      // Try again
      targetEntry = kernelFileLookup(fileName);
      if (targetEntry == NULL)
	return (status = ERR_NOSUCHFILE);
    }

  linkEntry->contents = (void *) targetEntry;
  return (status = 0);
}


int kernelFilesystemExtReadFile(kernelFileEntry *theFile, unsigned blockNum,
				unsigned blocks, unsigned char *buffer)
{
  // This function is the "read file" function that the filesystem
  // driver exports to the world.  It is mainly a wrapper for the
  // internal function of the same purpose, but with some additional
  // argument checking.  Returns 0 on success, negative otherwise.

  int status = 0;
  extInternalData *extData = NULL;
  extInode *inode = NULL;

  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check params
  if ((theFile == NULL) || (buffer == NULL))
    {
      kernelError(kernel_error, "Null file or buffer parameter");
      return (status = ERR_NULLPARAMETER);
    }

  // Get the EXT data for the requested filesystem
  extData = getExtData(theFile->filesystem);
  if (extData == NULL)
    return (status = ERR_BADDATA);
 
  inode = &(((extInodeData *) theFile->fileEntryData)->inode); 
  if (inode == NULL)
    {
      kernelError(kernel_error, "File \"%s\" has no private data",
		  theFile->name);
      return (status = ERR_NODATA);
    }

  return (readFile(extData, theFile, blockNum, blocks, buffer));
}


int kernelFilesystemExtWriteFile(kernelFileEntry *theFile, unsigned blockNum, 
				 unsigned blocks, unsigned char *buffer)
{
  // This function is the "write file" function that the filesystem
  // driver exports to the world.  It is mainly a wrapper for the
  // internal function of the same purpose, but with some additional
  // argument checking.  Returns 0 on success, negative otherwise.

  int status = 0;
  
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  return (status = ERR_NOTIMPLEMENTED);
}


int kernelFilesystemExtCreateFile(kernelFileEntry *theFile)
{
  // This function does the EXT-specific initialization of a new file.
  // There's not much more to this than getting a new entry data structure
  // and attaching it.  Returns 0 on success, negative otherwise

  int status = 0;
  
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  return (status = ERR_NOTIMPLEMENTED);
}


int kernelFilesystemExtDeleteFile(kernelFileEntry *theFile, int secure)
{
  // This function deletes a file.  It returns 0 on success, negative
  // otherwise

  int status = 0;
  
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  return (status = ERR_NOTIMPLEMENTED);
}


int kernelFilesystemExtFileMoved(kernelFileEntry *entry)
{
  // This function is called by the filesystem manager whenever a file
  // has been moved from one place to another.  This allows us the chance
  // do to any EXT-specific things to the file that are necessary.  In our
  // case, we need to re-create the file's short alias, since this is
  // directory-dependent.

  int status = 0;
  
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  return (status = ERR_NOTIMPLEMENTED);
}


int kernelFilesystemExtReadDir(kernelFileEntry *directory)
{
  // This function receives an emtpy file entry structure, which represents
  // a directory whose contents have not yet been read.  This will fill the
  // directory structure with its appropriate contents.  Returns 0 on
  // success, negative otherwise.

  int status = 0;
  extInternalData *extData = NULL;
  
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check params
  if (directory == NULL)
    {
      kernelError(kernel_error, "Directory parameter is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  // Make sure there's an inode attached
  if (directory->fileEntryData == NULL)
    {
      kernelError(kernel_error, "Directory \"%s\" has no private data",
		  directory->name);
      return (status = ERR_NODATA);
    }

  // Get the EXT data for the requested filesystem
  extData = getExtData(directory->filesystem);
  if (extData == NULL)
    return (status = ERR_BADDATA);

  return (status = scanDirectory(extData, directory));
}


int kernelFilesystemExtWriteDir(kernelFileEntry *directory)
{
  // This function takes a directory entry structure and updates it 
  // appropriately on the disk volume.  On success it returns zero,
  // negative otherwise.

  int status = 0;
  
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  return (status = ERR_NOTIMPLEMENTED);
}


int kernelFilesystemExtMakeDir(kernelFileEntry *directory)
{
  // This function is used to create a directory on disk.  The caller will
  // create the file entry data structures, and it is simply the
  // responsibility of this function to make the on-disk structures reflect
  // the new entry.  It returns 0 on success, negative otherwise.

  int status = 0;
  
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  return (status = ERR_NOTIMPLEMENTED);
}


int kernelFilesystemExtRemoveDir(kernelFileEntry *directory)
{
  // This function deletes a directory, but only if it is empty.  
  // It returns 0 on success, negative otherwise

  int status = 0;
  
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  return (status = ERR_NOTIMPLEMENTED);
}


int kernelFilesystemExtTimestamp(kernelFileEntry *theFile)
{
  // This function does EXT-specific stuff for time stamping a file.

  int status = 0;
  
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  return (status = ERR_NOTIMPLEMENTED);
}
