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
//  kernelHardwareEnumeration.c
//

// These routines enumerate all of the hardware devices in the system based
// on the hardware data structure passed to the kernel by the os loader.

#include "kernelHardwareEnumeration.h"
#include "kernelParameters.h"
#include "kernelDriverManagement.h"
#include "kernelPageManager.h"
#include "kernelProcessorX86.h"
#include "kernelText.h"
#include "kernelMiscFunctions.h"
#include "kernelLog.h"
#include "kernelError.h"
#include <sys/errors.h>
#include <stdio.h>
#include <string.h>


loaderInfoStruct *systemInfo = NULL;

static kernelPic picDevice;
static kernelSysTimer systemTimerDevice;
static kernelRtc rtcDevice;
static kernelDma dmaDevice;
static kernelKeyboard keyboardDevice;
static kernelMouse mouseDevice;
static kernelPhysicalDisk floppyDevices[MAXFLOPPIES];  
static int numberFloppies = 0;
static kernelPhysicalDisk hardDiskDevices[MAXHARDDISKS];  
static int numberHardDisks = 0;
static kernelGraphicAdapter graphicAdapterDevice;


static int enumeratePicDevice(void)
{
  // This routine enumerates the system's Programmable Interrupt Controller
  // device.  It doesn't really need enumeration; this really just registers 
  // the device and initializes the functions in the abstracted driver.

  int status = 0;

  kernelInstallPicDriver(&picDevice);

  status = kernelPicRegisterDevice(&picDevice);
  if (status < 0)
    return (status);

  status = kernelPicInitialize();
  if (status < 0)
    return (status);

  return (status = 0);
}


static int enumerateSysTimerDevice(void)
{
  // This routine enumerates the system timer device.  It doesn't really
  // need enumeration; this really just registers the device and initializes
  // the functions in the abstracted driver.

  int status = 0;

  kernelInstallSysTimerDriver(&systemTimerDevice);

  status = kernelSysTimerRegisterDevice(&systemTimerDevice);
  if (status < 0)
    return (status);

  // Initialize the system timer functions
  status = kernelSysTimerInitialize();
  if (status < 0)
    return (status);

  return (status = 0);
}


static int enumerateRtcDevice(void)
{
  // This routine enumerates the system's Real-Time clock device.  
  // It doesn't really need enumeration; this really just registers the 
  // device and initializes the functions in the abstracted driver.

  int status = 0;

  kernelInstallRtcDriver(&rtcDevice);

  status = kernelRtcRegisterDevice(&rtcDevice);
  if (status < 0)
    return (status);

  // Initialize the real-time clock functions
  status = kernelRtcInitialize();
  if (status < 0)
    return (status);

  return (status = 0);
}


static int enumerateDmaDevice(void)
{
  // This routine enumerates the system's DMA controller device(s).  
  // They doesn't really need enumeration; this really just registers the 
  // device and initializes the functions in the abstracted driver.

  int status = 0;

  kernelInstallDmaDriver(&dmaDevice);

  status = kernelDmaRegisterDevice(&dmaDevice);
  if (status < 0)
    return (status);

  // Initialize the DMA controller functions
  status = kernelDmaInitialize();
  if (status < 0)
    return (status);

  return (status = 0);
}


static int enumerateKeyboardDevice(void)
{
  // This routine enumerates the system's keyboard device.  
  // They doesn't really need enumeration; this really just registers the 
  // device and initializes the functions in the abstracted driver.

  int status = 0;

  kernelInstallKeyboardDriver(&keyboardDevice);

  status = kernelKeyboardRegisterDevice(&keyboardDevice);
  if (status < 0)
    return (status);

  // Initialize the keyboard functions
  status = kernelKeyboardInitialize();
  if (status < 0)
    return (status);

  // Set the default keyboard data stream to be the console input
  status =
    kernelKeyboardSetStream((stream *) &(kernelTextGetConsoleInput()->s));
  if (status < 0)
    return (status);

  return (status = 0);
}


static int enumerateFloppyDevices(void)
{
  // This routine enumerates floppy drives, and their types, and creates
  // kernelPhysicalDisks to store the information.  If successful, it 
  // returns the number of floppy devices it discovered.  It has a 
  // complementary routine which will return a pointer to the data
  // structure it has created.

  int status = 0;
  int count;

  // Reset the number of floppy devices 
  numberFloppies = systemInfo->floppyDisks;

  // We know the types.  We can fill out some data values in the
  // physical disk structure(s)
  for (count = 0; count < numberFloppies; count ++)
    {
      kernelInstallFloppyDriver(&floppyDevices[count]);

      switch(systemInfo->fddInfo[count].type)
	{
	case 1:
	  // This is a 360 KB 5.25" Disk.  Yuck.
	  floppyDevices[count].description = "360 Kb 5.25\" floppy"; 
	  floppyDevices[count].stepRate = 0x0D;
	  floppyDevices[count].gapLength = 0x2A;
	  break;
	
	case 2:
	  // This is a 1.2 MB 5.25" Disk.  Yuck.
	  floppyDevices[count].description = "1.2 Mb 5.25\" floppy"; 
	  floppyDevices[count].stepRate = 0x0D;
	  floppyDevices[count].gapLength = 0x2A;
	  break;
	
	case 3:
	  // This is a 720 KB 3.5" Disk.  Yuck.
	  floppyDevices[count].description = "720 Kb 3.5\" floppy"; 
	  floppyDevices[count].stepRate = 0x0D;
	  floppyDevices[count].gapLength = 0x1B;
	  break;
	
	case 4:
	  // This is a 1.44 MB 3.5" Disk.
	  floppyDevices[count].description = "1.44 Mb 3.5\" floppy"; 
	  floppyDevices[count].stepRate = 0x0A;
	  floppyDevices[count].gapLength = 0x1B;
	  break;
	
	case 5:
	  // This is a 2.88 MB 3.5" Disk.
	  floppyDevices[count].description = "2.88 Mb 3.5\" floppy"; 
	  floppyDevices[count].stepRate = 0x0A;
	  floppyDevices[count].gapLength = 0x1B;
	  break;
	
	default:
	  // Oh oh.  This is an unexpected value.  Make an error.
	  kernelError(kernel_error, "The disk type returned by the disk "
		      "controller is unknown");
	  return (status = ERR_INVALID);
	}

      // The device name and filesystem type
      sprintf((char *) floppyDevices[count].name, "fd%d", count);

      // The head, track and sector values we got from the loader
      floppyDevices[count].heads = systemInfo->fddInfo[count].heads;
      floppyDevices[count].cylinders = systemInfo->fddInfo[count].tracks;
      floppyDevices[count].sectorsPerCylinder =
	systemInfo->fddInfo[count].sectors;
      floppyDevices[count].numSectors = 
	(floppyDevices[count].heads * floppyDevices[count].cylinders *
	 floppyDevices[count].sectorsPerCylinder);

      // Some additional universal default values
      floppyDevices[count].type = floppy;
      floppyDevices[count].fixedRemovable = removable;
      floppyDevices[count].deviceNumber = count;
      floppyDevices[count].sectorSize = 512;
      floppyDevices[count].headLoad = 0x02;
      floppyDevices[count].headUnload = 0x0F;
      floppyDevices[count].dataRate = 0;
      floppyDevices[count].dmaChannel = 2;
      // Assume motor on for now
      floppyDevices[count].motorStatus = 1;

      // Register the floppy disk device
      status = kernelDiskRegisterDevice(&floppyDevices[count]);
      if (status < 0)
	return (status);
    }

  return (numberFloppies);
}


static int enumerateHardDiskDevices(void)
{
  // This routine enumerates hard disks, and creates kernelPhysicalDisks
  // to store the information.  If successful, it returns the number of 
  // devices it enumerated.  It has a complementary routine which will 
  // return a pointer to the data structure it has created.

  // The floppy disks must previously have been enumerated, because this
  // routine numbers disks starting with the number of floppies
  // previously detected.

  int status = 0;
  int physicalDisk = 0;

  // Reset the number of physical hard disk devices we've actually
  // examined, and reset the number of logical disks we've created
  numberHardDisks = 0;

  // We need to loop through the list of physical hard disks, and check
  // each one for partitions.  These in turn will become "logical" disk
  // objects in Visopsys.

  // Make a message
  kernelLog("Examining hard disk partitions...");

  for (physicalDisk = 0; ((numberHardDisks < systemInfo->hardDisks) &&
			  (physicalDisk < MAXHARDDISKS));
       physicalDisk ++)
    {
      // Save info about the physical device.
      
      // Install the IDE driver
      kernelInstallHardDiskDriver(&hardDiskDevices[numberHardDisks]);

      // The device name and filesystem type
      sprintf((char *) hardDiskDevices[numberHardDisks].name, (char *) "hd%d",
	      numberHardDisks);

      hardDiskDevices[numberHardDisks].deviceNumber = physicalDisk;
      hardDiskDevices[numberHardDisks].dmaChannel = 3;
      hardDiskDevices[numberHardDisks].description = "IDE hard disk";
      hardDiskDevices[numberHardDisks].fixedRemovable = fixed;
      hardDiskDevices[numberHardDisks].type = idedisk;

      // We get more hard disk info from the physical disk
      // info we were passed.
      hardDiskDevices[numberHardDisks].heads = 
	systemInfo->hddInfo[numberHardDisks].heads;
      hardDiskDevices[numberHardDisks].cylinders = 
	systemInfo->hddInfo[numberHardDisks].cylinders;
      hardDiskDevices[numberHardDisks].sectorsPerCylinder = 
	systemInfo->hddInfo[numberHardDisks].sectorsPerCylinder;
      hardDiskDevices[numberHardDisks].numSectors = (unsigned)
	systemInfo->hddInfo[numberHardDisks].totalSectors;
      hardDiskDevices[numberHardDisks].sectorSize = 
	systemInfo->hddInfo[numberHardDisks].bytesPerSector;
      // Sometimes 0?  We can't have that as we are about to use it to
      // perform a division operation.
      if (hardDiskDevices[numberHardDisks].sectorSize == 0)
	{
	  kernelError(kernel_warn, "Physical disk %d sector size 0; "
		      "assuming 512", physicalDisk);
	  hardDiskDevices[numberHardDisks].sectorSize = 512;
	}
      hardDiskDevices[numberHardDisks].bootLBA =
	systemInfo->hddInfo[numberHardDisks].bootLBA;
      hardDiskDevices[numberHardDisks].motorStatus = 1;

      // Register the hard disk device
      status = kernelDiskRegisterDevice(&hardDiskDevices[numberHardDisks]);
      if (status < 0)
	return (status);

      // Increase the number of logical hard disk devices
      numberHardDisks++;
    }

  return (numberHardDisks);
}


static int enumerateGraphicDevice(void)
{
  // This routine enumerates the system's graphic adapter device.  
  // They doesn't really need enumeration; this really just registers the 
  // device and initializes the functions in the abstracted driver.

  int status = 0;

  // Set up the device parameters
  graphicAdapterDevice.videoMemory = systemInfo->graphicsInfo.videoMemory;
  graphicAdapterDevice.mode = systemInfo->graphicsInfo.mode;
  graphicAdapterDevice.framebuffer = systemInfo->graphicsInfo.framebuffer;
  graphicAdapterDevice.xRes = systemInfo->graphicsInfo.xRes;
  graphicAdapterDevice.yRes = systemInfo->graphicsInfo.yRes;
  graphicAdapterDevice.bitsPerPixel = systemInfo->graphicsInfo.bitsPerPixel;
  if (graphicAdapterDevice.bitsPerPixel == 15)
    graphicAdapterDevice.bytesPerPixel = 2;
  else
    graphicAdapterDevice.bytesPerPixel =
      (graphicAdapterDevice.bitsPerPixel / 8);
  
  kernelInstallGraphicDriver(&graphicAdapterDevice);

  // If we are in a graphics mode, initialize the graphics functions
  if (graphicAdapterDevice.mode != 0)
    {
      // Map the supplied physical linear framebuffer address into kernel
      // memory
      status = kernelPageMapToFree(KERNELPROCID,
				   graphicAdapterDevice.framebuffer, 
				   &(graphicAdapterDevice.framebuffer),
				   (graphicAdapterDevice.xRes *
				    graphicAdapterDevice.yRes *
				    graphicAdapterDevice.bytesPerPixel));
      if (status < 0)
	{
	  kernelError(kernel_error, "Unable to map linear framebuffer");
	  return (status);
	}

      status = kernelGraphicRegisterDevice(&graphicAdapterDevice);
      if (status < 0)
	return (status);

      status = kernelGraphicInitialize();
      if (status < 0)
	return (status);
    }

  return (status = 0);
}


static int enumerateMouseDevice(void)
{
  // This routine enumerates the system's mouse device.  For the time
  // being it assumes that the mouse is a PS2 type

  int status = 0;

  kernelInstallMouseDriver(&mouseDevice);

  status = kernelMouseRegisterDevice(&mouseDevice);
  if (status < 0)
    return (status);

  // Initialize the mouse functions
  status = kernelMouseInitialize();
  if (status < 0)
    return (status);

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

  // Initialize the memory for the various objects we're managing
  kernelMemClear(&picDevice, sizeof(kernelPic));
  kernelMemClear(&systemTimerDevice, sizeof(kernelSysTimer));
  kernelMemClear(&rtcDevice, sizeof(kernelRtc));
  kernelMemClear(&dmaDevice, sizeof(kernelDma));
  kernelMemClear(&keyboardDevice, sizeof(kernelKeyboard));
  kernelMemClear(&mouseDevice, sizeof(kernelMouse));
  kernelMemClear((void *) floppyDevices, 
			(sizeof(kernelPhysicalDisk) * MAXFLOPPIES));
  kernelMemClear((void *) hardDiskDevices, 
			(sizeof(kernelPhysicalDisk) * MAXHARDDISKS));
  kernelMemClear(&graphicAdapterDevice, sizeof(kernelGraphicAdapter));

  // Start enumerating devices

  // The PIC device
  status = enumeratePicDevice();
  if (status < 0)
    return (status);

  // The system timer device
  status = enumerateSysTimerDevice();
  if (status < 0)
    return (status);

  // The Real-Time clock device
  status = enumerateRtcDevice();
  if (status < 0)
    return (status);

  // The DMA controller device
  status = enumerateDmaDevice();
  if (status < 0)
    return (status);

  // The keyboard device
  status = enumerateKeyboardDevice();
  if (status < 0)
    return (status);

  // Enable interrupts now.
  kernelProcessorEnableInts();

  // Enumerate the floppy disk devices
  status = enumerateFloppyDevices();
  if (status < 0)
    return (status);

  // Enumerate the hard disk devices
  status = enumerateHardDiskDevices();
  if (status < 0)
    return (status);

  // Enumerate the graphic adapter
  status = enumerateGraphicDevice();
  if (status < 0)
    return (status);

  // Do the mouse device after the graphic device so we can get screen
  // parameters, etc.  Also needs to be after the keyboard driver since
  // PS2 mouses use the keyboard controller.
  status = enumerateMouseDevice();
  if (status < 0)
    return (status);

  // Return success
  return (status = 0);
}
