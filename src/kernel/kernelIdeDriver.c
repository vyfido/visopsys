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
//  kernelIdeDriver.c
//

// Driver for standard ATA/ATAPI/IDE disks

#include "kernelIdeDriver.h"
#include "kernelDriverManagement.h"
#include "kernelParameters.h"
#include "kernelProcessorX86.h"
#include "kernelMultitasker.h"
#include "kernelMiscFunctions.h"
#include "kernelLock.h"
#include "kernelError.h"
#include <sys/errors.h>


// List of IDE ports, per device number
static idePorts ports[] = {
  { 0x01F0, 0x01F1, 0x01F2, 0x01F3, 0x01F4, 0x01F5, 0x01F6, 0x01F7, 0x03F6 },
  { 0x01F0, 0x01F1, 0x01F2, 0x01F3, 0x01F4, 0x01F5, 0x01F6, 0x01F7, 0x03F6 },
  { 0x0170, 0x0171, 0x0172, 0x0173, 0x0174, 0x0175, 0x0176, 0x0177, 0x0376 },
  { 0x0170, 0x0171, 0x0172, 0x0173, 0x0174, 0x0175, 0x0176, 0x0177, 0x0376 },
  { 0x00F0, 0x00F1, 0x00F2, 0x00F3, 0x00F4, 0x00F5, 0x00F6, 0x00F7, 0x02F6 },
  { 0x00F0, 0x00F1, 0x00F2, 0x00F3, 0x00F4, 0x00F5, 0x00F6, 0x00F7, 0x02F6 },
  { 0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077, 0x0276 },
  { 0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077, 0x0276 }
};

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

static kernelPhysicalDisk *disks[MAXHARDDISKS];

static volatile int controllerLock = 0;
static volatile int interruptReceived = 0;

static kernelDiskDriver defaultIdeDriver = {
  kernelIdeDriverDetect,
  kernelIdeDriverRegisterDevice,
  kernelIdeDriverReset,
  kernelIdeDriverRecalibrate,
  NULL,
  kernelIdeDriverSetLockState,
  kernelIdeDriverSetDoorState,
  NULL,
  kernelIdeDriverReadSectors,
  kernelIdeDriverWriteSectors,
};


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
  kernelProcessorOutPort8(ports[driveNum].featErr, commandByte);
  
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
  kernelProcessorOutPort8(ports[driveNum].featErr, commandByte);
  
  return;
}


static int evaluateError(int driveNum)
{
  // This routine will check the error status on the disk controller
  // of the selected drive.  It evaluates the returned byte and matches 
  // conditions to error codes and error messages
  
  int errorCode = 0;
  unsigned char data = 0;
  
  kernelProcessorInPort8(ports[driveNum].featErr, data);
  
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
  
  // Check for disk controller errors.  Test the error bit in the status
  // register.
  kernelProcessorInPort8(ports[driveNum].comStat, data);
  if (data & IDE_DRV_ERR)
    return (status = ERR_IO);

  else
    {
      if (interruptReceived)
	{
	  interruptReceived = 0;
	  return (status = 0);
	}
      else
	// No interrupt, no error -- just timed out.
	return (status = ERR_IO);
    }
}


static int pollStatus(int driveNum, unsigned char mask, int onOff)
{
  // Returns when the requested status bits are on or off, or else the
  // timeout is reached
  
  unsigned startTime = kernelSysTimerRead();
  unsigned char data = 0;
  
  while (kernelSysTimerRead() < (startTime + 20))
    {
      // Get the contents of the status register for the controller of the 
      // selected drive.
      kernelProcessorInPort8(ports[driveNum].altComStat, data);

      if ((onOff && (data & mask)) || (!onOff && !(data & mask)))
	return (0);
    }

  // Timed out.
  return (-1);
}


static int sendAtapiPacket(int driveNum, unsigned byteCount,
			   unsigned char *packet)
{
  int status = 0;
  unsigned char data = 0;

  kernelProcessorOutPort8(ports[driveNum].featErr, 0);
  //kernelProcessorOutPort8(ports[driveNum].sectorCount, 0);
  //kernelProcessorOutPort8(ports[driveNum].sectorNumber, 0);
  data = (unsigned char) (byteCount & 0x000000FF);
  kernelProcessorOutPort8(ports[driveNum].cylinderLow, data);
  data = (unsigned char) ((byteCount & 0x0000FF00) >> 8);
  kernelProcessorOutPort8(ports[driveNum].cylinderHigh, data);

  // Wait for the controller to be ready, and data request not active
  status = pollStatus(driveNum, (IDE_CTRL_BSY | IDE_DRV_DRQ), 0);
  if (status < 0)
    return (status);
  
  // Send the "ATAPI packet" command
  kernelProcessorOutPort8(ports[driveNum].comStat, (unsigned char)
			  ATA_ATAPIPACKET);

  // Wait for the data request bit
  status = pollStatus(driveNum, IDE_DRV_DRQ, 1);
  if (status < 0)
    return (status);

  // Wait for the controller to be ready
  status = pollStatus(driveNum, IDE_CTRL_BSY, 0);
  if (status < 0)
    return (status);

  kernelProcessorRepOutPort16(ports[driveNum].data, packet, 6);

  return (status = 0);
}


static int atapiStartStop(int driveNum, int state)
{
  // Start or stop an ATAPI device

  int status = 0;
  unsigned short dataWord = 0;

  if (state)
    {
      status = sendAtapiPacket(driveNum, 0, ATAPI_PACKET_START);
      if (status < 0)
	return (status);

      status = sendAtapiPacket(driveNum, 0, ATAPI_PACKET_READCAPACITY);
      if (status < 0)
	return (status);

      // Read the number of sectors
      pollStatus(driveNum, IDE_DRV_DRQ, 1);
      kernelProcessorInPort16(ports[driveNum].data, dataWord);
      disks[driveNum]->numSectors =
	(((unsigned)(dataWord & 0x00FF)) << 24);
      disks[driveNum]->numSectors |= 
	(((unsigned)(dataWord & 0xFF00)) << 8);
      pollStatus(driveNum, IDE_DRV_DRQ, 1);
      kernelProcessorInPort16(ports[driveNum].data, dataWord);
      disks[driveNum]->numSectors |=
	(((unsigned)(dataWord & 0x00FF)) << 8);
      disks[driveNum]->numSectors |=
	(((unsigned)(dataWord & 0xFF00)) >> 8);

      // Read the sector size
      pollStatus(driveNum, IDE_DRV_DRQ, 1);
      kernelProcessorInPort16(ports[driveNum].data, dataWord);
      disks[driveNum]->sectorSize =
	(((unsigned)(dataWord & 0x00FF)) << 24);
      disks[driveNum]->sectorSize |=
	(((unsigned)(dataWord & 0xFF00)) << 8);
      pollStatus(driveNum, IDE_DRV_DRQ, 1);
      kernelProcessorInPort16(ports[driveNum].data, dataWord);
      disks[driveNum]->sectorSize |=
	(((unsigned)(dataWord & 0x00FF)) << 8);
      disks[driveNum]->sectorSize |=
	(((unsigned)(dataWord & 0xFF00)) >> 8);

      // If there's no disk, the number of sectors will be illegal.  Set
      // to the maximum value and quit
      if ((disks[driveNum]->numSectors == 0) ||
	  (disks[driveNum]->numSectors == 0xFFFFFFFF))
	{
	  disks[driveNum]->numSectors = 0xFFFFFFFF;
	  disks[driveNum]->sectorSize = 2048;
	  kernelError(kernel_error, "No media in drive %s",
		      disks[driveNum]->name);
	  return (status = ERR_NOSUCHENTRY);
	}

      disks[driveNum]->logical[0].numSectors = disks[driveNum]->numSectors;
      
      // Read the TOC (Table Of Contents)
      status = sendAtapiPacket(driveNum, 0, ATAPI_PACKET_READTOC);
      if (status < 0)
	return (status);

      // Ignore the first four words
      pollStatus(driveNum, IDE_DRV_DRQ, 1);
      kernelProcessorInPort16(ports[driveNum].data, dataWord);
      pollStatus(driveNum, IDE_DRV_DRQ, 1);
      kernelProcessorInPort16(ports[driveNum].data, dataWord);
      pollStatus(driveNum, IDE_DRV_DRQ, 1);
      kernelProcessorInPort16(ports[driveNum].data, dataWord);
      pollStatus(driveNum, IDE_DRV_DRQ, 1);
      kernelProcessorInPort16(ports[driveNum].data, dataWord);

      // Read the LBA address of the start of the last track
      pollStatus(driveNum, IDE_DRV_DRQ, 1);
      kernelProcessorInPort16(ports[driveNum].data, dataWord);
      disks[driveNum]->lastSession =
	(((unsigned)(dataWord & 0x00FF)) << 24);
      disks[driveNum]->lastSession |=
	(((unsigned)(dataWord & 0xFF00)) << 8);
      pollStatus(driveNum, IDE_DRV_DRQ, 1);
      kernelProcessorInPort16(ports[driveNum].data, dataWord);
      disks[driveNum]->lastSession |=
	(((unsigned)(dataWord & 0x00FF)) << 8);
      disks[driveNum]->lastSession |=
	(((unsigned)(dataWord & 0xFF00)) >> 8);

      disks[driveNum]->motorState = 1;
    }
  else
    {
      status = sendAtapiPacket(driveNum, 0, ATAPI_PACKET_STOP);

      disks[driveNum]->motorState = 0;
    }

  return (status);
}


static int readWriteSectors(int driveNum, unsigned logicalSector,
			    unsigned numSectors, void *buffer, int read)
{
  // This routine reads or writes sectors to/from the drive.  Returns 0 on
  // success, negative otherwise.
  
  int status = 0;
  unsigned doSectors = 0;
  unsigned char data = 0;
  unsigned transferBytes = 0;
  int count;
  
  if (!disks[driveNum])
    {
      kernelError(kernel_error, "No such drive %d", driveNum);
      return (status = ERR_NOSUCHENTRY);
    }
  
  // Wait for a lock on the controller
  status = kernelLockGet((void *) &controllerLock);
  if (status < 0)
    return (status);
  
  // Select the drive
  selectDrive(driveNum);

  // If it's an ATAPI device
  if (disks[driveNum]->type == idecdrom)
    {
      // If it's not started, we start it
      if (!disks[driveNum]->motorState)
	{
	  status = atapiStartStop(driveNum, 1);
	  if (status < 0)
	    {
	      kernelLockRelease((void *) &controllerLock);
	      return (status);
	    }
	}

      transferBytes = (numSectors * disks[driveNum]->sectorSize);

      status = sendAtapiPacket(driveNum, 0xFFFF, ((unsigned char[])
	  { ATAPI_READ12, 0,
	    (unsigned char)((logicalSector & 0xFF000000) >> 24),
	    (unsigned char)((logicalSector & 0x00FF0000) >> 16),
	    (unsigned char)((logicalSector & 0x0000FF00) >> 8),
	    (unsigned char)(logicalSector & 0x000000FF),
	    (unsigned char)((numSectors & 0xFF000000) >> 24),
	    (unsigned char)((numSectors & 0x00FF0000) >> 16),
	    (unsigned char)((numSectors & 0x0000FF00) >> 8),
	    (unsigned char)(numSectors & 0x000000FF),
	    0, 0 } ));
      if (status < 0)
	{
	  kernelLockRelease((void *) &controllerLock);
	  return (status);
	}

      while (transferBytes)
	{
	  // Wait for the controller to assert data request
	  while (pollStatus(driveNum, IDE_DRV_DRQ, 1))
	    {
	      // Timed out.  Check for an error...
	      kernelProcessorInPort8(ports[driveNum].altComStat, data);
	      if (data & IDE_DRV_ERR)
		{
		  kernelError(kernel_error,
			      errorMessages[evaluateError(driveNum)]);
		  kernelLockRelease((void *) &controllerLock);
		  return (status);
		}
	    }

	  // How many words to read?
	  unsigned bytes = 0;
	  kernelProcessorInPort8(ports[driveNum].cylinderLow, data);
	  bytes = data;
	  kernelProcessorInPort8(ports[driveNum].cylinderHigh, data);
	  bytes |= (data << 8);

	  unsigned words = (bytes >> 1);

	  // Transfer the number of words from the drive.
	  kernelProcessorRepInPort16(ports[driveNum].data, buffer, words);

	  buffer += (words << 1);
	  transferBytes -= (words << 1);

	  // Just in case it's an odd number
	  if (bytes % 2)
	    {
	      kernelProcessorInPort8(ports[driveNum].data, data);
	      ((unsigned char *) buffer)[0] = data;
	      buffer += 1;
	      transferBytes -= 1;
	    }
	}

      kernelLockRelease((void *) &controllerLock);
      return (status = 0);
    }
      
  while (numSectors > 0)
    {
      doSectors = numSectors;
      if (doSectors > 256)
	doSectors = 256;
      
      // Wait for the controller to be ready
      status = pollStatus(driveNum, IDE_CTRL_BSY, 0);
      if (status < 0)
	{
	  kernelError(kernel_error, errorMessages[IDE_TIMEOUT]);
	  kernelLockRelease((void *) &controllerLock);
	  return (status);
	}
      
      // We always use LBA.  Break up the LBA value and deposit it into
      // the appropriate ports.
      LBASetup(driveNum, logicalSector);
      
      // This is where we send the actual command to the disk controller.  We
      // still have to get the number of sectors to read.
      
      // If it's 256, we need to change it to zero.
      if (doSectors == 256)
	data = 0;
      else
	data = (unsigned char) doSectors;
      kernelProcessorOutPort8(ports[driveNum].sectorCount, data);
      
      // Clear the "interrupt received" byte
      interruptReceived = 0;
      
      // Wait for the selected drive to be ready
      status = pollStatus(driveNum, IDE_DRV_RDY, 1);
      if (status < 0)
	{
	  kernelError(kernel_error, errorMessages[IDE_TIMEOUT]);
	  kernelLockRelease((void *) &controllerLock);
	  return (status);
	}
      
      if (read)
	// Send the "read multiple sectors" command
	data = ATA_READMULT_RET; // 0xC4
      else
	// Send the "write multiple sectors" command
	data = ATA_WRITEMULT_RET; // 0xC5
      kernelProcessorOutPort8(ports[driveNum].comStat, data);
      
      for (count = 0; count < doSectors; count ++)
	{
	  // Transfer the data to/from the disk controller.
	  
	  if (!read)
	    {
	      // Wait for DRQ
	      while (pollStatus(driveNum, IDE_DRV_DRQ, 1));

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


static inline void atapiDelay(void)
{
  // Delay 3 timer ticks
  unsigned startTime = kernelSysTimerRead();
  while (kernelSysTimerRead() < (startTime + 3));
}


static int reset(int driveNum)
{
  // Does a software reset of the requested disk controller.
  
  int status = 0;
  unsigned startTime = 0;
  int master = (driveNum - (driveNum % 2));
  int slave = (master + 1);
  unsigned char data = 0;

  // We need to set bit 2 for at least 4.8 microseconds.  We will set the bit
  // and then we will tell the multitasker to make us "wait" for at least
  // one timer tick
  kernelProcessorOutPort8(ports[master].altComStat, 0x04);
  
  // Delay 1/20th second
  startTime = kernelSysTimerRead();
  while (kernelSysTimerRead() < (startTime + 1));
  
  // Clear bit 2 again
  kernelProcessorOutPort8(ports[master].altComStat, 0);

  // If either the slave or the master on this controller is an ATAPI device,
  // delay
  if ((disks[master] && (disks[master]->type == idecdrom)) ||
      (disks[slave] && (disks[slave]->type == idecdrom)))
    atapiDelay();

  // Wait for controller ready
  status = pollStatus(master, IDE_CTRL_BSY, 0);
  if (status < 0)
    {
      kernelError(kernel_error, "Controller not ready after reset");
      return (status);
    }

  // Read the error status
  kernelProcessorInPort8(ports[master].altComStat, data);
  if (data & IDE_DRV_ERR)
    kernelError(kernel_error, errorMessages[evaluateError(master)]);

  // If there is a slave, make sure it is ready
  if (disks[slave])
    {
      // Select the slave
      selectDrive(slave);

      // Error, until the slave is ready
      status = -1;

      unsigned startTime = kernelSysTimerRead();
      unsigned char sectorCount = 0, sectorNumber = 0;
	  
      while (kernelSysTimerRead() < (startTime + 20))
	{
	  // Read the sector count and sector number registers
	  kernelProcessorInPort8(ports[slave].sectorCount, sectorCount);
	  kernelProcessorInPort8(ports[slave].sectorNumber, sectorNumber);

	  if ((sectorCount == 1) && (sectorNumber == 1))
	    {
	      // Wait for the controller to be non-busy
	      status = pollStatus(slave, IDE_CTRL_BSY, 0);
	      if (status < 0)
		{
		  kernelError(kernel_error, "Controller not ready after "
			      "reset");
		  return (status);
		}

	      break;
	    }
	}

      // Read the error status
      kernelProcessorInPort8(ports[slave].altComStat, data);
      if (data & IDE_DRV_ERR)
	kernelError(kernel_error, errorMessages[evaluateError(slave)]);
    }

  // Select the device again
  selectDrive(driveNum);
  
  return (status);
}


static int atapiReset(int driveNum)
{
  int status = 0;

  // Wait for controller ready
  status = pollStatus(driveNum, IDE_CTRL_BSY, 0);
  if (status < 0)
    return (status);

  // Disable "revert to power on defaults"
  kernelProcessorOutPort8(ports[driveNum].featErr, (unsigned char) 0xCC);
  kernelProcessorOutPort8(ports[driveNum].comStat, (unsigned char)
			  ATA_ATAPISETFEAT);
  
  // Wait for it...
  atapiDelay();

  // Wait for controller ready
  status = pollStatus(driveNum, IDE_CTRL_BSY, 0);
  if (status < 0)
    return (status);

  // Do ATAPI reset
  kernelProcessorOutPort8(ports[driveNum].comStat, (unsigned char)
			  ATA_ATAPIRESET);
  
  // Wait for it...
  atapiDelay();

  // Wait for the reset to finish
  status = pollStatus(driveNum, IDE_CTRL_BSY, 0);
  if (status < 0)
    return (status);

  return (status = 0);
}


static int atapiSetLockState(int driveNum, int lock)
{
  // Lock or unlock an ATAPI device

  int status = 0;

  if (lock)
    status = sendAtapiPacket(driveNum, 0, ATAPI_PACKET_LOCK);
  else
    status = sendAtapiPacket(driveNum, 0, ATAPI_PACKET_UNLOCK);

  disks[driveNum]->lockState = lock;

  return (status);
}


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


int kernelIdeDriverDetect(int driveNum, void *diskPointer)
{
  // Returns 1 if we detect a disk at the requested physical drive number.
  
  int status = 0;
  unsigned char cylinderLow = 0, cylinderHigh = 0;
  unsigned short buffer[256];

  // Check params
  if (diskPointer == NULL)
    {
      kernelError(kernel_error, "Disk structure is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  // Wait for a lock on the controller
  status = kernelLockGet((void *) &controllerLock);
  if (status < 0)
    return (status);
  
  // Select the drive
  selectDrive(driveNum);
  
  // Wait for the controller to be ready
  status = pollStatus(driveNum, IDE_CTRL_BSY, 0);
  if (status < 0)
    {
      kernelLockRelease((void *) &controllerLock);
      return (status);
    }
  
  // Wait for the selected drive to be ready
  status = pollStatus(driveNum, IDE_DRV_RDY, 1);
  if (status < 0)
    {
      kernelLockRelease((void *) &controllerLock);
      return (status);
    }
  
  // Seems to be an IDE device of one kind or another.
  disks[driveNum] = (kernelPhysicalDisk *) diskPointer;
  disks[driveNum]->description = "Unknown IDE disk";
  disks[driveNum]->deviceNumber = driveNum;
  disks[driveNum]->dmaChannel = 3;
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
      disks[driveNum]->description = "ATA/IDE hard disk";
      disks[driveNum]->fixedRemovable = fixed;
      disks[driveNum]->type = idedisk;
      
      // Transfer one sector's worth of data from the controller.
      kernelProcessorRepInPort16(ports[driveNum].data, buffer, 256);
      
      // Return some information we know from our device info command
      disks[driveNum]->heads = (unsigned) buffer[3];
      disks[driveNum]->cylinders = (unsigned) buffer[1];
      disks[driveNum]->sectorsPerCylinder = (unsigned) buffer[6];
      disks[driveNum]->numSectors = *((unsigned *)(buffer + 0x78));
      disks[driveNum]->sectorSize = (unsigned) buffer[5];
    }
  
  else
    {
      // Possibly ATAPI?
      
      // Read the cylinder low + high registers
      kernelProcessorInPort8(ports[driveNum].cylinderLow, cylinderLow);
      kernelProcessorInPort8(ports[driveNum].cylinderHigh, cylinderHigh);

      if ((cylinderLow != 0x14) || (cylinderHigh != 0xEB))
	{
	  kernelLockRelease((void *) &controllerLock);
          return (status);
	}

      // Send the "identify packet device" command
      kernelProcessorOutPort8(ports[driveNum].comStat, (unsigned char)
			      ATA_ATAPIIDENTIFY);

      // Wait for BSY=0
      status = pollStatus(driveNum, IDE_CTRL_BSY, 0);
      if (status < 0)
	{
	  kernelLockRelease((void *) &controllerLock);
	  return (status);
	}

      // Check for the signature again
      if ((cylinderLow != 0x14) || (cylinderHigh != 0xEB))
	{
	  kernelLockRelease((void *) &controllerLock);
          return (status);
	}

      // This is an ATAPI device (such as a CD-ROM)

      // Transfer one sector's worth of data from the controller.
      kernelProcessorRepInPort16(ports[driveNum].data, buffer, 256);
      
      // Check ATAPI packet interface supported
      if ((buffer[0] & ((unsigned short) 0xC000)) != 0x8000)
        {
          kernelLockRelease((void *) &controllerLock);
          return (status = ERR_NOTIMPLEMENTED);
        }
      
      // Removable?
      if (buffer[0] & (unsigned short) 0x0080)
	disks[driveNum]->fixedRemovable = removable;
      else
	disks[driveNum]->fixedRemovable = fixed;

      // Device type: Bits 12-8 of buffer[0] should indicate 0x05 for CDROM,
      // but we will just warn if it isn't for now
      disks[driveNum]->type = ((buffer[0] & (unsigned short) 0x1F00) >> 8);
      if (disks[driveNum]->type != 0x05)
	kernelError(kernel_warn, "ATAPI device type %x may not be supported",
		    disks[driveNum]->type);

      disks[driveNum]->description = "ATAPI CD-ROM";
      disks[driveNum]->type = idecdrom;

      if ((buffer[0] & (unsigned short) 0x0003) != 0)
	kernelError(kernel_warn, "ATAPI packet size not 12");

      atapiReset(driveNum);

      // Return some information we know from our device info command
      disks[driveNum]->heads = (unsigned) buffer[3];
      disks[driveNum]->cylinders = (unsigned) buffer[1];
      disks[driveNum]->sectorsPerCylinder = (unsigned) buffer[6];
      disks[driveNum]->numSectors = 0xFFFFFFFF;
      disks[driveNum]->sectorSize = 2048;
    }

  kernelLockRelease((void *) &controllerLock);
  return (status = 1);
}


int kernelIdeDriverRegisterDevice(void *diskPointer)
{
  // The disk routine has finished setting up the physical device.  Keep a
  // pointer to it.

  int status = 0;
  kernelPhysicalDisk *physicalDisk = (kernelPhysicalDisk *) diskPointer;

  // Check params
  if (physicalDisk == NULL)
    {
      kernelError(kernel_error, "Disk structure is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  disks[physicalDisk->deviceNumber] = physicalDisk;
  return (status = 0);
}


int kernelIdeDriverReset(int driveNum)
{
  // Does a software reset of the requested disk controller.
  
  int status = 0;
  
  if (!disks[driveNum])
    {
      kernelError(kernel_error, "No such drive %d", driveNum);
      return (status = ERR_NOSUCHENTRY);
    }

  // Wait for a lock on the controller
  status = kernelLockGet((void *) &controllerLock);
  if (status < 0)
    return (status);

  // Select the drive
  selectDrive(driveNum);

  status = reset(driveNum);
  
  // Unlock the controller
  kernelLockRelease((void *) &controllerLock);
  
  return (status);
}


int kernelIdeDriverRecalibrate(int driveNum)
{
  // Recalibrates the requested drive, causing it to seek to cylinder 0
  
  int status = 0;
  
  if (!disks[driveNum])
    {
      kernelError(kernel_error, "No such drive %d", driveNum);
      return (status = ERR_NOSUCHENTRY);
    }

  // Don't try to recalibrate ATAPI 
  if (disks[driveNum]->type == idecdrom)
    return (status = 0);

  // Wait for a lock on the controller
  status = kernelLockGet((void *) &controllerLock);
  if (status < 0)
    return (status);
  
  // Select the drive
  selectDrive(driveNum);
  
  // Wait for the controller to be ready
  status = pollStatus(driveNum, IDE_CTRL_BSY, 0);
  if (status < 0)
    {
      kernelError(kernel_error, errorMessages[IDE_TIMEOUT]);
      kernelLockRelease((void *) &controllerLock);
      return (status);
    }
  
  // Call the routine that will send drive/head information to the controller
  // registers prior to the recalibration.  Sector value: don't care.
  // Cylinder value: 0 by definition.  Head: Head zero is O.K.
  CHSSetup(driveNum, 0, 0, 0);
  
  // Wait for the selected drive to be ready
  status = pollStatus(driveNum, IDE_DRV_RDY, 1);
  if (status < 0)
    {
      kernelError(kernel_error, errorMessages[IDE_TIMEOUT]);
      kernelLockRelease((void *) &controllerLock);
      return (status);
    }
  
  // Clear the "interrupt received" byte
  interruptReceived = 0;
  
  // Send the recalibrate command
  kernelProcessorOutPort8(ports[driveNum].comStat, (unsigned char)
			  ATA_RECALIBRATE);
  
  // Wait for the recalibration to complete
  status = waitOperationComplete(driveNum);
  
  // Unlock the controller
  kernelLockRelease((void *) &controllerLock);
  
  if (status < 0)
    kernelError(kernel_error, errorMessages[evaluateError(driveNum)]);
  
  return (status);
}


int kernelIdeDriverSetLockState(int driveNum, int lock)
{
  // This will lock or unlock the CD-ROM door

  int status = 0;

  if (!disks[driveNum])
    {
      kernelError(kernel_error, "No such drive %d", driveNum);
      return (status = ERR_NOSUCHENTRY);
    }

  // Wait for a lock on the controller
  status = kernelLockGet((void *) &controllerLock);
  if (status < 0)
    return (status);
  
  // Select the drive
  selectDrive(driveNum);

  status = atapiSetLockState(driveNum, lock);

  // Unlock the controller
  kernelLockRelease((void *) &controllerLock);

  return (status);
}


int kernelIdeDriverSetDoorState(int driveNum, int open)
{
  // This will open or close the CD-ROM door

  int status = 0;

  if (!disks[driveNum])
    {
      kernelError(kernel_error, "No such drive %d", driveNum);
      return (status = ERR_NOSUCHENTRY);
    }

  if (open && disks[driveNum]->lockState)
    {
      // Don't try this if the door is locked
      kernelError(kernel_error, "Drive door is locked");
      return (status = ERR_PERMISSION);
    }

  // Wait for a lock on the controller
  status = kernelLockGet((void *) &controllerLock);
  if (status < 0)
    return (status);
  
  // Select the drive
  selectDrive(driveNum);

  if (open)
    {
      // If the disk is started, stop it
      if (disks[driveNum]->motorState)
	{
	  status = atapiStartStop(driveNum, 0);
	  if (status < 0)
	    {
	      kernelLockRelease((void *) &controllerLock);
	      return (status);
	    }
	}

      status = sendAtapiPacket(driveNum, 0, ATAPI_PACKET_EJECT);
    }

  else
    status = sendAtapiPacket(driveNum, 0, ATAPI_PACKET_CLOSE);

  // Unlock the controller
  kernelLockRelease((void *) &controllerLock);
  
  return (status);
}


int kernelIdeDriverReadSectors(int driveNum, unsigned logicalSector,
			       unsigned numSectors, void *buffer)
{
  // This routine is a wrapper for the readWriteSectors routine.
  return (readWriteSectors(driveNum, logicalSector, numSectors, buffer,
			   1));  // Read operation
}


int kernelIdeDriverWriteSectors(int driveNum, unsigned logicalSector,
				unsigned numSectors, void *buffer)
{
  // This routine is a wrapper for the readWriteSectors routine.
  return (readWriteSectors(driveNum, logicalSector, numSectors, buffer,
			   0));  // Write operation
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
