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
//  kernelDiskFunctions.c
//
	
// This file contains boilerplate functions for disk access, and routines
// for managing the array of disks in the kernel's data structure for
// such things.  

#include "kernelDiskFunctions.h"
#include "kernelParameters.h"
#include "kernelResourceManager.h"
#include "kernelMemoryManager.h"
#include "kernelPageManager.h"
#include "kernelMultitasker.h"
#include "kernelMiscAsmFunctions.h"
#include "kernelSysTimerFunctions.h"
#include "kernelError.h"
#include <sys/errors.h>
#include <stdio.h>
#include <string.h>


extern int kernelBootDisk;

// Probably should implement this as a linked list.  Nah, it's small.
static kernelDiskObject *diskObjectArray[MAXDISKDEVICES];
static volatile int diskObjectCounter = 0;


static int checkObjectAndDriver(kernelDiskObject *theDisk, char *invokedBy)
{
  // This routine consists of a couple of things commonly done by the
  // other driver wrapper routines.  I've done this to minimize the amount
  // of duplicated code.  As its argument, it takes the name of the 
  // routine that called it so that any error messages can better reflect
  // what was really being done when the error occurred.

  int status = 0;

  // Make sure the disk object isn't NULL (which could indicate that the
  // device has not been properly registered)
  if (theDisk == NULL)
    {
      // Make the error
      kernelError(kernel_error, "Disk object disk is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  // Make sure that the disk object has a non-NULL driver
  if (theDisk->deviceDriver == NULL)
    {
      // Make the error
      kernelError(kernel_error, "Disk driver is NULL");
      return (status = ERR_NOSUCHDRIVER);
    }

  return (status = 0);
}


static int recalibrate(kernelDiskObject *theDisk)
{
  // This is the generic disk "calibrate drive" routine which invokes 
  // the driver routine designed for that function.  Normally it simply 
  // returns the status as returned by the driver routine, unless
  // there is an error, in which case it returns negative

  int status = 0;

  // Make sure the device driver recalibrate routine has been installed
  if (theDisk->deviceDriver->driverRecalibrate == NULL)
    {
      // Make the error
      kernelError(kernel_error, "Driver routine is NULL");
      return (status = ERR_NOSUCHFUNCTION);
    }

  // Ok, now we can call the routine.
  status = theDisk->deviceDriver
    ->driverRecalibrate(theDisk->driverDiskNumber);

  // Make sure the driver routine didn't return an error code
  if (status < 0)
    {
      // Make the error
      kernelError(kernel_error, "Recalibrate error: %s",
		  theDisk->deviceDriver->driverLastErrorMessage());
      return (theDisk->deviceDriver->driverLastErrorCode());
    }

  return (status = 0);
}


static int getTransferArea(kernelDiskObject *theDisk)
{
  // This routine should (MUST) be called after the memory management
  // routine has been run, and after all of the other disk objects
  // have been installed.

  int status = 0;

  // Check the disk object and device driver before proceeding
  status = checkObjectAndDriver(theDisk, __FUNCTION__);
  if (status < 0)
    // Something went wrong, so we can't continue.   Return the status
    // that the routine gave us, since it tells whether the disk or the 
    // driver was the source of the problem
    return (status);  

  // Call the memory management routine to allocate some memory
  // for the disk transfer area.  Get a 64K-aligned block of memory
  // for each floppy device.  It should be the size of one complete track
  // (heads * sectors * sectorSize).  For fixed disks, get a 128K
  // transfer area.  It does not need to be aligned.

  if (theDisk->fixedRemovable == fixed)
    {
      theDisk->transferAreaSize = (unsigned) (128 * 1024);
      theDisk->transferArea = 
	kernelMemoryRequestSystemBlock(theDisk->transferAreaSize, 
				 0, "disk transfer area");
      
      // Make sure it's not NULL
      if (theDisk->transferArea == NULL)
	{
	  // Make the error
	  kernelError(kernel_error, "Unable to allocate memory for transfer "
		      "areas");
	  return (status = ERR_MEMORY);
	}
	  
      theDisk->transferAreaPhysical = NULL;
    }
  else
    {
      theDisk->transferAreaSize = (unsigned) 
	(theDisk->heads * theDisk->sectors * theDisk->sectorSize);
	  
      // We need to get a physical memory address to pass to the
      // DMA controller.  Therefore, we ask the memory manager 
      // specifically for the physical address.  We  
      theDisk->transferAreaPhysical = 
	kernelMemoryRequestPhysicalBlock(theDisk->transferAreaSize, 
				 TRANSFER_AREA_ALIGN, "disk transfer area");

      // Make sure it's not NULL
      if (theDisk->transferAreaPhysical == NULL)
	{
	  // Make the error
	  kernelError(kernel_error, "Unable to allocate memory for transfer "
		      "areas");
	  return (status = ERR_MEMORY);
	}

      // Map it into the kernel's address space
      status = 
	kernelPageMapToFree(KERNELPROCID, theDisk->transferAreaPhysical,
			    (void **) &(theDisk->transferArea),
			    theDisk->transferAreaSize);

      // Make sure it's not NULL
      if (status < 0)
	{
	  // Make the error
	  kernelError(kernel_error, "Unable to allocate memory for transfer "
		      "areas");
	  return (status);
	}

      // Clear it out, since the kernelMemoryRequestPhysicalBlock()
      // routine doesn't do it for us
      kernelMemClear(theDisk->transferArea, theDisk->transferAreaSize);
    }
  
  // Return success
  return (status = 0);
}


static int readWriteSectors(kernelDiskObject *theDisk, unsigned logicalSector,
			    unsigned numSectors, void *dataPointer,
			    kernelDiskOp readWrite)
{
  // This is the combined "read sectors" and "write sectors" routine 
  // which invokes the driver routines designed for those functions.  
  // If an error is encountered, the function returns negative.  
  // Otherwise, it returns the number of sectors it actually read or
  // wrote.  This should not be exported, and should not be called by 
  // users.  Users should call the routines kernelDiskFunctionsReadSectors
  // and kernelDiskFunctionsWriteSectors which in turn call this routine.

  int status = 0;
  int retryCount = 0;
  unsigned head = 0;
  unsigned cylinder = 0;
  unsigned sector = 0;
  unsigned currentHead = 0;
  unsigned currentCylinder = 0;
  unsigned currentSector = 0;
  unsigned currentLogical = 0;
  unsigned remainingSectors = 0;
  unsigned doSectors = 0;
  unsigned maxSectors = 0;
  void *transferArea = NULL;
  char errorBuff[512];

  // Now make sure the appropriate device driver routine has been installed
  if (((readWrite == readoperation) &&
       (theDisk->deviceDriver->driverReadSectors == NULL)) ||
      ((readWrite == writeoperation) &&
       (theDisk->deviceDriver->driverWriteSectors == NULL)))
    {
      // Make the error
      kernelError(kernel_error, "Driver routine is NULL");
      return (status = ERR_NOSUCHFUNCTION);
    }

  // Check that the disk's transfer area has been properly initialized.
  // If not, then it hasn't been used previously, and we'll have to allocate
  // one.
  if ((theDisk->transferArea == NULL) || (theDisk->transferAreaSize == NULL))
    {
      status = getTransferArea(theDisk);

      if (status < 0)
	{
	  // Ack, no transfer area
	  kernelError(kernel_error, "Unable to allocate memory for transfer "
		      "areas");
	  return (status);
	}
    }

  // Check the parameters we've got to make sure they're legal
  // for this disk object.

  // Make sure we've been actually been told to read one or more sectors
  if (numSectors == 0)
    // Don't make a kernelError.  Just return an error code
    return (status = ERR_INVALID);
  
  // Should we use LBA for this operation?
  if (theDisk->addressingMethod == addr_lba)
    {
      // Make sure we're starting at the beginning of the logical volume.
      logicalSector = (theDisk->startLogicalSector + logicalSector);
      
      // Make sure the logical sector number does not exceed the number
      // of logical sectors on this volume
      if (logicalSector >= 
	  (theDisk->startLogicalSector + theDisk->logicalSectors))
	{
	  // Make a kernelError.
	  kernelError(kernel_error, "Logical sector exceeds volume boundary");
	  return (status = ERR_BOUNDS);
	}

      // To save confusion later, make sure that the rest of the CHS values 
      // are all zero
      head = 0;
      cylinder = 0;
      sector = 0;
    }
  else
    {
      // Calculate the physical head, track and sector to use
      if ((theDisk->sectors != 0) && (theDisk->heads != 0))
	{
	  head = 
	    ((logicalSector % (theDisk->sectors * theDisk->heads)) 
	     / theDisk->sectors);
	  cylinder = (logicalSector / (theDisk->sectors * theDisk->heads));
	  sector = (logicalSector % theDisk->sectors) + 1;
	}
      else
	return (status = ERR_BADADDRESS);
      
      // We will be using P-CHS.  Make sure the head, track, and cylinder
      // are within the legal range of values

      if ((sector > theDisk->sectors) ||
	  (cylinder >= theDisk->cylinders) ||
	  (head >= theDisk->heads))
	// Don't make a kernelError.  Just return an error code
	return (status = ERR_BADADDRESS);

      // To save confusion later, make sure that the LBA value is zero
      logicalSector = 0;
    }


  // If this is a removable device, we will have to make sure its motor
  // is turned on
  if ((theDisk->fixedRemovable == removable) && (theDisk->motorStatus == 0))
    {
      // Turn the drive motor on
      status = kernelDiskFunctionsMotorOn(theDisk->diskNumber);

      if (status < 0)
	{
	  // Make the error
	  kernelError(kernel_error, "Motor on error: %s",
		      theDisk->deviceDriver->driverLastErrorMessage());
	  return (theDisk->deviceDriver->driverLastErrorCode());
	}
      
      // We don't have to wait for the disk to spin up on a read 
      // operation;  It will start reading when it's good and ready.
      // If it's a write operation we have to wait for it.
      if (readWrite == writeoperation)
	// Wait half a second for the drive to spin up
	kernelMultitaskerWait(10);
    }

  // Where should we transfer the data to/from?  If this is a floppy disk,
  // we will need to tell it a phsical address because of the DMA it uses.
  // Otherwise, we will use the virtual address of the transfer area
  if (theDisk->type == floppy)
    transferArea = theDisk->transferAreaPhysical;
  else
    transferArea = theDisk->transferArea;

  // What is the maximum number of sectors we can transfer at one time?
  // This is dependent on the disk's sector size (512, usually).
  maxSectors = theDisk->maxSectorsPerOp;

  // Set the initial currentHead, currentCylinder, currentSector 
  // and currentLogical values
  currentHead = head;
  currentCylinder = cylinder;
  currentSector = sector;
  currentLogical = logicalSector;

  // Make doSectors be zero to start
  doSectors = 0;

  // Now we start the actual read/write operation

  // This loop will ensure that we do not try to transfer more than
  // maxSectors per operation.
  for (remainingSectors = numSectors; remainingSectors > 0; )
    {
      // Figure out the number of sectors to transfer in this pass.
      if (remainingSectors > maxSectors)
	doSectors = maxSectors;
      else
	doSectors = remainingSectors;

      // If this is a floppy disk, we don't want to cross a cylinder
      // boundary in one operation.  Some floppy controllers can't do this.
      if (theDisk->type == floppy)
	{
	  if (((currentHead * theDisk->sectors) + currentSector +
	       (doSectors - 1)) >
	      (theDisk->heads * theDisk->sectors))
	    doSectors = ((theDisk->sectors - currentSector) + 1);
	}

      // If it's a write operation, copy the data from the user
      // area to the disk transfer area.  This will be up to
      // transferAreaSize bytes at any one time
      if (readWrite == writeoperation)
	kernelMemCopy(dataPointer, theDisk->transferArea,
			     (theDisk->sectorSize * doSectors));

      // We attempt the basic read/write operation RETRY_ATTEMPTS times
      for (retryCount = 0; retryCount < RETRY_ATTEMPTS; retryCount ++)
	{
	  // Call the read or write routine
	  if (readWrite == readoperation)
	    status = theDisk->deviceDriver->driverReadSectors(
		      theDisk->driverDiskNumber, currentHead, 
		      currentCylinder, currentSector, currentLogical,
		      doSectors, transferArea);
	  else
	    status = theDisk->deviceDriver->driverWriteSectors(
		       theDisk->driverDiskNumber, currentHead, 
		       currentCylinder, currentSector, currentLogical,
		       doSectors, transferArea);

	  if (status >= 0)
	    break;

	  // The disk drive returned an error code.  
	  sprintf(errorBuff, (readWrite == readoperation)?
		  "Read error: %s" : "Write error: %s",
		  (char *) theDisk->deviceDriver->driverLastErrorMessage());
	  status = theDisk->deviceDriver->driverLastErrorCode();
	  
	  // Should we retry the operation?
	  if (retryCount < (RETRY_ATTEMPTS - 1))
	    {
	      // Recalibrate the disk and try again
	      if (recalibrate(theDisk) < 0)
		{
		  // Something went wrong, so we can't continue.  Make 
		  // the error, with the status and error saved by the driver
		  kernelError(kernel_error, errorBuff);
		  return (status);
		}
	    }
	  else
	    {
	      // Make the error, with the status and error saved by the driver
	      kernelError(kernel_error, errorBuff);
	      return (status);
	    }

	} // retry loop
      
      // If it's a read operation, copy the data from the
      // disk transfer area to the user area.  This will be up to
      // transferAreaSize bytes at any one time
      if (readWrite == readoperation)
	kernelMemCopy(theDisk->transferArea, dataPointer, 
			     (theDisk->sectorSize * doSectors));

      // Now, if this is a multi-part operation, we should update
      // a bunch of values.

      // Figure out what the current CHS or LBA values should be, based
      // on how many we've written previously.  If it's LBA, it's
      // easy.  CHS is a little harder.

      if (theDisk->addressingMethod == addr_lba)
	currentLogical += doSectors;

      else
	{
	  currentSector += doSectors;
	  
	  // Did we move to another head?
	  if (currentSector > theDisk->sectors)
	    {
	      currentHead += (currentSector / theDisk->sectors);
	      currentSector = (currentSector % theDisk->sectors);
	      
	      // Did we move to another cylinder?
	      if (currentHead >= theDisk->heads)
		{
		  currentCylinder += (currentHead / theDisk->heads);
		  currentHead = (currentHead % theDisk->heads);
		}
	    }
	}

      // We subtract the number read from the total number to read
      remainingSectors -= doSectors;

      // Increment the place in the buffer we're using
      dataPointer += (doSectors * theDisk->sectorSize);
      
    } // per-operation loop

  // Finished.  Return success
  return (status = 0);
}


static int readWriteAbsoluteSectors(int physicalDevice,
				    unsigned absoluteSector,
				    unsigned numSectors, void *dataPointer,
				    kernelDiskOp readWrite)
{
  // This is the combined "read absolute sectors" and "write absolute sectors"
  // routine which invokes the driver routines designed for those functions.  
  // This should not be exported, and should not be called by users.

  int status = 0;
  kernelDiskObject *tmpObject = NULL;
  int retryCount = 0;
  unsigned head = 0;
  unsigned cylinder = 0;
  unsigned sector = 0;
  unsigned currentHead = 0;
  unsigned currentCylinder = 0;
  unsigned currentSector = 0;
  unsigned currentLogical = 0;
  unsigned remainingSectors = 0;
  unsigned doSectors = 0;
  unsigned maxSectors = 0;
  char errorBuff[512];
  int count;

  // We don't want to really use any disk objects because absolute sectors
  // are not strictly "related" to the logical volumes they represent.
  // However, we will find one that shares the same physical device so that
  // we can ensure it's a fixed disk, and so that we can use the correct
  // driver and parameters, etc.
  for (count = 0; count < diskObjectCounter; count ++)
    {
      if (diskObjectArray[count]->driverDiskNumber == physicalDevice)
	tmpObject = diskObjectArray[count];
    }

  // Find it?  Is it a fixed disk?
  if ((tmpObject == NULL) || (tmpObject->fixedRemovable != fixed))
    {
      // Make an error
      kernelError(kernel_error, "Disk number is invalid");
      return (ERR_INVALID);
    }

  // Reset the 'idle since' value
  tmpObject->idleSince = kernelSysTimerRead();
  
  // Now make sure the appropriate device driver routine has been installed
  if (((readWrite == readoperation) &&
       (tmpObject->deviceDriver->driverReadSectors == NULL)) ||
      ((readWrite == writeoperation) &&
       (tmpObject->deviceDriver->driverWriteSectors == NULL)))
    {
      // Make the error
      kernelError(kernel_error, "Driver routine is NULL");
      return (status = ERR_NOSUCHFUNCTION);
    }

  // Check that the disk's transfer area has been properly initialized.
  // If not, then it hasn't been used previously, and we'll have to allocate
  // one.
  if ((tmpObject->transferArea == NULL) ||
      (tmpObject->transferAreaSize == NULL))
    {
      status = getTransferArea(tmpObject);

      if (status < 0)
	{
	  // Ack, no transfer area
	  kernelError(kernel_error, "Unable to allocate memory for transfer "
		      "areas");
	  return (status);
	}
    }

  // Make sure we've been actually been told to read one or more sectors
  if (numSectors == 0)
    // Don't make a kernelError.  Just return an error code
    return (status = ERR_INVALID);
  
  // Should we use LBA for this operation?
  if (tmpObject->addressingMethod == addr_lba)
    {
      // To save confusion later, make sure that the rest of the CHS values 
      // are all zero
      head = 0;
      cylinder = 0;
      sector = 0;
    }
  else
    {
      // Calculate the physical head, track and sector to use
      if ((tmpObject->sectors != 0) && (tmpObject->heads != 0))
	{
	  head = 
	    ((absoluteSector % (tmpObject->sectors * tmpObject->heads)) 
	     / tmpObject->sectors);
	  cylinder = (absoluteSector /
		      (tmpObject->sectors * tmpObject->heads));
	  sector = (absoluteSector % tmpObject->sectors) + 1;
	}
      else
	return (status = ERR_BADADDRESS);
      
      // We will be using P-CHS.  Make sure the head, track, and cylinder
      // are within the legal range of values

      if ((sector > tmpObject->sectors) ||
	  (cylinder >= tmpObject->cylinders) ||
	  (head >= tmpObject->heads))
	// Don't make a kernelError.  Just return an error code
	return (status = ERR_BADADDRESS);

      // To save confusion later, make sure that the LBA value is zero
      absoluteSector = 0;
    }

  // What is the maximum number of sectors we can transfer at one time?
  // This is dependent on the disk's sector size (512, usually).
  maxSectors = tmpObject->maxSectorsPerOp;

  // Set the initial currentHead, currentCylinder, currentSector 
  // and currentLogical values
  currentHead = head;
  currentCylinder = cylinder;
  currentSector = sector;
  currentLogical = absoluteSector;

  // Make doSectors be zero to start
  doSectors = 0;

  // Now we start the actual read/write operation

  // This loop will ensure that we do not try to transfer more than
  // maxSectors per operation.
  for (remainingSectors = numSectors; remainingSectors > 0; )
    {
      // Figure out the number of sectors to transfer in this pass.
      if (remainingSectors > maxSectors)
	doSectors = maxSectors;
      else
	doSectors = remainingSectors;

      // If it's a write operation, copy the data from the user
      // area to the disk transfer area.  This will be up to
      // transferAreaSize bytes at any one time
      if (readWrite == writeoperation)
	kernelMemCopy(dataPointer, tmpObject->transferArea,
		      (tmpObject->sectorSize * doSectors));

      // We attempt the basic read/write operation RETRY_ATTEMPTS times
      for (retryCount = 0; retryCount < RETRY_ATTEMPTS; retryCount ++)
	{
	  // Call the read or write routine
	  if (readWrite == readoperation)
	    status = tmpObject->deviceDriver->driverReadSectors(
		      physicalDevice, currentHead, currentCylinder,
		      currentSector, currentLogical, doSectors,
		      tmpObject->transferArea);
	  else
	    status = tmpObject->deviceDriver->driverWriteSectors(
		       physicalDevice, currentHead, currentCylinder,
		       currentSector, currentLogical, doSectors,
		       tmpObject->transferArea);

	  if (status >= 0)
	    break;

	  // The disk drive returned an error code.  
	  sprintf(errorBuff, (readWrite == readoperation)?
		  "Read error: %s" : "Write error: %s",
		  (char *) tmpObject->deviceDriver->driverLastErrorMessage());
	  status = tmpObject->deviceDriver->driverLastErrorCode();
	  
	  // Should we retry the operation?
	  if (retryCount < (RETRY_ATTEMPTS - 1))
	    {
	      // Recalibrate the disk and try again
	      if (recalibrate(tmpObject) < 0)
		{
		  // Something went wrong, so we can't continue.  Make 
		  // the error, with the status and error saved by the driver
		  kernelError(kernel_error, errorBuff);
		  return (status);
		}
	    }
	  else
	    {
	      // Make the error, with the status and error saved by the driver
	      kernelError(kernel_error, errorBuff);
	      return (status);
	    }

	} // retry loop
      
      // If it's a read operation, copy the data from the
      // disk transfer area to the user area.  This will be up to
      // transferAreaSize bytes at any one time
      if (readWrite == readoperation)
	kernelMemCopy(tmpObject->transferArea, dataPointer, 
			     (tmpObject->sectorSize * doSectors));

      // Now, if this is a multi-part operation, we should update
      // a bunch of values.

      // Figure out what the current CHS or LBA values should be, based
      // on how many we've written previously.  If it's LBA, it's
      // easy.  CHS is a little harder.

      if (tmpObject->addressingMethod == addr_lba)
	currentLogical += doSectors;

      else
	{
	  currentSector += doSectors;
	  
	  // Did we move to another head?
	  if (currentSector > tmpObject->sectors)
	    {
	      currentHead += (currentSector / tmpObject->sectors);
	      currentSector = (currentSector % tmpObject->sectors);
	      
	      // Did we move to another cylinder?
	      if (currentHead >= tmpObject->heads)
		{
		  currentCylinder += (currentHead / tmpObject->heads);
		  currentHead = (currentHead % tmpObject->heads);
		}
	    }
	}

      // We subtract the number read from the total number to read
      remainingSectors -= doSectors;

      // Increment the place in the buffer we're using
      dataPointer += (doSectors * tmpObject->sectorSize);
      
    } // per-operation loop

  // Reset the 'idle since' value
  tmpObject->idleSince = kernelSysTimerRead();
  
  // Finished.  Return success
  return (status = 0);
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


int kernelDiskFunctionsRegisterDevice(kernelDiskObject *theDisk)
{
  // This routine will register a new disk object.  It takes a 
  // kernelDiskObject structure and returns the disk number of the
  // new disk.  On error, it returns negative

  int status = 0;
  int diskId = 0;

  if (theDisk == NULL)
    {
      // Make the error
      kernelError(kernel_error, "Disk object disk is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  // We think it's an OK pointer.  So, we should make sure the array of
  // disk objects isn't full
  if (diskObjectCounter >= (MAXDISKDEVICES - 1))
    {
      // Make the error
      kernelError(kernel_error, "Max disk objects already registered");
      return (status = ERR_NOFREE);
    }

  // Alright.  We'll put the device at the end of the list
  diskObjectArray[diskObjectCounter] = theDisk;
  
  // Make the disk's Id number be the same as its index in the 
  // array of disks, and increment the counter
  diskId = diskObjectCounter++;

  // Set the disk's disk number
  theDisk->diskNumber = diskId;

  // Reset the 'idle since' value
  theDisk->idleSince = kernelSysTimerRead();
  
  return (diskId);
}


int kernelDiskFunctionsInstallDriver(kernelDiskObject *theDisk, 
				     kernelDiskDeviceDriver *theDriver)
{
  // Attaches a driver object to a disk object.  If the operation is
  // successful, the routine returns 0.  Otherwise, it returns negative.

  int status = 0;

  // Make sure the disk object isn't NULL
  if (theDisk == NULL)
    {
      // Make the error
      kernelError(kernel_error, "Disk number is invalid");
      return (status = ERR_NULLPARAMETER);
    }

  // Make sure the driver isn't NULL
  if (theDriver == NULL)
    {
      // Make the error
      kernelError(kernel_error, "Disk driver is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  // Install the device driver
  theDisk->deviceDriver = theDriver;

  return (status = 0);
}


int kernelDiskFunctionsInitialize(void)
{
  // This is the "initialize" routine which invokes  the driver routine 
  // designed for that function.  Normally it returns zero, unless there
  // is an error.  If there's an error it returns negative.
  
  int count;
  int status = 0;
  kernelDiskObject *theDisk;

  // Check whether any disks have been registered.  If not, that's 
  // an indication that the hardware enumeration has not been done
  // properly.  We'll issue an error in this case
  if (diskObjectCounter <= 0)
    {
      // Make the error
      kernelError(kernel_error, "No disks have been registered");
      return (status = ERR_NOTINITIALIZED);
    }

  // Do a loop to step through all of the disk objects, initializing
  // and specifying each one
  for (count = 0; count < diskObjectCounter; count ++)
    {
      // Get the disk object
      theDisk = kernelFindDiskObjectByNumber(count);

      // Check the disk object and device driver before proceeding
      status = checkObjectAndDriver(theDisk, __FUNCTION__);
      if (status < 0)
	// Something went wrong, so we can't continue.   Return the status
	// that the routine gave us, since it tells whether the disk or the 
	// driver was the source of the problem
	return (status);

      // Now make sure the device driver initialize routine has been installed
      if (theDisk->deviceDriver->driverInitialize == NULL)
	{
	  // Make the error
	  kernelError(kernel_error, "Driver routine is NULL");
	  return (status = ERR_NOSUCHFUNCTION);
	}

      // Ok, now we can call the routine.
      status = theDisk->deviceDriver->driverInitialize();

      // Make sure the driver routine didn't return an error
      if (status < 0)
	{
	  // Make the error
	  kernelError(kernel_error, "Driver error: %s",
		      theDisk->deviceDriver->driverLastErrorMessage());
	  return (theDisk->deviceDriver->driverLastErrorCode());
	}

      // If it's removable, make sure the motor is off
      if (theDisk->fixedRemovable == removable)
	kernelDiskFunctionsMotorOff(theDisk->diskNumber);
    }

  return (status = 0);
}


int kernelDiskFunctionsShutdown(void)
{
  // Shut down.

  int count;

  // Loop through all the disks, looking for removable drives whose motors
  // we should ensure are turned off
  for (count = 0; count < diskObjectCounter; count ++)
    if (diskObjectArray[count]->fixedRemovable == removable)
      kernelDiskFunctionsMotorOff(diskObjectArray[count]->diskNumber);

  return (0);
}


int kernelDiskFunctionsGetBoot(void)
{
  // Returns the disk number of the boot device
  return (kernelBootDisk);
}


int kernelDiskFunctionsGetCount(void)
{
  // Returns the number of registered disk objects.  Useful for iterating
  // through calls to kernelFindDiskObjectByNumber or
  // kernelDiskFunctionsGetInfo
  return (diskObjectCounter);
}


int kernelDiskFunctionsGetInfo(int diskId, disk *theDisk)
{
  // Fills a simplified disk info structure for use by external programs,
  // since the kernelDiskObject structure is for internal kernel use

  int status = 0;
  kernelDiskObject *diskObject = NULL;

  if (theDisk == NULL)
    {
      // Make the error
      kernelError(kernel_error, "Disk object disk is NULL");
      return (status = ERR_NULLPARAMETER);
    }
 
  // Try to find the requested disk object
  diskObject = kernelFindDiskObjectByNumber(diskId);

  if (diskObject == NULL)
    // The previous call makes a kernelError
    return (status = ERR_NOSUCHENTRY);

  // Got it.  Fill in the relevant information
  theDisk->number = diskObject->diskNumber;
  theDisk->physicalDevice = diskObject->driverDiskNumber;
  strncpy(theDisk->description, diskObject->description,
	  MAX_DESCRIPTION_LENGTH);
  theDisk->type = diskObject->type;
  theDisk->fixedRemovable = diskObject->fixedRemovable;
  theDisk->startHead = diskObject->startHead;
  theDisk->startCylinder = diskObject->startCylinder;
  theDisk->startSector = diskObject->startSector;
  theDisk->startLogicalSector = diskObject->startLogicalSector;
  theDisk->heads = diskObject->heads;
  theDisk->cylinders = diskObject->cylinders;
  theDisk->sectors = diskObject->sectors;
  theDisk->logicalSectors = diskObject->logicalSectors;
  theDisk->sectorSize = diskObject->sectorSize;

  return (status = 0);
}


kernelDiskObject *kernelFindDiskObjectByNumber(int diskId)
{
  // This routine takes the number of a disk and finds it in the
  // array, returning a pointer to the disk.  If the disk number doesn't
  // exist, the function returns NULL

  kernelDiskObject *theDisk = NULL;

  // Make sure the number is valid
  if ((diskId < 0) || (diskId >= diskObjectCounter))
    {
      // Make an error
      kernelError(kernel_error, "Disk number is invalid");
      return (theDisk = NULL);
    }

  // Make sure the pointer isn't NULL before we try to access
  // its member
  if (diskObjectArray[diskId] == NULL)
    {
      // Just skip this one, I guess, but send a warning
      kernelError(kernel_error, "Disk object disk is NULL");
      return (theDisk = NULL);
    }

  if (diskObjectArray[diskId]->diskNumber == diskId)
    theDisk = diskObjectArray[diskId];
  else
    {
      // The disk numer doesn't appear to be consistent
      kernelError(kernel_error, "The disk's number is not consistent with "
		  "its array index");
      return (theDisk = NULL);
    }

  return (theDisk);
}


int kernelDiskFunctionsMotorOn(int diskId)
{
  // This is the generic disk "motor on" routine which invokes 
  // the driver routine designed for that function.  Normally it simply 
  // returns the status as returned by the driver routine, unless there
  // is an error.  If there's an error it returns negative

  int status = 0;
  kernelDiskObject *theDisk = NULL;

  // Get the disk object
  theDisk = kernelFindDiskObjectByNumber(diskId);

  // Check the disk object and device driver before proceeding
  status = checkObjectAndDriver(theDisk, __FUNCTION__);

  if (status < 0)
    // Something went wrong, so we can't continue.   Return the status
    // that the routine gave us, since it tells whether the disk or the 
    // driver was the source of the problem
    return (status);

  // Reset the 'idle since' value
  theDisk->idleSince = kernelSysTimerRead();
  
  // If it's a fixed disk, we don't need to turn the motor on, of course.
  if (theDisk->fixedRemovable == fixed)
    return (status = 0);

  // Make sure the motor isn't already on
  if (theDisk->motorStatus == 1)
    return (status = 0);

  // Now make sure the device driver motor on routine has been installed
  if (theDisk->deviceDriver->driverMotorOn == NULL)
    {
      // Make the error
      kernelError(kernel_error, "Driver routine is NULL");
      return (status = ERR_NOSUCHFUNCTION);
    }

  // Lock the disk
  status = kernelResourceManagerLock(&(theDisk->lock));
  if (status < 0)
    return (status = ERR_NOLOCK);

  // Ok, now we can call the routine.
  status = theDisk->deviceDriver->driverMotorOn(theDisk->driverDiskNumber);

  // Unlock the disk
  kernelResourceManagerUnlock(&(theDisk->lock));

  // Make sure the driver routine didn't return an error code
  if (status < 0)
    {
      // Make the error
      kernelError(kernel_error, "Motor on error: %s",
		  theDisk->deviceDriver->driverLastErrorMessage());
      return (theDisk->deviceDriver->driverLastErrorCode());
    }

  // Make note of the fact that the motor is on
  theDisk->motorStatus = 1;

  // Reset the 'idle since' value
  theDisk->idleSince = kernelSysTimerRead();
  
  return (status = 0);
}


int kernelDiskFunctionsMotorOff(int diskId)
{
  // This is the generic disk "motor off" routine which invokes 
  // the driver routine designed for that function.  If there's an error 
  // it returns negative.  

  // One important aspect of this function is that it SCHEDULES the motor to
  // be turned off after a set time period, rather that doing it 
  // immediately.  It also sets the global variable scheduledMotorOff
  // so that the motor on routine can check for it and potentially 
  // remove the event to avoid conflicts.

  // The whole point of this is that the drive motor will continue to spin
  // as long as disk accesses come in fairly quick succession.  If no 
  // accesses happen for the whole delay period, then the motor will
  // shut off

  int status = 0;
  kernelDiskObject *theDisk = NULL;

  // Get the disk object
  theDisk = kernelFindDiskObjectByNumber(diskId);

  // Check the disk object and device driver before proceeding
  status = checkObjectAndDriver(theDisk, __FUNCTION__);

  if (status < 0)
    // Something went wrong, so we can't continue.   Return the status
    // that the routine gave us, since it tells whether the disk or the 
    // driver was the source of the problem
    return (status);

  // Reset the 'idle since' value
  theDisk->idleSince = kernelSysTimerRead();
  
  // If it's a fixed disk, we don't need to turn the motor off, of course.
  if (theDisk->fixedRemovable == fixed)
    return (status = 0);

  // Make sure the motor isn't already off
  if (theDisk->motorStatus == 0)
    return (status = 0);

  // Now make sure the device driver motor off routine has been installed
  if (theDisk->deviceDriver->driverMotorOff == NULL)
    {
      // Make the error
      kernelError(kernel_error, "Driver routine is NULL");
      return (status = ERR_NOSUCHFUNCTION);
    }

  // Lock the disk
  status = kernelResourceManagerLock(&(theDisk->lock));
  if (status < 0)
    return (status = ERR_NOLOCK);

  // Ok, now turn the motor off
  theDisk->deviceDriver->driverMotorOff(theDisk->driverDiskNumber);

  // Unlock the disk
  kernelResourceManagerUnlock(&(theDisk->lock));

  // Make note of the fact that the motor is off
  theDisk->motorStatus = 0;

  // Reset the 'idle since' value
  theDisk->idleSince = kernelSysTimerRead();
  
  return (status = 0);
}


int kernelDiskFunctionsDiskChanged(int diskId)
{
  // This is the generic disk "media check" routine which invokes 
  // the driver routine designed for that function.  Normally it simply 
  // returns the status as returned by the driver routine, unless there's
  // an error, in which case it returns negative

  int status = 0;
  kernelDiskObject *theDisk = NULL;

  // Get the disk object
  theDisk = kernelFindDiskObjectByNumber(diskId);

  // Check the disk object and device driver before proceeding
  status = checkObjectAndDriver(theDisk, __FUNCTION__);

  if (status < 0)
    // Something went wrong, so we can't continue.   Return the status
    // that the routine gave us, since it tells whether the disk or the 
    // driver was the source of the problem
    return (status);

  // Reset the 'idle since' value
  theDisk->idleSince = kernelSysTimerRead();
  
  // Now make sure the device driver media check routine has been installed
  if (theDisk->deviceDriver->driverDiskChanged == NULL)
    {
      // Make the error
      kernelError(kernel_error, "Driver routine is NULL");
      return (status = ERR_NOSUCHFUNCTION);
    }

  // Lock the disk
  status = kernelResourceManagerLock(&(theDisk->lock));
  if (status < 0)
    return (status = ERR_NOLOCK);

  // The disk change line appears to only get checked when the motor
  // is turned on; thus, if the motor is off we must turn it on, then
  // off after the call
  if (theDisk->motorStatus == 0)
    {
      status = kernelDiskFunctionsMotorOn(theDisk->diskNumber);

      if (status < 0)
	{
	  // Make the error
	  kernelError(kernel_error, "Motor on error: %s",
		      theDisk->deviceDriver->driverLastErrorMessage());
	  kernelResourceManagerUnlock(&(theDisk->lock));
	  return (theDisk->deviceDriver->driverLastErrorCode());
	}
    }

  // Ok, now we can call the routine.
  status = theDisk->deviceDriver->driverDiskChanged(theDisk->driverDiskNumber);

  // The driver function should return 1 if the media has changed, 
  // and 0 if it has not changed.  We'll simply return these
  // values.

  // Make sure the driver routine didn't return an error code
  if (status < 0)
    {
      // Make the error
      kernelError(kernel_error, "Media check error: %s",
		  theDisk->deviceDriver->driverLastErrorMessage());
      kernelResourceManagerUnlock(&(theDisk->lock));
      return (theDisk->deviceDriver->driverLastErrorCode());
    }

  // Unlock the disk
  kernelResourceManagerUnlock(&(theDisk->lock));

  // Reset the 'idle since' value
  theDisk->idleSince = kernelSysTimerRead();
  
  return (status = 0);
}
 

int kernelDiskFunctionsReadSectors(int diskId, unsigned logicalSector, 
		   unsigned numSectors, void *dataPointer)
{
  // This routine is the user-accessible interface to reading data using
  // the various disk routines in this file.  Basically, it is a gatekeeper
  // that helps ensure correct use of the "read-write" method.  

  int status = 0;
  kernelDiskObject *theDisk = NULL;

  // Get the disk object
  theDisk = kernelFindDiskObjectByNumber(diskId);

  // Check the disk object and device driver before proceeding
  status = checkObjectAndDriver(theDisk, __FUNCTION__);

  if (status < 0)
    // Something went wrong, so we can't continue.   Return the status
    // that the routine gave us, since it tells whether the disk or the 
    // driver was the source of the problem
    return (status);

  // Reset the 'idle since' value
  theDisk->idleSince = kernelSysTimerRead();
  
  // Lock the disk
  status = kernelResourceManagerLock(&(theDisk->lock));
  if (status < 0)
    return (status = ERR_NOLOCK);

  // Call the read-write routine for a read operation
  status = readWriteSectors(theDisk, logicalSector, numSectors,
			    dataPointer, readoperation);

  // Unlock the disk
  kernelResourceManagerUnlock(&(theDisk->lock));

  // Reset the 'idle since' value
  theDisk->idleSince = kernelSysTimerRead();
  
  // Reset the 'idle since' value
  theDisk->idleSince = kernelSysTimerRead();
  
  return (status);
}


int kernelDiskFunctionsWriteSectors(int diskId, unsigned logicalSector, 
		    unsigned numSectors, void *dataPointer)
{
  // This routine is the user-accessible interface to writing data using
  // the various disk routines in this file.  Basically, it is a gatekeeper
  // that helps ensure correct use of the "read-write" method.  
  
  int status = 0;
  kernelDiskObject *theDisk = NULL;

  // Get the disk object
  theDisk = kernelFindDiskObjectByNumber(diskId);

  // Check the disk object and device driver before proceeding
  status = checkObjectAndDriver(theDisk, __FUNCTION__);

  if (status < 0)
    // Something went wrong, so we can't continue.   Return the status
    // that the routine gave us, since it tells whether the disk or the 
    // driver was the source of the problem
    return (status);

  // Reset the 'idle since' value
  theDisk->idleSince = kernelSysTimerRead();
  
  // Lock the disk
  status = kernelResourceManagerLock(&(theDisk->lock));
  if (status < 0)
    return (status = ERR_NOLOCK);

  // Call the read-write routine for a write operation
  status = readWriteSectors(theDisk, logicalSector, numSectors,
			    dataPointer, writeoperation);

  // Unlock the disk
  kernelResourceManagerUnlock(&(theDisk->lock));

  // Reset the 'idle since' value
  theDisk->idleSince = kernelSysTimerRead();
  
  return (status);
}
 

int kernelDiskFunctionsReadAbsoluteSectors(int physicalDevice,
					   unsigned absoluteSector, 
					   unsigned numSectors,
					   void *dataPointer)
{
  // This routine is the user-accessible interface to reading absolute sectors
  // from a physical hard disk.  Basically, it is a gatekeeper that helps
  // ensure correct use of the "read-write-absolute" method.  

  int status = 0;

  // Call the read-write routine for a read operation
  status = readWriteAbsoluteSectors(physicalDevice, absoluteSector, numSectors,
				    dataPointer, readoperation);
  return (status);
}


int kernelDiskFunctionsWriteAbsoluteSectors(int physicalDevice,
					    unsigned absoluteSector,
					    unsigned numSectors,
					    void *dataPointer)
{
  // This routine is the user-accessible interface to writing absolute sectors
  // from a physical hard disk.  Basically, it is a gatekeeper that helps
  // ensure correct use of the "read-write-absolute" method.  

  int status = 0;

  // Call the read-write routine for a read operation
  status = readWriteAbsoluteSectors(physicalDevice, absoluteSector, numSectors,
				    dataPointer, writeoperation);
  return (status);
}
