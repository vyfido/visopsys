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
//  kernelFilesystemFat.c
//

// This file contains the routines designed to interpret the FAT filesystem
// (commonly found on DOS (TM) disks)

#include "kernelFilesystemFat.h"
#include "kernelFile.h"
#include "kernelDriverManagement.h"
#include "kernelMalloc.h"
#include "kernelMultitasker.h"
#include "kernelLock.h"
#include "kernelSysTimer.h"
#include "kernelMiscFunctions.h"
#include "kernelLog.h"
#include "kernelError.h"
#include <string.h>
#include <ctype.h>
#include <sys/errors.h>


static kernelFilesystemDriver defaultFatDriver = {
  Fat,   // FS type
  "FAT", // Driver name
  kernelFilesystemFatInitialize,
  kernelFilesystemFatDetect,
  kernelFilesystemFatFormat,
  kernelFilesystemFatCheck,
  kernelFilesystemFatDefragment,
  kernelFilesystemFatMount,
  kernelFilesystemFatUnmount,
  kernelFilesystemFatGetFreeBytes,
  kernelFilesystemFatNewEntry,
  kernelFilesystemFatInactiveEntry,
  kernelFilesystemFatResolveLink,
  kernelFilesystemFatReadFile,
  kernelFilesystemFatWriteFile,
  kernelFilesystemFatCreateFile,
  kernelFilesystemFatDeleteFile,
  kernelFilesystemFatFileMoved,
  kernelFilesystemFatReadDir,
  kernelFilesystemFatWriteDir,
  kernelFilesystemFatMakeDir,
  kernelFilesystemFatRemoveDir,
  kernelFilesystemFatTimestamp
};

// These hold free private data memory
static fatEntryData *freeEntryDatas = NULL;
static unsigned numFreeEntryDatas = 0;

static int initialized = 0;


static int readBootSector(kernelDisk *theDisk, unsigned char *buffer)
{
  // This simple function will read a disk structure's boot sector into
  // the requested buffer and ensure that it is (at least trivially) 
  // valid.  Returns 0 on success, negative on error.

  int status = 0;

  // Initialize the buffer we were given
  kernelMemClear(buffer, FAT_MAX_SECTORSIZE);

  // Read the boot sector
  status = kernelDiskReadSectors((char *) theDisk->name, 0, 1, buffer);
  // Make sure that the read was successful
  if (status < 0)
    {
      // Couldn't read the boot sector.  Make an error
      kernelError(kernel_error, "Unable to gather information about the FAT "
		  "filesystem from the boot block");
      return (status);
    }

  // It MUST be true that the signature word 0xAA55 occurs at offset
  // 510 of the boot sector (regardless of the sector size of this device).  
  // If it does not, then this is not only NOT a FAT boot sector, but 
  // may not be a valid boot sector at all.
  if ((buffer[510] != (unsigned char) 0x55) || 
      (buffer[511] != (unsigned char) 0xAA))
    return (status = ERR_BADDATA);

  // Return success
  return (status = 0);
}


static int readFSInfo(kernelPhysicalDisk *theDisk, unsigned sectorNumber,
		      unsigned char *buffer)
{
  // This simple function will read a disk structure's fsInfo sector into
  // the requested buffer and ensure that it is (at least trivially) 
  // valid.  Returns 0 on success, negative on error.

  int status = 0;

  // Make sure that neither of the pointers we were passed are NULL
  if ((theDisk == NULL) || (buffer == NULL))
    return (status = ERR_NULLPARAMETER);

  // Initialize the buffer we were given
  kernelMemClear(buffer, FAT_MAX_SECTORSIZE);

  // Read the FSInfo sector (read 1 sector starting at the logical sector
  // number we were given)
  status = kernelDiskReadSectors((char *) theDisk->name, sectorNumber,
				 1, buffer);
  // Make sure that the read was successful
  if (status < 0)
    {
      // Couldn't read the FSInfo sector.  Make an error
      kernelError(kernel_error, "Unable to read or write the FAT32 FSInfo "
		  "structure");
      return (status);
    }

  // It MUST be true that the signature dword 0xAA550000 occurs at 
  // offset 0x1FC of the FSInfo sector (regardless of the sector size 
  // of this device).  If it does not, then this is not a valid 
  // FSInfo sector.  It must also be true that we find two 
  // signature dwords in the sector: 0x41615252 at offset 0 and 
  // 0x61417272 at offset 0x1E4.
  if ((*((unsigned *)(buffer + FAT_FSINFO_LEADSIG)) != 0x41615252) ||
      (*((unsigned *)(buffer + FAT_FSINFO_STRUCSIG)) != 0x61417272) ||
      (*((unsigned *)(buffer + FAT_FSINFO_TRAILSIG)) != 0xAA550000))
    {
      // Oops, there's something wrong with it.
      kernelError(kernel_error, "Not a valid FSInfo sector");
      return (status = ERR_BADDATA);
    }
  
  // After all that exhaustive checking (why, Microsoft?), it looks 
  // like we have a valid FAT32 FSInfo sector.  
  
  // Return success
  return (status = 0);
}


static int flushFSInfo(fatInternalData *fatData)
{
  // This function is designed to support extra functionality that is
  // found only in FAT32 filesystems (not FAT12 or FAT16).  The FAT32
  // filesystem contains a block called FSInfo, which (generally) occurs
  // right after the boot sector, and which is part of the reserved area
  // of the disk.  This data structure contains a few bits of information
  // which are pretty useful for maintaining large filesystems, specifically
  // the 'free cluster count' and 'first free cluster' values.  This function
  // will write the current values of these fields back to the FSInfo
  // block.

  int status = 0;
  unsigned char fsInfoBlock[FAT_MAX_SECTORSIZE];

  // Make sure the fsInfoSectorF32 value is still reasonable.  It must
  // come after the boot sector and be within the reserved area of the
  // volume.  Otherwise, we will assume there's an inconsistency somewhere.
  if ((fatData->fsInfoSectorF32 < 1) || 
      (fatData->fsInfoSectorF32 > fatData->reservedSectors))
    return (status = ERR_BADDATA);
  
  // Now we can read in the current FSInfo block (so that we can modify
  // select values before writing it back).  Call the function that will 
  // read the FSInfo block
  status =
    kernelDiskReadSectors((char *) ((kernelPhysicalDisk *) fatData->disk)
			  ->name, (unsigned) fatData->fsInfoSectorF32, 1,
			  fsInfoBlock);
  if (status < 0)
    // Couldn't read the boot FSInfo block
    return (status);

  // Set the first signature
  *((unsigned *)(fsInfoBlock + FAT_FSINFO_LEADSIG)) = 0x41615252;

  // Set the second signature
  *((unsigned *)(fsInfoBlock + FAT_FSINFO_STRUCSIG)) = 0x61417272;

  // Set the FAT32 "free cluster count".  Doubleword value.
  *((unsigned *)(fsInfoBlock + FAT_FSINFO_FREECOUNT)) =
    fatData->freeClusters;

  // Set the FAT32 "first free cluster".  Doubleword value.
  *((unsigned *)(fsInfoBlock + FAT_FSINFO_NEXTFREE)) =
    fatData->firstFreeClusterF32;

  // Set the third signature (Overkill?  Yeah, probably.  Don't blame me.)
  *((unsigned *)(fsInfoBlock + FAT_FSINFO_TRAILSIG)) = 0xAA550000;

  // Now write the updated FSInfo block back to the disk
  status = kernelDiskWriteSectors((char *) fatData->disk->name, 
				  fatData->fsInfoSectorF32, 1, fsInfoBlock);
  // Make sure that the write was successful
  if (status < 0)
    {
      // Couldn't read the FSInfo sector.  Make an error
      kernelError(kernel_error, "Unable to read or write the FAT32 FSInfo "
		  "structure");
      return (status);
    }

  // Return success
  return (status = 0);
}


static int readVolumeInfo(fatInternalData *fatData)
{
  // This function reads information about the filesystem from
  // the boot sector info block.  It assumes that the requirement to
  // do so has been met (i.e. it does not check whether the update
  // is necessary, and does so unquestioningly).  It returns 0 on success, 
  // negative otherwise.
  
  // This is only used internally, so there is no need to check the
  // disk structure.  The functions that are exported will do this.
  
  int status = 0;
  unsigned char bootSector[FAT_MAX_SECTORSIZE];
  kernelPhysicalDisk *physicalDisk = NULL;

  physicalDisk = (kernelPhysicalDisk *) fatData->disk->physical;

  // Call the function that reads the boot sector
  status = readBootSector((kernelDisk *) fatData->disk, bootSector);
  // Make sure we were successful
  if (status < 0)
    {
      // Couldn't read the boot sector, or it was bad
      kernelError(kernel_error, "Not a valid boot sector");
      return (status);
    }

  // Now we can gather some information about the filesystem

  // Whomever formatted the volume, blah.  It's just that we need to set
  // it to something when *we* format it.
  strncpy((char *) fatData->oemName,
	  ((char *)(bootSector + FAT_BS_OEMNAME)), 9);

  // The number of bytes per sector.  Word value.
  fatData->bytesPerSector = (unsigned)
    *((short *)(bootSector + FAT_BS_BYTESPERSECT));

  // The bytes-per-sector field may only contain one of the following 
  // values: 512, 1024, 2048 or 4096.  Anything else is illegal, according 
  // to MS.  512 is almost always the value found here.
  if ((fatData->bytesPerSector != 512) && 
      (fatData->bytesPerSector != 1024) && 
      (fatData->bytesPerSector != 2048) && 
      (fatData->bytesPerSector != 4096))
    {
      // Not a legal value for FAT
      kernelError(kernel_error, "Illegal bytes-per-sector value");
      return (status = ERR_BADDATA);
    }

  // Also, the bytes-per-sector field should match the value for the
  // disk structure we're referencing
  if (fatData->bytesPerSector != physicalDisk->sectorSize)
    {
      // Not a legal value for FAT
      kernelError(kernel_error, "Bytes-per-sector does not match disk");
      return (status = ERR_BADDATA);
    }

  // Get the number of sectors per cluster.  Byte value.
  fatData->sectorsPerCluster = (unsigned) bootSector[FAT_BS_SECPERCLUST];

  // The combined (bytes-per-sector * sectors-per-cluster) value should not
  // exceed 32K, according to MS.  Apparently, some think 64K is OK, but MS
  // says it's not.  32K it is.
  if ((fatData->bytesPerSector * fatData->sectorsPerCluster) > 32768)
    {
      // Not a legal value in our FAT driver
      kernelError(kernel_error, "Illegal sectors-per-cluster value");
      return (status = ERR_BADDATA);
    }

  // Get the number of reserved sectors.  Word value.
  fatData->reservedSectors = (unsigned)
    *((short *)(bootSector + FAT_BS_RESERVEDSECS));

  // This value must be one or more
  if (fatData->reservedSectors < 1)
    {
      // Not a legal value for FAT
      kernelError(kernel_error, "Illegal reserved sectors");
      return (status = ERR_BADDATA);
    }

  // Next, the number of FAT tables.  Byte value.
  fatData->numberOfFats = (unsigned) bootSector[FAT_BS_NUMFATS];

  // This value must be one or more
  if (fatData->numberOfFats < 1)
    {
      // Not a legal value in our FAT driver
      kernelError(kernel_error, "Illegal number of FATs");
      return (status = ERR_BADDATA);
    }

  // The number of root directory entries.  Word value.
  fatData->rootDirEntries = (unsigned)
    *((short *)(bootSector + FAT_BS_ROOTENTRIES));

  // The root-dir-entries value should either be 0 (must for FAT32) or,
  // for FAT12 and FAT16, should result in an even multiple of the
  // bytes-per-sector value when multiplied by 32; however, this is not 
  // a requirement.

  // Get the total number of sectors from the 16-bit field.  Word value.
  fatData->totalSectors = (unsigned)
    *((short *)(bootSector + FAT_BS_TOTALSECS16));

  // This 16-bit-total-sectors value is linked to the 32-bit-total-sectors 
  // value we will find further down in the boot sector (there are
  // requirements here) but we will save our evaluation of this field 
  // until we have gethered the 32-bit value
  
  // Get the media type.  Byte value.
  fatData->mediaType = (unsigned) bootSector[FAT_BS_MEDIABYTE];

  // There is a list of legal values for this field:  0xF0, 0xF8, 0xF9,
  // 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, and 0xFF.  We don't actually use this
  // value for anything, but we will ensure that the value is legal
  if ((fatData->mediaType < 0xF8) && (fatData->mediaType != 0xF0))
    {
      // Oops, not a legal value for FAT
      kernelError(kernel_error, "Illegal media type byte");
      return (status = ERR_BADDATA);
    }

  // The 16-bit number of sectors per FAT.  Word value.
  fatData->fatSectors = (unsigned) *((short *)(bootSector + FAT_BS_FATSIZE16));

  // OK, there's a little bit of a paradox here.  If this happens to be
  // a FAT32 volume, then the sectors-per-fat value must be zero in this
  // field.  To determine for certain whether this IS a FAT32 volume, we 
  // need to know how many data clusters there are (it's used in the 
  // "official" MS calculation that determines FAT type).  The problem is 
  // that the data-cluster-count calculation depends on the sectors-per-fat
  // value.  Oopsie, Micsosoft.  Unfortunately, this means that if the 
  // sectors-per-fat happens to be zero, we must momentarily ASSUME that
  // this is indeed a FAT32, and read the 32-bit sectors-per-fat in
  // advance from another part of the bootsector.  There's a data
  // corruption waiting to happen here.  Consider the case where this is 
  // a non-FAT32 volume, but this field has been zeroed by mistake; so, 
  // we read the 32-bit sectors-per-fat value from the FAT32-specific
  // portion of the boot sector.  What if this field contains some random,
  // non-zero value?  Then the data clusters calculation will probably
  // be wrong, and we could end up determining the wrong FAT type.  That
  // would lead to data corruption for sure.  Oh well, it's the only
  // way I can see...

  if (fatData->fatSectors == 0)
    // The FAT32 number of sectors per FAT.  Doubleword value.
    fatData->fatSectors = *((unsigned *)(bootSector + FAT_BS32_FATSIZE));

  // The sectors-per-fat value must now be non-zero
  if (!fatData->fatSectors)
    {
      // Oops, not a legal value for FAT32
      kernelError(kernel_error, "Illegal FAT32 sectors per fat");
      return (status = ERR_BADDATA);
    }

  // The 16-bit number of sectors per track/cylinder.  Word value.
  fatData->sectorsPerTrack = (unsigned)
    *((short *)(bootSector + FAT_BS_SECSPERCYL));

  // This sectors-per-track field is not always relevant.  We won't 
  // check it here.

  // The number of heads for this volume. Word value. 
  fatData->heads = (unsigned) *((short *)(bootSector + FAT_BS_NUMHEADS));

  // Like the sectors-per-track field, above, this heads field is not always 
  // relevant.  We won't check it here.

  // The number of hidden sectors.  Doubleword value.
  fatData->hiddenSectors = *((unsigned *)(bootSector + FAT_BS_HIDDENSECS));

  // Hmm, I'm not too sure what to make of this hidden-sectors field.
  // The way I read the documentation is that it describes the number of
  // sectors in any physical partitions that preceed this one.  However,
  // we already get this value from the master boot record where needed.
  // We will ignore the value of this field.

  if (!fatData->totalSectors)
    // Get the 32-bit value instead.  Doubleword value.
    fatData->totalSectors = *((unsigned *)(bootSector + FAT_BS_TOTALSECS32));

  // Ensure that we have a non-zero total-sectors value
  if (!fatData->totalSectors)
    {
      // Oops, this combination is not legal for FAT
      kernelError(kernel_error, "Illegal total sectors");
      return (status = ERR_BADDATA);
    }

  // This ends the portion of the boot sector (bytes 0 through 35) that is 
  // consistent between all 3 variations of FAT filesystem we're supporting.
  // Now we must determine which type of filesystem we've got here.  We 
  // must do a few calculations to finish gathering the information we 
  // need to make the determination.

  // Figure out the actual number of directory sectors.  We have already
  // ensured that the bytes-per-sector value is non-zero (don't worry
  // about a divide-by-zero error here)
  fatData->rootDirSectors = 
    ((FAT_BYTES_PER_DIR_ENTRY * (unsigned) fatData->rootDirEntries) / 
     fatData->bytesPerSector);
  if (((FAT_BYTES_PER_DIR_ENTRY * (unsigned) fatData->rootDirEntries) % 
       fatData->bytesPerSector) != 0)
    fatData->rootDirSectors += 1;

  // This calculation comes directly from MicrosoftTM.  This is how we
  // determine the number of data sectors and data clusters on the volume.
  // We have already ensured that the sectors-per-cluster value is 
  // non-zero (don't worry about a divide-by-zero error here)
  fatData->dataSectors = 
    (fatData->totalSectors - (fatData->reservedSectors + 
			      (fatData->numberOfFats * fatData->fatSectors) +
			      fatData->rootDirSectors));
  fatData->dataClusters = (fatData->dataSectors / fatData->sectorsPerCluster);

  // OK.  Now we now have enough data to determine the type of this FAT
  // filesystem.  According to the Microsoft white paper, the following 
  // is the only TRUE determination of specific FAT filesystem type.  
  // We will examine the "data clusters" value.  There are specific cluster 
  // limits which constrain these filesystems.

  if (fatData->dataClusters < 4085)
    {
      // We have a FAT12 filesystem.  Hooray.
      fatData->fsType = fat12;
      fatData->terminalCluster = 0x0FF8; // (or above)
    }

  else if (fatData->dataClusters < 65525)
    {
      // We have a FAT16 filesystem.  Hooray.
      fatData->fsType = fat16;
      fatData->terminalCluster = 0xFFF8; // (or above)
    }

  else
    {
      // Any larger value of data clusters denotes a FAT32 filesystem
      fatData->fsType = fat32;
      fatData->terminalCluster = 0x0FFFFFF8; // (or above)
    }

  fatData->volumeId = 0;
  fatData->volumeLabel[0] = '\0';
  fatData->fsSignature[0] = '\0';
  
  // Now we know the type of the FAT filesystem.  From here, the data
  // gathering we do must diverge.

  if ((fatData->fsType == fat12) || (fatData->fsType == fat16))
    {
      // We have either a FAT12 or FAT16 filesystem.  There are some
      // additional pieces of information we can gather from this boot
      // sector.

      // The drive number.  Byte value.
      fatData->driveNumber = (unsigned)
	*((char *)(bootSector + FAT_BS_DRIVENUM));

      // The extended boot block signature.  Byte value.
      fatData->bootSignature = (unsigned)
	*((char *)(bootSector + FAT_BS_BOOTSIG));
  
      // Only do these last ones if the boot signature was 0x29
      if (fatData->bootSignature == (unsigned char) 0x29)
	{
	  // The volume Id of the filesystem.  Doubleword value.
	  fatData->volumeId = (unsigned)
	    *((unsigned *)(bootSector + FAT_BS_VOLID));

	  // The volume label of the filesystem.
	  strncpy((char *) fatData->volumeLabel,
		  (bootSector + FAT_BS_VOLLABEL), 11);
	  fatData->volumeLabel[11] = '\0';
      
	  // The filesystem type indicator "hint"
	  strncpy((char *) fatData->fsSignature,
		  (bootSector + FAT_BS_FILESYSTYPE), 8);
	  fatData->fsSignature[8] = '\0';
	}

      // Since this is a FAT12/16 filesystem, we are now finished.  There
      // is no additional useful information we can get from the boot
      // sector.
    }

  else
    {
      // OK, this is FAT32.  There is some additional information we need to
      // gather from the disk that is specific to this type of filesystem.

      // The next value is the "extended flags" field for FAT32.  This
      // value indicates the number of "active" FATS.  Active FATS are
      // significant when FAT mirroring is active.  While our filesystem
      // driver does not do runtime mirroring per se, it does update 
      // all FATs whenever the master FAT is changed on disk.  Thus, at
      // least on disk, all FATs are generally synchronized.  We will 
      // ignore the field for now.

      // We have already read the FAT32 sectors-per-fat value.  This is
      // necessitated by a "logic bug" in the filesystem specification.
      // See the rather lengthy explanation nearer the beginning of this
      // function.

      // Now we have the FAT32 version field.  The FAT32 version we are
      // supporting here is 0.0.  We will not mount a FAT volume that has
      // a higher version number than that (but we don't need to save
      // the version number anywhere).
      if (*((short *)(bootSector + FAT_BS32_FSVERSION)) != (short) 0)
	{
	  // Oops, we cannot support this version of FAT32
	  kernelError(kernel_error, "Unsupported FAT32 version");
	  return (status = ERR_BADDATA);
	}

      // Next, we get the starting cluster number of the root directory.
      // Doubleword value.
      fatData->rootDirClusterF32 = (unsigned)
	*((unsigned *)(bootSector + FAT_BS32_ROOTCLUST));

      // This root-dir-cluster value must be >= 2, and <= (data-clusters + 1)
      if ((fatData->rootDirClusterF32 < 2) || 
	  (fatData->rootDirClusterF32 > (fatData->dataClusters + 1)))
	{
	  // Oops, this is not a legal cluster number
	  kernelError(kernel_error, "Illegal FAT32 root dir cluster %u",
		      fatData->rootDirClusterF32);
	  return (status = ERR_BADDATA);
	}

      // Next, we get the sector number (in the reserved area) of the
      // FSInfo structure.  This structure contains some extra information
      // that is useful to us for maintaining large volumes.  Word value.
      fatData->fsInfoSectorF32 = (unsigned)
	*((short *)(bootSector + FAT_BS32_FSINFO));

      // This number must be greater than 1, and less than the number
      // of reserved sectors in the volume
      if ((fatData->fsInfoSectorF32 < 1) ||
	  (fatData->fsInfoSectorF32 >= fatData->reservedSectors))
	{
	  // Oops, this is not a legal sector number
	  kernelError(kernel_error, "Illegal FAT32 FSInfo sector");
	  return (status = ERR_BADDATA);
	}

      // The sector number of the backup boot sector
      fatData->backupBootF32 = (unsigned)
	*((unsigned *)(bootSector + FAT_BS32_BACKUPBOOT));

      // The drive number.  Byte value.  Same as the field for FAT12/16,
      // but at a different offset
      fatData->driveNumber = (unsigned)
	*((unsigned *)(bootSector + FAT_BS32_DRIVENUM));

      // The extended boot block signature.  Byte value.  Same as the field 
      // for FAT12/16, but at a different offset
      fatData->bootSignature = (unsigned)
	*((unsigned *)(bootSector + FAT_BS32_BOOTSIG));
  
      // Only do these last ones if the boot signature was 0x29
      if (fatData->bootSignature == (unsigned char) 0x29)
	{
	  // The volume Id of the filesystem.  Doubleword value.  Same as 
	  // the field for FAT12/16, but at a different offset
	  fatData->volumeId = (unsigned)
	    *((unsigned *)(bootSector + FAT_BS32_VOLID));

	  // The volume label of the filesystem.  Same as the field for 
	  // FAT12/16, but at a different offset
	  strncpy((char *) fatData->volumeLabel,
		  (bootSector + FAT_BS32_VOLLABEL), 11);
	  fatData->volumeLabel[11] = '\0';
      
	  // The filesystem type indicator "hint"
	  strncpy((char *) fatData->fsSignature,
		  (bootSector + FAT_BS32_FILESYSTYPE), 8);
	  fatData->fsSignature[8] = '\0';

	  // Now we will read some additional information, not included
	  // in the boot sector.  This FSInfo sector contains more information
	  // that will be useful in managing large FAT32 volumes.  We 
	  // previously gathered the sector number of this structure from
	  // the boot sector.
	}

      // From here on, we are finished with the data in the boot
      // sector, so we will reuse the buffer to hold the FSInfo.
	  
      // Call the function that will read the FSInfo block
      status = readFSInfo((kernelPhysicalDisk *) fatData->disk, 
			  (unsigned) fatData->fsInfoSectorF32, bootSector);
      // Make sure we were successful
      if (status < 0)
	// Couldn't read the boot FSInfo block, or it was bad
	return (status);

      // Now we can gather some additional information about the 
      // FAT32 filesystem

      // Get the FAT32 "free cluster count".  Doubleword value.
      fatData->freeClusters = (unsigned)
	*((unsigned *)(bootSector + FAT_FSINFO_FREECOUNT));

      // This free-cluster-count value can be zero, but it cannot
      // be greater than data-clusters -- with one exeption.  It can
      // be 0xFFFFFFFF (meaning the free cluster count is unknown).
      if ((fatData->freeClusters > fatData->dataClusters) &&
	  (fatData->freeClusters != 0xFFFFFFFF))
	{
	  // Oops, not a legal value for FAT
	  kernelError(kernel_error, "Illegal FAT32 free cluster count");
	  return (status = ERR_BADDATA);
	}

      // Finally, get the FAT32 "first free cluster".  Doubleword value.
      fatData->firstFreeClusterF32 = (unsigned)
	*((unsigned *)(bootSector + FAT_FSINFO_NEXTFREE));

      // This first-free-cluster value must be >= 2, but it cannot be
      // greater than data-clusters, unless it is 0xFFFFFFFF (not
      // known)
      if ((fatData->firstFreeClusterF32 < 2) || 
	  ((fatData->firstFreeClusterF32 > fatData->dataClusters) &&
	   (fatData->firstFreeClusterF32 != 0xFFFFFFFF)))
	{
	  // Oops, not a legal value for FAT
	  kernelError(kernel_error, "Illegal FAT32 first free cluster");
	  return (status = ERR_BADDATA);
	}
    }

  // Return success
  return (status = 0);
}


static int flushVolumeInfo(fatInternalData *fatData)
{
  // This function writes information about the filesystem to
  // the boot sector info block.
  
  // This is only used internally, so there is no need to check the
  // disk structure.  The functions that are exported will do this.
  
  int status = 0;
  char bootSector[512];

  // Initialize the boot sector
  kernelMemClear((void *) &bootSector, 512);

  // Read the existing boot sector
  status =
    kernelDiskReadSectors((char *)((kernelDisk *) fatData->disk)->name,
			  0, 1, &bootSector);
  if (status < 0)
    {
      kernelError(kernel_error, "Unable to read boot sector of filesystem");
      return (status);
    }

  strncpy(((char *)(bootSector + FAT_BS_OEMNAME)),
	  (char *) fatData->oemName, 9);
  *((short *)(bootSector + FAT_BS_BYTESPERSECT)) =
    (short) fatData->bytesPerSector;
  *((char *)(bootSector + FAT_BS_SECPERCLUST)) =
    (char) fatData->sectorsPerCluster;
  *((short *)(bootSector + FAT_BS_RESERVEDSECS)) =
    (short) fatData->reservedSectors;
  *((char *)(bootSector + FAT_BS_NUMFATS)) = (char) fatData->numberOfFats;
  *((short *)(bootSector + FAT_BS_ROOTENTRIES)) =
    (short) fatData->rootDirEntries;
  if (fatData->totalSectors < 0x10000)
    *((short *)(bootSector + FAT_BS_TOTALSECS16)) =
      (short) fatData->totalSectors;
  else
    *((short *)(bootSector + FAT_BS_TOTALSECS16)) = (short) 0;
  *((char *)(bootSector + FAT_BS_MEDIABYTE)) = (char) fatData->mediaType;
  if ((fatData->fsType == fat12) || (fatData->fsType == fat16))
    *((short *)(bootSector + FAT_BS_FATSIZE16)) = (short) fatData->fatSectors;
  else
    *((short *)(bootSector + FAT_BS_FATSIZE16)) = (short) 0;
  *((short *)(bootSector + FAT_BS_SECSPERCYL)) =
    (short) fatData->sectorsPerTrack;
  *((short *)(bootSector + FAT_BS_NUMHEADS)) = (short) fatData->heads;
  *((unsigned *)(bootSector + FAT_BS_HIDDENSECS)) =
    (unsigned) fatData->hiddenSectors;
  if (fatData->totalSectors >= 0x10000)
    *((unsigned *)(bootSector + FAT_BS_TOTALSECS32)) =
      (unsigned) fatData->totalSectors;
  else
    *((unsigned *)(bootSector + FAT_BS_TOTALSECS32)) = (unsigned) 0;

  if (fatData->fsType == fat32)
    {
      *((unsigned *)(bootSector + FAT_BS32_FATSIZE)) = fatData->fatSectors;
      // All FATS have been active, 0 primary. 
      *((short *)(bootSector + FAT_BS32_EXTFLAGS)) &= (short) 0xFF00;
      *((short *)(bootSector + FAT_BS32_EXTFLAGS)) |= (short) 0x0008;
      *((short *)(bootSector + FAT_BS32_FSVERSION)) = (short) 0;
      *((unsigned *)(bootSector + FAT_BS32_ROOTCLUST)) =
	(unsigned) fatData->rootDirClusterF32;
      *((short *)(bootSector + FAT_BS32_FSINFO)) =
	(short) fatData->fsInfoSectorF32;
      *((short *)(bootSector + FAT_BS32_BACKUPBOOT)) =
	(short) fatData->backupBootF32;
      *((char *)(bootSector + FAT_BS32_DRIVENUM)) =
	(char) fatData->driveNumber;
      *((char *)(bootSector + FAT_BS32_BOOTSIG)) =
	(char) fatData->bootSignature;
      *((unsigned *)(bootSector + FAT_BS32_VOLID)) =
	(unsigned) fatData->volumeId;
      strncpy((char *)(bootSector + FAT_BS32_VOLLABEL),
	      (char *) fatData->volumeLabel, 11);
      strncpy((char *)(bootSector + FAT_BS32_FILESYSTYPE),
	      (char *) fatData->fsSignature, 8);
    }
  else
    {
      *((char *)(bootSector + FAT_BS_DRIVENUM)) =
	(unsigned char) fatData->driveNumber;
      *((char *)(bootSector + FAT_BS_BOOTSIG)) = (char) fatData->bootSignature;
      *((unsigned *)(bootSector + FAT_BS_VOLID)) =
	(unsigned) fatData->volumeId;
      strncpy((char *)(bootSector + FAT_BS_VOLLABEL),
	      (char *) fatData->volumeLabel, 11);
      strncpy((char *)(bootSector + FAT_BS_FILESYSTYPE),
	      (char *) fatData->fsSignature, 8);
    } 

  *((short *)(bootSector + 510)) = (unsigned short) 0xAA55;

  status = kernelDiskWriteSectors((char *) fatData->disk->name,
				  0, 1, (void *) &bootSector);
  
  // If FAT32 and backupBootF32 is non-zero, make a backup copy of the
  // boot sector
  if ((fatData->fsType == fat32) && fatData->backupBootF32)
    kernelDiskWriteSectors((char *) fatData->disk->name,
			   fatData->backupBootF32, 1, (void *) &bootSector);

  return (status);
}


static int writeDirtyFatSects(fatInternalData *fatData)
{
  // Just write the dirty FAT sectors

  int status = 0;
  int errors = 0;
  int fatCount, sectorCount;
  
  if (!fatData->numDirtyFatSects)
    // Nothing to do
    return (status = 0);

  for (fatCount = 0; fatCount < fatData->numberOfFats; fatCount ++)
    for (sectorCount = 0; sectorCount < fatData->numDirtyFatSects;
	 sectorCount ++)
      {
	status =
	  kernelDiskWriteSectors((char *) fatData->disk->name, 
				 (fatData->reservedSectors +
				  (fatCount * fatData->fatSectors) +
				  fatData->dirtyFatSectList[sectorCount]), 1,
				 (fatData->FAT +
				  (fatData->dirtyFatSectList[sectorCount] *
				   fatData->bytesPerSector)));
	if (status < 0)
	  errors = status;
      }
  
  fatData->numDirtyFatSects = 0;

  if (errors)
    kernelError(kernel_error, "Errors clearing dirty FAT sector list");

  return (errors);
}


static int markDiryFatSect(fatInternalData *fatData, unsigned sectNum)
{
  // Marks a FAT sector as dirty.  Writes out the other dirty sectors if the
  // list is full.

  int status = 0;
  int errors = 0;
  int count;

  if (fatData->numDirtyFatSects >= FAT_MAX_DIRTY_FATSECTS)
    {
      status = writeDirtyFatSects(fatData);
      if (status < 0)
	errors = status;
    }

  // Make sure it's not already in the list
  for (count = 0; count < fatData->numDirtyFatSects; count ++)
    if (fatData->dirtyFatSectList[count] == sectNum)
      return (errors);

  // Not found.  Add it.
  fatData->dirtyFatSectList[fatData->numDirtyFatSects++] = sectNum;

  return (errors);
}


static int getFatEntry(fatInternalData *fatData, unsigned entryNumber, 
		       unsigned *value)
{
  // This function is internal, and takes as a parameter the number
  // of the FAT entry to be read.  On success it returns 0, negative
  // on failure.

  int status = 0;
  unsigned entryOffset = 0;
  unsigned entryValue = 0;

  // Check to make sure there is such a cluster
  if (entryNumber >= (fatData->dataClusters + 2))
    {
      kernelError(kernel_error, "Requested FAT entry is beyond the limits "
		  "of the table");
      return (status = ERR_BUG);
    }

  if (fatData->fsType == fat12)
    {
      // FAT 12 entries are 3 nybbles each.  Thus, we need to take the 
      // entry number we were given and multiply it by 3/2 to get the 
      // starting byte.
      entryOffset = (entryNumber + (entryNumber >> 1));

      // Now, entryOffset is the index of the WORD value that contains the
      // value we're looking for.  Read that word value

      entryValue = *((short *)(fatData->FAT + entryOffset));

      // We're almost finished, except that we need to get rid of the
      // extra nybble of information contained in the word value.  If
      // the extra nybble is in the most-significant spot, we need to 
      // simply mask it out.  If it's in the least significant spot, we
      // need to shift the word right by 4 bits.
      
      // 0 = mask, since the extra data is in the upper 4 bits
      // 1 = shift, since the extra data is in the lower 4 bits
      if (entryNumber % 2)
	entryValue = ((entryValue & 0xFFF0) >> 4);
      else
	entryValue = (entryValue & 0x0FFF);
    }

  else if (fatData->fsType == fat16)
    {
      // FAT 16 entries are 2 bytes each.  Thus, we need to take the 
      // entry number we were given and multiply it by 2 to get the 
      // starting byte.
      entryOffset = (entryNumber * 2);
      entryValue = *((short *)(fatData->FAT + entryOffset));
    }

  else /* if (fatData->fsType == fat32) */
    {
      // FAT 32 entries are 4 bytes each.  Thus, we need to take the 
      // entry number we were given and multiply it by 4 to get the 
      // starting byte.
      entryOffset = (entryNumber * 4);
      
      entryValue = *((unsigned *)(fatData->FAT + entryOffset));

      // Really only the bottom 28 bits of this value are relevant
      entryValue &= 0x0FFFFFFF;
    }

  *value = entryValue;

  // Return success
  return (status = 0);
}


static int changeFatEntry(fatInternalData *fatData,
			  unsigned entryNumber, unsigned value)
{
  // This function is internal, and takes as its parameters the number
  // of the FAT entry to be written and the value to set.  On success it 
  // returns 0.  Returns negative on failure.

  int status = 0;
  unsigned entryOffset = 0;
  unsigned entryValue = 0;
  unsigned oldValue = 0;
  unsigned fatSectorNumber = 0;

  // Check the entry number
  if (entryNumber >= (fatData->dataClusters + 2))
    {
      kernelError(kernel_error, 
		  "Requested FAT entry is beyond the limits of the table");
      return (status = ERR_BUG);
    }

  if (fatData->fsType == fat12)
    {
      // FAT 12 entries are 3 nybbles each.  Thus, we need to take the 
      // entry number we were given and multiply it by 3/2 to get the 
      // starting byte.
      entryOffset = (entryNumber + (entryNumber >> 1));
      
      // Now, entryOffset is the index of the WORD value that contains the
      // 3 nybbles we want to set.  Read the current word value
      entryValue = *((short *)(fatData->FAT + entryOffset));

      if (entryNumber % 2)
	{
	  entryValue &= 0x000F;
	  entryValue |= ((value & 0x0FFF) << 4);
	}
      else
	{
	  entryValue &= 0xF000;
	  entryValue |= (value & 0x0FFF);
	}
    
      // Write the value back to the FAT buffer.
      *((short *)(fatData->FAT + entryOffset)) = entryValue;
    }

  else if (fatData->fsType == fat16)
    {
      // FAT 16 entries are 2 bytes each.  Thus, we need to take the 
      // entry number we were given and multiply it by 2 to get the
      // starting byte.  Much simpler for FAT 16.
      entryOffset = (entryNumber * 2);
      
      // Write the value back to the FAT buffer
      *((short *)(fatData->FAT + entryOffset)) = value;
    }

  else /* if (fatData->fsType == fat32) */
    {
      // FAT 32 entries are 4 bytes each.  Thus, we need to take the 
      // entry number we were given and multiply it by 4 to get the
      // starting byte.  Much simpler for FAT 16.
      entryOffset = (entryNumber * 4);
      
      // Get the current value of the entry
      oldValue = *((unsigned *) fatData->FAT + entryOffset);

      // Make sure we preserve the top 4 bits of the previous entry
      entryValue = (value | (oldValue & 0xF0000000));

      // Write the value back to the FAT buffer
      *((unsigned *)(fatData->FAT + entryOffset)) = entryValue;
    }

  // Calculate which FAT sector(s) which contain(s) the entry we changed
  fatSectorNumber = (entryOffset / fatData->bytesPerSector);
  status = markDiryFatSect(fatData, fatSectorNumber);

  if ((fatData->fsType == fat12) &&
      ((entryOffset % fatData->bytesPerSector) ==
       (fatData->bytesPerSector - 1)))
    status = markDiryFatSect(fatData, (fatSectorNumber + 1));

  return (status);
}


static int getNumClusters(fatInternalData *fatData, unsigned startCluster,
			  unsigned *clusters)
{
  // This function is internal, and takes as a parameter the starting
  // cluster of a file/directory.  The function will traverse the FAT table 
  // entries belonging to the file/directory, and return the number of 
  // clusters used by that item.

  int status = 0;
  unsigned currentCluster = 0;
  unsigned newCluster = 0;

  // Zero clusters by default
  *clusters = 0;

  if (startCluster == 0)
    // This file has no allocated clusters.  Return size zero.
    return (status = 0);

  // Save the starting value
  currentCluster = startCluster;

  // Now we go through a loop to gather the cluster numbers, adding 1 to the
  // total each time.  A value of terminalCluster or more means that there
  // are no more sectors
  while(1)
    {
      if ((currentCluster < 2) || (currentCluster >= fatData->terminalCluster))
	{
	  kernelError(kernel_error, "Invalid cluster number %u (start cluster "
		      "%u)", currentCluster, startCluster);
	  return (status = ERR_BADDATA);
	}

      status = getFatEntry(fatData, currentCluster, &newCluster);
      if (status < 0)
	{
	  kernelError(kernel_error, "Error reading FAT table");
	  return (status = ERR_BADDATA);
	}

      *clusters += 1;

      currentCluster = newCluster;

      // Finished?
      if (currentCluster >= fatData->terminalCluster)
	break;
    }

  return (status = 0);
}


static int getNthCluster(fatInternalData *fatData, unsigned startCluster,
			 unsigned *nthCluster)
{
  // This function is internal, and takes as a parameter the starting
  // cluster of a file/directory.  The function will traverse the FAT table 
  // entries belonging to the file/directory, and return the number of 
  // the requested cluster used by that item.  Zero-based.

  int status = 0;
  unsigned currentCluster = 0;
  unsigned newCluster = 0;
  unsigned clusterCount = 0;

  if (startCluster == 0)
    {
      // This is an error, because the file has no clusters, so even a
      // nthCluster request of zero is wrong.
      *nthCluster = 0;
      return (status = ERR_INVALID);
    }

  // Save the starting value
  currentCluster = startCluster;

  // Now we go through a loop to step through the cluster numbers.  A
  // value of terminalCluster or more means that there are no more clusters.
  while (1)
    {
      if (clusterCount == *nthCluster)
	{
	  // currentCluster is the one requested
	  *nthCluster = currentCluster;
	  return (status = 0);
	}

      if ((currentCluster < 2) || (currentCluster >= fatData->terminalCluster))
	{
	  kernelError(kernel_error, "Invalid cluster number %u",
		      currentCluster);
	  return (status = ERR_BADDATA);
	}

      status = getFatEntry(fatData, currentCluster, &newCluster);
      if (status < 0)
	{
	  kernelError(kernel_error, "Error reading FAT table");
	  return (status = ERR_BADDATA);
	}

      clusterCount++;

      if (newCluster < fatData->terminalCluster)
	currentCluster = newCluster;
      else
	{
	  // The Nth cluster requested does not exist.
	  *nthCluster = 0;
	  return (status = ERR_INVALID);
	}
    }
}


static int getLastCluster(fatInternalData *fatData, unsigned startCluster,
			  unsigned *lastCluster)
{
  // This function is internal, and takes as a parameter the starting
  // cluster of a file/directory.  The function will traverse the FAT table 
  // entries belonging to the file/directory, and return the number of 
  // the last cluster used by that item.

  int status = 0;
  unsigned currentCluster = 0;
  unsigned newCluster = 0;

  if (startCluster == 0)
    {
      // This file has no allocated clusters.  Return zero (which is not a
      // legal cluster number), however this is not an error
      *lastCluster = 0;
      return (status = 0);
    }

  // Save the starting value
  currentCluster = startCluster;

  // Now we go through a loop to step through the cluster numbers.  A
  // value of terminalCluster or more means that there are no more clusters.
  while (1)
    {
      if ((currentCluster < 2) || (currentCluster >= fatData->terminalCluster))
	{
	  kernelError(kernel_error, "Invalid cluster number %u",
		      currentCluster);
	  return (status = ERR_BADDATA);
	}

      status = getFatEntry(fatData, currentCluster, &newCluster);
      if (status < 0)
	{
	  kernelError(kernel_error, "Error reading FAT table");
	  return (status = ERR_BADDATA);
	}

      if (newCluster < fatData->terminalCluster)
	currentCluster = newCluster;
      else
	break;
    }

  *lastCluster = currentCluster;
  return (status = 0);
}


static void makeFreeBitmap(fatInternalData *fatData)
{
  // This function examines the FAT and fills out the bitmap of free clusters.
  // The function must be spawned as a new thread for itself to run in.  
  // This way, building a free list (especially for a large FAT volume) can
  // proceed without hanging up the system.  Note that when it is finished,
  // it doesn't "return": it kills its own process.  Don't let it kill you by
  // accident.

  int status = 0;
  unsigned freeClusters = 0;
  unsigned entry = 0;
  unsigned count;

  // This function should never be running more than one at a time for a 
  // particular filesystem instance.  Thus, if the thread is already
  // running for this filesystem we simply terminate.
  if (fatData->buildingFreeBitmap)
    {
      kernelError(kernel_error, "Already building filesystem free bitmap");
      kernelMultitaskerTerminate(status = ERR_ALREADY);
    }

  // Set the flag "buildingList", for the reason outlined above
  fatData->buildingFreeBitmap = 1;

  // Lock the free list so nobody tries to use it or change it while
  // it's in an inconsistent state
  status = kernelLockGet(&(fatData->freeBitmapLock));
  // Make sure we were successful
  if (status < 0)
    {
      kernelError(kernel_error, "Couldn't lock the free list");
      fatData->buildingFreeBitmap = 0;
      kernelMultitaskerTerminate(status);
    }

  // Ok, we will loop through the entire FAT.  If that sounds like that
  // is (potentially) a lot of data, you're right.  The advantage of doing
  // this is that knowledge of the entire FAT is very handy for preventing
  // fragmentation, and is otherwise good for easy management.  In order to
  // keep the memory footprint as small as possible, and to (hopefully) 
  // maximize speed, we keep the data as one large bitmap.  As mentioned
  // before, we will do the whole operation as a separate thread from the
  // rest of the filesystem driver, so that if it takes a minute or two
  // it doesn't keep people waiting.

  // Mark entries 0 and 1 as busy, since they can't be allocated for
  // anything (data clusters are numbered starting from 2)
  fatData->freeClusterBitmap[0] |= 0xC0;

  // We start from 2 and go up from there.
  for (count = 2; count < (fatData->dataClusters + 2); count ++)
    {
      // Read the corrensponding FAT table entry. 
      status = getFatEntry(fatData, count, &entry);
      // Check for an error code
      if (status < 0)
	{
	  kernelError(kernel_error, "Couldn't read FAT entry");
	  break;
	}

      // Is entry equal to zero?  If so, skip it; it's free and the entry
      // in our bitmap will already be zero.  Count the free cluster.  If 
      // it's non-zero, we need to set the corresponding bit in our bitmap
      // to mark the entry as "used".
      if (entry == 0)
	{
	  freeClusters++;
	  // Update the value in realtime if it's not FAT32 or if the FAT32
	  // value was 'unknown'
	  if ((fatData->fsType != fat32) ||
	      (fatData->freeClusters == 0xFFFFFFFF))
	    fatData->freeClusters = freeClusters;
	  continue;
	}
      
      // Add the used cluster bit to our bitmap.
      fatData->freeClusterBitmap[count / 8] |= (0x80 >> (count % 8));
    }

  // Unlock the free list
  kernelLockRelease(&(fatData->freeBitmapLock));

  fatData->freeClusters = freeClusters;

  // We are finished
  fatData->buildingFreeBitmap = 0;
  kernelMultitaskerTerminate(status = 0);
  while(1);
}


static int releaseClusterChain(fatInternalData *fatData, unsigned startCluster)
{
  // This function returns an unused cluster or sequence of clusters back 
  // to the free list, and marks them as unused in the volume's FAT table
  // Returns 0 on success, negative otherwise

  int status = 0;
  unsigned currentCluster = 0;
  unsigned nextCluster = 0;

  if ((startCluster == 0) || (startCluster == fatData->terminalCluster))
    // Nothing to do
    return (status = 0);

  // Attempt to lock the free-block list
  status = kernelLockGet(&(fatData->freeBitmapLock));
  // Make sure we were successful
  if (status < 0)
    {
      kernelError(kernel_error, "Unable to lock the free-cluster bitmap");
      return (status);
    }

  currentCluster = startCluster;

  // Loop through each of the unwanted clusters in the chain.  Change each
  // one in both the free list and the FAT table
  while(1)
    {
      // Get the next thing in the chain
      status = getFatEntry(fatData, currentCluster, &nextCluster);

      if (status)
	{
	  kernelError(kernel_error, "Unable to follow cluster chain");
	  kernelLockRelease(&(fatData->freeBitmapLock));
	  return (status);
	}

      // Deallocate the current cluster in the chain
      status = changeFatEntry(fatData, currentCluster, 0);

      if (status)
	{
	  kernelError(kernel_error, "Unable to deallocate cluster");
	  kernelLockRelease(&(fatData->freeBitmapLock));
	  return (status);
	}

      // Mark the cluster as free in the free cluster bitmap (mask it off)
      fatData->freeClusterBitmap[currentCluster / 8] &= 
	(0xFF7F >> (currentCluster % 8));

      // Adjust the free cluster count
      fatData->freeClusters++;

      // Any more to do?
      if (nextCluster >= fatData->terminalCluster)
	break;

      currentCluster = nextCluster;
    }

  // Write out dirty FAT sectors
  status = writeDirtyFatSects(fatData);

  // Unlock the list and return success
  kernelLockRelease(&(fatData->freeBitmapLock));
  return (status);
}


static int shortenFile(fatInternalData *fatData, kernelFileEntry *entry,
		       unsigned newBlocks)
{
  // This function is internal, and is used to truncate a file entry to the
  // requested number of blocks
  
  int status = 0;
  fatEntryData *entryData = NULL;
  unsigned newLastCluster = (newBlocks - 1);
  unsigned firstReleasedCluster = 0;

  // Check params
  if (entry == NULL)
    return (status = ERR_NULLPARAMETER);

  if (entry->blocks <= newBlocks)
    // Nothing to do
    return (status = 0);

  // Get the private FAT data structure attached to this file entry
  entryData = (fatEntryData *) entry->fileEntryData;
  
  if (entryData == NULL)
    return (status = ERR_NODATA);

  // Get the entry that will be the new last cluster
  status = getNthCluster(fatData, entryData->startCluster, &newLastCluster);
  if (status < 0)
    return (status);

  // Save the value this entry points to.  That's where we start deleting
  // stuff in a second.
  status = getFatEntry(fatData, newLastCluster, &firstReleasedCluster);
  if (status < 0)
    return (status);
  
  // Mark the last cluster as last
  status = changeFatEntry(fatData, newLastCluster, fatData->terminalCluster);
  if (status < 0)
    return (status);
  
  // Release the rest of the cluster chain
  status = releaseClusterChain(fatData, firstReleasedCluster);
  if (status < 0)
    return (status);

  entry->blocks = newBlocks;
  entry->size =
    (newBlocks * (fatData->bytesPerSector * fatData->sectorsPerCluster));

  return (status = 0);
}


static int clearClusterChainData(fatInternalData *fatData,
				 unsigned startCluster)
{
  // This function will zero all of the cluster data in the supplied
  // cluster chain.  Returns 0 on success, negative otherwise

  int status = 0;
  unsigned currentCluster = 0;
  unsigned nextCluster = 0;
  unsigned char *buffer;

  if ((startCluster == 0) || (startCluster == fatData->terminalCluster))
    // Nothing to do
    return (status = 0);

  // Allocate an empty buffer equal in size to one cluster.  The memory
  // allocation routine will make it all zeros.
  buffer = kernelMalloc(fatData->sectorsPerCluster * fatData->bytesPerSector);
  if (buffer == NULL)
    {
      kernelError(kernel_error, "Unable to allocate memory for clearing "
		  "clusters");
      return (status = ERR_MEMORY);
    }

  currentCluster = startCluster;

  // Loop through each of the unwanted clusters in the chain.  Write the
  // empty buffer to each cluster.

  while(1)
    {
      // Get the next thing in the chain
      status = getFatEntry(fatData, currentCluster, &nextCluster);
      if (status)
	{
	  kernelError(kernel_error, "Unable to follow cluster chain");
	  kernelFree(buffer);
	  return (status);
	}

      status = 
	kernelDiskWriteSectors((char *) fatData->disk->name,
			       (((currentCluster - 2) *
				 fatData->sectorsPerCluster) + 
				fatData->reservedSectors +
				(fatData->fatSectors * 2) + 
				fatData->rootDirSectors),
			       fatData->sectorsPerCluster, buffer);
      if (status < 0)
	{
	  kernelError(kernel_error, "Error clearing cluster data");
	  kernelFree(buffer);
	  return (status);
	}

      // Any more to do?
      if (nextCluster == fatData->terminalCluster)
	break;
    }

  // Rreturn success
  kernelFree(buffer);
  return (status = 0);
}


static int getUnusedClusters(fatInternalData *fatData,
			     unsigned requested, int *startCluster)
{
  // This function is used internally, and allocates a chain of free disk
  // clusters to the calling program.  It uses a "first fit" algorithm to
  // make the decision, looking for the first free block that is big enough
  // to fully accommodate the request.  This is good because if there IS a
  // block big enough to fit the entire request, there is no fragmentation.
  // On the other hand, if the routine cannot find a contiguous block that is
  // big enough, it will allocate (parts of) the largest available chunks
  // until the request can be satisfied.

  int status = 0;
  unsigned terminate = 0;
  unsigned div, mod;
  unsigned biggestSize = 0;
  unsigned biggestLocation = 0;
  unsigned consecutive = 0;
  unsigned lastCluster = 0;
  unsigned count;

  // Make sure the request is bigger than zero
  if (requested == 0)
    {
      // This isn't an "error" per se, we just won't do anything
      *startCluster = 0;
      return (status = 0);
    }

  // Make sure that overall, there are enough free clusters to satisfy
  // the request
  while (fatData->freeClusters < requested)
    {
      // If we are still building the free cluster bitmap, there might
      // be enough free shortly
      if (fatData->buildingFreeBitmap)
	{
	  kernelMultitaskerYield();
	  continue;
	}

      kernelError(kernel_error, 
		  "Not enough free space to complete operation");
      return (status = ERR_NOFREE);
    }

  // Attempt to lock the free-block bitmap
  status = kernelLockGet(&(fatData->freeBitmapLock));
  // Make sure we were successful
  if (status < 0)
    {
      kernelError(kernel_error, "Unable to lock the free-cluster bitmap");
      return (status);
    }

  // We will roll through the free cluster bitmap, looking for the first
  // free chunk that is big enough to accommodate the request.  We also keep
  // track of the biggest (but not big enough) block that we have encountered
  // so far.  This enables us to select the next-biggest available block
  // if no single block is large enough to accommodate the whole request.

  terminate = (fatData->dataClusters + 2);

  for (count = 2; count < terminate; count ++)
    {
      // We are searching a bitmap for a sequence of one or more zero bits,
      // which signify that the disk clusters are unused.  

      // Calculate the current "div" and "mod" for this operation so
      // we only have to do it once
      div = count / 8;
      mod = count % 8;

      // There will be a couple of little tricks we can do to speed up this
      // search.  If this iteration of "count" is divisible by 8, then we can
      // let the processor scan the whole byte ahead looking for ANY unused
      // clusters.  If the char value is 0xFF, we can skip all eight bits
      // of the byte in one shot
      if ((mod == 0) && (count < (terminate - 8)))
	{
	  // There might be other tricky things I'll do here later, but this
	  // will be all for now
	  if (fatData->freeClusterBitmap[div] == (unsigned char) 0xFF)
	    {
	      count += 8;
	      continue;
	    }
	}

      if (fatData->freeClusterBitmap[div] & (0x80 >> mod))
	{
	  // This cluster is used
	  consecutive = 0;
	  continue;
	}
      else
	{
	  // Ok, we've identified that there's at least one unused cluster
	  // here.  Let's start counting through them.

	  // This cluster is free.
	  consecutive++;

	  // Is this the biggest consectutive string so far?
	  if (consecutive > biggestSize)
	    {
	      // We set a new big record
	      biggestSize = consecutive;
	      biggestLocation = (count - (biggestSize - 1));

	      // Do we now have enough consecutive clusters to grant the
	      // request?
	      if (biggestSize >= requested)
		break;
	    }
	}
    }

  terminate = (biggestLocation + biggestSize);

  // Change all of the FAT table entries for the allocated clusters
  for (count = biggestLocation; count < terminate; count ++)
    {
      if (count < (terminate - 1))
	status = changeFatEntry(fatData, count, (count + 1));
      else
	{
	  // Last cluster
	  lastCluster = count;
	  status = changeFatEntry(fatData, count, fatData->terminalCluster);
	}

      if (status)
	{
	  // Ack!  Unable to change all the cluster values!
	  kernelError(kernel_error, "FAT table could not be modified");
	  // Attempt to get rid of all the ones we changed
	  releaseClusterChain(fatData, biggestLocation);
	  kernelLockRelease(&(fatData->freeBitmapLock));
	  return (status);
	}
    }

  // Loop through the free bitmap and mark these clusters as used
  for (count = biggestLocation; 
       count < (biggestLocation + biggestSize); count ++)
    fatData->freeClusterBitmap[count / 8] |= (0x80 >> (count % 8));

  // Adjust the free cluster count by whatever number we found
  fatData->freeClusters -= biggestSize;

  // If we didn't find enough clusters in the main loop, that means there's
  // no single block of clusters large enough.  We'll do a little recursion
  // to fill out the request.
  if (biggestSize < requested)
    {
      status = getUnusedClusters(fatData, (requested - biggestSize),
				 &count);
      if (status)
	{
	  // Ack!  This should never happen!
	  kernelError(kernel_error, "Cluster allocation error");
	  // Deallocate all of the clusters we reserved previously
	  releaseClusterChain(fatData, biggestLocation);
	  kernelLockRelease(&(fatData->freeBitmapLock));
	  return (status);
	}

      // We need to attach this new allocation on to the end of the one
      // that we did in this function call.  
      status = changeFatEntry(fatData, lastCluster, count);

      if (status)
	{
	  kernelError(kernel_error, "FAT table could not be modified");
	  // Attempt to get rid of all the ones we changed, plus the one
	  // created with our function call.
	  releaseClusterChain(fatData, biggestLocation);
	  releaseClusterChain(fatData, count);
	  kernelLockRelease(&(fatData->freeBitmapLock));
	  return (status);
	}
    }

  // Success.  Set the caller's variable
  *startCluster = biggestLocation;

  // Write out dirty FAT sectors
  status = writeDirtyFatSects(fatData);

  // Unlock the list and return success
  kernelLockRelease(&(fatData->freeBitmapLock));
  return (status);
}


static int dirRequiredEntries(kernelFileEntry *directory)
{
  // This function is internal, and is used to determine how many 32-byte
  // entries will be required to hold the requested directory.  Returns
  // the number of entries required on success, negative otherwise.

  int entries = 0;
  kernelFileEntry *listItemPointer = NULL;

  // Make sure directory is a directory
  if (directory->type != dirT)
    {
      kernelError(kernel_error, 
		  "Directory structure to count is not a directory");
      return (entries = ERR_NOTADIR);
    }

  // We have to count the files in the directory to figure out how
  // many items to fill in
  if (directory->contents == NULL)
    {
      // This directory is currently empty.  No way this should ever happen
      kernelError(kernel_error, "Directory structure to count is empty");
      return (entries = ERR_BUG);
    }

  listItemPointer = directory->contents;

  while (listItemPointer != NULL)
    {
      entries += 1;

      // '.' and '..' do not have long filename entries
      if (strcmp((char *) listItemPointer->name, ".") &&
	  strcmp((char *) listItemPointer->name, ".."))
	{
	  // All other entries have long filenames

	  // We can fit 13 characters into each long filename slot
	  entries += (strlen((char *) listItemPointer->name) / 13);
	  if (strlen((char *) listItemPointer->name) % 13)
	    entries += 1;
	}

      listItemPointer = listItemPointer->nextEntry;
    }
  
  // Add 1 for the NULL entry at the end
  entries++;

  return (entries);
}


static unsigned makeDosDate(unsigned date)
{
  // This function takes a packed-BCD date value in system format and returns
  // the equivalent in packed-BCD DOS format.

  unsigned temp = 0;
  unsigned returnedDate = 0;

  returnedDate = date;

  // This date is almost okay.  The RTC function returns a year value
  // that's nice.  It returns an absolute year, with no silly monkey
  // business.  For example, 1999 is represented as 1999.  2011 is 2011, etc.
  // in bits 7->.  Unfortunately, FAT dates don't work exactly the same way.
  // Year is a value between 0 and 127, representing the number of years
  // since 1980.

  // Extract the year
  temp = ((returnedDate & 0xFFFFFE00) >> 9);
  temp -= 1980;

  // Clear the year and reset it
  // Year should be 7 bits in places 9-15
  returnedDate &= 0x000001FF;
  returnedDate |= (temp << 9);

  return (returnedDate);
}


static inline unsigned makeDosTime(unsigned time)
{
  // This function takes a packed-BCD time value in system format and returns
  // the equivalent in packed-BCD DOS format.

  // The time we get is almost right, except that FAT seconds format only
  // has a granularity of 2 seconds, so we divide by 2 to get the final value.
  // The quick way to fix all of this is to simply shift the whole thing
  // by 1 bit, creating a nice 16-bit DOS time.

  return (time >> 1);
}


static unsigned makeSystemDate(unsigned date)
{
  // This function takes a packed-BCD date value in DOS format and returns
  // the equivalent in packed-BCD system format.

  unsigned temp = 0;
  unsigned returnedDate = 0;

  returnedDate = date;

  // Unfortunately, FAT dates don't work exactly the same way as system
  // dates.  The DOS year value is a number between 0 and 127, representing
  // the number of years since 1980.  It's found in bits 9-15.

  // Extract the year
  temp = ((returnedDate & 0x0000FE00) >> 9);
  temp += 1980;

  // Clear the year and reset it.  Year should be in places 9->
  returnedDate &= 0x000001FF;
  returnedDate |= (temp << 9);

  return (returnedDate);
}


static inline unsigned makeSystemTime(unsigned time)
{
  // This function takes a packed-BCD time value in DOS format and returns
  // the equivalent in packed-BCD system format.

  // The time we get is almost right, except that FAT seconds format only
  // has a granularity of 2 seconds, so we multiply by 2 to get the final
  // value.  The quick way to fix all of this is to simply shift the whole
  // thing left by 1 bit, which results in a time with the correct number
  // of bits, but always an even number of seconds.

  return (time << 1);
}


static int fillDirectory(kernelFileEntry *currentDir, 
			 unsigned dirBufferSize, char *dirBuffer)
{
  // This function takes a directory structure and writes it to the
  // appropriate directory on disk.

  int status = 0;
  char shortAlias[12];
  int fileNameLength = 0;
  int longFilenameSlots = 0;
  int longFilenamePos = 0;
  unsigned char fileCheckSum = 0;

  unsigned char *dirEntry = NULL;
  unsigned char *subEntry = NULL;

  kernelFileEntry *listItemPointer = NULL;
  kernelFileEntry *realEntry = NULL;
  fatEntryData *entryData = NULL;
  unsigned temp;
  int count, count2;

  // Don't try to fill in a directory that's really a link
  if (currentDir->type == linkT)
    {
      kernelError(kernel_error, "Cannot fill in a link directory");
      return (status = ERR_INVALID);
    }
  
  dirEntry = dirBuffer;
  listItemPointer = currentDir->contents;

  while(listItemPointer != NULL)
    {
      realEntry = listItemPointer;
      if (listItemPointer->type == linkT)
        // Resolve links
        realEntry = kernelFileResolveLink(listItemPointer);

      // Get the entry's data
      entryData = (fatEntryData *) realEntry->fileEntryData;
      if (entryData == NULL)
	{
	  kernelError(kernel_error, "File entry has no private filesystem "
		      "data");
	  return (status = ERR_BUG);
	}

      if (!strcmp((char *) listItemPointer->name, ".") ||
	  !strcmp((char *) listItemPointer->name, ".."))
	{
	  // Don't write '.' and '..' entries in the root directory of a
	  // filesystem
	  if (currentDir == 
	      ((kernelFilesystem *) currentDir->filesystem)->filesystemRoot)
	    {
	      listItemPointer = listItemPointer->nextEntry;
	      continue;
	    }

	  // Get the appropriate short alias.
	  if (!strcmp((char *) listItemPointer->name, "."))
	    strcpy(shortAlias, ".          ");
	  else if (!strcmp((char *) listItemPointer->name, ".."))
	    strcpy(shortAlias, "..         ");
	}
      else
	strcpy(shortAlias, (char *) entryData->shortAlias);

      // Calculate this file's 8.3 checksum.  We need this in advance for
      // associating the long filename entries
      fileCheckSum = 0;
      for (count = 0; count < 11; count++)
	fileCheckSum = (unsigned char)
	  ((((fileCheckSum & 0x01) << 7) | ((fileCheckSum & 0xFE) >> 1)) 
	   + shortAlias[count]);

      // All files except '.' and '..' will have at least one long filename
      // entry, just because that's the only kind we use in Visopsys.  Short
      // aliases are only generated for compatibility.

      if (strcmp((char *) listItemPointer->name, ".") &&
	  strcmp((char *) listItemPointer->name, ".."))
	{
	  // Figure out how many long filename slots we need
	  fileNameLength = strlen((char *) listItemPointer->name);
	  longFilenameSlots = (fileNameLength / 13);
	  if (fileNameLength % 13)
	    longFilenameSlots += 1;
      
	  // We must do a loop backwards through the directory slots
	  // before this one, writing the characters of this long filename
	  // into the appropriate slots
	  
	  dirEntry += ((longFilenameSlots - 1) * FAT_BYTES_PER_DIR_ENTRY);
	  subEntry = dirEntry;
	  longFilenamePos = 0;

	  for (count = 0; count < longFilenameSlots; count++)
	    {
	      // Put the "counter" byte into the first slot
	      subEntry[0] = (count + 1);
	      if (count == (longFilenameSlots - 1))
		subEntry[0] = (subEntry[0] | 0x40);

	      // Put the first five 2-byte characters into this entry
	      for (count2 = 1; count2 < 10; count2 += 2)
		{
		  if (longFilenamePos > fileNameLength)
		    {
		      subEntry[count2] = (unsigned char) 0xFF;
		      subEntry[count2 + 1] = (unsigned char) 0xFF;
		    }
		  else
		    subEntry[count2] = (unsigned char)
		      listItemPointer->name[longFilenamePos++];
		}

	      // Put the "long filename entry" attribute byte into
	      // the attribute slot
	      (unsigned char) subEntry[0x0B] = 0x0F;

	      // Put the file's 8.3 checksum into the 0x0Dth spot
	      subEntry[0x0D] = (unsigned char) fileCheckSum;

	      // Put the next six 2-byte characters
	      for (count2 = 14; count2 < 26; count2 += 2)
		{
		  if (longFilenamePos > fileNameLength)
		    {
		      subEntry[count2] = (unsigned char) 0xFF;
		      subEntry[count2 + 1] = (unsigned char) 0xFF;
		    }
		  else
		    subEntry[count2] = (unsigned char)
		      listItemPointer->name[longFilenamePos++];
		}

	      // Put the last two 2-byte characters
	      for (count2 = 28; count2 < 32; count2 += 2)
		{
		  if (longFilenamePos > fileNameLength)
		    {
		      subEntry[count2] = (unsigned char) 0xFF;
		      subEntry[count2 + 1] = (unsigned char) 0xFF;
		    }
		  else
		    subEntry[count2] = (unsigned char)
		      listItemPointer->name[longFilenamePos++];
		}

	      // Determine whether this was the last long filename
	      // entry for this file.  If not, we subtract 
	      // FAT_BYTES_PER_DIR_ENTRY from subEntry and loop
	      if (count == (longFilenameSlots - 1))
		break;
	      else
		subEntry -= FAT_BYTES_PER_DIR_ENTRY;
	    }

	  // Move to the next free directory entry
	  dirEntry +=  FAT_BYTES_PER_DIR_ENTRY;
	}

      // Copy the short alias into the entry.
      dirEntry[0] = NULL;
      strncpy(dirEntry, (char *) shortAlias, 11);

      // attributes (byte value)
      dirEntry[0x0B] = (unsigned char) entryData->attributes;

      // reserved (byte value)
      dirEntry[0x0C] = (unsigned char) entryData->res;

      // timeTenth (byte value)
      dirEntry[0x0D] = (unsigned char) entryData->timeTenth;

      // Creation time (word value)
      temp = makeDosTime(realEntry->creationTime);
      dirEntry[0x0E] = (unsigned char)(temp & 0x000000FF);
      dirEntry[0x0F] = (unsigned char)(temp >> 8);

      // Creation date (word value)
      temp = makeDosDate(realEntry->creationDate);
      dirEntry[0x10] = (unsigned char)(temp & 0x000000FF);
      dirEntry[0x11] = (unsigned char)(temp >> 8);

      // Accessed date (word value)
      temp = makeDosDate(realEntry->accessedDate);
      dirEntry[0x12] = (unsigned char) (temp & 0x000000FF);
      dirEntry[0x13] = (unsigned char) (temp >> 8);

      // startClusterHi (word value)
      dirEntry[0x14] = 
	(unsigned char) ((entryData->startCluster & 0x00FF0000) >> 16);
      dirEntry[0x15] = 
	(unsigned char)	((entryData->startCluster & 0xFF000000) >> 24);

      // lastWriteTime (word value)
      temp = makeDosTime(realEntry->modifiedTime);
      dirEntry[0x16] = (unsigned char) (temp & 0x000000FF);
      dirEntry[0x17] = (unsigned char) (temp >> 8);

      // lastWriteDate (word value)
      temp = makeDosDate(realEntry->modifiedDate);
      dirEntry[0x18] = (unsigned char) (temp & 0x000000FF);
      dirEntry[0x19] = (unsigned char) (temp >> 8);

      // startCluster (word value) 
      dirEntry[0x1A] = (unsigned char) (entryData->startCluster & 0x000000FF);
      dirEntry[0x1B] = (unsigned char) (entryData->startCluster >> 8);

      // Now we get the size.  If it's a directory we write zero for the size
      // (doubleword value)
      if ((entryData->attributes & FAT_ATTRIB_SUBDIR) != 0)
	*((unsigned *)(dirEntry + 0x1C)) = 0;
      else
	*((unsigned *)(dirEntry + 0x1C)) = realEntry->size;

      // Increment to the next directory entry spot
      dirEntry += FAT_BYTES_PER_DIR_ENTRY;

      // Increment to the next file structure
      listItemPointer = listItemPointer->nextEntry;
    }

  // Put a NULL entry in the last spot.
  dirEntry[0] = '\0';

  return (status = 0);
}


static int checkFileChain(fatInternalData *fatData, kernelFileEntry *checkFile)
{
  // This function is used to make sure (as much as possible) that the 
  // cluster allocation chain of a file is sane.  This is important so
  // that we don't end up (for example) deleting clusters that belong
  // to other files, etc.  It takes a file entry as its parameter

  int status = 0;
  fatEntryData *entryData = NULL;
  unsigned clusterSize = 0;
  unsigned expectedClusters = 0;
  unsigned allocatedClusters = 0;

  // Get the entry's data
  entryData = (fatEntryData *) checkFile->fileEntryData;

  if (entryData == NULL)
    {
      kernelError(kernel_error, "File entry has no private filesystem data");
      return (status = ERR_BUG);
    }

  // Make sure there is supposed to be a starting cluster for this file.
  // If not, make sure that the file's size is supposed to be zero
  if (entryData->startCluster == 0)
    {
      if (checkFile->size == 0)
	// Nothing to do for a zero-length file.
	return (status = 0);

      else
	{
	  kernelError(kernel_error, "Non-zero-length file has no clusters "
		      "allocated");
	  return (status = ERR_BADDATA);
	}
    }

  // Calculate the cluster size for this filesystem
  clusterSize = (fatData->bytesPerSector * fatData->sectorsPerCluster);

  // Calculate the number of clusters we would expect this file to have
  if (clusterSize != 0)
    {
      expectedClusters = (checkFile->size / clusterSize);
      if ((checkFile->size % clusterSize) != 0)
	expectedClusters += 1;
    }
  else
    {
      // This volume would appear to be corrupted
      kernelError(kernel_error, "The FAT volume is corrupt");
      return (status = ERR_BADDATA);
    }

  // We count the number of clusters used by this file, according to
  // the allocation chain
  status = getNumClusters(fatData, entryData->startCluster,
			  &allocatedClusters);
  if (status < 0)
    {
      return (status); 
    }

  // Now, just reconcile the expected size against the number of expected
  // clusters
  if (allocatedClusters == expectedClusters)
    return (status = 0);

  else if (allocatedClusters > expectedClusters)
    {
      kernelError(kernel_error, "Clusters allocated exceeds nominal size");
      return (status = ERR_BADDATA);
    }

  else
    {
      kernelError(kernel_error, "Clusters allocated are less than nominal "
		  "size");
      return (status = ERR_BADDATA);
    }
}


static int releaseEntryClusters(fatInternalData *fatData,
				kernelFileEntry *deallocateFile)
{
  // This function is internal, and is used to deallocate the cluster
  // chain associated with a file entry.  Returns 0 on success, negative
  // otherwise

  int status = 0;
  fatEntryData *entryData = NULL;

  // Get the entry's data
  entryData = (fatEntryData *) deallocateFile->fileEntryData;

  if (entryData == NULL)
    {
      kernelError(kernel_error, "File entry has no private filesystem data");
      return (status = ERR_BUG);
    }

  if (entryData->startCluster)
    {
      // Deallocate the clusters belonging to the file
      status = releaseClusterChain(fatData, entryData->startCluster);

      if (status)
	{
	  kernelError(kernel_error, "Unable to deallocate file's clusters");
	  return (status);
	}

      // Assign '0' to the file's entry's startcluster
      entryData->startCluster = 0;
    }

  // Update the size of the file
  deallocateFile->blocks = 0;
  deallocateFile->size = 0;
  
  return (status = 0);
}


static int write(fatInternalData *fatData, kernelFileEntry *writeFile, 
		 unsigned skipClusters, unsigned writeClusters,
		 unsigned char *buffer)
{
  // This function is internal, and takes as a parameter the data structure
  // of a file/directory.  The function will write the file to the disk
  // according to the data structure's information.  It will allocate new
  // clusters if the file needs to grow.  It assumes the buffer is big
  // enough and contains the appropriate amount of data (it doesn't
  // double-check this).  On success, it returns the number of clusters
  // actually written.  Negative otherwise.

  int status = 0;
  fatEntryData *entryData = NULL;
  unsigned clusterSize = 0;
  unsigned existingClusters = 0;
  unsigned needClusters = 0;
  unsigned lastCluster = 0;
  unsigned currentCluster = 0;
  unsigned nextCluster = 0;
  unsigned savedClusters = 0;
  unsigned startSavedClusters = 0;
  unsigned count;

  // Get the entry's data
  entryData = (fatEntryData *) writeFile->fileEntryData;

  if (entryData == NULL)
    {
      kernelError(kernel_error, "Entry has no data");
      return (status = ERR_NODATA);
    }

  // Calculate cluster size
  clusterSize = 
    (unsigned)(fatData->bytesPerSector * fatData->sectorsPerCluster);

  // Make sure there's enough free space on the volume BEFORE beginning the
  // write operation
  while (writeClusters > fatData->freeClusters)
    {
      // If we are still building the free cluster bitmap, there might
      // be enough free shortly
      if (fatData->buildingFreeBitmap)
	{
	  kernelMultitaskerYield();
	  continue;
	}

      // Not enough free space on the volume.  Make an error
      kernelError(kernel_error, "There is not enough free space on the "
		  "volume to complete the operation");
      return (status = ERR_NOFREE);
    }

  // How many clusters are currently allocated to this file?  Are there
  // already enough clusters to complete this operation (including any
  // clusters we're skipping)?

  needClusters = (skipClusters + writeClusters);

  status = getNumClusters(fatData, entryData->startCluster, &existingClusters);
  if (status < 0)
    {
      kernelError(kernel_error, "Unable to determine cluster count of file or "
		  "directory \"%s\"", writeFile->name);
      return (status = ERR_BADDATA);
    }

  if (existingClusters < needClusters)
    {
      // We need the difference between the current clusters and the needed
      // clusters
      needClusters = (needClusters - existingClusters);

      // We will need to allocate some more clusters
      status = getUnusedClusters(fatData, needClusters, &currentCluster);
      if (status < 0)
	{
	  kernelError(kernel_error, "Not enough free clusters");
	  return (status);
	}

      // Get the number of the current last cluster
      status = getLastCluster(fatData, entryData->startCluster, &lastCluster);
      if (status < 0)
	{
	  kernelError(kernel_error, "Unable to determine file's last "
		      "cluster");
	  return (status);
	}

      // If the last cluster is zero, then the file currently has no clusters
      // and we should set entryData->startCluster to the value returned
      // from getUnusedClusters.  Otherwise, the value from getUnusedClusters
      // should be assigned to lastCluster

      if (lastCluster)
	{
	  // Attach these new clusters to the file's chain
	  status = changeFatEntry(fatData, lastCluster, currentCluster);
	  if (status < 0)
	    {
	      kernelError(kernel_error, "Error connecting new clusters");
	      return (status);
	    }
	}
      
      else
	{
	  entryData->startCluster = currentCluster;
	}
    }

  // Get the starting cluster of the file
  currentCluster = entryData->startCluster;

  // Skip through the FAT entries until we've used up our 'skip' clusters
  for ( ; skipClusters > 0; skipClusters--)
    {
      status = getFatEntry(fatData, currentCluster, &currentCluster);
      if (status < 0)
	{
	  kernelError(kernel_error,
		      "Error reading FAT entry while skipping clusters");
	  return (status);
	}
    }

  // We already know the first cluster
  startSavedClusters = currentCluster;
  savedClusters = 1;
  
  // This is the loop where we write the clusters
  for (count = 0; count < writeClusters; count ++)
    {
      // At the start of this loop, we know the current cluster.  If this
      // is not the last cluster we're reading, peek at the next one
      if (count < (writeClusters - 1))
	{
	  status = getFatEntry(fatData, currentCluster, &nextCluster);
	  if (status < 0)
	    {
	      kernelError(kernel_error,
			  "Error reading FAT entry in existing chain");
	      return (status);
	    }

	  // We want to minimize the number of write operations, so if
	  // we get clusters with consecutive numbers we should read them
	  // all in a single operation
	  if (nextCluster == (currentCluster + 1))
	    {
	      if (savedClusters == 0)
		startSavedClusters = currentCluster;
	      
	      currentCluster = nextCluster;
	      savedClusters += 1;
	      continue;
	    }
	}

      // Alright, we can write the clusters we were saving up
      
      status =
	kernelDiskWriteSectors((char *) fatData->disk->name,
			       (((startSavedClusters - 2) *
				 fatData->sectorsPerCluster) +
				fatData->reservedSectors +
				(fatData->fatSectors * 2) +
				fatData->rootDirSectors),
			       (fatData->sectorsPerCluster * savedClusters),
			       buffer);
      if (status < 0)
	{
	  kernelError(kernel_error, "Error writing file");
	  return (status);
	}

      // Increment the buffer pointer
      buffer += (clusterSize * savedClusters);

      // Move to the next cluster
      currentCluster = nextCluster;

      // Reset our counts
      startSavedClusters = currentCluster;
      savedClusters = 1;
    }
  
  // Adjust the size of the file
  status = getNumClusters(fatData, entryData->startCluster,
			  (unsigned *) &(writeFile->blocks));
  if (status < 0)
    return (status);

  writeFile->size = (writeFile->blocks * fatData->sectorsPerCluster *
		     fatData->bytesPerSector);

  // Write out dirty FAT sectors
  writeDirtyFatSects(fatData);

  return (count);
}


static int checkFilename(volatile char *fileName)
{
  // This function is internal, and is used to ensure that new file
  // names are legal in the FAT filesystem.  It scans the filename
  // string looking for anything illegal.  If the filename is OK, it
  // returns 0, otherwise negative

  int fileNameOK = 0;
  int nameLength = 0;
  int count;

  // Get the length of the filename
  nameLength = strlen((char *)fileName);

  // Make sure the length of the filename is under the limit
  if (nameLength > MAX_NAME_LENGTH)
    return (fileNameOK = ERR_BOUNDS);

  // Make sure there's not a ' ' in the first position
  if (fileName[0] == (char) 0x20)
    return (fileNameOK = ERR_INVALID);
  
  // Loop through the entire filename, looking for any illegal
  // characters
  for (count = 0; count < nameLength; count ++)
    {
      if ((fileName[count] == (char) 0x22) ||
	  (fileName[count] == (char) 0x2a) ||
	  (fileName[count] == (char) 0x2f) ||
	  (fileName[count] == (char) 0x3a) ||
	  (fileName[count] == (char) 0x3c) ||
	  (fileName[count] == (char) 0x3e) ||
	  (fileName[count] == (char) 0x3f) ||
	  (fileName[count] == (char) 0x5c) ||
	  (fileName[count] == (char) 0x7c))
	{
	  fileNameOK = -1;
	  break;
	}
    }

  return (fileNameOK);
}


static char xlateShortAliasChar(char theChar)
{
  // This function is used internally to translate characters 
  // from long filenames into characters valid for short aliases

  char returnChar = NULL;

  // Unprintable control characters turn into '_'
  if (theChar < (char) 0x20)
    return (returnChar = '_');

  // Likewise, these illegal characters turn into '_'
  else if ((theChar == '"') || (theChar == '*') || (theChar == '+') ||
	   (theChar == ',') || (theChar == '/') || (theChar == ':') ||
	   (theChar == ';') || (theChar == '<') || (theChar == '=') ||
	   (theChar == '>') || (theChar == '?') || (theChar == '[') ||
	   (theChar == '\\') || (theChar == ']') || (theChar == '|'))
    return (returnChar = '_');

  // Capitalize any lowercase alphabetical characters
  else if ((theChar >= (char) 0x61) && (theChar <= (char) 0x7A))
    return (returnChar = (theChar - (char) 0x20));

  // Everything else is okay
  else
    return (returnChar = theChar);
}


static int makeShortAlias(kernelFileEntry *theFile)
{
  // This function is used internally to create the short alias in
  // a file structure.

  int status = 0;
  fatEntryData *entryData = NULL;
  kernelFileEntry *listItemPointer = NULL;
  fatEntryData *listItemData = NULL;
  char nameCopy[MAX_NAME_LENGTH];
  char aliasName[9];
  char aliasExt[4];
  int lastDot = 0;
  int shortened = 0;
  int tildeSpot = 0;
  int tildeNumber = 1;
  int count;
  
  // Get the entry's data
  entryData = (fatEntryData *) theFile->fileEntryData;
  if (entryData == NULL)
    {
      kernelError(kernel_error, "File has no private filesystem data");
      return (status = ERR_BUG);
    }

  // The short alias field of the file structure is a 13-byte field
  // with room for the 8 filename characters, a '.', the 3 extension
  // characters, and a NULL.  It must be in all capital letters, and
  // there is a particular shortening algorithm used.  This algorithm
  // is defined in the Microsoft FAT Filesystem Specification.

  // Initialize the shortAlias name and extension, since they both need to
  // be padded with [SPACE] characters

  strcpy(aliasName, "        ");
  strcpy(aliasExt, "   ");

  // Now, this is a little tricky.  We have to examine the first six
  // characters of the long filename, and interpret them according
  // to the predefined format.
  // - All capitals
  // - There are a bunch of illegal characters we have to check for
  // - Illegal characters are replaced with underscores
  // - "~#" is placed in the last two slots of the filename, if either the
  //   name or extension has been shortened.  # is based on the number of
  //   files in the same directory that conflict.  We have to search the
  //   directory for name clashes of this sort
  // - The file extension (at least the "last" file extension) is 
  //   kept, except that it is truncated to 3 characters if necessary.

  // Loop through the file name, translating characters to ones that are
  // legal for short aliases, and put them into a copy of the file name
  // Make a copy of the name
  kernelMemClear(nameCopy, MAX_NAME_LENGTH);
  int tmpCount = 0;
  for (count = 0; count < (MAX_NAME_LENGTH - 1); count ++)
    {
      if (theFile->name[count] == 0x20)
	// Skip spaces
	continue;

      if (theFile->name[count] == '\0')
	{
	  // Finished
	  nameCopy[tmpCount] = '\0';
	  break;
	}
      
      else
	// Copy the character, but make sure it's legal.
	nameCopy[tmpCount++] = xlateShortAliasChar(theFile->name[count]);
    }

  // Find the last '.'  This will determine what makes up the name and what
  // makes up the extension.
  for (count = (strlen(nameCopy) - 1); count > 0; count --)
    if (nameCopy[count] == '.')
      {
	lastDot = count;
	break;
      }

  if (!lastDot)
    {
      // No extension.  Just copy the name up to 8 chars.
      for (count = 0; ((count < 8) && (count < strlen(nameCopy))); count ++)
	aliasName[count] = nameCopy[count];

      lastDot = strlen(nameCopy);
      
      if (strlen(nameCopy) > 8)
	shortened = 1;
    }
  
  // Now, if we actually found a '.' in something other than the first
  // spot...
  else
    {
      // Copy the name up to 8 chars.
      for (count = 0; ((count < 8) && (count < lastDot) &&
		       (count < strlen(nameCopy))); count ++)
	aliasName[count] = nameCopy[count];

      if (lastDot > 7)
	shortened = 1;

      // There's an extension.  Copy it.
      for (count = 0; ((count < 3) &&
		       (count < strlen(nameCopy + lastDot + 1))); count ++)
	aliasExt[count] = nameCopy[lastDot + 1 + count];

      if (strlen(nameCopy + lastDot + 1) > 3)
	// We are shortening the extension part
	shortened = 1;
    }

  tildeSpot = lastDot;
  if (tildeSpot > 6)
    tildeSpot = 6;

  // If we shortened anything, we have to stick that goofy tilde thing on
  // the end.  Yay for Microsoft; that's a very innovative solution to the
  // long filename problem.  Isn't that innovative?  Innovating is an
  // important way to innovate the innovation -- that's what I always say.
  // Microsoft is innovating into the future of innovation.  (That's 7 points
  // for me, plus I get an extra one for using four different forms of the
  // word)
  if (shortened)
    {
      // We start with this default configuration before we
      // go looking for conflicts
      aliasName[tildeSpot] = '~';
      aliasName[tildeSpot + 1] = '1';
    }

  // Concatenate the name and extension
  strncpy((char *) entryData->shortAlias, aliasName, 8);
  strncpy((char *) (entryData->shortAlias + 8), aliasExt, 3);

  // Make sure there aren't any name conflicts in the file's directory
  listItemPointer = 
    ((kernelFileEntry *) theFile->parentDirectory)->contents;

  while (listItemPointer != NULL)
    {
      if (listItemPointer != theFile)
	{
	  // Get the list item's data
	  listItemData = (fatEntryData *) listItemPointer->fileEntryData;
	  if (listItemData == NULL)
	    {
	      kernelError(kernel_error, "File has no private filesystem "
			  "data");
	      return (status = ERR_BUG);
	    }
	  
	  if (!strcmp((char *) listItemData->shortAlias, 
		      (char *) entryData->shortAlias))
	    {
	      // Conflict.  Up the ~# thing we're using
	      
	      tildeNumber += 1;
	      if (tildeNumber >= 100)
		{
		  // Too many name clashes
		  kernelError(kernel_error, "Too many short alias name "
			      "clashes");
		  return (status = ERR_NOFREE);
		}
	      
	      if (tildeNumber >= 10)
		{
		  entryData->shortAlias[tildeSpot - 1] = '~';
		  entryData->shortAlias[tildeSpot] = 
		    ((char) 48 + (char) (tildeNumber / 10));
		}
	      
	      entryData->shortAlias[tildeSpot + 1] =
		((char) 48 + (char) (tildeNumber % 10));
	      
	      listItemPointer = 
		((kernelFileEntry *) theFile->parentDirectory)->contents;
	      continue;
	    }
	}

      listItemPointer = listItemPointer->nextEntry;
    }
  
  return (status = 0);
}


static int scanDirectory(fatInternalData *fatData, 
			 kernelFilesystem *filesystem, 
			 kernelFileEntry *currentDir, char *dirBuffer, 
			 unsigned dirBufferSize)
{
  // This function takes a pointer to a directory buffer and installs
  // files and directories in the file/directory list for each thing 
  // it finds.  Basically, this is the FAT directory scanner.

  int status = 0;
  unsigned char *dirEntry;
  unsigned char *subEntry;
  int longFilename = 0;
  int longFilenamePos = 0;
  int count1, count2, count3;

  kernelFileEntry *newItem = NULL;
  fatEntryData *entryData = NULL;

  // Manufacture some "." and ".." entries
  status = kernelFileMakeDotDirs(filesystem, currentDir->parentDirectory,
 				 currentDir);
  if (status < 0)
    kernelError(kernel_warn, "Unable to create '.' and '..' directory "
 		"entries");

  for (count1 = 0; count1 < (dirBufferSize / FAT_BYTES_PER_DIR_ENTRY); 
       count1 ++)
    {
      // Make dirEntry point to the current entry in the dirBuffer
      dirEntry = (dirBuffer + (count1 * FAT_BYTES_PER_DIR_ENTRY));

      // Now we must determine whether this is a valid, undeleted file.
      // E5 means this is a deleted entry
      if (dirEntry[0] == (unsigned char) 0xE5)
	continue;

      // 05 means that the first character is REALLY E5
      else if (dirEntry[0] == (unsigned char) 0x05)
	dirEntry[0] = (unsigned char) 0xE5;

      // 00 means that there are no more entries
      else if (dirEntry[0] == (unsigned char) 0x00)
	break;

      else if (dirEntry[0x0B] == 0x0F)
	// It's a long filename entry.  Skip it until we get to the regular
	// entry
	continue;

      // Skip '.' and '..' entries
      else if (!strncmp(dirEntry, ".          ", 11) ||
	       !strncmp(dirEntry, "..         ", 11))
	continue;

      // Peek ahead and get the attributes (byte value).  Figure out the 
      // type of the file
      if (((unsigned) dirEntry[0x0B] & FAT_ATTRIB_VOLUMELABEL) != 0)
	// Then it's a volume label.  Skip it for now
	continue;

      // If we fall through to here, it must be a good file or directory.

      // Now we should create a new entry in the "used" list for this item

      // Get a free file entry structure.
      newItem = kernelFileNewEntry(filesystem);
      if (newItem == NULL)
	{
	  // Not enough free file structures
	  kernelError(kernel_error, "Not enough free file structures");
	  return (status = ERR_NOFREE);
	}

      // Get the entry data structure.  This should have been created by
      // a call to our NewEntry function by the kernelFileNewEntry call.
      entryData = (fatEntryData *) newItem->fileEntryData;
      if (entryData == NULL)
	{
	  kernelError(kernel_error, "Entry has no private filesystem data");
	  return (status = ERR_NODATA);
	}

      // Check for a long filename entry by looking at the attributes
      // of the entry that occurs before this one.  Check whether the
      // appropriate attribute bits are set.  Also make sure it isn't
      // a deleted long filename entry

      subEntry = (dirEntry - 32);

      if ((count1 > 0) && (subEntry[0x0B] == 0x0F))
	{
	  longFilename = 1;
	  longFilenamePos = 0;
	      
	  while (1)
	    {
	      // Get the first five 2-byte characters from this entry
	      for (count3 = 1; count3 < 10; count3 += 2)
		newItem->name[longFilenamePos++] = 
		  (unsigned char) subEntry[count3];
	      
	      // Get the next six 2-byte characters
	      for (count3 = 14; count3 < 26; count3 += 2)
		newItem->name[longFilenamePos++] = 
		  (unsigned char) subEntry[count3];
	      
	      // Get the last two 2-byte characters
	      for (count3 = 28; count3 < 32; count3 += 2)
		newItem->name[longFilenamePos++] = 
		  (unsigned char) subEntry[count3];

	      // Determine whether this was the last long filename
	      // entry for this file.  If not, we subtract 32 from 
	      // subEntry and loop
	      if ((subEntry[0] & 0x40) != 0)
		break;
	      else
		subEntry -= 32;
	    }

	  newItem->name[longFilenamePos] = NULL;
	}
      else
	longFilename = 0;

      // Now go through the regular (DOS short) entry for this file.

      // Copy short alias into the shortAlias field of the file structure
      strncpy((char *) entryData->shortAlias, dirEntry, 11);
      entryData->shortAlias[11] = '\0';

      // If there's no long filename, set the filename to be the same as
      // the short alias we just extracted.  We'll need to construct it
      // from the drain-bamaged format used by DOS(TM)
      if (longFilename == 0)
	{
	  strncpy((char *) newItem->name, (char *) entryData->shortAlias, 8);
	  newItem->name[8] = '\0';

	  // Insert a NULL if there's a [space] character anywhere in 
	  // this part
	  for (count2 = 0; count2 < strlen((char *) newItem->name); count2 ++)
	    if (newItem->name[count2] == ' ')
	      newItem->name[count2] = NULL;
      
	  // If the extension is non-empty, insert a '.' character in the
	  // middle between the filename and extension
	  if (entryData->shortAlias[8] != ' ')
	    strncat((char *) newItem->name, ".", 1);
      
	  // Copy the filename extension
	  strncat((char *) newItem->name, ((char *) entryData->shortAlias + 8),
		  3);

	  // Insert a NULL if there's a [space] character anywhere in 
	  // the name
	  for (count2 = 0; count2 < strlen((char *) newItem->name); count2 ++)
	    if (newItem->name[count2] == ' ')
	      newItem->name[count2] = NULL;

	  // Short filenames are case-insensitive, and are usually 
	  // represented by all-uppercase letters.  This looks silly
	  // in the modern world, so we convert them all to lowercase
	  // as a matter of preference.
	  for (count2 = 0; count2 < strlen((char *) newItem->name); count2 ++)
	    newItem->name[count2] =
	      (char) tolower(newItem->name[count2]);
	}

      // Get the entry's various other information

      // Attributes (byte value)
      entryData->attributes = (unsigned) dirEntry[0x0B];

      if ((entryData->attributes & FAT_ATTRIB_SUBDIR) != 0)
	// Then it's a subdirectory
	newItem->type = dirT;
      else
	// It's a regular file
	newItem->type = fileT;

      // reserved (byte value)
      entryData->res = (unsigned) dirEntry[0x0C];

      // timeTenth (byte value)
      entryData->timeTenth = (unsigned) dirEntry[0x0D];

      // Creation time (word value)
      newItem->creationTime = 
	makeSystemTime(((unsigned) dirEntry[0x0F] << 8) + 
		       (unsigned) dirEntry[0x0E]);

      // Creation date (word value)
      newItem->creationDate = 
	makeSystemDate(((unsigned) dirEntry[0x11] << 8) + 
		       (unsigned) dirEntry[0x10]);

      // Last access date (word value)
      newItem->accessedDate = 
	makeSystemDate(((unsigned) dirEntry[0x13] << 8) + 
		       (unsigned) dirEntry[0x12]);

      // High word of startCluster (word value)
      entryData->startCluster = (((unsigned) dirEntry[0x15] << 24) + 
				 ((unsigned) dirEntry[0x14] << 16));

      // Last modified time (word value)
      newItem->modifiedTime = 
	makeSystemTime(((unsigned) dirEntry[0x17] << 8) + 
		       (unsigned) dirEntry[0x16]);

      // Last modified date (word value)
      newItem->modifiedDate = 
	makeSystemDate(((unsigned) dirEntry[0x19] << 8) + 
		       (unsigned) dirEntry[0x18]);

      // Low word of startCluster (word value) 
      entryData->startCluster |= (((unsigned) dirEntry[0x1B] << 8) + 
				  (unsigned) dirEntry[0x1A]);

      // Now we get the size.  If it's a directory we have to actually
      // call getNumClusters() to get the size in clusters

      status = getNumClusters(fatData, entryData->startCluster,
			      (unsigned *) &(newItem->blocks));
      if (status < 0)
	{
	  kernelError(kernel_error, "Couldn't determine the number of "
		      "clusters");
	  return (status);
	}

      if (entryData->attributes & FAT_ATTRIB_SUBDIR)
	newItem->size = (newItem->blocks * (fatData->bytesPerSector * 
					    fatData->sectorsPerCluster));
      else
	// (doubleword value)
	newItem->size = *((unsigned *)(dirEntry + 0x1C));

      // Add our new entry to the existing file chain.  Don't panic and/or
      // quit if we have a problem of some sort
      kernelFileInsertEntry(newItem, currentDir);
    }

  return (status = 0);
}


static int read(fatInternalData *fatData, kernelFileEntry *theFile, 
		unsigned skipClusters, unsigned readClusters, 
		unsigned char *buffer)
{
  // This function is internal, and is used to read the requested file
  // data into a buffer.  The function will read the respective
  // sectors of the file into the buffer provided as the last argument.
  // The function assumes that the buffer is large enough to hold the entire
  // file.  It doesn't double-check this.  On success, it returns the number
  // of clusters actually read.  Negative otherwise.

  int status = 0;
  fatEntryData *entryData = NULL;
  unsigned fileClusters = 0;
  unsigned clusterSize = 0;
  unsigned currentCluster = 0;
  unsigned nextCluster = 0;
  unsigned savedClusters = 0;
  unsigned startSavedClusters = 0;
  unsigned count;

  // Get the entry's data
  entryData = (fatEntryData *) theFile->fileEntryData;
  if (entryData == NULL)
    {
      kernelError(kernel_error, "Entry has no data");
      return (status = ERR_BUG);
    }

  // Calculate cluster size
  clusterSize = (unsigned)
    (fatData->bytesPerSector * fatData->sectorsPerCluster);

  currentCluster = entryData->startCluster;

  // Skip through the FAT entries until we've used up our 'skip' clusters
  for ( ; skipClusters > 0; skipClusters--)
    {
      status = getFatEntry(fatData, currentCluster, &currentCluster);
      if (status < 0)
	{
	  kernelError(kernel_error, "Error reading FAT entry");
	  return (status);
	}
    }

  // Now, it's possible that the file actually contains fewer clusters
  // than the 'readClusters' value.  If so, replace our readClusters value
  // with that value
  status = getNumClusters(fatData, currentCluster, &fileClusters);
  if (status < 0)
    return (status);

  if (fileClusters < readClusters)
    readClusters = fileClusters;
  
  // We already know the first cluster
  startSavedClusters = currentCluster;
  savedClusters = 1;
  
  // Now we go through a loop, reading the cluster numbers from the FAT,
  // then reading the sector info the buffer.

  for (count = 0; count < readClusters; count ++)
    {
      // At the start of this loop, we know the current cluster.  If this
      // is not the last cluster we're reading, peek at the next one
      if (count < (readClusters - 1))
	{
	  status = getFatEntry(fatData, currentCluster, &nextCluster);
	  if (status < 0)
	    {
	      kernelError(kernel_error, "Error reading FAT entry");
	      return (status);
	    }

	  // We want to minimize the number of read operations, so if
	  // we get clusters with consecutive numbers we should read
	  // them all in a single operation
	  if (nextCluster == (currentCluster + 1))
	    {
	      if (savedClusters == 0)
		startSavedClusters = currentCluster;
	      
	      currentCluster = nextCluster;
	      savedClusters += 1;
	      continue;
	    }
	}
      
      // Read the cluster into the buffer.

      status =
	kernelDiskReadSectors((char *) fatData->disk->name,
			      (((startSavedClusters - 2) *
				fatData->sectorsPerCluster) +
			       fatData->reservedSectors +
			       (fatData->fatSectors * 2) +
			       fatData->rootDirSectors), 
			      (fatData->sectorsPerCluster * savedClusters),
			      buffer);
     if (status < 0)
	{
	  kernelError(kernel_error, "Error reading file");
	  return (status);
	}

      // Increment the buffer pointer
      buffer += (clusterSize * savedClusters);

      // Move to the next cluster
      currentCluster = nextCluster;

      // Reset our counts
      startSavedClusters = currentCluster;
      savedClusters = 1;
    }

  return (count);
}


static int readRootDir(fatInternalData *fatData, kernelFilesystem *filesystem)
{
  // This function reads the root directory from the disk indicated by the
  // filesystem pointer.  It assumes that the requirement to do so has been
  // met (i.e. it does not check whether the update is necessary, and does so
  // unquestioningly).  It returns 0 on success, negative otherwise.
  
  // This is only used internally, so there is no need to check the
  // disk structure.  The functions that are exported will do this.
  
  int status = 0;
  kernelFileEntry *rootDir = NULL;
  unsigned rootDirStart = 0;
  unsigned char *dirBuffer = NULL;
  unsigned dirBufferSize = 0;
  fatEntryData *rootDirData = NULL;
  fatEntryData dummyEntryData;
  int rootDirBlocks = 0;
  kernelFileEntry dummyEntry;

  // The root directory scheme is different depending on whether this is
  // a FAT32 volume or a FAT12/16 volume.  If it is not FAT32, then the
  // root directory is in a fixed location, with a fixed size (which we
  // can determine using values in the fatData structure).  If the volume
  // is FAT32, then we can treat it like a regular directory, with the
  // starting cluster number in the fatData->rootDirClusterF32 field.
  
  if ((fatData->fsType == fat12) || (fatData->fsType == fat16))
    // Allocate a directory buffer based on the number of bytes per 
    // sector and the number of FAT sectors on the volume.  Since
    // we are working with a FAT12 or FAT16 volume, the root 
    // directory size is fixed. 
    dirBufferSize = (fatData->bytesPerSector * fatData->rootDirSectors);

  else // if (fatData->fsType == fat32)
    {
      // We need to take the starting cluster of the FAT32 root directory,
      // and determine the size of the directory.
      status = getNumClusters(fatData, fatData->rootDirClusterF32,
			      &dirBufferSize);
      if (status < 0)
	return (status);

      dirBufferSize *= (fatData->bytesPerSector *
			fatData->sectorsPerCluster);
    }

  dirBuffer = kernelMalloc(dirBufferSize);
  if (dirBuffer == NULL)
    {
      kernelError(kernel_error, "NULL directory buffer");
      return (status = ERR_MEMORY);
    }

  // Here again, we diverge depending on whether this is FAT12/16
  // or FAT32.  For FAT12/16, we will be reading consecutive sectors
  // from the front part of the disk.  For FAT32, we will be treating
  // the root directory just like any other old directory.
  
  if ((fatData->fsType == fat12) || (fatData->fsType == fat16))
    {
      // This is not FAT32, so we have to calculate the starting SECTOR of 
      // the root directory.  It's like this:  
      // bootSector + (numberOfFats * fatSectors)
      rootDirStart = (fatData->reservedSectors + (fatData->numberOfFats * 
						  fatData->fatSectors));

      // Now we need to read some straight sectors from the disk which
      // make up the root directory

      // Now we read all of the sectors for the root directory
      status = kernelDiskReadSectors((char *) fatData->disk->name,
				     rootDirStart, fatData->rootDirSectors,
				     dirBuffer);
      if (status < 0)
	{
	  kernelFree(dirBuffer);
	  return (status);
	}

      rootDirBlocks = (fatData->rootDirSectors / fatData->sectorsPerCluster);
    }

  else // if (fatData->fsType == fat32)
    {
      // We need to read in the FAT32 root directory now.  This is just
      // a regular directory -- we already know the size and starting
      // cluster, so we need to fill out a dummy kernelFileEntry structure so
      // that the read() routine can go get it for us.

      status = getNumClusters(fatData, fatData->rootDirClusterF32,
			      &rootDirBlocks);
      if (status < 0)
	{
	  kernelFree(dirBuffer);
	  return (status);
	}

      // The only thing the read routine needs in this data structure
      // is the starting cluster number.
      dummyEntryData.startCluster = fatData->rootDirClusterF32;
      dummyEntry.fileEntryData = (void *) &dummyEntryData;

      // Go.
      status = read(fatData, &dummyEntry, 0, rootDirBlocks, dirBuffer);
      // Were we successful?
      if (status < 0)
	{
	  kernelFree(dirBuffer);
	  return (status);
	}
    }

  // The whole root directory should now be in our buffer.  We can proceed
  // to make the applicable data structures

  rootDir = filesystem->filesystemRoot;

  // Get the entry data structure.  This should have been created by
  // a call to our NewEntry function by the kernelFileNewEntry call.
  rootDirData = (fatEntryData *) rootDir->fileEntryData;
  if (rootDirData == NULL)
    {
      kernelError(kernel_error, "Entry has no private data");
      kernelFileReleaseEntry(rootDir);
      kernelFree(dirBuffer);
      return (status = ERR_NODATA);
    }

  // Fill out some starting values in the file entry structure

  rootDir->blocks = rootDirBlocks;
  
  if ((fatData->fsType == fat12) || (fatData->fsType == fat16))
    {
      rootDir->size = (fatData->rootDirSectors * fatData->bytesPerSector);
      rootDirData->startCluster = 0;
    }
  else
    {
      rootDir->size = (rootDir->blocks * fatData->sectorsPerCluster * 
		       fatData->bytesPerSector);
      rootDirData->startCluster = fatData->rootDirClusterF32;
    }

  // Fill out some values in the directory's private data
  strncpy((char *) rootDirData->shortAlias, "/", 2);
  rootDirData->attributes = (FAT_ATTRIB_SUBDIR | FAT_ATTRIB_SYSTEM);

  // We have to read the directory and fill out the chain of its
  // files/subdirectories in the lists.
  status = scanDirectory(fatData, filesystem, rootDir, dirBuffer,
			 dirBufferSize);

  kernelFree(dirBuffer);

  if (status < 0)
    {
      kernelError(kernel_error, "Error parsing root directory");
      kernelFileReleaseEntry(rootDir);
      return (status);
    }

  // Return success
  return (status = 0);
}


static fatInternalData *getFatData(kernelFilesystem *filesystem)
{
  // This function reads the filesystem parameters from the control
  // structures on disk.

  int status = 0;
  fatInternalData *fatData = filesystem->filesystemData;
  
  // Have we already read the parameters for this filesystem?
  if (fatData != NULL)
    return (fatData);

  // We must allocate some new memory to hold information about
  // the filesystem
  fatData = kernelMalloc(sizeof(fatInternalData));
  if (fatData == NULL)
    {
      // Oops.  Something went wrong.
      kernelError(kernel_error, "Unable to allocate FAT data memory");
      return (fatData = NULL);
    }

  // Attach the disk structure to the fatData structure
  fatData->disk = filesystem->disk;

  // Get the disk's boot sector info
  status = readVolumeInfo(fatData);
  if (status < 0)
    {
      // Oops.  Something went wrong.
      kernelError(kernel_error, "Unable to get FAT volume info");
      // Attempt to free the fatData memory
      kernelFree((void *) fatData);
      return (fatData = NULL);
    }

  // Read the entire FAT table into memory
  fatData->FAT = kernelMalloc(fatData->bytesPerSector * fatData->fatSectors);
  if (fatData->FAT == NULL)
    {
      // Oops.  Something went wrong.
      kernelError(kernel_error, "Unable to allocate FAT data memory");
      return (fatData = NULL);
    }

  status = kernelDiskReadSectors((char *) fatData->disk->name, 
				 fatData->reservedSectors,
				 fatData->fatSectors, fatData->FAT);
  if (status < 0)
    {
      // Oops.  Something went wrong.
      kernelError(kernel_error, "Unable to read FAT");
      return (fatData = NULL);
    }

  fatData->freeBitmapSize = (fatData->dataClusters / 8);

  // Get memory for the free list
  fatData->freeClusterBitmap = kernelMalloc(fatData->freeBitmapSize);
  if (fatData->freeClusterBitmap == NULL)
    {
      // Oops.  Something went wrong.
      kernelError(kernel_error, "Unable to allocate FAT data memory");
      // Attempt to free the memory
      kernelFree(fatData->freeClusterBitmap);
      return (fatData = NULL);
    }
  
  // Build the free cluster list.  We need to spawn this function as an
  // independent, non-blocking thread, which must be a child process of
  // the kernel.
  status = kernelMultitaskerSpawnKernelThread(makeFreeBitmap, "building FAT "
					      "free cluster list", 1,
					      (void *) &fatData);
  if (status < 0)
    {
      // Oops.  Something went wrong.
      kernelError(kernel_error, "Unable to make free-cluster bitmap");
      // Attempt to free all the memory
      kernelFree(fatData->freeClusterBitmap);
      kernelFree((void *) fatData);
      return (fatData = NULL);
    }

  // Everything went right.  Looks like we will have a legitimate new
  // bouncing baby FAT filesystem.

  // Attach our new FS data to the filesystem structure
  filesystem->filesystemData = (void *) fatData;

  // Specify the filesystem block size
  filesystem->blockSize = 
    (fatData->bytesPerSector * fatData->sectorsPerCluster);

  return (fatData);
}


static int recursiveClusterChainCheck(kernelFilesystem *filesystem,
				      kernelFileEntry *entry, int repair)
{
  // Recurse through all of the kernelFileEntries attached to this one
  // (if any) and check their cluster chains

  int status = 0;
  int errors = 0;
  fatInternalData *fatData = (fatInternalData *) filesystem->filesystemData;
  kernelFileEntry *subEntry = NULL;
  fatEntryData *entryData = NULL;
  unsigned clusterSize = 0;
  unsigned nominalClusters = 0;
  unsigned allocatedClusters = 0;

  // First, process the current entry

  entryData = (fatEntryData *) entry->fileEntryData;

  if ((entry->size == 0) || (entry->blocks == 0))
    {
      // Make sure that there's no starting cluster
      if (entryData->startCluster == 0)
	// Nothing to do.  Just an empty file.
	return (status = 0);

      // Otherwise, there are clusters allocated to this entry that's
      // supposed to have no size

      if (repair)
	{
	  status = releaseClusterChain(fatData, entryData->startCluster);

	  if (status == 0)
	    {
	      entryData->startCluster = 0;
	      entry->size = 0;
	      entry->blocks = 0;
	    }

	  // Try to write the data clusters out as a file instead?
	}

      kernelTextPrintLine("    entry \"%s\" of size 0 has allocated "
			  "clusters%s",	entry->name,
			  (repair && !status)? " (fixed)" : "");

      // No need to continue processing here, even if the entry is a directory
      // because it's now a NULL file.
      return (status = ERR_BADDATA);
    }

  // In many cases it's okay for a FAT root directory to have no clusters,
  // even if it nominally has a size in the filesystem entry.
  else if (!((entry == filesystem->filesystemRoot) &&
	     (fatData->fsType != fat32)))
    {
      // The file is supposed to have a size.  Make sure that there's a
      // starting cluster.
      if (entryData->startCluster == 0)
	{
	  // No starting cluster, yet the file is claimed to have a size.
      
	  if (repair)
	    {
	      entry->size = 0;
	      entry->blocks = 0;
	    }

	  kernelTextPrintLine("    non-zero length file \"%s\" has no "
			      "clusters%s",
			      entry->name, repair? " (fixed)" : "");

	  return (status = ERR_BADDATA);
	}

      // Now check the cluster chain

      // Calculate the cluster size for this filesystem
      clusterSize = (fatData->bytesPerSector * fatData->sectorsPerCluster);

      // Calculate the number of clusters we would expect this file to have
      if (clusterSize != 0)
	{
	  nominalClusters = (entry->size / clusterSize);
	  if ((entry->size % clusterSize) != 0)
	    nominalClusters += 1;
	}
      else
	{
	  // This volume would appear to be corrupted
	  kernelError(kernel_error, "The FAT volume is corrupt");
	  return (status = ERR_BADDATA);
	}

      // We count the number of clusters used by this file, according to
      // the allocation chain
      status = getNumClusters(fatData, entryData->startCluster,
			      &allocatedClusters);

      if ((status < 0) && (allocatedClusters == 0))
	{
	  // The function couldn't follow the file chain at all.  We can't
	  // or shouldn't probably do anything with this right now.  The only
	  // action would be to truncate the file at zero, but that wouldn't
	  // be very nice.
	  return (status);
	}

      // Now, just reconcile the allocated clusters against the number of
      // expected clusters
      if (allocatedClusters != nominalClusters)
	{
	  // If there was an error following the cluster chain.  The function
	  // should have left the length of the legitimate part of the
	  // cluster chain in allocatedCluters.  We will need to adjust the
	  // file to that length, and set the file size to represent the
	  // maximum length that could fit in those clusters.

	  // There are extra clusters at the end of this file, or there are
	  // not enough clusters to match the claimed size of the file

	  if (repair)
	    {
	      // Adjust the size
	      entry->blocks = allocatedClusters;
	      // Best guess
	      entry->size = (allocatedClusters * clusterSize);
	    }

	  kernelTextPrintLine("    entry \"%s\" nominal cluster count %u.  "
			      "Actual %u.%s", entry->name, nominalClusters,
			      allocatedClusters, (repair? " (fixed)" : ""));

	  return (status = ERR_BADDATA);
	}

      // Finally, just make sure the nominal block size is the same as the
      // one in the entry
      if (entry->blocks != nominalClusters)
	{
	  if (repair)
	    entry->blocks = nominalClusters;

	  // This number is only maintained internally anyway, and is not
	  // written out to disk in this filesystem type, so don't make an
	  // error out of it.
	}
    }

  // Now we can move on to the recursion part

  // If this is a directory, check for entries
  if (entry->type == dirT)
    {
      if (entry->contents == NULL)
	{
	  // This directory has not been read yet.  Read it.
	  status = kernelFilesystemFatReadDir(entry);
	  if (status < 0)
	    return (status);
	}

      subEntry = (kernelFileEntry *) entry->contents;
      
      while (subEntry != NULL)
	{
	  if (strcmp((char *) subEntry->name, ".") &&
	      strcmp((char *) subEntry->name, ".."))
	    {
	      status = recursiveClusterChainCheck(filesystem, subEntry,
						  repair);
	      if (status < 0)
		errors = status;
	    }
	  
	  // Next
	  subEntry = subEntry->nextEntry;
	}

      // If there were errors and we repaired them, we should write
      // out the directory
      if (errors && repair)
	{
	  status = kernelFilesystemFatWriteDir(entry);
	  if (status < 0)
	    kernelError(kernel_warn, "Unable to write repaired directory");
	}

      // Now unbuffer each of the entries in this directory (but not the
      // directory itself, as our caller might not be finished with us
      // yet
      subEntry = (kernelFileEntry *) entry->contents;
      while (subEntry != NULL)
	{
	  kernelFileReleaseEntry(subEntry);
	  subEntry = subEntry->nextEntry;
	}
    }
  
  // Some error from previous processing, some error code
  return (status = (errors? errors : 0));
}


static int checkAllClusters(kernelFilesystem *filesystem,
			    kernelFileEntry *entry, int repair)
{
  // This function is the entry point for the recursive check of the cluster
  // chains.
  int status = 0;
  
  // Okay, work recursively from the entry supplied
  status = recursiveClusterChainCheck(filesystem, entry, repair);

  // If there were errors and we repaired them, return a successful exit
  // value
  return (status = (status && !repair)? status : 0);
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
//  Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


int kernelFilesystemFatInitialize(void)
{
  // Initialize the driver

  initialized = 1;

  // Register our driver
  return (kernelDriverRegister(fatDriver, &defaultFatDriver));
}


int kernelFilesystemFatDetect(const kernelDisk *theDisk)
{
  // This function is used to determine whether the data on a disk structure
  // is using a FAT filesystem.  It uses a simple test or two to determine
  // simply whether this is a FAT volume.  Any data that it gathers is
  // discarded when the call terminates.  It returns 1 for true, 0 for false, 
  // and negative if it encounters an error

  int status = 0;
  unsigned char bootSector[FAT_MAX_SECTORSIZE];
  unsigned temp = 0;

  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Make sure the disk structure isn't NULL
  if (theDisk == NULL)
    {
      kernelError(kernel_error, "NULL disk structure");
      return (status = ERR_NULLPARAMETER);
    }

  // We can start by reading the first sector of the volume
  // (the "boot sector").

  // Call the function that reads the boot sector
  status = readBootSector((kernelDisk *) theDisk, bootSector);
  // Make sure we were successful
  if (status < 0)
    // Couldn't read the boot sector, or it was bad
    return (status);

  // What if we cannot be sure that this is a FAT filesystem?  In the 
  // interest of data integrity, we will decline the invitation to use 
  // this as FAT.  It must pass a few tests here.

  // The word at offset 11, the bytes-per-sector field, may only contain 
  // one of the following values: 512, 1024, 2048 or 4096.  Anything else 
  // is illegal according to MS.  512 is almost always the value found here.
  temp = (unsigned) ((bootSector[12] << 8) + bootSector[11]);
  if ((temp != 512) && (temp != 1024) && (temp != 2048) && (temp != 4096))
    // Not a legal value for FAT
    return (status = 0);

  // Check the media type byte.  There are only a small number of legal
  // values that can occur here (so it's a reasonabe test to determine
  // whether this is a FAT)
  if ((bootSector[21] < (unsigned char) 0xF8) && 
      (bootSector[21] != (unsigned char) 0xF0))
    // Oops, not a legal value for FAT
    return (status = 0);

  // Look for the extended boot block signature.  Byte value.
  if (bootSector[38] == (unsigned char) 0x29)
    {
      // Now we look for the substring "FAT" in the boot sector.
      // If this is really a FAT filesystem, we SHOULD find the substring 
      // "FAT" in the first 3 characters of the fsSignature field
      if (strncmp((bootSector + 0x36), "FAT", 3) != 0)
	// We will say this is not a FAT filesystem.  We might be wrong, 
	// but we can't be sure otherwise.
	return (status = 0);
    }

  // We will accept this as a FAT filesystem.
  return (status = 1);
}


int kernelFilesystemFatFormat(kernelDisk *theDisk, const char *type,
			      const char *label, int longFormat)
{
  // Format the supplied disk as a FAT volume.

  int status = 0;
  kernelPhysicalDisk *physicalDisk = NULL;
  unsigned clearSectors = 0;
  unsigned char *sectorBuff = NULL;
  fatInternalData fatData;
  int count;

  // For calculating cluster sizes, below.
  typedef struct {
    unsigned diskSize;
    unsigned secPerClust;
  } sizeToSecsPerClust;

  sizeToSecsPerClust f16Tab[] = {
    { 32680, 2 },       // Up to 16M, 1K cluster
    { 262144, 4 },      // Up to 128M, 2K cluster
    { 524288, 8 },      // Up to 256M, 4K cluster
    { 1048576, 16 },    // Up to 512M, 8K cluster
    { 2097152, 32 },    // Up to 1G, 16K cluster
    { 4194304, 64 },    // Up to 2G, 32K cluster
    { 0xFFFFFFFF, 64 }  // Above 2G, 32K cluster
  };

  sizeToSecsPerClust f32Tab[] = {
    { 532480, 1 },     // Up to 260M, .5K cluster
    { 16777216, 8 },   // Up to 8G, 4K cluster
    { 33554432, 16 },  // Up to 16G, 8K cluster
    { 67108864, 32 },  // Up to 32G, 16K cluster
    { 0xFFFFFFFF, 64 } // Above 32G, 32K cluster
  };
      
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check params
  if ((theDisk == NULL) || (type == NULL) || (label == NULL))
    return (status = ERR_NULLPARAMETER);

  physicalDisk = theDisk->physical;

  // Only format disk with 512-byte sectors
  if (physicalDisk->sectorSize != 512)
    {
      kernelError(kernel_error, "Cannot format a disk with sector size of "
		  "%u (512 only)", physicalDisk->sectorSize);
      return (status = ERR_INVALID);
    }

  // Clear out our new FAT data structure
  kernelMemClear((void *) &fatData, sizeof(fatInternalData));

  // Set the disk structure
  fatData.disk = theDisk;

  strcpy((char *) fatData.oemName, "Visopsys");
  fatData.sectorsPerTrack = physicalDisk->sectorsPerCylinder;
  fatData.heads = physicalDisk->heads;
  fatData.bytesPerSector = physicalDisk->sectorSize;
  fatData.hiddenSectors = 0;
  fatData.numberOfFats = 2;
  fatData.totalSectors = theDisk->numSectors;
  fatData.sectorsPerCluster = 1;

  if (physicalDisk->fixedRemovable == fixed)
    fatData.mediaType = 0xF8;
  else
    fatData.mediaType = 0xF0;

  if (!strncmp(type, "fat12", 5))
    fatData.fsType = fat12;
  else if (!strncmp(type, "fat16", 5))
    fatData.fsType = fat16;
  else if (!strncmp(type, "fat32", 5))
    fatData.fsType = fat32;
  else if ((physicalDisk->type == floppy) || (fatData.totalSectors < 8400))
    fatData.fsType = fat12;
  else if (fatData.totalSectors < 66600)
    fatData.fsType = fat16;
  else
    fatData.fsType = fat32;

  if ((fatData.fsType == fat12) || (fatData.fsType == fat16))
    {
      // FAT12 or FAT16

      fatData.reservedSectors = 1;
      
      if (fatData.fsType == fat12)
	{
	  // FAT12

	  fatData.rootDirEntries = 224;

	  // Calculate the sectors per cluster.
	  while((theDisk->numSectors / fatData.sectorsPerCluster) >= 4085)
	    fatData.sectorsPerCluster *= 2;

	  fatData.terminalCluster = 0x0FF8;
	  strcpy((char *) fatData.fsSignature, "FAT12   ");
	}
      else
	{
	  // FAT16

	  fatData.rootDirEntries = 512;

	  // Calculate the sectors per cluster based on a Microsoft table
	  for (count = 0; ; count ++)
	    if (f16Tab[count].diskSize >= fatData.totalSectors)
	      {
		fatData.sectorsPerCluster = f16Tab[count].secPerClust;
		break;
	      }

	  fatData.terminalCluster = 0xFFF8;
	  strcpy((char *) fatData.fsSignature, "FAT16   ");
	}
    }
  else
    {
      // FAT32

      fatData.reservedSectors = 32;
      fatData.rootDirEntries = 0;

      // Calculate the sectors per cluster based on a Microsoft table
      for (count = 0; ; count ++)
	if (f32Tab[count].diskSize >= fatData.totalSectors)
	  {
	    fatData.sectorsPerCluster = f32Tab[count].secPerClust;
	    break;
	  }

      fatData.terminalCluster = 0x0FFFFFF8;
      strcpy((char *) fatData.fsSignature, "FAT32   ");
    }

  if (physicalDisk->type == floppy)
    {
      fatData.rootDirSectors = 14;
      fatData.fatSectors = 9;
    }

  else
    {
      fatData.rootDirSectors =
	(((FAT_BYTES_PER_DIR_ENTRY * fatData.rootDirEntries) +
	  (fatData.bytesPerSector - 1)) / fatData.bytesPerSector);

      // Figure out the number of FAT sectors based on a "clever bit of
      // math" provided by MicrosoftTM.
      unsigned tmp1 = (fatData.totalSectors - (fatData.reservedSectors +
					       fatData.rootDirSectors));
      unsigned tmp2 = ((256 * fatData.sectorsPerCluster) +
		       fatData.numberOfFats);
      if (fatData.fsType == fat32)
	tmp2 /= 2;
      fatData.fatSectors = ((tmp1 + (tmp2 - 1)) / tmp2);
    }

  fatData.dataSectors =
    (fatData.totalSectors - (fatData.reservedSectors +
			     (fatData.numberOfFats * fatData.fatSectors) +
			     fatData.rootDirSectors));
  fatData.dataClusters = (fatData.dataSectors / fatData.sectorsPerCluster);

  kernelTextPrint("Type: %s\nTotal Sectors: %u\nBytes Per Sector: "
		  "%u\nSectors Per Cluster: %u\nRoot Directory Sectors: "
		  "%u\nFat Sectors: %u\nData Clusters: %u\n\n",
		  fatData.fsSignature, fatData.totalSectors,
		  fatData.bytesPerSector, fatData.sectorsPerCluster,
		  fatData.rootDirSectors, fatData.fatSectors,
		  fatData.dataClusters);

  fatData.driveNumber =
    ((kernelPhysicalDisk *) theDisk->physical)->deviceNumber;
  if (physicalDisk->fixedRemovable == fixed)
    fatData.driveNumber |= 0x80;

  fatData.bootSignature = 0x29;  // Means volume id, label, etc., valid
  fatData.volumeId = kernelSysTimerRead();
  strncpy((char *) fatData.volumeLabel, (char *) label, 12);

  sectorBuff = kernelMalloc(fatData.bytesPerSector);
  if (sectorBuff == NULL)
    {
      kernelError(kernel_error, "Unable to allocate FAT data memory");
      return (status = ERR_MEMORY);
    }
  
  // How many empty sectors to write?  If we are doing a long format, write
  // every sector.  Otherwise, just the system areas
  if (longFormat)
    clearSectors = fatData.totalSectors;
  else
    clearSectors = (fatData.reservedSectors +
		    (fatData.numberOfFats * fatData.fatSectors) +
		    fatData.rootDirSectors);

  for (count = 0; count < clearSectors; count ++)
    {
      status = kernelDiskWriteSectors((char *) theDisk->name, count, 1,
				      sectorBuff);
      if (status < 0)
	{
	  kernelFree(sectorBuff);
	  return (status);
	}
    }

  // Set first two FAT table entries
  if (fatData.fsType == fat12)
    {
      *((short *) sectorBuff) = (0x0F00 | fatData.mediaType);
      *((short *)(sectorBuff + 1)) |= 0xFFF0;
    }
  else if (fatData.fsType == fat16)
    {
      *((short *) sectorBuff) = (0xFF00 | fatData.mediaType);
      *((short *)(sectorBuff + 2)) = 0xFFFF;
    }
  else if (fatData.fsType == fat32)
    {
      // These fields are specific to the FAT32 filesystem type, and
      // are not applicable to FAT12 or FAT16
      fatData.rootDirClusterF32 = 2;
      fatData.fsInfoSectorF32 = 1;
      fatData.backupBootF32 = 6;
      fatData.freeClusters = (fatData.dataClusters - 1);
      fatData.firstFreeClusterF32 = 3;

      // Write an empty root directory
      for (count = 0; count < fatData.sectorsPerCluster; count ++)
	{
	  status = kernelDiskWriteSectors((char *) theDisk->name,
			  (fatData.reservedSectors + count +
			   (fatData.numberOfFats * fatData.fatSectors)),
					  1, sectorBuff);
	  if (status < 0)
	    {
	      kernelFree(sectorBuff);
	      return (status);
	    }
	}

      *((unsigned *) sectorBuff) = (0x0FFFFF00 | fatData.mediaType);
      *((unsigned *)(sectorBuff + 4)) = 0x0FFFFFFF;
      
      // Mark the root dir cluster as being used in the FAT
      *((unsigned *)(sectorBuff + (fatData.rootDirClusterF32 * 4))) =
	fatData.terminalCluster;
    }

  // Write the first sector of the FATs
  fatData.FAT = sectorBuff;
  fatData.dirtyFatSectList[0] = 0;
  fatData.numDirtyFatSects = 1;

  status = writeDirtyFatSects(&fatData);
  if (status < 0)
    {
      kernelError(kernel_error, "Error writing empty FAT");
      kernelFree(sectorBuff);
      return (status);
    }

  status = flushVolumeInfo(&fatData);
  if (status < 0)
    {
      kernelError(kernel_error, "Error writing volume info");
      kernelFree(sectorBuff);
      return (status);
    }

  // Put a bogus 'jmp' instruction at the beginning of the boot sector,
  // otherwise Windows won't consider it formatted
  {
    status = kernelDiskReadSectors((char *) theDisk->name, 0, 1, sectorBuff);
    if (status < 0)
      {
	kernelError(kernel_error, "Error re-writing boot sector");
	kernelFree(sectorBuff);
	return (status);
      }

    *((short *) sectorBuff) = 0x3CEB;  // JMP inst
    sectorBuff[2] = 0x90;              // No op

    status = kernelDiskWriteSectors((char *) theDisk->name, 0, 1, sectorBuff);
    if (status < 0)
      {
	kernelError(kernel_error, "Error re-writing boot sector");
	kernelFree(sectorBuff);
	return (status);
      }
  }

  kernelFree(sectorBuff);

  if (fatData.fsType == fat32)
    {
      status = flushFSInfo(&fatData);
      if (status < 0)
	{
	  kernelError(kernel_error, "Error writing filesystem info block");
	  return (status);
	}
    }

  status = kernelDiskSyncDisk((char *) theDisk->name);
  return (status);
}


int kernelFilesystemFatCheck(kernelFilesystem *checkFilesystem, int force,
			     int repair)
{
  // This function performs a check of the FAT filesystem structure supplied.
  // Assumptions: the filesystem is REALLY a FAT filesystem, the filesystem
  // is not currently mounted anywhere, and the filesystem driver structure for
  // the filesystem is installed.

  int status = 0;
  int errors = 0;
  fatInternalData *fatData = NULL;
  int mountedForCheck = 0;

  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Make sure the filesystem structure isn't NULL
  if (checkFilesystem == NULL)
    {
      kernelError(kernel_error, "NULL filesystem structure");
      return (status = ERR_NULLPARAMETER);
    }

  kernelTextPrintLine("Checking FAT filesystem...");

  // Make sure there's really a FAT filesystem on the disk
  if (!kernelFilesystemFatDetect(checkFilesystem->disk))
    {
      kernelError(kernel_error, "Disk structure to check does not contain "
		  "a FAT filesystem");
      return (status = ERR_INVALID);
    }

  // Make sure it's not mounted anywhere, if we're going to try to repair
  // things.
  if (checkFilesystem->filesystemRoot && repair)
    {
      kernelError(kernel_warn, "Cannot repair a mounted filesytem");
      repair = 0;
    }

  // Read-only for checking, unless we have been told to repair things
  // automatically.
  checkFilesystem->readOnly = !repair;

  kernelTextPrintLine("  reading filesystem structures");

  if (!checkFilesystem->filesystemRoot)
    {
      // "mount" the filesystem so that its data gets read in.  This is not
      // a real mount in that it is never exposed to the rest of the system,
      status = kernelFilesystemFatMount(checkFilesystem);
      if (status < 0)
	{
	  kernelError(kernel_error, "Unable to check the filesystem");
	  return (status);
	}

      mountedForCheck = 1;
    }

  fatData = checkFilesystem->filesystemData;

  // Wait until the free cluster bitmap has been processed.  Normally this is
  // not done when mounting, in order that the mount operation can terminate
  // more quickly.
  while (fatData->buildingFreeBitmap)
    kernelMultitaskerYield();

  // Recurse through all the files, checking the cluster chains
  kernelTextPrintLine("  checking clusters");
  status = checkAllClusters(checkFilesystem, checkFilesystem->filesystemRoot,
			    repair);
  if (status < 0)
    {
      kernelError(kernel_error, "Cluster chain checking failed");
      errors = status;
    }

  if (mountedForCheck)
    {
      // Release all file entry data that we might have accumulated
      status = kernelFileUnbufferRecursive(checkFilesystem->filesystemRoot);
      if (status < 0)
	kernelError(kernel_warn, "Unable to unbuffer filesystem entries");

      checkFilesystem->filesystemRoot = NULL;

      // "unmount" the filesystem.  Same comment as above.
      kernelFilesystemFatUnmount(checkFilesystem);
    }

  if (!errors)
    kernelTextPrintLine("Done");

  // Return success
  return (status = (errors? errors : 0));
}


int kernelFilesystemFatDefragment(kernelFilesystem *filesystem)
{
  // Defragment the FAT filesystem.

  if (!initialized)
    return (ERR_NOTINITIALIZED);

  return (ERR_NOTIMPLEMENTED);
}


int kernelFilesystemFatMount(kernelFilesystem *filesystem)
{
  // This function initializes the filesystem driver by gathering all
  // of the required information from the boot sector.  In addition, 
  // it dynamically allocates memory space for the "used" and "free"
  // file and directory structure arrays.
  
  int status = 0;
  fatInternalData *fatData = NULL;
  
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Make sure the filesystem isn't NULL
  if (filesystem == NULL)
    {
      kernelError(kernel_error, "NULL filesystem structure");
      return (status = ERR_NULLPARAMETER);
    }

  // FAT filesystems are case preserving, but case insensitive.  Yuck.
  filesystem->caseInsensitive = 1;

  // The filesystem data cannot exist
  filesystem->filesystemData = NULL;

  // Get the FAT data for the requested filesystem.  We don't need
  // the info right now -- we just want to collect it.
  fatData = getFatData(filesystem);
  if (fatData == NULL)
    return (status = ERR_BADDATA);

  // Read the disk's root directory and attach it to the filesystem structure
  status = readRootDir(fatData, filesystem);
  if (status < 0)
    {
      // Oops.  Something went wrong.
      kernelError(kernel_error, "Unable to read the filesystem's root "
		  "directory");
      return (status = ERR_BADDATA);
    }

  // Set the proper filesystem type name on the disk structure
  switch (fatData->fsType)
    {
    case fat12:
      strcpy((char *) filesystem->disk->fsType, "fat12");
      break;
    case fat16:
      strcpy((char *) filesystem->disk->fsType, "fat16");
      break;
    case fat32:
      strcpy((char *) filesystem->disk->fsType, "fat32");
      break;
    default:
      strcpy((char *) filesystem->disk->fsType, "fat");
    }

  /*
  // Mark the filesystem as 'dirty'
  unsigned tmp;
  getFatEntry(fatData, 1, &tmp);
  if (((fatData->fsType == fat12) && (tmp & 0x0800)) ||
      ((fatData->fsType == fat16) && (tmp & 0x8000)) ||
      ((fatData->fsType == fat32) && (tmp & 0x08000000)))
    kernelLog("\"%s\" filesystem was not unmounted cleanly",
	      filesystem->mountPoint);
  if (fatData->fsType == fat12)
    changeFatEntry(fatData, 1, (tmp | 0x0800));
  else if (fatData->fsType == fat16)
    changeFatEntry(fatData, 1, (tmp | 0x8000));
  else if (fatData->fsType == fat32)
    changeFatEntry(fatData, 1, (tmp | 0x08000000));
  */

  // Return success
  return (status = 0);
}


int kernelFilesystemFatUnmount(kernelFilesystem *filesystem)
{
  // This function releases all of the stored information about a given
  // filesystem.

  int status = 0;
  fatInternalData *fatData = NULL;

  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check the filesystem pointer before proceeding
  if (filesystem == NULL)
    {
      kernelError(kernel_error, "NULL filesystem structure");
      return (status = ERR_NULLPARAMETER);
    }

  // Get the FAT data for the requested filesystem
  fatData = getFatData(filesystem);
  // Make sure there wasn't a problem
  if (fatData == NULL)
    return (status = ERR_BADDATA);

  // Never unmount while our makeFreeBitmap thread is still processing
  while (fatData->buildingFreeBitmap)
    kernelMultitaskerYield();

  if (!filesystem->readOnly)
    {
      /*
      // Mark the filesystem as 'clean'
      unsigned tmp;
      getFatEntry(fatData, 1, &tmp);
      if (fatData->fsType == fat12)
	changeFatEntry(fatData, 1, (tmp & 0xF7FF));
      else if (fatData->fsType == fat16)
	changeFatEntry(fatData, 1, (tmp & 0x7FFF));
      else if (fatData->fsType == fat32)
	changeFatEntry(fatData, 1, (tmp & 0xF7FFFFFF));
      */

      // Write out any 'dirty' FAT sectors
      status = writeDirtyFatSects(fatData);
      if (status < 0)
	return (status);

      // If this is a FAT32 filesystem, we need to flush the extended 
      // filesystem data back to the FSInfo block
      if (fatData->fsType == fat32)
	{
	  status = flushFSInfo(fatData);
	  if (status < 0)
	    {
	      kernelError(kernel_error ,"Error flushing FSInfo data block");
	      return (status);
	    }
	}
    }

  // Everything should be cozily tucked away now.  We can safely
  // discard the information we have cached about this filesystem.

  status = kernelFree((void *) fatData);
  if (status < 0)
    // Crap.  We couldn't deallocate the memory.  Make a warning.
    kernelError(kernel_warn, "Error deallocating FAT filesystem data");
  
  // Finally, remove the reference from the filesystem structure
  filesystem->filesystemData = NULL;

  return (status = 0);
}


unsigned kernelFilesystemFatGetFreeBytes(kernelFilesystem *filesystem)
{
  // This function returns the amount of free disk space, in bytes.

  fatInternalData *fatData = NULL;

  if (!initialized)
    return (0);

  // Check the filsystem pointer before proceeding
  if (filesystem == NULL)
    {
      kernelError(kernel_error, "NULL filesystem structure");
      return (0);
    }

  // Get the FAT data for the requested filesystem
  fatData = getFatData(filesystem);
  // Make sure there wasn't a problem
  if (fatData == NULL)
    return (0);

  // OK, now we can return the requested info
  return ((unsigned)(fatData->bytesPerSector * 
		     fatData->sectorsPerCluster) * fatData->freeClusters);
}


int kernelFilesystemFatNewEntry(kernelFileEntry *newEntry)
{
  // This function gets called when there's a new kernelFileEntry in the
  // filesystem (either because a file was created or because some existing
  // thing has been newly read from disk).  This gives us an opportunity
  // to attach FAT-specific data to the file entry

  int status = 0;
  fatEntryData *entryData = NULL;
  fatEntryData *newEntryDatas = NULL;
  int count;

  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Make sure the file entry pointer isn't NULL
  if (newEntry == NULL)
    {
      kernelError(kernel_error, "NULL file entry");
      return (status = ERR_NULLPARAMETER);
    }

  // Make sure there isn't already some sort of data attached to this
  // file entry
  if (newEntry->fileEntryData != NULL)
    {
      kernelError(kernel_error, "Entry already has private filesystem data");
      return (status = ERR_ALREADY);
    }

  // Make sure there is a free entry data available
  if (numFreeEntryDatas == 0)
    {
      // Allocate memory for file entries
      newEntryDatas = kernelMalloc(sizeof(fatEntryData) * MAX_BUFFERED_FILES);
      if (newEntryDatas == NULL)
	{
	  kernelError(kernel_error, "Error allocating memory for FAT private "
		      "data lists");
	  return (status = ERR_MEMORY);
	}

      // Initialize the new fatEntryData structures.
      for (count = 0; count < (MAX_BUFFERED_FILES - 1); count ++)
	newEntryDatas[count].next = (void *) &(newEntryDatas[count + 1]);

      // The free file entries are the new memory
      freeEntryDatas = newEntryDatas;

      // Add the number of new file entries
      numFreeEntryDatas = MAX_BUFFERED_FILES;
    }

  // Get a private data structure for FAT-specific information.  Grab it from
  // the first spot
  entryData = freeEntryDatas;
  freeEntryDatas = (fatEntryData *) entryData->next;
  numFreeEntryDatas -= 1;
  newEntry->fileEntryData = (void *) entryData;

  // Return success
  return (status = 0);
}


int kernelFilesystemFatInactiveEntry(kernelFileEntry *inactiveEntry)
{
  // This function gets called when a kernelFileEntry is about to be
  // deallocated by the system (either because a file was deleted or because
  // the entry is simply being unbuffered).  This gives us an opportunity
  // to deallocate our FAT-specific data from the file entry

  int status = 0;
  fatEntryData *entryData = NULL;

  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Make sure the file entry pointer isn't NULL
  if (inactiveEntry == NULL)
    {
      kernelError(kernel_error, "NULL file entry");
      return (status = ERR_NULLPARAMETER);
    }

  entryData = (fatEntryData *) inactiveEntry->fileEntryData;
  if (entryData == NULL)
    {
      kernelError(kernel_error, "File entry has no private filesystem data");
      return (status = ERR_ALREADY);
    }

  // Erase all of the data in this entry
  kernelMemClear((void *) entryData, sizeof(fatEntryData));

  // Release the entry data structure attached to this file entry.  Put the
  // entry data back into the pool of free ones.
  entryData->next = (void *) freeEntryDatas;
  freeEntryDatas = entryData;
  numFreeEntryDatas += 1;

  // Remove the reference
  inactiveEntry->fileEntryData = NULL;

  // Return success
  return (status = 0);
}


int kernelFilesystemFatResolveLink(kernelFileEntry *theFile)
{
  // This gets called to resolve any unresolved links, but that should
  // not happen in this type of filesystem.

  if (!initialized)
    return (ERR_NOTINITIALIZED);

  // All links should already have been resolved.
  return (ERR_NOSUCHFILE);
}


int kernelFilesystemFatReadFile(kernelFileEntry *theFile, unsigned blockNum,
				unsigned blocks, unsigned char *buffer)
{
  // This function is the "read file" function that the filesystem
  // driver exports to the world.  It is mainly a wrapper for the
  // internal function of the same purpose, but with some additional
  // argument checking.  Returns 0 on success, negative otherwise.

  int status = 0;
  kernelFilesystem *filesystem = NULL;
  fatInternalData *fatData = NULL;
  
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Make sure the file entry and buffer aren't NULL
  if ((theFile == NULL) || (buffer == NULL))
    {
      kernelError(kernel_error, "NULL file entry or buffer");
      return (status = ERR_NULLPARAMETER);
    }

  // Make sure the requested read size is more than 0 blocks
  if (!blocks)
    // Don't return an error, just finish
    return (status = 0);

  // Make sure that there's a private FAT data structure attached to this
  // file entry
  if (theFile->fileEntryData == NULL)
    {
      kernelError(kernel_error, "File entry has no private filesystem data");
      return (status = ERR_NODATA);
    }

  // Get the filesystem pointer
  filesystem = (kernelFilesystem *) theFile->filesystem;
  if (filesystem == NULL)
    {
      kernelError(kernel_error, "NULL filesystem structure");
      return (status = ERR_BADADDRESS);
    }

  // Get the FAT data for the requested filesystem
  fatData = getFatData(filesystem);
  // Make sure there wasn't a problem
  if (fatData == NULL)
    return (status = ERR_BADDATA);
  
  // Make sure it's really a file, and not a directory
  if (theFile->type != fileT)
    return (status = ERR_NOTAFILE);

  // Make sure the file is not corrupted
  status = checkFileChain(fatData, theFile);
  if (status < 0)
    return (status);

  // Ok, now we will call the internal function to read the file
  status = read(fatData, theFile, blockNum, blocks, buffer);

  return (status);
}


int kernelFilesystemFatWriteFile(kernelFileEntry *theFile, unsigned blockNum, 
				 unsigned blocks, unsigned char *buffer)
{
  // This function is the "write file" function that the filesystem
  // driver exports to the world.  It is mainly a wrapper for the
  // internal function of the same purpose, but with some additional
  // argument checking.  Returns 0 on success, negative otherwise.

  int status = 0;
  kernelFilesystem *filesystem = NULL;
  fatInternalData *fatData = NULL;
  
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Make sure the file entry and buffer aren't NULL
  if ((theFile == NULL) || (buffer == NULL))
    {
      kernelError(kernel_error, "NULL file entry or buffer");
      return (status = ERR_NULLPARAMETER);
    }

  // Make sure the requested write size is more than 0 blocks
  if (!blocks)
    // Don't return an error, just finish
    return (status = 0);

  // Make sure that there's a private FAT data structure attached to this
  // file entry
  if (theFile->fileEntryData == NULL)
    {
      kernelError(kernel_error, "File entry has no private filesystem data");
      return (status = ERR_NODATA);
    }

  // Get the filesystem pointer
  filesystem = (kernelFilesystem *) theFile->filesystem;
  if (filesystem == NULL)
    {
      kernelError(kernel_error, "NULL filesystem structure");
      return (status = ERR_BADADDRESS);
    }

  // Get the FAT data for the requested filesystem
  fatData = getFatData(filesystem);
  // Make sure there wasn't a problem
  if (fatData == NULL)
    return (status = ERR_BADDATA);

  // Make sure it's really a file, and not a directory
  if (theFile->type != fileT)
    return (status = ERR_NOTAFILE);

  // Ok, now we will call the internal function to read the file
  status = write(fatData, theFile, blockNum, blocks, buffer);
  if (status == ERR_NOWRITE)
    {
      kernelError(kernel_warn, "File system is read-only");
      filesystem->readOnly = 1;
    }
  
  return (status);
}


int kernelFilesystemFatCreateFile(kernelFileEntry *theFile)
{
  // This function does the FAT-specific initialization of a new file.
  // There's not much more to this than getting a new entry data structure
  // and attaching it.  Returns 0 on success, negative otherwise

  int status = 0;

  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Check params
  if (theFile == NULL)
    return (status = ERR_NULLPARAMETER);
      
  // Make sure that there's a private FAT data structure attached to this
  // file entry
  if (theFile->fileEntryData == NULL)
    {
      kernelError(kernel_error, "File entry has no private filesystem data");
      return (status = ERR_NODATA);
    }

  // Make sure the file's name is legal for FAT
  status = checkFilename(theFile->name);
  if (status < 0)
    {
      kernelError(kernel_error, "File name is illegal in FAT filesystems");
      return (status);
    }

  // Install the short alias for this file.  This is directory-dependent
  // because it assigns short names based on how many files in the
  // directory share common characters in the initial part of the filename.
  // Don't do it for '.' or '..' entries, however
  if (strcmp((char *) theFile->name, ".") && 
      strcmp((char *) theFile->name, ".."))
    {
      status = makeShortAlias(theFile);
      if (status < 0)
	return (status);
    }

  // Return success
  return (status = 0);
}


int kernelFilesystemFatDeleteFile(kernelFileEntry *theFile, int secure)
{
  // This function deletes a file.  It returns 0 on success, negative
  // otherwise

  int status = 0;
  kernelFilesystem *filesystem = NULL;
  fatInternalData *fatData = NULL;
  fatEntryData *entryData = NULL;

  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Make sure the file entry pointer isn't NULL
  if (theFile == NULL)
    {
      kernelError(kernel_error, "NULL file entry");
      return (status = ERR_NULLPARAMETER);
    }

  // Get the filesystem pointer
  filesystem = (kernelFilesystem *) theFile->filesystem;
  if (filesystem == NULL)
    {
      kernelError(kernel_error, "NULL filesystem structure");
      return (status = ERR_BADADDRESS);
    }

  // Get the FAT data for the requested filesystem
  fatData = getFatData(filesystem);
  // Make sure there wasn't a problem
  if (fatData == NULL)
    return (status = ERR_BADDATA);

  // Get the private FAT data structure attached to this file entry
  entryData = (fatEntryData *) theFile->fileEntryData;
  if (entryData == NULL)
    {
      kernelError(kernel_error, "File has no private filesystem data");
      return (status = ERR_NODATA);
    }

  // Make sure the chain of clusters is not corrupt
  status = checkFileChain(fatData, theFile);
  if (status)
    {
      kernelError(kernel_error, "File to delete appears to be corrupt");
      return (status);
    }

  // If we are doing a 'secure' delete, we need to zero out all of the data
  // in the file's clusters
  if (secure)
    {
      status = clearClusterChainData(fatData, entryData->startCluster);
      if (status < 0)
	// Ahh, we don't need to quit here, do we?
	kernelError(kernel_warn, "File data could not be cleared");
    }

  // Deallocate all clusters belonging to the item.
  status = releaseEntryClusters(fatData, theFile);
  if (status < 0)
    {
      kernelError(kernel_error, "Error deallocating file clusters");
      return (status);
    }

  // Return success
  return (status = 0);
}


int kernelFilesystemFatFileMoved(kernelFileEntry *entry)
{
  // This function is called by the filesystem manager whenever a file
  // has been moved from one place to another.  This allows us the chance
  // do to any FAT-specific things to the file that are necessary.  In our
  // case, we need to re-create the file's short alias, since this is
  // directory-dependent.

  int status = 0;

  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Make sure the file entry isn't NULL
  if (entry == NULL)
    {
      kernelError(kernel_error, "NULL file entry");
      return (status = ERR_NULLPARAMETER);
    }

  // Make sure that there's a private FAT data structure attached to this
  // file entry
  if (entry->fileEntryData == NULL)
    {
      kernelError(kernel_error, "File has no private filesystem data");
      return (status = ERR_NODATA);
    }

  // Generate a new short alias for the moved file
  status = makeShortAlias(entry);
  if (status < 0)
    {
      kernelError(kernel_error, "Unable to generate new short filename alias");
      return (status);
    }

  // Return success
  return (status = 0);
}


int kernelFilesystemFatReadDir(kernelFileEntry *directory)
{
  // This function receives an emtpy file entry structure, which represents
  // a directory whose contents have not yet been read.  This will fill the
  // directory structure with its appropriate contents.  Returns 0 on
  // success, negative otherwise.

  int status = 0;
  kernelFilesystem *filesystem = NULL;
  fatInternalData *fatData = NULL;
  fatEntryData *entryData = NULL;
  unsigned char *dirBuffer = NULL;
  unsigned dirBufferSize = 0;

  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Make sure the file entry isn't NULL
  if (directory == NULL)
    {
      kernelError(kernel_error, "NULL directory entry");
      return (status = ERR_NULLPARAMETER);
    }

  // Make sure that there's a private FAT data structure attached to this
  // file entry
  if (directory->fileEntryData == NULL)
    {
      kernelError(kernel_error, "File entry has no private filesystem data");
      return (status = ERR_NODATA);
    }

  // Get the filesystem pointer
  filesystem = (kernelFilesystem *) directory->filesystem;
  if (filesystem == NULL)
    {
      kernelError(kernel_error, "NULL filesystem structure");
      return (status = ERR_BADADDRESS);
    }

  // Get the FAT data for the requested filesystem
  fatData = getFatData(filesystem);
  // Make sure there wasn't a problem
  if (fatData == NULL)
    return (status = ERR_BADDATA);
  
  // Make sure it's really a directory, and not a regular file
  if (directory->type != dirT)
    return (status = ERR_NOTADIR);

  // Get the directory entry's data
  entryData = (fatEntryData *) directory->fileEntryData;

  // Now we can go about scanning the directory.

  dirBufferSize = (directory->blocks * fatData->sectorsPerCluster
		   * fatData->bytesPerSector);

  dirBuffer = kernelMalloc(dirBufferSize);
  if (dirBuffer == NULL)
    {
      kernelError(kernel_error, "Memory allocation error");
      return (status = ERR_MEMORY);
    }
  
  // Now we read all of the sectors of the directory
  status = read(fatData, directory, 0, directory->blocks, dirBuffer);
  if (status < 0)
    {
      kernelError(kernel_error, "Error reading directory");
      kernelFree(dirBuffer);
      return (status);
    }
  
  // Call the routine to interpret the directory data
  status = scanDirectory(fatData, filesystem, directory, dirBuffer, 
			 dirBufferSize);

  // Free the directory buffer we used
  kernelFree(dirBuffer);

  if (status < 0)
    {
      kernelError(kernel_error, "Error parsing directory");
      return (status);
    }
	      
  // Return success
  return (status = 0);
}


int kernelFilesystemFatWriteDir(kernelFileEntry *directory)
{
  // This function takes a directory entry structure and updates it 
  // appropriately on the disk volume.  On success it returns zero,
  // negative otherwise.

  int status = 0;
  kernelFilesystem *filesystem = NULL;
  fatInternalData *fatData = NULL;
  fatEntryData *entryData = NULL;
  unsigned clusterSize = 0;
  unsigned char *dirBuffer = NULL;
  unsigned dirBufferSize = 0;
  unsigned directoryEntries = 0;
  unsigned blocks = 0;

  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Make sure the directory entry isn't NULL
  if (directory == NULL)
    {
      kernelError(kernel_error, "NULL directory entry");
      return (status = ERR_NULLPARAMETER);
    }

  // Get the filesystem pointer
  filesystem = (kernelFilesystem *) directory->filesystem;
  if (filesystem == NULL)
    {
      kernelError(kernel_error, "NULL filesystem structure");
      return (status = ERR_BADADDRESS);
    }

  // Get the private FAT data structure attached to this file entry
  entryData = (fatEntryData *) directory->fileEntryData;
  if (entryData == NULL)
    {
      kernelError(kernel_error, "NULL private file data");
      return (status = ERR_NODATA);
    }

  // Get the FAT data for the requested filesystem
  fatData = getFatData(filesystem);
  // Make sure there wasn't a problem
  if (fatData == NULL)
    {
      kernelError(kernel_error, "Unable to find FAT filesystem data");
      return (status = ERR_BADDATA);
    }
  
  // Make sure it's really a directory, and not a regular file
  if (directory->type != dirT)
    {
      kernelError(kernel_error, "Directory to write is not a directory");
      return (status = ERR_NOTADIR);
    }

  // Figure out the size of the buffer we need to allocate to hold the
  // directory
  if ((fatData->fsType != fat32) &&
      (directory == (kernelFileEntry *) filesystem->filesystemRoot))
    {
      dirBufferSize = (fatData->rootDirSectors * fatData->bytesPerSector);
      blocks = fatData->rootDirSectors;
    }

  else
    {
      // Figure out how many directory entries there are
      directoryEntries = dirRequiredEntries(directory);

      // Calculate the size of a cluster in this filesystem
      clusterSize = (fatData->bytesPerSector * fatData->sectorsPerCluster);
  
      if (clusterSize == 0)
	{
	  // This volume would appear to be corrupted
	  kernelError(kernel_error, "The FAT volume is corrupt");
	  return (status = ERR_BADDATA);
	}

      dirBufferSize = (directoryEntries * FAT_BYTES_PER_DIR_ENTRY);
      if (dirBufferSize % clusterSize)
	dirBufferSize += (clusterSize - (dirBufferSize % clusterSize));

      // Calculate the new number of blocks that will be occupied by
      // this directory
      blocks = (dirBufferSize / 
		(fatData->sectorsPerCluster * fatData->bytesPerSector));
      if (dirBufferSize % 
	  (fatData->sectorsPerCluster * fatData->bytesPerSector))
	blocks += 1;

      // If the new number of blocks is less than the previous value, we
      // should deallocate all of the extra clusters at the end

      if (blocks < directory->blocks)
	{
	  status = shortenFile(fatData, directory, blocks);
	  if (status < 0)
	    // Not fatal.  Just warn
	    kernelError(kernel_warn, "Unable to shorten directory");
	}
    }

  // Allocate temporary space for the directory buffer
  dirBuffer = kernelMalloc(dirBufferSize);
  if (dirBuffer == NULL)
    {
      kernelError(kernel_error, "Memory allocation error writing directory");
      return (status = ERR_MEMORY);
    }

  // Fill in the directory entries
  status = fillDirectory(directory, dirBufferSize, dirBuffer);
  if (status < 0)
    {
      kernelError(kernel_error, "Error filling directory structure");
      kernelFree(dirBuffer);
      return (status);
    }

  // Write the directory "file".  If it's the root dir of a non-FAT32
  // filesystem we do a special version of this write.
  if ((fatData->fsType != fat32) &&
      (directory == (kernelFileEntry *) filesystem->filesystemRoot))
    status = 
      kernelDiskWriteSectors((char *) fatData->disk->name, 
			     (fatData->reservedSectors +
			      (fatData->fatSectors * fatData->numberOfFats)),
			     blocks, dirBuffer);
  else
    status = write(fatData, directory, 0, blocks, dirBuffer);

  // De-allocate the directory buffer
  kernelFree(dirBuffer);

  if (status == ERR_NOWRITE)
    {
      kernelError(kernel_warn, "File system is read-only");
      filesystem->readOnly = 1;
    }
  else if (status < 0)
    kernelError(kernel_error, "Error writing directory \"%s\"",
		directory->name);

  return (status);
}


int kernelFilesystemFatMakeDir(kernelFileEntry *directory)
{
  // This function is used to create a directory on disk.  The caller will
  // create the file entry data structures, and it is simply the
  // responsibility of this function to make the on-disk structures reflect
  // the new entry.  It returns 0 on success, negative otherwise.

  int status = 0;
  kernelFilesystem *filesystem = NULL;
  fatInternalData *fatData = NULL;
  fatEntryData *dirData = NULL;
  unsigned newCluster = 0;

  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Make sure the new directory isn't NULL
  if (directory == NULL)
    {
      kernelError(kernel_error, "NULL directory entry");
      return (status = ERR_NULLPARAMETER);
    }

  // Make sure that there's a private FAT data structure attached to this
  // file entry
  if (directory->fileEntryData == NULL)
    {
      kernelError(kernel_error, "File entry has no private filesystem data");
      return (status = ERR_NODATA);
    }

  // Get the filesystem pointer
  filesystem = (kernelFilesystem *) directory->filesystem;
  if (filesystem == NULL)
    {
      kernelError(kernel_error, "NULL filesystem structure");
      return (status = ERR_BADADDRESS);
    }

  // Get the FAT data for the requested filesystem
  fatData = getFatData(filesystem);
  // Make sure there wasn't a problem
  if (fatData == NULL)
    return (status = ERR_BADDATA);

  // Make sure the file name is legal for FAT
  status = checkFilename(directory->name);
  if (status < 0)
    {
      kernelError(kernel_error, "File name is illegal in FAT filesystems");
      return (status);
    }

  // Allocate a new, single cluster for this new directory
  status = getUnusedClusters(fatData, 1, &newCluster);
  if (status < 0)
    {
      kernelError(kernel_error, "No more free clusters");
      return (status);
    }

  // Set the size on the new directory entry
  directory->blocks = 1;
  directory->size = (fatData->sectorsPerCluster * fatData->bytesPerSector);

  // Set all the appropriate attributes in the directory's private data
  dirData = directory->fileEntryData;
  dirData->attributes = (FAT_ATTRIB_ARCHIVE | FAT_ATTRIB_SUBDIR);
  dirData->res = 0;
  dirData->timeTenth = 0;
  dirData->startCluster = newCluster;

  // Make the short alias
  status = makeShortAlias(directory);
  if (status < 0)
    return (status);

  return (status = 0);
}


int kernelFilesystemFatRemoveDir(kernelFileEntry *directory)
{
  // This function deletes a directory, but only if it is empty.  
  // It returns 0 on success, negative otherwise

  int status = 0;
  kernelFilesystem *filesystem = NULL;
  fatInternalData *fatData = NULL;
  fatEntryData *entryData = NULL;

  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Make sure the file entry pointer isn't NULL
  if (directory == NULL)
    {
      kernelError(kernel_error, "NULL directory");
      return (status = ERR_NULLPARAMETER);
    }

  // Get the filesystem pointer
  filesystem = (kernelFilesystem *) directory->filesystem;
  if (filesystem == NULL)
    {
      kernelError(kernel_error, "NULL filesystem structure");
      return (status = ERR_BADADDRESS);
    }

  // Get the FAT data for the requested filesystem
  fatData = getFatData(filesystem);
  // Make sure there wasn't a problem
  if (fatData == NULL)
    return (status = ERR_BADDATA);

  // Get the private FAT data structure attached to this file entry
  entryData = (fatEntryData *) directory->fileEntryData;
  if (entryData == NULL)
    {
      kernelError(kernel_error, "Directory has no private filesystem data");
      return (status = ERR_NODATA);
    }

  // Make sure the chain of clusters is not corrupt
  status = checkFileChain(fatData, directory);
  if (status)
    {
      kernelError(kernel_error, "Directory to delete appears to be corrupt");
      return (status);
    }

  // Deallocate all of the clusters belonging to this directory.
  status = releaseEntryClusters(fatData, directory);
  if (status < 0)
    {
      kernelError(kernel_error, "Error deallocating directory clusters");
      return (status);
    }

  // Return success
  return (status = 0);
}


int kernelFilesystemFatTimestamp(kernelFileEntry *theFile)
{
  // This function does FAT-specific stuff for time stamping a file.

  int status = 0;
  kernelFilesystem *filesystem = NULL;
  fatInternalData *fatData = NULL;
  fatEntryData *entryData = NULL;

  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Make sure the file entry pointer isn't NULL
  if (theFile == NULL)
    {
      kernelError(kernel_error, "NULL file entry");
      return (status = ERR_NULLPARAMETER);
    }

  // Make sure that there's a private FAT data structure attached to this
  // file entry
  if (theFile->fileEntryData == NULL)
    {
      kernelError(kernel_error, "File entry has no private filesystem data");
      return (status = ERR_NODATA);
    }

  // Get the filesystem pointer
  filesystem = (kernelFilesystem *) theFile->filesystem;
  if (filesystem == NULL)
    {
      kernelError(kernel_error, "NULL filesystem structure");
      return (status = ERR_BADADDRESS);
    }

  // Get the FAT data for the requested filesystem
  fatData = getFatData(filesystem);
  // Make sure there wasn't a problem
  if (fatData == NULL)
    return (status = ERR_BADDATA);

  // Get the entry's data
  entryData = (fatEntryData *) theFile->fileEntryData;

  // The only FAT-specific thing we're doing here is setting the 
  // 'archive' bit.
  entryData->attributes = (entryData->attributes | FAT_ATTRIB_ARCHIVE);

  return (status = 0);
}
