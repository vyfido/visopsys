//
//  Visopsys
//  Copyright (C) 1998-2011 J. Andrew McLaughlin
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
//  kernelScsiDiskDriver.c
//

// Driver for standard and USB SCSI disks

#include "kernelScsiDiskDriver.h"
#include "kernelDebug.h"
#include "kernelDisk.h"
#include "kernelError.h"
#include "kernelFilesystem.h"
#include "kernelMalloc.h"
#include "kernelMisc.h"
#include "kernelProcessorX86.h"
#include "kernelRandom.h"
#include <stdio.h>
#include <string.h>

static kernelPhysicalDisk *disks[SCSI_MAX_DISKS];
static int numDisks = 0;


static void usbInterrupt(usbDevice *usbDev __attribute__((unused)),
			 void *buffer __attribute__((unused)),
			 unsigned length __attribute__((unused)))
{
  kernelDebug(debug_scsi, "USB MS interrupt");
}


static int usbMassStorageReset(kernelScsiDisk *dsk)
{
  // Send the USB "mass storage reset" command to the first interface of
  // the device

  int status = 0;

  kernelDebug(debug_scsi, "USB MS reset");

  // Do the control transfer to send the reset command
  status = kernelUsbControlTransfer(&dsk->usb.usbDev, USB_MASSSTORAGE_RESET,
				    0, dsk->usb.usbDev.interDesc[0]->interNum,
				    0, NULL, NULL);
  if (status < 0)
    kernelDebug(debug_scsi, "USB MS reset failed");

  return (status);
}


static int usbClearHalt(kernelScsiDisk *dsk, unsigned char endpoint)
{
  int status = 0;

  kernelDebug(debug_scsi, "USB MS clear halt, endpoint %d", endpoint);

  // Do the control transfer to send the 'clear (halt) feature' to the
  // endpoint
  status = kernelUsbControlTransfer(&dsk->usb.usbDev, USB_CLEAR_FEATURE,
				    USB_FEATURE_ENDPOINTHALT, endpoint, 0,
				    NULL, NULL);
  if (status < 0)
    kernelDebug(debug_scsi, "USB MS clear halt failed");

  return (status);
}


static int usbMassStorageResetRecovery(kernelScsiDisk *dsk)
{
  // Send the USB "mass storage reset" command to the first interface of
  // the device, and clear any halt conditions on the bulk-in and bulk-out
  // endpoints.

  int status = 0;

  kernelDebug(debug_scsi, "USB MS reset recovery");

  status = usbMassStorageReset(dsk);
  if (status < 0)
    goto out;

  status = usbClearHalt(dsk, dsk->usb.bulkInEndpoint);
  if (status < 0)
    goto out;

  status = usbClearHalt(dsk, dsk->usb.bulkOutEndpoint);

 out:
  if (status < 0)
    kernelDebug(debug_scsi, "USB MS reset recovery failed");

  return (status);
}


static int usbScsiCommand(kernelScsiDisk *dsk, unsigned char lun, void *cmd,
			  unsigned char cmdLength, void *data,
			  unsigned dataLength, unsigned *bytes, int read)
{
  // Wrap a SCSI command in a USB command block wrapper and send it to
  // the device.
  
  int status = 0;
  usbCmdBlockWrapper cmdWrapper;
  usbCmdStatusWrapper statusWrapper;
  usbTransaction trans[3];
  usbTransaction *cmdTrans = NULL;
  usbTransaction *dataTrans = NULL;
  usbTransaction *statusTrans = NULL;
  int transCount = 0;

  kernelDebug(debug_scsi, "USB MS command 0x%02x datalength %d",
	      ((scsiUsbCmd12 *) cmd)->byte[0], dataLength);

  kernelMemClear((void *) trans, (3 * sizeof(usbTransaction)));

  // Set up the command wrapper
  kernelMemClear(&cmdWrapper, sizeof(usbCmdBlockWrapper));
  cmdWrapper.signature = USB_CMDBLOCKWRAPPER_SIG;
  cmdWrapper.tag = ++(dsk->usb.tag);
  cmdWrapper.dataLength = dataLength;
  cmdWrapper.flags = (read << 7);
  cmdWrapper.lun = lun;
  cmdWrapper.cmdLength = cmdLength;
  // Copy the command data into the wrapper
  kernelMemCopy(cmd, cmdWrapper.cmd, cmdLength);
  kernelDebug(debug_scsi, "USB MS command length %d", cmdWrapper.cmdLength);

  // Set up the USB transaction to send the command
  cmdTrans = &trans[transCount++];
  cmdTrans->type = usbxfer_bulk;
  cmdTrans->address = dsk->usb.usbDev.address;
  cmdTrans->endpoint = dsk->usb.bulkOutEndpoint;
  cmdTrans->pid = USB_PID_OUT;
  cmdTrans->length = sizeof(usbCmdBlockWrapper);
  cmdTrans->buffer = &cmdWrapper;

  if (dataLength)
    {
      if (bytes)
	*bytes = 0;

      // Set up the USB transaction to read or write the data
      dataTrans = &trans[transCount++];
      dataTrans->type = usbxfer_bulk;
      dataTrans->address = dsk->usb.usbDev.address;
      dataTrans->length = dataLength;
      dataTrans->buffer = data;

      if (read)
	{
	  dataTrans->endpoint = dsk->usb.bulkInEndpoint;
	  dataTrans->pid = USB_PID_IN;
	}
      else
	{
	  dataTrans->endpoint = dsk->usb.bulkOutEndpoint;
	  dataTrans->pid = USB_PID_OUT;
	}

      kernelDebug(debug_scsi, "USB MS datalength=%u", dataLength);
    }

  // Set up the status wrapper
  kernelMemClear(&statusWrapper, sizeof(usbCmdStatusWrapper));
  statusWrapper.signature = USB_CMDSTATUSWRAPPER_SIG;
  statusWrapper.tag = cmdWrapper.tag;

  // Set up the USB transaction to read the status
  statusTrans = &trans[transCount++];
  statusTrans->type = usbxfer_bulk;
  statusTrans->address = dsk->usb.usbDev.address;
  statusTrans->endpoint = dsk->usb.bulkInEndpoint;
  statusTrans->pid = USB_PID_IN;
  statusTrans->length = sizeof(usbCmdStatusWrapper);
  statusTrans->buffer = &statusWrapper;
  kernelDebug(debug_scsi, "USB MS status length=%u", statusTrans->length);

  // Write the transactions
  status =
    kernelBusWrite(dsk->busTarget, (transCount * sizeof(usbTransaction)),
		   (void *) &trans);
  if (status < 0)
    {
      kernelDebug(debug_scsi, "USB MS trans error=%d", status);

      // Try to clear the stall
      if (usbClearHalt(dsk, dsk->usb.bulkInEndpoint) < 0)
	// Try a reset
	usbMassStorageResetRecovery(dsk);

      return (status);
    }

  if (dataLength)
    {
      if (!dataTrans->bytes)
	{
	  kernelDebug(debug_scsi, "USB MS data trans no data error");
	  return (status = ERR_NODATA);
	}
      
      if (bytes)
	*bytes = (unsigned) dataTrans->bytes;
    }

  if (statusWrapper.status & SCSI_STAT_MASK)
    {
      kernelDebug(debug_scsi, "USB MS command error status %02x",
		  (statusWrapper.status & SCSI_STAT_MASK));
      return (status = ERR_IO);
    }
  else
    {
      kernelDebug(debug_scsi, "USB MS command successful");
      return (status = 0);
    }
}


static void debugInquiry(scsiInquiryData *inquiryData)
{
  char vendorId[9];
  char productId[17];
  char productRev[17];

  strncpy(vendorId, inquiryData->vendorId, 8);
  vendorId[8] = '\0';
  strncpy(productId, inquiryData->productId, 16);
  productId[16] = '\0';
  strncpy(productRev, inquiryData->productRev, 4);
  productRev[4] = '\0';

  kernelDebug(debug_scsi, "SCSI debug inquiry data:\n"
	      "    qual/devType=%02x\n"
	      "    removable=%02x\n"
	      "    version=%02x\n"
	      "    normACA/hiSup/format=%02x\n"
	      "    addlLength=%02x\n"
	      "    byte5Flags=%02x\n"
	      "    byte6Flags=%02x\n"
	      "    relAddr=%02x\n"
	      "    vendorId=%s\n"
	      "    productId=%s\n"
	      "    productRev=%s", inquiryData->byte0.periQual,
	      inquiryData->byte1.removable, inquiryData->byte2.ansiVersion,
	      inquiryData->byte3.dataFormat, inquiryData->byte4.addlLength,
	      inquiryData->byte5, inquiryData->byte6,
	      inquiryData->byte7.relAdr, vendorId, productId, productRev);
}


static int scsiInquiry(kernelScsiDisk *dsk, unsigned char lun,
		       scsiInquiryData *inquiryData)
{
  // Do a SCSI 'inquiry' command.

  int status = 0;
  scsiUsbCmd12 cmd12;
  unsigned bytes = 0;

  kernelDebug(debug_scsi, "SCSI inquiry");
  kernelMemClear(&cmd12, sizeof(scsiUsbCmd12));
  cmd12.byte[0] = SCSI_CMD_INQUIRY;
  cmd12.byte[1] = (lun << 5);
  cmd12.byte[4] = sizeof(scsiInquiryData);

  if (dsk->busTarget->bus->type == bus_usb)
    {
      // Set up the USB transaction, with the SCSI 'inquiry' command.
      status = usbScsiCommand(dsk, lun, &cmd12, sizeof(scsiUsbCmd12),
			      inquiryData, sizeof(scsiInquiryData), &bytes, 1);
      if ((status < 0) && (bytes < 36))
	return (status);
    }
  else
    {
      kernelDebugError("Non-USB SCSI not supported");
      return (status = ERR_NOTIMPLEMENTED);
    }

  debugInquiry(inquiryData);
  return (status = 0);
}


/*
static int scsiModeSense(kernelScsiDisk *dsk, unsigned char lun,
			 int pageCode, unsigned char *buffer, unsigned length)
{
  // Do a SCSI 'mode sense' command.

  int status = 0;
  scsiUsbCmd12 cmd12;
  unsigned bytes = 0;

  kernelDebug(debug_scsi, "SCSI mode sense");
  kernelMemClear(&cmd12, sizeof(scsiUsbCmd12));
  cmd12.byte[0] = SCSI_CMD_MODESENSE6;
  cmd12.byte[1] = ((lun << 5) | 0x08);
  cmd12.byte[2] = pageCode;
  cmd12.byte[4] = length;

  if (dsk->busTarget->bus->type == bus_usb)
    {
      // Set up the USB transaction, with the SCSI 'mode sense' command.
      status = usbScsiCommand(dsk, lun, &cmd12, sizeof(scsiUsbCmd12), buffer,
			      min(dsk->usb.bulkIn->maxPacketSize, length),
			      &bytes, 1);
      if ((status < 0) && (bytes < length))
	return (status);
    }
  else
    {
      kernelDebugError("Non-USB SCSI not supported");
      return (status = ERR_NOTIMPLEMENTED);
    }

  return (status = 0);
}
*/


static int scsiReadWrite(kernelScsiDisk *dsk, unsigned char lun,
			 unsigned logicalSector, unsigned short numSectors,
			 void *buffer, int read)
{
  // Do a SCSI 'read' or 'write' command

  int status = 0;
  unsigned dataLength = 0;
  scsiUsbCmd12 cmd12;
  unsigned bytes = 0;

  dataLength = (numSectors * dsk->sectorSize);
  kernelDebug(debug_scsi, "SCSI %s %u bytes sectorsize %u",
	      (read? "read" : "write"), dataLength, dsk->sectorSize);

  kernelMemClear(&cmd12, sizeof(scsiUsbCmd12));
  if (read)
    cmd12.byte[0] = SCSI_CMD_READ10;
  else
    cmd12.byte[0] = SCSI_CMD_WRITE10;
  cmd12.byte[1] = (lun << 5);
  *((unsigned *) &cmd12.byte[2]) = kernelProcessorSwap32(logicalSector);
  *((unsigned short *) &cmd12.byte[7]) = kernelProcessorSwap16(numSectors);

  if (dsk->busTarget->bus->type == bus_usb)
    {
      // Set up the USB transaction, with the SCSI 'read' command.
      status = usbScsiCommand(dsk, lun, &cmd12, sizeof(scsiUsbCmd12), buffer,
			      dataLength, &bytes, read);
      if ((status < 0) && (bytes < dataLength))
	return (status);
    }
  else
    {
      kernelDebugError("Non-USB SCSI not supported");
      return (status = ERR_NOTIMPLEMENTED);
    }

  kernelDebug(debug_scsi, "SCSI %s successul %u bytes",
	      (read? "read" : "write"), bytes);
  return (status = 0);
}


static int scsiReadCapacity(kernelScsiDisk *dsk, unsigned char lun,
			    scsiCapacityData *capacityData)
{
  // Do a SCSI 'read capacity' command.

  int status = 0;
  scsiUsbCmd12 cmd12;
  unsigned bytes = 0;

  kernelDebug(debug_scsi, "SCSI read capacity");
  kernelMemClear(&cmd12, sizeof(scsiUsbCmd12));
  cmd12.byte[0] = SCSI_CMD_READCAPACITY;
  cmd12.byte[1] = (lun << 5);

  if (dsk->busTarget->bus->type == bus_usb)
    {
      // Set up the USB transaction, with the SCSI 'read capacity' command.
      status =
	usbScsiCommand(dsk, lun, &cmd12, sizeof(scsiUsbCmd12), capacityData,
		       sizeof(scsiCapacityData), &bytes, 1);
      if ((status < 0) && (bytes < sizeof(scsiCapacityData)))
	return (status);
    }
  else
    {
      kernelDebugError("Non-USB SCSI not supported");
      return (status = ERR_NOTIMPLEMENTED);
    }

  // Swap bytes around
  capacityData->blockNumber = kernelProcessorSwap32(capacityData->blockNumber);
  capacityData->blockLength = kernelProcessorSwap32(capacityData->blockLength);

  kernelDebug(debug_scsi, "SCSI read capacity successul");
  return (status = 0);
}


static int scsiRequestSense(kernelScsiDisk *dsk, unsigned char lun,
			    scsiSenseData *senseData)
{
  // Do a SCSI 'request sense' command.

  int status = 0;
  scsiUsbCmd12 cmd12;
  unsigned bytes = 0;

  kernelDebug(debug_scsi, "SCSI request sense");
  kernelMemClear(&cmd12, sizeof(scsiUsbCmd12));
  cmd12.byte[0] = SCSI_CMD_REQUESTSENSE;
  cmd12.byte[1] = (lun << 5);
  cmd12.byte[4] = sizeof(scsiSenseData);

  if (dsk->busTarget->bus->type == bus_usb)
    {
      // Set up the USB transaction, with the SCSI 'request sense' command.
      status = usbScsiCommand(dsk, lun, &cmd12, sizeof(scsiUsbCmd12),
			      senseData, sizeof(scsiSenseData), &bytes, 1);
      if ((status < 0) && (bytes < sizeof(scsiSenseData)))
	return (status);
    }
  else
    {
      kernelDebugError("Non-USB SCSI not supported");
      return (status = ERR_NOTIMPLEMENTED);
    }

  // Swap bytes around
  senseData->info = kernelProcessorSwap32(senseData->info);
  senseData->cmdSpecific = kernelProcessorSwap32(senseData->cmdSpecific);

  kernelDebug(debug_scsi, "SCSI request sense successul");
  return (status = 0);
}


static int scsiStartStopUnit(kernelScsiDisk *dsk, unsigned char lun,
			     unsigned char startStop, unsigned char loadEject)
{
  // Do a SCSI 'start/stop unit' command.

  int status = 0;
  scsiUsbCmd12 cmd12;

  kernelDebug(debug_scsi, "SCSI %s unit", (startStop? "start" : "stop"));
  kernelMemClear(&cmd12, sizeof(scsiUsbCmd12));
  cmd12.byte[0] = SCSI_CMD_STARTSTOPUNIT;
  cmd12.byte[1] = (lun << 5);
  cmd12.byte[4] = (((loadEject & 0x01) << 1) | (startStop & 0x01));

  if (dsk->busTarget->bus->type == bus_usb)
    {
      // Set up the USB transaction, with the SCSI 'start/stop unit' command.
      status = usbScsiCommand(dsk, lun, &cmd12, sizeof(scsiUsbCmd12), NULL, 0,
			      NULL, 0);
      if (status < 0)
	return (status);
    }
  else
    {
      kernelDebugError("Non-USB SCSI not supported");
      return (status = ERR_NOTIMPLEMENTED);
    }

  kernelDebug(debug_scsi, "SCSI %s unit successful",
	      (startStop? "start" : "stop"));
  return (status = 0);
}


static int scsiTestUnitReady(kernelScsiDisk *dsk, unsigned char lun)
{
  // Do a SCSI 'test unit ready' command.

  int status = 0;
  scsiUsbCmd12 cmd12;

  kernelDebug(debug_scsi, "SCSI test unit ready");
  kernelMemClear(&cmd12, sizeof(scsiUsbCmd12));
  cmd12.byte[0] = SCSI_CMD_TESTUNITREADY;
  cmd12.byte[1] = (lun << 5);

  if (dsk->busTarget->bus->type == bus_usb)
    {
      // Set up the USB transaction, with the SCSI 'test unit ready' command.
      status = usbScsiCommand(dsk, lun, &cmd12, sizeof(scsiUsbCmd12), NULL, 0,
			      NULL, 0);
      if (status < 0)
	return (status);
    }
  else
    {
      kernelDebugError("Non-USB SCSI not supported");
      return (status = ERR_NOTIMPLEMENTED);
    }

  kernelDebug(debug_scsi, "SCSI test unit ready successful");
  return (status = 0);
}


static int getNewDiskNumber(void)
{
  // Return an unused disk number

  int diskNumber = 0;
  int count;

  for (count = 0; count < numDisks ; count ++)
    {
      if (disks[count]->deviceNumber == diskNumber)
	{
	  diskNumber += 1;
	  count = -1;
	  continue;
	}
    }

  return (diskNumber);
}


static void guessDiskGeom(kernelPhysicalDisk *physicalDisk)
{
  // Given a disk with the number of sectors field set, try to figure out
  // some geometry values that make sense

  struct {
    unsigned heads;
    unsigned sectors;
  } guesses[] = {
    { 255, 63 },
    { 16, 63 },
    { 255, 32 },
    { 16, 32 },
    { 0, 0 }
  };
  int count;

  // See if any of our guesses match up with the number of sectors.
  for (count = 0; guesses[count].heads; count ++)
    {
      if (!(physicalDisk->numSectors %
	    (guesses[count].heads * guesses[count].sectors)))
	{
	  physicalDisk->heads = guesses[count].heads;
	  physicalDisk->sectorsPerCylinder = guesses[count].sectors;
	  physicalDisk->cylinders =
	    (physicalDisk->numSectors /
	     (guesses[count].heads * guesses[count].sectors));
	  goto out;
	}
    }

  // Nothing yet.  Instead, try to calculate something on the fly.
  physicalDisk->heads = 16;
  physicalDisk->sectorsPerCylinder = 32;
  while (physicalDisk->heads < 256)
    {
      if (!(physicalDisk->numSectors %
	    (physicalDisk->heads * physicalDisk->sectorsPerCylinder)))
	{
	  physicalDisk->cylinders =
	    (physicalDisk->numSectors /
	     (physicalDisk->heads * physicalDisk->sectorsPerCylinder));
	  goto out;
	}

      physicalDisk->heads += 1;
    }

  kernelError(kernel_warn, "Unable to guess disk geometry");
  return;

 out:

  kernelDebug(debug_scsi, "SCSI guess geom %u/%u/%u", physicalDisk->cylinders,
	      physicalDisk->heads, physicalDisk->sectorsPerCylinder);
  return;
}


static kernelPhysicalDisk *detectTarget(void *parent, int busType,
					int targetId, void *driver)
{
  // Given a bus type and a bus target number, see if the device is a
  // SCSI disk

  int status = 0;
  kernelScsiDisk *dsk = NULL;
  kernelPhysicalDisk *physicalDisk = NULL;
  scsiSenseData senseData;
  scsiInquiryData inquiryData;
  scsiCapacityData capacityData;
  int count;

  dsk = kernelMalloc(sizeof(kernelScsiDisk));
  if (dsk == NULL)
    goto err_out;

  dsk->busTarget = kernelBusGetTarget(busType, targetId);
  if (dsk->busTarget == NULL)
    goto err_out;

  dsk->dev = kernelMalloc(sizeof(kernelDevice));
  if (dsk->dev == NULL)
    goto err_out;

  physicalDisk = kernelMalloc(sizeof(kernelPhysicalDisk));
  if (physicalDisk == NULL)
    goto err_out;

  if (dsk->busTarget->bus->type == bus_usb)
    {
      // Try to get the USB information about the target
      status =
	kernelBusGetTargetInfo(dsk->busTarget, (void *) &dsk->usb.usbDev);
      if (status < 0)
	goto err_out;

      // If the USB class is 0x08 and the subclass is 0x06 then we believe
      // we have a SCSI device
      if ((dsk->usb.usbDev.classCode != 0x08) ||
	  (dsk->usb.usbDev.subClassCode != 0x06) ||
	  (dsk->usb.usbDev.protocol != 0x50))
	goto err_out;

      // Record the bulk-in and bulk-out endpoints, and any interrupt endpoint
      kernelDebug(debug_scsi, "USB MS search for bulk endpoints");
      for (count = 1; count < dsk->usb.usbDev.numEndpoints; count ++)
	{
	  if ((dsk->usb.usbDev.endpointDesc[count]->attributes == 2) &&
	      !dsk->usb.bulkInDesc &&
	      (dsk->usb.usbDev.endpointDesc[count]->endpntAddress & 0x80))
	    {
	      dsk->usb.bulkInDesc = dsk->usb.usbDev.endpointDesc[count];
	      dsk->usb.bulkInEndpoint =
		dsk->usb.bulkInDesc->endpntAddress;
	      kernelDebug(debug_scsi, "USB MS bulk in endpoint %d",
			  dsk->usb.bulkInEndpoint);
	    }

	  if ((dsk->usb.usbDev.endpointDesc[count]->attributes == 2) &&
	      !dsk->usb.bulkOutDesc &&
	      !(dsk->usb.usbDev.endpointDesc[count]->endpntAddress & 0x80))
	    {
	      dsk->usb.bulkOutDesc = dsk->usb.usbDev.endpointDesc[count];
	      dsk->usb.bulkOutEndpoint =
		dsk->usb.bulkOutDesc->endpntAddress;
	      kernelDebug(debug_scsi, "USB MS bulk out endpoint %d",
			  dsk->usb.bulkOutEndpoint);
	    }

	  if ((dsk->usb.usbDev.endpointDesc[count]->attributes == 3) &&
	      !dsk->usb.intrInDesc)
	    {
	      dsk->usb.intrInDesc = dsk->usb.usbDev.endpointDesc[count];
	      dsk->usb.intrInEndpoint =	dsk->usb.intrInDesc->endpntAddress;
	      kernelDebug(debug_scsi, "USB MS interrupt endpoint %d",
			  dsk->usb.intrInEndpoint);
	    }
	}

      // If there's an interrupt endpoint, try to register an interrupt
      // callback.
      if (dsk->usb.intrInDesc)
	kernelUsbScheduleInterrupt(&dsk->usb.usbDev, dsk->usb.intrInEndpoint,
				   dsk->usb.intrInDesc->interval,
				   dsk->usb.intrInDesc->maxPacketSize,
				   &usbInterrupt);
      
      kernelDebug(debug_scsi, "USB MS device detected");
      physicalDisk->type |= DISKTYPE_FLASHDISK;
    }
  else
    {
      kernelDebugError("Non-USB SCSI not supported");
      status = ERR_NOTIMPLEMENTED;
      goto err_out;
    }

  // Send a 'test unit ready' command
  status = scsiTestUnitReady(dsk, 0);
  if (status < 0)
    {
      status = scsiRequestSense(dsk, 0, &senseData);
      if (status < 0)
	goto err_out;
    }

  status = scsiRequestSense(dsk, 0, &senseData);
  if (status < 0)
    goto err_out;

  // Try to communicate with the new target by sending 'start unit' command
  status = scsiStartStopUnit(dsk, 0, 1, 0);
  if (status < 0)
    goto err_out;

  // Send an 'inquiry' command
  status = scsiInquiry(dsk, 0, &inquiryData);
  if (status < 0)
    goto err_out;

  if (inquiryData.byte1.removable & 0x80)
    physicalDisk->type |= DISKTYPE_REMOVABLE;
  else
    physicalDisk->type |= DISKTYPE_FIXED;

  // Set up the vendor and product ID strings

  strncpy(dsk->vendorId, inquiryData.vendorId, 8);
  dsk->vendorId[8] = '\0';
  for (count = 7; count >= 0; count --)
    {
      if (dsk->vendorId[count] != ' ')
	{
	  dsk->vendorId[count + 1] = '\0';
	  break;
	}
      else if (count == 0)
	dsk->vendorId[0] = '\0';
    }
  strncpy(dsk->productId, inquiryData.productId, 16);
  dsk->productId[16] = '\0';
  for (count = 15; count >= 0; count --)
    {
      if (dsk->productId[count] != ' ')
	{
	  dsk->productId[count + 1] = '\0';
	  break;
	}
      else if (count == 0)
	dsk->productId[0] = '\0';
    }
  snprintf(dsk->vendorProductId, 26, "%s%s%s", dsk->vendorId,
	   (dsk->vendorId[0]? " " : ""), dsk->productId);

  // Send a 'read capacity' command
  status = scsiReadCapacity(dsk, 0, &capacityData);
  if (status < 0)
    goto err_out;

  dsk->numSectors = (capacityData.blockNumber + 1);
  dsk->sectorSize = capacityData.blockLength;

  if ((dsk->sectorSize <= 0) || (dsk->sectorSize > 4096))
    {
      kernelError(kernel_error, "Unsupported sector size %u", dsk->sectorSize);
      status = ERR_NOTIMPLEMENTED;
      goto err_out;
    }

  kernelDebug(debug_scsi, "SCSI disk \"%s\" sectors %u sectorsize %u",
	      dsk->vendorProductId, dsk->numSectors, dsk->sectorSize);

  physicalDisk->deviceNumber = getNewDiskNumber();
  kernelDebug(debug_scsi, "SCSI disk %d detected", physicalDisk->deviceNumber);
  physicalDisk->description = dsk->vendorProductId;
  physicalDisk->type |= (DISKTYPE_PHYSICAL | DISKTYPE_SCSIDISK);
  physicalDisk->flags = DISKFLAG_MOTORON;
  physicalDisk->numSectors = dsk->numSectors;
  guessDiskGeom(physicalDisk);
  physicalDisk->sectorSize = dsk->sectorSize;
  physicalDisk->driverData = (void *) dsk;
  physicalDisk->driver = driver;
  disks[numDisks++] = physicalDisk;

  dsk->dev->device.class = kernelDeviceGetClass(DEVICECLASS_DISK);
  dsk->dev->device.subClass = kernelDeviceGetClass(DEVICESUBCLASS_DISK_SCSI);
  kernelVariableListCreate(&dsk->dev->device.attrs);
  kernelVariableListSet(&dsk->dev->device.attrs, DEVICEATTRNAME_VENDOR,
			dsk->vendorId);
  kernelVariableListSet(&dsk->dev->device.attrs, DEVICEATTRNAME_MODEL,
			dsk->productId);
  dsk->dev->driver = driver;
  dsk->dev->data = (void *) physicalDisk;

  status = kernelDiskRegisterDevice(dsk->dev);
  if (status < 0)
    goto err_out;

  status = kernelDeviceAdd(parent, dsk->dev);
  if (status < 0)
    goto err_out;

  return (physicalDisk);

 err_out:

  kernelError(kernel_error, "Error %d while detecting %sSCSI device",
	      status, ((dsk->busTarget->bus->type == bus_usb)? "USB " : ""));

  if (physicalDisk)
    kernelFree((void *) physicalDisk);
  if (dsk->dev)
    kernelFree(dsk->dev);
  if (dsk->busTarget)
    kernelFree(dsk->busTarget);
  if (dsk)
    kernelFree(dsk);

  return (physicalDisk = NULL);
}


static kernelPhysicalDisk *findBusTarget(kernelBusType busType, int target)
{
  // Try to find a disk in our list.
  
  kernelScsiDisk *dsk = NULL;
  int count;

  for (count = 0; count < numDisks; count ++)
    if (disks[count] && disks[count]->driverData)
      {
	dsk = (kernelScsiDisk *) disks[count]->driverData;

	if (dsk->busTarget && dsk->busTarget->bus &&
	    (dsk->busTarget->bus->type == busType) &&
	    (dsk->busTarget->id == target))
	  return (disks[count]);
      }

  // Not found
  return (NULL);
}


static void removeDisk(kernelPhysicalDisk *physicalDisk)
{
  // Remove a disk from our list.

  int position = -1;
  int count;

  // Find its position
  for (count = 0; count < numDisks; count ++)
    {
      if (disks[count] == physicalDisk)
	{
	  position = count;
	  break;
	}
    }

  if (position >= 0)
    {
      if ((numDisks > 1) && (position < (numDisks - 1)))
	{
	  for (count = position; count < (numDisks - 1); count ++)
	    disks[count] = disks[count + 1];
	}

      numDisks -= 1;
    }
}


static kernelScsiDisk *findDiskByNumber(int driveNum)
{
  int count = 0;

  for (count = 0; count < numDisks; count ++)
    {
      if (disks[count]->deviceNumber == driveNum)
	return ((kernelScsiDisk *) disks[count]->driverData);
    }

  // Not found
  return (NULL);
}


static int readWriteSectors(int driveNum, uquad_t logicalSector,
			    uquad_t numSectors, void *buffer, int read)
{
  // Read or write sectors.

  int status = 0;
  kernelScsiDisk *dsk = NULL;

  // Check params
  if (buffer == NULL)
    {
      kernelError(kernel_error, "NULL buffer parameter");
      return (status = ERR_NULLPARAMETER);
    }

  if (numSectors == 0)
    // Not an error we guess, but nothing to do
    return (status = 0);

  // Find the disk based on the disk number
  dsk = findDiskByNumber(driveNum);
  if (dsk == NULL)
    {
      kernelError(kernel_error, "No such disk, device number %d", driveNum);
      return (status = ERR_NOSUCHENTRY);
    }

  // Send a 'test unit ready' command
  status = scsiTestUnitReady(dsk, 0);
  if (status < 0)
    return (status);

  kernelDebug(debug_scsi, "SCSI %s %llu sectors on \"%s\" at %llu sectorsize "
	      "%u", (read? "read" : "write"), numSectors, dsk->vendorProductId,
	      logicalSector, dsk->sectorSize);

  status = scsiReadWrite(dsk, 0, logicalSector, numSectors, buffer, read);

  return (status);
}


static int driverReadSectors(int driveNum, uquad_t logicalSector,
			     uquad_t numSectors, void *buffer)
{
  // This routine is a wrapper for the readWriteSectors routine.
  return (readWriteSectors(driveNum, logicalSector, numSectors, buffer,
			   1));  // Read operation
}


static int driverWriteSectors(int driveNum, uquad_t logicalSector,
			      uquad_t numSectors, const void *buffer)
{
  // This routine is a wrapper for the readWriteSectors routine.
  return (readWriteSectors(driveNum, logicalSector, numSectors,
			   (void *) buffer, 0));  // Write operation
}


static int driverDetect(void *parent __attribute__((unused)),
			kernelDriver *driver)
{
  // Try to detect SCSI disks.

  int status = 0;
  kernelBusTarget *busTargets = NULL;
  int numBusTargets = 0;
  int deviceCount = 0;
  usbDevice usbDev;

  kernelDebug(debug_scsi, "SCSI search for devices");

  // Search the USB bus(es) for devices
  numBusTargets = kernelBusGetTargets(bus_usb, &busTargets);
  if (numBusTargets > 0)
    {
      // Search the bus targets for SCSI disk devices
      for (deviceCount = 0; deviceCount < numBusTargets; deviceCount ++)
	{
	  // Try to get the USB information about the target
	  status = kernelBusGetTargetInfo(&busTargets[deviceCount],
					  (void *) &usbDev);
	  if (status < 0)
	    continue;

	  if (usbDev.classCode != 0x08)
	    continue;
  
	  kernelDebug(debug_scsi, "SCSI found USB mass storage device");
	  detectTarget(usbDev.controller->device, bus_usb,
		       busTargets[deviceCount].id, driver);
	}

      kernelFree(busTargets);
    }

  return (status = 0);
}


static int driverHotplug(void *parent, int busType, int target, int connected,
			 kernelDriver *driver)
{
  // This routine is used to detect whether a newly-connected, hotplugged
  // device is supported by this driver during runtime, and if so to do the
  // appropriate device setup and registration.  Alternatively if the device
  // is disconnected a call to this function lets us know to stop trying
  // to communicate with it.

  int status = 0;
  kernelPhysicalDisk *physicalDisk = NULL;
  kernelScsiDisk *dsk = NULL;
  int count;

  if (connected)
    {
      // Determine whether any new SCSI disks have appeared on the USB bus

      physicalDisk = detectTarget(parent, busType, target, driver);
      if (physicalDisk != NULL)
	kernelDiskReadPartitions((char *) physicalDisk->name);
    }
  else
    {
      // Try to find the disk in our list

      physicalDisk = findBusTarget(busType, target);
      if (physicalDisk == NULL)
	{
	  // This can happen if SCSI initialization did not complete
	  // successfully.  In that case, it could be that we're still the
	  // registered driver for the device, but we never added it to our
	  // list.
	  kernelDebugError("No such SCSI device %d", target);
	  return (status = ERR_NOSUCHENTRY);
	}

      // Found it.
      kernelDebug(debug_scsi, "SCSI device removed");

      // If there are filesystems mounted on this disk, try to unmount them
      for (count = 0; count < physicalDisk->numLogical; count ++)
	{
	  if (physicalDisk->logical[count].filesystem.mounted)
	    kernelFilesystemUnmount((char *) physicalDisk->logical[count]
				    .filesystem.mountPoint);
	}

      dsk = (kernelScsiDisk *) physicalDisk->driverData;

      // Remove it from the system's disks
      kernelDiskRemoveDevice(dsk->dev);

      // Remove it from the device tree
      kernelDeviceRemove(dsk->dev);

      // Delete.
      removeDisk(physicalDisk);
      if (dsk->busTarget)
	kernelFree(dsk->busTarget);
      if (dsk->dev)
	kernelFree(dsk->dev);
    }

  return (status = 0);
}


static kernelDiskOps scsiOps = {
  NULL, // driverReset
  NULL, // driverRecalibrate
  NULL, // driverSetMotorState
  NULL, // driverSetLockState
  NULL, // driverSetDoorState
  NULL, // driverDiskChanged
  driverReadSectors,
  driverWriteSectors,
  NULL  // driverFlush
};


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


void kernelScsiDiskDriverRegister(kernelDriver *driver)
{
   // Device driver registration.

  driver->driverDetect = driverDetect;
  driver->driverHotplug = driverHotplug;
  driver->ops = &scsiOps;

  return;
}
