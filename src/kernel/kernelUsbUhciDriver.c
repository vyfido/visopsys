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
#include "kernelDebug.h"
#include "kernelError.h"
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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef DEBUG

static inline void debugUhciRegs(usbRootHub *usb)
{
  unsigned short cmd = 0;
  unsigned short stat = 0;
  unsigned short intr = 0;
  unsigned short frnum = 0;
  unsigned flbase = 0;
  unsigned char sof = 0;
  unsigned short portsc1 = 0;
  unsigned short portsc2 = 0;

  kernelProcessorInPort16((usb->ioAddress + USBUHCI_PORTOFFSET_CMD), cmd);
  kernelProcessorInPort16((usb->ioAddress + USBUHCI_PORTOFFSET_STAT), stat);
  kernelProcessorInPort16((usb->ioAddress + USBUHCI_PORTOFFSET_INTR), intr);
  kernelProcessorInPort16((usb->ioAddress + USBUHCI_PORTOFFSET_FRNUM), frnum);
  kernelProcessorInPort32((usb->ioAddress + USBUHCI_PORTOFFSET_FLBASE),
			  flbase);
  kernelProcessorInPort8((usb->ioAddress + USBUHCI_PORTOFFSET_SOF), sof);
  kernelProcessorInPort16((usb->ioAddress + USBUHCI_PORTOFFSET_PORTSC1),
			  portsc1);
  kernelProcessorInPort16((usb->ioAddress + USBUHCI_PORTOFFSET_PORTSC2),
			  portsc2);

  kernelDebug(debug_usb, "Debug UHCI registers:\n"
	      "    cmd=%04x\n"
	      "    stat=%04x\n"
	      "    intr=%04x\n"
	      "    frnum=%04x\n"
	      "    flbase=%08x\n"
	      "    sof=%02x\n"
	      "    portsc1=%04x\n"
	      "    portsc2=%04x\n", cmd, stat, intr, frnum, flbase, sof,
	      portsc1, portsc2);
}


static inline void debugDeviceReq(usbDeviceRequest *req)
{
    kernelDebug(debug_usb, "Debug device request:\n"
		"    requestType=%02x\n"
		"    request=%02x\n"
		"    value=%04x\n"
		"    index=%04x\n"
		"    length=%04x", req->requestType, req->request, req->value,
		req->index, req->length);
}


static inline void debugTransDesc(usbUhciTransDesc *desc)
{
    kernelDebug(debug_usb, "Debug transfer descriptor:\n"
		"    linkPointer=%08x\n"
		"    contStatus=%08x\n"
		"        spd=%d\n"
		"        errcount=%d\n"
		"        lowspeed=%d\n"
		"        isochronous=%d\n"
		"        interrupt=%d\n"
		"        status=%02x\n"
		"        actlen=%d (%03x)\n"
		"    tdToken=%08x\n"
		"        maxlen=%d (%03x)\n"
		"        datatoggle=%d\n"
		"        endpoint=%d\n"
		"        address=%d\n"
		"        pid=%02x\n"
		"    buffer=%08x", desc->linkPointer, desc->contStatus,
		((desc->contStatus & USBUHCI_TDCONTSTAT_SPD) >> 29),
		((desc->contStatus & USBUHCI_TDCONTSTAT_ERRCNT) >> 27),
		((desc->contStatus & USBUHCI_TDCONTSTAT_LSPEED) >> 26),
		((desc->contStatus & USBUHCI_TDCONTSTAT_ISOC) >> 25),
		((desc->contStatus & USBUHCI_TDCONTSTAT_INTR) >> 24),
		((desc->contStatus & USBUHCI_TDCONTSTAT_STATUS) >> 16),
		(desc->contStatus & USBUHCI_TDCONTSTAT_ACTLEN),
		(desc->contStatus & USBUHCI_TDCONTSTAT_ACTLEN),
		desc->tdToken,
		((desc->tdToken & USBUHCI_TDTOKEN_MAXLEN) >> 21),
		((desc->tdToken & USBUHCI_TDTOKEN_MAXLEN) >> 21),
		((desc->tdToken & USBUHCI_TDTOKEN_DATATOGGLE) >> 19),
		((desc->tdToken & USBUHCI_TDTOKEN_ENDPOINT) >> 15),
		((desc->tdToken & USBUHCI_TDTOKEN_ADDRESS) >> 8),
		(desc->tdToken & USBUHCI_TDTOKEN_PID),
		(unsigned) desc->buffer);
}


static inline void debugDeviceDesc(usbDeviceDesc *deviceDesc
				   __attribute__((unused)))
{
  kernelDebug(debug_usb, "Debug device descriptor:\n"
	      "    descLength=%d\n"
	      "    descType=%d\n"
	      "    usbVersion=%d.%d\n"
	      "    deviceClass=%x\n"
	      "    deviceSubClass=%x\n"
	      "    deviceProtocol=%x\n"
	      "    maxPacketSize0=%d\n"
	      "    vendorId=%04x\n"
	      "    productId=%04x\n"
	      "    deviceVersion=%d.%d\n"
	      "    manuStringIdx=%d\n"
	      "    prodStringIdx=%d\n"
	      "    serStringIdx=%d\n"
	      "    numConfigs=%d", deviceDesc->descLength,
	      deviceDesc->descType, ((deviceDesc->usbVersion & 0xFF00) >> 8),
	      (deviceDesc->usbVersion & 0xFF), deviceDesc->deviceClass,
	      deviceDesc->deviceSubClass, deviceDesc->deviceProtocol,
	      deviceDesc->maxPacketSize0, deviceDesc->vendorId,
	      deviceDesc->productId,
	      ((deviceDesc->deviceVersion & 0xFF00) >> 8),
	      (deviceDesc->deviceVersion & 0xFF), deviceDesc->manuStringIdx,
	      deviceDesc->prodStringIdx, deviceDesc->serStringIdx,
	      deviceDesc->numConfigs);
}


static inline void debugDevQualDesc(usbDevQualDesc *devQualDesc
				    __attribute__((unused)))
{
  kernelDebug(debug_usb, "Debug device qualifier descriptor:\n"
	      "    descLength=%d\n"
	      "    descType=%d\n"
	      "    usbVersion=%d.%d\n"
	      "    deviceClass=%x\n"
	      "    deviceSubClass=%x\n"
	      "    deviceProtocol=%x\n"
	      "    maxPacketSize0=%d\n"
	      "    numConfigs=%d", devQualDesc->descLength,
	      devQualDesc->descType, ((devQualDesc->usbVersion & 0xFF00) >> 8),
	      (devQualDesc->usbVersion & 0xFF), devQualDesc->deviceClass,
	      devQualDesc->deviceSubClass, devQualDesc->deviceProtocol,
	      devQualDesc->maxPacketSize0, devQualDesc->numConfigs);
}


static inline void debugConfigDesc(usbConfigDesc *configDesc
				   __attribute__((unused)))
{
  kernelDebug(debug_usb, "Debug config descriptor:\n"
	      "    descLength=%d\n"
	      "    descType=%d\n"
	      "    totalLength=%d\n"
	      "    numInterfaces=%d\n"
	      "    confValue=%d\n"
	      "    confStringIdx=%d\n"
	      "    attributes=%d\n"
	      "    maxPower=%d", configDesc->descLength, configDesc->descType,
	      configDesc->totalLength, configDesc->numInterfaces,
	      configDesc->confValue, configDesc->confStringIdx,
	      configDesc->attributes, configDesc->maxPower);
}


static inline void debugInterDesc(usbInterDesc *interDesc
				  __attribute__((unused)))
{
  kernelDebug(debug_usb, "Debug inter descriptor:\n"
	      "    descLength=%d\n"
	      "    descType=%d\n"
	      "    interNum=%d\n"
	      "    altSetting=%d\n"
	      "    numEndpoints=%d\n"
	      "    interClass=%d\n"
	      "    interSubClass=%d\n"
	      "    interProtocol=%d\n"
	      "    interStringIdx=%d", interDesc->descLength,
	      interDesc->descType, interDesc->interNum, interDesc->altSetting,
	      interDesc->numEndpoints, interDesc->interClass,
	      interDesc->interSubClass, interDesc->interProtocol,
	      interDesc->interStringIdx);
}


static inline void debugEndpointDesc(usbEndpointDesc *endpointDesc
				     __attribute__((unused)))
{
  kernelDebug(debug_usb, "Debug endpoint descriptor:\n"
	      "    descLength=%d\n"
	      "    descType=%d\n"
	      "    endpntAddress=%d\n"
	      "    attributes=%d\n"
	      "    maxPacketSize=%d\n"
	      "    interval=%d", endpointDesc->descLength,
	      endpointDesc->descType, endpointDesc->endpntAddress,
	      endpointDesc->attributes, endpointDesc->maxPacketSize,
	      endpointDesc->interval);
}

// Since the kernelDebug() API doesn't have debugging levels, this will
// just output warning messages when DEBUG is defined.
#define debugError(message, arg...) kernelError(kernel_warn, message, ##arg)

#else
  #define debugUhciRegs(usb) do { } while (0)
  #define debugDeviceReq(req) do { } while (0)
  #define debugTransDesc(desc) do { } while (0)
  #define debugDeviceDesc(desc) do { } while (0)
  #define debugDevQualDesc(desc) do { } while (0)
  #define debugConfigDesc(desc) do { } while (0)
  #define debugInterDesc(desc) do { } while (0)
  #define debugEndpointDesc(desc) do { } while (0)
  #define debugError(message, arg...) do { } while (0)
#endif


static inline unsigned char readCommand(usbRootHub *usb)
{
  unsigned short command = 0;
  kernelProcessorInPort16((usb->ioAddress + USBUHCI_PORTOFFSET_CMD), command);
  return ((unsigned char)(command & 0xFF));
}


static inline void writeCommand(usbRootHub *usb, unsigned char command)
{
  unsigned short tmp = 0;
  kernelProcessorInPort16((usb->ioAddress + USBUHCI_PORTOFFSET_CMD), tmp);
  tmp = ((tmp & 0xFF00) | command);
  kernelProcessorOutPort16((usb->ioAddress + USBUHCI_PORTOFFSET_CMD), tmp);
}


static inline unsigned char readStatus(usbRootHub *usb)
{
  unsigned short status = 0;
  kernelProcessorInPort16((usb->ioAddress + USBUHCI_PORTOFFSET_STAT), status);
  return ((unsigned char)(status & 0x3F));
}


static inline void writeStatus(usbRootHub *usb, unsigned char status)
{
  unsigned short tmp = 0;
  kernelProcessorInPort16((usb->ioAddress + USBUHCI_PORTOFFSET_STAT), tmp);
  tmp |= (status & 0x3F);
  kernelProcessorOutPort16((usb->ioAddress + USBUHCI_PORTOFFSET_STAT), tmp);
}


static inline unsigned short readFrameNum(usbRootHub *usb)
{
  unsigned short num = 0;
  kernelProcessorInPort16((usb->ioAddress + USBUHCI_PORTOFFSET_FRNUM), num);
  return (num & 0x7FF);
}


static inline void writeFrameNum(usbRootHub *usb, unsigned short num)
{
  unsigned short tmp = 0;
  kernelProcessorInPort16((usb->ioAddress + USBUHCI_PORTOFFSET_FRNUM), tmp);
  tmp = ((tmp & 0xF800) | (num & 0x7FF));
  kernelProcessorOutPort16((usb->ioAddress + USBUHCI_PORTOFFSET_FRNUM), tmp);
}


static void startStop(usbRootHub *usb, int start)
{
  // Start the USB controller

  unsigned char command = 0;
  unsigned char status = 0;

  //kernelDebug(debug_usb, "%s controller", (start? "Start" : "Stop"));

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

  // Clear the status register
  writeStatus(usb, status);
}


static void delay(usbRootHub *usb, int delayMs)
{
  unsigned timeOut = 0;
  int elapsedMs = 0;
  unsigned short frameNum = 0;

  kernelDebug(debug_usb, "Delay %d ms", delayMs);

  // To make sure we delay at *least* the requested time
  delayMs += 1;

  // To make sure we don't get hung up here.  This ensures sure we don't wait
  // more than a full second longer than we're supposed to.
  timeOut = (kernelSysTimerRead() + (((delayMs / 1000) + 1) * 20));

  while (1)
    {
      frameNum = readFrameNum(usb);
      //kernelDebug(debug_usb, "frameNum %d", frameNum);
      while (readFrameNum(usb) == frameNum)
	{
	  // Make sure the controller is running
	  if (readStatus(usb) & USBUHCI_STAT_HCHALTED)
	    {
	      debugError("Can't delay() while controller is halted");
	      return;
	    }

	  if (kernelSysTimerRead() >= timeOut)
	    {
	      debugError("System timer timeout");
	      return;
	    }
	}

      elapsedMs += 1;

      if (elapsedMs >= delayMs)
	break;
    }
}


static void reset(usbRootHub *usb)
{
  // Do complete USB reset

  unsigned char command = 0;

  // Stop the controller
  startStop(usb, 0);

  // Set global reset
  command = readCommand(usb);
  command |= USBUHCI_CMD_GRESET;
  writeCommand(usb, command);

  // Delay.  Can't use the delay() function whilst the controller is stopped.
  kernelDebug(debug_usb, "Delay for global reset");
  kernelSysTimerWaitTicks(2);

  // Clear global reset
  command = readCommand(usb);
  command &= ~USBUHCI_CMD_GRESET;
  writeCommand(usb, command);

  // Clear the lock
  kernelMemClear((void *) &(usb->lock), sizeof(lock));

  kernelDebug(debug_usb, "UHCI controller reset");
  return;
}


static inline unsigned short readPortStatus(usbRootHub *usb, int num)
{
  unsigned portOffset = 0;
  unsigned short status = 0;

  if (num == 0)
    portOffset = USBUHCI_PORTOFFSET_PORTSC1;
  else if (num == 1)
    portOffset = USBUHCI_PORTOFFSET_PORTSC2;
  else
    return (status = 0);

  kernelProcessorInPort16((usb->ioAddress + portOffset), status);

  // Mask off meaningless bits
  status &= 0x137F;
  return (status);
}


static inline void writePortStatus(usbRootHub *usb, int num,
				   unsigned short status)
{
  unsigned portOffset = 0;

  if (num == 0)
    portOffset = USBUHCI_PORTOFFSET_PORTSC1;
  else if (num == 1)
    portOffset = USBUHCI_PORTOFFSET_PORTSC2;
  else
    return;

  // Mask off meaningless and read-only bits
  status &= 0x124E;

  kernelProcessorOutPort16((usb->ioAddress + portOffset), status);
  return;
}


#ifdef DEBUG
static inline void printPortStatus(usbRootHub *usb)
{
  unsigned short port0status = 0;
  unsigned short port1status = 0;
  unsigned short frameNum = 0;
  usbUhciData *uhciData __attribute__((unused)) = usb->data;

  port0status = readPortStatus(usb, 0);
  port1status = readPortStatus(usb, 1);
  frameNum = readFrameNum(usb);

  kernelDebug(debug_usb, "Port 0: %04x  port 1: %04x frnum %d=%x",
	      port0status, port1status, (frameNum & 0x3FF),
	      uhciData->frameList[frameNum & 0x3FF]);
}
#else
  #define printPortStatus(usb) do { } while (0)
#endif


static void portReset(usbRootHub *usb, int num)
{
  unsigned short status = 0;

  // Set the reset bit
  status = readPortStatus(usb, num);
  status |= USBUHCI_PORT_RESET;
  writePortStatus(usb, num, status);

  // Delay for 50ms
  kernelDebug(debug_usb, "Delay for port reset");
  delay(usb, 50);

  if (!(readPortStatus(usb, num) & USBUHCI_PORT_RESET))
    {
      kernelError(kernel_error, "Couldn't set port reset bit");
      return;
    }

  // Clear the reset bit
  status = readPortStatus(usb, num);
  status &= ~USBUHCI_PORT_RESET;
  writePortStatus(usb, num, status);

  // Delay another 10ms
  kernelDebug(debug_usb, "Delay for port un-reset");
  delay(usb, 10);

  if (readPortStatus(usb, num) & USBUHCI_PORT_RESET)
    {
      kernelError(kernel_error, "Couldn't clear port reset bit");
      return;
    }

  return;
}


static void portEnable(usbRootHub *usb, int num, int enable)
{
  unsigned short status = 0;

  // Set or clelear the enabled bit
  status = readPortStatus(usb, num);
  if (enable)
    status |= USBUHCI_PORT_ENABLED;
  else
    status &= ~USBUHCI_PORT_ENABLED;
  writePortStatus(usb, num, status);

  if (enable && !(readPortStatus(usb, num) & USBUHCI_PORT_ENABLED))
    kernelError(kernel_error, "Couldn't enable port");
  else if (!enable && (readPortStatus(usb, num) & USBUHCI_PORT_ENABLED))
    kernelError(kernel_error, "Couldn't disable port");

  return;
}


static int allocTransDescs(unsigned numDescs, void **physical,
			   usbUhciTransDesc **descs)
{
  // Allocate an array of USB transfer descriptors, page-aligned.

  int status = 0;
  unsigned memSize = 0; 
  unsigned count;

  memSize = (numDescs * sizeof(usbUhciTransDesc));

  // Use the 'get physical' call because it allows us to specify alignment.
  *physical =
    kernelMemoryGetPhysical(memSize, MEMORY_PAGE_SIZE, "usb xfer descriptors");

  if ((*physical == NULL) || ((unsigned) *physical & 0x0F))
    {
      kernelError(kernel_error, "Unable to get USB transfer descriptor "
		  "memory");
      return (status = ERR_MEMORY);
    }

  // Map it to a virtual address
  status =
    kernelPageMapToFree(KERNELPROCID, *physical, (void **) descs, memSize);
  if (status < 0)
    {
      kernelError(kernel_error, "Unable to map USB transfer descriptor "
		  "memory");
      kernelMemoryReleasePhysical(*physical);
      return (status);
    }

  // Clear the memory, since the 'allocate physical' can't do it for us.
  kernelMemClear((void *) *descs, memSize);

  // Set all the link pointers invalid
  for (count = 0; count < numDescs; count ++)
    (*descs)[count].linkPointer = USBUHCI_LINKPTR_TERM;

  return (status = 0);
}


static void deallocTransDescs(usbUhciTransDesc *descs, int numDescs)
{
  void *transDescsPhysical = NULL;
  
  transDescsPhysical =
    (void *) kernelPageGetPhysical(KERNELPROCID, (void *) descs);
  kernelPageUnmap(KERNELPROCID, (void *) descs,
		  (numDescs * sizeof(usbUhciTransDesc)));
  if (transDescsPhysical)
    kernelMemoryReleasePhysical(transDescsPhysical);
}


static void deallocUhciMemory(usbRootHub *usb)
{
  usbUhciData *uhciData = usb->data;
  void *queueHeadsPhysical = NULL;

  if (uhciData)
    {
      if (uhciData->frameList)
	kernelPageUnmap(KERNELPROCID, uhciData->frameList, MEMORY_PAGE_SIZE);

      if (uhciData->frameListPhysical)
	kernelMemoryReleasePhysical(uhciData->frameListPhysical);

      if (uhciData->intrQueueHead)
	{
	  queueHeadsPhysical =
	    kernelPageGetPhysical(KERNELPROCID,
				  (void *) uhciData->intrQueueHead);
	  kernelPageUnmap(KERNELPROCID, (void *) uhciData->intrQueueHead,
			  USBUHCI_QUEUEHEADS_MEMSIZE);
	  if (queueHeadsPhysical)
	    kernelMemoryReleasePhysical(queueHeadsPhysical);
	}

      if (uhciData->termTransDesc)
	deallocTransDescs(uhciData->termTransDesc, 1);

      kernelFree(uhciData);
      usb->data = NULL;
    }
}


static int allocUhciMemory(usbRootHub *usb)
{
  // Allocate all of the memory bits specific to the the UHCI controller.

  int status = 0;
  usbUhciData *uhciData = usb->data;
  void *queueHeadsPhysical = NULL;
  usbUhciQueueHead *queueHeads = NULL;
  void *transDescPhysical = NULL;
  int count;

  // Allocate the UHCI hub's private data
  usb->data = kernelMalloc(sizeof(usbUhciData));
  if (usb->data == NULL)
    return (status = ERR_MEMORY);

  uhciData = usb->data;

  // Allocate the USB frame list.  1024 32-bit values, so one page of memory,
  // page-aligned.  We need to put the physical address into the register.

  uhciData->frameListPhysical =
    kernelMemoryGetPhysical((1024 * sizeof(unsigned)), MEMORY_PAGE_SIZE,
			    "usb frame list");
  if ((uhciData->frameListPhysical == NULL) ||
      ((unsigned) uhciData->frameListPhysical & 0x0FFF))
    {
      kernelError(kernel_error, "Unable to get USB frame list memory");
      status = ERR_MEMORY;
      goto err_out;
    }

  // Map it to a virtual address
  status =
    kernelPageMapToFree(KERNELPROCID, uhciData->frameListPhysical,
			(void **) &(uhciData->frameList), MEMORY_PAGE_SIZE);
  if (status < 0)
    {
      kernelError(kernel_error, "Unable to map USB frame list memory");
      goto err_out;
    }

  // Fill the list with 32-bit 'term' (1) values, indicating that all pointers
  // are currently invalid
  kernelProcessorWriteDwords(USBUHCI_LINKPTR_TERM, uhciData->frameList,
			     (MEMORY_PAGE_SIZE / sizeof(unsigned)));

  // Allocate an array of USBUHCI_NUM_QUEUEHEADS queue heads, page-aligned.

  queueHeadsPhysical =
    kernelMemoryGetPhysical(USBUHCI_QUEUEHEADS_MEMSIZE, MEMORY_PAGE_SIZE,
			    "usb queue heads");
  if ((queueHeadsPhysical == NULL) || ((unsigned) queueHeadsPhysical & 0x0F))
    {
      kernelError(kernel_error, "Unable to get USB queue heads memory");
      status = ERR_MEMORY;
      goto err_out;
    }

  // Map it to a virtual address
  status =
    kernelPageMapToFree(KERNELPROCID, queueHeadsPhysical,
			(void **) &queueHeads, USBUHCI_QUEUEHEADS_MEMSIZE);
  if (status < 0)
    {
      kernelError(kernel_error, "Unable to map USB queue heads memory");
      goto err_out;
    }

  // Set the link pointers invalid
  for (count = 0; count < USBUHCI_NUM_QUEUEHEADS; count ++)
    {
      queueHeads[count].linkPointer = USBUHCI_LINKPTR_TERM;
      queueHeads[count].element = USBUHCI_LINKPTR_TERM;
    }

  // Assign them
  uhciData->intrQueueHead = &(queueHeads[0]);
  uhciData->controlQueueHead = &(queueHeads[1]);
  uhciData->bulkQueueHead = &(queueHeads[2]);
  uhciData->termQueueHead = &(queueHeads[3]);

  // Allocate a blank transfer descriptor to attach to the terminating queue
  // head
  status = allocTransDescs(1, &transDescPhysical, &(uhciData->termTransDesc));
  if (status < 0)
    goto err_out;

  // Success
  return (status = 0);

 err_out:
  deallocUhciMemory(usb);
  return (status);
}


static int runTransaction(usbRootHub *usb, usbUhciQueueHead *queueHead,
			  usbUhciTransDesc *descs, unsigned numDescs)
{
  // Given a list of transfer descriptors associated with a single transaction,
  // queue them up on the controller.

  int status = 0;
  unsigned descPhysical = NULL;
  unsigned firstPhysical = NULL;
  unsigned prevFirstActive = 0;
  unsigned firstActive = 0;
  unsigned startTime = 0;
  int active = 0;
  int stalled = 0;
  unsigned count;

  kernelDebug(debug_usb, "Run transaction with %d transfers", numDescs);

  // Lock the controller
  status = kernelLockGet(&(usb->lock));
  if (status < 0)
    {
      kernelError(kernel_error, "Can't get controller lock");
      goto out;
    }

  // Process the transfer descriptors
  for (count = 0; count < numDescs; count ++)
    {
      // Isochronous?
      if (descs[count].contStatus & USBUHCI_TDCONTSTAT_ISOC)
	{
	  kernelError(kernel_error, "Isochronous transfers not yet supported");
	  status = ERR_NOTIMPLEMENTED;
	  goto out;
	}
      else
	{
	  // Get the physical address of the TD
	  descPhysical = (unsigned)
	    kernelPageGetPhysical(KERNELPROCID, (void *) &(descs[count]));
	  if (descPhysical == NULL)
	    {
	      kernelError(kernel_error, "Can't get xfer descriptor physical "
			  "address");
	      status = ERR_MEMORY;
	      goto out;
	    }
	  if (descPhysical & 0xF)
	    {
	      kernelError(kernel_error, "Xfer descriptor not 16-byte aligned");
	      status = ERR_ALIGN;
	      goto out;
	    }

	  if (count)
	    // Attach this TD to the previous TD.
	    descs[count - 1].linkPointer =
	      (descPhysical | USBUHCI_TDLINK_DEPTHFIRST);
	  else
	    // Save the address because we'll attach it to the queue head in
	    // a minute.
	    firstPhysical = descPhysical;

	  // Blank the descriptor's link pointer and set the 'terminate' bit
	  descs[count].linkPointer = USBUHCI_LINKPTR_TERM;
	}
    }

  // Everything's queued up.  Attach the first TD to the queue head
  queueHead->element = firstPhysical;

  prevFirstActive = 0;
  firstActive = 0;
  startTime = kernelSysTimerRead();

  // Now wait while some TD is active, or until we detect an error
  while (1)
    {
      // See if there are still any active TDs, or if any have 'stalled'
      // error status
      active = 0;
      stalled = 0;
      for (count = 0; count < numDescs; count ++)
	{
	  if (descs[count].contStatus & USBUHCI_TDCONTSTAT_ESTALL)
	    {
	      debugError("UHCI: transaction stalled");
	      stalled = 1;
	      break;
	    }
	  if (descs[count].contStatus & USBUHCI_TDCONTSTAT_ACTIVE)
	    {
	      firstActive = count;
	      active = 1;
	      break;
	    }
	}

      // If no more active, or errors, we're finished.
      if (!active || stalled)
	{
	  if (stalled)
	    status = ERR_IO;
	  else
	    status = 0;
	  break;
	}

      // If the controller is moving through the queue, reset the timeout
      if (firstActive > prevFirstActive)
	{
	  startTime = kernelSysTimerRead();
	  prevFirstActive = firstActive;
	}
      else if (kernelSysTimerRead() > (startTime + 100))
	{
	  // Software timeout after ~5 seconds
	  debugError("UHCI: Software timeout on TD %d", firstActive);
	  status = ERR_NODATA;
	  break;
	}
      
      kernelMultitaskerYield();
    }

  // Unschedule by setting the terminate bit on the queue head element pointer
  queueHead->element = USBUHCI_LINKPTR_TERM;

 out:

  // Clear the status register
  writeStatus(usb, readStatus(usb));

  kernelLockRelease(&(usb->lock));
  return (status);
}


static int setupTransDesc(usbUhciTransDesc *desc, usbxferType type,
			  int address, int endpoint, char dataToggle,
			  unsigned char pid)
{
  // Do the nuts-n-bolts setup for a transfer descriptor

  int status = 0;
  const char *pidString = NULL;

  // Blank the descriptor's link pointer and set the 'terminate' bit
  desc->linkPointer = USBUHCI_LINKPTR_TERM;

  // Initialize the 'control and status' field.  Short packet detect, 3 error
  // retries, active, no interrupt on complete,
  // active
  desc->contStatus = (USBUHCI_TDCONTSTAT_SPD | USBUHCI_TDCONTSTAT_ERRCNT |
		      USBUHCI_TDCONTSTAT_ACTIVE);
  if (type == usbxfer_isochronous)
    desc->contStatus |= USBUHCI_TDCONTSTAT_ISOC;

  // Set up the TD token field

  // First the data size
  if (desc->buffSize)
    desc->tdToken = (((desc->buffSize - 1) << 21) & USBUHCI_TDTOKEN_MAXLEN);
  else
    desc->tdToken = (USBUHCI_TD_NULLDATA << 21);

  if (type != usbxfer_isochronous)
    // The data toggle
    desc->tdToken |= ((dataToggle << 19) & USBUHCI_TDTOKEN_DATATOGGLE);

  // The endpoint
  desc->tdToken |= ((endpoint << 15) & USBUHCI_TDTOKEN_ENDPOINT);
  // The address
  desc->tdToken |= ((address << 8) & USBUHCI_TDTOKEN_ADDRESS);
  // The packet identification
  desc->tdToken |= (pid & USBUHCI_TDTOKEN_PID);

  if (pid == USB_PID_SETUP)
    pidString = "SETUP for";
  else if (pid == USB_PID_IN)
    pidString = "IN from";
  else if (pid == USB_PID_OUT)
    pidString = "OUT to";
  else
    {
      kernelError(kernel_error, "PID type %02x is unknown", pid);
      return (status = ERR_INVALID);
    }

  kernelDebug(debug_usb, "Setup transfer %s address %d:%d, %d bytes, "
   	      "dataToggle %d", pidString, address, endpoint, desc->buffSize,
   	      dataToggle);

  return (status = 0);
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


static int allocTransDescBuffer(usbUhciTransDesc *desc, unsigned buffSize)
{
  // Allocate a data buffer for a transfer descriptor.

  int status = 0;

  desc->buffVirtAddr = kernelMalloc(buffSize);
  if (desc->buffVirtAddr == NULL)
    {
      debugError("Can't alloc trans desc buffer size %u", buffSize);
      return (status = ERR_MEMORY);
    }

  // Get the physical address of this memory
  desc->buffer = kernelPageGetPhysical(KERNELPROCID, desc->buffVirtAddr);
  if (desc->buffer == NULL)
    {
      debugError("Can't get buffer physical address");
      kernelFree(desc->buffVirtAddr);
      return (status = ERR_MEMORY);
    }

  desc->buffSize = buffSize;
  return (status = 0);
}


static int transaction(usbRootHub *usb, usbDevice *dev, usbTransaction *trans)
{
  // This function contains the intelligence necessary to initiate a
  // transaction (all phases)

  int status = 0;
  usbEndpointDesc *endpoint = NULL;
  unsigned bytesPerTransfer = 0;
  unsigned numDescs = 0;
  void *descsPhysical = NULL;
  usbUhciTransDesc *descs = NULL;
  usbUhciData *uhciData = usb->data;
  usbUhciQueueHead *queueHead = NULL;
  usbUhciTransDesc *setupDesc = NULL;
  usbDeviceRequest *req = NULL;
  const char *opString = NULL;
  usbUhciTransDesc *dataDesc = NULL;
  unsigned bytesToTransfer = 0;
  char dataToggle = 0;
  usbUhciTransDesc *statusDesc = NULL;
  void *buffer = NULL;
  const char *transString = NULL;
  char *errorText = NULL;
  unsigned count;

  // Figure out how many transfer descriptors we're going to need for the whole
  // transaction

  if (trans->type == usbxfer_control)
    // At least one each for setup and status
    numDescs += 2;

  if (trans->length)
    {
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
	      status = ERR_NOSUCHFUNCTION;
	      goto out;
	    }

	  bytesPerTransfer = endpoint->maxPacketSize;
	}

      // If we haven't yet got the descriptors, etc., 8 is the minimum size
      if (bytesPerTransfer < 8)
	{
	  kernelDebug(debug_usb, "Using minium endpoint transfer size 8 "
		      "instead of %d for endpoint %d", bytesPerTransfer,
		      trans->endpoint);
	  bytesPerTransfer = 8;
	}
      else
	kernelDebug(debug_usb, "Max bytes per transfer for endpoint %d: %d",
		    trans->endpoint, bytesPerTransfer);

      numDescs += ((trans->length / bytesPerTransfer) +
		   ((trans->length % bytesPerTransfer)? 1 : 0));

      // If this is an outbound interrupt or bulk transfer, and the bytes
      // to transfer is a multiple of bytesPerTransfer, we need one more.
      if ((trans->pid == USB_PID_OUT) &&
	  ((trans->type == usbxfer_interrupt) ||
	   (trans->type == usbxfer_bulk)) &&
	  !(trans->length % bytesPerTransfer))
	numDescs += 1;
    }

  // Get memory for the transfer descriptors
  kernelDebug(debug_usb, "Transaction requires %d transfers", numDescs);
  status = allocTransDescs(numDescs, &descsPhysical, &descs);
  if (status < 0)
    goto out;
  numDescs = 0;

  if (trans->type == usbxfer_control)
    {
      // Use the control queue head
      queueHead = uhciData->controlQueueHead;

      // Get the transfer descriptor for the setup phase
      setupDesc = &(descs[numDescs++]);

      // By default it's an OUT transaction
      trans->pid = USB_PID_OUT;

      // Begin setting up the device request

      // Get a buffer for the device request memory
      status = allocTransDescBuffer(setupDesc, sizeof(usbDeviceRequest));
      if (status < 0)
	goto out;
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

      if (req->requestType & (USB_DEVREQTYPE_CLASS | USB_DEVREQTYPE_VENDOR))
	// The request is class- or vendor-specific
	opString = "class/vendor-specific control transfer";

      else
	{
	  // What request are we doing?  Determine the correct requestType and
	  // whether there will be a data phase, etc.
	  switch (trans->control.request)
	    {
	    case USB_GET_STATUS:
	      opString = "USB_GET_STATUS";
	      req->requestType |= USB_DEVREQTYPE_DEV2HOST;
	      trans->pid = USB_PID_IN;
	      break;
	    case USB_CLEAR_FEATURE:
	      opString = "USB_CLEAR_FEATURE";
	      req->requestType |= USB_DEVREQTYPE_HOST2DEV;
	      break;
	    case USB_SET_FEATURE:
	      opString = "USB_SET_FEATURE";
	      req->requestType |= USB_DEVREQTYPE_HOST2DEV;
	      break;
	    case USB_SET_ADDRESS:
	      opString = "USB_SET_ADDRESS";
	      req->requestType |= USB_DEVREQTYPE_HOST2DEV;
	      break;
	    case USB_GET_DESCRIPTOR:
	      opString = "USB_GET_DESCRIPTOR";
	      req->requestType |= USB_DEVREQTYPE_DEV2HOST;
	      trans->pid = USB_PID_IN;
	      break;
	    case USB_SET_DESCRIPTOR:
	      opString = "USB_SET_DESCRIPTOR";
	      req->requestType |= USB_DEVREQTYPE_HOST2DEV;
	      break;
	    case USB_GET_CONFIGURATION:
	      opString = "USB_GET_CONFIGURATION";
	      req->requestType |= USB_DEVREQTYPE_DEV2HOST;
	      trans->pid = USB_PID_IN;
	      break;
	    case USB_SET_CONFIGURATION:
	      opString = "USB_SET_CONFIGURATION";
	      req->requestType |= USB_DEVREQTYPE_HOST2DEV;
	      break;
	    case USB_GET_INTERFACE:
	      opString = "USB_GET_INTERFACE";
	      req->requestType |= USB_DEVREQTYPE_DEV2HOST;
	      trans->pid = USB_PID_IN;
	      break;
	    case USB_SET_INTERFACE:
	      opString = "USB_SET_INTERFACE";
	      req->requestType |= USB_DEVREQTYPE_HOST2DEV;
	      break;
	    case USB_SYNCH_FRAME:
	      opString = "USB_SYNCH_FRAME";
	      req->requestType |= USB_DEVREQTYPE_DEV2HOST;
	      trans->pid = USB_PID_IN;
	      break;
	    case USB_MASSSTORAGE_RESET:
	      opString = "USB_MASSSTORAGE_RESET";
	      req->requestType |= USB_DEVREQTYPE_HOST2DEV;
	      break;
	    default:
	      // Perhaps some thing we don't know about.  Try to proceed
	      // anyway.
	      opString = "unknown control transfer";
	      break;
	    }
	}

      kernelDebug(debug_usb, "Do %s for address %d", opString, trans->address);
    }

  else if (trans->type == usbxfer_interrupt)
    // Use the interrupt queue head
    queueHead = uhciData->intrQueueHead;

  else if (trans->type == usbxfer_bulk)
    // Use the bulk queue head
    queueHead = uhciData->bulkQueueHead;

  else
    {
      kernelError(kernel_error, "Unsupported transaction type %d",
		  trans->type);
      status = ERR_NOTIMPLEMENTED;
      goto out;
    }

  if (trans->type == usbxfer_control)
    {
      // Setup the transfer descriptor for the setup phase
      status = setupTransDesc(setupDesc, trans->type, trans->address,
			      trans->endpoint, 0, USB_PID_SETUP);
      if (status < 0)
	goto out;

      // Data toggle
      dataToggle ^= 1;
    }

  // If there is a data phase, setup the transfer descriptor(s) for the data
  // phase
  if (trans->length)
    {
      buffer = trans->buffer;
      bytesToTransfer = trans->length;

      // If this is an interrupt or bulk transfer, endpoint data toggle
      // is persistent
      if ((trans->type == usbxfer_interrupt) || (trans->type == usbxfer_bulk))
	dataToggle = dev->dataToggle[trans->endpoint];

      while (bytesToTransfer)
	{
	  unsigned doBytes = min(bytesToTransfer, bytesPerTransfer);

	  dataDesc = &(descs[numDescs++]);

	  // Point the data descriptor's buffer to the relevent portion of
	  // the transaction buffer
	  dataDesc->buffVirtAddr = buffer;
	  dataDesc->buffer =
	    kernelPageGetPhysical((((unsigned) dataDesc->buffVirtAddr <
				    KERNEL_VIRTUAL_ADDRESS)?
				   kernelCurrentProcess->processId :
				   KERNELPROCID), dataDesc->buffVirtAddr);
	  if (dataDesc->buffer == NULL)
	    {
	      debugError("Can't get physical address for buffer fragment at "
			 "%p", dataDesc->buffVirtAddr);
	      status = ERR_MEMORY;
	      goto out;
	    }
	  dataDesc->buffSize = doBytes;

	  status = setupTransDesc(dataDesc, trans->type, trans->address,
				  trans->endpoint, dataToggle, trans->pid);
	  if (status < 0)
	    goto out;
	  
	  // Data toggle
	  dataToggle ^= 1;
	  
	  bytesToTransfer -= doBytes;
	  buffer += doBytes;

	  // If this is an outbound interrupt or bulk transfer, and the bytes
	  // to transfer is a multiple of bytesPerTransfer, set up an
	  // additional, empty descriptor at the end.
	  if ((bytesToTransfer <= 0) && (trans->pid == USB_PID_OUT) &&
	      ((trans->type == usbxfer_interrupt) ||
	       (trans->type == usbxfer_bulk)) &&
	      !(trans->length % bytesPerTransfer))
	    {
	      dataDesc = &(descs[numDescs++]);

	      status = setupTransDesc(dataDesc, trans->type, trans->address,
				      trans->endpoint, dataToggle, trans->pid);
	      if (status < 0)
		goto out;
	  
	      // Data toggle
	      dataToggle ^= 1;
	    }
	}
    }

  if (trans->type == usbxfer_control)
    {
      // Setup the transfer descriptor for the status phase

      statusDesc = &(descs[numDescs++]);

      // Setup the status packet
      status = setupTransDesc(statusDesc, trans->type, trans->address,
			      trans->endpoint, 1,
			      ((trans->pid == USB_PID_OUT)? USB_PID_IN :
			       USB_PID_OUT));
      if (status < 0)
	goto out;
    }

  // Run the transaction
  status = runTransaction(usb, queueHead, descs, numDescs);
  if (status < 0)
    {
      // Check for errors
      for (count = 0; count < numDescs; count ++)
	if (descs[count].contStatus & USBUHCI_TDCONTSTAT_ERROR)
	  {
	    errorText = kernelMalloc(MAXSTRINGLENGTH);
	    if (errorText)
	      {
		switch (descs[count].tdToken & USBUHCI_TDTOKEN_PID)
		  {
		  case USB_PID_SETUP:
		    transString = "SETUP";
		    break;
		  case USB_PID_IN:
		    transString = "IN";
		    break;
		  case USB_PID_OUT:
		    transString = "OUT";
		    break;
		  }
		sprintf(errorText, "UHCI: Transaction %d %s: ", count,
			transString);
		if (descs[count].contStatus & USBUHCI_TDCONTSTAT_ESTALL)
		  strcat(errorText, "stalled, ");
		if (descs[count].contStatus & USBUHCI_TDCONTSTAT_EDBUFF)
		  strcat(errorText, "data buffer error, ");
		if (descs[count].contStatus & USBUHCI_TDCONTSTAT_EBABBLE)
		  strcat(errorText, "babble, ");
		if (descs[count].contStatus & USBUHCI_TDCONTSTAT_ENAK)
		  strcat(errorText, "NAK, ");
		if (descs[count].contStatus & USBUHCI_TDCONTSTAT_ECRCTO)
		  strcat(errorText, "CRC/timeout, ");
		if (descs[count].contStatus & USBUHCI_TDCONTSTAT_EBSTUFF)
		  strcat(errorText, "bitstuff error, ");

		if (descs[count].contStatus & USBUHCI_TDCONTSTAT_ACTIVE)
		  strcat(errorText, "TD is still active");

		debugError(errorText);
		kernelFree(errorText);
	      }

	    status = ERR_IO;
	    debugTransDesc(&(descs[count]));
	    debugUhciRegs(usb);
	    break;
	  }
	else
	  {
	    if ((trans->type == usbxfer_interrupt) ||
		(trans->type == usbxfer_bulk))
	      // Record the last (highest numbered) successful data toggle
	      dev->dataToggle[trans->endpoint] =
		((descs[count].tdToken & USBUHCI_TDTOKEN_DATATOGGLE) >> 19);
	  }
    }
  else
    {
      if ((trans->type == usbxfer_interrupt) || (trans->type == usbxfer_bulk))
	// Endpoint data toggle is persistent
	dev->dataToggle[trans->endpoint] = dataToggle;

      kernelDebug(debug_usb, "Transaction completed successfully");
    }

 out:
  
  if (setupDesc && setupDesc->buffVirtAddr)
    kernelFree(setupDesc->buffVirtAddr);

  if (descs)
    deallocTransDescs(descs, numDescs);

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

  kernelDebug(debug_usb, "Control transfer of %d bytes", length);

  kernelMemClear((void *) &trans, sizeof(usbTransaction));
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


static int enumerateNewDevice(usbRootHub *usb, int port)
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

  // Reset the port and then enable it (resetting clears the enabled bit)
  portReset(usb, port);
  printPortStatus(usb);
  portEnable(usb, port, 1);
  printPortStatus(usb);

  dev = kernelMalloc(sizeof(usbDevice));
  if (dev == NULL)
    return (status = ERR_MEMORY);

  dev->controller = usb->controller;
  dev->port = port;

  // Try to set a device address
  kernelDebug(debug_usb, "Set address %d for new device",
	      (usb->addressCounter + 1));
  status = controlTransfer(usb, dev, 0, USB_SET_ADDRESS,
			   (usb->addressCounter + 1), 0, 0, NULL, NULL);
  if (status < 0)
    // No device waiting for an address, we guess
    goto err_out;

  // We're supposed to allow a 2ms delay for the device after the set address
  // command.
  kernelDebug(debug_usb, "Delay after set_address");
  delay(usb, 2);

  dev->address = (usb->addressCounter + 1);

  // Try getting a device descriptor of only 8 bytes.  Thereafter we will
  // *know* the supported packet size.
  kernelDebug(debug_usb, "Get short device descriptor for new device %d",
	      dev->address);
  status = controlTransfer(usb, dev, 0, USB_GET_DESCRIPTOR,
			   (USB_DESCTYPE_DEVICE << 8), 0, 8,
			   (void *) &(dev->deviceDesc), NULL);
  if (status < 0)
    {
      kernelError(kernel_error, "Error getting device descriptor");
      goto err_out;
    }

  // Now get the whole descriptor
  kernelDebug(debug_usb, "Get full device descriptor for new device %d",
	      dev->address);
  status =
    controlTransfer(usb, dev, 0, USB_GET_DESCRIPTOR,
		    (USB_DESCTYPE_DEVICE << 8), 0, sizeof(usbDeviceDesc),
		    (void *) &(dev->deviceDesc), NULL);
  if (status < 0)
    {
      kernelError(kernel_error, "Error getting device descriptor");
      goto err_out;
    }

  debugDeviceDesc((usbDeviceDesc *) &(dev->deviceDesc));

  // If the device is a USB 2.0+ device, see if it's got a 'device qualifier'
  // descriptor
  if (dev->deviceDesc.usbVersion >= 0x0200)
    {
      kernelDebug(debug_usb, "Get device qualifier for new device %d",
		  dev->address);
      controlTransfer(usb, dev, 0, USB_GET_DESCRIPTOR,
		      (USB_DESCTYPE_DEVICEQUAL << 8), 0,
		      sizeof(usbDevQualDesc), (void *) &(dev->devQualDesc),
		      &bytes);
      if (bytes)
	debugDevQualDesc((usbDevQualDesc *) &(dev->devQualDesc));
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
      status = ERR_MEMORY;
      goto err_out;
    }

  kernelDebug(debug_usb, "Get short first configuration for new device %d",
	      dev->address);
  status = controlTransfer(usb, dev, 0, USB_GET_DESCRIPTOR,
			   (USB_DESCTYPE_CONFIG << 8), 0,
			   min(dev->deviceDesc.maxPacketSize0,
			       sizeof(usbConfigDesc)), tmpConfigDesc, &bytes);
  if ((status < 0) &&
      (bytes < min(dev->deviceDesc.maxPacketSize0, sizeof(usbConfigDesc))))
    goto err_out;

  // If the device wants to return more information than will fit in the
  // max packet size for endpoint zero, we need to do a second request that
  // splits the data transfer into parts

  if (tmpConfigDesc->totalLength >
      min(dev->deviceDesc.maxPacketSize0, sizeof(usbConfigDesc)))
    {
      dev->configDesc = kernelMalloc(tmpConfigDesc->totalLength);
      if (dev->configDesc == NULL)
	{
	  status = ERR_MEMORY;
	  goto err_out;
	}

      kernelDebug(debug_usb, "Get full first configuration for new device %d",
		  dev->address);
      status = controlTransfer(usb, dev, 0, USB_GET_DESCRIPTOR,
			       (USB_DESCTYPE_CONFIG << 8), 0,
			       tmpConfigDesc->totalLength, dev->configDesc,
			       &bytes);
      if ((status < 0) && (bytes < tmpConfigDesc->totalLength))
	goto err_out;

      kernelFree(tmpConfigDesc);
      tmpConfigDesc = NULL;
    }
  else
    {
      dev->configDesc = tmpConfigDesc;
      tmpConfigDesc = NULL;
    }

  debugConfigDesc(dev->configDesc);

  // Set the configuration
  kernelDebug(debug_usb, "Set configuration for new device %d", dev->address);
  status = controlTransfer(usb, dev, 0, USB_SET_CONFIGURATION,
			   dev->configDesc->confValue, 0, 0, NULL, NULL);
  if (status < 0)
    goto err_out;

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
  usb->addressCounter += 1;

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

 err_out:

  if (tmpConfigDesc)
    kernelFree(tmpConfigDesc);

  if (dev->configDesc)
    kernelFree(dev->configDesc);

  if (dev)
    kernelFree((void *) dev);

  return (status);
}


static void enumerateDisconnectedDevice(usbRootHub *usb, int port)
{
  // If the port status(es) indicate that a device has disconnected, figure
  // out which one it is and remove it from the root hub's list

  usbDevice *dev = NULL;
  usbClass *class = NULL;
  usbSubClass *subClass = NULL;
  int count1, count2;

  // Try a 'get device descriptor' command for each device
  for (count1 = 0; count1 < usb->numDevices; )
    if ((usb->devices[count1]->controller == usb->controller) &&
	(usb->devices[count1]->port == port))
      {
	dev = usb->devices[count1];

	kernelDebug(debug_usb, "Device %d disconnected", dev->address);

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
	kernelFree((void *) dev);
      }
}


static int setup(usbRootHub *usb)
{
  // Do USB setup

  int status = 0;
  unsigned char command = 0;
  usbUhciData *uhciData = NULL;
  unsigned intrPhysical = 0;
  unsigned controlPhysical = 0;
  unsigned bulkPhysical = 0;
  unsigned termPhysical = 0;
  unsigned termTdPhysical = 0;
  unsigned delayEnd = 0;
  int count;

  // Reset the controller
  reset(usb);

  // Make sure we've waited 1 second between stopped and running
  delayEnd = (kernelSysTimerRead() + 20);

  // Disable all interrupts
  kernelProcessorOutPort16((usb->ioAddress + USBUHCI_PORTOFFSET_INTR), 0);

  // Allocate memory
  status = allocUhciMemory(usb);
  if (status < 0)
    return (status);

  uhciData = usb->data;

  // Set up the queues

  intrPhysical = (unsigned)
    kernelPageGetPhysical(KERNELPROCID, (void *) uhciData->intrQueueHead);
  controlPhysical = (unsigned)
    kernelPageGetPhysical(KERNELPROCID, (void *) uhciData->controlQueueHead);
  bulkPhysical = (unsigned)
    kernelPageGetPhysical(KERNELPROCID, (void *) uhciData->bulkQueueHead);
  termPhysical = (unsigned)
    kernelPageGetPhysical(KERNELPROCID, (void *) uhciData->termQueueHead);
  termTdPhysical = (unsigned)
    kernelPageGetPhysical(KERNELPROCID, (void *) uhciData->termTransDesc);

  // Interrupt queue head points to control queue head
  uhciData->intrQueueHead->linkPointer =
    (controlPhysical | USBUHCI_LINKPTR_QHEAD);
  // Control queue head points to bulk queue head
  uhciData->controlQueueHead->linkPointer =
    (bulkPhysical | USBUHCI_LINKPTR_QHEAD);
  // Bulk queue head points to terminating queue head
  uhciData->bulkQueueHead->linkPointer =
    (termPhysical | USBUHCI_LINKPTR_QHEAD);
  // Terminating queue head points back to control queue head for bandwidth
  // reclamation
  uhciData->termQueueHead->linkPointer =
    (controlPhysical | USBUHCI_LINKPTR_QHEAD);

  // Attach the terminating transfer descriptor to the terminating queue head
  uhciData->termQueueHead->element = termTdPhysical;
  uhciData->termTransDesc->linkPointer = USBUHCI_LINKPTR_TERM;

  // Point all frame list pointers at the interrupt queue head
  for (count = 0; count < 1024; count ++)
    uhciData->frameList[count] = (intrPhysical | USBUHCI_LINKPTR_QHEAD);

  // Put the physical address of the frame list into the frame list base
  // address register
  kernelProcessorOutPort32((usb->ioAddress + USBUHCI_PORTOFFSET_FLBASE),
			   uhciData->frameListPhysical);

  command = readCommand(usb);
  // Clear: software debug
  command &= ~USBUHCI_CMD_SWDBG;
  // Set: max packet size to 64 bytes, configure flag
  command |= (USBUHCI_CMD_MAXP | USBUHCI_CMD_CF);
  writeCommand(usb, command);

  // Clear the frame number
  writeFrameNum(usb, 0);

  // Until we've waited 1 second between stopped and running
  while (kernelSysTimerRead() < delayEnd);

  // Start the controller
  startStop(usb, 1);

  return (status = 0);
}


static void threadCall(usbRootHub *usb)
{
  unsigned short status = 0;
  int count;

  for (count = 0; count < 2; count ++)
    {
      status = readPortStatus(usb, count);

      if (status & (USBUHCI_PORT_ENABCHG | USBUHCI_PORT_CONNCHG))
	{
	  printPortStatus(usb);

	  // Write the status back to reset the 'changed' bits.
	  writePortStatus(usb, count, status);

	  if (status & USBUHCI_PORT_CONNCHG)
	    {
	      if (status & USBUHCI_PORT_CONNSTAT)
		{
		  // Something connected, so wait 100ms
		  kernelDebug(debug_usb, "Delay after port status change");
		  delay(usb, 100);

		  // If the device is low-speed, we don't support it.
		  if (status & USBUHCI_PORT_LSDA)
		    debugError("Low-speed devices not supported");
		  else
		    enumerateNewDevice(usb, count);

		  kernelDebug(debug_usb, "Port %d is connected", count);
		}
	      else
		{
		  enumerateDisconnectedDevice(usb, count);
		  kernelDebug(debug_usb, "Port %d is disconnected", count);
		}
	    }
		    
	  printPortStatus(usb);
	}
    }
}


kernelDevice *kernelUsbUhciDetect(kernelDevice *parent,
				  kernelBusTarget *busTarget,
				  kernelDriver *driver)
{
  // This routine is used to detect and initialize each device, as well as
  // registering each one with any higher-level interfaces.

  int status = 0;
  pciDeviceInfo pciDevInfo;
  kernelDevice *dev = NULL;
  usbRootHub *usb = NULL;
  const char *headerType = NULL;

  // Get the PCI device header
  status = kernelBusGetTargetInfo(bus_pci, busTarget->target, &pciDevInfo);
  if (status < 0)
    goto err_out;

  // Make sure it's a UHCI controller (programming interface is 0 in the
  // PCI header)
  if (pciDevInfo.device.progIF != 0)
    goto err_out;

  // After this point, we believe we have a supported device.

  // Enable the device on the PCI bus as a bus master
  if ((kernelBusDeviceEnable(bus_pci, busTarget->target, 1) < 0) ||
      (kernelBusSetMaster(bus_pci, busTarget->target, 1) < 0))
    goto err_out;

  // Allocate memory for the device
  dev = kernelMalloc(sizeof(kernelDevice));
  if (dev == NULL)
    goto err_out;

  usb = kernelMalloc(sizeof(usbRootHub));
  if (usb == NULL)
    goto err_out;

  // Get the USB version number
  usb->usbVersion = kernelBusReadRegister(bus_pci, busTarget->target, 0x60, 8);
  kernelLog("USB: UHCI bus version %d.%d", ((usb->usbVersion & 0xF0) >> 4),
	    (usb->usbVersion & 0xF));

  // Don't care about the 'multi-function' bit in the header type
  if (pciDevInfo.device.headerType & PCI_HEADERTYPE_MULTIFUNC)
    pciDevInfo.device.headerType &= ~PCI_HEADERTYPE_MULTIFUNC;

  // Get the interrupt line
  if (pciDevInfo.device.headerType == PCI_HEADERTYPE_NORMAL)
    {
      usb->interrupt = (int) pciDevInfo.device.nonBridge.interruptLine;
      headerType = "normal";
    }
  else if (pciDevInfo.device.headerType == PCI_HEADERTYPE_BRIDGE)
    {
      usb->interrupt = (int) pciDevInfo.device.bridge.interruptLine;
      headerType = "bridge";
    }
  else if (pciDevInfo.device.headerType == PCI_HEADERTYPE_CARDBUS)
    {
      usb->interrupt = (int) pciDevInfo.device.cardBus.interruptLine;
      headerType = "cardbus";
    }
  else
    {
      debugError("UHCI: Unsupported USB controller header type %d",
		 pciDevInfo.device.headerType);
      goto err_out;
    }
  kernelDebug(debug_usb, "UHCI controller PCI type: %s", headerType);

  // Get the I/O space base address.  For USB, it comes in the 5th
  // PCI base address register
  usb->ioAddress = (void *)
    (kernelBusReadRegister(bus_pci, busTarget->target, 0x08, 32) & 0xFFFFFFE0);

  if (usb->ioAddress == NULL)
    {
      debugError("UHCI: Unknown USB controller I/O address");
      goto err_out;
    }

  // Disable legacy support
  kernelBusWriteRegister(bus_pci, busTarget->target, 0x60, 16, 0x2000);

  // Set up the controller
  status = setup(usb);
  if (status < 0)
    {
      kernelError(kernel_error, "Error setting up USB operation");
      goto err_out;
    }

  usb->reset = &reset;
  usb->threadCall = &threadCall;
  usb->transaction = &transaction;

  // Create the USB kernel device
  dev->device.class = kernelDeviceGetClass(DEVICECLASS_BUS);
  dev->device.subClass = kernelDeviceGetClass(DEVICESUBCLASS_BUS_USB);
  dev->driver = driver;
  dev->data = (void *) usb;

  status = kernelDeviceAdd(parent, dev);
  if (status < 0)
    goto err_out;
  else 
    return (dev);

 err_out:

  if (dev)
    kernelFree(dev);

  if (usb)
    {
      if (usb->data)
	deallocUhciMemory(usb);
      kernelFree((void *) usb);
    }

  return (dev = NULL);
}
