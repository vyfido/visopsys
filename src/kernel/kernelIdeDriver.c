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
//  kernelIdeDriver.c
//

// Driver for standard ATA/ATAPI/IDE disks

#include "kernelDriverManagement.h" // Contains my prototypes
#include "kernelProcessorX86.h"
#include "kernelMultitasker.h"
#include "kernelMiscFunctions.h"
#include "kernelLock.h"
#include "kernelError.h"
#include <sys/errors.h>


void kernelIdeDriverReceiveInterrupt(void);

// Status register bits
#define IDE_CTRL_BSY   0x80
#define IDE_DRV_RDY    0x40
#define IDE_DRV_WRTFLT 0x20
#define IDE_DRV_SKCOMP 0x10
#define IDE_DRV_DRQ    0x08
#define IDE_DRV_CORDAT 0x04
#define IDE_DRV_IDX    0x02
#define IDE_DRV_ERR    0x01

// ATA commands
#define ATA_NOP            0x00
#define ATA_RECALIBRATE    0x10
#define ATA_READMULT_RET   0x20
#define ATA_READMULT       0x21
#define ATA_READECC_RET    0x22
#define ATA_READECC        0x23
#define ATA_WRITEMULT_RET  0x30
#define ATA_WRITEMULT      0x31
#define ATA_WRITEECC_RET   0x32
#define ATA_WRITEECC       0x33
#define ATA_VERIFYMULT_RET 0x40
#define ATA_VERIFYMULT     0x41
#define ATA_FORMATTRACK    0x50
#define ATA_SEEK           0x70
#define ATA_DIAG           0x90
#define ATA_INITPARAMS     0x91
#define ATA_ATAPIPACKET    0xA0
#define ATA_ATAPIIDENTIFY  0xA1
#define ATA_GETDEVINFO     0xEC

// Error codes
#define IDE_ADDRESSMARK    0
#define IDE_CYLINDER0      1
#define IDE_INVALIDCOMMAND 2
#define IDE_MEDIAREQ       3
#define IDE_SECTNOTFOUND   4
#define IDE_MEDIACHANGED   5
#define IDE_BADDATA        6
#define IDE_BADSECTOR      7
#define IDE_UNKNOWN        8
#define IDE_TIMEOUT        9

// Error messages
static char *errorMessages[] = {
  "Address mark not found",
  "Cylinder 0 not found",
  "Command aborted - invalid command",
  "Media change requested",
  "ID or target sector not found",
  "Media changed",
  "Uncorrectable data error",
  "Bad sector detected",
  "Unknown error",
  "Command timed out"
};

static struct {
  unsigned data;
  unsigned error;
  unsigned sectorCount;
  unsigned sectorNumber;
  unsigned cylinderLow;
  unsigned cylinderHigh;
  unsigned driveHead;
  unsigned comStat;
  unsigned altStat;

} ports[] = {
  { 0x01F0, 0x01F1, 0x01F2, 0x01F3, 0x01F4, 0x01F5, 0x01F6, 0x01F7, 0x03F6 },
  { 0x01F0, 0x01F1, 0x01F2, 0x01F3, 0x01F4, 0x01F5, 0x01F6, 0x01F7, 0x03F6 },
  { 0x0170, 0x0171, 0x0172, 0x0173, 0x0174, 0x0175, 0x0176, 0x0177, 0x03F6 },
  { 0x0170, 0x0171, 0x0172, 0x0173, 0x0174, 0x0175, 0x0176, 0x0177, 0x03F6 },
  { 0x00F0, 0x00F1, 0x00F2, 0x00F3, 0x00F4, 0x00F5, 0x00F6, 0x00F7, 0x03F6 },
  { 0x00F0, 0x00F1, 0x00F2, 0x00F3, 0x00F4, 0x00F5, 0x00F6, 0x00F7, 0x03F6 },
  { 0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077, 0x03F6 },
  { 0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077, 0x03F6 }
};

static volatile int controllerLock = 0;
static volatile int interruptReceived = 0;


static int selectDrive(int driveNum)
{
  // Selects the drive on the controller.  Returns 0 on success, negative
  // otherwise
  
  int status = 0;
  unsigned char data = 0;
  
  // Make sure the drive number is legal (7 or fewer, since this driver only
  // supports 8).
  if (driveNum > 7)
    return (status = ERR_INVALID);
  
  // Set the drive select bit in the drive/head register.  This will help to
  // introduce some delay between drive selection and any actual commands.
  // Drive number is LSBit.  Move drive number to bit 4.  NO LBA.
  data = (unsigned char) (((driveNum & 0x01) << 4) | 0xA0);
  kernelProcessorOutPort8(ports[driveNum].driveHead, data);
  
  // Return success
  return (status = 0);
}

		
static void CHSSetup(int driveNum, int head, int cylinder, int startSector)
{
  // This routine is strictly internal, and is used to set up the disk
  // controller registers with head, cylinder, sector and sector-count values
  // (prior to a read, write, seek, etc.).  It doesn't return anything.
  
  unsigned char commandByte = 0;
  
  // Set the drive and head.  The drive number for the particular controller
  // will be the least-significant bit of the selected drive number
  commandByte = (unsigned char) (((driveNum & 0x01) << 4) | (head & 0x0F));
  kernelProcessorOutPort8(ports[driveNum].driveHead, commandByte);
  
  // Set the 'cylinder low' number
  commandByte = (unsigned char) (cylinder & 0xFF);
  kernelProcessorOutPort8(ports[driveNum].cylinderLow, commandByte);
  
  // Set the 'cylinder high' number
  commandByte = (unsigned char) ((cylinder >> 8) & 0xFF);
  kernelProcessorOutPort8(ports[driveNum].cylinderHigh, commandByte);
  
  // Set the starting sector number
  commandByte = (unsigned char) startSector;
  kernelProcessorOutPort8(ports[driveNum].sectorNumber, commandByte);
  
  // Send a value of FFh (no precompensation) to the error/precomp register
  commandByte = 0xFF;
  kernelProcessorOutPort8(ports[driveNum].error, commandByte);
  
  return;
}


static void LBASetup(int driveNum, unsigned LBAAddress)
{
  // This routine is strictly internal, and is used to set up the disk
  // controller registers with an LBA drive address in the drive/head, cylinder
  // low, cylinder high, and start sector registers.  It doesn't return
  // anything.
  
  unsigned char commandByte = 0;
  
  // Set the drive and head.  The drive number for the particular controller
  // will be the least-significant bit of the selected drive number
  commandByte = (unsigned char) (0xE0 | ((driveNum & 0x00000001) << 4) |
				 ((LBAAddress >> 24) & 0x0000000F));
  kernelProcessorOutPort8(ports[driveNum].driveHead, commandByte);
  
  // Set the cylinder low byte with bits 8-15 of the LBA address
  commandByte = (unsigned char) ((LBAAddress >> 8) & 0x000000FF);
  kernelProcessorOutPort8(ports[driveNum].cylinderLow, commandByte);
  
  // Set the cylinder high byte with bits 16-23 of the LBA address
  commandByte = (unsigned char) ((LBAAddress >> 16) & 0x000000FF);
  kernelProcessorOutPort8(ports[driveNum].cylinderHigh, commandByte);
  
  // Set the sector number byte with bits 0-7 of the LBA address
  commandByte = (unsigned char) (LBAAddress & 0x000000FF);
  kernelProcessorOutPort8(ports[driveNum].sectorNumber, commandByte);
  
  // Send a value of FFh (no precompensation) to the error/precomp register
  commandByte = 0xFF;
  kernelProcessorOutPort8(ports[driveNum].error, commandByte);
  
  return;
}


static int evaluateError(int driveNum)
{
  // This routine will check the error status on the disk controller
  // of the selected drive.  It evaluates the returned byte and matches 
  // conditions to error codes and error messages
  
  int errorCode = 0;
  unsigned char data = 0;
  
  kernelProcessorInPort8(ports[driveNum].error, data);
  
  if (data & 0x01)
    errorCode = IDE_ADDRESSMARK;
  else if (data & 0x02)
    errorCode = IDE_CYLINDER0;
  else if (data & 0x04)
    errorCode = IDE_INVALIDCOMMAND;
  else if (data & 0x08)
    errorCode = IDE_MEDIAREQ;
  else if (data & 0x10)
    errorCode = IDE_SECTNOTFOUND;
  else if (data & 0x20)
    errorCode = IDE_MEDIACHANGED;
  else if (data & 0x40)
    errorCode = IDE_BADDATA;
  else if (data & 0x80)
    errorCode = IDE_BADSECTOR;
  else
    errorCode = IDE_UNKNOWN;
  
  return (errorCode);
}


static int waitOperationComplete(int driveNum)
{
  // This routine reads the "interrupt received" byte, waiting for the last
  // command to complete.  Every time the command has not completed, the
  // driver returns the remainder of the process' timeslice to the
  // multitasker.  When the interrupt byte becomes 1, it resets the byte and
  // checks the status of the selected disk controller
  
  int status = 0;
  unsigned char data = 0;
  unsigned startTime = kernelSysTimerRead();
  
  while (!interruptReceived)
    {
      // Yield the rest of this timeslice if we are in multitasking mode
      //kernelMultitaskerYield();
      
      if (kernelSysTimerRead() > (startTime + 20))
	break;
    }
  
  if (interruptReceived)
    {
      interruptReceived = 0;
      
      // Check for disk controller errors.  Test the error bit in the status
      // register.
      kernelProcessorInPort8(ports[driveNum].comStat, data);
      if (data & IDE_DRV_ERR)
	return (status = ERR_IO);
      else
	return (status = 0);
    }
  else
    // No interrupt -- timed out.
    return (status = ERR_IO);
}


static int pollBits(int driveNum, unsigned char mask, int onOff)
{
  // Returns when the requested status bits are on or off, or else the
  // timeout is reached
  
  unsigned startTime = kernelSysTimerRead();
  unsigned char data = 0;
  
  while (kernelSysTimerRead() < (startTime + 5))
    {
      // Get the contents of the status register for the controller of the 
      // selected drive.
      kernelProcessorInPort8(ports[driveNum].comStat, data);
      
      // We are interested in bits 6 and 4.  They should be set.
      if ((onOff && (data & mask)) || (!onOff && !(data & mask)))
	return (0);
    }
  
  // Timed out.
  return (-1);
}


/*
static int sendAtapiPacket(int driveNum, unsigned char *packet)
{
  int status = 0;
  unsigned char data = 0;
  int count;
  
  // Wait for the controller to be ready
  status = pollBits(driveNum, IDE_CTRL_BSY, 0);
  if (status < 0)
    {
      kernelTextPrintLine("controller not ready");
      return (status);
    }
  
  // Select the drive
  selectDrive(driveNum);
  
  // Clear the "interrupt received" byte
  interruptReceived = 0;
  
  // Send the "ATAPI packet" command
  kernelProcessorOutPort8(ports[driveNum].comStat, (char) ATA_ATAPIPACKET);
  
  // Wait for the drive to show busy
  status = pollBits(driveNum, IDE_DRV_RDY, 0);
  if (status < 0)
    {
      kernelTextPrintLine("not busy!");
      return (status);
    }
  
  kernelTextPrintLine(errorMessages[evaluateError(driveNum)]);

  // Wait for the data request bit
  status = pollBits(driveNum, IDE_DRV_DRQ, 1);
  if (status < 0)
    {
      kernelTextPrintLine("not ready for packet");
      return (status);
    }

  for (count = 0; count < 12; count ++)
    {
      data = packet[count];
      kernelProcessorOutPort8(ports[driveNum].data, data);
    }

  // Wait for the controller to finish the operation
  status = waitOperationComplete(driveNum);
  if (status < 0)
    {
      kernelError(kernel_error, errorMessages[evaluateError(driveNum)]);
      return (status);
    }
	  
  return (status = 0);
}
*/


static int readWriteSectors(int driveNum, unsigned logicalSector,
			    unsigned numSectors, void *buffer, int read)
{
  // This routine reads or writes sectors to/from the drive.  Returns 0 on
  // success, negative otherwise.
  
  int status = 0;
  unsigned doSectors = 0;
  unsigned char commandByte = 0;
  int count;
  
  // Wait for a lock on the controller
  status = kernelLockGet((void *) &controllerLock);
  if (status < 0)
    return (status);
  
  while (numSectors > 0)
    {
      doSectors = numSectors;
      if (doSectors > 256)
	doSectors = 256;
      
      // Wait for the controller to be ready
      status = pollBits(driveNum, IDE_CTRL_BSY, 0);
      if (status < 0)
	{
	  kernelError(kernel_error, errorMessages[IDE_TIMEOUT]);
	  kernelLockRelease((void *) &controllerLock);
	  return (status);
	}
      
      // Select the drive
      selectDrive(driveNum);
      
      // We always use LBA.  Break up the LBA value and deposit it into
      // the appropriate ports.
      LBASetup(driveNum, logicalSector);
      
      // This is where we send the actual command to the disk controller.  We
      // still have to get the number of sectors to read.
      
      // If it's 256, we need to change it to zero.
      if (doSectors == 256)
	commandByte = 0;
      else
	commandByte = (unsigned char) doSectors;
      kernelProcessorOutPort8(ports[driveNum].sectorCount, commandByte);
      
      // Clear the "interrupt received" byte
      interruptReceived = 0;
      
      // Wait for the selected drive to be ready
      status = pollBits(driveNum, IDE_DRV_RDY, 1);
      if (status < 0)
	{
	  kernelError(kernel_error, errorMessages[IDE_TIMEOUT]);
	  kernelLockRelease((void *) &controllerLock);
	  return (status);
	}
      
      if (read)
	// Send the "read multiple sectors" command
	commandByte = ATA_READMULT_RET; // 0xC4
      else
	// Send the "write multiple sectors" command
	commandByte = ATA_WRITEMULT_RET; // 0xC5
      kernelProcessorOutPort8(ports[driveNum].comStat, commandByte);
      
      for (count = 0; count < doSectors; count ++)
	{
	  // Transfer the data to/from the disk controller.
	  
	  if (!read)
	    {
	      commandByte = 0;
	      while (!(commandByte & IDE_DRV_DRQ))
		kernelProcessorInPort8(ports[driveNum].comStat, commandByte);
	      // Transfer one sector's worth of data to the controller.
	      kernelProcessorRepOutPort16(ports[driveNum].data, buffer, 256);
	    }
	  
	  // Wait for the controller to finish the operation
	  status = waitOperationComplete(driveNum);
	  if (status < 0)
	    {
	      kernelError(kernel_error,
			  errorMessages[evaluateError(driveNum)]);
	      kernelLockRelease((void *) &controllerLock);
	      return (status);
	    }
	  
	  if (read)
	    // Transfer one sector's worth of data from the controller.
	    kernelProcessorRepInPort16(ports[driveNum].data, buffer, 256);
	  
	  buffer += 512;
	}
      
      numSectors -= doSectors;
      logicalSector += doSectors;
    }
  
  // We are finished.  The data should be transferred.
  
  // Unlock the controller
  kernelLockRelease((void *) &controllerLock);
  
  return (status = 0);
}


static int detect(int driveNum, void *diskPointer)
{
  // Returns 1 if we detect a disk at the requested physical drive number.
  
  int status = 0;
  kernelPhysicalDisk *physicalDisk = (kernelPhysicalDisk *) diskPointer;
  unsigned short buffer[256];
  
  // Wait for a lock on the controller
  status = kernelLockGet((void *) &controllerLock);
  if (status < 0)
    return (status);
  
  // Wait for the controller to be ready
  status = pollBits(driveNum, IDE_CTRL_BSY, 0);
  if (status < 0)
    {
      kernelLockRelease((void *) &controllerLock);
      return (status);
    }
  
  // Select the drive
  selectDrive(driveNum);
  
  // Wait for the selected drive to be ready
  status = pollBits(driveNum, IDE_DRV_RDY, 1);
  if (status < 0)
    {
      kernelLockRelease((void *) &controllerLock);
      return (status);
    }
  
  // Seems to be an IDE device of one kind or another.
  
  physicalDisk->deviceNumber = driveNum;
  physicalDisk->dmaChannel = 3;
  kernelMemClear(buffer, 512);
  
  // First try a plain, ATA "identify device" command.  If the device doesn't
  // respond to that, try the ATAPI "identify packet device" command.
  
  // Clear the "interrupt received" byte
  interruptReceived = 0;
  
  // Send the "identify device" command
  kernelProcessorOutPort8(ports[driveNum].comStat, (unsigned char)
			  ATA_GETDEVINFO);
  
  // Wait for the controller to finish the operation
  status = waitOperationComplete(driveNum);
  
  if (status == 0)
    {
      // This is an ATA hard disk device
      physicalDisk->description = "ATA/IDE hard disk";
      physicalDisk->fixedRemovable = fixed;
      physicalDisk->type = idedisk;
      
      // Transfer one sector's worth of data from the controller.
      kernelProcessorRepInPort16(ports[driveNum].data, buffer, 256);
      
      // Return some information we know from our device info command
      physicalDisk->heads = (unsigned) buffer[3];
      physicalDisk->cylinders = (unsigned) buffer[1];
      physicalDisk->sectorsPerCylinder = (unsigned) buffer[6];
      physicalDisk->numSectors = *((unsigned *)(buffer + 0x78));
      physicalDisk->sectorSize = (unsigned) buffer[5];
    }
  
  else
    {
      // Possibly ATAPI?
      
      // Wait for the controller to be ready
      status = pollBits(driveNum, IDE_CTRL_BSY, 0);
      if (status < 0)
	{
	  kernelLockRelease((void *) &controllerLock);
	  return (status);
	}
      
      // Wait for the selected drive to be ready
      status = pollBits(driveNum, IDE_DRV_RDY, 1);
      if (status < 0)
        {
          kernelLockRelease((void *) &controllerLock);
          return (status);
        }
      
      // Clear the "interrupt received" byte
      interruptReceived = 0;
      
      // Send the "identify packet device" command
      kernelProcessorOutPort8(ports[driveNum].comStat, (unsigned char)
			      ATA_ATAPIIDENTIFY);
      
      // Wait for the controller to finish the operation
      status = waitOperationComplete(driveNum);
      if (status < 0)
        {
          kernelLockRelease((void *) &controllerLock);
          return (status);
        }
      
      // It's ATAPI.
      
      // This is an ATAPI device (possibly a CD-ROM)
      physicalDisk->description = "ATAPI CD-ROM";
      physicalDisk->fixedRemovable = removable;
      physicalDisk->type = idecdrom;
      
      // Transfer one sector's worth of data from the controller.
      kernelProcessorRepInPort16(ports[driveNum].data, buffer, 256);
      
      // Check packet interface supported
      if ((buffer[0] & ((unsigned short) 0xC000)) != 0x8000)
        {
          kernelLockRelease((void *) &controllerLock);
          return (status = ERR_NOTIMPLEMENTED);
        }
      
      // Return some information we know from our device info command
      physicalDisk->heads = (unsigned) buffer[3];
      physicalDisk->cylinders = (unsigned) buffer[1];
      physicalDisk->sectorsPerCylinder = (unsigned) buffer[6];
      physicalDisk->numSectors = *((unsigned *)(buffer + 0x78));
      physicalDisk->sectorSize = (unsigned) buffer[5];
      
      //sendAtapiPacket(driveNum, (unsigned char[])
      //{ 0x1B, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0 } );
    }
  
  kernelLockRelease((void *) &controllerLock);
  return (status = 1);
}


static int reset(int driveNum)
{
  // Does a software reset of the requested disk controller.
  
  int status = 0;
  unsigned char data = 0;
  
  // Wait for a lock on the controller
  status = kernelLockGet((void *) &controllerLock);
  if (status < 0)
    return (status);
  
  // Wait for the controller to be ready
  status = pollBits(driveNum, IDE_CTRL_BSY, 0);
  if (status < 0)
    {
      kernelError(kernel_error, errorMessages[IDE_TIMEOUT]);
      kernelLockRelease((void *) &controllerLock);
      return (status);
    }
  
  // Select the drive
  selectDrive(driveNum);
  
  // Get the adapter status register number of the selected drive
  kernelProcessorInPort8(ports[driveNum].comStat, data);
  
  // We need to set bit 2 for at least 4.8 microseconds.  We will set the bit
  // and then we will tell the multitasker to make us "wait" for at least
  // one timer tick
  data |= 0x04;
  kernelProcessorOutPort8(ports[driveNum].comStat, data);
  
  kernelMultitaskerWait(1);
  
  // Clear bit 2 again
  data &= 0xFB;
  kernelProcessorOutPort8(ports[driveNum].comStat, data);
  
  // Unlock the controller
  kernelLockRelease((void *) &controllerLock);
  
  return (status = 0);
}


static int recalibrate(int driveNum)
{
  // Recalibrates the requested drive, causing it to seek to cylinder 0
  
  int status = 0;
  unsigned char commandByte;
  
  // Wait for a lock on the controller
  status = kernelLockGet((void *) &controllerLock);
  if (status < 0)
    return (status);
  
  // Wait for the controller to be ready
  status = pollBits(driveNum, IDE_CTRL_BSY, 0);
  if (status < 0)
    {
      kernelError(kernel_error, errorMessages[IDE_TIMEOUT]);
      kernelLockRelease((void *) &controllerLock);
      return (status);
    }
  
  // Select the drive
  selectDrive(driveNum);
  
  // Call the routine that will send drive/head information to the controller
  // registers prior to the recalibration.  Sector value: don't care.
  // Cylinder value: 0 by definition.  Head: Head zero is O.K.
  CHSSetup(driveNum, 0, 0, 0);
  
  // Clear the "interrupt received" byte
  interruptReceived = 0;
  
  // Wait for the selected drive to be ready
  status = pollBits(driveNum, IDE_DRV_RDY, 1);
  if (status < 0)
    {
      kernelError(kernel_error, errorMessages[IDE_TIMEOUT]);
      kernelLockRelease((void *) &controllerLock);
      return (status);
    }
  
  // Send the recalibrate command
  commandByte = ATA_RECALIBRATE;
  kernelProcessorOutPort8(ports[driveNum].comStat, commandByte);
  
  // Wait for the recalibration to complete
  status = waitOperationComplete(driveNum);
  
  // Unlock the controller
  kernelLockRelease((void *) &controllerLock);
  
  if (status < 0)
    kernelError(kernel_error, errorMessages[evaluateError(driveNum)]);
  
  return (status);
}


static int readSectors(int driveNum, unsigned logicalSector,
		       unsigned numSectors, void *buffer)
{
  // This routine is a wrapper for the readWriteSectors routine.
  return (readWriteSectors(driveNum, logicalSector, numSectors, buffer,
			   1));  // Read operation
}


static int writeSectors(int driveNum, unsigned logicalSector,
			unsigned numSectors, void *buffer)
{
  // This routine is a wrapper for the readWriteSectors routine.
  return (readWriteSectors(driveNum, logicalSector, numSectors, buffer,
			   0));  // Write operation
}


static kernelDiskDriver defaultIdeDriver =
  {
    detect,
    NULL,
    reset,
    recalibrate,
    NULL,
    NULL,
    NULL,
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


int kernelIdeDriverInitialize(void)
{
  // This initializes the driver and returns success.
  
  int status = 0;
  
  // Clear the "interrupt received" byte
  interruptReceived = 0;
  
  status = kernelDriverRegister(ideDriver, &defaultIdeDriver);
  
  return (status);
}


void kernelIdeDriverReceiveInterrupt(void)
{
  // This routine will be called whenever the disk controller issues its
  // service interrupt.  It will simply change a data value to indicate that
  // one has been received.  It's up to other routines to do something useful
  // with the information.
  interruptReceived = 1;
  return;
}