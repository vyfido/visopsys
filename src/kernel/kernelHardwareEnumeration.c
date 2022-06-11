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
//  kernelHardwareEnumeration.c
//

#include "kernelHardwareEnumeration.h"
#include "kernelParameters.h"
#include "kernelDriverManagement.h"
#include "kernelMemoryManager.h"
#include "kernelText.h"
#include "kernelMiscAsmFunctions.h"
#include "kernelLog.h"
#include "kernelError.h"
#include <sys/errors.h>
#include <string.h>


loaderInfoStruct *systemInfo = NULL;

static int kernelHardwareObjectsInitialized = 0;
static kernelProcessorObject processorDevice;
static kernelPicObject picDevice;
static kernelSysTimerObject systemTimerDevice;
static kernelRtcObject rtcDevice;
static kernelDmaObject dmaDevice;
static kernelKeyboardObject keyboardDevice;
static kernelDiskObject floppyDevices[MAXFLOPPIES];  
int numberFloppies = 0;
static kernelDiskObject hardDiskDevices[MAXHARDDISKS];  
int numberHardDisks = 0;


// These routines enumerate all of the hardware devices in the system based
// on the hardware data structure passed to the kernel by the os loader.


static int kernelHardwareEnumerateProcessorDevice(void)
{
  // This routine enumerates the system timer device.  It doesn't really
  // need enumeration; this really just registers the device and initializes
  // the functions in the abstracted driver.

  int status = 0;

  status = kernelProcessorRegisterDevice(&processorDevice);
  if (status < 0)
    return (status);
  
  status = kernelInstallProcessorDriver();
  if (status < 0)
    return (status);

  // Initialize the processor functions
  status = kernelProcessorInitialize();
  if (status < 0)
    return (status);

  return (status = 0);
}


static int kernelHardwareEnumeratePicDevice(void)
{
  // This routine enumerates the system's Programmable Interrupt Controller
  // device.  It doesn't really need enumeration; this really just registers 
  // the device and initializes the functions in the abstracted driver.

  int status = 0;


  status = kernelPicRegisterDevice(&picDevice);
  if (status < 0)
    return (status);

  status = kernelInstallPicDriver();
  if (status < 0)
    return (status);

  status = kernelPicInitialize();
  if (status < 0)
    return (status);

  return (status = 0);
}


static int kernelHardwareEnumerateSysTimerDevice(void)
{
  // This routine enumerates the system timer device.  It doesn't really
  // need enumeration; this really just registers the device and initializes
  // the functions in the abstracted driver.

  int status = 0;

  status = kernelSysTimerRegisterDevice(&systemTimerDevice);
  if (status < 0)
    return (status);

  status = kernelInstallSysTimerDriver();
  if (status < 0)
    return (status);

  // Initialize the system timer functions
  status = kernelSysTimerInitialize();
  if (status < 0)
    return (status);

  return (status = 0);
}


static int kernelHardwareEnumerateRtcDevice(void)
{
  // This routine enumerates the system's Real-Time clock device.  
  // It doesn't really need enumeration; this really just registers the 
  // device and initializes the functions in the abstracted driver.

  int status = 0;


  status = kernelRtcRegisterDevice(&rtcDevice);
  if (status < 0)
    return (status);

  status = kernelInstallRtcDriver();
  if (status < 0)
    return (status);

  // Initialize the real-time clock functions
  status = kernelRtcInitialize();
  if (status < 0)
    return (status);

  return (status = 0);
}


static int kernelHardwareEnumerateDmaDevice(void)
{
  // This routine enumerates the system's DMA controller device(s).  
  // They doesn't really need enumeration; this really just registers the 
  // device and initializes the functions in the abstracted driver.

  int status = 0;

  status = kernelDmaRegisterDevice(&dmaDevice);
  if (status < 0)
    return (status);

  status = kernelInstallDmaDriver();
  if (status < 0)
    return (status);

  // Initialize the DMA controller functions
  status = kernelDmaInitialize();
  if (status < 0)
    return (status);

  return (status = 0);
}


static int kernelHardwareEnumerateKeyboardDevice(void)
{
  // This routine enumerates the system's keyboard device.  
  // They doesn't really need enumeration; this really just registers the 
  // device and initializes the functions in the abstracted driver.

  int status = 0;

  status = kernelKeyboardRegisterDevice(&keyboardDevice);
  if (status < 0)
    return (status);

  status = kernelInstallKeyboardDriver();
  if (status < 0)
    return (status);

  // Initialize the keyboard functions
  status = kernelKeyboardInitialize();
  if (status < 0)
    return (status);

  return (status = 0);
}


static int kernelHardwareEnumerateFloppyDevices(void)
{
  // This routine enumerates floppy drives, and their types, and creates
  // kernelDiskObjects to store the information.  If successful, it 
  // returns the number of floppy devices it discovered.  It has a 
  // complementary routine which will return a pointer to the data
  // structure it has created.

  int status = 0;
  int count;
  int stepRate = 0;
  int headLoad = 0;
  int headUnload = 0;
  int gapLength = 0;


  // We'll need to call a couple of the driver functions
  int (*floppyInitialize) (void) = DEFAULTFLOPPYINIT;
  int (*floppyDescribe) (int, ...) =
    DEFAULTFLOPPYDESCRIBE;

  // Initialize the floppy disk driver code
  floppyInitialize();

  // Reset the number of floppy devices 
  numberFloppies = systemInfo->floppyDisks;

  // Now we know the types.  Now we can fill out some data values
  // in the kernelDiskObject structure(s)
  for (count = 0; count < numberFloppies; count ++)
    {
      switch(systemInfo->fddInfo[count].type)
	{
	case 1:
	  // This is a 360 KB 5.25" Disk.  Yuck.
	  floppyDevices[count].description = 
	    "Standard 360 Kb 5.25\" floppy"; 
	  floppyDevices[count].sectorSize = 512;
	  stepRate = 0x0D;
	  headLoad = 0x02;
	  headUnload = 0x0F;
	  gapLength = 0x2A;
	  break;
	
	case 2:
	  // This is a 1.2 MB 5.25" Disk.  Yuck.
	  floppyDevices[count].description = 
	    "Standard 1.2 Mb 5.25\" floppy"; 
	  floppyDevices[count].sectorSize = 512;
	  stepRate = 0x0D;
	  headLoad = 0x02;
	  headUnload = 0x0F;
	  gapLength = 0x2A;
	  break;
	
	case 3:
	  // This is a 720 KB 3.5" Disk.  Yuck.
	  floppyDevices[count].description = 
	    "Standard 720 Kb 3.5\" floppy"; 
	  floppyDevices[count].sectorSize = 512;
	  stepRate = 0x0D;
	  headLoad = 0x02;
	  headUnload = 0x0F;
	  gapLength = 0x1B;
	  break;
	
	case 4:
	  // This is a 1.44 MB 3.5" Disk.
	  floppyDevices[count].description = 
	    "Standard 1.44 Mb 3.5\" floppy"; 
	  floppyDevices[count].sectorSize = 512;
	  stepRate = 0x0A;
	  headLoad = 0x02;
	  headUnload = 0x0F;
	  gapLength = 0x1B;
	  break;
	
	case 5:
	  // This is a 2.88 MB 3.5" Disk.
	  floppyDevices[count].description = 
	    "Standard 2.88 Mb 3.5\" floppy"; 
	  floppyDevices[count].sectorSize = 512;
	  stepRate = 0x0A;
	  headLoad = 0x02;
	  headUnload = 0x0F;
	  gapLength = 0x1B;
	  break;
	
	default:
	  // Oh oh.  This is an unexpected value.  Make an error.
	  kernelError(kernel_error,
	      "The disk type returned by the disk controller is unknown");
	  return (status = ERR_INVALID);
	}

      // The head, track and sector values we got from the loader
      floppyDevices[count].heads = systemInfo->fddInfo[count].heads;
      floppyDevices[count].cylinders = systemInfo->fddInfo[count].tracks;
      floppyDevices[count].sectors = systemInfo->fddInfo[count].sectors;

      // Floppies are not partitioned, so the starting head, cylinder
      // and sector are always the same
      floppyDevices[count].startHead = 0;
      floppyDevices[count].startCylinder = 0;
      floppyDevices[count].startSector = 1;
      floppyDevices[count].startLogicalSector = 0;
      floppyDevices[count].logicalSectors = (floppyDevices[count].heads *
	     floppyDevices[count].cylinders * floppyDevices[count].sectors);
      floppyDevices[count].maxSectorsPerOp = 
	(floppyDevices[count].sectors * floppyDevices[count].heads);
	
      // Some additional universal default values
      floppyDevices[count].type = floppy;
      floppyDevices[count].driverDiskNumber = count;
      floppyDevices[count].addressingMethod = addr_pchs;
      floppyDevices[count].fixedRemovable = removable;
      floppyDevices[count].dmaChannel = 2;
      floppyDevices[count].lock = 0;
      floppyDevices[count].motorStatus = 0;

      // Describe the floppy device to the driver
      floppyDescribe(count, headLoad, headUnload, stepRate, 0,
		     floppyDevices[count].sectorSize,
		     floppyDevices[count].sectors, gapLength);

      // Register the floppy disk device
      status = kernelDiskFunctionsRegisterDevice(&floppyDevices[count]);
      if (status < 0)
	return (status);
    }

  status = kernelInstallFloppyDriver();
  if (status < 0)
    return (status);

  return (numberFloppies);
}


static int kernelHardwareEnumerateHardDiskDevices(void)
{
  // This routine enumerates hard disks, and creates kernelDiskObjects
  // to store the information.  If successful, it returns the number of 
  // devices it enumerated.  It has a complementary routine which will 
  // return a pointer to the data structure it has created.

  // The floppy disks must previously have been enumerated, because this
  // routine numbers disks starting with the number of floppies
  // previously detected.

  int status = 0;
  int physicalDisks = 0;
  unsigned char sectBuf[512];
  unsigned char partitionType = 0;
  char *partitionRecord = NULL;
  char *partitionDescription = NULL;
  int count1, count2, count3;

  // These are the default hard disk driver functions that we need to
  // use to examine the hard disks.  We need to do this since there 
  // obviously isn't a driver preattached to the disks we're creating.
  int (*hddInitialize) (void) = DEFAULTHDDINIT;
  int (*hddRecalibrate) (int) = DEFAULTHDDRECALIBRATE;
  int (*hddReadSectors) (int, unsigned int, unsigned int,
	 unsigned int, unsigned int, unsigned int, void *) = DEFAULTHDDREAD;

  // This structure is used to describe a known partition type
  typedef struct
  {
    unsigned char index;
    char *description;
  } partType;   

  // This is a table for keeping partition types
  static partType partitionTypes[] =
  {
    { 0x01, "12-bit FAT"},
    { 0x04, "16-bit FAT"},
    { 0x05, "Extended partition"},
    { 0x06, "16-bit FAT"},
    { 0x07, "OS/2 HPFS, or NTFS"},
    { 0x0A, "OS/2 Boot Manager"},
    { 0x0B, "FAT32"},
    { 0x0C, "FAT32 (LBA)"},
    { 0x0E, "16-bit FAT (LBA)"},
    { 0x0F, "Extended partition (LBA)"},
    { 0x63, "GNU HURD"},
    { 0x80, "Minix"},
    { 0x81, "Linux or Minix"},
    { 0x82, "Linux swap or Solaris"},
    { 0x83, "Linux ext2"},
    { 0x87, "HPFS mirrored"},
    { 0xBE, "Solaris boot"},
    { 0xC7, "HPFS mirrored"},
    { 0xEB, "BeOS BFS"},
    { 0xF2, "DOS 3.3+ second partition"},
    { 0, 0 }
  };


  // We need to loop through the list of physical hard disks, and check
  // each one for partitions.  These in turn will become "logical" disk
  // objects in Visopsys.

  // Initialize the hard disk driver
  if (hddInitialize() < 0)
    {
      kernelError(kernel_error, "Error initializing hard disk driver");
      return (status = ERR_NOTINITIALIZED);
    }

  // Reset the number of physical hard disk devices we've actually
  // examined, and reset the number of logical disks we've created
  physicalDisks = 0; numberHardDisks = 0;

  // Make a message
  kernelLog("Examining hard disk partitions...");

  for (count1 = 0; ((physicalDisks < systemInfo->hardDisks) &&
		    (count1 < MAXHARDDISKDEVICES)); count1 ++)
    {
      // We need to read the master boot record, and make disk objects
      // for each of the partitions we find

      // Recalibrate the disk before we attempt to do a read
      status = hddRecalibrate(count1);

      if (status < 0)
	// There might not really be any such device
	continue;

      // The hard disk responded to our query.  We will add it to the
      // number of physical devices we found
      physicalDisks++;

      // Initialize the sector buffer
      kernelMemClear(sectBuf, 512);

      // Read the first sector of the disk
      status = hddReadSectors(count1, 0 /* head */, 0 /* cylinder */,
			      1 /* startsector */, 0 /* LBA */, 
			      1 /* numsectors */, sectBuf /* buffer */);
      if (status < 0)
	{
	  // We couldn't read from the disk
	  kernelError(kernel_error, "Error reading MBR on hard disk");
	  continue;
	}

      // Is this a valid MBR?
      if ((sectBuf[511] != (unsigned char) 0xAA) ||
	  (sectBuf[510] != (unsigned char) 0x55))
	{
	  // This is not a valid master boot record.
	  kernelError(kernel_error, "Invalid MBR on hard disk");
	  continue;
	}
      
      // Set this pointer to the first partition record in the master
      // boot record
      partitionRecord = (sectBuf + 0x01BE);

      // Loop through the partition records, looking for non-zero entries
      for (count2 = 0; count2 < 4; count2 ++)
	{
	  partitionType = partitionRecord[4];
	  
	  if (partitionType == 0)
	    // The "rules" say we must be finished with this physical
	    // device.
	    break;

	  partitionDescription = "Unsupported partition type";
	  for (count3 = 0; partitionTypes[count3].index != 0; count3 ++)
	    if (partitionTypes[count3].index == partitionType)
	      partitionDescription = partitionTypes[count3].description;
	  
	  
	  // We will make a disk object to correspond with the
	  // partition we've discovered

	  hardDiskDevices[numberHardDisks].driverDiskNumber = count1;
	  hardDiskDevices[numberHardDisks].dmaChannel = 3;
	  hardDiskDevices[numberHardDisks].description = 
	    "Standard ATA(IDE) hard disk";
	  hardDiskDevices[numberHardDisks].fixedRemovable = fixed;
	  hardDiskDevices[numberHardDisks].type = idedisk;

	  hardDiskDevices[numberHardDisks].startHead = 
	    (unsigned int) partitionRecord[0x01];
	  hardDiskDevices[numberHardDisks].startSector = 
	    (unsigned int) (partitionRecord[0x02] & 0x3F);
	  hardDiskDevices[numberHardDisks].startCylinder = 
	    (unsigned int) (partitionRecord[0x02] & 0xC0);
	  hardDiskDevices[numberHardDisks].startCylinder = 
	    (hardDiskDevices[numberHardDisks].startCylinder << 2);
	  hardDiskDevices[numberHardDisks].startCylinder += 
	    (unsigned int) partitionRecord[0x03];
	  hardDiskDevices[numberHardDisks].startLogicalSector = 
	    *((unsigned int *)(partitionRecord + 0x08));
	  hardDiskDevices[numberHardDisks].logicalSectors = 
	    *((unsigned int *)(partitionRecord + 0x0C));

	  // We get more hard disk info from the physical disk
	  // info we were passed.  We need to get it from 
	  // hddInfo[physicalDisks - 1] since we have already added
	  // one to that number
	  hardDiskDevices[numberHardDisks].heads = 
	    systemInfo->hddInfo[physicalDisks - 1].heads;
	  hardDiskDevices[numberHardDisks].cylinders = 
	    systemInfo->hddInfo[physicalDisks - 1].cylinders;
	  hardDiskDevices[numberHardDisks].sectors = 
	    systemInfo->hddInfo[physicalDisks - 1].sectors;
	  hardDiskDevices[numberHardDisks].sectorSize = 
	    systemInfo->hddInfo[physicalDisks - 1].bytesPerSector;
	  hardDiskDevices[numberHardDisks].maxSectorsPerOp = 
	    ((128 * 1024) / hardDiskDevices[numberHardDisks].sectorSize);
	  
	  // Does this disk use LBA or CHS?
	  if (hardDiskDevices[numberHardDisks].heads > 16)
	    // This disk MUST use LBA, since 16 heads is the maximum.
	    hardDiskDevices[numberHardDisks].addressingMethod = addr_lba;
	  else
	    // Use CHS
	    hardDiskDevices[numberHardDisks].addressingMethod = addr_pchs;
	  
	  hardDiskDevices[numberHardDisks].lock = 0;
	  hardDiskDevices[numberHardDisks].motorStatus = 1;

	  // Register the hard disk device
	  status = kernelDiskFunctionsRegisterDevice(
			     &hardDiskDevices[numberHardDisks]);
	  if (status < 0)
	    return (status);

	  kernelLog("Disk %d (hard disk %d, partition %d): %s",
		    hardDiskDevices[numberHardDisks].diskNumber,
		    count1, count2, partitionDescription);
	  
	  // Increase the number of logical hard disk devices
	  numberHardDisks++;

	  // Move to the next partition record
	  partitionRecord += 16;
	}
    }

  status = kernelInstallHardDiskDriver();
  if (status < 0)
    return (status);

  // What if we didn't successfully examine all the disks?
  if (physicalDisks < systemInfo->hardDisks)
    {
      // Make a warning
      kernelError(kernel_warn, "Some hard disks could not be enumerated");
    }

  return (numberHardDisks);
}


static int kernelHardwareEnumerationInitialize(void)
{
  // This routine initializes all of the data structures used to
  // keep track of hardware devices.

  int status = 0;


  // Initialize the memory for the various objects we're managing
  kernelMemClear(&processorDevice, sizeof(kernelProcessorObject));
  kernelMemClear(&picDevice, sizeof(kernelPicObject));
  kernelMemClear(&systemTimerDevice, sizeof(kernelSysTimerObject));
  kernelMemClear(&rtcDevice, sizeof(kernelRtcObject));
  kernelMemClear(&dmaDevice, sizeof(kernelDmaObject));
  kernelMemClear(&keyboardDevice, sizeof(kernelKeyboardObject));
  kernelMemClear((void *) floppyDevices, 
			(sizeof(kernelDiskObject) * MAXFLOPPIES));
  kernelMemClear((void *) hardDiskDevices, 
			(sizeof(kernelDiskObject) * MAXHARDDISKS));

  // Make a note of the fact that the devices have been initialized
  kernelHardwareObjectsInitialized = 1;
  
  return (status = 0);
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


int kernelHardwareEnumerate(loaderInfoStruct *info)
{
  // Just calls all of the above hardware enumeration routines.  Used during
  // kernel initialization.  Returns 0 unless any of the other routines
  // return an error (negative), in which case it relays the error code.

  int status = 0;


  // Make sure the info structure isn't NULL
  if (info == NULL)
    return (status = ERR_NULLPARAMETER);

  // Save the pointer to the data structure that describes the hardware
  systemInfo = info;

  // Initialize all of the data objects
  status = kernelHardwareEnumerationInitialize();
  if (status < 0)
    return (status);

  // Nail down the processor device
  status = kernelHardwareEnumerateProcessorDevice();
  if (status < 0)
    return (status);

  // The PIC device
  status = kernelHardwareEnumeratePicDevice();
  if (status < 0)
    return (status);

  // This is the earliest point at which we can allow interrupts to occur
  // (since the PICs are now enabled).  Enable interrupts now.
  kernelPicEnableInterrupts();

  // The system timer device
  status = kernelHardwareEnumerateSysTimerDevice();
  if (status < 0)
    return (status);

  // The Real-Time clock device
  status = kernelHardwareEnumerateRtcDevice();
  if (status < 0)
    return (status);

  // The DMA controller device
  status = kernelHardwareEnumerateDmaDevice();
  if (status < 0)
    return (status);

  // The keyboard device
  status = kernelHardwareEnumerateKeyboardDevice();
  if (status < 0)
    return (status);

  // Enumerate the floppy disk devices
  status = kernelHardwareEnumerateFloppyDevices();
  if (status < 0)
    return (status);

  // Enumerate the hard disk devices
  status = kernelHardwareEnumerateHardDiskDevices();
  if (status < 0)
    return (status);

  /*
  if (systemInfo->videoLFB)
    {
      // Reserve the memory block that belongs to the linear framebuffer
      status = kernelMemoryReserveBlock((unsigned int) systemInfo->videoLFB,
					(systemInfo->videoMemory * 1024),
					"video linear framebuffer");
      if (status < 0)
	return (status);
    }
  */

  // Return success
  return (status = 0);
}


kernelDiskObject *kernelGetFloppyDiskObject(int whichDisk)
{
  // This is the complementary routine to the kernelEnumerateFloppyDevices
  // routine.  It returns a pointer to any of the kernelDiskObjects
  // created by that routine.  It DOES check the requested member against
  // the possible range, and returns NULL and a kernelError if it is out
  // of range

  // Make sure whichDisk is a valid number
  if ((whichDisk < 0) || (whichDisk >= numberFloppies))
    {
      // The number is out of range.  Make an error.
      kernelError(kernel_error,
      "The requested disk object is out of the range of possible devices");
      return (NULL);
    }

  return (&floppyDevices[whichDisk]);
}


int kernelGetNumberLogicalHardDisks(void)
{
  // Returns the number of hard disk devices enumerated
  return (numberHardDisks);
}
 

kernelDiskObject *kernelGetHardDiskObject(int whichDisk)
{
  // This is the complementary routine to the kernelEnumerateHardDiskDevices
  // routine.  It returns a pointer to any of the kernelDiskObjects
  // created by that routine.  It DOES check the requested member against
  // the possible range, and returns NULL and a kernelError if it is out
  // of range

  // Make sure whichDisk is a valid number
  if ((whichDisk < 0) || (whichDisk >= numberHardDisks))
    {
      // The number is out of range.  Make an error.
      kernelError(kernel_error,
      "The requested disk object is out of the range of possible devices");
      return (NULL);
    }

  return (&hardDiskDevices[whichDisk]);
}
