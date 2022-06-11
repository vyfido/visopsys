//
//  Visopsys
//  Copyright (C) 1998-2006 J. Andrew McLaughlin
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
//  kernelUsbUhciDriver.c
//

#include "kernelUsbDriver.h"
#include "kernelUsbUhciDriver.h"
#include "kernelLog.h"
#include "kernelMalloc.h"
#include "kernelMemory.h"
#include "kernelMisc.h"
#include "kernelMultitasker.h"
#include "kernelPage.h"
#include "kernelParameters.h"
#include "kernelPciDriver.h"
#include "kernelProcessorX86.h"
#include "kernelSysTimer.h"
#include "kernelError.h"
#include <string.h>
#include <stdlib.h>


//#define DEBUG

#ifdef DEBUG
static void debugDeviceDesc(usbDeviceDesc *deviceDesc)
{
  kernelTextPrintLine("USB: debug device descriptor:");
  kernelTextPrintLine("    descLength=%d", deviceDesc->descLength);
  kernelTextPrintLine("    descType=%d", deviceDesc->descType);
  kernelTextPrintLine("    usbVersion=%d.%d",
		      ((deviceDesc->usbVersion & 0xFF00) >> 8),
		      (deviceDesc->usbVersion & 0xFF));
  kernelTextPrintLine("    deviceClass=%x", deviceDesc->deviceClass);
  kernelTextPrintLine("    deviceSubClass=%x", deviceDesc->deviceSubClass);
  kernelTextPrintLine("    deviceProtocol=%x", deviceDesc->deviceProtocol);
  kernelTextPrintLine("    maxPacketSize0=%d", deviceDesc->maxPacketSize0);
  kernelTextPrintLine("    vendorId=%04x", deviceDesc->vendorId);
  kernelTextPrintLine("    productId=%04x", deviceDesc->productId);
  kernelTextPrintLine("    deviceVersion=%d.%d",
		      ((deviceDesc->deviceVersion & 0xFF00) >> 8),
		      (deviceDesc->deviceVersion & 0xFF));
  kernelTextPrintLine("    manuStringIdx=%d", deviceDesc->manuStringIdx);
  kernelTextPrintLine("    prodStringIdx=%d", deviceDesc->prodStringIdx);
  kernelTextPrintLine("    serStringIdx=%d", deviceDesc->serStringIdx);
  kernelTextPrintLine("    numConfigs=%d", deviceDesc->numConfigs);
}


static void debugDevQualDesc(usbDevQualDesc *devQualDesc)
{
  kernelTextPrintLine("USB: debug device qualifier descriptor:");
  kernelTextPrintLine("    descLength=%d", devQualDesc->descLength);
  kernelTextPrintLine("    descType=%d", devQualDesc->descType);
  kernelTextPrintLine("    usbVersion=%d.%d",
		      ((devQualDesc->usbVersion & 0xFF00) >> 8),
		      (devQualDesc->usbVersion & 0xFF));
  kernelTextPrintLine("    deviceClass=%x", devQualDesc->deviceClass);
  kernelTextPrintLine("    deviceSubClass=%x", devQualDesc->deviceSubClass);
  kernelTextPrintLine("    deviceProtocol=%x", devQualDesc->deviceProtocol);
  kernelTextPrintLine("    maxPacketSize0=%d", devQualDesc->maxPacketSize0);
  kernelTextPrintLine("    numConfigs=%d", devQualDesc->numConfigs);
}


static void debugConfigDesc(usbConfigDesc *configDesc)
{
  kernelTextPrintLine("USB: debug config descriptor:");
  kernelTextPrintLine("    descLength=%d", configDesc->descLength);
  kernelTextPrintLine("    descType=%d", configDesc->descType);
  kernelTextPrintLine("    totalLength=%d", configDesc->totalLength);
  kernelTextPrintLine("    numInterfaces=%d", configDesc->numInterfaces);
  kernelTextPrintLine("    confValue=%d", configDesc->confValue);
  kernelTextPrintLine("    confStringIdx=%d", configDesc->confStringIdx);
  kernelTextPrintLine("    attributes=%d", configDesc->attributes);
  kernelTextPrintLine("    maxPower=%d", configDesc->maxPower);
}


static void debugInterDesc(usbInterDesc *interDesc)
{
  kernelTextPrintLine("USB: debug inter descriptor:");
  kernelTextPrintLine("    descLength=%d", interDesc->descLength);
  kernelTextPrintLine("    descType=%d", interDesc->descType);
  kernelTextPrintLine("    interNum=%d", interDesc->interNum);
  kernelTextPrintLine("    altSetting=%d", interDesc->altSetting);
  kernelTextPrintLine("    numEndpoints=%d", interDesc->numEndpoints);
  kernelTextPrintLine("    interClass=%d", interDesc->interClass);
  kernelTextPrintLine("    interSubClass=%d", interDesc->interSubClass);
  kernelTextPrintLine("    interProtocol=%d", interDesc->interProtocol);
  kernelTextPrintLine("    interStringIdx=%d", interDesc->interStringIdx);
}


static void debugEndpointDesc(usbEndpointDesc *endpointDesc)
{
  kernelTextPrintLine("USB: debug endpoint descriptor:");
  kernelTextPrintLine("    descLength=%d", endpointDesc->descLength);
  kernelTextPrintLine("    descType=%d", endpointDesc->descType);
  kernelTextPrintLine("    endpntAddress=%d", endpointDesc->endpntAddress);
  kernelTextPrintLine("    attributes=%d", endpointDesc->attributes);
  kernelTextPrintLine("    maxPacketSize=%d", endpointDesc->maxPacketSize);
  kernelTextPrintLine("    interval=%d", endpointDesc->interval);
}

#define debugNoNl(message, arg...) kernelTextPrint(message, ##arg)
#define debug(message, arg...) kernelTextPrintLine(message, ##arg)
#define debugError(message, arg...) kernelError(kernel_warn, message, ##arg)

#else
  #define debugDeviceDesc(desc) do { } while (0)
  #define debugDevQualDesc(desc) do { } while (0)
  #define debugConfigDesc(desc) do { } while (0)
  #define debugInterDesc(desc) do { } while (0)
  #define debugEndpointDesc(desc) do { } while (0)
  #define debugNoNl(message, arg...) do { } while (0)
  #define debug(message, arg...) do { } while (0)
  #define debugError(message, arg...) do { } while (0)
#endif


static inline unsigned short readCommand(usbRootHub *usb)
{
  unsigned short command = 0;
  kernelProcessorInPort16((usb->ioAddress + USBUHCI_PORTOFFSET_CMD), command);
  return (command & 0xFF);
}


static inline void writeCommand(usbRootHub *usb, unsigned short command)
{
  kernelProcessorOutPort16((usb->ioAddress + USBUHCI_PORTOFFSET_CMD), command);
}


static inline unsigned short readStatus(usbRootHub *usb)
{
  unsigned short status = 0;
  kernelProcessorInPort16((usb->ioAddress + USBUHCI_PORTOFFSET_STAT), status);
  return (status & 0x3F);
}


static inline void writeStatus(usbRootHub *usb, unsigned short status)
{
  kernelProcessorOutPort16((usb->ioAddress + USBUHCI_PORTOFFSET_STAT),
			   (status & 0x1F));
}


static inline unsigned short readFrameNum(usbRootHub *usb)
{
  unsigned short num = 0;
  kernelProcessorInPort16((usb->ioAddress + USBUHCI_PORTOFFSET_FRNUM), num);
  return (num & 0x7FF);
}


static inline void writeFrameNum(usbRootHub *usb, unsigned short num)
{
  kernelProcessorOutPort16((usb->ioAddress + USBUHCI_PORTOFFSET_FRNUM), num);
}


static void startStop(usbRootHub *usb, int start)
{
  // Start the USB controller

  int status = 0;
  unsigned short command = 0;

  command = readCommand(usb);

  if (start)
    command |= USBUHCI_CMD_RUNSTOP;
  else
    command &= ~USBUHCI_CMD_RUNSTOP;

  writeCommand(usb, command);
  
  if (start)
    {
      // Wait for started
      status = USBUHCI_STAT_HCHALTED;
      while (status & USBUHCI_STAT_HCHALTED)
	status = readStatus(usb);
    }
  else
    {
      // Wait for stopped
      status = 0;
      while (!(status & USBUHCI_STAT_HCHALTED))
	status = readStatus(usb);
    }
}


static void delay(void)
{
  unsigned tm = 0;
  tm = kernelSysTimerRead();
  while (kernelSysTimerRead() < (tm + 2));
}


static int reset(usbRootHub *usb)
{
  // Do complete USB reset

  int status = 0;
  unsigned short command = 0;

  // Set global reset
  command = readCommand(usb);
  command |= USBUHCI_CMD_GRESET;
  writeCommand(usb, command);

  // Delay
  delay();

  // Clear global reset
  command = readCommand(usb);
  command &= ~USBUHCI_CMD_GRESET;
  writeCommand(usb, command);

  return (status = 0);
}


static void portReset(usbRootHub *usb, int num)
{
  unsigned portOffset = 0;
  unsigned short command = 0;

  if (num == 0)
    portOffset = USBUHCI_PORTOFFSET_PORTSC1;
  else if (num == 1)
    portOffset = USBUHCI_PORTOFFSET_PORTSC2;
  else
    // Illegal
    return;

  kernelProcessorInPort16((usb->ioAddress + portOffset), command);
  command |= USBUHCI_PORT_RESET;
  kernelProcessorOutPort16((usb->ioAddress + portOffset), command);
  delay();
  kernelProcessorInPort16((usb->ioAddress + portOffset), command);
  command &= ~USBUHCI_PORT_RESET;
  kernelProcessorOutPort16((usb->ioAddress + portOffset), command);
}


static void portEnable(usbRootHub *usb, int num, int enable)
{
  unsigned portOffset = 0;
  unsigned short command = 0;

  if (num == 0)
    portOffset = USBUHCI_PORTOFFSET_PORTSC1;
  else if (num == 1)
    portOffset = USBUHCI_PORTOFFSET_PORTSC2;
  else
    // Illegal
    return;

  kernelProcessorInPort16((usb->ioAddress + portOffset), command);

  if (enable)
    command |= USBUHCI_PORT_ENABLED;
  else
    command &= ~USBUHCI_PORT_ENABLED;

  kernelProcessorOutPort16((usb->ioAddress + portOffset), command);
}


static void readPortStatus(usbRootHub *usb)
{
  kernelProcessorInPort16((usb->ioAddress + USBUHCI_PORTOFFSET_PORTSC1),
			  usb->portStatus[0]);
  kernelProcessorInPort16((usb->ioAddress + USBUHCI_PORTOFFSET_PORTSC2),
			  usb->portStatus[1]);
  return;
}


static void writePortStatus(usbRootHub *usb, int num)
{
  unsigned portOffset = 0;

  if (num == 0)
    portOffset = USBUHCI_PORTOFFSET_PORTSC1;
  else if (num == 1)
    portOffset = USBUHCI_PORTOFFSET_PORTSC2;
  else
    return;

  kernelProcessorOutPort16((usb->ioAddress + portOffset),
			   usb->portStatus[num]);
  return;
}


static int allocFrameList(usbRootHub *usb)
{
  // Allocate the USB frame list.  1024 32-bit values, so one page of memory,
  // page-aligned.  We need to put the physical address into the register.

  int status = 0;
  usbUhciData *uhciData = (usbUhciData *) usb->data;

  uhciData->frameListPhysical =
    kernelMemoryGetPhysical(MEMORY_PAGE_SIZE, MEMORY_PAGE_SIZE,
			    "usb frame list");
  if ((uhciData->frameListPhysical == NULL) ||
      ((unsigned) uhciData->frameListPhysical & 0x0FFF))
    {
      kernelError(kernel_error, "Unable to get USB frame list memory");
      return (status = ERR_MEMORY);
    }

  // Map it to a virtual address
  status =
    kernelPageMapToFree(KERNELPROCID, uhciData->frameListPhysical,
			(void **) &(uhciData->frameList), MEMORY_PAGE_SIZE);
  if (status < 0)
    {
      kernelMemoryReleasePhysical(uhciData->frameListPhysical);
      kernelError(kernel_error, "Unable to map USB frame list memory");
      return (status);
    }

  // Fill the list with 32-bit '1' values, indicating that all pointers
  // are currently invalid
  kernelProcessorWriteDwords(1, uhciData->frameList,
			     (MEMORY_PAGE_SIZE / sizeof(unsigned)));

  return (status = 0);
}


static int allocTransDescs(usbRootHub *usb)
{
  // Allocate an array of USB transfer descriptors.  One page worth,
  // page-aligned.

  int status = 0;
  usbUhciData *uhciData = (usbUhciData *) usb->data;

  uhciData->transDescsPhysical =
    kernelMemoryGetPhysical(MEMORY_PAGE_SIZE, MEMORY_PAGE_SIZE,
			    "usb xfer descriptors");
  if ((uhciData->transDescsPhysical == NULL) ||
      ((unsigned) uhciData->transDescsPhysical & 0x0F))
    {
      kernelError(kernel_error, "Unable to get USB transfer descriptor "
		  "memory");
      return (status = ERR_MEMORY);
    }

  // Map it to a virtual address
  status =
    kernelPageMapToFree(KERNELPROCID, uhciData->transDescsPhysical,
			(void **) &(uhciData->transDescs), MEMORY_PAGE_SIZE);
  if (status < 0)
    {
      kernelMemoryReleasePhysical(uhciData->transDescsPhysical);
      kernelError(kernel_error, "Unable to map USB transfer descriptor "
		  "memory");
      return (status);
    }

  // Clear the memory, since the 'allocate physical' can't do it for us.
  kernelMemClear((void *) uhciData->transDescs, MEMORY_PAGE_SIZE);

  uhciData->numTransDescs = (MEMORY_PAGE_SIZE / sizeof(usbUhciTransDesc));

  return (status = 0);
}


static int allocQueueHeads(usbRootHub *usb)
{
  // Allocate an array of USB queue heads.  One page worth, page-aligned.

  int status = 0;
  usbUhciData *uhciData = (usbUhciData *) usb->data;
  int count;

  uhciData->queueHeadsPhysical =
    kernelMemoryGetPhysical(MEMORY_PAGE_SIZE, MEMORY_PAGE_SIZE,
			    "usb queue heads");
  if ((uhciData->queueHeadsPhysical == NULL) ||
      ((unsigned) uhciData->queueHeadsPhysical & 0x0F))
    {
      kernelError(kernel_error, "Unable to get USB queue heads memory");
      return (status = ERR_MEMORY);
    }

  // Map it to a virtual address
  status =
    kernelPageMapToFree(KERNELPROCID, uhciData->queueHeadsPhysical,
			(void **) &(uhciData->queueHeads), MEMORY_PAGE_SIZE);
  if (status < 0)
    {
      kernelMemoryReleasePhysical(uhciData->queueHeadsPhysical);
      kernelError(kernel_error, "Unable to map USB queue heads memory");
      return (status);
    }

  uhciData->numQueueHeads = (MEMORY_PAGE_SIZE / sizeof(usbUhciQueueHead));

  // Set all the link pointers invalid
  for (count = 0; count < uhciData->numQueueHeads; count ++)
    {
      uhciData->queueHeads[count].linkPointer = 1;
      uhciData->queueHeads[count].element = 1;
    }

  return (status = 0);
}


static usbUhciTransDesc *getTransDesc(usbRootHub *usb, unsigned buffSize)
{
  // Returns the virtual address of a new USB transfer descriptor, with the
  // requested buffer space allocated.
  
  usbUhciData *uhciData = (usbUhciData *) usb->data;
  usbUhciTransDesc *desc = NULL;
  int count;

  // Loop through the transfer descriptors, looking for one with a NULL
  // buffer
  for (count = 0; count < uhciData->numTransDescs; count ++)
    if (uhciData->transDescs[count].buffVirtAddr == NULL)
      {
	desc = &(uhciData->transDescs[count]);
	break;
      }

  if (!desc)
    return (desc = NULL);

  if (buffSize)
    {
      desc->buffVirtAddr = kernelMalloc(buffSize);
      if (desc->buffVirtAddr == NULL)
	return (desc = NULL);

      // Get the physical address of this memory
      desc->buffer = kernelPageGetPhysical(KERNELPROCID, desc->buffVirtAddr);
      if (desc->buffer == NULL)
	{
	  kernelFree(desc->buffVirtAddr);
	  return (desc = NULL);
	}

      desc->buffSize = buffSize;
    }

  return (desc);
}


static void putTransDesc(usbUhciTransDesc *desc, int deallocBuff)
{
  // Release a transfer descriptor.

  if (deallocBuff)
    {
      // First release the buffer memory
      if (desc->buffVirtAddr)
	kernelFree(desc->buffVirtAddr);
    }

  // Clear it
  kernelMemClear((void *) desc, sizeof(usbUhciTransDesc));
}


static usbUhciQueueHead *getQueueHead(usbRootHub *usb)
{
  // Returns the virtual address of a new USB queue head.
  
  usbUhciData *uhciData = (usbUhciData *) usb->data;
  usbUhciQueueHead *queueHead = NULL;
  int count;

  // Loop through the queue heads, looking for one with an invalid element
  // link pointer
  for (count = 0; count < uhciData->numQueueHeads; count ++)
    if (uhciData->queueHeads[count].element & 1)
      {
	queueHead = &(uhciData->queueHeads[count]);
	break;
      }

  return (queueHead);
}


static inline void putQueueHead(usbUhciQueueHead *queueHead)
{
  // Release a queue head.

  // Set the link pointers to be invalid
  queueHead->linkPointer = 1;
  queueHead->element = 1;
}


static int scheduleTransDesc(usbRootHub *usb, usbUhciTransDesc *desc,
			     usbxferType type, int address, int endpoint,
			     unsigned char pid, unsigned *bytes)
{
  // Schedule a transfer descriptor of arbitrary type and with arbitrary
  // data.

  int status = 0;
  unsigned descPhysical = NULL;
  usbUhciQueueHead *queueHead = NULL;
  unsigned queueHeadPhysical = NULL;
  usbUhciData *uhciData = (usbUhciData *) usb->data;
  unsigned short currFrameNum = 0;
  unsigned short currFrameIndex = 0;
  unsigned startTime = 0;
  int timeOut = 0;

  // Get the physical address of the TD
  descPhysical = (unsigned) kernelPageGetPhysical(KERNELPROCID, (void *) desc);
  if (descPhysical == NULL)
    return (status = ERR_MEMORY);
  if (descPhysical & 0xF)
    {
      kernelError(kernel_error, "Xfer descriptor not 16-byte aligned");
      return (status = ERR_ALIGN);
    }

  // Do we need a queue head?
  if (type != usbxfer_isochronous)
    {
      queueHead = getQueueHead(usb);
      if (queueHead == NULL)
	return (status = ERR_NOFREE);

      // Get the physical address of the queue head
      queueHeadPhysical =
	(unsigned) kernelPageGetPhysical(KERNELPROCID, (void *) queueHead);
      if (queueHeadPhysical == NULL)
	{
	  putQueueHead(queueHead);
	  return (status = ERR_MEMORY);
	}
      if (queueHeadPhysical & 0xF)
	{
	  kernelError(kernel_error, "Queue head not 16-byte aligned");
	  putQueueHead(queueHead);
	  return (status = ERR_ALIGN);
	}

      // Blank the queue head's link pointer and set the 'terminate' bit
      queueHead->linkPointer = 1;
      queueHead->element = descPhysical;
    }

  // Set up the TD

  // Blank the descriptor's link pointer and set the 'vertical' and
  // 'terminate' bits
  desc->linkPointer = 5;

  // Initialize the 'control and status' field.  3 errors, no interrupt on
  // complete, active
  desc->contStatus = 0x18800000;
  if (type == usbxfer_isochronous)
    desc->contStatus |= 0x02000000;

  // Set up the TD token field
  if (desc->buffSize)
    desc->tdToken = (((desc->buffSize - 1) & 0x07FF) << 21);
  else
    desc->tdToken = (0x07FF << 21);
  desc->tdToken |= ((endpoint & 0xF) << 15);
  desc->tdToken |= ((address & 0x7F) << 8);
  desc->tdToken |= (pid & 0xFF);

  // Now we look for a place in the schedule.

  // Get the current frame counter value.  Start looking from the next one.
  currFrameNum = readFrameNum(usb);

  // Start looking in the next one
  currFrameNum += 1;

  while (1)
    {
      if (currFrameNum > 0x7FF)
	currFrameNum = 0;

      currFrameIndex = (currFrameNum & 0x3FF);

      // Look for an 'invalid' one
      if (uhciData->frameList[currFrameIndex] == 1)
	{
	  // Schedule it here.

	  if (queueHead)
	    uhciData->frameList[currFrameIndex] = (queueHeadPhysical | 0x02);
	  else
	    uhciData->frameList[currFrameIndex] = descPhysical;

	  //debug("USB: schedule TD in frame %d%s (current %d)", currFrameNum,
	  //(queueHead? " with queue head" : ""), readFrameNum(usb));
	  break;
	}

      currFrameNum += 1;
    }

  startTime = kernelSysTimerRead();

  // Now wait while the TD is active
  while (desc->contStatus & 0x00800000)
    {
      // Timeout?
      if (kernelSysTimerRead() > (startTime + 100))
	{
	  timeOut = 1;
	  break;
	}

      // Skipped our frame?  If so, it takes way too long for it to come back
      // around in the queue, so we reset the frame number to where we want
      // it.
      if (readFrameNum(usb) > currFrameNum)
	{
	  startStop(usb, 0);
	  writeFrameNum(usb, currFrameNum);
	  startStop(usb, 1);
	}

      kernelMultitaskerYield();
    }

  if (bytes)
    {
      // Record the number of actual bytes
      if ((desc->contStatus & 0x7FF) == 0x7FF)
	*bytes = 0;
      else
	*bytes = ((desc->contStatus & 0x3FF) + 1);
    }

  // Error conditions?
  if (timeOut || (desc->contStatus & 0x007E0000))
    {
      if (desc->contStatus & 0x00400000)
	debugError("USB: transaction stalled");
      if (desc->contStatus & 0x00200000)
	debugError("USB: transaction data buffer error");
      if (desc->contStatus & 0x00100000)
	debugError("USB: transaction babble");
      if (desc->contStatus & 0x00080000)
	debugError("USB: transaction NAK");
      if (desc->contStatus & 0x00040000)
	debugError("USB: transaction CRC/timeout");
      if (desc->contStatus & 0x00020000)
	debugError("USB: transaction bitstuff error");
      if (bytes && ((((desc->tdToken >> 21) & 0x07FF) + 1) != *bytes))
	debugError("USB: transaction max=%d != actual=%d)",
		   (((desc->tdToken >> 21) & 0x07FF) + 1), *bytes);
      if (desc->contStatus & 0x00800000)
	debugError("USB: TD is still active");
      if (timeOut)
	debugError("USB: Request timed out");
      status = ERR_IO;
    }
  else
    status = 0;

  // Unschedule
  uhciData->frameList[currFrameIndex] = 1;

  if (queueHead)
    putQueueHead(queueHead);

  return (status);
}


static usbEndpointDesc *getEndpointDesc(usbDevice *dev,
					unsigned char endpntAddress)
{
  // Try to find the endpoint descriptor with the given address

  usbEndpointDesc *endpoint = NULL;
  int count;

  for (count = 0; count < dev->interDesc[0]->numEndpoints; count ++)
    if ((dev->endpointDesc[count]->endpntAddress & 0xF) == endpntAddress)
      {
	endpoint = dev->endpointDesc[count];
	break;
      }

  return (endpoint);
}


static int transaction(void *controller, usbDevice *dev, usbTransaction *trans)
{
  // This function contains the intelligence necessary to initiate a
  // transaction (all phases)

  int status = 0;
  usbRootHub *usb = (usbRootHub *) controller;
  usbUhciTransDesc *setupDesc = NULL;
  usbUhciTransDesc *dataDesc = NULL;
  usbUhciTransDesc *statusDesc = NULL;
  usbDeviceRequest *req = NULL;
  int setupPhase = 0;
  int setupData = 0;
  int statusPhase = 0;
  unsigned short bytesToTransfer = 0;
  unsigned short bytesPerTransfer = 0;
  usbEndpointDesc *endpoint = NULL;
  void *buffer = NULL;

  if (trans->type == usbxfer_control)
    {
      // Get the transfer descriptor for the setup phase
      setupDesc = getTransDesc(usb, sizeof(usbDeviceRequest));
      if (setupDesc == NULL)
	return (status = ERR_NOFREE);
      setupPhase = 1;
      setupData = 1;
      statusPhase = 1;

      // By default it's an OUT transaction
      trans->pid = USB_PID_OUT;

      // Begin setting up the device request
      req = setupDesc->buffVirtAddr;

      req->requestType = trans->control.requestType;

      // Does the request go to an endpoint?
      if (trans->endpoint)
	{
	  req->requestType |= USB_DEVREQTYPE_ENDPOINT;
	  trans->endpoint = 0;
	}

      req->request = trans->control.request;
      req->value = trans->control.value;
      req->index = trans->control.index;
      req->length = trans->length;

      // What request are we doing?  Determine the correct requestType and
      // whether there will be a data phase, etc.
      switch (trans->control.request)
	{
	case USB_GET_STATUS:
	  debugNoNl("USB: do USB_GET_STATUS");
	  req->requestType |= USB_DEVREQTYPE_DEV2HOST;
	  trans->pid = USB_PID_IN;
	  break;
	case USB_CLEAR_FEATURE:
	  debugNoNl("USB: do USB_CLEAR_FEATURE");
	  req->requestType |= USB_DEVREQTYPE_HOST2DEV;
	  break;
	case USB_SET_FEATURE:
	  debugNoNl("USB: do USB_SET_FEATURE");
	  req->requestType |= USB_DEVREQTYPE_HOST2DEV;
	  break;
	case USB_SET_ADDRESS:
	  debugNoNl("USB: do USB_SET_ADDRESS");
	  req->requestType |= USB_DEVREQTYPE_HOST2DEV;
	  break;
	case USB_GET_DESCRIPTOR:
	  debugNoNl("USB: do USB_GET_DESCRIPTOR");
	  req->requestType |= USB_DEVREQTYPE_DEV2HOST;
	  trans->pid = USB_PID_IN;
	  break;
	case USB_SET_DESCRIPTOR:
	  debugNoNl("USB: do USB_SET_DESCRIPTOR");
	  req->requestType |= USB_DEVREQTYPE_HOST2DEV;
	  break;
	case USB_GET_CONFIGURATION:
	  debugNoNl("USB: do USB_GET_CONFIGURATION");
	  req->requestType |= USB_DEVREQTYPE_DEV2HOST;
	  trans->pid = USB_PID_IN;
	  break;
	case USB_SET_CONFIGURATION:
	  debugNoNl("USB: do USB_SET_CONFIGURATION");
	  req->requestType |= USB_DEVREQTYPE_HOST2DEV;
	  break;
	case USB_GET_INTERFACE:
	  debugNoNl("USB: do USB_GET_INTERFACE");
	  req->requestType |= USB_DEVREQTYPE_DEV2HOST;
	  trans->pid = USB_PID_IN;
	  break;
	case USB_SET_INTERFACE:
	  debugNoNl("USB: do USB_SET_INTERFACE");
	  req->requestType |= USB_DEVREQTYPE_HOST2DEV;
	  break;
	case USB_SYNCH_FRAME:
	  debugNoNl("USB: do USB_SYNCH_FRAME");
	  req->requestType |= USB_DEVREQTYPE_DEV2HOST;
	  trans->pid = USB_PID_IN;
	  break;
	case USB_MASSSTORAGE_RESET:
	  debugNoNl("USB: do USB_MASSSTORAGE_RESET");
	  req->requestType |= USB_DEVREQTYPE_HOST2DEV;
	  break;
	default:
	  // Perhaps a class-specific thing we don't know about.  Try to
	  // proceed anyway.
	  debugNoNl("USB: do unknown control transfer");
	  break;
	}
      debug(" for address %d", trans->address);
    }

  else if (trans->type == usbxfer_bulk)
    ;

  else
    {
      kernelError(kernel_error, "Unsupported transaction type %d",
		  trans->type);
      putTransDesc(setupDesc, 1);
      return (status = ERR_NOTIMPLEMENTED);
    }

  if (setupPhase)
    {
      // Schedule the setup packet
      //debug("USB: schedule setup transfer");
      status = scheduleTransDesc(usb, setupDesc, trans->type, trans->address,
				 trans->endpoint, USB_PID_SETUP, NULL);
      putTransDesc(setupDesc, setupData);
      if (status < 0)
	return (status);
    }

  // If there is a data phase, schedule the data packet(s)
  if (trans->length)
    {
      bytesToTransfer = trans->length;
      buffer = trans->buffer;

      // Figure out the maximum number of bytes per transfer, depending on
      // the endpoint we're addressing.
      if (trans->endpoint == 0)
	bytesPerTransfer = dev->deviceDesc.maxPacketSize0;
      else
	{
	  endpoint = getEndpointDesc(dev, trans->endpoint);
	  if (endpoint == NULL)
	    {
	      kernelError(kernel_error, "No such endpoint %d",
			  trans->endpoint);
	      return (status = ERR_NOSUCHFUNCTION);
	    }

	  bytesPerTransfer = endpoint->maxPacketSize;
	}

      // If we haven't got the descriptors, etc., yet -- 8 is the minimum size
      if (bytesPerTransfer < 8)
	bytesPerTransfer = 8;

      //debug("Max bytes per transfer: %d", bytesPerTransfer);

      while (bytesToTransfer)
	{
	  unsigned short doBytes = min(bytesToTransfer, bytesPerTransfer);
	  unsigned bytes = 0;

	  // Get the transfer descriptors for the data phase
	  dataDesc = getTransDesc(usb, doBytes);
	  if (dataDesc == NULL)
	    return (status = ERR_NOFREE);

	  if (trans->pid == USB_PID_OUT)
	    // Copy the data into the descriptor's buffer
	    kernelMemCopy(buffer, dataDesc->buffVirtAddr, doBytes);

	  debug("USB: schedule data transfer %s of %d bytes %s address %d:%d",
		((trans->pid == USB_PID_IN)? "IN" : "OUT"),
		doBytes, ((trans->pid == USB_PID_IN)? "from" : "to"),
		trans->address, trans->endpoint);
	  status = scheduleTransDesc(usb, dataDesc, trans->type,
				     trans->address, trans->endpoint,
				     trans->pid, &bytes);

	  if (trans->pid == USB_PID_IN)
	    // Copy the data out of the descriptor's buffer
	    kernelMemCopy(dataDesc->buffVirtAddr, buffer, doBytes);

	  trans->bytes += bytes;

	  putTransDesc(dataDesc, 1);

	  if (status < 0)
	    return (status);

	  bytesToTransfer -= doBytes;
	  buffer += doBytes;
	}
    }

  if (statusPhase)
    {
      // Get the transfer descriptor for the status phase
      statusDesc = getTransDesc(usb, 0);
      if (statusDesc == NULL)
	return (status = ERR_NOFREE);

      // Schedule the status packet
      //debug("USB: schedule status transfer");
      if (trans->pid == USB_PID_OUT)
	status =
	  scheduleTransDesc(usb, statusDesc, trans->type, trans->address,
			    trans->endpoint, USB_PID_IN, NULL);
      else
	status =
	  scheduleTransDesc(usb, statusDesc, trans->type, trans->address,
			    trans->endpoint, USB_PID_OUT, NULL);

      putTransDesc(statusDesc, 0);
    }

  return (status);
}


static int controlTransfer(usbRootHub *usb, usbDevice *dev,
			   unsigned char endpoint, unsigned char request,
			   unsigned short value, unsigned short index,
			   unsigned short length, void *buffer,
			   unsigned *bytes)
{
  // This is a wrapper for doTransaction() so callers don't have to construct
  // a usbTransaction structure

  int status = 0;
  usbTransaction trans;

  debug("USB: control transfer of %d bytes", length);

  kernelMemClear(&trans, sizeof(usbTransaction));
  trans.type = usbxfer_control;
  trans.address = dev->address;
  trans.endpoint = endpoint;
  trans.control.request = request;
  trans.control.value = value;
  trans.control.index = index;
  trans.length = length;
  trans.buffer = buffer;

  status = transaction(usb, dev, &trans);

  if (bytes)
    *bytes = trans.bytes;

  return (status);
}


static int enumerateNewDevice(usbRootHub *usb)
{
  // Enumerate a new device in respose to a positive port connection status
  // change by sending out a 'set address' transaction and getting device
  // information

  int status = 0;
  usbDevice *dev = NULL;
  unsigned bytes = 0;
  usbConfigDesc *tmpConfigDesc = NULL;
  char *className = NULL;
  char *subClassName = NULL;
  usbClass *class = NULL;
  usbSubClass *subClass = NULL;
  void *ptr = NULL;
  int count1, count2;

  dev = kernelMalloc(sizeof(usbDevice));
  if (dev == NULL)
    return (status = ERR_MEMORY);
  
  // Try to set a device address
  status = controlTransfer(usb, dev, 0, USB_SET_ADDRESS,
			   (usb->addressCounter + 1), 0, 0, NULL, NULL);
  if (status < 0)
    {
      // No device waiting for an address, we guess
      kernelFree(dev);
      return (status = ERR_NOSUCHENTRY);
    }
  
  // We're supposed to allow a delay for the device after the set address
  // command.
  delay();
  
  dev->controller = usb->controller;
  dev->address = ++usb->addressCounter;

  // We've set an address for the device, so from here it "exists", as such,
  // whether or not the rest of the configuration succeeds

  // Try getting a device descriptor of only 8 bytes.  Thereafter we will
  // *know* the supported packet size.
  status = controlTransfer(usb, dev, 0, USB_GET_DESCRIPTOR,
			   (USB_DESCTYPE_DEVICE << 8), 0, 8,
			   &(dev->deviceDesc), NULL);
  if (status < 0)
    {
      kernelError(kernel_error, "Error getting device descriptor");
      kernelFree(dev);
      return (status);
    }

  // Now Get the whole descriptor
  status = controlTransfer(usb, dev, 0, USB_GET_DESCRIPTOR,
			   (USB_DESCTYPE_DEVICE << 8), 0,
			   sizeof(usbDeviceDesc), &(dev->deviceDesc), NULL);
  if (status < 0)
    {
      kernelError(kernel_error, "Error getting device descriptor");
      kernelFree(dev);
      return (status);
    }

  debugDeviceDesc(&(dev->deviceDesc));

  // If the device is a USB 2.0+ device, see if it's got a 'device qualifier'
  // descriptor
  if (dev->deviceDesc.usbVersion >= 0x0200)
    {
      controlTransfer(usb, dev, 0, USB_GET_DESCRIPTOR,
		      (USB_DESCTYPE_DEVICEQUAL << 8), 0,
		      sizeof(usbDevQualDesc), &(dev->devQualDesc), &bytes);
      
      if (bytes)
	debugDevQualDesc(&(dev->devQualDesc));
    }

  dev->usbVersion = dev->deviceDesc.usbVersion;
  dev->classCode = dev->deviceDesc.deviceClass;
  dev->subClassCode = dev->deviceDesc.deviceSubClass;
  dev->protocol = dev->deviceDesc.deviceProtocol;
  dev->deviceId = dev->deviceDesc.productId;
  dev->vendorId = dev->deviceDesc.vendorId;

  // Get the first configuration, which includes interface and endpoint
  // descriptors.  The initial attempt must be limited to the max packet
  // size for endpoint zero.

  tmpConfigDesc = kernelMalloc(dev->deviceDesc.maxPacketSize0);
  if (tmpConfigDesc == NULL)
    {
      kernelFree(dev);
      return (status = ERR_MEMORY);
    }

  status = controlTransfer(usb, dev, 0, USB_GET_DESCRIPTOR,
			   (USB_DESCTYPE_CONFIG << 8), 0,
			   dev->deviceDesc.maxPacketSize0, tmpConfigDesc,
			   &bytes);
  if ((status < 0) &&
      (bytes < min(dev->deviceDesc.maxPacketSize0, sizeof(usbConfigDesc))))
    {
      kernelFree(tmpConfigDesc);
      kernelFree(dev);
      return (status);
    }

  debugConfigDesc(tmpConfigDesc);

  // If the device wants to return more information than will fit in the
  // max packet size for endpoint zero, we need to do a second request that
  // splits the data transfer into parts

  if (tmpConfigDesc->totalLength > dev->deviceDesc.maxPacketSize0)
    {
      dev->configDesc = kernelMalloc(tmpConfigDesc->totalLength);
      if (dev->configDesc == NULL)
	{
	  kernelFree(tmpConfigDesc);
	  kernelFree(dev);
	  return (status = ERR_MEMORY);
	}

      status = controlTransfer(usb, dev, 0, USB_GET_DESCRIPTOR,
			       (USB_DESCTYPE_CONFIG << 8), 0,
			       tmpConfigDesc->totalLength, dev->configDesc,
			       &bytes);
      if ((status < 0) && (bytes < tmpConfigDesc->totalLength))
	{
	  kernelFree(tmpConfigDesc);
	  kernelFree(dev);
	  return (status);
	}

      kernelFree(tmpConfigDesc);
    }
  else
    dev->configDesc = tmpConfigDesc;

  debugConfigDesc(dev->configDesc);

  // Set the configuration
  status = controlTransfer(usb, dev, 0, USB_SET_CONFIGURATION,
			   dev->configDesc->confValue, 0, 0, NULL, NULL);
  if (status < 0)
    {
      kernelFree(dev->configDesc);
      kernelFree(dev);
      return (status);
    }

  ptr = ((void *) dev->configDesc + sizeof(usbConfigDesc));
  for (count1 = 0; ((count1 < dev->configDesc->numInterfaces) &&
		    (count1 < USB_MAX_INTERFACES)); count1 ++)
    {
      if (ptr >= ((void *) dev->configDesc + dev->configDesc->totalLength))
	break;

      dev->interDesc[count1] = ptr;

      if (!dev->classCode)
	{
	  dev->classCode = dev->interDesc[count1]->interClass;
	  dev->subClassCode = dev->interDesc[count1]->interSubClass;
	  dev->protocol = dev->interDesc[count1]->interProtocol;
	}

      debugInterDesc(dev->interDesc[count1]);

      ptr += sizeof(usbInterDesc);

      for (count2 = 0; ((count2 < dev->interDesc[count1]->numEndpoints) &&
			(count2 < USB_MAX_ENDPOINTS)); count2 ++)
	{
	  if (ptr >= ((void *) dev->configDesc + dev->configDesc->totalLength))
	    break;

	  dev->endpointDesc[count2] = ptr;

	  debugEndpointDesc(dev->endpointDesc[count2]);

	  ptr += sizeof(usbEndpointDesc);
	}
    }

  // Ok, we will add this device to our list
  usb->devices[usb->numDevices++] = dev;

  if (usb->getClassName)
    usb->getClassName(dev->classCode, dev->subClassCode, dev->protocol,
		      &className, &subClassName);

  kernelLog("USB: %s %s %u:%u dev:%04x, vend:%04x, class:%02x, "
	    "sub:%02x proto:%02x usb:%x.%x", subClassName, className,
	    dev->controller, dev->address, dev->deviceId, dev->vendorId,
	    dev->classCode, dev->subClassCode, dev->protocol,
	    ((dev->usbVersion & 0xFF00) >> 8), (dev->usbVersion & 0xFF));

  // If we've already done device enumeration, then see about calling the
  // appropriate hotplug detection functions of the appropriate drivers
  if (usb->didEnum)
    {
      if (usb->getClass)
	{
	  usb->getClass(dev->classCode, &class);
	  if (usb->getSubClass)
	    usb->getSubClass(class, dev->subClassCode, dev->protocol,
			     &subClass);

	  if (subClass)
	    {
	      status =
		kernelDeviceHotplug(usb->device, subClass->systemSubClassCode,
				    bus_usb,
				    usbMakeTargetCode(dev->controller,
						      dev->address, 0), 1);
	      if (status < 0)
		return (status);
	    }
	}
    }

  return (status = 0);
}


static void enumerateDisconnectedDevice(usbRootHub *usb)
{
  // If the port status(es) indicate that a device has disconnected, figure
  // out which one it is and remove it from the root hub's list

  int status = 0;
  usbDevice *dev = NULL;
  usbDeviceDesc deviceDesc;
  usbClass *class = NULL;
  usbSubClass *subClass = NULL;
  int count1, count2;

  // Try a 'get device descriptor' command for each device
  for (count1 = 0; count1 < usb->numDevices; )
    {
      dev = usb->devices[count1];

      debug("Try device %d", dev->address);
      status = controlTransfer(usb, dev, 0, USB_GET_DESCRIPTOR,
			       (USB_DESCTYPE_DEVICE << 8), 0, 8,
			       &deviceDesc, NULL);
      if (status < 0)
	{
	  // The 'get status' failed so we will consider this device
	  // disconnected

	  debug("Device %d disconnected", dev->address);

	  if (usb->getClass)
	    {
	      usb->getClass(dev->classCode, &class);
	      if (usb->getSubClass)
		usb->getSubClass(class, dev->subClassCode, dev->protocol,
				 &subClass);
	
	      if (subClass)
		// Tell the device hotplug function that the device has
		// disconnected
		kernelDeviceHotplug(usb->device, subClass->systemSubClassCode,
				    bus_usb, usbMakeTargetCode(dev->controller,
							       dev->address,
							       0), 0);
	    }

	  // If this was not the only or last device in the list, we need
	  // to shift the list
	  if ((usb->numDevices > 1) && (count1 < (usb->numDevices - 1)))
	    for (count2 = count1; count2 < (usb->numDevices - 1); count2 ++)
	      usb->devices[count2] = usb->devices[count2 + 1];
	  usb->numDevices -= 1;

	  // Free the device memory
	  if (dev->configDesc)
	    kernelFree(dev->configDesc);
	  kernelFree(dev);

	  continue;
	}

      count1++;
    }
}


static int setup(usbRootHub *usb)
{
  // Do USB setup

  int status = 0;
  unsigned short command = 0;
  usbUhciData *uhciData = NULL;

  // Enable all interrupts
  kernelProcessorInPort16((usb->ioAddress + USBUHCI_PORTOFFSET_INTR), command);
  command |= 0xF;
  kernelProcessorOutPort16((usb->ioAddress + USBUHCI_PORTOFFSET_INTR),
			   command);

  // Clear the frame number register
  kernelProcessorInPort16((usb->ioAddress + USBUHCI_PORTOFFSET_FRNUM),
			  command);
  command &= 0xF800;
  kernelProcessorOutPort16((usb->ioAddress + USBUHCI_PORTOFFSET_FRNUM),
			   command);

  // Allocate the UHCI hub's private data
  uhciData = kernelMalloc(sizeof(usbUhciData));
  if (uhciData == NULL)
    return (status = ERR_MEMORY);
  usb->data = uhciData;

  // Allocate the controller's frame list
  status = allocFrameList(usb);
  if (status < 0)
    goto err_out;

  // Allocate the controller's transfer descriptors
  status = allocTransDescs(usb);
  if (status < 0)
    goto err_out;

  // Allocate the controller's queue heads
  status = allocQueueHeads(usb);
  if (status < 0)
    goto err_out;

  // Put the physical address of the frame list into the frame list base
  // address register
  kernelProcessorOutPort32((usb->ioAddress + USBUHCI_PORTOFFSET_FLBASE),
			   uhciData->frameListPhysical);

  // Clear:
  // software debug
  // Set:
  // max packet size to 64 bytes
  // configure flag
  command = readCommand(usb);
  command &= ~USBUHCI_CMD_SWDBG;
  command |= (USBUHCI_CMD_MAXP | USBUHCI_CMD_CF);
  writeCommand(usb, command);

  // Start the controller
  startStop(usb, 1);

  return (status = 0);

 err_out:
  if (uhciData)
    {
      if (uhciData->frameList)
	{
	  kernelPageUnmap(KERNELPROCID, uhciData->frameList, MEMORY_PAGE_SIZE);
	  kernelMemoryReleasePhysical(uhciData->frameListPhysical);
	}
      if (uhciData->transDescs)
	{
	  kernelPageUnmap(KERNELPROCID, (void *) uhciData->transDescs,
			  MEMORY_PAGE_SIZE);
	  kernelMemoryReleasePhysical(uhciData->transDescsPhysical);
	}
      if (uhciData->queueHeads)
	{
	  kernelPageUnmap(KERNELPROCID, (void *) uhciData->queueHeads,
			  MEMORY_PAGE_SIZE);
	  kernelMemoryReleasePhysical(uhciData->queueHeadsPhysical);
	}
      kernelFree(uhciData);
    }
  return (status);
}


#ifdef DEBUG
static void printPortStatus(usbRootHub *usb)
{
  unsigned short frameNum = 0;
  usbUhciData *uhciData = (usbUhciData *) usb->data;

  readPortStatus(usb);
  frameNum = readFrameNum(usb);
  debug("USB: port 1: %04x  port 2: %04x frnum %d=%x", usb->portStatus[0],
	usb->portStatus[1], (frameNum & 0x3FF),
	uhciData->frameList[frameNum & 0x3FF]);
}
#else
  #define printPortStatus(usb) do { } while (0)
#endif


static void threadCall(void *controller)
{
  unsigned short status = 0;
  usbRootHub *usb = (usbRootHub *) controller;
  int count;

  readPortStatus(usb);

  status = readStatus(usb);
  if (status)
    // Clear it
    writeStatus(usb, status);

  for (count = 0; count < 2; count ++)
    {
      if ((usb->portStatus[count] & USBUHCI_PORT_ENABCHG) ||
	  (usb->portStatus[count] & USBUHCI_PORT_CONNCHG))
	{
	  printPortStatus(usb);

	  if (usb->portStatus[count] & USBUHCI_PORT_CONNCHG)
	    {
	      if (usb->portStatus[count] & USBUHCI_PORT_CONNSTAT)
		{
		  // Something connected, so enable the port.  This resets
		  // the devices.
		  portEnable(usb, count, 1);
		  portReset(usb, count);

		  while (enumerateNewDevice(usb) != ERR_NOSUCHENTRY);
		}
	      else
		{
		  //debug("USB: port %d is disconnected", count);
		  enumerateDisconnectedDevice(usb);
		}
	    }
		    
	  // Write the status back to reset the 'changed' bits.
	  readPortStatus(usb);
	  writePortStatus(usb, count);
	  printPortStatus(usb);
	}
    }
}


kernelDevice *kernelUsbUhciDetect(kernelDevice *parent,
				  kernelBusTarget *busTarget, void *driver)
{
  // This routine is used to detect and initialize each device, as well as
  // registering each one with any higher-level interfaces.

  int status = 0;
  pciDeviceInfo pciDevInfo;
  kernelDevice *dev = NULL;
  usbRootHub *usb = NULL;

  // Get the PCI device header
  status = kernelBusGetTargetInfo(bus_pci, busTarget->target, &pciDevInfo);
  if (status < 0)
    return (dev = NULL);

  // Make sure it's a non-bridge header
  if (pciDevInfo.device.headerType != PCI_HEADERTYPE_NORMAL)
    return (dev = NULL);

  // Make sure it's a UHCI controller (programming interface is 0 in the
  // PCI header)
  if (pciDevInfo.device.progIF != 0)
    return (dev = NULL);

  // After this point, we believe we have a supported device.

  // Enable the device on the PCI bus as a bus msater
  if ((kernelBusDeviceEnable(bus_pci, busTarget->target, 1) < 0) ||
      (kernelBusSetMaster(bus_pci, busTarget->target, 1) < 0))
    return (dev = NULL);

  // Allocate memory for the device
  dev = kernelMalloc(sizeof(kernelDevice));
  if (dev == NULL)
    return (dev);

  usb = kernelMalloc(sizeof(usbRootHub));
  if (usb == NULL)
    {
      kernelFree(dev);
      return (dev = NULL);
    }

  // Get the USB version number
  usb->usbVersion = kernelBusReadRegister(bus_pci, busTarget->target, 0x60, 8);
  kernelLog("USB: UHCI bus version %d.%d", ((usb->usbVersion & 0xF0) >> 4),
	    (usb->usbVersion & 0xF));

  // Get the I/O space base address.  For USB, it comes in the 5th
  // PCI base address register
  usb->ioAddress = (void *)
    (kernelBusReadRegister(bus_pci, busTarget->target, 0x08, 32) & 0xFFFFFFE0);

  if (usb->ioAddress == NULL)
    {
      kernelError(kernel_error, "Unknown USB controller I/O address");
      kernelFree(dev);
      kernelFree(usb);
      return (dev = NULL);
    }

  // Get the interrupt line
  usb->interrupt = (int) pciDevInfo.device.nonBridge.interruptLine;

  // Stop the controller
  startStop(usb, 0);

  // Reset it
  status = reset(usb);
  if (status < 0)
    {
      kernelError(kernel_error, "Error resetting USB controller");
      kernelFree(dev);
      kernelFree(usb);
      return (dev = NULL);
    }

  // Set up the controller
  status = setup(usb);
  if (status < 0)
    {
      kernelError(kernel_error, "Error setting up USB operation");
      kernelFree(dev);
      kernelFree(usb);
      return (dev = NULL);
    }

  usb->threadCall = &threadCall;
  usb->transaction = &transaction;

  // Create the USB kernel device
  dev->device.class = kernelDeviceGetClass(DEVICECLASS_BUS);
  dev->device.subClass = kernelDeviceGetClass(DEVICESUBCLASS_BUS_USB);
  dev->driver = driver;
  dev->data = usb;

  status = kernelDeviceAdd(parent, dev);
  if (status < 0)
    {
      kernelFree(dev);
      kernelFree(usb);
      return (dev = NULL);
    }

  return (dev);
}
