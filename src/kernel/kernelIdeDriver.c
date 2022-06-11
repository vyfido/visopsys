//
//  Visopsys
//  Copyright (C) 1998-2007 J. Andrew McLaughlin
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
#include "kernelBus.h"
#include "kernelDebug.h"
#include "kernelDisk.h"
#include "kernelError.h"
#include "kernelInterrupt.h"
#include "kernelLog.h"
#include "kernelMain.h"
#include "kernelMalloc.h"
#include "kernelMemory.h"
#include "kernelMisc.h"
#include "kernelMultitasker.h"
#include "kernelParameters.h"
#include "kernelPciDriver.h"
#include "kernelPic.h"
#include "kernelProcessorX86.h"
#include "kernelSysTimer.h"
#include <stdio.h>
#include <stdlib.h>

// Some little macro shortcuts
#define CHANNEL(driveNum)     controller.channel[driveNum / 2]
#define DISK(driveNum)        CHANNEL(driveNum).disk[driveNum % 2]
#define DISKISMULTI(driveNum) (DISK(driveNum).featureFlags & IDE_FEATURE_MULTI)
#define DISKISDMA(driveNum)						\
  (controller.busMaster && (DISK(driveNum).featureFlags & IDE_FEATURE_DMA))
#define DISKISSMART(driveNum) (DISK(driveNum).featureFlags & IDE_FEATURE_SMART)
#define DISKISRCACHE(driveNum)				\
  (DISK(driveNum).featureFlags & IDE_FEATURE_RCACHE)
#define DISKISWCACHE(driveNum)				\
  (DISK(driveNum).featureFlags & IDE_FEATURE_WCACHE)
#define DISKIS48(driveNum)    (DISK(driveNum).featureFlags & IDE_FEATURE_48BIT)
#define BMPORT_CH0_CMD        (controller.busMasterIo)
#define BMPORT_CH0_STATUS     (controller.busMasterIo + 2)
#define BMPORT_CH0_PRDADDR    (controller.busMasterIo + 4)
#define BMPORT_CH1_CMD        (controller.busMasterIo + 8)
#define BMPORT_CH1_STATUS     (controller.busMasterIo + 10)
#define BMPORT_CH1_PRDADDR    (controller.busMasterIo + 12)

// List of supported DMA modes
static ideDmaMode dmaModes[] = {
  { "UDMA6", IDE_TRANSMODE_UDMA6, 88, 0x0040, 0x4000, IDE_FEATURE_UDMA },
  { "UDMA5", IDE_TRANSMODE_UDMA5, 88, 0x0020, 0x2000, IDE_FEATURE_UDMA },
  { "UDMA4", IDE_TRANSMODE_UDMA4, 88, 0x0010, 0x1000, IDE_FEATURE_UDMA },
  { "UDMA3", IDE_TRANSMODE_UDMA3, 88, 0x0008, 0x0800, IDE_FEATURE_UDMA },
  { "UDMA2", IDE_TRANSMODE_UDMA2, 88, 0x0004, 0x0400, IDE_FEATURE_UDMA },
  { "UDMA1", IDE_TRANSMODE_UDMA1, 88, 0x0002, 0x0200, IDE_FEATURE_UDMA },
  { "UDMA0", IDE_TRANSMODE_UDMA0, 88, 0x0001, 0x0100, IDE_FEATURE_UDMA },
  { "DMA2", IDE_TRANSMODE_DMA2, 63, 0x0004, 0x0040, IDE_FEATURE_MWDMA },
  { "DMA1", IDE_TRANSMODE_DMA1, 63, 0x0002, 0x0020, IDE_FEATURE_MWDMA },
  { "DMA0", IDE_TRANSMODE_DMA0, 63, 0x0001, 0x0010, IDE_FEATURE_MWDMA },
  { NULL, 0, 0, 0, 0, 0 }
};

// Read cache (lookahead) and write cache
static ideFeature features[] = {
  { "SMART", 82, 0x0001, 0, 0, 0, IDE_FEATURE_SMART },
  { "write caching", 82, 0x0020, 0x02, 85, 0x0020, IDE_FEATURE_WCACHE },
  { "read caching", 82, 0x0040, 0xAA, 85, 0x0040, IDE_FEATURE_RCACHE },
  { "48-bit addressing", 83, 0x0400, 0, 0, 0, IDE_FEATURE_48BIT },
  { NULL, 0, 0, 0, 0, 0, 0 }
};

// List of default IDE ports, per device number
static idePorts defaultPorts[] = {
  { 0x01F0, 0x01F1, 0x01F2, 0x01F3, 0x01F4, 0x01F5, 0x01F6, 0x01F7, 0x03F6 },
  { 0x0170, 0x0171, 0x0172, 0x0173, 0x0174, 0x0175, 0x0176, 0x0177, 0x0376 },
  { 0x00F0, 0x00F1, 0x00F2, 0x00F3, 0x00F4, 0x00F5, 0x00F6, 0x00F7, 0x02F6 },
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

static ideController controller;
static kernelPhysicalDisk disks[IDE_MAX_DISKS];


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
  data = (((driveNum & 0x01) << 4) | 0xA0);
  kernelProcessorOutPort8(CHANNEL(driveNum).ports.device, data);
  
  // Return success
  return (status = 0);
}

		
static void lbaSetup(int driveNum, unsigned lba, unsigned count)
{
  // This routine is strictly internal, and is used to set up the disk
  // controller registers with an LBA drive address in the drive/head, cylinder
  // low, cylinder high, and start sector registers.  It doesn't return
  // anything.
  
  unsigned char cmd = 0;

  // Set the LBA registers

  if (DISKIS48(driveNum))
    {
      // If count is 65536, we need to change it to zero.
      if (count == 65536)
	count = 0;

      // Send a value of 0 to the error/precomp register
      kernelProcessorOutPort8(CHANNEL(driveNum).ports.featErr, 0);

      // With 48-bit addressing, we write the top bytes to the same registers
      // as we will later write the bottom 3 bytes.

      // Send the high byte of the sector count
      cmd = ((count >> 8) & 0xFF);
      kernelProcessorOutPort8(CHANNEL(driveNum).ports.sectorCount, cmd);

      // Bits 24-31 of the address
      cmd = ((lba >> 24) & 0xFF);
      kernelProcessorOutPort8(CHANNEL(driveNum).ports.lbaLow, cmd);
      // Bits 32-39 of the address
      kernelProcessorOutPort8(CHANNEL(driveNum).ports.lbaMid, 0);
      // Bits 40-47 of the address
      kernelProcessorOutPort8(CHANNEL(driveNum).ports.lbaHigh, 0);
    }
  else 
    {
      // If count is 256, we need to change it to zero.
      if (count == 256)
	count = 0;
    }

  // Send a value of 0 to the error/precomp register
  kernelProcessorOutPort8(CHANNEL(driveNum).ports.featErr, 0);

  // Send the low byte of the sector count
  cmd = (count & 0xFF);
  kernelProcessorOutPort8(CHANNEL(driveNum).ports.sectorCount, cmd);

  // Bits 0-7 of the address
  cmd = (lba & 0xFF);
  kernelProcessorOutPort8(CHANNEL(driveNum).ports.lbaLow, cmd);
  // Bits 8-15 of the address
  cmd = ((lba >> 8) & 0xFF);
  kernelProcessorOutPort8(CHANNEL(driveNum).ports.lbaMid, cmd);
  // Bits 16-23 of the address
  cmd = ((lba >> 16) & 0xFF);
  kernelProcessorOutPort8(CHANNEL(driveNum).ports.lbaHigh, cmd);

  // LBA and device
  cmd = (0x40 | ((driveNum & 0x01) << 4));
  if (!DISKIS48(driveNum))
    // Bits 24-27 of the address
    cmd |= ((lba >> 24) & 0xF);
  kernelProcessorOutPort8(CHANNEL(driveNum).ports.device, cmd);
  
  return;
}


static int evaluateError(int driveNum)
{
  // This routine will check the error status on the disk controller
  // of the selected drive.  It evaluates the returned byte and matches 
  // conditions to error codes and error messages
  
  int errorCode = 0;
  unsigned char data = 0;
  
  kernelProcessorInPort8(CHANNEL(driveNum).ports.featErr, data);
  
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
  
  while (!CHANNEL(driveNum).gotInterrupt)
    {
      // Yield the rest of this timeslice if we are in multitasking mode
      //kernelMultitaskerYield();
      
      if (kernelSysTimerRead() > (startTime + 20))
	break;
    }
  
  // Check for disk controller errors.  Test the error bit in the status
  // register.
  kernelProcessorInPort8(CHANNEL(driveNum).ports.comStat, data);
  if (data & IDE_DRV_ERR)
    return (status = ERR_IO);

  else
    {
      if (CHANNEL(driveNum).gotInterrupt)
	{
	  CHANNEL(driveNum).gotInterrupt = 0;
	  return (status = 0);
	}
      else
	{
	  // No interrupt, no error -- just timed out.
	  kernelDebug(debug_io, "IDE: Disk %d no interrupt received - "
		      "timeout", driveNum);
	  return (status = ERR_IO);
	}
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
      kernelProcessorInPort8(CHANNEL(driveNum).ports.altComStat, data);

      if ((onOff && ((data & mask) == mask)) ||
	  (!onOff && ((data & mask) == 0)))
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

  // Wait for the controller to be ready, and data request not active
  status = pollStatus(driveNum, (IDE_CTRL_BSY | IDE_DRV_DRQ), 0);
  if (status < 0)
    return (status);
  
  kernelProcessorOutPort8(CHANNEL(driveNum).ports.featErr, 0);
  data = (unsigned char) (byteCount & 0x000000FF);
  kernelProcessorOutPort8(CHANNEL(driveNum).ports.lbaMid, data);
  data = (unsigned char) ((byteCount & 0x0000FF00) >> 8);
  kernelProcessorOutPort8(CHANNEL(driveNum).ports.lbaHigh, data);

  // Send the "ATAPI packet" command
  kernelProcessorOutPort8(CHANNEL(driveNum).ports.comStat, ATA_ATAPIPACKET);

  // Wait for the data request bit
  status = pollStatus(driveNum, IDE_DRV_DRQ, 1);
  if (status < 0)
    return (status);

  // Wait for the controller to be ready
  status = pollStatus(driveNum, IDE_CTRL_BSY, 0);
  if (status < 0)
    return (status);

  // Send the 12 bytes of packet data.
  kernelProcessorRepOutPort16(CHANNEL(driveNum).ports.data, packet, 6);

  return (status = 0);
}


static int atapiStartStop(int driveNum, int state)
{
  // Start or stop an ATAPI device

  int status = 0;
  unsigned short dataWord = 0;

  if (state)
    {
      // If we know the drive door is open, try to close it
      if (disks[driveNum].flags & DISKFLAG_DOOROPEN)
	sendAtapiPacket(driveNum, 0, ATAPI_PACKET_CLOSE);

      // Well, okay, assume this.
      disks[driveNum].flags &= ~DISKFLAG_DOOROPEN;

      status = sendAtapiPacket(driveNum, 0, ATAPI_PACKET_START);
      if (status < 0)
	return (status);

      status = sendAtapiPacket(driveNum, 8, ATAPI_PACKET_READCAPACITY);
      if (status < 0)
	return (status);

      // Read the number of sectors
      pollStatus(driveNum, IDE_DRV_DRQ, 1);
      kernelProcessorInPort16(CHANNEL(driveNum).ports.data, dataWord);
      disks[driveNum].numSectors = (((unsigned)(dataWord & 0x00FF)) << 24);
      disks[driveNum].numSectors |= (((unsigned)(dataWord & 0xFF00)) << 8);
      pollStatus(driveNum, IDE_DRV_DRQ, 1);
      kernelProcessorInPort16(CHANNEL(driveNum).ports.data, dataWord);
      disks[driveNum].numSectors |= (((unsigned)(dataWord & 0x00FF)) << 8);
      disks[driveNum].numSectors |= (((unsigned)(dataWord & 0xFF00)) >> 8);

      // Read the sector size
      pollStatus(driveNum, IDE_DRV_DRQ, 1);
      kernelProcessorInPort16(CHANNEL(driveNum).ports.data, dataWord);
      disks[driveNum].sectorSize = (((unsigned)(dataWord & 0x00FF)) << 24);
      disks[driveNum].sectorSize |= (((unsigned)(dataWord & 0xFF00)) << 8);
      pollStatus(driveNum, IDE_DRV_DRQ, 1);
      kernelProcessorInPort16(CHANNEL(driveNum).ports.data, dataWord);
      disks[driveNum].sectorSize |= (((unsigned)(dataWord & 0x00FF)) << 8);
      disks[driveNum].sectorSize |= (((unsigned)(dataWord & 0xFF00)) >> 8);

      // If there's no disk, the number of sectors will be illegal.  Set
      // to the maximum value and quit
      if ((disks[driveNum].numSectors == 0) ||
	  (disks[driveNum].numSectors == 0xFFFFFFFF))
	{
	  disks[driveNum].numSectors = 0xFFFFFFFF;
	  disks[driveNum].sectorSize = 2048;
	  kernelError(kernel_error, "No media in drive %s",
		      disks[driveNum].name);
	  return (status = ERR_NOMEDIA);
	}

      disks[driveNum].logical[0].numSectors = disks[driveNum].numSectors;
      
      // Read the TOC (Table Of Contents)
      status = sendAtapiPacket(driveNum, 12, ATAPI_PACKET_READTOC);
      if (status < 0)
	return (status);

      // Ignore the first four words
      pollStatus(driveNum, IDE_DRV_DRQ, 1);
      kernelProcessorInPort16(CHANNEL(driveNum).ports.data, dataWord);
      pollStatus(driveNum, IDE_DRV_DRQ, 1);
      kernelProcessorInPort16(CHANNEL(driveNum).ports.data, dataWord);
      pollStatus(driveNum, IDE_DRV_DRQ, 1);
      kernelProcessorInPort16(CHANNEL(driveNum).ports.data, dataWord);
      pollStatus(driveNum, IDE_DRV_DRQ, 1);
      kernelProcessorInPort16(CHANNEL(driveNum).ports.data, dataWord);

      // Read the LBA address of the start of the last track
      pollStatus(driveNum, IDE_DRV_DRQ, 1);
      kernelProcessorInPort16(CHANNEL(driveNum).ports.data, dataWord);
      disks[driveNum].lastSession = (((unsigned)(dataWord & 0x00FF)) << 24);
      disks[driveNum].lastSession |= (((unsigned)(dataWord & 0xFF00)) << 8);
      pollStatus(driveNum, IDE_DRV_DRQ, 1);
      kernelProcessorInPort16(CHANNEL(driveNum).ports.data, dataWord);
      disks[driveNum].lastSession |= (((unsigned)(dataWord & 0x00FF)) << 8);
      disks[driveNum].lastSession |= (((unsigned)(dataWord & 0xFF00)) >> 8);
      disks[driveNum].flags |= DISKFLAG_MOTORON;
    }
  else
    {
      status = sendAtapiPacket(driveNum, 0, ATAPI_PACKET_STOP);
      disks[driveNum].flags &= ~DISKFLAG_MOTORON;
    }

  return (status);
}


static int setMultiMode(int driveNum)
{
  // Set multiple mode

  int status = 0;

  // Wait for the controller to be ready
  status = pollStatus(driveNum, IDE_CTRL_BSY, 0);
  if (status < 0)
    {
      kernelError(kernel_error, errorMessages[IDE_TIMEOUT]);
      return (status);
    }

  // Clear the "interrupt received" byte
  CHANNEL(driveNum).gotInterrupt = 0;
  
  // Send the "set multiple mode" command
  kernelProcessorOutPort8(CHANNEL(driveNum).ports.sectorCount,
			  (unsigned char) disks[driveNum].multiSectors);
  kernelProcessorOutPort8(CHANNEL(driveNum).ports.comStat, ATA_SETMULTIMODE);
  
  // Wait for the controller to finish the operation
  status = waitOperationComplete(driveNum);
  return (status);
}


static void dmaSetCommand(int driveNum, unsigned char data, int set)
{
  // Set or clear bits in the DMA command register for the appropriate channel

  unsigned char cmd = 0;
  
  // Get the command reg
  if (!(driveNum / 2))
    kernelProcessorInPort8(BMPORT_CH0_CMD, cmd);
  else
    kernelProcessorInPort8(BMPORT_CH1_CMD, cmd);

  if (set)
    cmd |= data;
  else
    cmd &= ~data;

  // Write the command reg.
  if (!(driveNum / 2))
    kernelProcessorOutPort8(BMPORT_CH0_CMD, cmd);
  else
    kernelProcessorOutPort8(BMPORT_CH1_CMD, cmd);

  return;
}


static inline void dmaStartStop(int driveNum, int start)
{
  // Start or stop DMA for the appropriate channel
  dmaSetCommand(driveNum, 1, start);
  return;
}


static inline void dmaReadWrite(int driveNum, int read)
{
  // Set the DMA read/write bit for the appropriate channel
  dmaSetCommand(driveNum, 8, read);
  return;
}


static unsigned char dmaGetStatus(int driveNum)
{
  // Gets and clears the DMA status register

  unsigned char stat = 0;

  // Get the status, and write it back to clear it.
  if (!(driveNum / 2))
    {
      kernelProcessorInPort8(BMPORT_CH0_STATUS, stat);
      kernelProcessorOutPort8(BMPORT_CH0_STATUS, stat);
    }
  else
    {
      kernelProcessorInPort8(BMPORT_CH1_STATUS, stat);
      kernelProcessorOutPort8(BMPORT_CH1_STATUS, stat);
    }

  return (stat);
}


static int dmaSetup(int driveNum, void *address, unsigned bytes, int read,
		    unsigned *doneBytes)
{
  // Do DMA transfer setup.

  int status = 0;
  unsigned maxBytes = 0;
  void *physicalAddress = NULL;
  unsigned doBytes = 0;
  int numPrds = 0;
  int count;

  // Clear DMA status
  dmaGetStatus(driveNum);

  // Stop DMA
  dmaStartStop(driveNum, 0);

  // Set DMA read/write bit
  dmaReadWrite(driveNum, read);

  // How many bytes can we do per DMA operation?
  maxBytes = (disks[driveNum].multiSectors * 512);
  if (maxBytes > 0x10000)
    maxBytes = 0x10000;

  // Get the buffer physical address
  physicalAddress =
    kernelPageGetPhysical((((unsigned) address < KERNEL_VIRTUAL_ADDRESS)?
			   kernelCurrentProcess->processId : KERNELPROCID),
			  address);
  if (physicalAddress == NULL)
    {
      kernelError(kernel_error, "Couldn't get buffer physical address for %p",
		  address);
      return (status = ERR_INVALID);
    }
  // Address must be dword-aligned
  if ((unsigned) physicalAddress % 4)
    {
      kernelError(kernel_error, "Physical address %p not dword-aligned",
		  physicalAddress);
      return (status = ERR_ALIGN);
    }

  kernelDebug(debug_io, "IDE: Disk %d do DMA setup for %u bytes to address %p",
	      driveNum, bytes, physicalAddress);

  // Set up all the PRDs

  for (count = 0; bytes > 0; count ++)
    {
      if (numPrds >= CHANNEL(driveNum).prdEntries)
	// We've reached the limit of what we can do in one DMA setup
	break;

      doBytes = min(bytes, maxBytes);

      // No individual transfer (as represented by 1 PRD) should cross a
      // 64K boundary -- some DMA chips won't do that.
      if ((((unsigned) physicalAddress & 0xFFFF) + doBytes) > 0x10000)
        {
	  kernelDebug(debug_io, "IDE: Physical buffer crosses a 64K boundary");
	  doBytes = (0x10000 - ((unsigned) physicalAddress & 0xFFFF));
        }

      // If the number of bytes is exactly 64K, break it up into 2 transfers
      // in case the controller gets confused by a count of zero.
      if (doBytes == 0x10000)
	doBytes = 0x8000;

      // Each byte count must be dword-multiple
      if (doBytes % 4)
	{
	  kernelError(kernel_error, "Byte count not dword-multiple");
	  return (status = ERR_ALIGN);
	}

      // Set up the address and count in the channel's PRD
      CHANNEL(driveNum).prd[count].physicalAddress = physicalAddress;
      CHANNEL(driveNum).prd[count].count = doBytes;
      CHANNEL(driveNum).prd[count].EOT = 0;

      kernelDebug(debug_io, "IDE: Disk %d set up PRD for address %p, bytes %u",
		  driveNum, CHANNEL(driveNum).prd[count].physicalAddress,
		  doBytes);
		  
      physicalAddress += doBytes;
      bytes -= doBytes;
      *doneBytes += doBytes;
      numPrds += 1;
    }

  // Mark the last entry in the PRD table.
  CHANNEL(driveNum).prd[numPrds - 1].EOT = 0x8000;

  // Set the PRD table address
  if (!(driveNum / 2))
    kernelProcessorOutPort32(BMPORT_CH0_PRDADDR,
			     CHANNEL(driveNum).prdPhysical);
  else
    kernelProcessorOutPort32(BMPORT_CH1_PRDADDR,
			     CHANNEL(driveNum).prdPhysical);

  return (status = 0);
}


static int dmaCheckStatus(int driveNum)
{
  // Get the DMA status (for example after an operation) and clear it.

  int status = 0;
  unsigned char stat = 0;

  stat = dmaGetStatus(driveNum);

  if (stat & 0x01)
    {
      kernelError(kernel_error, "DMA transfer is still active");
      return (status = ERR_BUSY);
    }
  if (stat & 0x02)
    {
      kernelError(kernel_error, "DMA error");
      return (status = ERR_IO);
    }
  if (!(stat & 0x04))
    {
      kernelError(kernel_error, "No DMA interrupt");
      return (status = ERR_TIMEOUT);
    }

  return (status = 0);
}


static int readWriteSectors(int driveNum, unsigned logicalSector,
			    unsigned numSectors, void *buffer, int read)
{
  // This routine reads or writes sectors to/from the drive.  Returns 0 on
  // success, negative otherwise.
  
  int status = 0;
  unsigned doBytes = 0;
  unsigned doSectors = 0;
  unsigned multi = 0;
  unsigned reps = 0;
  unsigned char command = 0;
  unsigned char data8 = 0;
  int dmaStatus = 0;
  unsigned count;

  if (!disks[driveNum].name[0])
    {
      kernelError(kernel_error, "No such drive %d", driveNum);
      return (status = ERR_NOSUCHENTRY);
    }

  // Make sure we don't try to read/write an address we can't access
  if (!DISKIS48(driveNum) && ((logicalSector + numSectors - 1) > 0x0FFFFFFF))
    {
      kernelError(kernel_error, "Can't access sectors %u->%u on disk %d with "
		  "28-bit addressing", logicalSector,
		  (logicalSector + numSectors - 1), driveNum);
      return (status = ERR_BOUNDS);
    }

  // Wait for a lock on the controller
  status = kernelLockGet(&(CHANNEL(driveNum).lock));
  if (status < 0)
    return (status);
  
  // Select the drive
  selectDrive(driveNum);

  // If it's an ATAPI device
  if (disks[driveNum].type & DISKTYPE_IDECDROM)
    {
      // If it's not started, we start it
      if (!(disks[driveNum].flags & DISKFLAG_MOTORON))
	{
	  status = atapiStartStop(driveNum, 1);
	  if (status < 0)
	    {
	      kernelLockRelease(&(CHANNEL(driveNum).lock));
	      return (status);
	    }
	}

      doBytes = (numSectors * disks[driveNum].sectorSize);

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
	  kernelLockRelease(&(CHANNEL(driveNum).lock));
	  return (status);
	}

      while (doBytes)
	{
	  // Wait for the controller to assert data request
	  while (pollStatus(driveNum, IDE_DRV_DRQ, 1))
	    {
	      // Timed out.  Check for an error...
	      kernelProcessorInPort8(CHANNEL(driveNum).ports.altComStat,
				     data8);
	      if (data8 & IDE_DRV_ERR)
		{
		  kernelError(kernel_error,
			      errorMessages[evaluateError(driveNum)]);
		  kernelLockRelease(&(CHANNEL(driveNum).lock));
		  return (status = ERR_NODATA);
		}
	    }

	  // How many words to read?
	  unsigned bytes = 0;
	  kernelProcessorInPort8(CHANNEL(driveNum).ports.lbaMid, data8);
	  bytes = data8;
	  kernelProcessorInPort8(CHANNEL(driveNum).ports.lbaHigh, data8);
	  bytes |= (data8 << 8);

	  unsigned words = (bytes >> 1);

	  // Transfer the number of words from the drive.
	  kernelProcessorRepInPort16(CHANNEL(driveNum).ports.data, buffer,
				     words);

	  buffer += (words << 1);
	  doBytes -= (words << 1);

	  // Just in case it's an odd number
	  if (bytes % 2)
	    {
	      kernelProcessorInPort8(CHANNEL(driveNum).ports.data, data8);
	      ((unsigned char *) buffer)[0] = data8;
	      buffer += 1;
	      doBytes -= 1;
	    }
	}

      kernelLockRelease(&(CHANNEL(driveNum).lock));
      return (status = 0);
    }

  if (!DISKISDMA(driveNum) && DISKISMULTI(driveNum))
    {
      status = setMultiMode(driveNum);
      if (status < 0)
	{
	  while ((status < 0) && (disks[driveNum].multiSectors > 1))
	    {
	      kernelLog("IDE: Reduce multi-sectors for disk %s to %d",
			disks[driveNum].name,
			(disks[driveNum].multiSectors / 2));
	      disks[driveNum].multiSectors /= 2;
	      status = setMultiMode(driveNum);
	    }
	  if ((status < 0) || (disks[driveNum].multiSectors <= 1))
	    {
	      // No more multi-transfers for you
	      kernelError(kernel_error, "Error setting multi-sector mode for "
			  "disk %s.  Disabled.", disks[driveNum].name);
	      DISK(driveNum).featureFlags &= ~IDE_FEATURE_MULTI;
	    }
	}
    }

  multi = disks[driveNum].multiSectors;

  // Figure out which command we're going to be sending to the controller
  if (DISKISDMA(driveNum))
    {
      if (DISKIS48(driveNum))
	{
	  if (read)
	    command = ATA_READDMA_EXT;
	  else
	    command = ATA_WRITEDMA_EXT;
	}
      else
	{
	  if (read)
	    command = ATA_READDMA;
	  else
	    command = ATA_WRITEDMA;
	}
    }
  else if (DISKISMULTI(driveNum))
    {
      if (DISKIS48(driveNum))
	{
	  if (read)
	    command = ATA_READMULTI_EXT;
	  else
	    command = ATA_WRITEMULTI_EXT;
	}
      else
	{
	  if (read)
	    command = ATA_READMULTI;
	  else
	    command = ATA_WRITEMULTI;
	}
    }
  else
    {
      if (DISKIS48(driveNum))
	{
	  if (read)
	    command = ATA_READSECTS_EXT;
	  else
	    command = ATA_WRITESECTS_EXT;
	}
      else
	{
	  if (read)
	    command = ATA_READSECTS;
	  else
	    command = ATA_WRITESECTS;
	}
    }

  // This outer loop is done once for each *command* we send.  Actual
  // data transfers, DMA transfers, etc. may occur more than once per command
  // and are handled by the inner loop.  The number of times we send a command
  // depends upon the maximum number of sectors we can specify per command.

  while (numSectors > 0)
    {
      // Figure out the number of sectors per command
      doSectors = numSectors;
      if (DISKIS48(driveNum))
	{
	  if (doSectors > 65536)
	    doSectors = 65536;
	}
      else if (doSectors > 256)
	doSectors = 256;

      // Wait for the controller to be ready
      status = pollStatus(driveNum, IDE_CTRL_BSY, 0);
      if (status < 0)
	{
	  kernelError(kernel_error, errorMessages[IDE_TIMEOUT]);
	  kernelLockRelease(&(CHANNEL(driveNum).lock));
	  return (status);
	}

      if (DISKISDMA(driveNum))
	{
	  // Set up the DMA transfer
	  unsigned dmaBytes = 0;
	  status =
	    dmaSetup(driveNum, buffer, (doSectors * 512), read, &dmaBytes);
	  if (status < 0)
	    {
	      kernelLockRelease(&(CHANNEL(driveNum).lock));
	      return (status);
	    }
	  if (dmaBytes < (doSectors * 512))
	    doSectors = (dmaBytes / 512);
	}

      // We always use LBA.  Break up the sector count and LBA value and
      // deposit them into the appropriate controller registers.
      lbaSetup(driveNum, logicalSector, doSectors);

      // Wait for the selected drive to be ready
      status = pollStatus(driveNum, IDE_DRV_RDY, 1);
      if (status < 0)
	{
	  kernelError(kernel_error, errorMessages[IDE_TIMEOUT]);
	  kernelLockRelease(&(CHANNEL(driveNum).lock));
	  return (status);
	}

      // Clear the "interrupt received" byte
      CHANNEL(driveNum).gotInterrupt = 0;

      // Issue the command
      kernelDebug(debug_io, "IDE: Sending command");
      kernelProcessorOutPort8(CHANNEL(driveNum).ports.comStat, command);

      // Figure out how many data cycles (reps) we need for this command.
      if (DISKISDMA(driveNum))
	{
	  // DMA.  Only one rep.  Start DMA.
	  dmaStartStop(driveNum, 1);
	  reps = 1;
	}
      else
	reps = ((doSectors / multi) + ((doSectors % multi)? 1 : 0));

      // The inner loop that follows works differently depending on whether
      // we're using DMA or not.  It is used to service each interrupt.
      // With DMA there will be only one interrupt at the end of the DMA
      // transfer, so the loop will execute once.  Without DMA the drive
      // will interrupt once for each sector, and we will read each word
      // from the port.

      for (count = 0; count < reps; count ++)
	{
	  unsigned doMulti = 0;

	  if (DISKISDMA(driveNum))
	    doMulti = doSectors;
	  else
	    {
	      doMulti = min(multi, doSectors);
	      if ((doSectors % multi) && (count == (reps - 1)))
		doMulti = (doSectors % multi);

	      if (!read)
		{
		  // Wait for DRQ
		  while (pollStatus(driveNum, IDE_DRV_DRQ, 1));
	      
		  kernelProcessorRepOutPort16(CHANNEL(driveNum).ports.data,
					      buffer, (doMulti * 256));
		}
	    }

	  // Wait for the controller to finish the operation
	  status = waitOperationComplete(driveNum);
	  if (status < 0)
	    break;

	  if (read && !DISKISDMA(driveNum))
	    kernelProcessorRepInPort16(CHANNEL(driveNum).ports.data,
				       buffer, (doMulti * 256));

	  buffer += (doMulti * 512);

	  kernelDebug(debug_io, "IDE: Transfer successful");
	}

      if (DISKISDMA(driveNum))
	{
	  dmaStatus = dmaCheckStatus(driveNum);
	  dmaStartStop(driveNum, 0);
	}

      if ((status < 0) || (dmaStatus < 0))
	{
	  kernelError(kernel_error, "Disk %s, %s %u at %u: %s",
		      disks[driveNum].name, (read? "read" : "write"),
		      numSectors, logicalSector,
		      ((dmaStatus < 0)? "DMA error" :
		       errorMessages[evaluateError(driveNum)]));
	  kernelLockRelease(&(CHANNEL(driveNum).lock));
	  return (status);
	}

      numSectors -= doSectors;
      logicalSector += doSectors;
    }
  
  // We are finished.  The data should be transferred.
  
  // Unlock the controller
  kernelLockRelease(&(CHANNEL(driveNum).lock));
  
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
  kernelProcessorOutPort8(CHANNEL(master).ports.altComStat, 0x04);
  
  // Delay 1/20th second
  startTime = kernelSysTimerRead();
  while (kernelSysTimerRead() < (startTime + 1));
  
  // Clear bit 2 again
  kernelProcessorOutPort8(CHANNEL(master).ports.altComStat, 0);

  // If either the slave or the master on this controller is an ATAPI device,
  // delay
  if ((disks[master].name[0] && (disks[master].type & DISKTYPE_IDECDROM)) ||
      (disks[slave].name[0] && (disks[slave].type & DISKTYPE_IDECDROM)))
    atapiDelay();

  // Wait for controller ready
  status = pollStatus(master, IDE_CTRL_BSY, 0);
  if (status < 0)
    {
      kernelError(kernel_error, "Controller not ready after reset");
      return (status);
    }

  // Read the error status
  kernelProcessorInPort8(CHANNEL(master).ports.altComStat, data);
  if (data & IDE_DRV_ERR)
    kernelError(kernel_error, errorMessages[evaluateError(master)]);

  // If there is a slave, make sure it is ready
  if (disks[slave].name[0])
    {
      // Select the slave
      selectDrive(slave);

      // Error, until the slave is ready
      status = -1;

      startTime = kernelSysTimerRead();
      unsigned char sectorCount = 0, sectorNumber = 0;
	  
      while (kernelSysTimerRead() < (startTime + 20))
	{
	  // Read the sector count and LBA low registers
	  kernelProcessorInPort8(CHANNEL(slave).ports.sectorCount,
				 sectorCount);
	  kernelProcessorInPort8(CHANNEL(slave).ports.lbaLow, sectorNumber);

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
      kernelProcessorInPort8(CHANNEL(slave).ports.altComStat, data);
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

  // Enable "revert to power on defaults"
  kernelProcessorOutPort8(CHANNEL(driveNum).ports.featErr, 0xCC);
  kernelProcessorOutPort8(CHANNEL(driveNum).ports.device,
			  ((driveNum & 1) << 4));
  kernelProcessorOutPort8(CHANNEL(driveNum).ports.comStat, ATA_SETFEATURES);
  
  // Wait for it...
  atapiDelay();

  // Wait for controller ready
  status = pollStatus(driveNum, IDE_CTRL_BSY, 0);
  if (status < 0)
    return (status);

  // Do ATAPI reset
  kernelProcessorOutPort8(CHANNEL(driveNum).ports.comStat, ATA_ATAPIRESET);
  
  // Wait for it...
  atapiDelay();

  // Wait for the reset to finish
  status = pollStatus(driveNum, IDE_CTRL_BSY, 0);
  if (status < 0)
    return (status);

  return (status = 0);
}


static int atapiSetLockState(int driveNum, int locked)
{
  // Lock or unlock an ATAPI device

  int status = 0;

  if (locked)
    {
      status = sendAtapiPacket(driveNum, 0, ATAPI_PACKET_LOCK);
      disks[driveNum].flags |= DISKFLAG_DOORLOCKED;
    }
  else
    {
      status = sendAtapiPacket(driveNum, 0, ATAPI_PACKET_UNLOCK);
      disks[driveNum].flags &= ~DISKFLAG_DOORLOCKED;
    }

  return (status);
}


static int atapiSetDoorState(int driveNum, int open)
{
  // Open or close the door of an ATAPI device

  int status = 0;

  if (open)
    {
      // If the disk is started, stop it
      if (disks[driveNum].flags & DISKFLAG_MOTORON)
	{
	  status = atapiStartStop(driveNum, 0);
	  if (status < 0)
	    {
	      kernelLockRelease(&(CHANNEL(driveNum).lock));
	      return (status);
	    }
	}

      status = sendAtapiPacket(driveNum, 0, ATAPI_PACKET_EJECT);
      disks[driveNum].flags |= DISKFLAG_DOOROPEN;
    }
  else
    {
      status = sendAtapiPacket(driveNum, 0, ATAPI_PACKET_CLOSE);
      disks[driveNum].flags &= ~DISKFLAG_DOOROPEN;
    }

  return (status);
}


static void primaryIdeInterrupt(void)
{
  // This is the IDE interrupt handler for the primary controller.  It will
  // be called whenever the disk controller issues its service interrupt,
  // and will simply change a data value to indicate that one has been
  // received.  It's up to the other routines to do something useful with
  // the information.

  void *address = NULL;

  kernelProcessorIsrEnter(address);

  kernelProcessingInterrupt = 1;
  controller.channel[0].gotInterrupt = 1;
  kernelPicEndOfInterrupt(INTERRUPT_NUM_PRIMARYIDE);
  kernelDebug(debug_io, "IDE: Primary interrupt");
  kernelProcessingInterrupt = 0;

  kernelProcessorIsrExit(address);
}


static void secondaryIdeInterrupt(void)
{
  // This is the IDE interrupt handler for the secondary controller.  It will
  // be called whenever the disk controller issues its service interrupt,
  // and will simply change a data value to indicate that one has been
  // received.  It's up to the other routines to do something useful with
  // the information.

  void *address = NULL;
  unsigned char data;

  kernelProcessorIsrEnter(address);

  // This interrupt can sometimes occur frivolously from "noise"
  // on the interrupt request lines.  Before we do anything at all,
  // we MUST ensure that the interrupt really occurred.
  kernelProcessorOutPort8(0xA0, 0x0B);
  kernelProcessorInPort8(0xA0, data);
  if (data & 0x80)
    {
      kernelProcessingInterrupt = 1;
      controller.channel[1].gotInterrupt = 1;
      kernelPicEndOfInterrupt(INTERRUPT_NUM_SECONDARYIDE);
      kernelDebug(debug_io, "IDE: Secondary interrupt");
      kernelProcessingInterrupt = 0;
    }
  
  kernelProcessorIsrExit(address);
}


static int identify(int driveNum, unsigned short *buffer)
{
  // Issues the ATA "identify device" command.  If that fails, it tries the
  // ATAPI "identify packet device" command.

  int status = 0;
  unsigned char sigLow = 0;
  unsigned char sigHigh = 0;

  kernelMemClear(buffer, 512);

  // Wait for controller ready
  status = pollStatus(driveNum, IDE_CTRL_BSY, 0);
  if (status < 0)
    return (status);

  // Clear the "interrupt received" byte
  CHANNEL(driveNum).gotInterrupt = 0;
  
  // Send the "identify device" command
  kernelProcessorOutPort8(CHANNEL(driveNum).ports.comStat, ATA_IDENTIFY);
  
  // Wait for the controller to finish the operation
  status = waitOperationComplete(driveNum);

  if (status >= 0)
    // Transfer one sector's worth of data from the controller.
    kernelProcessorRepInPort16(CHANNEL(driveNum).ports.data, buffer, 256);

  else
    {
      // Possibly ATAPI?
	  
      // Read the cylinder low + high registers
      kernelProcessorInPort8(CHANNEL(driveNum).ports.lbaMid, sigLow);
      kernelProcessorInPort8(CHANNEL(driveNum).ports.lbaHigh, sigHigh);

      // Check for the ATAPI signature
      if ((sigLow != 0x14) || (sigHigh != 0xEB))
	// We don't know what this is
	return (status = ERR_NOTIMPLEMENTED);

      // Send the "identify packet device" command
      kernelProcessorOutPort8(CHANNEL(driveNum).ports.comStat,
			      ATA_ATAPIIDENTIFY);

      // Wait for BSY=0
      status = pollStatus(driveNum, IDE_CTRL_BSY, 0);
      if (status < 0)
	return (status);

      // Check for the signature again
      if ((sigLow != 0x14) || (sigHigh != 0xEB))
	// We don't know what this is
	return (status = ERR_NOTIMPLEMENTED);

      // Transfer one sector's worth of data from the controller.
      kernelProcessorRepInPort16(CHANNEL(driveNum).ports.data, buffer, 256);
    }
 
  return (status = 0);
}


static int setTransferMode(int driveNum, ideDmaMode *mode,
			   unsigned short *buffer)
{
  // Try to set the transfer mode (e.g. DMA, UDMA).

  int status = 0;

  // Wait for controller ready
  status = pollStatus(driveNum, IDE_CTRL_BSY, 0);
  if (status < 0)
    return (status);

  // Clear the "interrupt received" byte
  CHANNEL(driveNum).gotInterrupt = 0;
  
  // Send the "set features" command, subcommand "set transfer mode".
  kernelProcessorOutPort8(CHANNEL(driveNum).ports.featErr,
			  0x03 /* Set transfer mode */);
  kernelProcessorOutPort8(CHANNEL(driveNum).ports.sectorCount, mode->val);
  kernelProcessorOutPort8(CHANNEL(driveNum).ports.device,
			  ((driveNum & 1) << 4));
  kernelProcessorOutPort8(CHANNEL(driveNum).ports.comStat, ATA_SETFEATURES);

  // Wait for the controller to finish the operation
  status = waitOperationComplete(driveNum);
  if (status < 0)
    {
      kernelError(kernel_error, "Couldn't set features for transfer mode");
      return (status);
    }

  // Now we do an "identify device" to find out if we were successful
  status = identify(driveNum, buffer);
  if (status < 0)
    // Couldn't verify.
    return (status);

  // Verify that the requested mode has been set
  if (buffer[mode->identByte] & mode->enabledMask)
    {
      kernelDebug(debug_io, "IDE: Disk %d successfully set transfer mode %s",
		  driveNum, mode->name);
      return (status = 0);
    }
  else
    {
      kernelError(kernel_error, "Failed to set transfer mode %s for disk "
		  "%d", mode->name, driveNum);
      return (status = ERR_INVALID);
    }
}


static int enableFeature(int driveNum, ideFeature *feature,
			 unsigned short *buffer)
{
  // Try to enable a general feature.

  int status = 0;

  // Wait for controller ready
  status = pollStatus(driveNum, IDE_CTRL_BSY, 0);
  if (status < 0)
    return (status);

  // Clear the "interrupt received" byte
  CHANNEL(driveNum).gotInterrupt = 0;
  
  // Send the "set features" command
  kernelProcessorOutPort8(CHANNEL(driveNum).ports.featErr,
			  feature->featureCode);
  kernelProcessorOutPort8(CHANNEL(driveNum).ports.device,
			  ((driveNum & 1) << 4));
  kernelProcessorOutPort8(CHANNEL(driveNum).ports.comStat, ATA_SETFEATURES);

  // Wait for the controller to finish the operation
  status = waitOperationComplete(driveNum);
  if (status < 0)
    {
      kernelError(kernel_error, "Couldn't set feature %s", feature->name);
      return (status);
    }

  // Can we verify that we were successful?
  if (feature->enabledByte)
    {
      // Now we do an "identify device" to find out if we were successful
      status = identify(driveNum, buffer);
      if (status < 0)
	// Couldn't verify.
	return (status);

      // Verify that the requested mode has been set
      if (buffer[feature->enabledByte] & feature->enabledMask)
	{
	  kernelDebug(debug_io, "IDE: Disk %d successfully set feature %s",
		      driveNum, feature->name);
	  return (status = 0);
	}
      else
	{
	  kernelError(kernel_error, "Failed to set feature %s for disk %d",
		      feature->name, driveNum);
	  return (status = ERR_INVALID);
	}
    }

  return (status = 0);
}


static kernelDevice *driverDetectPci(void)
{
  // Try to detect a PCI-capable controller

  int status = 0;
  kernelDevice *controllerDevice = NULL;
  kernelDevice *busDevice = NULL;
  kernelBusTarget *pciTargets = NULL;
  int numPciTargets = 0;
  int deviceCount = 0;
  pciDeviceInfo pciDevInfo;
  int count;

  // See if there are any IDE controllers on the PCI bus.  This obviously
  // depends upon PCI hardware detection occurring before IDE detection.

  // Get the PCI bus device
  status = kernelDeviceFindType(kernelDeviceGetClass(DEVICECLASS_BUS),
				kernelDeviceGetClass(DEVICESUBCLASS_BUS_PCI),
				&busDevice, 1);
  if (status <= 0)
    return (controllerDevice = NULL);

  // Search the PCI bus(es) for devices
  numPciTargets = kernelBusGetTargets(bus_pci, &pciTargets);
  if (numPciTargets <= 0)
    return (controllerDevice = NULL);

  // Search the PCI bus targets for IDE controllers
  for (deviceCount = 0; deviceCount < numPciTargets; deviceCount ++)
    {
      // If it's not an IDE controller, skip it
      if ((pciTargets[deviceCount].class == NULL) ||
	  (pciTargets[deviceCount].class->class != DEVICECLASS_DISK) ||
	  (pciTargets[deviceCount].subClass == NULL) ||
	  (pciTargets[deviceCount].subClass->class != DEVICESUBCLASS_DISK_IDE))
	continue;

      kernelDebug(debug_io, "PCI IDE: Found");

      // Get the PCI device header
      status = kernelBusGetTargetInfo(bus_pci, pciTargets[deviceCount].target,
				      &pciDevInfo);
      if (status < 0)
	continue;

      // Make sure it's a non-bridge header
      if (pciDevInfo.device.headerType != PCI_HEADERTYPE_NORMAL)
	{
	  kernelDebug(debug_io, "PCI IDE: Headertype not 'normal' (%d)",
		      pciDevInfo.device.headerType);
	  continue;
	}

      // Enable the device and set bus-master mode.

      kernelBusDeviceEnable(bus_pci, pciTargets[deviceCount].target, 1);
      kernelBusSetMaster(bus_pci, pciTargets[deviceCount].target, 1);

      if (!kernelBusReadRegister(bus_pci, pciTargets[deviceCount].target,
				 PCI_CONFREG_COMMAND_16, 16) & 0x05)
	{
	  kernelDebug(debug_io, "PCI IDE: Couldn't enable bus mastering");
	  continue;
	}
      kernelDebug(debug_io, "PCI IDE: Bus mastering enabled in PCI");

      // We found a bus mastering controller.
      break;
    }

  // Get the PCI IDE controller IO address
  controller.busMasterIo =
    (pciDevInfo.device.nonBridge.baseAddress[4] & 0xFFFFFFFE);
  if (controller.busMasterIo == NULL)
    {
      kernelError(kernel_error, "Unknown controller I/O address");
      goto out;
    }
  kernelDebug(debug_io, "PCI IDE: Bus master I/O address=0x%08x",
	      controller.busMasterIo);

  // Get memory for physical region descriptors and transfer areas

  for (count = 0; count < 2; count ++)
    {
      controller.channel[count].prdEntries = 512;

      controller.channel[count].prdPhysical =
	kernelMemoryGetPhysical(controller.channel[count].prdEntries *
				sizeof(idePrd), DISK_CACHE_ALIGN,
				"ide prd entries");
      if (controller.channel[count].prdPhysical == NULL)
	goto out;

      status = kernelPageMapToFree(KERNELPROCID,
				   controller.channel[count].prdPhysical,
				   (void **) &(controller.channel[count].prd),
				   (controller.channel[count].prdEntries *
				    sizeof(idePrd)));
      if (status < 0)
	goto out;

      if (((unsigned) controller.channel[count].prd % 4) ||
	  ((unsigned) controller.channel[count].prdPhysical % 4))
	kernelError(kernel_warn, "PRD or PRD physical not dword-aligned");
    }

  // Enable bus mastering for both channels
  unsigned char tmpByte = 0;
  kernelProcessorInPort8(BMPORT_CH0_CMD, tmpByte);
  kernelProcessorOutPort8(BMPORT_CH0_CMD, (tmpByte | 1));
  kernelProcessorInPort8(BMPORT_CH0_CMD, tmpByte);
  if (!(tmpByte & 1))
    {
      kernelDebug(debug_io, "PCI IDE: Couldn't enable bus mastering, "
		  "CH0_CMD=0x%02x", tmpByte);
      goto out;
    }
  kernelProcessorInPort8(BMPORT_CH1_CMD, tmpByte);
  kernelProcessorOutPort8(BMPORT_CH1_CMD, (tmpByte | 1));
  kernelProcessorInPort8(BMPORT_CH1_CMD, tmpByte);
  if (!(tmpByte & 1))
    {
      kernelDebug(debug_io, "PCI IDE: Couldn't enable bus mastering, "
		  "CH1_CMD=0x%02x", tmpByte);
      goto out;
    }
  kernelDebug(debug_io, "PCI IDE: Bus mastering enabled in controller");

  /*
    I don't think we really need to do this, as we don't have any particular
    reason to remap the I/O addresses and/or interrupt numbers.

  // Check whether the device claims to be able to operate in 100% native
  // mode.  For the moment we will only try to operate in full native-PCI
  // mode if both channels can do it.
  if (((pciDevInfo.device.progIF & 0x05) == 0x05) ||
      ((pciDevInfo.device.progIF & 0x0A) == 0x0A))
    {
      pciDevInfo.device.progIF |= 0x05;
      kernelBusWriteRegister(bus_pci, pciTargets[deviceCount].target,
			     PCI_CONFREG_PROGIF_8, 8,
			     pciDevInfo.device.progIF);

      // Both channels should now be in PCI mode
      pciDevInfo.device.progIF =
	kernelBusReadRegister(bus_pci, pciTargets[deviceCount].target,
			      PCI_CONFREG_PROGIF_8, 8);
      if ((pciDevInfo.device.progIF & 0x05) == 0x05)
	kernelDebug(debug_io, "PCI IDE: Successfully switched to native-PCI "
		    "mode");
      else
	kernelDebug(debug_io, "PCI IDE: Can't switch to native-PCI mode "
		    "(progIF=%02x)", pciDevInfo.device.progIF);
    }
  else
    // Both channels are not native-PCI
    kernelLog("IDE: PCI controller in compatibility mode only");
  */

  // Success.
  controller.busMaster = 1;
  kernelLog("IDE: Bus mastering PCI controller enabled");

  // Create a device for it in the kernel.

  // Allocate memory for the device.
  controllerDevice = kernelMalloc(sizeof(kernelDevice));
  if (controllerDevice == NULL)
    goto out;

  controllerDevice->device.class =
    kernelDeviceGetClass(DEVICECLASS_DISKCTRL);
  controllerDevice->device.subClass =
    kernelDeviceGetClass(DEVICESUBCLASS_DISKCTRL_IDE);

  // Register the controller
  status = kernelDeviceAdd(busDevice, controllerDevice);
  if (status < 0)
    {
      kernelFree(controllerDevice);
      goto out;
    }

 out:
  kernelFree(pciTargets);
  return (controllerDevice);
}


static void testDma(int driveNum)
{
  // This is called once for each hard disk for which we've initially selected
  // DMA operation.  It does a simple single-sector read.  If that succeeds
  // then we continue to use DMA.  Otherwise, we clear the DMA feature flags.

  int status = 0;
  unsigned char buffer[512];
  
  status = readWriteSectors(driveNum, 0, 1, buffer, 1);
  if (status < 0)
    {
      kernelLog("IDE: Disk %d DMA support disabled", driveNum);
      DISK(driveNum).featureFlags &= ~IDE_FEATURE_DMA;
    }
}


static int driverReset(int driveNum)
{
  // Does a software reset of the requested disk controller.
  
  int status = 0;
  
  if (!disks[driveNum].name[0])
    {
      kernelError(kernel_error, "No such drive %d", driveNum);
      return (status = ERR_NOSUCHENTRY);
    }

  // Wait for a lock on the controller
  status = kernelLockGet(&(CHANNEL(driveNum).lock));
  if (status < 0)
    return (status);

  // Select the drive
  selectDrive(driveNum);

  status = reset(driveNum);
  
  // Unlock the controller
  kernelLockRelease(&(CHANNEL(driveNum).lock));
  
  return (status);
}


static int driverRecalibrate(int driveNum)
{
  // Recalibrates the requested drive, causing it to seek to cylinder 0
  
  int status = 0;
  
  if (!disks[driveNum].name[0])
    {
      kernelError(kernel_error, "No such drive %d", driveNum);
      return (status = ERR_NOSUCHENTRY);
    }

  // Don't try to recalibrate ATAPI 
  if (disks[driveNum].type & DISKTYPE_IDECDROM)
    return (status = 0);

  // Wait for a lock on the controller
  status = kernelLockGet(&(CHANNEL(driveNum).lock));
  if (status < 0)
    return (status);
  
  // Select the drive
  selectDrive(driveNum);
  
  // Wait for the controller to be ready
  status = pollStatus(driveNum, IDE_CTRL_BSY, 0);
  if (status < 0)
    {
      kernelError(kernel_error, errorMessages[IDE_TIMEOUT]);
      kernelLockRelease(&(CHANNEL(driveNum).lock));
      return (status);
    }
  
  // Wait for the selected drive to be ready
  status = pollStatus(driveNum, IDE_DRV_RDY, 1);
  if (status < 0)
    {
      kernelError(kernel_error, errorMessages[IDE_TIMEOUT]);
      kernelLockRelease(&(CHANNEL(driveNum).lock));
      return (status);
    }
  
  // Clear the "interrupt received" byte
  CHANNEL(driveNum).gotInterrupt = 0;
  
  // Send the recalibrate command
  kernelProcessorOutPort8(CHANNEL(driveNum).ports.comStat, ATA_RECALIBRATE);
  
  // Wait for the recalibration to complete
  status = waitOperationComplete(driveNum);
  
  // Unlock the controller
  kernelLockRelease(&(CHANNEL(driveNum).lock));
  
  if (status < 0)
    kernelError(kernel_error, errorMessages[evaluateError(driveNum)]);
  
  return (status);
}


static int driverSetLockState(int driveNum, int lockState)
{
  // This will lock or unlock the CD-ROM door

  int status = 0;

  if (!disks[driveNum].name[0])
    {
      kernelError(kernel_error, "No such drive %d", driveNum);
      return (status = ERR_NOSUCHENTRY);
    }

  if (lockState && (disks[driveNum].flags & DISKFLAG_DOOROPEN))
    {
      // Don't to lock the door if it is open
      kernelError(kernel_error, "Drive door is open");
      return (status = ERR_PERMISSION);
    }

  // Wait for a lock on the controller
  status = kernelLockGet(&(CHANNEL(driveNum).lock));
  if (status < 0)
    return (status);
  
  // Select the drive
  selectDrive(driveNum);

  status = atapiSetLockState(driveNum, lockState);

  // Unlock the controller
  kernelLockRelease(&(CHANNEL(driveNum).lock));

  return (status);
}


static int driverSetDoorState(int driveNum, int open)
{
  // This will open or close the CD-ROM door

  int status = 0;

  if (!disks[driveNum].name[0])
    {
      kernelError(kernel_error, "No such drive %d", driveNum);
      return (status = ERR_NOSUCHENTRY);
    }

  if (open && (disks[driveNum].flags & DISKFLAG_DOORLOCKED))
    {
      // Don't try to open the door if it is locked
      kernelError(kernel_error, "Drive door is locked");
      return (status = ERR_PERMISSION);
    }

  // Wait for a lock on the controller
  status = kernelLockGet(&(CHANNEL(driveNum).lock));
  if (status < 0)
    return (status);
  
  // Select the drive
  selectDrive(driveNum);

  status = atapiSetDoorState(driveNum, open);

  // Unlock the controller
  kernelLockRelease(&(CHANNEL(driveNum).lock));
  
  return (status);
}


static int driverReadSectors(int driveNum, unsigned logicalSector,
			     unsigned numSectors, void *buffer)
{
  // This routine is a wrapper for the readWriteSectors routine.
  return (readWriteSectors(driveNum, logicalSector, numSectors, buffer,
			   1));  // Read operation
}


static int driverWriteSectors(int driveNum, unsigned logicalSector,
			      unsigned numSectors, const void *buffer)
{
  // This routine is a wrapper for the readWriteSectors routine.
  return (readWriteSectors(driveNum, logicalSector, numSectors,
			   (void *) buffer, 0));  // Write operation
}


static int driverFlush(int driveNum)
{
  // If write caching is enabled for this disk, flush the cache

  int status = 0;
  unsigned char command = 0;

  if (!disks[driveNum].name[0])
    {
      kernelError(kernel_error, "No such drive %d", driveNum);
      return (status = ERR_NOSUCHENTRY);
    }

  // If write caching is not enabled, just return
  if (!DISKISWCACHE(driveNum))
    return (status = 0);

  // Wait for a lock on the controller
  status = kernelLockGet(&(CHANNEL(driveNum).lock));
  if (status < 0)
    return (status);
  
  // Select the drive
  selectDrive(driveNum);

  // Wait for the controller to be ready
  status = pollStatus(driveNum, IDE_CTRL_BSY, 0);
  if (status < 0)
    {
      kernelError(kernel_error, errorMessages[IDE_TIMEOUT]);
      goto out;
    }

  // Wait for the selected drive to be ready
  status = pollStatus(driveNum, IDE_DRV_RDY, 1);
  if (status < 0)
    {
      kernelError(kernel_error, errorMessages[IDE_TIMEOUT]);
      goto out;
    }

  // Figure out which command we're going to be sending to the controller
  if (DISKIS48(driveNum))
    command = ATA_FLUSHCACHE_EXT;
  else
    command = ATA_FLUSHCACHE;

  // Clear the "interrupt received" byte
  CHANNEL(driveNum).gotInterrupt = 0;

  // Issue the command
  kernelDebug(debug_io, "IDE: Sending command");
  kernelProcessorOutPort8(CHANNEL(driveNum).ports.comStat, command);

  // Wait for the controller to finish the operation
  status = waitOperationComplete(driveNum);
  if (status < 0)
    goto out;

  status = 0;

 out:
  // Unlock the controller
  kernelLockRelease(&(CHANNEL(driveNum).lock));
  
  return (status);
}


static int driverDetect(void *parent, kernelDriver *driver)
{
  // This routine is used to detect and initialize each device, as well as
  // registering each one with any higher-level interfaces.  Also does
  // general driver initialization.
  
  int status = 0;
  int driveNum = 0;
  int numberHardDisks = 0;
  int numberCdRoms = 0;
  int numberIdeDisks = 0;
  unsigned short buffer[256];
  uquad_t tmpNumSectors = 0;
  char model[IDE_MAX_DISKS][41];
  int needPci = 0;
  kernelDevice *devices = NULL;
  char value[80];
  int count;

  kernelLog("IDE: Examining disks...");

  // Clear the controller memory
  kernelMemClear((void *) &controller, sizeof(ideController));
  kernelMemClear((void *) disks, (sizeof(kernelPhysicalDisk) * IDE_MAX_DISKS));
  // Clear model strings
  kernelMemClear(model, (IDE_MAX_DISKS * 41));

  // Copy the default interrupt numbers
  controller.channel[0].interrupt = INTERRUPT_NUM_PRIMARYIDE;
  controller.channel[1].interrupt = INTERRUPT_NUM_SECONDARYIDE;

  // Copy the default port addresses
  for (count = 0; count < (IDE_MAX_DISKS / 2); count ++)
    kernelMemCopy(&defaultPorts[count], (void *)
		  &controller.channel[count].ports, sizeof(idePorts));

  // Register interrupt handlers and turn on the interrupts

  // Primary
  status = kernelInterruptHook(controller.channel[0].interrupt,
			       &primaryIdeInterrupt);
  if (status < 0)
    return (status);
  kernelPicMask(controller.channel[0].interrupt, 1);
  
  // Secondary
  status = kernelInterruptHook(controller.channel[1].interrupt,
			       &secondaryIdeInterrupt);
  if (status < 0)
    return (status);
  kernelPicMask(controller.channel[1].interrupt, 1);

  for (driveNum = 0; (driveNum < IDE_MAX_DISKS); driveNum ++)
    {
      // Wait for a lock on the controller
      status = kernelLockGet(&(CHANNEL(driveNum).lock));
      if (status < 0)
	return (status);

      // Select the drive
      selectDrive(driveNum);
  
      // Try to wait for the selected drive to be ready, but don't quit
      // if not since CD-ROMs don't seem to respond to this when they're
      // masters.
      pollStatus(driveNum, IDE_DRV_RDY, 1);

      disks[driveNum].description = "Unknown IDE disk";
      disks[driveNum].deviceNumber = driveNum;
      disks[driveNum].driver = driver;
  
      // Send an 'identify' command to the disk
      status = identify(driveNum, buffer);
      if (status < 0)
	// Eek, skip it.
	goto nextDisk;

      // Is it regular ATA?
      if ((buffer[0] & 0x8000) == 0)
	{
	  // This is an ATA hard disk device
	  kernelLog("IDE: Disk %d is an IDE hard disk", driveNum);
	      
	  sprintf((char *) disks[driveNum].name, "hd%d", numberHardDisks);
	  disks[driveNum].description = "IDE/ATA hard disk";
	  disks[driveNum].type =
	    (DISKTYPE_PHYSICAL | DISKTYPE_FIXED | DISKTYPE_IDEDISK);
	  disks[driveNum].flags = DISKFLAG_MOTORON;

	  // Get the geometry

	  disks[driveNum].cylinders =
	    kernelOsLoaderInfo->hddInfo[numberHardDisks].cylinders;
	  disks[driveNum].heads =
	    kernelOsLoaderInfo->hddInfo[numberHardDisks].heads;
	  disks[driveNum].sectorsPerCylinder = 
	    kernelOsLoaderInfo->hddInfo[numberHardDisks].sectorsPerCylinder;
	  disks[driveNum].numSectors =
	    kernelOsLoaderInfo->hddInfo[numberHardDisks].totalSectors;
	  disks[driveNum].sectorSize =
	    kernelOsLoaderInfo->hddInfo[numberHardDisks].bytesPerSector;

	  // If the 'identify device' data specifies a number of sectors,
	  // and that number is greater than the number we got from the BIOS,
	  // use the larger value.
	  tmpNumSectors = *((unsigned *)(buffer + 60));

	  if (tmpNumSectors && (tmpNumSectors < 0x0FFFFFFF))
	    {
	      if (tmpNumSectors > disks[driveNum].numSectors)
		disks[driveNum].numSectors = tmpNumSectors;
	    }
	  else
	    {
	      tmpNumSectors = *((uquad_t *)(buffer + 100));

	      if (tmpNumSectors && (tmpNumSectors < 0x0000FFFFFFFFFFFFULL))
		{
		  if (tmpNumSectors > disks[driveNum].numSectors)
		    disks[driveNum].numSectors = tmpNumSectors;
		}
	    }

	  // Sector size sometimes 0?  We can't have that as we are about
	  // to use it to perform a division operation.
	  if (disks[driveNum].sectorSize == 0)
	    {
	      // Try to get it from the 'identify device' info
	      disks[driveNum].sectorSize = buffer[5];

	      if (disks[driveNum].sectorSize == 0)
		{
		  kernelError(kernel_warn, "Physical disk %d sector size 0; "
			      "assuming 512", driveNum);
		  disks[driveNum].sectorSize = 512;
		}
	    }

	  // Sanity-check the geometry

	  // In some cases, we are detecting hard disks that don't seem
	  // to actually exist.  Check whether the number of cylinders
	  // passed by the loader is non-NULL.
	  if (disks[driveNum].cylinders == 0)
	    {
	      // Try to get it from the 'identify device' info
	      disks[driveNum].cylinders = buffer[1];
	  
	      if (disks[driveNum].cylinders == 0)
		kernelError(kernel_warn, "Physical disk %d cylinders 0",
			    driveNum);
	    }

	  if (disks[driveNum].heads == 0)
	    {
	      // Try to get it from the 'identify device' info
	      disks[driveNum].heads = buffer[3];
	  
	      if (disks[driveNum].heads == 0)
		kernelError(kernel_warn, "Physical disk %d heads 0", driveNum);
	    }

	  if (disks[driveNum].sectorsPerCylinder == 0)
	    {
	      // Try to get it from the 'identify device' info
	      disks[driveNum].sectorsPerCylinder = buffer[6];
	  
	      if (disks[driveNum].sectorsPerCylinder == 0)
		kernelError(kernel_warn, "Physical disk %d sectors 0",
			    driveNum);
	    }

	  // The cylinder number can be limited by the BIOS, but the heads
	  // and sectors are usually correct.  Make sure C*H*S is the same
	  // as the number of sectors, and if not, adjust the cylinder number
	  // accordingly.
	  if ((disks[driveNum].cylinders * disks[driveNum].heads *
	       disks[driveNum].sectorsPerCylinder) !=
	      disks[driveNum].numSectors)
	    disks[driveNum].cylinders =
	      (disks[driveNum].numSectors /
	       (disks[driveNum].heads * disks[driveNum].sectorsPerCylinder));

	  numberHardDisks += 1;
	}

      // Is it ATAPI?
      else if ((buffer[0] & 0xC000) == 0x8000)
	{
	  // This is an ATAPI device (such as a CD-ROM)
	  kernelLog("IDE: Disk %d is an IDE CD-ROM", driveNum);

	  sprintf((char *) disks[driveNum].name, "cd%d", numberCdRoms);
	  disks[driveNum].description = "IDE/ATAPI CD-ROM";
	  // Removable?
	  if (buffer[0] & 0x0080)
	    disks[driveNum].type |= DISKTYPE_REMOVABLE;
	  else
	    disks[driveNum].type |= DISKTYPE_FIXED;

	  // Device type: Bits 12-8 of buffer[0] should indicate 0x05 for
	  // CDROM, but we will just warn if it isn't for now
	  disks[driveNum].type |= DISKTYPE_IDECDROM;
	  if (((buffer[0] & 0x1F00) >> 8) != 0x05)
	    kernelError(kernel_warn, "ATAPI device type may not be supported");

	  if ((buffer[0] & 0x0003) != 0)
	    kernelError(kernel_warn, "ATAPI packet size not 12");

	  atapiReset(driveNum);

	  // Return some information we know from our device info command
	  disks[driveNum].cylinders = (unsigned) buffer[1];
	  disks[driveNum].heads = (unsigned) buffer[3];
	  disks[driveNum].sectorsPerCylinder = (unsigned) buffer[6];
	  disks[driveNum].numSectors = 0xFFFFFFFF;
	  disks[driveNum].sectorSize = 2048;

	  numberCdRoms += 1;
	}

      // Get the model string
      for (count = 0; count < 20; count ++)
	((unsigned short *) model[driveNum])[count] =
	  kernelProcessorSwap16(buffer[27 + count]);
      for (count = 39; ((count >= 0) && (model[driveNum][count] == ' '));
	   count --)
	model[driveNum][count] = '\0';
      kernelLog("IDE: Disk %d model \"%s\"", driveNum, model[driveNum]);

      // Now do some general feature detection (common to hard disks and
      // CD-ROMs)

      // See whether the disk supports multi-sector reads/writes.
      if ((buffer[47] & 0xFF) > 1)
	{
	  DISK(driveNum).featureFlags |= IDE_FEATURE_MULTI;
	  disks[driveNum].multiSectors = (buffer[47] & 0xFF);
	  kernelDebug(debug_io, "IDE: Disk %d multiSectors %d", driveNum,
		      disks[driveNum].multiSectors);
	}
      else 
	disks[driveNum].multiSectors = 1;

      // See whether the disk supports various DMA transfer modes.
      if (((buffer[53] & 0x0004) && (buffer[88] & 0x007F)) ||
	  (buffer[49] & 0x0100))
	{
	  for (count = 0; dmaModes[count].name; count ++)
	    {
	      if (buffer[dmaModes[count].identByte] &
		  dmaModes[count].supportedMask)
		{
		  if (setTransferMode(driveNum, &(dmaModes[count]),
				      buffer) >= 0)
		    {
		      DISK(driveNum).featureFlags |=
			dmaModes[count].featureFlag;
		      DISK(driveNum).dmaMode = dmaModes[count].name;
		      kernelDebug(debug_io, "IDE: Disk %d supports %s",
				  driveNum, DISK(driveNum).dmaMode);
		      break;
		    }
		}
	    }
	}

      // If one of the DMA modes was enabled, we need bus mastering info
      // from PCI
      if (DISK(driveNum).featureFlags & IDE_FEATURE_DMA)
	{
	  kernelLog("IDE: Disk %d in DMA mode", driveNum);
	  needPci = 1;
	}

      // See whether the disk supports SMART functionality.
      if (buffer[82] & 0x0001)
	{
	  // SMART is supported
	  DISK(driveNum).featureFlags |= IDE_FEATURE_SMART;
	  kernelDebug(debug_io, "IDE: Disk %d supports SMART", driveNum);
	}

      // Other features
      for (count = 0; features[count].name; count ++)
	{
	  if (buffer[features[count].suppByte] & features[count].suppMask)
	    {
	      // Supported.  Do we have to enable it?
	      if (features[count].featureCode)
		{
		  if (enableFeature(driveNum, &features[count], buffer) < 0)
		    continue;
		}

	      DISK(driveNum).featureFlags |= features[count].featureFlag;
	      kernelDebug(debug_io, "IDE: Disk %d supports %s",
			  driveNum, features[count].name);
	    }
	}

      // Increase the overall count of IDE disks
      numberIdeDisks += 1;

    nextDisk:
      kernelLockRelease(&(CHANNEL(driveNum).lock));
    }

  // If we have DMA-enabled disks, gather the bus mastering info from PCI
  if (needPci)
    parent = driverDetectPci();

  // Test DMA operation for any ATA (not ATAPI) devices that claim to
  // support it
  for (driveNum = 0; (driveNum < IDE_MAX_DISKS); driveNum ++)
    if (DISKISDMA(driveNum) && !(disks[driveNum].type & DISKTYPE_IDECDROM))
      testDma(driveNum);

  // Allocate memory for the device(s)
  devices = kernelMalloc(numberIdeDisks * (sizeof(kernelDevice) +
					   sizeof(kernelPhysicalDisk)));
  if (devices == NULL)
    return (status = 0);

  for (driveNum = 0; (driveNum < IDE_MAX_DISKS); driveNum ++)
    if (disks[driveNum].name[0])
      {
	devices[driveNum].device.class =
	  kernelDeviceGetClass(DEVICECLASS_DISK);
	devices[driveNum].device.subClass =
	  kernelDeviceGetClass(DEVICESUBCLASS_DISK_IDE);
	devices[driveNum].driver = driver;
	devices[driveNum].data = (void *) &disks[driveNum];

	// Register the disk
	status = kernelDiskRegisterDevice(&devices[driveNum]);
	if (status < 0)
	  return (status);

	status = kernelDeviceAdd(parent, &devices[driveNum]);
	if (status < 0)
	  return (status);

	kernelDebug(debug_io, "IDE: Disk %s successfully detected",
		    (char *) disks[driveNum].name);

	// Initialize the variable list for attributes of the disk.
	status = kernelVariableListCreate(&(devices[driveNum].device.attrs));
	if (status >= 0)
	  {
	    kernelVariableListSet(&(devices[driveNum].device.attrs),
				  DEVICEATTRNAME_MODEL, model[driveNum]);

	    if (DISKISMULTI(driveNum))
	      {
		sprintf(value, "%d", disks[driveNum].multiSectors);
		kernelVariableListSet(&(devices[driveNum].device.attrs),
				      "disk.multisectors", value);
	      }

	    value[0] = '\0';
	    if (DISKISDMA(driveNum))
	      strcat(value, DISK(driveNum).dmaMode);
	    else
	      strcat(value, "PIO");

	    if (DISKISSMART(driveNum))
	      strcat(value, ",SMART");

	    if (DISKISRCACHE(driveNum))
	      strcat(value, ",rcache");

	    if (DISKISWCACHE(driveNum))
	      strcat(value, ",wcache");
	    
	    if (DISKIS48(driveNum))
	      strcat(value, ",48-bit");
	    
	    kernelVariableListSet(&(devices[driveNum].device.attrs),
				  "disk.features", value);
	  }
      }

  return (status = 0);
}


static kernelDiskOps ideOps = {
  driverReset,
  driverRecalibrate,
  NULL, // driverSetMotorState
  driverSetLockState,
  driverSetDoorState,
  NULL, // driverDiskChanged
  driverReadSectors,
  driverWriteSectors,
  driverFlush
};


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


void kernelIdeDriverRegister(kernelDriver *driver)
{
  // Device driver registration.

  driver->driverDetect = driverDetect;
  driver->ops = &ideOps;

  return;
}
