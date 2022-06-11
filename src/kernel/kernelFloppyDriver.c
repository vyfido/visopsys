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
//  kernelFloppyDriver.c
//

// Driver for standard 3.5" floppy disks

#include "kernelDriverManagement.h" // Contains my prototypes
#include "kernelDma.h"
#include "kernelParameters.h"
#include "kernelProcessorX86.h"
#include "kernelMemoryManager.h"
#include "kernelPageManager.h"
#include "kernelMultitasker.h"
#include "kernelLock.h"
#include "kernelMiscFunctions.h"
#include "kernelError.h"
#include <sys/errors.h>

void kernelFloppyDriverReceiveInterrupt(void);

// Error codes and messages
#define FLOPPY_ABNORMAL           0
#define FLOPPY_INVALIDCOMMAND     1
#define FLOPPY_EQUIPMENTCHECK     2
#define FLOPPY_ENDOFTRACK         3
#define FLOPPY_CRCERROR1          4
#define FLOPPY_DMAERROR           5
#define FLOPPY_INVALIDSECTOR      6
#define FLOPPY_WRITEPROTECT       7
#define FLOPPY_MISSINGADDRESSMARK 8
#define FLOPPY_CONTROLMARK        9
#define FLOPPY_CRCERROR2          10
#define FLOPPY_INVALIDTRACK       11
#define FLOPPY_BADTRACK           12
#define FLOPPY_BADADDRESSMARK     13
#define FLOPPY_TIMEOUT            14
#define FLOPPY_UNKNOWN            15

static char *errorMessages[] = {
  "Abnormal termination - command did not complete",
  "Invalid command",
  "Equipment check - seek to invalid track",
  "The requested sector is past the end of the track",
  "ID byte or data - the CRC integrity check failed",
  "DMA transfer overrun or underrun",
  "No data - the requested sector was not found",
  "Write protect",
  "Missing address mark",
  "Sector control mark - data was not the expected type",
  "Data - the CRC integrity check failed",
  "Invalid or unexpected track",
  "Bad track",
  "Bad address mark",
  "Command timed out",
  "Unknown error"
};

static kernelPhysicalDisk *floppies[MAXFLOPPIES];
static volatile int controllerLock = 0;
static volatile unsigned currentTrack = 0;
static volatile int readStatusOnInterrupt = 0;
static volatile int interruptReceived = 0;
static volatile unsigned char statusRegister0;
static volatile unsigned char statusRegister1;
static volatile unsigned char statusRegister2;
static volatile unsigned char statusRegister3;

// An area for doing floppy disk DMA transfers (physically aligned, etc)
static volatile void *xFerPhysical = NULL;
static volatile void *xFer = NULL;


static void commandWrite(unsigned char cmd)
{
  // Waits until the floppy controller is ready for a new command (or part
  // thereof) in port 03F5h, and then writes it.
  
  unsigned startTime = kernelSysTimerRead();
  unsigned char data;

  while (kernelSysTimerRead() < (startTime + 10))
    {
      // Get the drive transfer mode from the port
      kernelProcessorDelay();
      kernelProcessorInPort8(0x03F4, data);
      
      // Check whether access is permitted
      if ((data & 0xC0) == 0x80)
	break;
    }

  kernelProcessorOutPort8(0x03F5, cmd);
  kernelProcessorDelay();

  return;
}


static unsigned char statusRead(void)
{
  // Waits until the floppy controller is ready for a read of port 03F5h,
  // and then reads it.

  unsigned startTime = kernelSysTimerRead();
  unsigned char data;

  while (kernelSysTimerRead() < (startTime + 10))
    {
      // Get the drive transfer mode from the port
      kernelProcessorDelay();
      kernelProcessorInPort8(0x03F4, data);
      
      // Check whether access is permitted
      if ((data >> 6) == 3)
	break;
    }

  kernelProcessorInPort8(0x03F5, data);
  return (data);
}


static int waitOperationComplete(void)
{
  // This routine just loops, reading the "interrupt received" byte.  When
  // the byte becomes 1, it resets the byte and returns.  If the wait times
  // out, the function returns negative.  Otherwise, it returns 0.

  int status = 0;
  unsigned startTime = kernelSysTimerRead();

  while (!interruptReceived)
    {
      // Yield the rest of this timeslice if we are in multitasking mode
      // kernelMultitaskerYield();

      if (kernelSysTimerRead() > (startTime + 20))
	break;
    }

  if (interruptReceived)
    {
      interruptReceived = 0;
      return (status = 0);
    }
  else
    {
      // No interrupt -- timed out.
      kernelError(kernel_error, errorMessages[FLOPPY_TIMEOUT]);
      return (status = ERR_IO);
    }
}


static int evaluateError(void)
{
  // This is an internal-only routine that takes no parameters and returns
  // no value.  It evaluates the returned bytes in the statusRegister[X] bytes
  // and matches conditions to error codes and error messages

  int errorCode = 0;

  // Check for abnormal termination of command
  if ((statusRegister0 & 0xC0) == 0x40) 
    errorCode = FLOPPY_ABNORMAL;

  // Check for invalid command
  else if ((statusRegister0 & 0xC0) == 0x80)
    errorCode = FLOPPY_INVALIDCOMMAND;
  
  // Check for Equipment check error
  if (statusRegister0 & 0x10)  // bit 4 (status register 0)
    errorCode = FLOPPY_EQUIPMENTCHECK;

  // Check for end-of-track
  if (statusRegister1 & 0x80)  // bit 7 (status register 1)
    errorCode = FLOPPY_ENDOFTRACK;

  // Bit 6 is unused in status register 1

  // Check for the first kind of data error
  if (statusRegister1 & 0x20)  // bit 5 (status register 1)
    errorCode = FLOPPY_CRCERROR1;

  // Check for overrun/underrun
  if (statusRegister1 & 0x10)  // bit 4 (status register 1)
    errorCode = FLOPPY_DMAERROR;

  // Bit 3 is unused in status register 1

  // Check for no data error
  if (statusRegister1 & 0x04)  // bit 2 (status register 1)
    errorCode = FLOPPY_INVALIDSECTOR;

  // Check for write protect error
  if (statusRegister1 & 0x02)  // bit 1 (status register 1)
    errorCode = FLOPPY_WRITEPROTECT;

  // Check for missing address mark
  if (statusRegister1 & 0x01)  // bit 0 (status register 1)
    errorCode = FLOPPY_MISSINGADDRESSMARK;

  // Bit 7 is unused in status register 2

  // Check for control mark error
  if (statusRegister2 & 0x40)  // bit 6 (status register 2)
    errorCode = FLOPPY_CONTROLMARK;

  // Check for the second kind of data error
  if (statusRegister2 & 0x20)  // bit 5 (status register 2)
    errorCode = FLOPPY_CRCERROR2;

  // Check for invalid track / wrong cylinder
  if (statusRegister2 & 0x10)  // bit 4 (status register 2)
    errorCode = FLOPPY_INVALIDTRACK;

  // Bit 3 is unused in status register 2
  // Bit 2 is unused in status register 2

  // Check for bad track
  if (statusRegister2 & 0x02)  // bit 1 (status register 2)
    errorCode = FLOPPY_BADTRACK;

  // Check for bad address mark
  if (statusRegister2 & 0x01)  // bit 0 (status register 2)
    errorCode = FLOPPY_BADADDRESSMARK;

  if (!errorCode)
    errorCode = FLOPPY_UNKNOWN;

  return (errorCode);
}


static void selectDrive(unsigned driveNum)
{
  // Takes the floppy number to select as its only parameter.  Selects the
  // drive on the controller

  unsigned char data = 0;

  // Select the drive on the controller

  // Get the current register value
  kernelProcessorDelay();
  kernelProcessorInPort8(0x03F2, data);

  // Make sure the DMA/Interrupt and reset-off bits are set
  data |= 0x0C;
  
  // Clear out the selection bits
  data &= 0xFC;

  // Set the selection bits
  data |= (unsigned char) driveNum;

  // Issue the command
  kernelProcessorOutPort8(0x03F2, data);
  kernelProcessorDelay();

  return;
}


static void specify(unsigned driveNum)
{
  // Sends some essential timing information to the floppy drive controller
  // about the specified drive.

  unsigned char commandByte;

  // Construct the data rate byte
  commandByte = (unsigned char) floppies[driveNum]->dataRate;
  kernelProcessorOutPort8(0x03F7, commandByte);
  kernelProcessorDelay();

  // Construct the command byte
  commandByte = (unsigned char) 0x03;  // Specify command
  commandWrite(commandByte);

  // Construct the step rate/head unload byte
  commandByte = (unsigned char) ((floppies[driveNum]->stepRate << 4) |
				 (floppies[driveNum]->headUnload & 0x0F));
  commandWrite(commandByte);

  // Construct the head load time byte.  Make sure that DMA mode is enabled.
  commandByte = (unsigned char) ((floppies[driveNum]->headLoad << 1) & 0xFE);
  commandWrite(commandByte);
  
  // There is no status information or interrupt after this command
  return;
}


static int setMotorStatus(unsigned driveNum, int onOff)
{
  // Turns the floppy motor on or off

  unsigned char data = 0;
  unsigned char tmp;

  // Select the drive
  selectDrive(driveNum);

  // Read the port's current state
  kernelProcessorDelay();
  kernelProcessorInPort8(0x03F2, data);

  // Move the motor select bit to the correct location [7:4]
  tmp = ((unsigned char) 0x10 << driveNum); 

  // Test whether the motor is on already
  if (onOff)
    {
      if (!(data & tmp))
	{
	  // Turn on the 'motor on' bit
	  data |= tmp;
	  
	  // Issue the command
	  kernelProcessorOutPort8(0x03F2, data);
	  kernelProcessorDelay();
	}
    }
  else
    {
      // Move the motor select bit to the correct location [7:4]
      tmp = 0x0F | ((unsigned char) 0xEF << driveNum); 

      // Turn off the 'motor on' bit
      data &= tmp;
	  
      // Issue the command
      kernelProcessorOutPort8(0x03F2, data);
      kernelProcessorDelay();
    }

  floppies[driveNum]->motorStatus = onOff;

  return (0);
}


static int readWriteSectors(unsigned driveNum, unsigned logicalSector,
			    unsigned numSectors, void *buffer, int read)
{
  // Reads or writes data to/from the disk.  Both types of operation are
  // combined here since the functionality is nearly identical.  Returns 0
  // on success, negative otherwise.

  int status = 0;
  kernelPhysicalDisk *theDisk = NULL;
  int errorCode = 0;
  unsigned head, track, sector;
  unsigned doSectors = 0;
  unsigned xFerBytes = 0;
  unsigned char commandByte, tmp;
  int count;

  // Get a pointer to the requested disk
  theDisk = floppies[driveNum];

  // Wait for a lock on the controller
  status = kernelLockGet((void *) &controllerLock);
  if (status < 0)
    return (status);

  // Select the drive
  selectDrive(driveNum);
  
  // We will have to make sure the motor is turned on
  if (theDisk->motorStatus == 0)
    {
      // Turn the drive motor on
      setMotorStatus(driveNum, 1);
      
      // We don't have to wait for the disk to spin up on a read operation;
      // It will start reading when it's good and ready.  If it's a write
      // operation we have to wait for it.
      if (!read)
	// Wait half a second for the drive to spin up
	kernelMultitaskerWait(10);
    }

  // We don't want to cross a track boundary in one operation.  Some
  // floppy controllers can't do this.  Thus, if necessary we break up the
  // operation with this loop.

  while (numSectors > 0)
    {
      // Calculate the physical head, track and sector to use
      head = ((logicalSector % (theDisk->sectorsPerCylinder * theDisk->heads)) 
	      / theDisk->sectorsPerCylinder);
      track = (logicalSector / (theDisk->sectorsPerCylinder * theDisk->heads));
      sector = (logicalSector % theDisk->sectorsPerCylinder) + 1;

      // Make sure the head, track, and sector are within the legal range
      // of values
      if ((sector > theDisk->sectorsPerCylinder) ||
	  (track >= theDisk->cylinders) || (head >= theDisk->heads))
	{
	  kernelLockRelease((void *) &controllerLock);
	  return (status = ERR_BADADDRESS);
	}

      // Here's where we check for crossing track boundaries
      doSectors = numSectors;
      if (((head * theDisk->sectorsPerCylinder) + sector + (doSectors - 1)) >
	  (theDisk->heads * theDisk->sectorsPerCylinder))
	doSectors = ((theDisk->heads * theDisk->sectorsPerCylinder) -
		     ((head * theDisk->sectorsPerCylinder) + (sector - 1)));

      // We need to do a seek for every read/write operation

      // Tell the interrupt-received routine to issue the "sense interrupt
      // status" command after the operation
      readStatusOnInterrupt = 1;
      interruptReceived = 0;

      // Construct the command byte
      commandByte = 0x0F;  // Seek command
      commandWrite(commandByte);

      // Construct the drive/head select byte
      // Format [00000 (head 1 bit)(drive 2 bits)]
      commandByte = (unsigned char) (((head & 1) << 2) | (driveNum & 3));
      commandWrite(commandByte);

      // Construct the track number byte
      commandByte = (unsigned char) track;
      commandWrite(commandByte);

      // The drive should now be seeking.  While we wait for the seek to
      // complete, we can do some other things.

      // How many bytes will we transfer?
      xFerBytes = (doSectors * floppies[driveNum]->sectorSize);
      
      // If it's a write operation, copy xFerBytes worth of user data
      // into the transfer area
      if (!read)
	kernelMemCopy(buffer, (void *) xFer, xFerBytes);

      // Set up the DMA controller for the transfer.
      if (read)
	// Set the DMA channel for writing TO memory, demand mode
	status =
	  kernelDmaOpenChannel(theDisk->dmaChannel, (void *) xFerPhysical,
			       xFerBytes, DMA_WRITEMODE);
      else
	// Set the DMA channel for reading FROM memory, demand mode
	status =
	  kernelDmaOpenChannel(theDisk->dmaChannel, (void *) xFerPhysical,
			       xFerBytes, DMA_READMODE);
      if (status < 0)
	{
	  kernelError(kernel_error, "Unable to open DMA channel");
	  kernelLockRelease((void *) &controllerLock);
	  return (status);
	}

      // Now wait for the seek to complete, check error conditions in the
      // status byte, and make sure that we are now at the correct track
      status = waitOperationComplete();
      if ((status < 0) || ((statusRegister0 & 0xF8) != 0x20) ||
	  (currentTrack != track))
	{
	  kernelDmaCloseChannel(theDisk->dmaChannel);
	  kernelLockRelease((void *) &controllerLock);
	  kernelError(kernel_error, "Seek error: %s",
		      errorMessages[evaluateError()]);
	  return (status = ERR_IO);
	}

      // Now proceed with the read/write operation

      // Tell the interrupt-received routine NOT to issue the "sense interrupt
      // status" command after the read/write operation
      readStatusOnInterrupt = 0;
      interruptReceived = 0;

      // Command byte
      // Drive/head select byte
      // Track number byte
      // Head number byte
      // Sector byte
      // Sector size code
      // End of track byte
      // Gap length byte
      // Custom sector size byte

      if (read)
	commandByte = 0xE6;  // "Read normal data" command
      else
	commandByte = 0xC5;  // "Write data" command
      commandWrite(commandByte);
      
      // Construct the drive/head select byte
      // Format [00000 (head 1 bit)(drive 2 bits)]
      commandByte = (unsigned char) (((head & 1) << 2) | (driveNum & 3));
      commandWrite(commandByte);

      // Construct the track number byte
      commandByte = (unsigned char) track;
      commandWrite(commandByte);
      
      // Construct the head number byte
      commandByte = (unsigned char) head;
      commandWrite(commandByte);

      // Construct the sector byte
      commandByte = (unsigned char) sector;
      commandWrite(commandByte);

      // Construct the sector size code
      commandByte = (unsigned char) (floppies[driveNum]->sectorSize >> 8);
      commandWrite(commandByte);

      // Construct the end of track byte
      commandByte = (unsigned char) floppies[driveNum]->sectorsPerCylinder;
      commandWrite(commandByte);

      // Construct the gap length byte
      commandByte = (unsigned char) floppies[driveNum]->gapLength;
      commandWrite(commandByte);

      // Construct the custom sector size byte
      commandByte = (unsigned char) 0xFF;  // Always FFh
      commandWrite(commandByte);

      status = waitOperationComplete();

      // Close the DMA channel
      kernelDmaCloseChannel(theDisk->dmaChannel);
      if (status < 0)
	{
	  // The command timed out.  Save the error and return error.
	  kernelLockRelease((void *) &controllerLock);
	  return (status);
	}

      // We have to read the seven status bytes from the controller.  Save them
      // in the designated memory locations

      statusRegister0 = statusRead();
      statusRegister1 = statusRead();
      statusRegister2 = statusRead();
      statusRegister3 = statusRead();

      // We don't care about status registers 4-6.
      for (count = 0; count < 3; count ++)
	tmp = statusRead();

      // Save the current track
      currentTrack = (unsigned ) statusRegister3;

      // Now we can examine the status.  If the top two bits of register 0 are
      // clear, then the operation completed normally.
      if (statusRegister0 & 0xC0)
	{
	  // We have an error.  We have to try to determine the cause and set
	  // the error message.  We'll call a routine which does all of this
	  // for us.
	  errorCode = evaluateError();
	  break;
	}
      else
	{
	  // If this was a read operation, copy xFerBytes worth of data from
	  // the transfer area to the user buffer
	  if (read)
	    kernelMemCopy((void *) xFer, buffer, xFerBytes);
	}
      
      logicalSector += doSectors;
      numSectors -= doSectors;
      buffer += (doSectors * theDisk->sectorSize);
      
    } // Per-operation loop
  
  // Unlock the controller
  kernelLockRelease((void *) &controllerLock);

  if (errorCode == FLOPPY_WRITEPROTECT)
    return (status = ERR_NOWRITE);
  else if (errorCode)
    {
      kernelError(kernel_error, "Read/write error: %s",
		  errorMessages[errorCode]);
      return (status = ERR_IO);
    }
  else return (status = 0);
}


static int recalibrate(int driveNum)
{
  // Recalibrates the selected drive, causing it to seek to track 0

  int status = 0;
  unsigned char commandByte, driveByte;

  if (driveNum >= MAXFLOPPIES)
    return (status = ERR_BOUNDS);

  // Wait for a lock on the controller
  status = kernelLockGet((void *) &controllerLock);
  if (status < 0)
    return (status);

  // Select the drive
  selectDrive(driveNum);

  // Tell the interrupt-received routine to issue the "sense interrupt
  // status" command after the operation
  readStatusOnInterrupt = 1;
  interruptReceived = 0;

  // We have to send two byte commands to the controller
  commandByte = (unsigned char) 0x07;
  commandWrite(commandByte);
  driveByte = (unsigned char) driveNum;
  commandWrite(driveByte);

  // Now wait for the operation to end
  status = waitOperationComplete();

  // Unlock the controller
  kernelLockRelease((void *) &controllerLock);

  if (status < 0)
    return (status);

  // Check error status
  if ((statusRegister0 & 0xF8) != 0x20)
    return (status = ERR_IO);

  // Make sure that we are now at track 0
  if (currentTrack == 0)
    return (status = 0);
  else
    return (status = ERR_IO);
}


static int registerDevice(void *diskPointer)
{
  // This function should be called before any attempt is made to use the
  // drive in question.  This allows us to store some information about this
  // drive.

  int status = 0;
  kernelPhysicalDisk *newDisk = (kernelPhysicalDisk *) diskPointer;

  if ((newDisk->sectorsPerCylinder == 0) || (newDisk->heads == 0))
    // We do division operations with these values
    return (status = ERR_INVALID);

  // Save the pointer to the disk info
  floppies[newDisk->deviceNumber] = newDisk;

  recalibrate(newDisk->deviceNumber);

  // Select the drive on the controller
  selectDrive(newDisk->deviceNumber);

  // Send the controller info about the drive.
  specify(newDisk->deviceNumber);

  return (status = 0);
}


static int reset(int driveNum)
{
  // Does a software reset of the requested floppy controller.  Always
  // returns success.

  int status = 0;
  unsigned char data = 0;

  if (driveNum >= MAXFLOPPIES)
    return (status = ERR_BOUNDS);

  // Wait for a lock on the controller
  status = kernelLockGet((void *) &controllerLock);
  if (status < 0)
    return (status);

  // Select the drive
  selectDrive(driveNum);

  // Read the port's current state
  kernelProcessorDelay();
  kernelProcessorInPort8(0x03F2, data);

  // Mask off the 'reset' bit (go to reset mode)
  data &= 0xFB;

  // Issue the command
  kernelProcessorOutPort8(0x03F2, data);
  kernelProcessorDelay();
  kernelProcessorDelay();

  // Now mask on the 'reset' bit (exit reset mode)
  data |= 0x04;

  // Issue the command
  kernelProcessorOutPort8(0x03F2, data);
  kernelProcessorDelay();

  // Unlock the controller
  kernelLockRelease((void *) &controllerLock);

  return (status = 0);
}


static int motorOn(int driveNum)
{
  // Turns the requested floppy motor on.

  int status = 0;

  if (driveNum >= MAXFLOPPIES)
    return (status = ERR_BOUNDS);

  // Wait for a lock on the controller
  status = kernelLockGet((void *) &controllerLock);
  if (status < 0)
    return (status);

  status = setMotorStatus(driveNum, 1);

  // Unlock the controller
  kernelLockRelease((void *) &controllerLock);

  return (status);
}


static int motorOff(int driveNum)
{
  // Turns the requested floppy motor off.

  int status = 0;

  if (driveNum >= MAXFLOPPIES)
    return (status = ERR_BOUNDS);

  // Wait for a lock on the controller
  status = kernelLockGet((void *) &controllerLock);
  if (status < 0)
    return (status);

  status = setMotorStatus(driveNum, 0);

  // Unlock the controller
  kernelLockRelease((void *) &controllerLock);

  return (status);
}


static int diskChanged(int driveNum)
{
  // This routine determines whether the media in the floppy has changed.
  // drive.  It takes no parameters, and returns 1 if the disk is missing
  // or has been changed, 0 if it has not been changed, and  negative if it
  // encounters some other type of error.

  int status = 0;
  unsigned char data = 0;

  if (driveNum >= MAXFLOPPIES)
    return (status = ERR_BOUNDS);

  // Wait for a lock on the controller
  status = kernelLockGet((void *) &controllerLock);
  if (status < 0)
    return (status);

  // Select the drive
  selectDrive(driveNum);

  // Now simply read port 03F7h.  Bit 7 is the only part that matters.
  kernelProcessorDelay();
  kernelProcessorInPort8(0x03F7, data);

  // Unlock the controller
  kernelLockRelease((void *) &controllerLock);

  if (data & 0x80)
    return (status = 1);
  else
    return (status = 0);
}


static int readSectors(int driveNum, unsigned logicalSector,
		       unsigned numSectors, void *buffer)
{
  if (driveNum >= MAXFLOPPIES)
    return (ERR_BOUNDS);

  // This routine is a wrapper for the readWriteSectors routine.
  return (readWriteSectors(driveNum, logicalSector, numSectors, buffer,
			   1));  // Read operation
}


static int writeSectors(int driveNum, unsigned logicalSector,
			unsigned numSectors, void *buffer)
{
  if (driveNum >= MAXFLOPPIES)
    return (ERR_BOUNDS);

  // This routine is a wrapper for the readWriteSectors routine.
  return (readWriteSectors(driveNum, logicalSector, numSectors, buffer,
			   0));  // Write operation
}


static kernelDiskDriver defaultFloppyDriver =
{
  NULL, // driverDetect
  registerDevice,
  reset,
  recalibrate,
  motorOn,
  motorOff,
  diskChanged,
  readSectors,
  writeSectors,
};


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


int kernelFloppyDriverInitialize(void)
{
  // This initializes the driver and returns success

  int status = 0;

  // Get memory for a disk transfer area.

  // We need to get a physical memory address to pass to the DMA controller
  // (in the case of floppies).  Therefore, we ask the memory manager
  // specifically for the physical address.
  xFerPhysical =
    kernelMemoryGetPhysical(DISK_CACHE_ALIGN, DISK_CACHE_ALIGN,
			    "floppy disk transfer");
  if (xFerPhysical == NULL)
    {
      kernelError(kernel_error, "Unable to allocate memory for floppy "
		  "disk transfer area");
      return (status = ERR_MEMORY);
    }
  
  // Map it into the kernel's address space
  status = kernelPageMapToFree(KERNELPROCID, (void *) xFerPhysical,
			       (void **) &xFer, DISK_CACHE_ALIGN);
  if (status < 0)
    {
      kernelError(kernel_error, "Unable to map memory for floppy disk "
		  "transfer area");
      return (status);
    }

  // Clear it out, since the kernelMemoryGetPhysical() routine doesn't do
  // it for us
  kernelMemClear((void *) xFer, DISK_CACHE_ALIGN);

  // Clear the "interrupt received" byte
  interruptReceived = 0;
  readStatusOnInterrupt = 0;    
  
  return (kernelDriverRegister(floppyDriver, &defaultFloppyDriver));
}


void kernelFloppyDriverReceiveInterrupt(void)
{
  // This routine will be called whenever the floppy drive issues its service
  // interrupt.  It will simply change a data value to indicate that one has
  // been received, and acknowldege the interrupt to the PIC.  It's up to
  // other routines to do something useful with the information.

  // Check whether to do the "sense interrupt status" command.
  if (readStatusOnInterrupt)
    {
      // Tell the diskette drive that the interrupt was serviced.  This
      // helps the drive stop doing the operation, and returns some status
      // operation, which we will save.

      commandWrite((unsigned char) 0x08);

      statusRegister0 = statusRead();
      currentTrack = (unsigned) statusRead();
      
      readStatusOnInterrupt = 0;
    }

  // Note that we got the interrupt.
  interruptReceived = 1;

  return;
}