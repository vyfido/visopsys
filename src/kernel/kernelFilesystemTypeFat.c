//
//  Visopsys
//  Copyright (C) 1998-2001 J. Andrew McLaughlin
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
//  kernelFilesystemTypeFat.c
//

// This file contains the routines designed to interpret the FAT
// filesystem (commonly found on DOS (TM) disks)

#include "kernelFilesystemTypeFat.h"
#include "kernelFile.h"
#include "kernelResourceManager.h"
#include "kernelMemoryManager.h"
#include "kernelMultitasker.h"
#include "kernelSysTimerFunctions.h"
#include "kernelText.h"
#include "kernelMiscAsmFunctions.h"
#include "kernelLog.h"
#include "kernelError.h"
#include "kernelDebug.h"
#include <string.h>
#include <ctype.h>
#include <sys/errors.h>


// These represent a pool of memory for holding private data for each
// FAT file we manage. 
static fatEntryData *entryDataMemory = NULL;
static fatEntryData *entryDatas[MAX_BUFFERED_FILES];
static int usedEntryDatas = 0;


static int readBootSector(kernelDiskObject *theDisk, 
			  unsigned char *buffer)
{
  // This simple function will read a disk object's boot sector into
  // the requested buffer and ensure that it is (at least trivially) 
  // valid.  Returns 0 on success, negative on error.

  int status = 0;


  kernelDebugEnter();

  
  // Make sure that neither of the pointers we were passed are NULL
  if ((theDisk == NULL) || (buffer == NULL))
    return (status = ERR_NULLPARAMETER);

  // Initialize the buffer we were given
  kernelMemClear(buffer, FAT_MAX_SECTORSIZE);

  // Obtain a lock on the disk device
  status = kernelDiskFunctionsLockDisk(theDisk->diskNumber);

  if (status < 0)
    {
      // We couldn't lock the disk object
      kernelError(kernel_error, "Couldn't lock disk object");
      return (status);
    }

  // Read the boot sector
  status = kernelDiskFunctionsReadSectors(theDisk->diskNumber, 
		  0, 1, buffer);

  // We can now release our lock on the disk object
  kernelDiskFunctionsUnlockDisk(theDisk->diskNumber);

  // Make sure that the read was successful
  if (status < 0)
    {
      // Couldn't read the boot sector.  Make an error
      kernelError(kernel_error, FAT_NO_BOOTSECTOR);
      return (status);
    }

  // It MUST be true that the signature word 0xAA55 occurs at offset
  // 510 of the boot sector (regardless of the sector size of this device).  
  // If it does not, then this is not only NOT a FAT boot sector, but 
  // may not be a valid boot sector at all.
  if ((buffer[510] != (unsigned char) 0x55) || 
      (buffer[511] != (unsigned char) 0xAA))
    {
      kernelError(kernel_error, "Not a valid boot sector");
      return (status = ERR_BADDATA);
    }

  // Return success
  return (status = 0);
}


static int readFSInfo(kernelDiskObject *theDisk, 
			  unsigned int sectorNumber, unsigned char *buffer)
{
  // This simple function will read a disk object's fsInfo sector into
  // the requested buffer and ensure that it is (at least trivially) 
  // valid.  Returns 0 on success, negative on error.

  int status = 0;


  kernelDebugEnter();

  // Make sure that neither of the pointers we were passed are NULL
  if ((theDisk == NULL) || (buffer == NULL))
    return (status = ERR_NULLPARAMETER);

  // Initialize the buffer we were given
  kernelMemClear(buffer, FAT_MAX_SECTORSIZE);

  // Obtain a lock on the disk device
  status = kernelDiskFunctionsLockDisk(theDisk->diskNumber);

  if (status < 0)
    {
      // We couldn't lock the disk object
      kernelError(kernel_error, "Couldn't lock disk object");
      return (status);
    }

  // Read the FSInfo sector (read 1 sector starting at the logical sector
  // number we were given
  status = kernelDiskFunctionsReadSectors(theDisk->diskNumber, 
				  sectorNumber, 1, buffer);

  // We can now release our lock on the disk object
  kernelDiskFunctionsUnlockDisk(theDisk->diskNumber);
  
  // Make sure that the read was successful
  if (status < 0)
    {
      // Couldn't read the FSInfo sector.  Make an error
      kernelError(kernel_error, FAT_NO_FSINFOSECTOR);
      return (status);
    }

  // It MUST be true that the signature dword 0xAA550000 occurs at 
  // offset 0x1FC of the FSInfo sector (regardless of the sector size 
  // of this device).  If it does not, then this is not a valid 
  // FSInfo sector.  It must also be true that we find two 
  // signature dwords in the sector: 0x41615252 at offset 0 and 
  // 0x61417272 at offset 0x1E4.
  if (((buffer[0x01FF] != (unsigned char) 0xAA) || 
       (buffer[0x01FE] != (unsigned char) 0x55) || 
       (buffer[0x01FD] != (unsigned char) 0x00) || 
       (buffer[0x01FC] != (unsigned char) 0x00)) ||
      ((buffer[0x3] != (unsigned char) 0x41) || 
       (buffer[0x2] != (unsigned char) 0x61) || 
       (buffer[0x1] != (unsigned char) 0x52) || 
       (buffer[0x0] != (unsigned char) 0x52)) ||
      ((buffer[0x01E7] != (unsigned char) 0x61) || 
       (buffer[0x01E6] != (unsigned char) 0x41) || 
       (buffer[0x01E5] != (unsigned char) 0x72) || 
       (buffer[0x01E4] != (unsigned char) 0x72)))
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
  // the freeClusterCountF32 and firstFreeClusterF32 values.  This function
  // will write the current values of these fields back to the FSInfo
  // block.

  int status = 0;
  unsigned char fsInfoBlock[FAT_MAX_SECTORSIZE];


  kernelDebugEnter();

  // Make sure the fsInfoSectorF32 value is still reasonable.  It must
  // come after the boot sector and be within the reserved area of the
  // volume.  Otherwise, we will assume there's an inconsistency somewhere.
  if ((fatData->fsInfoSectorF32 < 1) || 
      (fatData->fsInfoSectorF32 > fatData->reservedSectors))
    return (status = ERR_BADDATA);
  
  // Now we can read in the current FSInfo block (so that we can modify
  // select values before writing it back).  Call the function that will 
  // read the FSInfo block
  status = readFSInfo((kernelDiskObject *) fatData->diskObject, 
		      (unsigned int) fatData->fsInfoSectorF32, fsInfoBlock);

  // Make sure we were successful
  if (status < 0)
    // Couldn't read the boot FSInfo block, or it was bad
    return (status);

  // Set the FAT32 "free cluster count".  Doubleword value.
  fsInfoBlock[0x01EB] = (unsigned char) 
    ((fatData->freeClusterCountF32 & 0xFF000000) >> 24);
  fsInfoBlock[0x01EA] = (unsigned char)
    ((fatData->freeClusterCountF32 & 0x00FF0000) >> 16);
  fsInfoBlock[0x01E9] = (unsigned char)
    ((fatData->freeClusterCountF32 & 0x0000FF00) >> 8);
  fsInfoBlock[0x01E8] = (unsigned char)
    (fatData->freeClusterCountF32 & 0x000000FF);

  // Set the FAT32 "first free cluster".  Doubleword value.
  fsInfoBlock[0x01EF] = (unsigned char)
    ((fatData->firstFreeClusterF32 & 0xFF000000) >> 24);
  fsInfoBlock[0x01EE] = (unsigned char)
    ((fatData->firstFreeClusterF32 & 0x00FF0000) >> 16);
  fsInfoBlock[0x01ED] = (unsigned char)
    ((fatData->firstFreeClusterF32 & 0x0000FF00) >> 8);
  fsInfoBlock[0x01EC] = (unsigned char)
    (fatData->firstFreeClusterF32 & 0x000000FF);

  // We must now write the updated FSInfo block back to the disk

  // Obtain a lock on the disk device
  status = kernelDiskFunctionsLockDisk(fatData->diskObject->diskNumber);

  if (status < 0)
    {
      // We couldn't lock the disk object
      kernelError(kernel_error, "Couldn't lock disk object");
      return (status);
    }

  // Write the FSInfo sector
  status = kernelDiskFunctionsWriteSectors(fatData->diskObject->diskNumber, 
				  fatData->fsInfoSectorF32, 1, fsInfoBlock);

  // We can now release our lock on the disk object
  kernelDiskFunctionsUnlockDisk(fatData->diskObject->diskNumber);
  
  // Make sure that the write was successful
  if (status < 0)
    {
      // Couldn't read the FSInfo sector.  Make an error
      kernelError(kernel_error, FAT_NO_FSINFOSECTOR);
      return (status);
    }

  // Return success
  return (status = 0);
}


static int getVolumeInfo(fatInternalData *fatData)
{
  // This function reads information about the filesystem from
  // the boot sector info block.  It assumes that the requirement to
  // do so has been met (i.e. it does not check whether the update
  // is necessary, and does so unquestioningly).  It returns 0 on success, 
  // negative otherwise.
  
  // This is only used internally, so there is no need to check the
  // disk object.  The functions that are exported will do this.
  
  int status = 0;
  unsigned char bootSector[FAT_MAX_SECTORSIZE];


  kernelDebugEnter();

  // Call the function that reads the boot sector
  status = readBootSector((kernelDiskObject *) fatData->diskObject, 
			  bootSector);

  // Make sure we were successful
  if (status < 0)
    // Couldn't read the boot sector, or it was bad
    return (status);

  // Now we can gather some information about the filesystem

  // Start with the number of bytes per sector.  Word value.
  fatData->bytesPerSector = 
    ((((unsigned int) bootSector[0x0C]) << 8) 
     + (unsigned int) bootSector[0x0B]);

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
  // disk object we're referencing
  if (fatData->bytesPerSector != fatData->diskObject->sectorSize)
    {
      // Not a legal value for FAT
      kernelError(kernel_error, "Bytes-per-sector does not match disk");
      return (status = ERR_BADDATA);
    }

  // Get the number of sectors per cluster.  Byte value.
  fatData->sectorsPerCluster = (unsigned int) bootSector[0x0D];

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
  fatData->reservedSectors = 
    ((((unsigned int) bootSector[0x0F]) << 8)
     + (unsigned int) bootSector[0x0E]);

  // This value must be one or more
  if (fatData->reservedSectors < 1)
    {
      // Not a legal value for FAT
      kernelError(kernel_error, "Illegal reserved sectors");
      return (status = ERR_BADDATA);
    }

  // Next, the number of FAT tables.  Byte value.
  fatData->numberOfFats = (unsigned int) bootSector[0x10];

  // This value must be one or more
  if (fatData->numberOfFats < 1)
    {
      // Not a legal value in our FAT driver
      kernelError(kernel_error, "Illegal number of FATs");
      return (status = ERR_BADDATA);
    }

  // The number of root directory entries.  Word value.
  fatData->rootDirEntries = 
    ((((unsigned int) bootSector[0x12]) << 8)
     + (unsigned int) bootSector[0x11]);

  // The root-dir-entries value should either be 0 (must for FAT32) or,
  // for FAT12 and FAT16, should result in an even multiple of the
  // bytes-per-sector value when multiplied by 32; however, this is not 
  // a requirement.

  // Get the total number of sectors from the 16-bit field.  Word value.
  fatData->totalSectors16 = 
    ((((unsigned int) bootSector[0x14]) << 8) 
     + (unsigned int) bootSector[0x13]);

  // This 16-bit-total-sectors value is linked to the 32-bit-total-sectors 
  // value we will find further down in the boot sector (there are
  // requirements here) but we will save our evaluation of this field 
  // until we have gethered the 32-bit value
  
  // Get the media type.  Byte value.
  fatData->mediaType = (unsigned int) bootSector[0x15];

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
  fatData->fatSectors = 
    ((((unsigned int) bootSector[0x17]) << 8) +
     (unsigned int) bootSector[0x16]);

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
    {
      // The FAT32 number of sectors per FAT.  Doubleword value.
      fatData->fatSectors = 
	((((unsigned int) bootSector[0x27]) << 24)
	 + (((unsigned int) bootSector[0x26]) << 16)
	 + (((unsigned int) bootSector[0x25]) << 8)
	 + ((unsigned int) bootSector[0x24]));
    }

  // The sectors-per-fat value must now be non-zero
  if (fatData->fatSectors < 1)
    {
      // Oops, not a legal value for FAT32
      kernelError(kernel_error, "Illegal FAT32 sectors per fat");
      return (status = ERR_BADDATA);
    }

  // The 16-bit number of sectors per track/cylinder.  Word value.
  fatData->sectorsPerTrack = 
    ((((unsigned int) bootSector[0x19]) << 8)
     + (unsigned int) bootSector[0x18]);

  // This sectors-per-track field is not always relevant.  We won't 
  // check it here.

  // The number of heads for this volume. Word value. 
  fatData->heads = 
    ((((unsigned int) bootSector[0x1B]) << 8) +
     (unsigned int) bootSector[0x1A]);

  // Like the sectors-per-track field, above, this heads field is not always 
  // relevant.  We won't check it here.

  // The number of hidden sectors.  Doubleword value.
  fatData->hiddenSectors = 
    ((((unsigned int) bootSector[0x1F]) << 24)
     + (((unsigned int) bootSector[0x1E]) << 16) 
     + (((unsigned int) bootSector[0x1D]) << 8) 
     + ((unsigned int) bootSector[0x1C]));

  // Hmm, I'm not too sure what to make of this hidden-sectors field.
  // The way I read the documentation is that it describes the number of
  // sectors in any physical partitions that preceed this one.  However,
  // we already get this value from the master boot record where needed.
  // We will ignore the value of this field.

  // The 32-bit number of total sectors.  Doubleword value.
  fatData->totalSectors32 = 
    ((((unsigned int) bootSector[0x23]) << 24)
     + (((unsigned int) bootSector[0x22]) << 16)
     + (((unsigned int) bootSector[0x21]) << 8)
     + ((unsigned int) bootSector[0x20]));

  // Here we must check to ensure that this 32-bit-total-sectors value
  // is consistent with the 16-bit version.  This value can be zero, but
  // only if the 16-bit value is non-zero.  The 16-bit value can be zero,
  // but only if the 32-bit value is non-zero.  Got it?  Both values cannot 
  // be zero at the same time.  
  if ((fatData->totalSectors32 == 0) && (fatData->totalSectors16 == 0))
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
    ((FAT_BYTES_PER_DIR_ENTRY * (unsigned int) fatData->rootDirEntries) / 
     fatData->bytesPerSector);
  if (((FAT_BYTES_PER_DIR_ENTRY * (unsigned int) fatData->rootDirEntries) % 
       fatData->bytesPerSector) != 0)
    fatData->rootDirSectors += 1;
  
  // Calculate the number of total sectors, based on which total-sectors
  // value is non-zero
  if (fatData->totalSectors16 != 0)
    fatData->totalSectors = fatData->totalSectors16;
  else
    fatData->totalSectors = fatData->totalSectors32;

  // This calculation comes directly from MicrosoftTM.  This is how we
  // determine the number of data sectors and data clusters on the volume.
  // We have already ensured that the sectors-per-cluster value is 
  // non-zero (don't worry about a divide-by-zero error here)
  fatData->dataSectors = 
    (fatData->totalSectors - (fatData->reservedSectors + 
    (fatData->numberOfFats * fatData->fatSectors) + fatData->rootDirSectors));
  fatData->dataClusters = (fatData->dataSectors / fatData->sectorsPerCluster);

  /*
    kernelTextPrintUnsigned(fatData->bytesPerSector);
    kernelTextPrintLine(" - Bytes per sector");
    kernelTextPrintUnsigned(fatData->sectorsPerCluster);
    kernelTextPrintLine(" - Sectors per cluster");
    kernelTextPrintUnsigned(fatData->totalSectors16);
    kernelTextPrintLine(" - Total sectors (16 bit)");
    kernelTextPrintUnsigned(fatData->numberOfFats);
    kernelTextPrintLine(" - Number of FATS");
    kernelTextPrintUnsigned(fatData->rootDirEntries);
    kernelTextPrintLine(" - Root directory entries");
    kernelTextPrintUnsigned(fatData->mediaType);
    kernelTextPrintLine(" - Media type");
    kernelTextPrintUnsigned(fatData->fatSectors);
    kernelTextPrintLine(" - Sectors per FAT");
    kernelTextPrintUnsigned(fatData->sectorsPerTrack);
    kernelTextPrintLine(" - Sectors per track");
    kernelTextPrintUnsigned(fatData->heads);
    kernelTextPrintLine(" - Heads");
    kernelTextPrintUnsigned(fatData->hiddenSectors);
    kernelTextPrintLine(" - Hidden sectors");
    kernelTextPrintUnsigned(fatData->totalSectors32);
    kernelTextPrintLine(" - Total sectors (32 bit)");
    kernelTextPrintUnsigned(fatData->totalSectors);
    kernelTextPrintLine(" - Total sectors");
    kernelTextPrintUnsigned(fatData->rootDirSectors);
    kernelTextPrintLine(" - Root directory sectors");
    kernelTextPrintUnsigned(fatData->dataSectors);
    kernelTextPrintLine(" - Data sectors");
    kernelTextPrintUnsigned(fatData->dataClusters);
    kernelTextPrintLine(" - Data clusters");
  */

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

  
  // Now we know the type of the FAT filesystem.  From here, the data
  // gathering we do must diverge.

  if ((fatData->fsType == fat12) || (fatData->fsType == fat16))
    {
      // We have either a FAT12 or FAT16 filesystem.  There are some
      // additional pieces of information we can gather from this boot
      // sector.

      // The drive number.  Byte value.
      fatData->driveNumber = (unsigned int) bootSector[0x24];

      fatData->volumeId = 0;
      fatData->volumeLabel[0] = NULL;
      fatData->fsSignature[0] = NULL;

      // The extended boot block signature.  Byte value.
      fatData->bootSignature = (unsigned int) bootSector[0x26];
  
      // Only do these last ones if the boot signature was 0x29
      if (fatData->bootSignature == (unsigned char) 0x29)
	{
	  // The volume Id of the filesystem.  Doubleword value.
	  fatData->volumeId = 
	    ((((unsigned int) bootSector[0x2A]) << 24) + 
	     (((unsigned int) bootSector[0x29]) << 16) + 
	     (((unsigned int) bootSector[0x28]) << 8) + 
	     (unsigned int) bootSector[0x27]);

	  // The volume label of the filesystem.
	  strncpy((char *) fatData->volumeLabel, 
			(bootSector + 0x2B), 11);
	  fatData->volumeLabel[11] = NULL;
      
	  // The filesystem type indicator "hint"
	  strncpy((char *) fatData->fsSignature, 
			(bootSector + 0x36), 8);
	  fatData->fsSignature[8] = NULL;

	  /*
	    kernelTextPrintUnsigned(fatData->driveNumber);
	    kernelTextPrintLine(" - Drive number");
	    kernelTextPrintUnsigned(fatData->bootSignature);
	    kernelTextPrintLine(" - Boot signature");
	    kernelTextPrintUnsigned(fatData->volumeId);
	    kernelTextPrintLine(" - Volume Id");
	    kernelTextPrint(fatData->volumeLabel);
	    kernelTextPrintLine(" - Volume label");
	  */
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
      if (((unsigned int) bootSector[0x2B] != 0) || 
	  ((unsigned int) bootSector[0x2A] != 0))
	{
	  // Oops, we cannot support this version of FAT32
	  kernelError(kernel_error, "Unsupported FAT32 version");
	  return (status = ERR_BADDATA);
	}

      // Next, we get the starting cluster number of the root directory.
      // Doubleword value.
      fatData->rootDirClusterF32 = 
	((((unsigned int) bootSector[0x2F]) << 24) + 
	 (((unsigned int) bootSector[0x2E]) << 16) +
	 (((unsigned int) bootSector[0x2D]) << 8) +
	 (unsigned int) bootSector[0x2C]);

      // This root-dir-cluster value must be >= 2, and <= (data-clusters + 1)
      if ((fatData->rootDirClusterF32 < 2) || 
	  (fatData->rootDirClusterF32 > (fatData->dataClusters + 1)))
	{
	  // Oops, this is not a legal cluster number
	  kernelError(kernel_error, "Illegal FAT32 root dir cluster");
	  return (status = ERR_BADDATA);
	}

      // Next, we get the sector number (in the reserved area) of the
      // FSInfo structure.  This structure contains some extra information
      // that is useful to us for maintaining large volumes.  Word value.
      fatData->fsInfoSectorF32 = 
	((((unsigned int) bootSector[0x31]) << 8) +
	 (unsigned int) bootSector[0x30]);

      // This number must be greater than 1, and less than the number
      // of reserved sectors in the volume
      if ((fatData->fsInfoSectorF32 < 1) ||
	  (fatData->fsInfoSectorF32 >= fatData->reservedSectors))
	{
	  // Oops, this is not a legal sector number
	  kernelError(kernel_error, "Illegal FAT32 FSInfo sector");
	  return (status = ERR_BADDATA);
	}

      // We will ignore the next 2 fields for now.  They consist of
      // a pointer to a backup boot sector, and a reserved field.

      // The drive number.  Byte value.  Same as the field for FAT12/16,
      // but at a different offset
      fatData->driveNumber = (unsigned int) bootSector[0x40];

      fatData->volumeId = 0;
      fatData->volumeLabel[0] = NULL;
      fatData->fsSignature[0] = NULL;

      // The extended boot block signature.  Byte value.  Same as the field 
      // for FAT12/16, but at a different offset
      fatData->bootSignature = (unsigned int) bootSector[0x42];
  
      // Only do these last ones if the boot signature was 0x29
      if (fatData->bootSignature == (unsigned char) 0x29)
	{
	  // The volume Id of the filesystem.  Doubleword value.  Same as 
	  // the field for FAT12/16, but at a different offset
	  fatData->volumeId = 
	    ((((unsigned int) bootSector[0x46]) << 24) + 
	     (((unsigned int) bootSector[0x45]) << 16) + 
	     (((unsigned int) bootSector[0x44]) << 8) + 
	     (unsigned int) bootSector[0x43]);

	  // The volume label of the filesystem.  Same as the field for 
	  // FAT12/16, but at a different offset
	  strncpy((char *) fatData->volumeLabel, 
			(bootSector + 0x47), 11);
	  fatData->volumeLabel[11] = NULL;
      
	  // The filesystem type indicator "hint"
	  strncpy((char *) fatData->fsSignature, 
			(bootSector + 0x52), 8);
	  fatData->fsSignature[8] = NULL;

	  /*
	    kernelTextPrintUnsigned(fatData->driveNumber);
	    kernelTextPrintLine(" - Drive number");
	    kernelTextPrintUnsigned(fatData->bootSignature);
	    kernelTextPrintLine(" - Boot signature");
	    kernelTextPrintUnsigned(fatData->volumeId);
	    kernelTextPrintLine(" - Volume Id");
	    kernelTextPrint((char *) fatData->volumeLabel);
	    kernelTextPrintLine(" - Volume label");
	  */

	  // Now we will read some additional information, not included
	  // in the boot sector.  This FSInfo sector contains more information
	  // that will be useful in managing large FAT32 volumes.  We 
	  // previously gathered the sector number of this structure from
	  // the boot sector.

	  // From here on, we are finished with the data in the boot
	  // sector, so we will reuse the buffer to hold the FSInfo.
	  
	  // Call the function that will read the FSInfo block
	  status = readFSInfo(
		      (kernelDiskObject *) fatData->diskObject, 
		      (unsigned int) fatData->fsInfoSectorF32, bootSector);

	  // Make sure we were successful
	  if (status < 0)
	    // Couldn't read the boot FSInfo block, or it was bad
	    return (status);

	  // Now we can gather some additional information about the 
	  // FAT32 filesystem

	  // Get the FAT32 "free cluster count".  Doubleword value.
	  fatData->freeClusterCountF32 = 
	    ((((unsigned int) bootSector[0x01EB]) << 24) + 
	     (((unsigned int) bootSector[0x01EA]) << 16) + 
	     (((unsigned int) bootSector[0x01E9]) << 8) + 
	     (unsigned int) bootSector[0x01E8]);

	  // This free-cluster-count value can be zero, but it cannot
	  // be greater than data-clusters -- with one exeption.  It can
	  // be 0xFFFFFFFF (meaning the free cluster count is unknown).
	  if ((fatData->freeClusterCountF32 > fatData->dataClusters) &&
	      (fatData->freeClusterCountF32 != 0xFFFFFFFF))
	    {
	      // Oops, not a legal value for FAT
	      kernelError(kernel_error, "Illegal FAT32 free cluster count");
	      return (status = ERR_BADDATA);
	    }

	  // Finally, get the FAT32 "first free cluster".  Doubleword value.
	  fatData->firstFreeClusterF32 = 
	    ((((unsigned int) bootSector[0x01EF]) << 24) + 
	     (((unsigned int) bootSector[0x01EE]) << 16) + 
	     (((unsigned int) bootSector[0x01ED]) << 8) + 
	     (unsigned int) bootSector[0x01EC]);

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
    }

  // Return success
  return (status = 0);
}


static int checkRemove(fatInternalData *fatData)
{
  // This routine is internal, and is used to check whether a 
  // removable media has been changed.  First, it checks whether
  // the "disk change" line of the disk is active.  If so, it will
  // examine the filesystem on the volume (if any) and report whether
  // the filesystem is different than the mounted filesystem.  It will
  // return 1 if the filesystem has changed, 0 if it has not, and
  // negative on error.

  int changed = 0, status = 0;
  unsigned char tmpBootSector[FAT_MAX_SECTORSIZE];
  unsigned int volumeId = 0;
  int count;


  kernelDebugEnter();

  // Check whether we are dealing with removable media
  if (fatData->diskObject->fixedRemovable != removable)
    {
      // This is not removable media.  We won't call this an error, 
      // but it definitely hasn't been changed
      kernelError(kernel_warn, "Not removable media");
      return (changed = 0);
    }

  // We should only consult the disk driver about the status of the
  // disk change line if we have not previously registered a change;
  // The disk change line can become "unset" when we don't expect it.
  if (fatData->changedLock == 0)
    {
      // Ask the disk driver whether the disk's "disk changed" line
      // has been activated.
      changed = 
	kernelDiskFunctionsDiskChanged(fatData->diskObject->diskNumber);
      
      if (changed == 0)
	{
	  // No disk change has occurred
	  // We can now release our lock on the disk object
	  return (changed = 0);
	}
    }  

  // Ok, the filesystem driver indicates that the disk has been changed
  // at some point since the last time we checked.  This means that we
  // must try to determine whether this is actually still the same 
  // media, since it is theoretically still possible that the user has
  // removed and then replaced the media in question.  

  // Read the boot sector block and compare the volume Id to the
  // cached value

  // Initialize the boot sector block
  for (count = 0; count < fatData->bytesPerSector; count ++)
    tmpBootSector[count] = NULL;

  // Obtain a lock on the disk device
  status = kernelDiskFunctionsLockDisk(fatData->diskObject->diskNumber);

  if (status < 0)
    // We couldn't lock the disk object
    return (status);

  // Read the boot sector
  status = kernelDiskFunctionsReadSectors(fatData->diskObject->diskNumber, 
			  0, 1, tmpBootSector);

  // We can now release our lock on the disk object
  kernelDiskFunctionsUnlockDisk(fatData->diskObject->diskNumber);

  // Make sure that the read was successful
  if (status < 0)
    {
      // Couldn't read the boot sector.  Make an error
      kernelError(kernel_error, FAT_NO_BOOTSECTOR);
      return (status);
    }

  // We will check the volume id on the volume.  If the volume Id is
  // different, we KNOW the volume has been changed unexpectedly.
  // The volume Id of the filesystem.  Doubleword value.
  volumeId = ((((unsigned int) tmpBootSector[0x2A]) << 24) + 
	      (((unsigned int) tmpBootSector[0x29]) << 16) + 
	      (((unsigned int) tmpBootSector[0x28]) << 8) + 
	      (unsigned int) tmpBootSector[0x27]);

  if (volumeId != fatData->volumeId)
    {
      fatData->changedLock = 1;
      return (changed = 1);
    }
  else
    {
      fatData->changedLock = 0;
      return (changed = 0);
    }
}


static int readFatSector(fatInternalData *fatData, 
			 unsigned int sector, unsigned int slot)
{
  // This function will load the requested FAT sector into the requested
  // FAT sector list slot.  Returns 0 on success, negative otherwise
  // Both the requested sector number and slot number are zero-based
  // index numbers.

  // This is only used internally, so there is no need to check the
  // fatData object.  The functions that are exported will do this.
  
  int status = 0;


  kernelDebugEnter();

  // kernelTextPrint("Reading FAT sector ");
  // kernelTextPrintUnsigned(sector);
  // kernelTextPrint(" into list slot ");
  // kernelTextPrintUnsigned(slot);
  // kernelTextNewline();

  // Make sure the FAT sector number and slot number are legal
  if ((sector > fatData->fatSectors) || (slot >= fatData->fatSectorsBuffered))
    return (status = ERR_BOUNDS);

  // Obtain a lock on the disk device
  status = kernelDiskFunctionsLockDisk(fatData->diskObject->diskNumber);

  if (status < 0)
    // We couldn't lock the disk object
    return (status);

  // Read the requested FAT sector into the requested slot.  The FAT 
  // sector address can be calculated as (reservedSectors + fatSectorNumber)
  status = kernelDiskFunctionsReadSectors(
				  fatData->diskObject->diskNumber, 
				  (fatData->reservedSectors + sector), 
				  1, fatData->FAT[slot].data);

  // We can now release our lock on the disk object
  kernelDiskFunctionsUnlockDisk(fatData->diskObject->diskNumber);

  if (status < 0)
    {
      kernelError(kernel_error, "Couldn't read FAT sector");
      return (status);
    }

  // Update the FAT sector's index number 
  fatData->FAT[slot].index = sector;

  // Mark the FAT sector as clean
  fatData->FAT[slot].dirty = 0;

  // Update the "last access" time
  fatData->FAT[slot].lastAccess = kernelSysTimerRead();

  return (status = 0);
}


static int writeFatSector(fatInternalData *fatData, 
			  unsigned int slot)
{
  // This function will write the requested FAT sector back to the disk.  
  // Returns 0 on success, negative otherwise.  The requested slot number 
  // is a zero-based index number.

  // This is only used internally, so there is no need to check the
  // fatData object.  The functions that are exported will do this.
  
  int status = 0;
  int masterStatus = 0;
  int count;


  kernelDebugEnter();

  // kernelTextPrint("Writing FAT sector ");
  // kernelTextPrintUnsigned(fatData->FAT[slot].index);
  // kernelTextPrint(" from list slot ");
  // kernelTextPrintUnsigned(slot);
  // kernelTextNewline();

  // Make sure the FAT slot number is legal
  if (slot >= fatData->fatSectorsBuffered)
    return (status = ERR_BOUNDS);

  // Obtain a lock on the disk device
  status = kernelDiskFunctionsLockDisk(fatData->diskObject->diskNumber);

  if (status < 0)
    // We couldn't lock the disk object
    return (status);

  // When we READ the FAT sectors, we only read sectors from the first
  // FAT.  Many (most?/all?) FAT volumes contain at least one backup FAT.
  // All of the FATs are consecutive on the disk.  If we have changed anything
  // in the master FAT, we should also make those changes to any backup
  // FATs.  The consecutive FATs start immediately after the reserved sectors
  // (i.e. after the boot sector -- usually sector 2)

  // Write this FAT sector's data to each FAT on the volume.  Loop once 
  // for each FAT on the volume.
  for (count = 0; count < fatData->numberOfFats; count ++)
    {
      // Write the requested FAT sector from the requested slot.  The FAT 
      // sector address can be calculated as (reservedSectors + 
      // fatSectorNumber + (fatSectors * count))
      status = kernelDiskFunctionsWriteSectors(
       fatData->diskObject->diskNumber, 
       (fatData->reservedSectors + (fatData->fatSectors * count) + 
	fatData->FAT[slot].index), 1, fatData->FAT[slot].data);

      // Were we successful writing this copy of the sector?
      if (status < 0)
	masterStatus = status;
    }

  // We can now release our lock on the disk object
  kernelDiskFunctionsUnlockDisk(fatData->diskObject->diskNumber);

  // Were all of the write attempts successful?
  if (masterStatus < 0)
    {
      // Couldn't write one or more of the FAT sectors.  Return an error.
      kernelError(kernel_error, "Couldn't write FAT sector");
      return (masterStatus);
    }

  // Mark the FAT sector as clean
  fatData->FAT[slot].dirty = 0;

  // Update the "last access" time
  fatData->FAT[slot].lastAccess = kernelSysTimerRead();

  return (status = 0);
}


static int bufferFatSector(fatInternalData *fatData, 
			   unsigned int sector)
{
  // This function will attempt to find a particular sector in the
  // list of buffered FAT sectors.  If successful, the function returns
  // the relevent slot number.  If the FAT sector is NOT buffered, 
  // the function will cause it to BE buffered.  Returns negative on error.
  
  int status = 0;
  int FATslot = 0;
  unsigned int oldestSectorAge = 0;
  int count;


  //kernelDebugEnter();

  // This is only used internally, so there is no need to check the
  // fatData object.  The functions that are exported will do this.

  // kernelTextPrint("Looking for FAT sector ");
  // kernelTextPrintUnsigned(sector);
  // kernelTextPrintLine(" in buffer list");

  // kernelTextPrint("There are ");
  // kernelTextPrintUnsigned(fatData->fatSectorsBuffered);
  // kernelTextPrintLine(" FAT sectors buffered");

  // Make sure the sector number is legal
  if (sector > fatData->fatSectors)
    return (FATslot = ERR_BOUNDS);

  // Loop through the list of all buffered FAT sectors.  If found, return
  // the current slot number.

  for (count = 0; count < fatData->fatSectorsBuffered; count++)
    if (fatData->FAT[count].index == sector)
      break;

  // Did we find the sector in the list?
  if (count < fatData->fatSectorsBuffered)
    {
      // Update the access time of this FAT sector
      fatData->FAT[count].lastAccess = kernelSysTimerRead();
      return (count);
    }

  // Oops.  The requested FAT sector is not currently buffered.  We need
  // to add it to the list.

  // In order to add a new FAT sector to the buffer list, it will be
  // necessary to expire some other buffered sector (the list is always
  // "full")

  // We will loop through the list of sectors again, this time looking
  // for the least-recently accessed FAT sector

  FATslot = 0;
  oldestSectorAge = fatData->FAT[0].lastAccess;
  
  for (count = 1; count < fatData->fatSectorsBuffered; count++)
    if (fatData->FAT[count].lastAccess < oldestSectorAge)
      {
	FATslot = count;
	oldestSectorAge = fatData->FAT[count].lastAccess;
      }

  // We should now have found the "oldest" FAT sector that is buffered.
  // kernelTextPrint("Un-buffering sector ");
  // kernelTextPrintUnsigned(fatData->FAT[FATslot].index);
  // kernelTextPrint(" from slot ");
  // kernelTextPrintInteger(FATslot);
  // kernelTextPrint(".  Access time is ");
  // kernelTextPrintUnsigned(fatData->FAT[FATslot].lastAccess);
  // kernelTextNewline();
  
  // If this oldest FAT sector is "dirty", we will need to commit 
  // it back to the disk
  if (fatData->FAT[FATslot].dirty)
    {
      // Write the FAT sector back to the disk
      status = writeFatSector(fatData, FATslot);
      
      if (status < 0)
	// If we could not commit this dirty FAT sector to the disk,
	// then we must not overwrite it or data loss will occur.
	return (status);
    }

  // Now we can overwrite the FAT data.  Read the new FAT sector from 
  // the disk.
  status = readFatSector(fatData, sector, FATslot);

  if (status < 0)
    {
      // If we could not read this FAT sector from the disk,
      // we need to return an error code
      kernelError(kernel_error, "Error reading FAT sector");
      return (status);
    }

  // kernelTextPrint("Buffered sector ");
  // kernelTextPrintUnsigned(fatData->FAT[FATslot].index);
  // kernelTextPrint(" in slot ");
  // kernelTextPrintUnsigned(FATslot);
  // kernelTextPrint(".  Access time is ");
  // kernelTextPrintUnsigned(fatData->FAT[FATslot].lastAccess);
  // kernelTextNewline();

  // Return the slot number
  return (FATslot);
}


static int getFatEntry(fatInternalData *fatData, unsigned int entryNumber, 
		       unsigned int *value)
{
  // This function is internal, and takes as a parameter the number
  // of the FAT entry to be read.  On success it returns 0, negative
  // on failure.

  int status = 0;
  unsigned int entryOffset = 0;
  unsigned int entryValue = 0;
  unsigned int fatSector = 0;
  int fatSlot = 0;
  int fatSlot2 = 0;


  //kernelDebugEnter();

  // There is no good reason to read entries 0 or 1.  They're not legitimate
  // clusters anyway.
  if (entryNumber < 2)
    {
      kernelError(kernel_error, "Cannot read FAT value 0 or 1");
      return (status = ERR_BUG);
    }

  // Check to make sure there is such a cluster
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

      // Next, we need to calculate which FAT sector contains the entry
      // word we need to access.
      fatSector = (entryOffset / fatData->bytesPerSector);

      // Call the routine which will load the fat sector into memory,
      // if necessary, and tell us which FAT slot it's in
      fatSlot = bufferFatSector(fatData, fatSector);

      if (fatSlot < 0)
	{
	  // Oops, we could not get the FAT sector.  Return an error.
	  kernelError(kernel_error, "Unable to buffer FAT sector");
	  return (fatSlot);
	}

      // "fatSlot" now contains the slot number of the FAT sector
      // we need to use.

      // Now, once again, we need to calculate the offset -- but this
      // time we get the offset WITHIN the selected FAT sector
      entryOffset = (entryOffset % fatData->bytesPerSector);

      // Now, entryOffset is the index of the WORD value that contains the
      // value we're looking for.  Read that word value

      entryValue = (unsigned int) fatData->FAT[fatSlot].data[entryOffset];

      // This is peculiar to FAT12.  It is possible for the FAT entry to
      // be split between two FAT sectors.  If this is occurring now, we
      // will now need to go ahead and load the NEXT fat sector as well.
      if (entryOffset == (fatData->bytesPerSector - 1))
	{
	  // Load the next FAT sector, whose first byte contains the other
	  // half of the word value we need
	  fatSlot2 = bufferFatSector(fatData, (fatSector + 1));

	  if (fatSlot2 < 0)
	    {
	      // Oops, we could not get the FAT sector.  Return an error.
	      kernelError(kernel_error, "Unable to buffer FAT sector");
	      return (fatSlot2);
	    }

	  entryValue |= (unsigned int) (fatData->FAT[fatSlot2].data[0] << 8);
	}
      else
	entryValue |= (unsigned int) 
	  (fatData->FAT[fatSlot].data[entryOffset + 1] << 8);

      // We're almost finished, except that we need to get rid of the
      // extra nybble of information contained in the word value.  If
      // the extra nybble is in the most-significant spot, we need to 
      // simply mask it out.  If it's in the least significant spot, we
      // need to shift the word right by 4 bits.
      
      // 0 = mask, since the extra data is in the upper 4 bits
      // 1 = shift, since the extra data is in the lower 4 bits
      if ((entryNumber % 2) == 0)
	entryValue = (entryValue & 0x0FFF);
      else
	entryValue = ((entryValue & 0xFFF0) >> 4);
    }

  else if (fatData->fsType == fat16)
    {
      // FAT 16 entries are 2 bytes each.  Thus, we need to take the 
      // entry number we were given and multiply it by 2 to get the 
      // starting byte.
      entryOffset = (entryNumber * 2);
      
      // Next, we need to calculate which FAT sector contains the entry
      // word we need to access.
      fatSector = (entryOffset / fatData->bytesPerSector);

      // Call the routine which will load the fat sector into memory,
      // if necessary, and tell us which FAT slot it's in
      fatSlot = bufferFatSector(fatData, fatSector);

      if (fatSlot < 0)
	{	
	  // Oops, we could not get the FAT sector.  Return an error.
	  kernelError(kernel_error, "Unable to buffer FAT sector");
	  return (fatSlot);
	}

      // "fatSlot" now contains the slot number of the FAT sector
      // we need to use.

      // Now, once again, we need to calculate the offset -- but this
      // time we get the offset WITHIN the selected FAT sector
      entryOffset = (entryOffset % fatData->bytesPerSector);

      entryValue = (unsigned int) 
	((fatData->FAT[fatSlot].data[entryOffset + 1] << 8) + 
	 fatData->FAT[fatSlot].data[entryOffset]);
    }

  else /* if (fatData->fsType == fat32) */
    {
      // FAT 32 entries are 4 bytes each.  Thus, we need to take the 
      // entry number we were given and multiply it by 4 to get the 
      // starting byte.
      entryOffset = (entryNumber * 4);
      
      // Next, we need to calculate which FAT sector contains the entry
      // word we need to access.
      fatSector = (entryOffset / fatData->bytesPerSector);

      // Call the routine which will load the fat sector into memory,
      // if necessary, and tell us which FAT slot it's in
      fatSlot = bufferFatSector(fatData, fatSector);

      if (fatSlot < 0)
	{
	  kernelError(kernel_error, "Unable to buffer FAT sector");
	  // Oops, we could not get the FAT sector.  Return an error.
	  return (fatSlot);
	}

      // "fatSlot" now contains the slot number of the FAT sector
      // we need to use.

      // Now, once again, we need to calculate the offset -- but this
      // time we get the offset WITHIN the selected FAT sector
      entryOffset = (entryOffset % fatData->bytesPerSector);

      entryValue = (unsigned int) 
	((fatData->FAT[fatSlot].data[entryOffset + 3] << 24) + 
	 (fatData->FAT[fatSlot].data[entryOffset + 2] << 16) + 
	 (fatData->FAT[fatSlot].data[entryOffset + 1] << 8) + 
	 fatData->FAT[fatSlot].data[entryOffset]);

      // Really only the bottom 28 bits of this value are relevant
      entryValue = (entryValue & 0x0FFFFFFF);
    }

  *value = entryValue;

  // Return success
  return (status = 0);
}


static int changeFatEntry(fatInternalData *fatData,
			  unsigned int entryNumber, unsigned int value)
{
  // This function is internal, and takes as its parameters the number
  // of the FAT entry to be written and the value to set.  On success it 
  // returns 0.  Returns negative on failure.

  int status = 0;
  unsigned int entryOffset = 0;
  unsigned int entryValue = 0;
  unsigned int oldValue = 0;
  unsigned int newValue = 0;
  unsigned int fatSector = 0;
  int fatSlot = 0;
  int fatSlot2 = 0;


  kernelDebugEnter();

  // NEVER allow anyone to change entries 0 or 1.  They're not legitimate
  // clusters anyway.
  if (entryNumber < 2)
    {
      kernelError(kernel_error, "Cannot change FAT value 0 or 1");
      return (status = ERR_BUG);
    }

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
      
      // Next, we need to calculate which FAT sector contains the entry
      // word we need to access.
      fatSector = (entryOffset / fatData->bytesPerSector);

      // Call the routine which will load the fat sector into memory,
      // if necessary, and tell us which FAT slot it's in
      fatSlot = bufferFatSector(fatData, fatSector);

      if (fatSlot < 0)
	{
	  // Oops, we could not get the FAT sector.  Return an error.
	  kernelError(kernel_error, "Unable to buffer FAT sector");
	  return (fatSlot);
	}

      // "fatSlot" now contains the slot number of the FAT sector
      // we need to use.

      // Now, once again, we need to calculate the offset -- but this
      // time we get the offset WITHIN the selected FAT sector
      entryOffset = (entryOffset % fatData->bytesPerSector);

      // Now, entryOffset is the index of the WORD value that contains the
      // 3 nybbles we want to set.  Read the current word value

      entryValue = (unsigned int) fatData->FAT[fatSlot].data[entryOffset];

      // This is peculiar to FAT12.  It is possible for the FAT entry to
      // be split between two FAT sectors.  If this is occurring now, we
      // will now need to go ahead and load the NEXT fat sector as well.
      if (entryOffset == (fatData->bytesPerSector - 1))
	{
	  // Load the next FAT sector, whose first byte contains the other
	  // half of the word value we need
	  fatSlot2 = bufferFatSector(fatData, (fatSector + 1));

	  if (fatSlot2 < 0)
	    {
	      // Oops, we could not get the FAT sector.  Return an error.
	      kernelError(kernel_error, "Unable to buffer FAT sector");
	      return (fatSlot2);
	    }
	  
	  entryValue |= (unsigned int) (fatData->FAT[fatSlot2].data[0] << 8);
	}
      else
	entryValue |= (unsigned int) 
	  (fatData->FAT[fatSlot].data[entryOffset + 1] << 8);

      // Make a copy of the value we're changing to
      newValue = value;

      // Now we may need to change the value we were passed based on the 
      // offset we've figured out.
  
      if ((entryNumber % 2) == 0)
	{
	  oldValue = (entryValue & 0x0FFF);
	  entryValue = (entryValue & 0xF000);
	  newValue = (value & 0x0FFF);
	}
      else
	{
	  oldValue = (entryValue & 0xFFF0);
	  entryValue = (entryValue & 0x000F);
	  newValue = (value << 4);
	}
    
      // Now we OR our new value into the equation
      entryValue = (entryValue | newValue);

      // Mark this FAT sector as "dirty" (before we actually change it,
      // just to be on the safe side)
      fatData->FAT[fatSlot].dirty = 1;

      // Write the value back to the FAT buffer.  Remember to watch out for
      // the case where we cross a FAT sector boundary
      fatData->FAT[fatSlot].data[entryOffset] = 
	(unsigned char) (entryValue & 0x00FF);

      if (entryOffset == (fatData->bytesPerSector - 1))
	{
	  // Mark the second FAT sector as dirty (before we actually change 
	  // it, just to be on the safe side)
	  fatData->FAT[fatSlot2].dirty = 1;

	  // Change the byte
	  fatData->FAT[fatSlot2].data[0] = 
	    (unsigned char) ((entryValue & 0xFF00) >> 8);
	}
      else
	fatData->FAT[fatSlot].data[entryOffset + 1] = 
	  (unsigned char) ((entryValue & 0xFF00) >> 8);
    }

  else if (fatData->fsType == fat16)
    {
      // FAT 16 entries are 2 bytes each.  Thus, we need to take the 
      // entry number we were given and multiply it by 2 to get the
      // starting byte.  Much simpler for FAT 16.
      entryOffset = (entryNumber * 2);
      
      // Next, we need to calculate which FAT sector contains the entry
      // word we need to access.
      fatSector = (entryOffset / fatData->bytesPerSector);

      // Call the routine which will load the fat sector into memory,
      // if necessary, and tell us which FAT slot it's in
      fatSlot = bufferFatSector(fatData, fatSector);

      if (fatSlot < 0)
	{
	  // Oops, we could not get the FAT sector.  Return an error.
	  kernelError(kernel_error, "Unable to buffer FAT sector");
	  return (fatSlot);
	}

      // "fatSlot" now contains the slot number of the FAT sector
      // we need to use.

      // Now, once again, we need to calculate the offset -- but this
      // time we get the offset WITHIN the selected FAT sector
      entryOffset = (entryOffset % fatData->bytesPerSector);

      // Get the current value of the entry
      oldValue = (unsigned int) 
	((fatData->FAT[fatSlot].data[entryOffset + 1] << 8) + 
	 fatData->FAT[fatSlot].data[entryOffset]);

      entryValue = value;

      // Mark this FAT sector as "dirty" (before we actually change it,
      // just to be on the safe side)
      fatData->FAT[fatSlot].dirty = 1;

      // Write the value back to the FAT buffer
      fatData->FAT[fatSlot].data[entryOffset] = 
	(unsigned char) (entryValue & 0x00FF);
      fatData->FAT[fatSlot].data[entryOffset + 1] = 
	(unsigned char) ((entryValue & 0xFF00) >> 8);
    }

  else /* if (fatData->fsType == fat32) */
    {
      // FAT 32 entries are 4 bytes each.  Thus, we need to take the 
      // entry number we were given and multiply it by 4 to get the
      // starting byte.  Much simpler for FAT 16.
      entryOffset = (entryNumber * 4);
      
      // Next, we need to calculate which FAT sector contains the entry
      // word we need to access.
      fatSector = (entryOffset / fatData->bytesPerSector);

      // Call the routine which will load the fat sector into memory,
      // if necessary, and tell us which FAT slot it's in
      fatSlot = bufferFatSector(fatData, fatSector);

      if (fatSlot < 0)
	{
	  // Oops, we could not get the FAT sector.  Return an error.
	  kernelError(kernel_error, "Unable to buffer FAT sector");
	  return (fatSlot);
	}

      // "fatSlot" now contains the slot number of the FAT sector
      // we need to use.

      // Now, once again, we need to calculate the offset -- but this
      // time we get the offset WITHIN the selected FAT sector
      entryOffset = (entryOffset % fatData->bytesPerSector);

      // Get the current value of the entry
      oldValue = (unsigned int) 
	((fatData->FAT[fatSlot].data[entryOffset + 3] << 24) + 
	 (fatData->FAT[fatSlot].data[entryOffset + 2] << 16) + 
	 (fatData->FAT[fatSlot].data[entryOffset + 1] << 8) + 
	 fatData->FAT[fatSlot].data[entryOffset]);

      // Make sure we preserve the top 4 bits of the previous entry
      entryValue = (value | (oldValue & 0xF0000000));

      // Mark this FAT sector as "dirty" (before we actually change it,
      // just to be on the safe side)
      fatData->FAT[fatSlot].dirty = 1;

      // Write the value back to the FAT buffer
      fatData->FAT[fatSlot].data[entryOffset] = 
	(unsigned char) (entryValue & 0x000000FF);
      fatData->FAT[fatSlot].data[entryOffset + 1] = 
	(unsigned char) ((entryValue & 0x0000FF00) >> 8);
      fatData->FAT[fatSlot].data[entryOffset + 2] = 
	(unsigned char) ((entryValue & 0x00FF0000) >> 16);
      fatData->FAT[fatSlot].data[entryOffset + 3] = 
	(unsigned char) ((entryValue & 0xFF000000) >> 24);
    }

  // Return success
  return (status = 0);
}


static int getNumClusters(fatInternalData *fatData, unsigned int startCluster,
			  unsigned int *clusters)
{
  // This function is internal, and takes as a parameter the starting
  // cluster of a file/directory.  The function will traverse the FAT table 
  // entries belonging to the file/directory, and return the number of 
  // clusters used by that item.

  int status = 0;
  unsigned int currentCluster = 0;
  unsigned int newCluster = 0;


  kernelDebugEnter();

  // Zero clusters by default
  *clusters = 0;

  if (startCluster == 0)
    {
      // This file has no allocated clusters.  Return size zero.
      return (status = 0);
    }

  if ((startCluster < 2) || (startCluster >= fatData->terminalCluster))
    {
      kernelError(kernel_error, "Invalid starting cluster number");
      return (status = ERR_BADDATA);
    }

  // Save the starting value
  currentCluster = startCluster;

  // Now we go through a loop to gather the cluster numbers, adding 1 to the
  // total each time.  A value of terminalCluster or more means that there
  // are no more sectors
  while(1)
    {
      status = getFatEntry(fatData, currentCluster, &newCluster);

      if (status < 0)
	{
	  kernelError(kernel_error, "Error reading FAT table");
	  return (status = ERR_BADDATA);
	}

      *clusters += 1;

      if (newCluster >= fatData->terminalCluster)
	break;

      currentCluster = newCluster;
    }

  return (status = 0);
}


static int getLastCluster(fatInternalData *fatData, unsigned int startCluster,
			  unsigned int *lastCluster)
{
  // This function is internal, and takes as a parameter the starting
  // cluster of a file/directory.  The function will traverse the FAT table 
  // entries belonging to the file/directory, and return the number of 
  // the last cluster used by that item.

  int status = 0;
  unsigned int currentCluster = 0;
  unsigned int newCluster = 0;


  kernelDebugEnter();

  if (startCluster == 0)
    {
      // This file has no allocated clusters.  Return zero (which is not a
      // legal cluster number), however this is not an error
      *lastCluster = 0;
      return (status = 0);
    }

  else if ((startCluster < 2) || (startCluster >= fatData->terminalCluster))
    {
      kernelError(kernel_error, "Invalid startCluster number");
      return (status = ERR_INVALID);
    }

  // Save the starting value
  currentCluster = startCluster;

  // Now we go through a loop to step through the cluster numbers.  A
  // value of terminalCluster or more means that there are no more clusters.
  while (1)
    {
      status = getFatEntry(fatData, currentCluster, &newCluster);

      if (status < 0)
	{
	  kernelError(kernel_error, "Error reading FAT table");
	  return (status = ERR_INVALID);
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
  unsigned int entry = 0;
  unsigned int count;


  // kernelTextPrint("fatData is ");
  // kernelTextPrintUnsigned((unsigned int) fatData);
  // kernelTextNewline();
  
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
  status = kernelResourceManagerLock(&(fatData->freeBitmapLock));

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

  fatData->freeClusters = 0;

  // Mark entries 0 and 1 as busy, since they can't be allocated for
  // anything (data clusters are numbered starting from 2)
  fatData->freeClusterBitmap[0] |= 0xC0;

  // kernelTextPrint("makeFreeBitmap: Examining "); 
  // kernelTextPrintUnsigned(fatData->dataClusters); 
  // kernelTextPrintLine(" data clusters"); 

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
	  fatData->freeClusters++;
	  continue;
	}
      
      // Add the used cluster bit to our bitmap.
      fatData->freeClusterBitmap[count / 8] |= (0x80 >> (count % 8));
    }

  // Unlock the free list
  kernelResourceManagerUnlock(&(fatData->freeBitmapLock));

  // kernelTextPrintUnsigned(fatData->freeClusters);
  // kernelTextPrintLine(" free clusters on volume");

  // We are finished
  fatData->buildingFreeBitmap = 0;
  kernelMultitaskerTerminate(status = 0);
  while(1);
}


static int releaseClusterChain(fatInternalData *fatData,
			       unsigned int startCluster)
{
  // This function returns an unused cluster or sequence of clusters back 
  // to the free list, and marks them as unused in the volume's FAT table
  // Returns 0 on success, negative otherwise

  int status = 0;
  unsigned int currentCluster = 0;
  unsigned int nextCluster = 0;


  kernelDebugEnter();

  if ((startCluster == 0) || (startCluster == fatData->terminalCluster))
    // Nothing to do
    return (status = 0);

  // Attempt to lock the free-block list
  status = kernelResourceManagerLock(&(fatData->freeBitmapLock));
  // Make sure we were successful
  if (status < 0)
    {
      kernelError(kernel_error, "Unable to lock the free-cluster bitmap");
      return (status);
    }

  // kernelTextPrint("releasing clusters: ");

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
	  kernelResourceManagerUnlock(&(fatData->freeBitmapLock));
	  return (status);
	}

      // kernelTextPrintUnsigned(currentCluster);
      // kernelTextPutc(' ');

      // Deallocate the current cluster in the chain
      status = changeFatEntry(fatData, currentCluster, 0);

      if (status)
	{
	  kernelError(kernel_error, "Unable to deallocate cluster");
	  kernelResourceManagerUnlock(&(fatData->freeBitmapLock));
	  return (status);
	}

      // Mark the cluster as free in the free cluster bitmap (mask it off)
      fatData->freeClusterBitmap[currentCluster / 8] &= 
	(0xFF7F >> (currentCluster % 8));

      // Adjust the free cluster count
      fatData->freeClusters++;

      // Any more to do?
      if (nextCluster == fatData->terminalCluster)
	break;

      currentCluster = nextCluster;
    }

  // kernelTextNewline();

  // Unlock the list and return success
  kernelResourceManagerUnlock(&(fatData->freeBitmapLock));
  return (status = 0);
}


static int clearClusterChainData(fatInternalData *fatData,
				 unsigned int startCluster)
{
  // This function will zero all of the cluster data in the supplied
  // cluster chain.  Returns 0 on success, negative otherwise

  int status = 0;
  unsigned int currentCluster = 0;
  unsigned int nextCluster = 0;
  unsigned char *buffer;


  kernelDebugEnter();

  if ((startCluster == 0) || (startCluster == fatData->terminalCluster))
    // Nothing to do
    return (status = 0);

  // Allocate an empty buffer equal in size to one cluster.  The memory
  // allocation routine will make it all zeros.
  buffer = kernelMemoryRequestBlock((fatData->sectorsPerCluster *
	     fatData->bytesPerSector), 0, "temporary filesystem data");

  if (buffer == NULL)
    {
      kernelError(kernel_error, 
		  "Unable to allocate memory for clearing clusters");
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
	  return (status);
	}

      // Obtain a lock on the disk device
      status = kernelDiskFunctionsLockDisk(fatData->diskObject->diskNumber);

      if (status < 0)
	{
	  // We couldn't lock the disk object
	  kernelError(kernel_error, "Unable to lock the disk object");
	  return (status);
	}

      status = 
	kernelDiskFunctionsWriteSectors(fatData->diskObject->diskNumber,
	       (((currentCluster - 2) * fatData->sectorsPerCluster) + 
		fatData->reservedSectors + (fatData->fatSectors * 2) + 
		fatData->rootDirSectors), fatData->sectorsPerCluster, buffer);
	  
      // We can now release our lock on the disk object
      kernelDiskFunctionsUnlockDisk(fatData->diskObject->diskNumber);

      if (status < 0)
	{
	  kernelError(kernel_error, "Error clearing cluster data");
	  return (status);
	}

      // Any more to do?
      if (nextCluster == fatData->terminalCluster)
	break;
    }

  // Rreturn success
  return (status = 0);
}


static int getUnusedClusters(fatInternalData *fatData,
			     unsigned int requested, int *startCluster)
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
  unsigned int terminate = 0;
  unsigned int div, mod;
  unsigned int biggestSize = 0;
  unsigned int biggestLocation = 0;
  unsigned int consecutive = 0;
  unsigned int lastCluster = 0;
  unsigned int count;


  kernelDebugEnter();

  // Make sure the request is bigger than zero
  if (requested == 0)
    {
      // This isn't an "error" per se, we just won't do anything
      *startCluster = 0;
      return (status = 0);
    }

  // Make sure that overall, there are enough free clusters to satisfy
  // the request
  if (fatData->freeClusters < requested)
    {
      kernelError(kernel_error, 
		  "Not enough free space to complete operation");
      return (status = ERR_NOFREE);
    }

  // Attempt to lock the free-block bitmap
  status = kernelResourceManagerLock(&(fatData->freeBitmapLock));
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

  // kernelTextPrint("Allocating cluster block ");
  // kernelTextPrintUnsigned(biggestLocation);
  // kernelTextPutc('-');
  // kernelTextPrintUnsigned(biggestLocation + biggestSize - 1);
  // kernelTextNewline();

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
	  kernelResourceManagerUnlock(&(fatData->freeBitmapLock));
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
	  kernelResourceManagerUnlock(&(fatData->freeBitmapLock));
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
	  kernelResourceManagerUnlock(&(fatData->freeBitmapLock));
	  return (status);
	}
    }

  // Success.  Set the caller's variable
  *startCluster = biggestLocation;

  // Unlock the list and return success
  kernelResourceManagerUnlock(&(fatData->freeBitmapLock));
  return (status = 0);
}


static int dirRequiredEntries(kernelFileEntry *directory)
{
  // This function is internal, and is used to determine how many 32-byte
  // entries will be required to hold the requested directory.  Returns
  // the number of entries required on success, negative otherwise.

  int entries = 0;
  kernelFileEntry *listItemPointer = NULL;


  kernelDebugEnter();

  // Make sure directory is a directory
  if (directory->type != dirT)
    {
      kernelError(kernel_error, 
		  "Directory structure to count is not a directory");
      return (entries = ERR_NOTADIR);
    }

  // We have to count the files in the directory to figure out how
  // many items to fill in
  if (directory->firstFile == NULL)
    {
      // This directory is currently empty.  No way this should ever happen
      kernelError(kernel_error, "Directory structure to count is empty");
      return (entries = ERR_BUG);
    }

  listItemPointer = directory->firstFile;

  while (listItemPointer->nextEntry != NULL)
    {
      entries += 1;

      // '.' and '..' do not have long filename entries
      if (strcmp((char *) listItemPointer->fileName, ".") &&
	  strcmp((char *) listItemPointer->fileName, ".."))
	{
	  // All other entries have long filenames

	  // We can fit 13 characters into each long filename slot
	  entries += (strlen((char *) listItemPointer->fileName) / 13);
	  if ((strlen((char *) listItemPointer->fileName) % 13) != 0)
	    entries += 1;
	}

      listItemPointer = listItemPointer->nextEntry;
    }
  
  return (entries);
}


static unsigned int makeDosDate(unsigned int date)
{
  // This function takes a packed-BCD date value in system format and returns
  // the equivalent in packed-BCD DOS format.

  unsigned int temp = 0;
  unsigned int returnedDate = 0;


  kernelDebugEnter();
  
  
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


static inline unsigned int makeDosTime(unsigned int time)
{
  // This function takes a packed-BCD time value in system format and returns
  // the equivalent in packed-BCD DOS format.

  kernelDebugEnter();

  // The time we get is almost right, except that FAT seconds format only
  // has a granularity of 2 seconds, so we divide by 2 to get the final value.
  // The quick way to fix all of this is to simply shift the whole thing
  // by 1 bit, creating a nice 16-bit DOS time.

  return (time >> 1);
}


static unsigned int makeSystemDate(unsigned int date)
{
  // This function takes a packed-BCD date value in DOS format and returns
  // the equivalent in packed-BCD system format.

  unsigned int temp = 0;
  unsigned int returnedDate = 0;


  kernelDebugEnter();
  
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


static inline unsigned int makeSystemTime(unsigned int time)
{
  // This function takes a packed-BCD time value in DOS format and returns
  // the equivalent in packed-BCD system format.

  kernelDebugEnter();

  // The time we get is almost right, except that FAT seconds format only
  // has a granularity of 2 seconds, so we multiply by 2 to get the final
  // value.  The quick way to fix all of this is to simply shift the whole
  // thing left by 1 bit, which results in a time with the correct number
  // of bits, but always an even number of seconds.

  return (time << 1);
}


static int fillDirectory(kernelFileEntry *currentDir, 
			 unsigned int dirBufferSize, char *dirBuffer)
{
  // This function takes a directory structure and writes it to the
  // appropriate directory on disk.

  int status = 0;
  int fileNameLength = 0;
  int longFilenameSlots = 0;
  int longFilenamePos = 0;
  unsigned char fileCheckSum = 0;

  unsigned char *dirEntry = NULL;
  unsigned char *subEntry = NULL;

  kernelFileEntry *listItemPointer = NULL;
  fatEntryData *entryData = NULL;
  unsigned int temp;
  int count, count2;

  
  kernelDebugEnter();

  if (currentDir->firstFile == NULL)
    {
      // This directory is currently empty.  No way this should ever happen
      kernelError(kernel_error, "Directory structure to fill is empty");
      return (status = ERR_BUG);
    }


  dirEntry = dirBuffer;
  listItemPointer = currentDir->firstFile;

  while(listItemPointer != NULL)
    {
      // kernelTextPrint("filldir: filling entry ");
      // kernelTextPrintLine((char *) listItemPointer->fileName);

      // Make sure this entry belongs to the same filesystem as the
      // directory we're writing
      if (listItemPointer->filesystem != currentDir->filesystem)
	{
	  // Skip this entry.  Probably a mount point.
	  listItemPointer = listItemPointer->nextEntry;
	  continue;
	}

      // Get the entry's data
      entryData = (fatEntryData *) listItemPointer->fileEntryData;

      if (entryData == NULL)
	{
	  kernelError(kernel_error, 
		      "File entry has no private filesystem data");
	  return (status = ERR_BUG);
	}

      // Calculate this file's 8.3 checksum.  We need this in advance for
      // associating the long filename entries
      fileCheckSum = 0;
      for (count = 0; count < 11; count++)
	fileCheckSum = (unsigned char)
	  ((((fileCheckSum & 0x01) << 7) | ((fileCheckSum & 0xFE) >> 1)) 
	   + (unsigned char) entryData->shortAlias[count]);

      // All files except '.' and '..' will have at least one long filename
      // entry, just because that's the only kind we use in Visopsys.  Short
      // aliases are only generated for compatibility.

      if (strcmp((char *) listItemPointer->fileName, ".") &&
	  strcmp((char *) listItemPointer->fileName, ".."))
	{
	  // Figure out how many long filename slots we need
	  fileNameLength = strlen((char *) listItemPointer->fileName);
	  longFilenameSlots = (fileNameLength / 13);
	  if ((fileNameLength % 13) != 0)
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
		      listItemPointer->fileName[longFilenamePos++];
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
		      listItemPointer->fileName[longFilenamePos++];
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
		      listItemPointer->fileName[longFilenamePos++];
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
      
      dirEntry[0x00] = NULL;
      strncpy(dirEntry, (char *) entryData->shortAlias, 11);

      // attributes (byte value)
      dirEntry[0x0B] = (unsigned char) entryData->attributes;

      // reserved (byte value)
      dirEntry[0x0C] = (unsigned char) entryData->res;

      // timeTenth (byte value)
      dirEntry[0x0D] = (unsigned char) entryData->timeTenth;

      // Creation time (word value)
      temp = makeDosTime(listItemPointer->creationTime);
      dirEntry[0x0E] = (unsigned char)(temp & 0x000000FF);
      dirEntry[0x0F] = (unsigned char)(temp >> 8);

      // Creation date (word value)
      temp = makeDosDate(listItemPointer->creationDate);
      dirEntry[0x10] = (unsigned char)(temp & 0x000000FF);
      dirEntry[0x11] = (unsigned char)(temp >> 8);

      // accessedDate (word value)
      temp = makeDosDate(listItemPointer->accessedDate);
      dirEntry[0x12] = (unsigned char) (temp & 0x000000FF);
      dirEntry[0x13] = (unsigned char) (temp >> 8);

      // startClusterHi (word value)
      dirEntry[0x14] = 
	(unsigned char) ((entryData->startCluster & 0x00FF0000) >> 16);
      dirEntry[0x15] = 
	(unsigned char)	((entryData->startCluster & 0xFF000000) >> 24);

      // lastWriteTime (word value)
      temp = makeDosTime(listItemPointer->modifiedTime);
      dirEntry[0x16] = (unsigned char) (temp & 0x000000FF);
      dirEntry[0x17] = (unsigned char) (temp >> 8);

      // lastWriteDate (word value)
      temp = makeDosDate(listItemPointer->modifiedDate);
      dirEntry[0x18] = (unsigned char) (temp & 0x000000FF);
      dirEntry[0x19] = (unsigned char) (temp >> 8);

      // startCluster (word value) 
      dirEntry[0x1A] = (unsigned char) (entryData->startCluster & 0x000000FF);
      dirEntry[0x1B] = (unsigned char) (entryData->startCluster >> 8);

      // Now we get the size.  If it's a directory we write zero for the size
      // (doubleword value)
      if ((entryData->attributes & FAT_ATTRIB_SUBDIR) != 0)
	{
	  dirEntry[0x1C] = (unsigned char) 0;
	  dirEntry[0x1D] = (unsigned char) 0;
	  dirEntry[0x1E] = (unsigned char) 0;
	  dirEntry[0x1F] = (unsigned char) 0;
	}
      else
	{
	  dirEntry[0x1C] = 
	    (unsigned char) (listItemPointer->size & 0x000000FF);
	  dirEntry[0x1D] = 
	    (unsigned char) ((listItemPointer->size & 0x0000FF00) >> 8);
	  dirEntry[0x1E] = 
	    (unsigned char) ((listItemPointer->size & 0x00FF0000) >> 16);
	  dirEntry[0x1F] = 
	    (unsigned char) ((listItemPointer->size & 0xFF000000) >> 24);
	}

      // Increment to the next directory entry spot
      dirEntry += FAT_BYTES_PER_DIR_ENTRY;

      // Increment to the next file structure
      listItemPointer = listItemPointer->nextEntry;
    }
  
  return (status = 0);
}


static int checkFileChain(fatInternalData *fatData,
			  kernelFileEntry *checkFile)
{
  // This function is used to make sure (as much as possible) that the 
  // cluster allocation chain of a file is sane.  This is important so
  // that we don't end up (for example) deleting clusters that belong
  // to other files, etc.  It takes a file entry as its parameter, and
  // returns 0 on success, negative otherwise.

  int status = 0;
  fatEntryData *entryData = NULL;
  unsigned int clusterSize = 0;
  unsigned int expectedClusters = 0;
  unsigned int allocatedClusters = 0;


  kernelDebugEnter();

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
	  kernelError(kernel_error, 
		      "Non-zero-length file has no clusters allocated");
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
      kernelError(kernel_error, FAT_BAD_VOLUME);
      return (status = ERR_BADDATA);
    }

  // We count the number of clusters used by this file, according to
  // the allocation chain
  status = getNumClusters(fatData, entryData->startCluster,
			  &allocatedClusters);

  if (status < 0)
    return (status);

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
      kernelError(kernel_error,
		  "Clusters allocated are less than nominal size");
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


  kernelDebugEnter();

  // Get the entry's data
  entryData = (fatEntryData *) deallocateFile->fileEntryData;

  if (entryData == NULL)
    {
      kernelError(kernel_error, "File entry has no private filesystem data");
      return (status = ERR_BUG);
    }

  // This means that, at least nominally, the file uses up no space
  if (entryData->startCluster == 0)
    return (status = 0);

  // Deallocate the clusters belonging to the file
  status = releaseClusterChain(fatData, entryData->startCluster);

  if (status)
    {
      kernelError(kernel_error, "Unable to deallocate file's clusters");
      return (status);
    }

  // Assign '0' to the file's entry's startcluster
  entryData->startCluster = 0;

  // Update the size of the file
  deallocateFile->blocks = 0;
  deallocateFile->size = 0;
  
  return (status = 0);
}


static int write(fatInternalData *fatData, kernelFileEntry *writeFile, 
		 unsigned int skipClusters, unsigned int writeClusters,
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
  unsigned int clusterSize = 0;
  unsigned int existingClusters = 0;
  unsigned int needClusters = 0;
  unsigned int lastCluster = 0;
  unsigned int currentCluster = 0;
  unsigned int nextCluster = 0;
  unsigned int savedClusters = 0;
  unsigned int startSavedClusters = 0;
  unsigned int count;


  kernelDebugEnter();

  // Get the entry's data
  entryData = (fatEntryData *) writeFile->fileEntryData;

  if (entryData == NULL)
    {
      kernelError(kernel_error, "Entry has no data");
      return (status = ERR_NODATA);
    }

  // Calculate cluster size
  clusterSize = 
    (unsigned int)(fatData->bytesPerSector * fatData->sectorsPerCluster);

  // kernelTextPrint("Write: File size is ");
  // kernelTextPrintInteger(entryData->size);
  // kernelTextNewline();

  // Make sure there's enough free space on the volume BEFORE
  // beginning the write operation
  if (writeClusters > fatData->freeClusters)
    {
      // Not enough free space on the volume.  Make an error
      kernelError(kernel_error, FAT_NOT_ENOUGH_FREE);
      return (status = ERR_NOFREE);
    }

  // How many clusters are currently allocated to this file?  Are there
  // already enough clusters to complete this operation (including any
  // clusters we're skipping)?

  needClusters = (skipClusters + writeClusters);
  status =
    getNumClusters(fatData, entryData->startCluster, &existingClusters);

  if (status < 0)
    {
      kernelError(kernel_error, "Unable to determine current file size");
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

      /*
      // Clear out the data in all of these clusters
      status = clearClusterChainData(fatData, currentCluster);
      
      if (status < 0)
	{
	  kernelError(kernel_error, "Unable to clear new cluster data");
	  releaseClusterChain(fatData, nextCluster);
	  return (status);
	}
      */
      
      // Get the number of the current last cluster
      status = getLastCluster(fatData, entryData->startCluster, &lastCluster);

      if (status < 0)
	{
	  kernelError(kernel_error,
		      "Unable to determine file's last cluster");
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

  // kernelTextPrint("Write: Writing ");
  // kernelTextPrintInteger(writeClusters);
  // kernelTextPrintLine(" clusters");

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
	      
	      savedClusters += 1;
	      continue;
	    }
	}

      // Alright, we can write the clusters we were saving up
      
      // kernelTextPrint("Writing ");
      // kernelTextPrintInteger(savedClusters);
      // kernelTextPrint(" clusters starting at ");
      // kernelTextPrintInteger(startSavedClusters);
      // kernelTextNewline();
      
      // If the filesystem is on removable media, check to make sure that 
      // it hasn't changed unexpectedly
      if (fatData->diskObject->fixedRemovable == removable)
	if (checkRemove(fatData))
	  {
	    // The media has changed unexpectedly.  Make an error
	    kernelError(kernel_error, FAT_MEDIA_REMOVED);
	    return (status = ERR_IO);
	  }

      // Obtain a lock on the disk device
      status = kernelDiskFunctionsLockDisk(fatData->diskObject->diskNumber);

      if (status < 0)
	{
	  // We couldn't lock the disk object
	  kernelError(kernel_error, "Unable to lock the disk object");
	  return (status);
	}

      status = kernelDiskFunctionsWriteSectors(
	       fatData->diskObject->diskNumber,
	       (((startSavedClusters - 2) * fatData->sectorsPerCluster) + 
		fatData->reservedSectors + (fatData->fatSectors * 2) + 
		fatData->rootDirSectors), (fatData->sectorsPerCluster 
					   * savedClusters), buffer);
	  
      // We can now release our lock on the disk object
      kernelDiskFunctionsUnlockDisk(fatData->diskObject->diskNumber);

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
			  (unsigned int *) &(writeFile->blocks));
  if (status < 0)
    return (status);

  writeFile->size = (writeFile->blocks * fatData->sectorsPerCluster *
		     fatData->bytesPerSector);

  // kernelTextPrint("Done.  Wrote ");
  // kernelTextPrintInteger(count);
  // kernelTextPrintLine(" clusters");

  return (count);
}


static int flushFat(fatInternalData *fatData)
{
  // This function flushes the File Allocation Table (FAT) to the
  // disk object.  It writes any dirty FAT sectors in the internal cache,
  // and makes extra backup FAT copies as well.  It returns 0 on success, 
  // negative otherwise.
  
  // This is only used internally, so there is no need to check the
  // disk object.  The functions that are exported will do this.
  
  int status = 0;
  int fatSectorList[MAX_FATSECTORS];
  int dirtySectors = 0;
  int masterFatStatus = 0;
  int count, count2, tmp;


  kernelDebugEnter();

  // We need to loop through the FAT sectors we have cached, looking
  // for ones that are marked "dirty" (i.e. they have been changed since
  // they were loaded from disk).  We add each dirty sector's slot number
  // into a list, sorted by physical sector number.
  
  // Go through the chaotic working list, adding all dirty FAT sectors 
  // to our new list of FAT sectors
  dirtySectors = 0;
  for (count = 0; count < fatData->fatSectorsBuffered; count ++)
    if (fatData->FAT[count].dirty > 0)
      fatSectorList[dirtySectors++] = count;

  /*
  kernelTextPrintInteger(dirtySectors);
  kernelTextPrintLine(" dirty FAT sectors.  Pre-sort:");
  for (count = 0; count < dirtySectors; count ++)
    {
      kernelTextPrintUnsigned(
			  fatData->FAT[ fatSectorList[count] ].index);
      kernelTextPutc(' ');
    }
  kernelTextNewline();
  */

  // Now we will do a quick bubble-sort of this list, sorting them by physical
  // sector number.  This will minimize the number of disk seeks that need
  // to be done to write them.
  for (count = 0; count < dirtySectors; count ++)
    for (count2 = 0; count2 < (dirtySectors - 1); count2 ++)
      if (fatData->FAT[ fatSectorList[count2] ].index > 
	  fatData->FAT[ fatSectorList[count2 + 1] ].index)
	{
	  // The first one is bigger than the second.  Swap them.
	  tmp = fatSectorList[count2 + 1];
	  fatSectorList[count2 + 1] = fatSectorList[count2];
	  fatSectorList[count2] = tmp;
	}

  /*
  kernelTextPrintLine("Dirty FAT sectors, post-sort:");
  for (count = 0; count < dirtySectors; count ++)
    {
      kernelTextPrintUnsigned(
			  fatData->FAT[ fatSectorList[count] ].index);
      kernelTextPutc(' ');
    }
  kernelTextNewline();
  */

  // Write all the master FAT sectors to disk using the "official" function
  for (count = 0; count < dirtySectors; count ++)
    {
      status = writeFatSector(fatData, fatSectorList[count]);

      // If there's an error, don't quit, but save status; we will attempt 
      // to perform the operation for the backup FAT anyway
      if (status < 0)
	masterFatStatus = status;
    }

  // If writing some FAT sector failed, we return an error code
  if (masterFatStatus < 0)
    return (masterFatStatus);
  
  return (status = 0);
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

  kernelDebugEnter();

  // Get the length of the filename
  nameLength = strlen((char *)fileName);

  // Make sure the length of the filename is under the limit
  if (nameLength > MAX_NAME_LENGTH)
    return (fileNameOK = -1);

  // Make sure there's not a ' ' in the first position
  if (fileName[0] == (char) 0x20)
    return (fileNameOK = -2);
  
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
  

  kernelDebugEnter();

  // Unprintable control characters turn into '_'
  if (theChar < (char) 0x20)
    return (returnChar = '_');

  // Likewise, these illegal characters turn into '_'
  else if ((theChar == (char) 0x22) ||
	   (theChar == (char) 0x2A) ||
	   (theChar == (char) 0x2B) ||
	   (theChar == (char) 0x2C) ||
	   (theChar == (char) 0x2E) ||
	   (theChar == (char) 0x2F) ||
	   (theChar == (char) 0x3A) ||
	   (theChar == (char) 0x3B) ||
	   (theChar == (char) 0x3C) ||
	   (theChar == (char) 0x3D) ||
	   (theChar == (char) 0x3E) ||
	   (theChar == (char) 0x3F) ||
	   (theChar == (char) 0x5B) ||
	   (theChar == (char) 0x5C) ||
	   (theChar == (char) 0x5D) ||
	   (theChar == (char) 0x7C))
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
  char theChar = NULL;
  char aliasName[9];
  int aliasNameCount = 0;
  char aliasExt[4];
  int aliasExtCount = 0;
  int lastDot = 0;
  int conflict = 0;
  int tildeNumber = 1;
  int count;

  
  kernelDebugEnter();

  // Get the entry's data
  entryData = (fatEntryData *) theFile->fileEntryData;

  if (entryData == NULL)
    {
      kernelError(kernel_error, "File has no private filesystem data");
      return (status = ERR_BUG);
    }

  // strncpy((char *) entryData->shortAlias, "FOOFILE FOO", 11);
  // return (status = 0);

  // The short alias field of the file structure is a 13-byte field
  // with room for the 8 filename characters, a '.', the 3 extension
  // characters, and a NULL.  It must be in all capital letters, and
  // there is a particular shortening algorithm used.  This algorithm
  // is defined in the Microsoft FAT Filesystem Specification.

  // Fully initialize the shortAlias name and extension, since they
  // both need to be padded with [SPACE] characters

  for (count = 0; count < 8; count ++)
    aliasName[count] = ' ';
  aliasName[8] = NULL;

  for (count = 0; count < 3; count ++)
    aliasExt[count] = ' ';
  aliasExt[3] = NULL;

  // Now, this is a little tricky.  We have to examine the first six
  // characters of the long filename, and interpret them according
  // to the predefined format.
  // - All capitals
  // - There are a bunch of illegal characters we have to check for
  // - Illegal characters are replaced with underscores
  // - "~#" is placed in the last two slots of the filename.  # is
  //   based on the number of files in the same directory that might
  //   share the same abreviation.  We have to search the directory
  //   for name clashes of this sort
  // - The file extension (at least the "last" file extension) is 
  //   kept, except that it is truncated to 3 characters if necessary.

  // Get the filename extension first.  This will be up to 3 characters AFTER
  // the LAST '.' character (if any).  Find the last '.'

  count = (strlen((char *) theFile->fileName) - 1); 
  theChar = NULL;

  while ((count >= 0) && ((theChar = theFile->fileName[count]) != '.'))
    count--;

  // Now, if we actually found a '.'...
  if (theChar == '.')
    {
      lastDot = count;
      count += 1;

      for (aliasExtCount = 0; aliasExtCount < 3; )
	{
	  theChar = theFile->fileName[count++];
	  
	  if (theChar == NULL)
	    break;
	  
	  // We just eliminate spaces
	  else if (theChar == (char) 0x20)
	    continue;
	  
	  // Otherwise, call this function to make sure the character
	  // is something legal
	  aliasExt[aliasExtCount++] = xlateShortAliasChar(theChar);
	}
    }

  // kernelTextPrint("LastDot is ");
  // kernelTextPrintInteger(lastDot);
  // kernelTextNewline();

  // Reset to the beginning of the filename
  count = 0;

  // The first eight valid characters of the name (not the extension)
  for (aliasNameCount = 0; aliasNameCount < 8; )
    {
      theChar = theFile->fileName[count++];
      
      if (((lastDot > 0) && (count > lastDot)) || (theChar == NULL))
	break;

      // We just eliminate spaces and dots
      if ((theChar == (char) 0x20) || (theChar == (char) 0x2E))
	continue;

      // Call this function to make sure the character is something legal
      aliasName[aliasNameCount++] = xlateShortAliasChar(theChar);
    }

  // If there's more to the filename part, we have to stick that goofy 
  // tilde thing on the end.  Yay for Microsoft; that's a very innovative
  // solution to the short filename problem.  Isn't that innovative?
  // Innovating is an important way to innovate the innovation -- that's 
  // what I always say.  Microsoft is innovating us into the future.  
  // Innovation.  (That's 7 points for me, plus I get an extra one for 
  // using four different forms of the word)

  if ((aliasNameCount == 8) &&
      ((theFile->fileName[count] != '.') &&
       (theFile->fileName[count] != NULL)))
    {
      // We start with this default configuration before we
      // go looking for conflicts
      aliasName[6] = '~';
      aliasName[7] = '1';
    }

  // kernelTextPrint("The filename part is \"");
  // kernelTextPrint(aliasName);
  // kernelTextPrintLine("\"");

  // Concatenate the name and extension
  strncpy((char *) entryData->shortAlias, aliasName, 8);
  if (aliasExt[0] != NULL)
    {
      // kernelTextPrint("The extension part is \"");
      // kernelTextPrint(aliasExt);
      // kernelTextPrintLine("\"");

      strcat((char *) entryData->shortAlias, aliasExt);
    }


  while(1)
    {
      // Make sure there aren't any name conflicts in the file's directory
      listItemPointer = 
	((kernelFileEntry *) theFile->parentDirectory)->firstFile;

      conflict = 0;

      while (listItemPointer != NULL)
	{
	  if (listItemPointer != theFile)
	    {
	      // kernelTextPrint("comparing \"");
	      // kernelTextPrint(listItemPointer->shortAlias);
	      // kernelTextPrint("\" and \"");
	      // kernelTextPrint(theFile->shortAlias);
	      // kernelTextPrintLine("\"");

	      // Get the list item's data
	      listItemData = (fatEntryData *) listItemPointer->fileEntryData;

	      if (listItemData == NULL)
		{
		  kernelError(kernel_error, 
			      "File has no private filesystem data");
		  return (status = ERR_BUG);
		}

	      if (strcmp((char *) listItemData->shortAlias, 
			       (char *) entryData->shortAlias) == 0)
		{
		  conflict = 1;
		  break;
		}
	    }

	  listItemPointer = listItemPointer->nextEntry;
	}

      if (conflict == 0)
	break;

      else
	{
	  // There was a name conflict using the current configuration.
	  // We must increment the ~n thing until there are no more
	  // conflicts
	  // kernelTextPrint("Name clash: ");
	  // kernelTextPrintLine((char *) entryData->shortAlias);

	  // We will increment the tilde number on the end of the filename
	  tildeNumber += 1;

	  if (tildeNumber >= 100)
	    {
	      // Too many name clashes
	      kernelError(kernel_error, "Too many short alias name clashes");
	      return (status = ERR_NOFREE);
	    }

	  else if (tildeNumber >= 10)
	    {
	      entryData->shortAlias[5] = '~';
	      entryData->shortAlias[6] = 
		((char) 48 + (char) (tildeNumber / 10));
	    }

	  entryData->shortAlias[7] = ((char) 48 + (char) (tildeNumber % 10));

	  conflict = 0;
	  continue;
	}
    }

  // kernelTextPrint("Short alias for ");
  // kernelTextPrint((char *) theFile->fileName);
  // kernelTextPrint(" is \"");
  // kernelTextPrint((char *) entryData->shortAlias);
  // kernelTextPrintLine("\"");

  return (status = 0);
}


static fatEntryData *newEntryData(void)
{
  // This function will find an unused entry data and return it to
  // the calling function.

  fatEntryData *entryData = NULL;


  kernelDebugEnter();

  // Make sure there is a free entry data available
  if (usedEntryDatas >= MAX_BUFFERED_FILES)
    {
      // This should never happen.  Something is wrong at an upper level
      kernelError(kernel_error, 
		  "No more entries for private filesystem data");
      return (entryData = NULL);
    }

  // Get a free entry data.  Grab it from the spot right after the
  // last used entry data
  entryData = entryDatas[usedEntryDatas++];

  // Make sure it's not NULL
  if (entryData == NULL)
    {
      kernelError(kernel_error, 
		  "No more entries for private filesystem data");
      return (entryData = NULL);
    }
      
  // Initialize this new one
  kernelMemClear((void *) entryData, sizeof(fatEntryData));

  return (entryData);
}


static void releaseEntryData(fatEntryData *entryData)
{
  // This function takes an entry fata to release, and puts it back
  // in the pool of free entry datas.

  int spotNumber = -1;
  fatEntryData *temp = NULL;
  int count;


  kernelDebugEnter();

  // Find the supplied entry in the list.
  for (count = 0; ((count < usedEntryDatas) && 
		   (count < MAX_BUFFERED_FILES)); count ++)
    if (entryData == entryDatas[count])
      {
	spotNumber = count;
	break;
      }

  // Did we find it?
  if (spotNumber == -1)
    return;

  // Erase all of the data in this entry
  kernelMemClear((void *) entryData, sizeof(fatEntryData));

  // Put the entry data back into the pool of unallocated entry datas.
  // The way we do this is to move the LAST entry into the spot occupied by
  // this entry, and stick this entry where the old LAST entry was.  The only
  // time we DON'T do this is if the entry was (a) the only used entry; or
  // (b) the last used entry.

  // Update the counts
  usedEntryDatas -= 1;

  if ((usedEntryDatas != 0) && (spotNumber != usedEntryDatas))
    {
      temp = entryDatas[usedEntryDatas];
      entryDatas[usedEntryDatas] = entryData;
      entryDatas[spotNumber] = temp;
    }

  // Cool.
  return;
}


static int scanDirectory(fatInternalData *fatData, 
			 kernelFilesystem *filesystem, 
			 kernelFileEntry *currentDir, char *dirBuffer, 
			 unsigned int dirBufferSize)
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


  kernelDebugEnter();

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

      // Peek ahead and get the attributes (byte value).  Figure out the 
      // type of the file
      if (((unsigned int) dirEntry[0x0B] & FAT_ATTRIB_VOLUMELABEL) != 0)
	// Then it's a volume label.  Skip it for now
	continue;


      // If we fall through to here, it must be a good file or directory.

      // Now we should create a new entry in the "used" list for this item

      // Get a free file entry structure.
      newItem = kernelFileNewEntry(filesystem);
      
      // Make sure it's OK
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
	      
	  for (count2 = 0; count2 < 2; count2 ++)
	    {
	      // Get the first five 2-byte characters from this entry
	      for (count3 = 1; count3 < 10; count3 += 2)
		newItem->fileName[longFilenamePos++] = 
		  (unsigned char) subEntry[count3];

	      // Get the next six 2-byte characters
	      for (count3 = 14; count3 < 26; count3 += 2)
		newItem->fileName[longFilenamePos++] = 
		  (unsigned char) subEntry[count3];

	      // Get the last two 2-byte characters
	      for (count3 = 28; count3 < 32; count3 += 2)
		newItem->fileName[longFilenamePos++] = 
		  (unsigned char) subEntry[count3];

	      // Determine whether this was the last long filename
	      // entry for this file.  If not, we subtract 32 from 
	      // subEntry and loop
	      if ((subEntry[0] & 0x40) != 0)
		break;
	      else
		subEntry -= 32;
	    }

	  newItem->fileName[longFilenamePos] = NULL;

	}
      else
	longFilename = 0;

      // Now go through the regular (DOS short) entry for this file.

      // Copy short alias into the shortAlias field of the file structure
      strncpy((char *) entryData->shortAlias, dirEntry, 11);

      // If there's no long filename, set the filename to be the same as
      // the short alias we just extracted.  We'll need to construct it
      // from the drain-bamaged format used by DOS(TM)
      if (longFilename == 0)
	{
	  strncpy((char *) newItem->fileName, 
			(char *) entryData->shortAlias, 8);

	  // Short filenames are case-insensitive, and are usually 
	  // represented by all-uppercase letters.  This looks silly
	  // in the modern world, so we convert them all to lowercase
	  // as a matter of preference.
	  for (count2 = 0; count2 < strlen((char *) newItem->fileName);
	       count2 ++)
	    newItem->fileName[count2] =
	      (char) tolower(newItem->fileName[count2]);

	  // Insert a NULL if there's a [space] character anywhere in 
	  // this part
	  for (count2 = 0; count2 < strlen((char *) newItem->fileName);
	       count2 ++)
	    if (newItem->fileName[count2] == ' ')
	      newItem->fileName[count2] = NULL;
      
	  // If the extension is non-empty, insert a '.' character in the
	  // middle between the filename and extension
	  if (entryData->shortAlias[8] != ' ')
	    strncat((char *) newItem->fileName, ".", 1);
      
	  // Copy the filename extension
	  strncat((char *) newItem->fileName,
			((char *) entryData->shortAlias + 8), 3);

	  // Insert a NULL if there's a [space] character anywhere in 
	  // the name
	  for (count2 = 0; count2 < strlen((char *) newItem->fileName); 
	       count2 ++)
	    if (newItem->fileName[count2] == ' ')
	      newItem->fileName[count2] = NULL;
	}
      

      // Get the entry's various other information


      // Attributes (byte value)
      entryData->attributes = (unsigned int) dirEntry[0x0B];

      if ((entryData->attributes & FAT_ATTRIB_SUBDIR) != 0)
	// Then it's a subdirectory
	newItem->type = dirT;
      else
	// It's a regular file
	newItem->type = fileT;

      // reserved (byte value)
      entryData->res = (unsigned int) dirEntry[0x0C];

      // timeTenth (byte value)
      entryData->timeTenth = (unsigned int) dirEntry[0x0D];

      // Creation time (word value)
      newItem->creationTime = 
	makeSystemTime(((unsigned int) dirEntry[0x0F] << 8) + 
		       (unsigned int) dirEntry[0x0E]);

      // Creation date (word value)
      newItem->creationDate = 
	makeSystemDate(((unsigned int) dirEntry[0x11] << 8) + 
		       (unsigned int) dirEntry[0x10]);

      // Last access date (word value)
      newItem->accessedDate = 
	makeSystemDate(((unsigned int) dirEntry[0x13] << 8) + 
		       (unsigned int) dirEntry[0x12]);

      // High word of startCluster (word value)
      entryData->startCluster = (((unsigned int) dirEntry[0x15] << 24) + 
				 ((unsigned int) dirEntry[0x14] << 16));

      // Last modified time (word value)
      newItem->modifiedTime = 
	makeSystemTime(((unsigned int) dirEntry[0x17] << 8) + 
		       (unsigned int) dirEntry[0x16]);

      // Last modified date (word value)
      newItem->modifiedDate = 
	makeSystemDate(((unsigned int) dirEntry[0x19] << 8) + 
		       (unsigned int) dirEntry[0x18]);

      // Low word of startCluster (word value) 
      entryData->startCluster |= (((unsigned int) dirEntry[0x1B] << 8) + 
				  (unsigned int) dirEntry[0x1A]);

      // Now we get the size.  If it's a directory we have to actually
      // call getNumClusters() to get the size in clusters

      status = getNumClusters(fatData, entryData->startCluster,
			      (unsigned int *) &(newItem->blocks));

      if (status < 0)
	{
	  kernelError(kernel_error,
		      "Couldn't determine the number of clusters");
	  return (status);
	}

      if ((entryData->attributes & FAT_ATTRIB_SUBDIR) != 0)
	newItem->size = (newItem->blocks * (fatData->bytesPerSector * 
					    fatData->sectorsPerCluster));
      else
	{
	  // (doubleword value)
	  newItem->size = (((unsigned int) dirEntry[0x1F] << 24) + 
		  ((unsigned int) dirEntry[0x1E] << 16) + 
		  ((unsigned int) dirEntry[0x1D] << 8) + 
		  (unsigned int) dirEntry[0x1C]);
	}

      // Add our new entry to the existing file chain.  Don't panic and/or
      // quit if we have a problem of some sort
      kernelFileInsertEntry(newItem, currentDir);
    }

  return (status = 0);
}


static int read(fatInternalData *fatData, kernelFileEntry *theFile, 
		unsigned int skipClusters, unsigned int readClusters, 
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
  unsigned int fileClusters = 0;
  unsigned int clusterSize = 0;
  unsigned int currentCluster = 0;
  unsigned int nextCluster = 0;
  unsigned int savedClusters = 0;
  unsigned int startSavedClusters = 0;
  unsigned int count;


  kernelDebugEnter();

  // Get the entry's data
  entryData = (fatEntryData *) theFile->fileEntryData;

  if (entryData == NULL)
    {
      kernelError(kernel_error, "Entry has no data");
      return (status = ERR_BUG);
    }

  // Calculate cluster size
  clusterSize = 
    (unsigned int)(fatData->bytesPerSector * fatData->sectorsPerCluster);

  // kernelTextPrint("Read: skipping ");
  // kernelTextPrintUnsigned(skipClusters);
  // kernelTextPrintLine(" clusters");

  currentCluster = entryData->startCluster;

  // kernelTextPrint("Read: startCluster is ");
  // kernelTextPrintUnsigned(currentCluster);
  // kernelTextNewline();

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
  
  // kernelTextPrint("Read: reading ");
  // kernelTextPrintUnsigned(readClusters);
  // kernelTextPrintLine(" clusters");

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

	  // kernelTextPrintInteger(nextCluster);
	  // kernelTextPrintLine(" - Next cluster");

	  // We want to minimize the number of read operations, so if
	  // we get clusters with consecutive numbers we should read
	  // them all in a single operation
	  if (nextCluster == (currentCluster + 1))
	    {
	      if (savedClusters == 0)
		startSavedClusters = currentCluster;
	      
	      savedClusters += 1;
	      continue;
	    }
	}

      // kernelTextPrint("Reading ");
      // kernelTextPrintInteger(savedClusters);
      // kernelTextPrint(" clusters starting at ");
      // kernelTextPrintInteger(startSavedClusters);
      // kernelTextNewline();
      
      // If the filesystem is on removable media, check to make sure that 
      // it hasn't changed unexpectedly
      if (fatData->diskObject->fixedRemovable == removable)
	if (checkRemove(fatData))
	  {
	    // The media has changed unexpectedly.  Make an error
	    kernelError(kernel_error, FAT_MEDIA_REMOVED);
	    return (status = ERR_IO);
	  }

      // Obtain a lock on the disk device
      status = kernelDiskFunctionsLockDisk(fatData->diskObject->diskNumber);

      if (status < 0)
	// We couldn't lock the disk object
	return (status);

      // Read the cluster into the buffer.
      status = kernelDiskFunctionsReadSectors(
	      fatData->diskObject->diskNumber, (((startSavedClusters - 2) 
		 * fatData->sectorsPerCluster) + fatData->reservedSectors + 
		(fatData->fatSectors * 2) + fatData->rootDirSectors ), 
	      (fatData->sectorsPerCluster * savedClusters), buffer);

      // We can now release our lock on the disk object
      kernelDiskFunctionsUnlockDisk(fatData->diskObject->diskNumber);

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

  // kernelTextPrint("Done.  Read ");
  // kernelTextPrintInteger(count);
  // kernelTextPrintLine(" clusters");

  return (count);
}


static int readRootDir(fatInternalData *fatData, kernelFilesystem *filesystem)
{
  // This function reads the root directory from the disk indicated by the
  // filesystem pointer.  It assumes that the requirement to do so has been
  // met (i.e. it does not check whether the update is necessary, and does so
  // unquestioningly).  It returns 0 on success, negative otherwise.
  
  // This is only used internally, so there is no need to check the
  // disk object.  The functions that are exported will do this.
  
  int status = 0;
  kernelFileEntry *rootDir = NULL;
  unsigned int rootDirStart = 0;
  unsigned char *dirBuffer = NULL;
  unsigned int dirBufferSize = 0;
  fatEntryData *rootDirData = NULL;
  fatEntryData dummyEntryData;
  int rootDirBlocks = 0;
  kernelFileEntry dummyEntry;


  kernelDebugEnter();

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

  dirBuffer = kernelMemoryRequestBlock(dirBufferSize, 0, 
				       "temporary filesystem data");

  // kernelTextPrint("Dir buffer size is ");
  // kernelTextPrintInteger(dirBufferSize);
  // kernelTextPrint(" starting at ");
  // kernelTextPrintInteger((int) dirBuffer);
  // kernelTextNewline();

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

      // Obtain a lock on the disk device
      status = kernelDiskFunctionsLockDisk(fatData->diskObject->diskNumber);

      if (status < 0)
	{
	  // We couldn't lock the disk object
	  kernelMemoryReleaseByPointer(dirBuffer);
	  return (status);
	}

      // Now we read all of the sectors for the root directory
      status = kernelDiskFunctionsReadSectors(fatData->diskObject->diskNumber,
		      rootDirStart, fatData->rootDirSectors, dirBuffer);

      // We can now release our lock on the disk object
      kernelDiskFunctionsUnlockDisk(fatData->diskObject->diskNumber);

      // Were we successful?
      if (status < 0)
	{
	  kernelMemoryReleaseByPointer(dirBuffer);
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
	return (status);

      // The only thing the read routine needs in this data structure
      // is the starting cluster number.
      dummyEntryData.startCluster = fatData->rootDirClusterF32;
      dummyEntry.fileEntryData = (void *) &dummyEntryData;

      // Go.
      status = read(fatData, &dummyEntry, 0, rootDirBlocks, dirBuffer);

      // Were we successful?
      if (status < 0)
	{
	  kernelMemoryReleaseByPointer(dirBuffer);
	  return (status);
	}
    }

  // The whole root directory should now be in our buffer.  We can proceed
  // to make the applicable data structures

  // Get a new file entry for the filesystem's root directory
  rootDir = kernelFileNewEntry(filesystem);

  // Make sure it's OK
  if (rootDir == NULL)
    {
      // Not enough free file structures
      kernelError(kernel_error, "Not enough free file structures");
      kernelMemoryReleaseByPointer(dirBuffer);
      return (status = ERR_NOFREE);
    }
  
  // Get the entry data structure.  This should have been created by
  // a call to our NewEntry function by the kernelFileNewEntry call.
  rootDirData = (fatEntryData *) rootDir->fileEntryData;

  if (rootDirData == NULL)
    {
      kernelError(kernel_error, "Entry has no private data");
      kernelFileReleaseEntry(rootDir);
      return (status = ERR_NODATA);
    }

  // Fill out some starting values in the file entry structure

  strcpy((char *) rootDir->fileName, (char *) filesystem->mountPoint);

  rootDir->type = dirT;
  rootDir->blocks = rootDirBlocks;

  if ((fatData->fsType == fat12) || (fatData->fsType == fat16))
    rootDir->size = (fatData->rootDirSectors * fatData->bytesPerSector);
  else
    rootDir->size = (rootDir->blocks * fatData->sectorsPerCluster * 
		     fatData->bytesPerSector);

  // Fill out some values in the directory's private data
  strncpy((char *) rootDirData->shortAlias, "/", 1);
  rootDirData->attributes = (FAT_ATTRIB_SUBDIR | FAT_ATTRIB_SYSTEM);
  rootDirData->startCluster = 0;

  // We have to read the directory and fill out the chain of its
  // files/subdirectories in the lists.
  status = scanDirectory(fatData, filesystem, rootDir, dirBuffer,
			 dirBufferSize);

  if (status < 0)
    {
      kernelError(kernel_error, "Error parsing root directory");
      kernelFileReleaseEntry(rootDir);
      kernelMemoryReleaseByPointer(dirBuffer);
      return (status);
    }
	      
  // Attach the root directory to the filesystem structure
  filesystem->filesystemRoot = rootDir;

  // We have to release the directory buffer
  kernelMemoryReleaseByPointer(dirBuffer);

  // Return success
  return (status = 0);
}


static fatInternalData *fatUpdate(kernelFilesystem *filesystem)
{
  // This function updates the saved information about the requested
  // filesystem.  It assumes that the requirement to do so has been met
  // (i.e. it does not check whether the update is necessary, and does so
  // unquestioningly).  It returns 0 on success, negative otherwise.

  // This is only used internally, so there is no need to check the
  // disk object.  The functions that are exported will do this.
  
  int status = 0;
  fatInternalData *fatData = NULL;
  int fatSectorsToBuffer = 0;
  int count;


  kernelDebugEnter();

  // We must allocate some new memory to hold information about
  // the filesystem
  fatData = kernelMemoryRequestSystemBlock(sizeof(fatInternalData), 
			     0 /* no alignment */, "FAT filesystem data");
  if (fatData == NULL)
    {
      // Oops.  Something went wrong.
      kernelError(kernel_error, "Unable to allocate FAT data memory");
      return (fatData = NULL);
    }

  // Attach the disk object to the fatData structure
  fatData->diskObject = filesystem->disk;

  // Get the disk's boot sector info
  status = getVolumeInfo(fatData);

  if (status < 0)
    {
      // Oops.  Something went wrong.
      kernelError(kernel_error, "Unable to get FAT volume info");
      // Attempt to free the fatData memory
      kernelMemoryReleaseByPointer((void *) fatData);
      return (fatData = NULL);
    }

  // Now that we know the size of the FAT, we can allocate memory
  // for the FAT and the free list.  We use the number of FAT sectors on the
  // volume and their bytes per sector for the FAT, and total count of 
  // legal data clusters on the volume to determine the free list size.
  // Note that there is a maximum number of FAT sectors that we will keep
  // in memory at any given time.  This is done because some large volumes
  // will have far too many such sectors for us to reasonably store in
  // RAM.  Thus, if fatData->fatSectors exceeds the MAX_FATSECTORS
  // parameter, we will allocate only enough memory to hold MAX_FATSECTORS
  if (fatData->fatSectors <= MAX_FATSECTORS)
    fatSectorsToBuffer = fatData->fatSectors;
  else
    fatSectorsToBuffer = MAX_FATSECTORS;

  fatData->fatBufferSize = (fatData->bytesPerSector * fatSectorsToBuffer);
  fatData->freeBitmapSize = (fatData->dataClusters / 8);

  fatData->fatBuffer = 
    kernelMemoryRequestSystemBlock((fatData->fatBufferSize + 
			    fatData->freeBitmapSize), 0, "filesystem data");

  if (fatData->fatBuffer == NULL)
    {
      // Oops.  Something went wrong.
      kernelError(kernel_error, 
		  "Couldn't allocate memory for FAT table data");
      // Attempt to free the fatData memory
      kernelMemoryReleaseByPointer((void *) fatData);
      return (fatData = NULL);
    }

  // The FAT buffer pointer is already set, above.  Now set it for the
  // free list
  fatData->freeClusterBitmap = (fatData->fatBuffer + fatData->fatBufferSize);

  // The number of FAT sectors buffered is currently zero
  fatData->fatSectorsBuffered = 0;

  // Now we need to set up the list of fatSector structures so that
  // they contain the correct intitial set of FAT data in memory.
  for (count = 0; count < fatSectorsToBuffer; count++)
    {
      // Set the FAT structure's data pointer
      fatData->FAT[count].data = 
	(fatData->fatBuffer + (count * fatData->bytesPerSector));

      // "Age" it infinitely (to timer count zero), mark it clean,
      // and give it an improbable sector number so that the buffering 
      // routine will replace this empty entry with the real desired data.
      fatData->FAT[count].lastAccess = 0;
      fatData->FAT[count].dirty = 0;
      fatData->FAT[count].index = (0xFFFFFFFF - count);

      // Increase the size of the FAT buffer list to be big enough to
      // hold this new sector
      fatData->fatSectorsBuffered += 1;

      // Call the routine that will buffer FAT sector "count" in slot 
      // number "count"
      status = bufferFatSector(fatData, count);

      if (status != count)
	{
	  // Oops.  Something went wrong.  Either the function returned
	  // an error code, or it's attempting to buffer this sector in
	  // the wrong slot.  Either one of these is bad.
	  kernelError(kernel_error, "Unable to buffer FAT sector");

	  // Attempt to free all the memory
	  kernelMemoryReleaseByPointer(fatData->fatBuffer);
	  kernelMemoryReleaseByPointer((void *) fatData);
	  return (fatData = NULL);
	}
    }

  // kernelTextPrint("fatData is ");
  // kernelTextPrintUnsigned((unsigned int) fatData);
  // kernelTextNewline();

  // Build the free cluster list.  We need to spawn this function as an
  // independent, non-blocking thread, which must be a child process of
  // the kernel.  This requires some trickery.

  status = kernelMultitaskerSpawn(makeFreeBitmap, 
	  "building FAT free cluster list", 1, (void *) &fatData);

  if (status < 0)
    {
      // Oops.  Something went wrong.
      kernelError(kernel_error, "Unable to make free-cluster bitmap");
      // Attempt to free all the memory
      kernelMemoryReleaseByPointer(fatData->fatBuffer);
      kernelMemoryReleaseByPointer((void *) fatData);
      return (fatData = NULL);
    }

  // Read the disk's root directory and attach it to the filesystem structure
  status = readRootDir(fatData, filesystem);

  if (status < 0)
    {
      // Oops.  Something went wrong.
      kernelError(kernel_error, 
		  "Unable to read the filesystem's root directory");
      // Attempt to free all the memory
      kernelMemoryReleaseByPointer(fatData->fatBuffer);
      kernelMemoryReleaseByPointer((void *) fatData);
      return (fatData = NULL);
    }

  // Everything went right.  Looks like we will have a legitimate new
  // bouncing baby FAT filesystem.

  // Attach our new FS data to the filesystem structure
  filesystem->filesystemData = (void *) fatData;

  // Specify the filesystem block size
  filesystem->blockSize = 
    (fatData->bytesPerSector * fatData->sectorsPerCluster);

  // kernelTextPrint("OK buffering filesystem info (");
  // kernelTextPrintInteger(filesystemsBuffered);
  // kernelTextPrintLine(")");

  return (fatData);
}


static fatInternalData *getFatData(kernelFilesystem *filesystem)
{
  // This function updates the saved information about the 
  // requested disk.  It checks whether the disk NEEDS to have
  // its information updated (i.e. Have we already cached information
  // about this filesystem?).  Returns a pointer to the filsystem data
  // on success, NULL otherwise.

  // This is only used internally, so there is no need to check the
  // disk object.  The functions that are exported will do this.
  
  fatInternalData *fatData = NULL;

  
  kernelDebugEnter();

  // We must test to see whether we have already cached information about
  // the given filesystem.
  fatData = filesystem->filesystemData;

  // Did we find the filesystem in our list?  If so, we don't
  // need to do anything else.  The fatData pointer should already
  // point to it.
  if (fatData != NULL)
    // We've seen this one before.  
    return (fatData);

  // If we fall through to here, we have to read the disk info using the 
  // fatUpdate function.
  fatData = fatUpdate(filesystem);

  if (fatData == NULL)
    {
      kernelError(kernel_error, "FAT update failed");
      return (fatData = NULL);
    }
 
  return (fatData);
}


static int initEntryDataList(void)
{
  // This function is used to initialize (or re-initialize) the list
  // of FAT file entry data

  int status = 0;
  int count;


  kernelDebugEnter();

  // Allocate memory for fatEntryData structures
  entryDataMemory = kernelMemoryRequestSystemBlock((sizeof(fatEntryData) * 
			    MAX_BUFFERED_FILES), 0, "FAT private file data");

  if (entryDataMemory == NULL)
    {
      kernelError(kernel_error, 
		  "Error allocating memory for fat entry data lists");
      return (status = ERR_MEMORY);
    }

  // Initialize all of the fatEntryData structures.

  for (count = 0; count < MAX_BUFFERED_FILES; count ++)
    entryDatas[count] = &entryDataMemory[count];

  // Reset the number of used files and directories to 0
  usedEntryDatas = 0;

  return (status = 0);
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
//  Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


int kernelFilesystemTypeFatDetect(const kernelDiskObject *theDisk)
{
  // This function is used to determine whether the data on a disk object
  // is using a FAT filesystem.  It uses a simple test or two to determine
  // simply whether this is a FAT volume.  Any data that it gathers is
  // discarded when the call terminates.  It returns 1 for true, 0 for false, 
  // and negative if it encounters an error

  int status = 0;
  unsigned char bootSector[FAT_MAX_SECTORSIZE];
  unsigned int temp = 0;


  kernelDebugEnter();

  // Make sure the disk object isn't NULL
  if (theDisk == NULL)
    {
      kernelError(kernel_error, "NULL disk object");
      return (status = ERR_NULLPARAMETER);
    }

  // We can start by reading the first sector of the volume
  // (the "boot sector").

  // Call the function that reads the boot sector
  status = readBootSector((kernelDiskObject *) theDisk, 
			  bootSector);

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
  temp = (unsigned int) ((bootSector[12] << 8) + bootSector[11]);
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


int kernelFilesystemTypeFatCheck(const kernelDiskObject *theDisk)
{
  // This function performs a check of the FAT filesystem on the selected
  // disk.

  int status = 0;


  kernelDebugEnter();

  // Make sure the disk object isn't NULL
  if (theDisk == NULL)
    {
      kernelError(kernel_error, "NULL disk object");
      return (status = ERR_NULLPARAMETER);
    }

  kernelLog("Checking FAT filesystem... ");

  // Make sure there's really a FAT filesystem on the disk
  if (!kernelFilesystemTypeFatDetect(theDisk))
    {
      kernelTextNewline();
      kernelError(kernel_error,
		  "Disk object to check does not contain a FAT filesystem");
      return (status = ERR_INVALID);
    }

  // Return success
  return (status = 0);
}


int kernelFilesystemTypeFatMount(kernelFilesystem *filesystem)
{
  // This function initializes the filesystem driver by gathering all
  // of the required information from the boot sector.  In addition, 
  // it dynamically allocates memory space for the "used" and "free"
  // file and directory structure arrays.
  
  int status = 0;

  
  kernelDebugEnter();

  // Make sure the filesystem isn't NULL
  if (filesystem == NULL)
    {
      kernelError(kernel_error, "NULL filesystem object");
      return (status = ERR_NULLPARAMETER);
    }

  // If this is the first FAT filesystem we're mounting, initialize
  // the list we use for file entry data
  if (entryDataMemory == NULL)
    {
      status = initEntryDataList();

      if (status < 0)
	{
	  kernelError(kernel_error, "Error making entry data list");
	  return (status);
	}
    }

  // FAT filesystems are case preserving, but case insensitive.  Yuck.
  filesystem->caseInsensitive = 1;

  // Get the FAT data for the requested filesystem.  We don't need
  // the info right now -- we just want to collect it.
  if (getFatData(filesystem) == NULL)
    return (status = ERR_BADDATA);

  // Return success
  return (status = 0);
}


int kernelFilesystemTypeFatSync(kernelFilesystem *filesystem)
{
  // This function will initialize a synchronization of the filesystem
  // on the disk object.

  int status = 0;
  fatInternalData *fatData = NULL;


  kernelDebugEnter();

  // Check the filesystem pointer before proceeding
  if (filesystem == NULL)
    {
      kernelError(kernel_error, "NULL filesystem object");
      return (status = ERR_NULLPARAMETER);
    }

  // Get the FAT data for the requested filesystem
  fatData = getFatData(filesystem);
  // Make sure there wasn't a problem
  if (fatData == NULL)
    return (status = ERR_BADDATA);

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

  // Syncronize the FAT

  status = flushFat(fatData);

  if (status < 0)
    {
      kernelError(kernel_error, "Error flushing FAT table");
      return (status);
    }

  // Return success
  return (status = 0);
}


int kernelFilesystemTypeFatUnmount(kernelFilesystem *filesystem)
{
  // This function releases all of the stored information about a given
  // filesystem.

  int status = 0;
  fatInternalData *fatData = NULL;


  kernelDebugEnter();

  // Check the filesystem pointer before proceeding
  if (filesystem == NULL)
    {
      kernelError(kernel_error, "NULL filesystem object");
      return (status = ERR_NULLPARAMETER);
    }

  // Get the FAT data for the requested filesystem
  fatData = getFatData(filesystem);
  // Make sure there wasn't a problem
  if (fatData == NULL)
    return (status = ERR_BADDATA);

  // Whomoever is calling this function may have carefully synchronized
  // the filesystem already.  However, we will double-check it here
  // (there's not much overhead to this if things are synchronized already)
  status = kernelFilesystemTypeFatSync(filesystem);
  if (status < 0)
    {
      // Crap.  We couldn't sync the filesystem.  We'd better stop
      // this shutdown to give the user a chance to recover their data.
      kernelError(kernel_error, "Couldn't synchronize filesystem");
      return (status);
    }

  // Everything should be cozily tucked away now.  We can safely
  // discard the information we have cached about this filesystem.

  // Deallocate the memory we've been using for this filesystem
  status = kernelMemoryReleaseByPointer(fatData->fatBuffer);
  if (status < 0)
    // Crap.  We couldn't deallocate the memory.  Make a warning.
    kernelError(kernel_warn, "error deallocating FAT sector buffer");

  fatData->fatBuffer = NULL;

  status = kernelMemoryReleaseByPointer((void *) fatData);
  if (status < 0)
    // Crap.  We couldn't deallocate the memory.  Make a warning.
    kernelError(kernel_warn, "error deallocating FAT filesystem data");

  // Finally, remove the reference from the filesystem structure
  filesystem->filesystemData = NULL;

  return (status = 0);
}


unsigned int kernelFilesystemTypeFatGetFreeBytes(kernelFilesystem *filesystem)
{
  // This function returns the amount of free disk space, in bytes.

  fatInternalData *fatData = NULL;


  kernelDebugEnter();

  // Check the filsystem pointer before proceeding
  if (filesystem == NULL)
    {
      kernelError(kernel_error, "NULL filesystem object");
      return (0);
    }

  // Get the FAT data for the requested filesystem
  fatData = getFatData(filesystem);

  // Make sure there wasn't a problem
  if (fatData == NULL)
    return (0);

  // OK, now we can return the requested info
  return ((unsigned int)(fatData->bytesPerSector * 
			 fatData->sectorsPerCluster) * fatData->freeClusters);
}


int kernelFilesystemTypeFatNewEntry(kernelFileEntry *newEntry)
{
  // This function gets called when there's a new kernelFileEntry in the
  // filesystem (either because a file was created or because some existing
  // thing has been newly read from disk).  This gives us an opportunity
  // to attach FAT-specific data to the file entry

  int status = 0;


  kernelDebugEnter();

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

  // Get a private data structure for FAT-specific information
  newEntry->fileEntryData = (void *) newEntryData();

  // s'ok?
  if (newEntry->fileEntryData == NULL)
    return (status = ERR_NOCREATE);

  // Return success
  return (status = 0);
}


int kernelFilesystemTypeFatInactiveEntry(kernelFileEntry *inactiveEntry)
{
  // This function gets called when a kernelFileEntry is about to be
  // deallocated by the system (either because a file was deleted or because
  // the entry is simply being unbuffered).  This gives us an opportunity
  // to deallocate our FAT-specific data from the file entry

  int status = 0;


  kernelDebugEnter();

  // Make sure the file entry pointer isn't NULL
  if (inactiveEntry == NULL)
    {
      kernelError(kernel_error, "NULL file entry");
      return (status = ERR_NULLPARAMETER);
    }

  if (inactiveEntry->fileEntryData == NULL)
    {
      kernelError(kernel_error, "File entry has no private filesystem data");
      return (status = ERR_ALREADY);
    }

  // Release the entry data structure attached to this file entry
  releaseEntryData(inactiveEntry->fileEntryData);

  // Remove the reference
  inactiveEntry->fileEntryData = NULL;

  // Return success
  return (status = 0);
}


int kernelFilesystemTypeFatReadFile(kernelFileEntry *theFile,
				    unsigned int blockNum,
				    unsigned int blocks,
				    unsigned char *buffer)
{
  // This function is the "read file" function that the filesystem
  // driver exports to the world.  It is mainly a wrapper for the
  // internal function of the same purpose, but with some additional
  // argument checking.  Returns 0 on success, negative otherwise.

  int status = 0;
  kernelFilesystem *filesystem = NULL;
  fatInternalData *fatData = NULL;

  
  kernelDebugEnter();

  // Make sure the file entry isn't NULL
  if (theFile == NULL)
    {
      kernelError(kernel_error, "NULL file entry");
      return (status = ERR_NULLPARAMETER);
    }

  // Make sure the requested read size is more than 0 blocks
  if (blocks == 0)
    // Don't return an error, just finish
    return (status = 0);

  // Make sure the buffer isn't NULL
  if (buffer == NULL)
    {
      kernelError(kernel_error, "NULL data buffer");
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
      kernelError(kernel_error, "NULL filesystem object");
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


int kernelFilesystemTypeFatWriteFile(kernelFileEntry *theFile,
				     unsigned int skipPages, 
				     unsigned int pages,
				     unsigned char *buffer)
{
  // This function is the "write file" function that the filesystem
  // driver exports to the world.  It is mainly a wrapper for the
  // internal function of the same purpose, but with some additional
  // argument checking.  Returns 0 on success, negative otherwise.

  int status = 0;
  kernelFilesystem *filesystem = NULL;
  fatInternalData *fatData = NULL;

  
  kernelDebugEnter();

  // Make sure the file entry isn't NULL
  if (theFile == NULL)
    {
      kernelError(kernel_error, "NULL file entry");
      return (status = ERR_NULLPARAMETER);
    }

  // Make sure the requested write size is more than 0 pages
  if (pages == 0)
    // Don't return an error, just finish
    return (status = 0);

  // Make sure the buffer isn't NULL
  if (buffer == NULL)
    {
      kernelError(kernel_error, "NULL data buffer");
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
      kernelError(kernel_error, "NULL filesystem object");
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
  status = write(fatData, theFile, skipPages, pages, buffer);
  
  return (status);
}


int kernelFilesystemTypeFatCreateFile(kernelFileEntry *theFile)
{
  // This function does the FAT-specific initialization of a new file.
  // There's not much more to this than getting a new entry data structure
  // and attaching it.  Returns 0 on success, negative otherwise

  int status = 0;


  kernelDebugEnter();

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

  // Make sure the file's name is legal for FAT
  if (checkFilename(theFile->fileName) < 0)
    return (status = ERR_INVALID);
      
  // Install the short alias for this file.  This is directory-dependent
  // because it assigns short names based on how many files in the
  // directory share common characters in the initial part of the filename.
  // Don't do it for '.' or '..' entries, however
  if (strcmp((char *) theFile->fileName, ".") && 
      strcmp((char *) theFile->fileName, ".."))
    status = makeShortAlias(theFile);

  if (status < 0)
    return (status);
  
  // Return success
  return (status = 0);
}


int kernelFilesystemTypeFatDeleteFile(kernelFileEntry *theFile, int secure)
{
  // This function deletes a file.  It returns 0 on success, negative
  // otherwise

  int status = 0;
  kernelFilesystem *filesystem = NULL;
  fatInternalData *fatData = NULL;
  fatEntryData *entryData = NULL;


  kernelDebugEnter();

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
      kernelError(kernel_error, "NULL filesystem object");
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


int kernelFilesystemTypeFatFileMoved(kernelFileEntry *entry)
{
  // This function is called by the filesystem manager whenever a file
  // has been moved from one place to another.  This allows us the chance
  // do to any FAT-specific things to the file that are necessary.  In our
  // case, we need to re-create the file's short alias, since this is
  // directory-dependent.

  int status = 0;


  kernelDebugEnter();

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
      kernelError(kernel_error, 
		  "Unable to generate new short filename alias");
      return (status);
    }

  // Return success
  return (status = 0);
}


int kernelFilesystemTypeFatReadDir(kernelFileEntry *directory)
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
  unsigned int dirBufferSize = 0;

  
  kernelDebugEnter();

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
      kernelError(kernel_error, "NULL filesystem object");
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

  dirBuffer = kernelMemoryRequestBlock(dirBufferSize, 0, 
				       "temporary filesystem data");

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
      kernelMemoryReleaseByPointer(dirBuffer);
      return (status);
    }

  // Call the routine to interpret the directory data
  status = scanDirectory(fatData, filesystem, directory, dirBuffer, 
				     dirBufferSize);

  // Free the directory buffer we used
  kernelMemoryReleaseByPointer(dirBuffer);

  if (status < 0)
    {
      kernelError(kernel_error, "Error parsing directory");
      return (status);
    }
	      
  // Return success
  return (status = 0);
}


int kernelFilesystemTypeFatWriteDir(kernelFileEntry *directory)
{
  // This function takes a directory entry structure and updates it 
  // appropriately on the disk volume.  On success it returns zero,
  // negative otherwise.

  int status = 0;
  kernelFilesystem *filesystem = NULL;
  fatInternalData *fatData = NULL;
  fatEntryData *entryData = NULL;
  unsigned int clusterSize = 0;
  unsigned char *dirBuffer = NULL;
  unsigned int dirBufferSize = 0;
  unsigned int directoryEntries = 0;
  unsigned int blocks = 0;


  kernelDebugEnter();

  // Make sure the directory entry isn't NULL
  if (directory == NULL)
    {
      kernelError(kernel_error, "NULL directory entry");
      return (status = ERR_NULLPARAMETER);
    }

  // Get the private FAT data structure attached to this file entry
  entryData = (fatEntryData *) directory->fileEntryData;

  if (entryData == NULL)
    {
      kernelError(kernel_error, "NULL private file data");
      return (status = ERR_NODATA);
    }

  // Get the filesystem pointer
  filesystem = (kernelFilesystem *) directory->filesystem;

  if (filesystem == NULL)
    {
      kernelError(kernel_error, "NULL filesystem object");
      return (status = ERR_BADADDRESS);
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
  if (directory == (kernelFileEntry *) filesystem->filesystemRoot)
    dirBufferSize = (fatData->rootDirSectors * fatData->bytesPerSector);
  else
    {
      // Figure out how many directory entries there are
      directoryEntries = dirRequiredEntries(directory);

      // Calculate the size of a cluster in this filesystem
      clusterSize = (fatData->bytesPerSector * fatData->sectorsPerCluster);
  
      if (clusterSize == 0)
	{
	  // This volume would appear to be corrupted
	  kernelError(kernel_error, FAT_BAD_VOLUME);
	  return (status = ERR_BADDATA);
	}

      dirBufferSize = (directoryEntries * FAT_BYTES_PER_DIR_ENTRY);

      if ((dirBufferSize % clusterSize) != 0)
	dirBufferSize += (clusterSize - (dirBufferSize % clusterSize));

      // Calculate the new number of blocks that will be occupied by
      // this directory
      blocks = (dirBufferSize / 
		(fatData->sectorsPerCluster * fatData->bytesPerSector));
      if (dirBufferSize % 
	  (fatData->sectorsPerCluster * fatData->bytesPerSector))
	blocks += 1;
      
      // If the new number of blocks is less than the previous value, we
      // should deallocate all of the directory's existing clusters and
      // re-write it from scratch

      if (blocks < directory->blocks)
	{
	  status = releaseClusterChain(fatData, entryData->startCluster);

	  if (status < 0)
	    // Don't quit; this might not be fatal.  Make a warning message
	    kernelError(kernel_warn, "Unable to shorten directory");
	}

      // Set the directory structure's file size  and blocks
      directory->size = dirBufferSize;
      directory->blocks = blocks;
    }

  // Allocate temporary space for the directory buffer
  dirBuffer =
    kernelMemoryRequestBlock(dirBufferSize, 0, "temporary filesystem data");

  // Make sure it's not NULL
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
      kernelMemoryReleaseByPointer(dirBuffer);
      return (status);
    }

  // Write the directory "file".  If it's the root dir we do a special
  // version of this write.
  if (directory == (kernelFileEntry *) filesystem->filesystemRoot)
    {
      // If the filesystem is on removable media, check to make sure that 
      // it hasn't changed unexpectedly
      if (fatData->diskObject->fixedRemovable == removable)
	if (checkRemove(fatData))
	  {
	    // The media has changed unexpectedly.  Make an error
	    kernelError(kernel_error, FAT_MEDIA_REMOVED);
	    kernelMemoryReleaseByPointer(dirBuffer);
	    return (status = ERR_IO);
	  }

      // Obtain a lock on the disk device
      status = kernelDiskFunctionsLockDisk(fatData->diskObject->diskNumber);

      if (status < 0)
	{
	  // We couldn't lock the disk object
	  kernelError(kernel_error, "Unable to lock disk object");
	  kernelMemoryReleaseByPointer(dirBuffer);
	  return (status);
	}

      status = 
	kernelDiskFunctionsWriteSectors(fatData->diskObject->diskNumber, 
		(fatData->reservedSectors + (fatData->fatSectors 
	     * fatData->numberOfFats)), fatData->rootDirSectors, dirBuffer);

      // Release our lock on the disk object      
      kernelDiskFunctionsUnlockDisk(fatData->diskObject->diskNumber);

      if (status < 0)
	{
	  kernelError(kernel_error, "Error writing root dir");
	  kernelMemoryReleaseByPointer(dirBuffer);
	  return (status);
	}
    }
  else
    {
      status = write(fatData, directory, 0, directory->blocks, dirBuffer);

      if (status < 0)
	{
	  kernelError(kernel_error, "Error writing directory");
	  kernelMemoryReleaseByPointer(dirBuffer);
	  return (status);
	}
    }

  // De-allocate the directory buffer
  kernelMemoryReleaseByPointer(dirBuffer);

  return (status = 0);
}


int kernelFilesystemTypeFatMakeDir(kernelFileEntry *directory)
{
  // This function is used to create a directory on disk.  The caller will
  // create the file entry data structures, and it is simply the
  // responsibility of this function to make the on-disk structures reflect
  // the new entry.  It returns 0 on success, negative otherwise.

  int status = 0;
  kernelFilesystem *filesystem = NULL;
  fatInternalData *fatData = NULL;
  fatEntryData *dirData = NULL;
  kernelFileEntry *dotDir = NULL;
  kernelFileEntry *dotDotDir = NULL;
  fatEntryData *dotData = NULL;
  fatEntryData *dotDotData = NULL;
  unsigned int newCluster = 0;


  kernelDebugEnter();

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
      kernelError(kernel_error, "NULL filesystem object");
      return (status = ERR_BADADDRESS);
    }

  // Get the FAT data for the requested filesystem
  fatData = getFatData(filesystem);

  // Make sure there wasn't a problem
  if (fatData == NULL)
    return (status = ERR_BADDATA);

  // Make sure the file name is legal for FAT
  status = checkFilename(directory->fileName);

  if (status < 0)
    {
      kernelError(kernel_error, "File name is illegal in FAT filesystems");
      return (status);
    }

  // Make sure there are '.' and '..' entries in the directory, and that
  // they also have private FAT data attached to them
  dotDir = (kernelFileEntry *) directory->firstFile;
  if ((dotDir == NULL) || 
      (strcmp((char *) dotDir->fileName, ".") != 0) ||
      (dotDir->fileEntryData == NULL))
    {
      kernelError(kernel_error, "Directory has no '.' entry");
      return (status = ERR_NOSUCHDIR);
    }
  dotDotDir = (kernelFileEntry *) dotDir->nextEntry;
  if ((dotDotDir == NULL) || 
      (strcmp((char *) dotDotDir->fileName, "..") != 0) ||
      (dotDotDir->fileEntryData == NULL))
    {
      kernelError(kernel_error, "Directory has no '..' entry");
      return (status = ERR_NOSUCHDIR);
    }

  // Allocate a new, single cluster for this new directory
  status = getUnusedClusters(fatData, 1, &newCluster);

  if (status < 0)
    {
      kernelError(kernel_error, "No more free clusters");
      return (status);
    }

  // Set the sizes on the new directory entries

  directory->blocks = 1;
  directory->size = (fatData->sectorsPerCluster * fatData->bytesPerSector);

  dotDir->blocks = directory->blocks;
  dotDir->size = directory->size;

  dotDotDir->blocks =
    ((kernelFileEntry *) directory->parentDirectory)->blocks;
  dotDotDir->size = ((kernelFileEntry *) directory->parentDirectory)->size;


  // Set the applicable private data on the new directory entries

  dirData = directory->fileEntryData;
  dotData = dotDir->fileEntryData;
  dotDotData = dotDotDir->fileEntryData;

  // Set all the appropriate attributes in the directory's private data
  dirData->attributes = (FAT_ATTRIB_ARCHIVE | FAT_ATTRIB_SUBDIR);
  dirData->res = 0;
  dirData->timeTenth = 0;
  dirData->startCluster = newCluster;

  dotData->attributes = FAT_ATTRIB_SUBDIR;
  dotData->res = 0;
  dotData->timeTenth = 0;
  dotData->startCluster = dirData->startCluster;

  dotDotData->attributes = FAT_ATTRIB_SUBDIR;
  dotDotData->res = 0;
  dotDotData->timeTenth = 0;
  dotDotData->startCluster = 
    ((fatEntryData *) ((kernelFileEntry *)
	       directory->parentDirectory)->fileEntryData)->startCluster;

  // Make the short aliases
  makeShortAlias(directory);
  strncpy((char *) dotData->shortAlias, ".          ", 11);
  strncpy((char *) dotDotData->shortAlias, "..         ", 11);

  return (status = 0);
}


int kernelFilesystemTypeFatRemoveDir(kernelFileEntry *directory)
{
  // This function deletes a directory, but only if it is empty.  
  // It returns 0 on success, negative otherwise

  int status = 0;
  kernelFilesystem *filesystem = NULL;
  fatInternalData *fatData = NULL;
  fatEntryData *entryData = NULL;


  kernelDebugEnter();

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
      kernelError(kernel_error, "NULL filesystem object");
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


int kernelFilesystemTypeFatTimestamp(kernelFileEntry *theFile)
{
  // This function does FAT-specific stuff for time stamping a file.

  int status = 0;
  kernelFilesystem *filesystem = NULL;
  fatInternalData *fatData = NULL;
  fatEntryData *entryData = NULL;


  kernelDebugEnter();

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
      kernelError(kernel_error, "NULL filesystem object");
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
