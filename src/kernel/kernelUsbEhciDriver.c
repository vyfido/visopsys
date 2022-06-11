//
//  Visopsys
//  Copyright (C) 1998-2014 J. Andrew McLaughlin
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
//  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
//
//  kernelUsbEhciDriver.c
//

#include "kernelUsbEhciDriver.h"
#include "kernelCpu.h"
#include "kernelDebug.h"
#include "kernelError.h"
#include "kernelLog.h"
#include "kernelMalloc.h"
#include "kernelMisc.h"
#include "kernelMemory.h"
#include "kernelPage.h"
#include "kernelParameters.h"
#include "kernelPciDriver.h"
#include "kernelProcessorX86.h"
#include "kernelSysTimer.h"
#include "kernelVariableList.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#ifdef DEBUG
static inline void debugEhciCapRegs(usbController *controller)
{
	usbEhciData *ehciData = controller->data;

	kernelDebug(debug_usb, "EHCI capability registers:\n"
		"    capslen=0x%02x\n"
		"    hciver=0x%04x\n"
		"    hcsparams=0x%08x\n"
		"    hccparams=0x%08x\n"
		"    hcsp_portroute=0x%llx (%svalid)",
		ehciData->capRegs->capslen, ehciData->capRegs->hciver,
		ehciData->capRegs->hcsparams, ehciData->capRegs->hccparams,
		ehciData->capRegs->hcsp_portroute,
		((ehciData->capRegs->hcsparams & USBEHCI_HCSP_PORTRTERULES)?
			"" : "in"));
}

static inline void debugEhciHcsParams(usbController *controller)
{
	usbEhciData *ehciData = controller->data;

	kernelDebug(debug_usb, "EHCI HCSParams register:\n"
		"    debug port=%d\n"
		"    port indicators=%d\n"
		"    num companion controllers=%d\n"
		"    ports per companion=%d\n"
		"    port routing rules=%d\n"
		"    port power control=%d\n"
		"    num ports=%d",
		((ehciData->capRegs->hcsparams &
			USBEHCI_HCSP_DEBUGPORT) >> 20),
		((ehciData->capRegs->hcsparams &
			USBEHCI_HCSP_PORTINICATORS) >> 16),
		((ehciData->capRegs->hcsparams &
			USBEHCI_HCSP_NUMCOMPANIONS) >> 12),
		((ehciData->capRegs->hcsparams &
			USBEHCI_HCSP_PORTSPERCOMP) >> 8),
		((ehciData->capRegs->hcsparams &
			USBEHCI_HCSP_PORTRTERULES) >> 7),
		((ehciData->capRegs->hcsparams &
			USBEHCI_HCSP_PORTPOWERCTRL) >> 4),
		(ehciData->capRegs->hcsparams & USBEHCI_HCSP_NUMPORTS));
}

static inline void debugEhciHccParams(usbController *controller)
{
	usbEhciData *ehciData = controller->data;

	kernelDebug(debug_usb, "EHCI HCCParams register:\n"
		"    extended caps ptr=0x%02x\n"
		"    isoc schedule threshold=0x%x\n"
		"    async schedule park=%d\n"
		"    programmable frame list=%d\n"
		"    64-bit addressing=%d",
		((ehciData->capRegs->hccparams &
			USBEHCI_HCCP_EXTCAPPTR) >> 8),
		((ehciData->capRegs->hccparams &
			USBEHCI_HCCP_ISOCSCHDTHRES) >> 4),
		((ehciData->capRegs->hccparams &
			USBEHCI_HCCP_ASYNCSCHDPARK) >> 2),
		((ehciData->capRegs->hccparams &
			USBEHCI_HCCP_PROGFRAMELIST) >> 1),
		(ehciData->capRegs->hccparams & USBEHCI_HCCP_ADDR64));
}

static void debugEhciOpRegs(usbController *controller)
{
	usbEhciData *ehciData = controller->data;
	int numPorts = (ehciData->capRegs->hcsparams & USBEHCI_HCSP_NUMPORTS);
	char portsStatCtl[1024];
	int count;

	// Read the port status/control registers
	portsStatCtl[0] = '\0';
	for (count = 0; count < numPorts; count ++)
	{
		sprintf((portsStatCtl + strlen(portsStatCtl)), "\n    portsc%d=0x%08x",
			(count + 1), ehciData->opRegs->portsc[count]);
	}

	kernelDebug(debug_usb, "EHCI operational registers:\n"
		"    cmd=0x%08x\n"
		"    stat=0x%08x\n"
		"    intr=0x%08x\n"
		"    frindex=0x%08x\n"
		"    ctrldsseg=0x%08x\n"
		"    perlstbase=0x%08x\n"
		"    asynclstaddr=0x%08x\n"
		"    configflag=0x%08x%s",
		ehciData->opRegs->cmd, ehciData->opRegs->stat,
		ehciData->opRegs->intr, ehciData->opRegs->frindex,
		ehciData->opRegs->ctrldsseg, ehciData->opRegs->perlstbase,
		ehciData->opRegs->asynclstaddr, ehciData->opRegs->configflag,
		portsStatCtl);
}

static inline void debugPortStatus(usbController *controller, int portNum)
{
	usbEhciData *ehciData = controller->data;

	kernelDebug(debug_usb, "EHCI controller %d, port %d: 0x%08x", controller->num,
		portNum, ehciData->opRegs->portsc[portNum]);
}

static inline void debugQtd(ehciQtd *qtd)
{
	kernelDebug(debug_usb, "EHCI qTD (%p):\n"
		"    nextQtd=0x%08x\n"
		"    altNextQtd=0x%08x\n"
		"    token=0x%08x\n"
		"        dataToggle=%d\n"
		"        totalBytes=%d\n"
		"        interruptOnComplete=%d\n"
		"        currentPage=%d\n"
		"        errorCounter=%d\n"
		"        pidCode=%d\n"
		"        status=0x%02x\n"
		"    buffer0=0x%08x\n"
		"    buffer1=0x%08x\n"
		"    buffer2=0x%08x\n"
		"    buffer3=0x%08x\n"
		"    buffer4=0x%08x",
		kernelPageGetPhysical((((unsigned) qtd < KERNEL_VIRTUAL_ADDRESS)?
			kernelCurrentProcess->processId :
				KERNELPROCID), (void *) qtd),
		qtd->nextQtd, qtd->altNextQtd, qtd->token,
		((qtd->token & USBEHCI_QTDTOKEN_DATATOGG) >> 31),
		((qtd->token & USBEHCI_QTDTOKEN_TOTBYTES) >> 16),
		((qtd->token & USBEHCI_QTDTOKEN_IOC) >> 15),
		((qtd->token & USBEHCI_QTDTOKEN_CURRPAGE) >> 12),
		((qtd->token & USBEHCI_QTDTOKEN_ERRCOUNT) >> 10),
		((qtd->token & USBEHCI_QTDTOKEN_PID) >> 8),
		(qtd->token & USBEHCI_QTDTOKEN_STATMASK),
		qtd->buffPage[0], qtd->buffPage[1], qtd->buffPage[2],
		qtd->buffPage[3], qtd->buffPage[4]);
}

static inline void debugQueueHead(ehciQueueHead *queueHead)
{
	kernelDebug(debug_usb, "EHCI queue head (%p):\n"
		"    horizLink=0x%08x\n"
		"    endpointChars=0x%08x\n"
		"        nakCountReload=%d\n"
		"        controlEndpoint=%d\n"
		"        maxPacketLen=%d\n"
		"        reclListHead=%d\n"
		"        dataToggleCntl=%d\n"
		"        endpointSpeed=%d\n"
		"        endpointNum=%02x\n"
		"        inactivateOnNext=%d\n"
		"        deviceAddress=%d\n"
		"    endpointCaps=0x%08x\n"
		"        hiBandMult=%d\n"
		"        portNumber=%d\n"
		"        hubAddress=%d\n"
		"        splitCompMask=0x%02x\n"
		"        intSchedMask=0x%02x\n"
		"    currentQtd=%08x",
		kernelPageGetPhysical((((unsigned) queueHead <
			KERNEL_VIRTUAL_ADDRESS)?
				kernelCurrentProcess->processId :
					KERNELPROCID), (void *) queueHead),
		queueHead->horizLink, queueHead->endpointChars,
		((queueHead->endpointChars & USBEHCI_QHDW1_NAKCNTRELOAD) >> 28),
		((queueHead->endpointChars & USBEHCI_QHDW1_CTRLENDPOINT) >> 27),
		((queueHead->endpointChars & USBEHCI_QHDW1_MAXPACKETLEN) >> 16),
		((queueHead->endpointChars & USBEHCI_QHDW1_RECLLISTHEAD) >> 15),
		((queueHead->endpointChars & USBEHCI_QHDW1_DATATOGGCTRL) >> 14),
		((queueHead->endpointChars & USBEHCI_QHDW1_ENDPTSPEED) >> 12),
		((queueHead->endpointChars & USBEHCI_QHDW1_ENDPOINT) >> 8),
		((queueHead->endpointChars & USBEHCI_QHDW1_INACTONNEXT) >> 7),
		(queueHead->endpointChars & USBEHCI_QHDW1_DEVADDRESS),
		queueHead->endpointCaps,
		((queueHead->endpointCaps & USBEHCI_QHDW2_HISPEEDMULT) >> 30),
		((queueHead->endpointCaps & USBEHCI_QHDW2_PORTNUMBER) >> 23),
		((queueHead->endpointCaps & USBEHCI_QHDW2_HUBADDRESS) >> 16),
		((queueHead->endpointCaps & USBEHCI_QHDW2_SPLTCOMPMASK) >> 8),
		(queueHead->endpointCaps & USBEHCI_QHDW2_INTSCHEDMASK),	       
		queueHead->currentQtd);
	debugQtd(&(queueHead->overlay));
}

static void debugTransError(ehciQtd *qtd)
{
	char *errorText = NULL;
	char *transString = NULL;

	errorText = kernelMalloc(MAXSTRINGLENGTH);
	if (errorText)
	{
		switch (qtd->token & USBEHCI_QTDTOKEN_PID)
		{
			case USBEHCI_QTDTOKEN_PID_SETUP:
				transString = "SETUP";
				break;
			case USBEHCI_QTDTOKEN_PID_IN:
				transString = "IN";
				break;
			case USBEHCI_QTDTOKEN_PID_OUT:
				transString = "OUT";
				break;
		}
		sprintf(errorText, "EHCI: Trans desc %s: ", transString);

		if (!(qtd->token & USBEHCI_QTDTOKEN_ERROR))
			strcat(errorText, "no error, ");

		if (qtd->token & USBEHCI_QTDTOKEN_ERRHALT)
			strcat(errorText, "halted, ");
		if (qtd->token & USBEHCI_QTDTOKEN_ERRDATBUF)
			strcat(errorText, "data buffer error, ");
		if (qtd->token & USBEHCI_QTDTOKEN_ERRBABBLE)
			strcat(errorText, "babble, ");
		if (qtd->token & USBEHCI_QTDTOKEN_ERRXACT)
			strcat(errorText, "transaction error, ");
		if (qtd->token & USBEHCI_QTDTOKEN_ERRMISSMF)
			strcat(errorText, "missed micro-frame, ");

		if (qtd->token & USBEHCI_QTDTOKEN_ACTIVE)
			strcat(errorText, "TD is still active");
		else
			strcat(errorText, "finished");

		kernelDebugError("%s", errorText);
		kernelFree(errorText);
	}
}
#else
	#define debugEhciCapRegs(usb) do { } while (0)
	#define debugEhciHcsParams(usb) do { } while (0)
	#define debugEhciHccParams(usb) do { } while (0)
	#define debugEhciOpRegs(usb) do { } while (0)
	#define debugPortStatus(usb, num) do { } while (0)
	#define debugQtd(qtd) do { } while (0)
	#define debugQueueHead(queueHead) do { } while (0)
	#define debugTransError(qtd) do { } while (0)
#endif // DEBUG


static ehciQueueHeadItem *findQueueHead(usbController *controller,
	usbDevice *usbDev, unsigned char endpoint)
{
	// Search the controller's list of used queue heads for one that belongs
	// to the requested device and endpoint.

	usbEhciData *ehciData = controller->data;
	kernelLinkedList *usedList =
		(kernelLinkedList *) &(ehciData->usedQueueHeadItems);
	ehciQueueHeadItem *queueHeadItem = NULL;
	kernelLinkedListItem *iter = NULL;

	kernelDebug(debug_usb, "EHCI find queue head for controller %d, usbDev %p, "
		"endpoint %02x", controller->num, usbDev, endpoint);

	// Try searching for an existing queue head
	if (usedList->numItems)
	{
		queueHeadItem = kernelLinkedListIterStart(usedList, &iter);

		while (queueHeadItem)
		{
			kernelDebug(debug_usb, "EHCI examine queue head for device %p "
				"endpoint %02x", queueHeadItem->usbDev,
				queueHeadItem->endpoint);
			if ((queueHeadItem->usbDev == usbDev) &&
				(queueHeadItem->endpoint == endpoint))
				break;
			else
				queueHeadItem = kernelLinkedListIterNext(usedList, &iter);
		}

		// Found it?
		if (queueHeadItem)
			kernelDebug(debug_usb, "EHCI found queue head");
		else
			kernelDebug(debug_usb, "EHCI queue head not found");
	}
	else
		kernelDebug(debug_usb, "EHCI no items in queue head list");

	return (queueHeadItem);
}


static void freeIoMem(void *virtual, void *physical, unsigned size)
{
	// Attempt to unmap and free any memory allocated using the allocIoMem()
	// function, below

	if (virtual)
		kernelPageUnmap(KERNELPROCID, virtual, size);

	if (physical)
		kernelMemoryReleasePhysical(physical);
}


static int allocIoMem(unsigned size, unsigned alignment, void **virtual,
	void **physical)
{
	// Use this to allocate kernel-owned memory when we need to
	// a) align it;
	// b) know both the physical and virtual addresses; and
	// c) make it non-cacheable

	int status = 0;

	// Request memory for an aligned array of TRBs
	*physical = kernelMemoryGetPhysical(size, alignment, "usb i/o memory");
	if (*physical == NULL)
	{
		status = ERR_MEMORY;
		goto err_out;
	}

	// Map the physical memory into virtual memory
	status = kernelPageMapToFree(KERNELPROCID, *physical, virtual, size);
	if (status < 0)
		goto err_out; 

	// Make it non-cacheable
	status = kernelPageSetAttrs(KERNELPROCID, 1 /* set */,
		PAGEFLAG_CACHEDISABLE, *virtual, size);
	if (status < 0)
		goto err_out;

	// Clear it out, as the kernelMemoryGetPhysical function can't do that
	// for us
	kernelMemClear(*virtual, size);

	return (status = 0);

err_out:

	freeIoMem(*virtual, *physical, size);

	return (status);
}


static int allocQueueHeads(kernelLinkedList *freeList)
{
	// Allocate a page worth of physical memory for ehciQueueHead data
	// structures, allocate an equal number of ehciQueueHeadItem
	// structures to point at them, link them together, and add them to the
	// supplied kernelLinkedList.

	int status = 0;
	void *physicalMem = NULL;
	ehciQueueHead *queueHeads = NULL;
	int numQueueHeads = 0;
	ehciQueueHeadItem *queueHeadItems = NULL;
	unsigned physicalAddr = 0;
	int count;

	kernelDebug(debug_usb, "EHCI adding queue heads to free list");

	// Request an aligned page of I/O memory (we need to be sure of 32-byte
	// alignment for each queue head)
	status = allocIoMem(MEMORY_PAGE_SIZE, MEMORY_PAGE_SIZE,
		(void **) &queueHeads, &physicalMem);
	if (status < 0)
		goto err_out;

	// How many queue heads per memory page?  The ehciQueueHead data structure
	// will be padded so that they start on 32-byte boundaries, as required by
	// the hardware
	numQueueHeads = (MEMORY_PAGE_SIZE / sizeof(ehciQueueHead));

	// Get memory for ehciQueueHeadItem structures
	queueHeadItems = kernelMalloc(numQueueHeads * sizeof(ehciQueueHeadItem));
	if (queueHeadItems == NULL)
	{
		status = ERR_MEMORY;
		goto err_out;
	}

	// Now loop through each list item, link it to a queue head, and add it
	// to the free list
	physicalAddr = (unsigned) physicalMem;
	for (count = 0; count < numQueueHeads; count ++)
	{
		queueHeadItems[count].queueHead = &(queueHeads[count]);
		queueHeadItems[count].physicalAddr = physicalAddr;
		physicalAddr += sizeof(ehciQueueHead);

		status = kernelLinkedListAdd(freeList, &(queueHeadItems[count]));
		if (status < 0)
		{
			kernelError(kernel_error, "Couldn't add new queue heads to free "
				"list");
			goto err_out;
		}
	}

	kernelDebug(debug_usb, "EHCI added %d queue heads", numQueueHeads);
	return (status = 0);

err_out:

	if (queueHeadItems)
		kernelFree(queueHeadItems);
	if (queueHeads && physicalMem)
		freeIoMem((void *) queueHeads, physicalMem, MEMORY_PAGE_SIZE);

	return (status);
}


static int setQueueHeadEndpointState(usbDevice *usbDev,
	unsigned char endpointNum, ehciQueueHead *queueHead)
{
	// Given a usbDevice structure and an endpoint number, set all the relevent
	// "static endpoint state" fields in the queue head

	int status = 0;
	int maxPacketLen = 0;
	usbEndpointDesc *endpointDesc = NULL;

	kernelDebug(debug_usb, "EHCI set queue head endpoint state for %s speed "
		"device %u, endpoint %02x", usbDevSpeed2String(usbDev->speed),
		usbDev->address, endpointNum);

	// Max NAK retries, we guess
	queueHead->endpointChars = USBEHCI_QHDW1_NAKCNTRELOAD;

	// If this is not a high speed device, and we're talking to the control
	// endpoint, set this to 1
	if ((usbDev->speed != usbspeed_high) && !endpointNum)
		queueHead->endpointChars |= USBEHCI_QHDW1_CTRLENDPOINT;

	// Figure out the maximum number of bytes per transfer, depending on the
	// endpoint we're addressing.
	if ((endpointNum == 0) && !usbDev->numEndpoints)
		maxPacketLen = usbDev->deviceDesc.maxPacketSize0;
	else
	{
		endpointDesc = kernelUsbGetEndpointDesc(usbDev, endpointNum);
		if (endpointDesc == NULL)
		{
			kernelError(kernel_error, "No such endpoint %02x", endpointNum);
			return (status = ERR_NOSUCHFUNCTION);
		}

		maxPacketLen = endpointDesc->maxPacketSize;
	}

	// If we haven't yet got the descriptors, etc., use 8 as the maximum size
	if (!maxPacketLen)
	{
		kernelDebug(debug_usb, "EHCI using default maximum endpoint transfer "
			"size 8 for endpoint %02x", endpointNum);
		maxPacketLen = 8;
	}

	// Set the maximum endpoint packet length
	queueHead->endpointChars |=
		((maxPacketLen << 16) & USBEHCI_QHDW1_MAXPACKETLEN);

	// Tell the controller to get the data toggle from the qTDs
	queueHead->endpointChars |= USBEHCI_QHDW1_DATATOGGCTRL;

	// Mark the speed of the device
	switch (usbDev->speed)
	{
		case usbspeed_high:
		default:
			queueHead->endpointChars |= USBEHCI_QHDW1_ENDPTSPDHIGH;
			break;
		case usbspeed_full:
			queueHead->endpointChars |= USBEHCI_QHDW1_ENDPTSPDFULL;
			break;
		case usbspeed_low:
			queueHead->endpointChars |= USBEHCI_QHDW1_ENDPTSPDLOW;
			break;
	}

	// The endpoint number
	queueHead->endpointChars |= ((endpointNum << 8) & USBEHCI_QHDW1_ENDPOINT);

	// The device address
	queueHead->endpointChars |= (usbDev->address & USBEHCI_QHDW1_DEVADDRESS);

	// Assume minimum speed multiplier for now
	queueHead->endpointCaps = USBEHCI_QHDW2_HISPEEDMULT1;

	if (usbDev->speed != usbspeed_high)
	{
		// Port number, hub address, and split completion mask only set for a
		// full- or low-speed device

		kernelDebugError("Non-high-speed devices are not yet supported");

		queueHead->endpointCaps |=
			(((usbDev->port + 1) << 23) & USBEHCI_QHDW2_PORTNUMBER);

		// Root hubs don't have constituent USB devices
		if (usbDev->hub->usbDev)
		{
			kernelDebug(debug_usb, "EHCI using hub address %d",
				usbDev->hub->usbDev->address);
			queueHead->endpointCaps |= ((usbDev->hub->usbDev->address << 16) &
				USBEHCI_QHDW2_HUBADDRESS);
		}
		else
			kernelDebug(debug_usb, "EHCI not using hub address - connected to "
				"root hub");
	}

	return (status = 0);
}


static int startStopSched(usbController *controller, unsigned statBit,
	unsigned cmdBit, int start)
{
	// Start or stop the processing of a schedule

	int status = 0;
	const char *schedName = NULL;
	usbEhciData *ehciData = controller->data;
	int count;

	switch (statBit)
	{
		case USBEHCI_STAT_ASYNCSCHED:
			schedName = "asynchronous";
			break;
		case USBEHCI_STAT_PERIODICSCHED:
			schedName = "periodic";
			break;
	}

	kernelDebug(debug_usb, "EHCI st%s %s processing", (start? "art" : "op"),
		schedName);

	if (start)
	{
		if (!(ehciData->opRegs->stat & statBit))
		{
			// Start schedule processing
			ehciData->opRegs->cmd |= cmdBit;

			// Wait for it to be started
			for (count = 0; count < 20; count ++)
			{
				if (ehciData->opRegs->stat & statBit)
				{
					kernelDebug(debug_usb, "EHCI starting %s schedule took "
						"%dms", schedName, count);
					break;
				}

				kernelCpuSpinMs(1);
			}

			// Started?
			if (!(ehciData->opRegs->stat & statBit))
			{
				kernelError(kernel_error, "Couldn't enable %s schedule",
					schedName);
				return (status = ERR_TIMEOUT);
			}
		}
	}
	else
	{
		if (ehciData->opRegs->stat & statBit)
		{
			// Stop schedule processing
			ehciData->opRegs->cmd &= ~cmdBit;

			// Wait for it to be stopped
			for (count = 0; count < 20; count ++)
			{
				if (!(ehciData->opRegs->stat & statBit))
				{
					kernelDebug(debug_usb, "EHCI stopping %s schedule took "
						"%dms", schedName, count);
					break;
				}

				kernelCpuSpinMs(1);
			}

			// Stopped?
			if (ehciData->opRegs->stat & statBit)
			{
				kernelError(kernel_error, "Couldn't disable %s schedule",
					schedName);
				return (status = ERR_TIMEOUT);
			}
		}
	}

	kernelDebug(debug_usb, "EHCI %s processing st%s", schedName,
		(start? "arted" : "opped"));

		return (status = 0);
}


static int releaseQueueHead(usbController *controller,
	ehciQueueHeadItem *queueHeadItem)
{
	// Remove the queue head from the list of 'used' queue heads, and add it
	// back into the list of 'free' queue heads.

	int status = 0;
	usbEhciData *ehciData = controller->data;
	kernelLinkedList *usedList =
		(kernelLinkedList *) &(ehciData->usedQueueHeadItems);
	kernelLinkedList *freeList =
		(kernelLinkedList *) &(ehciData->freeQueueHeadItems);

	// Remove it from the used list
	if (kernelLinkedListRemove(usedList, queueHeadItem) >= 0)
	{
		// Add it to the free list
		if (kernelLinkedListAdd(freeList, queueHeadItem) < 0)
			kernelError(kernel_warn, "Couldn't add item to queue head free "
				"list");
	}
	else
		kernelError(kernel_warn, "Couldn't remove item from queue head used "
			"list");

	return (status = 0);
}


static ehciQueueHeadItem *allocQueueHead(usbController *controller,
	usbDevice *usbDev, unsigned char endpoint)
{
	// Allocate the queue head for the device endpoint.
	//
	// Each device endpoint has at most one queue head (which may be linked
	// into either the synchronous or asynchronous queue, depending on the
	// endpoint type).
	//
	// It's OK for the usbDevice parameter to be NULL; the asynchronous list
	// will have a single, unused queue head to mark the start of the list.

	usbEhciData *ehciData = controller->data;
	kernelLinkedList *usedList =
		(kernelLinkedList *) &(ehciData->usedQueueHeadItems);
	kernelLinkedList *freeList =
		(kernelLinkedList *) &(ehciData->freeQueueHeadItems);
	ehciQueueHeadItem *queueHeadItem = NULL;
	kernelLinkedListItem *iter = NULL;

	kernelDebug(debug_usb, "EHCI alloc queue head for controller %d, "
		"usbDev %p, endpoint %02x", controller->num, usbDev, endpoint);

	// Anything in the free list?
	if (!freeList->numItems)
	{
		// Super, the free list is empty.  We need to allocate everything.
		if (allocQueueHeads(freeList) < 0)
		{
			kernelError(kernel_error, "Couldn't allocate new queue heads");
			goto err_out;
		}
	}

	// Grab the first item in the free list
	queueHeadItem = kernelLinkedListIterStart(freeList, &iter);
	if (queueHeadItem == NULL)
	{
		kernelError(kernel_error, "Couldn't get a list item for a new Queue "
			"Head");
		goto err_out;
	}

	// Remove it from the free list
	if (kernelLinkedListRemove(freeList, queueHeadItem) < 0)
	{
		kernelError(kernel_error, "Couldn't remove item from queue head free "
			"list");
		goto err_out;
	}

	// Initialize the queue head item
	queueHeadItem->usbDev = (void *) usbDev;
	queueHeadItem->endpoint = endpoint;
	queueHeadItem->firstQtdItem = NULL;

	// Initialize the queue head
	kernelMemClear((void *) queueHeadItem->queueHead, sizeof(ehciQueueHead));
	queueHeadItem->queueHead->horizLink = USBEHCI_LINK_TERM;
	queueHeadItem->queueHead->currentQtd  = USBEHCI_LINK_TERM;
	queueHeadItem->queueHead->overlay.nextQtd = USBEHCI_LINK_TERM;
	queueHeadItem->queueHead->overlay.altNextQtd = USBEHCI_LINK_TERM;

	// Add it to the used list
	if (kernelLinkedListAdd(usedList, queueHeadItem) < 0)
	{
		kernelError(kernel_error, "Couldn't add item to queue head used list");
		goto err_out;
	}

	kernelDebug(debug_usb, "EHCI added queue head for usbDev %p, endpoint %02x",
		queueHeadItem->usbDev, queueHeadItem->endpoint);

	if (usbDev)
	{
		// Set the "static endpoint state" in the queue head
		if (setQueueHeadEndpointState(usbDev, endpoint,
				queueHeadItem->queueHead) < 0)
			goto err_out;
	}

	// Return success
	return (queueHeadItem);

err_out:

	if (queueHeadItem)
		releaseQueueHead(controller, queueHeadItem);

	return (queueHeadItem = NULL);
}


static int unlinkQueueHead(usbController *controller,
	ehciQueueHeadItem *unlinkQueueHeadItem)
{
	// Search the controller's list of used queue heads for any that link to
	// the one supplied, and unlink them.

	int status = 0;
	usbEhciData *ehciData = controller->data;
	kernelLinkedList *usedList =
		(kernelLinkedList *) &(ehciData->usedQueueHeadItems);
	ehciQueueHeadItem *queueHeadItem = NULL;
	kernelLinkedListItem *iter = NULL;

	kernelDebug(debug_usb, "EHCI unlink queue head");

	// Try searching for a queue head that links to the one we're removing
	if (usedList->numItems)
	{
		queueHeadItem = kernelLinkedListIterStart(usedList, &iter);

		while (queueHeadItem)
		{
			if ((queueHeadItem->queueHead->horizLink & ~USBEHCI_LINKTYP_MASK) ==
				unlinkQueueHeadItem->physicalAddr)
			{
				// This one links to the one we're removing
				kernelDebug(debug_usb, "EHCI found linking queue head");

				// Replace the horizontal link pointer with whatever the one
				// we're removing points to
				queueHeadItem->queueHead->horizLink =
					unlinkQueueHeadItem->queueHead->horizLink;

				// Finished
				return (status = 0);
			}

			queueHeadItem = kernelLinkedListIterNext(usedList, &iter);
		}

		// Not found
		kernelDebugError("No such item in queue head list");
		return (status = ERR_NOSUCHENTRY);
	}
	else
	{
		kernelDebugError("No items in queue head list");
		return (status = ERR_NOSUCHENTRY);
	}
}


static void setStatusBits(usbController *controller, unsigned bits)
{
	// Set the requested status bits, without affecting the read-only and
	// read-write-clear bits (can also be used to clear read-write-clear bits)

	usbEhciData *ehciData = controller->data;

	ehciData->opRegs->stat =
		((ehciData->opRegs->stat &
			~(USBEHCI_STAT_ROMASK | USBEHCI_STAT_RWCMASK)) | bits);
}


static int removeAsyncQueueHead(usbController *controller,
	ehciQueueHeadItem *queueHeadItem)
{
	// Remove the queue head from the asynchronous queue, and release it.

	int status = 0;
	usbEhciData *ehciData = controller->data;
	int count;

	// Unlink the queue head from the queue
	status = unlinkQueueHead(controller, queueHeadItem);
	if (status < 0)
		return (status);

	// Now we set the 'interrupt on async advance doorbell' but in the command
	// register
	ehciData->opRegs->cmd |= USBEHCI_CMD_INTASYNCADVRST;

	// Wait for the controller to set the 'interrupt on async advance' bit
	// in the status register
	kernelDebug(debug_usb, "EHCI wait for async advance");
	for (count = 0; count < 20; count ++)
	{
		if (ehciData->opRegs->stat & USBEHCI_STAT_ASYNCADVANCE)
		{
			kernelDebug(debug_usb, "EHCI async advance took %dms", count);
			break;
		}

		kernelCpuSpinMs(1);
	}

	// Did the controller respond?
	if (!(ehciData->opRegs->stat & USBEHCI_STAT_ASYNCADVANCE))
	{
		kernelError(kernel_error, "Controller did not set async advance bit");
		return (status = ERR_TIMEOUT);
	}	

	// Clear it
	setStatusBits(controller, USBEHCI_STAT_ASYNCADVANCE);

	status = releaseQueueHead(controller, queueHeadItem);
	if (status < 0)
		return (status);

	return (status = 0);
}


static ehciQueueHeadItem *allocAsyncQueueHead(usbController *controller,
	usbDevice *usbDev, unsigned char endpoint)
{
	// Allocate a queue head for the asynchronous queue, insert it, and make
	// sure the controller is processing them.

	usbEhciData *ehciData = controller->data;
	ehciQueueHeadItem *queueHeadItem = NULL;

	queueHeadItem = allocQueueHead(controller, usbDev, endpoint);
	if (queueHeadItem == NULL)
	{
		kernelError(kernel_error, "Couldn't allocate asynchronous queue head");
		goto err_out;
	}

	kernelDebug(debug_usb, "EHCI inserting queue head into asynchronous "
		"schedule");

	// Insert it into the asynchronous queue
	queueHeadItem->queueHead->horizLink =
		ehciData->reclaimHead->queueHead->horizLink;
	ehciData->reclaimHead->queueHead->horizLink =
		(queueHeadItem->physicalAddr | USBEHCI_LINKTYP_QH);

	// If the asynchronous schedule is not running, start it now
	if (!(ehciData->opRegs->stat & USBEHCI_STAT_ASYNCSCHED))
	{
		// Seems like sometimes this register gets corrupted by something after
		// our initial setup - starting the controller?
		ehciData->opRegs->asynclstaddr = ehciData->reclaimHead->physicalAddr;

		if (startStopSched(controller, USBEHCI_STAT_ASYNCSCHED,
			USBEHCI_CMD_ASYNCSCHEDENBL, 1) < 0)
		{
			goto err_out;
		}
	}

	// Return success
	return (queueHeadItem);

err_out:

	if (queueHeadItem)
		releaseQueueHead(controller, queueHeadItem);

	return (queueHeadItem = NULL);
}


static ehciQueueHeadItem *allocIntrQueueHead(usbController *controller,
	usbDevice *usbDev, unsigned char endpoint)
{
	// Allocate a queue head for the periodic queue, insert it, and make
	// sure the controller is processing them.

	usbEhciData *ehciData = controller->data;
	ehciQueueHeadItem *queueHeadItem = NULL;

	queueHeadItem = allocQueueHead(controller, usbDev, endpoint);
	if (queueHeadItem == NULL)
	{
		kernelError(kernel_error, "Couldn't allocate interrupt queue head");
		goto err_out;
	}

	if (usbDev->speed != usbspeed_high)
	{
		// Port number, hub address, and split completion mask only set for a
		// full- or low-speed device

		kernelDebugError("Non-high-speed devices are not yet supported");

		// For now, set all bits in the split completion mask, and leave the
		// interrupt schedule mask empty (its value will depend on the
		// interval)
		queueHeadItem->queueHead->endpointCaps |= USBEHCI_QHDW2_SPLTCOMPMASK;
	}

	// If the periodic schedule is not running, start it now
	if (!(ehciData->opRegs->stat & USBEHCI_STAT_PERIODICSCHED))
	{
		if (startStopSched(controller, USBEHCI_STAT_PERIODICSCHED,
			USBEHCI_CMD_PERSCHEDENBL, 1) < 0)
		goto err_out;
	}

	// Return success
	return (queueHeadItem);

err_out:

	if (queueHeadItem)
		releaseQueueHead(controller, queueHeadItem);

	return (queueHeadItem = NULL);
}


static int allocQtds(kernelLinkedList *freeList)
{
	// Allocate a page worth of physical memory for ehciQtd data structures,
	// allocate an equal number of ehciQtdItem structures to point at them,
	// and add them to the supplied kernelLinkedList.

	int status = 0;
	void *physicalMem = NULL;
	ehciQtd *qtds = NULL;
	int numQtds = 0;
	ehciQtdItem *qtdItems = NULL;
	unsigned physicalAddr = 0;
	int count;

	// Request an aligned page of I/O memory (we need to be sure of 32-byte
	// alignment for each qTD)
	status = allocIoMem(MEMORY_PAGE_SIZE, MEMORY_PAGE_SIZE, (void **) &qtds,
		&physicalMem);
	if (status < 0)
		goto err_out;

	// How many queue heads per memory page?  The ehciQueueHead data structure
	// will be padded so that they start on 32-byte boundaries, as required by
	// the hardware
	numQtds = (MEMORY_PAGE_SIZE / sizeof(ehciQtd));

	// Get memory for ehciQtdItem structures
	qtdItems = kernelMalloc(numQtds * sizeof(ehciQtdItem));
	if (qtdItems == NULL)
	{
		status = ERR_MEMORY;
		goto err_out;
	}

	// Now loop through each list item, link it to a qTD, and add it to the
	// free list
	physicalAddr = (unsigned) physicalMem;
	for (count = 0; count < numQtds; count ++)
	{
		qtdItems[count].qtd = &(qtds[count]);
		qtdItems[count].physicalAddr = physicalAddr;
		physicalAddr += sizeof(ehciQtd);

		status = kernelLinkedListAdd(freeList, &(qtdItems[count]));
		if (status < 0)
			goto err_out;
	}

	// Return success
	return (status = 0);

err_out:

	if (qtdItems)
		kernelFree(qtdItems);
	if (qtds && physicalMem)
		freeIoMem((void *) qtds, physicalMem, MEMORY_PAGE_SIZE);

	return (status);
}


static void releaseQtds(usbController *controller, ehciQtdItem **qtdItems,
	int numQtds)
{
	// Release qTDs back to the free pool after use

	usbEhciData *ehciData = controller->data;
	kernelLinkedList *freeList = (kernelLinkedList *) &(ehciData->freeQtdItems);
	int count;

	for (count = 0; count < numQtds; count ++)
	{
		if (qtdItems[count])
		{
			// If a buffer was allocated for the qTD, free it
			if (qtdItems[count]->buffer)
				kernelFree(qtdItems[count]->buffer);

			// Try to add it back to the free list
			kernelLinkedListAdd(freeList, qtdItems[count]);
		}
	}

	kernelFree(qtdItems);

	return;
}


static ehciQtdItem **getQtds(usbController *controller, int numQtds)
{
	// Allocate the requested number of Queue Element Transfer Descriptors,
	// and chain them together.

	ehciQtdItem **qtdItems = NULL;
	usbEhciData *ehciData = controller->data;
	kernelLinkedList *freeList = (kernelLinkedList *) &(ehciData->freeQtdItems);
	kernelLinkedListItem *iter = NULL;
	int count;

	kernelDebug(debug_usb, "EHCI get %d qTDs", numQtds);

	// Get memory for our list of ehciQtdItem pointers
	qtdItems = kernelMalloc(numQtds * sizeof(ehciQtdItem *));
	if (qtdItems == NULL)
		goto err_out;

	for (count = 0; count < numQtds; count ++)
	{
		// Anything in the free list?
		if (!freeList->numItems)
		{
			// Super, the free list is empty.  We need to allocate everything.
			if (allocQtds(freeList) < 0)
			{
				kernelError(kernel_error, "Couldn't allocate new qTDs");
				goto err_out;
			}
		}

		// Grab the first one from the free list
		qtdItems[count] = kernelLinkedListIterStart(freeList, &iter);
		if (qtdItems[count] == NULL)
		{
			kernelError(kernel_error, "Couldn't get a list item for a new qTD");
			goto err_out;
		}

		// Remove it from the free list
		if (kernelLinkedListRemove(freeList, qtdItems[count]) < 0)
		{
			kernelError(kernel_error, "Couldn't remove item from qTD free list");
			goto err_out;
		}

		// Initialize the qTD item
		qtdItems[count]->buffer = NULL;
		qtdItems[count]->nextQtdItem = NULL;

		// Clear/terminate the 'next' pointers, and any old data
		kernelMemClear((void *) qtdItems[count]->qtd, sizeof(ehciQtd));
		qtdItems[count]->qtd->nextQtd = USBEHCI_LINK_TERM;
		qtdItems[count]->qtd->altNextQtd = USBEHCI_LINK_TERM;

		// Chain it to the previous one, if applicable
		if (count)
		{
			qtdItems[count - 1]->nextQtdItem = qtdItems[count];
			qtdItems[count - 1]->qtd->nextQtd = qtdItems[count]->physicalAddr;
		}
	}

	// Return success
	return (qtdItems);

err_out:

	if (qtdItems)
		releaseQtds(controller, qtdItems, numQtds);

	return (qtdItems = NULL);
}


static int setQtdBufferPages(ehciQtd *qtd, unsigned buffPhysical,
	unsigned buffSize)
{
	// Given a physical buffer address and size, apportion the buffer page
	// fields in the qTD, so that they don't cross physical memory page
	// boundaries

	int status = 0;
	unsigned bytes = 0;
	int count;

	for (count = 0; ((count < USBEHCI_MAX_QTD_BUFFERS) && (buffSize > 0));
		count ++)
	{
		bytes =
			min(buffSize, (USBEHCI_MAX_QTD_BUFFERSIZE -
				(buffPhysical % USBEHCI_MAX_QTD_BUFFERSIZE)));

		kernelDebug(debug_usb, "EHCI qTD buffer page %d=0x%08x size=%u", count,
			buffPhysical, bytes);

		qtd->buffPage[count] = buffPhysical;

		buffPhysical += bytes;
		buffSize -= bytes;
	}

	if (buffSize)
	{
		// The size and/or page alignment of the buffer didn't fit into the qTD
		kernelError(kernel_error, "Buffer does not fit in a single qTD");
		return (status = ERR_BOUNDS);
	}

	// Return success
	return (status = 0);
}


static int allocQtdBuffer(ehciQtdItem *qtdItem, unsigned buffSize)
{
	// Allocate a data buffer for a qTD.  This is only used for cases in which
	// the caller doesn't supply its own data buffer, such as the setup stage
	// of control transfers, or for interrupt registrations.

	int status = 0;
	void *buffPhysical = NULL;

	kernelDebug(debug_usb, "EHCI allocate qTD buffer of %u", buffSize);

	// Get the memory from kernelMalloc(), so that the caller can easily
	// kernelFree() it when finished.
	qtdItem->buffer = kernelMalloc(buffSize);
	if (qtdItem->buffer == NULL)
	{
		kernelDebugError("Can't alloc trans desc buffer size %u", buffSize);
		return (status = ERR_MEMORY);
	}

	// Get the physical address of this memory
	buffPhysical = kernelPageGetPhysical(KERNELPROCID, qtdItem->buffer);
	if (buffPhysical == NULL)
	{
		kernelDebugError("Can't get buffer physical address");
		kernelFree(qtdItem->buffer);
		qtdItem->buffer = NULL;
		return (status = ERR_BADADDRESS);
	}

	// Now set up the buffer pointers in the qTD
	status = setQtdBufferPages(qtdItem->qtd, (unsigned) buffPhysical, buffSize);
	if (status < 0)
		return (status);

	// Return success
	return (status = 0);
}


static int setupQtdToken(ehciQtd *qtd, volatile unsigned char *dataToggle,
	unsigned totalBytes, int interrupt, unsigned char pid)
{
	// Do the nuts-n-bolts setup for a qTD transfer descriptor

	int status = 0;

	qtd->token = 0;

	if (dataToggle)
	{
		// Set the data toggle
		//kernelDebug(debug_usb, "EHCI set up qTD, dataToggle=%d", *dataToggle);
		qtd->token |= (*dataToggle << 31);
	}
	//else
		//kernelDebug(debug_usb, "EHCI set up qTD, no dataToggle");

	// Set the number of bytes to transfer
	qtd->token |= ((totalBytes << 16) & USBEHCI_QTDTOKEN_TOTBYTES);

	// Interrupt on complete?
	qtd->token |= ((interrupt << 15) & USBEHCI_QTDTOKEN_IOC);

	// Current page is 0

	// Set the error down-counter to 3
	qtd->token |= USBEHCI_QTDTOKEN_ERRCOUNT;

	switch (pid)
	{
		case USB_PID_OUT:
			qtd->token |= USBEHCI_QTDTOKEN_PID_OUT;
			break;
		case USB_PID_IN:
			qtd->token |= USBEHCI_QTDTOKEN_PID_IN;
			break;
		case USB_PID_SETUP:
			qtd->token |= USBEHCI_QTDTOKEN_PID_SETUP;
			break;
		default:
			kernelError(kernel_error, "Invalid PID %u", pid);
			return (status = ERR_INVALID);
	}

	// Mark it active
	qtd->token |= USBEHCI_QTDTOKEN_ACTIVE;

	// Return success
	return (status = 0);
}


static int queueTransaction(ehciTransQueue *transQueue)
{
	// The ehciTransQueue structure contains pointers to a queue head, and
	// an array of qTDs that should be linked to it.  If any existing qTDs are
	// linked, walk the chain and attach the new ones at the end.  Otherwise,
	// attach them directly to the queue head.

	int status = 0;
	ehciQtdItem *qtdItem = NULL;

	kernelDebug(debug_usb, "EHCI add transaction to queue");

	if (transQueue->queueHeadItem->firstQtdItem)
	{
		// There's already something in the queue.  Walk it to find the end.

		kernelDebug(debug_usb, "EHCI link to existing qTDs");

		qtdItem = transQueue->queueHeadItem->firstQtdItem;
		while (qtdItem->nextQtdItem)
			qtdItem = qtdItem->nextQtdItem;

		qtdItem->nextQtdItem = transQueue->qtdItems[0];
		qtdItem->qtd->nextQtd = transQueue->qtdItems[0]->physicalAddr;

		// Make sure the 'next' pointer of the queue head points to something
		// valid (if not, point to our first qTD)
		if (transQueue->queueHeadItem->queueHead->overlay.nextQtd &
			USBEHCI_LINK_TERM)
		{
			transQueue->queueHeadItem->queueHead->overlay.nextQtd =
				transQueue->qtdItems[0]->physicalAddr;
		}
	}
	else
	{
		// There's nothing in the queue.  Link to the queue head.

		kernelDebug(debug_usb, "EHCI link directly to queue head");

		transQueue->queueHeadItem->firstQtdItem = transQueue->qtdItems[0];
		transQueue->queueHeadItem->queueHead->overlay.nextQtd =
			transQueue->qtdItems[0]->physicalAddr;
	}

	// Return success
	return (status = 0);
}


static int dequeueTransaction(ehciTransQueue *transQueue)
{
	// The ehciTransQueue structure contains pointers to a queue head, and
	// an array of qTDs that should be unlinked from it.  Determine whether
	// they are linked directly from the queue head, or else somewhere else in
	// a chain of transactions.

	int status = 0;
	ehciQtdItem *qtdItem = NULL;

	kernelDebug(debug_usb, "EHCI remove transaction from queue head");

	if (transQueue->queueHeadItem == NULL)
		return (status = ERR_NOTINITIALIZED);

	if (transQueue->queueHeadItem->firstQtdItem == transQueue->qtdItems[0])
	{
		// We're linked directly from the queue head.  Replace the 'next'
		// qTD pointers in the queue head with the 'next' pointers from our
		// last qTD (which might be NULL/terminating)

		kernelDebug(debug_usb, "EHCI unlink directly from queue head");

		transQueue->queueHeadItem->firstQtdItem =
			transQueue->qtdItems[transQueue->numQtds - 1]->nextQtdItem;
		transQueue->queueHeadItem->queueHead->overlay.nextQtd =
			transQueue->qtdItems[transQueue->numQtds - 1]->qtd->nextQtd;
	}
	else
	{
		// There's something else in the queue.  Walk it to find the qTD that
		// links to our first one, and replace its 'next' qTD pointers with
		// the 'next' pointers from our last qTD (which might be 
		// NULL/terminating)

		kernelDebug(debug_usb, "EHCI unlink from chained qTDs");

		qtdItem = transQueue->queueHeadItem->firstQtdItem;
		while (qtdItem && (qtdItem->nextQtdItem != transQueue->qtdItems[0]))
			qtdItem = qtdItem->nextQtdItem;

		if (qtdItem == NULL)
		{
			// Not found!
			kernelError(kernel_error, "Transaction to de-queue was not found");
			return (status = ERR_NOSUCHENTRY);
		}

		qtdItem->nextQtdItem =
			transQueue->qtdItems[transQueue->numQtds - 1]->nextQtdItem;
		qtdItem->qtd->nextQtd =
			transQueue->qtdItems[transQueue->numQtds - 1]->qtd->nextQtd;
	}

	// Return success
	return (status = 0);
}


static int runTransaction(ehciTransQueue *transQueue)
{
	int status = 0;
	unsigned currTime = 0;
	unsigned activeTime = 0;
	int active = 0;
	unsigned previousQtd = 0;
	int error = 0;
	int count;

	kernelDebug(debug_usb, "EHCI run transaction with %d qTDs",
		transQueue->numQtds);

	currTime = kernelSysTimerRead();
	activeTime = currTime;

	// Wait while some qTD is active, or until we detect an error
	while (1)
	{
		active = 0;

		for (count = 0; count < transQueue->numQtds; count ++)
		{
			if (transQueue->qtdItems[count]->qtd->token &
				USBEHCI_QTDTOKEN_ACTIVE)
			{
				active = 1;
				break;
			}
			else if (transQueue->qtdItems[count]->qtd->token &
				USBEHCI_QTDTOKEN_ERROR)
			{
				kernelDebugError("Transaction error on qTD %d", count);
				debugTransError(transQueue->qtdItems[count]->qtd);
				error = 1;
				break;
			}
		}

		// If no more active, or errors, we're finished.
		if (!active || error)
		{
			if (error)
				status = ERR_IO;
			else
			{
				kernelDebug(debug_usb, "EHCI transaction completed "
					"successfully");
				status = 0;
			}

			break;
		}

		currTime = kernelSysTimerRead();

		// If the controller is moving through the queue, reset the timeout
		if ((transQueue->queueHeadItem->queueHead->currentQtd) != previousQtd)
		{
			activeTime = currTime;
			previousQtd = transQueue->queueHeadItem->queueHead->currentQtd;
		}

		if (currTime > (activeTime + 200))
		{
			// Software timeout after ~10 seconds per qTD
			kernelDebugError("Software timeout");
			status = ERR_TIMEOUT;
			break;
		}

		// Yielding here really hits performance.  Perhaps need to experiment
		// with interrupt-driven system.
		// kernelMultitaskerYield();
	}

	// Were any bytes left un-transferred?
	for (count = 0; count < transQueue->numQtds; count ++)
		transQueue->bytesRemaining +=
			((transQueue->qtdItems[count]->qtd->token &
				USBEHCI_QTDTOKEN_TOTBYTES) >> 16);

	return (status);
}


static int queue(usbController *controller, usbDevice *usbDev,
	usbTransaction *trans, int numTrans)
{
	// This function contains the intelligence necessary to initiate a
	// transaction (all phases)

	int status = 0;
	ehciTransQueue *transQueues = NULL;
	int packetSize = 0;
	void *buffPtr = NULL;
	unsigned bytesToTransfer = 0;
	unsigned bufferPhysical = 0;
	unsigned doBytes = 0;
	volatile unsigned char *dataToggle = NULL;
	ehciQtdItem *setupQtdItem = NULL;
	usbDeviceRequest *req = NULL;
	ehciQtdItem **dataQtdItems = NULL;
	ehciQtdItem *statusQtdItem = NULL;
	int transCount, qtdCount;

	kernelDebug(debug_usb, "EHCI queue %d transaction%s", numTrans,
		((numTrans > 1)? "s" : ""));

	if ((controller == NULL) || (usbDev == NULL) || (trans == NULL))
	{
		kernelError(kernel_error, "NULL hub, device, or transaction");
		return (status = ERR_NULLPARAMETER);
	}

	// Get memory for pointers to the transaction queues
	transQueues = kernelMalloc(numTrans * sizeof(ehciTransQueue));
	if (transQueues == NULL)
		return (status = ERR_MEMORY);

	// Loop to set up each transaction
	for (transCount = 0; transCount < numTrans; transCount ++)
	{
		// Lock the controller
		status = kernelLockGet(&controller->lock);
		if (status < 0)
		{
			kernelError(kernel_error, "Can't get controller lock");
			goto out;
		}

		// Try to find an existing queue head for this transaction's endpoint
		transQueues[transCount].queueHeadItem =
			findQueueHead(controller, usbDev, trans[transCount].endpoint);

		// Found it?
		if (transQueues[transCount].queueHeadItem)
		{
			kernelDebug(debug_usb, "EHCI found existing queue head");

			// Update the "static endpoint state" in the queue head (in case
			// anything has changed, such as the device address)
			status =
				setQueueHeadEndpointState(usbDev, trans[transCount].endpoint,
					transQueues[transCount].queueHeadItem->queueHead);
			if (status < 0)
				goto out;
		}
		else
		{
			// We don't yet have a queue head for this endpoint.  Try to
			// allocate one
			transQueues[transCount].queueHeadItem =
				allocAsyncQueueHead(controller, usbDev,
					trans[transCount].endpoint);
			if (transQueues[transCount].queueHeadItem == NULL)
			{
				kernelError(kernel_error, "Couldn't allocate endpoint queue "
					"head");
				status = ERR_NOSUCHENTRY;
				goto out;
			}
		}

		// We can get the maximum packet size for this endpoint from the queue
		// head (it will have been updated with the current device info upon
		// retrieval, above).
		packetSize =
			((transQueues[transCount].queueHeadItem->queueHead
				->endpointChars & USBEHCI_QHDW1_MAXPACKETLEN) >> 16);

		// Figure out how many transfer descriptors we're going to need for this
		// transaction
		transQueues[transCount].numDataQtds = 0;
		transQueues[transCount].numQtds = 0;

		// Setup/status descriptors?
		if (trans[transCount].type == usbxfer_control)
			// At least one each for setup and status
			transQueues[transCount].numQtds += 2;

		// Data descriptors?
		if (trans[transCount].length)
		{
			buffPtr = trans[transCount].buffer;
			bytesToTransfer = trans[transCount].length;

			while (bytesToTransfer)
			 {
				bufferPhysical = (unsigned)
					kernelPageGetPhysical((((unsigned) buffPtr <
						KERNEL_VIRTUAL_ADDRESS)?
							kernelCurrentProcess->processId :
								KERNELPROCID), buffPtr);

				doBytes = min(bytesToTransfer,
					(USBEHCI_MAX_QTD_DATA -
						(bufferPhysical % USBEHCI_MAX_QTD_BUFFERSIZE)));

				// Don't let packets cross qTD boundaries (for some reason)
				if ((doBytes < bytesToTransfer) && (doBytes % packetSize))
					doBytes -= (doBytes % packetSize);

				transQueues[transCount].numDataQtds += 1;
				bytesToTransfer -= doBytes;
				buffPtr += doBytes;
			}

			kernelDebug(debug_usb, "EHCI data payload of %u requires %d "
				"descriptors", trans[transCount].length,
				transQueues[transCount].numDataQtds);

			transQueues[transCount].numQtds +=
				transQueues[transCount].numDataQtds;
		}

		kernelDebug(debug_usb, "EHCI transaction requires %d descriptors",
			transQueues[transCount].numQtds);

		// Allocate the qTDs we need for this transaction
		transQueues[transCount].qtdItems =
			getQtds(controller, transQueues[transCount].numQtds);
		if (transQueues[transCount].qtdItems == NULL)
		{
			kernelError(kernel_error, "Couldn't get qTDs for transaction");
			status = ERR_NOFREE;
			goto out;
		}

		// Get the data toggle for the endpoint
		dataToggle =
			kernelUsbGetEndpointDataToggle(usbDev, trans[transCount].endpoint);
		if (dataToggle == NULL)
		{
			kernelError(kernel_error, "No data toggle for endpoint %02x",
				trans[transCount].endpoint);
			status = ERR_NOSUCHFUNCTION;
			goto out;
		}

		setupQtdItem = NULL;
		if (trans[transCount].type == usbxfer_control)
		{
			// Begin setting up the device request

			// Get the qTD for the setup phase
			setupQtdItem = transQueues[transCount].qtdItems[0];

			// Get a buffer for the device request memory
			status = allocQtdBuffer(setupQtdItem, sizeof(usbDeviceRequest));
			if (status < 0)
				goto out;

			req = setupQtdItem->buffer;

			status = kernelUsbSetupDeviceRequest(&trans[transCount], req);
			if (status < 0)
				goto out;

			// Data toggle is always 0 for the setup transfer
			*dataToggle = 0;

			// Setup the qTD for the setup phase
			status = setupQtdToken(setupQtdItem->qtd, dataToggle,
				sizeof(usbDeviceRequest), 0, USB_PID_SETUP);
			if (status < 0)
				goto out;

			// Data toggle
			*dataToggle ^= 1;
		}

		// If there is a data phase, set up the transfer descriptor(s) for the
		// data phase
		if (trans[transCount].length)
		{
			buffPtr = trans[transCount].buffer;
			bytesToTransfer = trans[transCount].length;

			dataQtdItems = &(transQueues[transCount].qtdItems[0]);
			if (setupQtdItem)
				dataQtdItems = &(transQueues[transCount].qtdItems[1]);

			for (qtdCount = 0; qtdCount < transQueues[transCount].numDataQtds;
				qtdCount ++)
			{
				bufferPhysical = (unsigned)
				kernelPageGetPhysical((((unsigned) buffPtr <
					KERNEL_VIRTUAL_ADDRESS)?
						kernelCurrentProcess->processId :
							KERNELPROCID), buffPtr);
				if (bufferPhysical == NULL)
				{
					kernelDebugError("Can't get physical address for buffer "
						"fragment at %p", buffPtr);
					status = ERR_MEMORY;
					goto out;
				}

				doBytes = min(bytesToTransfer,
					(USBEHCI_MAX_QTD_DATA -
						(bufferPhysical % USBEHCI_MAX_QTD_BUFFERSIZE)));

				// Don't let packets cross qTD boundaries (for some reason)
				if ((doBytes < bytesToTransfer) && (doBytes % packetSize))
				doBytes -= (doBytes % packetSize);

				kernelDebug(debug_usb, "EHCI bytesToTransfer=%u, doBytes=%u",
					bytesToTransfer, doBytes);

				// Set the qTD's buffer pointers to the relevent portions of
				// the transaction buffer.
				status = setQtdBufferPages(dataQtdItems[qtdCount]->qtd,
					bufferPhysical, doBytes);
				if (status < 0)
					goto out;

				// Set up the data qTD
				status = setupQtdToken(dataQtdItems[qtdCount]->qtd, dataToggle,
					doBytes, 0, trans[transCount].pid);
				if (status < 0)
					goto out;

				// If the qTD generated an odd number of packets, toggle the
				// data toggle.
				if (((doBytes + (packetSize - 1)) / packetSize) % 2)
					*dataToggle ^= 1;

				buffPtr += doBytes;
				bytesToTransfer -= doBytes;
			}
		}

		if (trans[transCount].type == usbxfer_control)
		{
			// Setup the transfer descriptor for the status phase

			statusQtdItem = transQueues[transCount]
				.qtdItems[transQueues[transCount].numQtds - 1];

			// Data toggle is always 1 for the status transfer
			*dataToggle = 1;

			// Setup the status packet
			status = setupQtdToken(statusQtdItem->qtd, dataToggle, 0, 0,
				((trans[transCount].pid == USB_PID_OUT)?
					USB_PID_IN : USB_PID_OUT));
			if (status < 0)
				goto out;
		}

		if (usbDev->speed != usbspeed_high)
			debugQueueHead(transQueues[transCount].queueHeadItem->queueHead);

		// Link the qTDs into the queue via the queue head
		status = queueTransaction(&transQueues[transCount]);
		if (status < 0)
			goto out;

		// Release the controller lock to process the transaction
		kernelLockRelease(&controller->lock);

		// Process the transaction
		status = runTransaction(&transQueues[transCount]);

		// Record the actual number of bytes transferred
		trans[transCount].bytes =
			(trans[transCount].length- transQueues[transCount].bytesRemaining);

		if (status < 0)
			goto out;
	}

out:
	// If the call to runTransaction() returned an error, the controller lock
	// won't be currently held.
	if (kernelLockVerify(&controller->lock) <= 0)
	{
		if (kernelLockGet(&controller->lock) < 0)
			kernelError(kernel_error, "Can't get controller lock");
	}

	if (kernelLockVerify(&controller->lock) > 0)
	{
		for (transCount = 0; transCount < numTrans; transCount ++)
		{
			if (transQueues[transCount].qtdItems)
			{
				// De-queue the qTDs from the queue head
				dequeueTransaction(&transQueues[transCount]);

				// Release the qTDs
				releaseQtds(controller, transQueues[transCount].qtdItems,
					transQueues[transCount].numQtds);
			}
		}

		kernelFree(transQueues);
		kernelLockRelease(&controller->lock);
	}
	else
		kernelError(kernel_error, "Don't have controller lock");

	return (status);
}


static int unschedInterrupt(usbController *controller, usbDevice *usbDev)
{
	// This function is used to unschedule an interrupt.

	int status = ERR_NOSUCHENTRY;
	usbEhciData *ehciData = NULL;
	kernelLinkedListItem *iter = NULL;
	usbEhciInterruptReg *intrReg = NULL;
	int count;

	// Check params
	if ((controller == NULL) || (usbDev == NULL))
	{
		kernelError(kernel_error, "NULL hub, device, or callback");
		return (status = ERR_NULLPARAMETER);
	}

	kernelDebug(debug_usb, "EHCI unschedule interrupt for device %d",
		usbDev->address);

	// Lock the controller
	status = kernelLockGet(&controller->lock);
	if (status < 0)
	{
		kernelError(kernel_error, "Can't get controller lock");
		return (status);
	}

	ehciData = controller->data;

	// Find the interrupt registration
	intrReg = kernelLinkedListIterStart(&ehciData->intrRegs, &iter);
	while (intrReg)
 	{
		if (intrReg->usbDev == usbDev)
		{
			// Remove it from the list of interrupt registrations
			kernelLinkedListRemove(&ehciData->intrRegs, intrReg);

			if (intrReg->transQueue.qtdItems)
			{
				// De-queue the qTDs from the queue head
				dequeueTransaction(&intrReg->transQueue);

				// Release the qTDs
				releaseQtds(controller, intrReg->transQueue.qtdItems,
					intrReg->transQueue.numQtds);
			}

			// Remove the queue head from the periodic schedule.

			// First unlink it from any others that point to it.
			status = unlinkQueueHead(controller,
				intrReg->transQueue.queueHeadItem);
			if (status < 0)
				goto out;

			// Now remove it from any places where it's linked directly from
			// the periodic list
			for (count = 0; count < USBEHCI_NUM_FRAMES;
				count += intrReg->interval)
			{
				if ((ehciData->periodicList[count] & ~USBEHCI_LINKTYP_MASK) ==
					intrReg->transQueue.queueHeadItem->physicalAddr)
				{
					kernelDebug(debug_usb, "EHCI remove queue head from "
						"interrupt slot %d", count);
					ehciData->periodicList[count] =
						intrReg->transQueue.queueHeadItem->queueHead->horizLink;
				}
			}

			// Release the queue head
			status = releaseQueueHead(controller,
				intrReg->transQueue.queueHeadItem);
			if (status < 0)
				goto out;

			// Free the memory
			kernelFree(intrReg);

			status = 0;
			break;
		}

		intrReg = kernelLinkedListIterNext(&ehciData->intrRegs, &iter);
	}

	if (!intrReg)
	{
		kernelError(kernel_warn, "Interrupt registration not found");
		status = ERR_NOSUCHENTRY;
	}

out:
	kernelLockRelease(&controller->lock);
	return (status);
}


static int schedInterrupt(usbController *controller, usbDevice *usbDev,
	unsigned char endpoint, int interval, unsigned maxLen,
	void (*callback)(usbDevice *, void *, unsigned))
{
	// This function is used to schedule an interrupt.

	int status = 0;
	usbEhciData *ehciData = NULL;
	usbEhciInterruptReg *intrReg = NULL;
	unsigned char sMask = 0;
	int count;

	// Check params
	if ((controller == NULL) || (usbDev == NULL) || (callback == NULL))
	{
		kernelError(kernel_error, "NULL hub, device, or callback");
		status = ERR_NULLPARAMETER;
		goto out;
	}

	kernelDebug(debug_usb, "EHCI schedule interrupt for address %d endpoint "
		"%02x len %u", usbDev->address, endpoint, maxLen);

	// Lock the controller
	status = kernelLockGet(&controller->lock);
	if (status < 0)
	{
		kernelError(kernel_error, "Can't get controller lock");
		return (status);
	}

	ehciData = controller->data;

	// Get memory to hold info about the interrupt
	intrReg = kernelMalloc(sizeof(usbEhciInterruptReg));
	if (intrReg == NULL)
	{
		status = ERR_MEMORY;
		goto out;
	}

	intrReg->usbDev = usbDev;
	intrReg->endpoint = endpoint;
	intrReg->maxLen = maxLen;
	intrReg->callback = callback;

	// Get a queue head for the interrupt endpoint.
	intrReg->transQueue.queueHeadItem =
		allocIntrQueueHead(controller, usbDev, endpoint);
	if (intrReg->transQueue.queueHeadItem == NULL)
	{
		kernelError(kernel_error, "Couldn't retrieve endpoint queue head");
		status = ERR_BUG;
		goto out;
	}

	// Get a qTD for it.
	intrReg->transQueue.numQtds = intrReg->transQueue.numDataQtds = 1;
	intrReg->transQueue.qtdItems = getQtds(controller, 1);
	if (intrReg->transQueue.qtdItems == NULL)
	{
		kernelError(kernel_error, "Couldn't get qTD for interrupt");
		status = ERR_BUG;
		goto out;
	}

	// Get the buffer for the qTD
	status = allocQtdBuffer(intrReg->transQueue.qtdItems[0], maxLen);
	if (status < 0)
		goto out;

	// Set up the qTD
	status = setupQtdToken(intrReg->transQueue.qtdItems[0]->qtd, NULL,
		intrReg->maxLen, 1 /* interrupt */, USB_PID_IN);
	if (status < 0)
		goto out;

	// Enqueue the qTD onto the queue head
	status = queueTransaction(&intrReg->transQueue);
	if (status < 0)
		goto out;

	// Add the interrupt registration to the controller's list.
	status = kernelLinkedListAdd(&ehciData->intrRegs, intrReg);
	if (status < 0)
		goto out;

	// Interpret the interval value.  Expressed in frames or microframes
	// depending on the device operating speed (i.e., either 1 millisecond or
	// 125 us units).  For full- or low-speed interrupt endpoints, the value of
	// this field may be from 1 to 255.  For high-speed interrupt endpoints,
	// interval is used as the exponent for a 2^(interval - 1) value;
	// e.g., an interval of 4 means a period of 8 (2^(4 - 1)).  This value
	// must be from 1 to 16.
	sMask = 1;
	if (usbDev->speed == usbspeed_high)
	{
		// Get the interval in microframes
		intrReg->interval = (1 << (interval - 1));

		// Set the interrupt mask in the queue head
		if (intrReg->interval < 8)
		{
			for (count = 1; count < 8; count ++)
				if (!(count % (intrReg->interval & 0x7)))
					sMask |= (1 << count);

			intrReg->interval = 1;
		}
		else
		{
			sMask = 1;
			intrReg->interval >>= 3;
		}
	}

	kernelDebug(debug_usb, "EHCI interrupt interval at %d frames, "
		"s-mask=0x%02x", intrReg->interval, sMask);
 
	intrReg->transQueue.queueHeadItem->queueHead->endpointCaps |=
		(sMask & USBEHCI_QHDW2_INTSCHEDMASK);

	// Insert it into the periodic schedule
	for (count = 0; count < USBEHCI_NUM_FRAMES; count += intrReg->interval)
	{
		if (!(ehciData->periodicList[count] & USBEHCI_LINK_TERM))
		{
			// There's already an interrupt queue head at this interval.
			// Link to it.
			intrReg->transQueue.queueHeadItem->queueHead->horizLink =
				ehciData->periodicList[count];
		}

		// Insert our queue head.
		ehciData->periodicList[count] =
			(intrReg->transQueue.queueHeadItem->physicalAddr |
				USBEHCI_LINKTYP_QH);
	}

	status = 0;

out:

	if (status < 0)
		unschedInterrupt(controller, usbDev);

	kernelLockRelease(&controller->lock);
	return (status);
}


static int startStop(usbController *controller, int start)
{
	// Start or stop the EHCI controller

	int status = 0;
	usbEhciData *ehciData = controller->data;
	int count;

	kernelDebug(debug_usb, "EHCI st%s controller", (start? "art" : "op"));

	if (start)
	{
		if (ehciData->opRegs->stat & USBEHCI_STAT_HCHALTED)
		{
			// Set the run/stop bit
			ehciData->opRegs->cmd |= USBEHCI_CMD_RUNSTOP;

			// Wait for not halted
			for (count = 0; count < 20; count ++)
			{
				if (!(ehciData->opRegs->stat & USBEHCI_STAT_HCHALTED))
				{
					kernelDebug(debug_usb, "EHCI starting controller took %dms",
						count);
					break;
				}

				kernelCpuSpinMs(1);
			}

			// Started?
			if (!(ehciData->opRegs->stat & USBEHCI_STAT_HCHALTED))
			{
				// Started, but some controllers need a small delay here,
				// before they're fully up and running.  3ms seems to be
				// enough, but we'll give it a little bit longer .
				kernelCpuSpinMs(5);
			}
			else
			{
				kernelError(kernel_error, "Couldn't clear controller halted "
					"bit");
				status = ERR_TIMEOUT;
			}
		}
	}
	else // stop
	{
		if (!(ehciData->opRegs->stat & USBEHCI_STAT_HCHALTED))
		{
			// Clear the run/stop bit
			ehciData->opRegs->cmd &= ~USBEHCI_CMD_RUNSTOP;

			// Wait for halted
			for (count = 0; count < 20; count ++)
			{
				if (ehciData->opRegs->stat & USBEHCI_STAT_HCHALTED)
				{
					kernelDebug(debug_usb, "EHCI stopping controller took %dms",
						count);
					break;
				}

				kernelCpuSpinMs(1);
			}

			// Stopped?
			if (!(ehciData->opRegs->stat & USBEHCI_STAT_HCHALTED))
			{
				kernelError(kernel_error, "Couldn't set controller halted bit");
				status = ERR_TIMEOUT;
			}
		}
	}

	kernelDebug(debug_usb, "EHCI controller %sst%sed", (status? "not " : ""),
		(start? "art" : "opp"));

	return (status);
}


static int handoff(usbController *controller, kernelBusTarget *busTarget,
	pciDeviceInfo *pciDevInfo)
{
	// If the controller supports the extended capability for legacy handoff
	// synchronization between the BIOS and the OS, do that here.

	int status = 0;
	usbEhciData *ehciData = controller->data;
	unsigned eecp = 0;
	ehciExtendedCaps *extCap = NULL;
	ehciLegacySupport *legSupp = NULL;
	int count;

	kernelDebug(debug_usb, "EHCI try BIOS-to-OS handoff");

	eecp = ((ehciData->capRegs->hccparams & USBEHCI_HCCP_EXTCAPPTR) >> 8);
	if (eecp)
	{
		kernelDebug(debug_usb, "EHCI has extended capabilities");

		extCap = (ehciExtendedCaps *)((void *) pciDevInfo->header + eecp);
		while (1)
		{
			kernelDebug(debug_usb, "EHCI extended capability %d", extCap->id);
			if (extCap->id == USBEHCI_EXTCAP_HANDOFFSYNC)
			{
				kernelDebug(debug_usb, "EHCI legacy support implemented");

				legSupp = (ehciLegacySupport *) extCap;

				// Does the BIOS claim ownership of the controller?
				if (legSupp->legSuppCap & USBEHCI_LEGSUPCAP_BIOSOWND)
					kernelDebug(debug_usb, "EHCI BIOS claims ownership, "
						"legSuppContStat=0x%08x", legSupp->legSuppContStat);
				else
					kernelDebug(debug_usb, "EHCI BIOS does not claim ownership");

				// Attempt to take over ownership; write the 'OS-owned' flag,
				// and wait for the BIOS to release ownership, if applicable
				for (count = 0; count < 50; count ++)
				{
					legSupp->legSuppCap |= USBEHCI_LEGSUPCAP_OSOWNED;
					kernelBusWriteRegister(busTarget,
						((eecp +
							offsetof(ehciLegacySupport, legSuppCap)) >> 2),
						32, legSupp->legSuppCap);

					// Re-read target info
					kernelBusGetTargetInfo(busTarget, pciDevInfo);

					if ((legSupp->legSuppCap & USBEHCI_LEGSUPCAP_OSOWNED) &&
						!(legSupp->legSuppCap & USBEHCI_LEGSUPCAP_BIOSOWND))
					{
						kernelDebug(debug_usb, "EHCI OS ownership took %dms",
							count);
						break;
					}

					kernelDebug(debug_usb, "EHCI legSuppCap=0x%08x",
						legSupp->legSuppCap);

					kernelCpuSpinMs(1);
				}

				// Do we have ownership?
				if (!(legSupp->legSuppCap & USBEHCI_LEGSUPCAP_OSOWNED) ||
					(legSupp->legSuppCap & USBEHCI_LEGSUPCAP_BIOSOWND))
				{
					kernelError(kernel_error, "BIOS did not release ownership");
					return (status = ERR_TIMEOUT);
				}

				// Make sure any SMIs are acknowledged and disabled
				legSupp->legSuppContStat = 0xE0000000;
				kernelBusWriteRegister(busTarget,
					((eecp +
						offsetof(ehciLegacySupport, legSuppContStat)) >> 2),
					32, legSupp->legSuppContStat);

				// Re-read target info
				kernelBusGetTargetInfo(busTarget, pciDevInfo);

				kernelDebug(debug_usb, "EHCI legSuppContStat now=0x%08x",
					legSupp->legSuppContStat);
			}

			if (extCap->next)
			{
				eecp = extCap->next;
				extCap = (ehciExtendedCaps *)
					((void *) pciDevInfo->header + eecp);
			}
			else
				break;
		}
	}
	else
		kernelDebug(debug_usb, "EHCI has no extended capabilities");

	return (status = 0);
}


static int reset(usbController *controller)
{
	// Do complete USB (controller and bus) reset

	int status = 0;
	usbEhciData *ehciData = NULL;
	int count;

	// Check params
	if (controller == NULL)
	{
		kernelError(kernel_error, "NULL controller");
		return (status = ERR_NULLPARAMETER);
	}

	// Make sure the controller is stopped
	status = startStop(controller, 0);
	if (status < 0)
		return (status);

	kernelDebug(debug_usb, "EHCI reset controller");

	ehciData = controller->data;

	// Set host controller reset
	ehciData->opRegs->cmd |= USBEHCI_CMD_HCRESET;

	// Wait until the host controller clears it
	for (count = 0; count < 20; count ++)
	{
		if (!(ehciData->opRegs->cmd & USBEHCI_CMD_HCRESET))
		{
			kernelDebug(debug_usb, "EHCI resetting controller took %dms",
				count);
			break;
		}

		kernelCpuSpinMs(1);
	}

	// Clear?
	if (ehciData->opRegs->cmd & USBEHCI_CMD_HCRESET)
	{
		kernelError(kernel_error, "Controller did not clear reset bit");
		status = ERR_TIMEOUT;
	}
	else
	{
		// Clear the lock
		kernelMemClear((void *) &controller->lock, sizeof(lock));
		status = 0;
	}

	kernelDebug(debug_usb, "EHCI controller reset %s",
		(status? "failed" : "successful"));

	return (status);
}


static inline void setPortStatusBits(usbController *controller, int portNum,
	unsigned bits)
{
	// Set the requested read-write status bits, without affecting any of
	// the read-only or read-write-clear bits

	usbEhciData *ehciData = controller->data;

	ehciData->opRegs->portsc[portNum] =
		((ehciData->opRegs->portsc[portNum] &
			~(USBEHCI_PORTSC_ROMASK | USBEHCI_PORTSC_RWCMASK)) | bits);
}


static inline void clearPortStatusBits(usbController *controller, int portNum,
	unsigned bits)
{
	// Clear the requested read-write status bits, without affecting any of
	// the read-only or read-write-clear bits

	usbEhciData *ehciData = controller->data;

	ehciData->opRegs->portsc[portNum] =
		(ehciData->opRegs->portsc[portNum] &
			~(USBEHCI_PORTSC_ROMASK | USBEHCI_PORTSC_RWCMASK | bits));
}


static int portPower(usbController *controller, int portNum, int on)
{
	// If port power control is available, this function will turn it on or
	// off

	int status = 0;
	usbEhciData *ehciData = controller->data;
	int count;

	kernelDebug(debug_usb, "EHCI %sable port power", (on? "en" : "dis"));

	if (on)
	{
		// Turn on the port power bit
		setPortStatusBits(controller, portNum, USBEHCI_PORTSC_PORTPOWER);

		// Wait for it to read as set
		for (count = 0; count < 20; count ++)
		{
			if (ehciData->opRegs->portsc[portNum] & USBEHCI_PORTSC_PORTPOWER)
			{
				kernelDebug(debug_usb, "EHCI turning on port power took %dms",
					count);
				break;
			}

			kernelCpuSpinMs(1);
		}

		// Set?
		if (!(ehciData->opRegs->portsc[portNum] & USBEHCI_PORTSC_PORTPOWER))
		{
			kernelError(kernel_warn, "Couldn't set port power bit");
			return (status = ERR_TIMEOUT);
		}
	}
	else // off - will we ever use this?
	{
		// Turn off the port power bit
		clearPortStatusBits(controller, portNum, USBEHCI_PORTSC_PORTPOWER);

		// Wait for it to read as clear
		for (count = 0; count < 20; count ++)
		{
			if (!(ehciData->opRegs->portsc[portNum] & USBEHCI_PORTSC_PORTPOWER))
			{
				kernelDebug(debug_usb, "EHCI turning off port power took %dms",
					count);
				break;
			}

			kernelCpuSpinMs(1);
		}

		// Clear?
		if (ehciData->opRegs->portsc[portNum] & USBEHCI_PORTSC_PORTPOWER)
		{
			kernelError(kernel_warn, "Couldn't clear port power bit");
			return (status = ERR_TIMEOUT);
		}
	}

	return (status = 0);
}


static int portReset(usbController *controller, int portNum)
{
	// Reset the port, with the appropriate delays, etc.

	int status = 0;
	usbEhciData *ehciData = controller->data;
	int count;

	kernelDebug(debug_usb, "EHCI port reset");

	// Clear the port 'enabled' bit
	clearPortStatusBits(controller, portNum, USBEHCI_PORTSC_PORTENABLED);

	// Wait for it to read as clear
	for (count = 0; count < 20; count ++)
	{
		if (!(ehciData->opRegs->portsc[portNum] & USBEHCI_PORTSC_PORTENABLED))
		{
			kernelDebug(debug_usb, "EHCI disabling port took %dms", count);
			break;
		}

		kernelCpuSpinMs(1);
	}

	// Clear?
	if (ehciData->opRegs->portsc[portNum] & USBEHCI_PORTSC_PORTENABLED)
	{
		kernelError(kernel_warn, "Couldn't clear port enabled bit");
		status = ERR_TIMEOUT;
		goto out;
	}

	// Set the port 'reset' bit
	setPortStatusBits(controller, portNum, USBEHCI_PORTSC_PORTRESET);

	// Wait for it to read as set
	for (count = 0; count < 20; count ++)
	{
		if (ehciData->opRegs->portsc[portNum] & USBEHCI_PORTSC_PORTRESET)
		{
			kernelDebug(debug_usb, "EHCI setting reset bit took %dms", count);
			break;
		}

		kernelCpuSpinMs(1);
	}

	// Set?
	if (!(ehciData->opRegs->portsc[portNum] & USBEHCI_PORTSC_PORTRESET))
	{
		kernelError(kernel_warn, "Couldn't set port reset bit");
		status = ERR_TIMEOUT;
		goto out;
	}

	// Delay 50ms
	kernelDebug(debug_usb, "EHCI delay for port reset");
	kernelCpuSpinMs(50);

	// Clear the port 'reset' bit
	clearPortStatusBits(controller, portNum, USBEHCI_PORTSC_PORTRESET);

	// Wait for it to read as clear
	for (count = 0; count < 200; count ++)
	{
		if (!(ehciData->opRegs->portsc[portNum] & USBEHCI_PORTSC_PORTRESET))
		{
			kernelDebug(debug_usb, "EHCI clearing reset bit took %dms", count);
			break;
		}

		kernelCpuSpinMs(1);
	}

	// Clear?
	if (ehciData->opRegs->portsc[portNum] & USBEHCI_PORTSC_PORTRESET)
	{
		kernelError(kernel_warn, "Couldn't clear port reset bit");
		status = ERR_TIMEOUT;
		goto out;
	}

	// Delay another 10ms
	kernelDebug(debug_usb, "EHCI delay after port reset");
	kernelCpuSpinMs(10);

	// Return success
	status = 0;

out:

	kernelDebug(debug_usb, "EHCI port reset %s",
		(status? "failed" : "success"));

	return (status);
}


static int setup(usbController *controller)
{
	// Allocate things, and set up any global controller registers prior to
	// changing the controller to the 'running' state.

	// Note that in the case of a host system error, we use this function to
	// re-initialize things, but we don't have to reallocate the memory.

	int status = 0;
	usbEhciData *ehciData = controller->data;
	int count;

	kernelDebug(debug_usb, "EHCI set up controller %d", controller->num);

	if (!ehciData->reclaimHead)
	{
		// Allocate memory for the NULL queue head that will be the head of the
		// 'reclaim' (asynchronous) queue
		ehciData->reclaimHead = allocQueueHead(controller, NULL /* no device */,
			0 /* no endpoint */);
		if (ehciData->reclaimHead == NULL)
			return (status = ERR_NOTINITIALIZED);
	}

	// Make it point to itself, set the 'H' bit, and make sure the qTD
	// pointers don't point to anything.
	ehciData->reclaimHead->queueHead->horizLink =
		(ehciData->reclaimHead->physicalAddr | USBEHCI_LINKTYP_QH);
	ehciData->reclaimHead->queueHead->endpointChars =
		USBEHCI_QHDW1_RECLLISTHEAD;
	ehciData->reclaimHead->queueHead->currentQtd =
		ehciData->reclaimHead->queueHead->overlay.nextQtd =
			ehciData->reclaimHead->queueHead->overlay.altNextQtd =
				USBEHCI_LINK_TERM;

	// After the reset, the default value of the USBCMD register is 0x00080000
	// (no async schedule park) or 0x00080B00 (async schedule park).  If any of
	// the default values aren't acceptable for us, change them here.

	// Hmm, VMware doesn't seem to set the defaults.  Set the interrupt
	// threshold control
	ehciData->opRegs->cmd &= ~USBEHCI_CMD_INTTHRESCTL;
	ehciData->opRegs->cmd |= (0x08 << 16);

	// The FRINDEX register defaults to 0x00000000 (start of periodic frame
	// list).  This is fine.

	// The CTRLDSSEGMENT register defaults to 0x00000000, which means we're
	// using a 32-bit address space.  Check.

	// If the size of the periodic queue frame list is programmable, make sure
	// it's set to the default (1024 = 0)
	if (ehciData->capRegs->hccparams & USBEHCI_HCCP_PROGFRAMELIST)
		ehciData->opRegs->cmd &= ~USBEHCI_CMD_FRAMELISTSIZE;

	if (!ehciData->periodicList)
	{
		// Allocate memory for the periodic queue frame list, and assign the
		// physical address to the PERIODICLISTBASE register
		status = allocIoMem(USBEHCI_FRAMELIST_MEMSIZE, MEMORY_PAGE_SIZE,
			(void **) &ehciData->periodicList,
			(void **) &ehciData->opRegs->perlstbase);
		if (status < 0)
		{
			kernelError(kernel_error, "Couldn't get memory for the periodic "
				"schedule");
			return (status);
		}
	}
	else
	{
		ehciData->opRegs->perlstbase = (unsigned)
			kernelPageGetPhysical(KERNELPROCID, ehciData->periodicList);
	}

	// Set the termination bit in each periodic list pointer
	for (count = 0; count < USBEHCI_NUM_FRAMES; count ++)
		ehciData->periodicList[count] = USBEHCI_LINK_TERM;

	// We don't program the ASYNCLISTADDR register to point to the NULL
	// 'reclaim' queue head we created above, or enable the asynchronous
	// schedule until an asynchronous transaction is queued.

	// Enable the interrupts we're interested in, in the USBINTR register; Host
	// system error, port change, error interrupt, and USB (data) interrupt.
	ehciData->opRegs->intr =
		(USBEHCI_INTR_HOSTSYSERROR | USBEHCI_INTR_USBERRORINT |
			USBEHCI_INTR_USBINTERRUPT);

	// Set the 'configured' flag in the CONFIGFLAG register
	ehciData->opRegs->configflag |= 1;

	return (status = 0);
}


static int portConnected(usbController *controller, int portNum, int hotPlug)
{
	// This function is called whenever we notice that a port has indicated
	// a new connection.

	int status = 0;
	usbEhciData *ehciData = controller->data;
	usbDevSpeed speed = usbspeed_unknown;

	kernelDebug(debug_usb, "EHCI controller %d, port %d connected",
		controller->num, portNum);

	debugPortStatus(controller, portNum);

	if (ehciData->opRegs->portsc[portNum] & USBEHCI_PORTSC_CONNCHANGE)
		// Acknowledge connection status change
		setPortStatusBits(controller, portNum, USBEHCI_PORTSC_CONNCHANGE);

	debugPortStatus(controller, portNum);

	// Check the line status bits to see whether this is a low-speed device
	if ((ehciData->opRegs->portsc[portNum] & USBEHCI_PORTSC_LINESTATUS) ==
		USBEHCI_PORTSC_LINESTAT_LS)
	{
		speed = usbspeed_low;

		// Release ownership of the port.  Not sure what we'd do here, if there
		// are no companion controllers.
		kernelDebug(debug_usb, "EHCI low-speed connection.  Releasing port "
			"ownership");
		setPortStatusBits(controller, portNum, USBEHCI_PORTSC_PORTOWNER);

		debugPortStatus(controller, portNum);
	}
	else
	{
		// Reset the port
		status = portReset(controller, portNum);
		if (status < 0)
			return (status);

		debugPortStatus(controller, portNum);

		// Is the port showing as enabled?
		if (ehciData->opRegs->portsc[portNum] & USBEHCI_PORTSC_PORTENABLED)
		{
			if (ehciData->opRegs->portsc[portNum] & USBEHCI_PORTSC_PORTENBLCHG)
				// Acknowledge enabled status change
				setPortStatusBits(controller, portNum,
					USBEHCI_PORTSC_PORTENBLCHG);

			speed = usbspeed_high;
		}
		else
			// Release ownership?
			speed = usbspeed_full;

		kernelDebug(debug_usb, "EHCI connection speed: %s",
			usbDevSpeed2String(speed));

		status = kernelUsbDevConnect(controller, &controller->hub, portNum,
			speed, hotPlug);
		if (status < 0)
		{
			kernelError(kernel_error, "Error enumerating new USB device");
			return (status);
		}
	}

	debugPortStatus(controller, portNum);

	return (status = 0);
}


static usbDevice *findRootHubDev(usbController *controller, int portNum)
{
	// Given a controller and a root hub port number, try to find the device
	// attached to that port

	usbDevice *usbDev = NULL;
	kernelLinkedListItem *iter = NULL;

	kernelDebug(debug_usb, "EHCI search root hub for controller %d, port %d",
		controller->num, portNum);

	usbDev = kernelLinkedListIterStart((kernelLinkedList *)
		&(controller->hub.devices), &iter);
	while (usbDev)
	{
		if (usbDev->port == portNum)
			break;
		else
			usbDev = kernelLinkedListIterNext((kernelLinkedList *)
				&(controller->hub.devices), &iter);
	}

	if (usbDev)
		kernelDebug(debug_usb, "EHCI found device %p", usbDev);
	else
		kernelDebug(debug_usb, "EHCI device not found");

	return (usbDev);
}


static void portDisconnected(usbController *controller, int portNum)
{
	usbEhciData *ehciData = controller->data;
	usbDevice *usbDev = NULL;
	ehciQueueHeadItem *queueHeadItem = NULL;
	int count;

	kernelDebug(debug_usb, "EHCI controller %d, port %d disconnected",
		controller->num, portNum);

	debugPortStatus(controller, portNum);

	// Try to find the device that's attached to this root hub port
	usbDev = findRootHubDev(controller, portNum);
	if (usbDev)
	{
		for (count = 0; count < usbDev->numEndpoints; count ++)
		{
			switch (usbDev->endpointDesc[count]->attributes &
				USB_ENDP_ATTR_MASK)
			{
				case USB_ENDP_ATTR_CONTROL:
				case USB_ENDP_ATTR_BULK:
				{
					// Remove any queue heads belonging to this device's
					// endpoints from the asynchronous queue
					queueHeadItem =
						findQueueHead(controller, usbDev,
							usbDev->endpointDesc[count]->endpntAddress);
					if (queueHeadItem)
						removeAsyncQueueHead(controller, queueHeadItem);
					break;
				}

				case USB_ENDP_ATTR_ISOCHRONOUS:
				case USB_ENDP_ATTR_INTERRUPT:
				{
					// Unschedule any interrupt registrations for the device
					unschedInterrupt(controller, usbDev);
					break;
				}

				default:
					break;
			}
		}
	}

	if (ehciData->opRegs->portsc[portNum] & USBEHCI_PORTSC_PORTENBLCHG)
		// Acknowledge enabled status change
		setPortStatusBits(controller, portNum, USBEHCI_PORTSC_PORTENBLCHG);

	if (ehciData->opRegs->portsc[portNum] & USBEHCI_PORTSC_CONNCHANGE)
		// Acknowledge connection status change
		setPortStatusBits(controller, portNum, USBEHCI_PORTSC_CONNCHANGE);

	debugPortStatus(controller, portNum);

	kernelUsbDevDisconnect(controller, &controller->hub, portNum);

	return;
}


static void hostSystemError(usbController *controller)
{
	int status = 0;
	usbEhciData *ehciData = controller->data;
	kernelLinkedList *usedList =
		(kernelLinkedList *) &(ehciData->usedQueueHeadItems);
	ehciQueueHeadItem *queueHeadItem = NULL;
	kernelLinkedListItem *iter = NULL;
	ehciQtdItem *qtdItem = NULL;
	int count;

	// Reset the controller
	status = reset(controller);
	if (status < 0)
		return;

	// Set up the controller's data structures, etc.
	status = setup(controller);
	if (status < 0)
		return;

	// Start the controller
	status = startStop(controller, 1);
	if (status < 0)
		return;

	// Power on all the ports, if applicable
	if (ehciData->capRegs->hcsparams & USBEHCI_HCSP_PORTPOWERCTRL)
	{
		for (count = 0; count < ehciData->numPorts; count ++)
			portPower(controller, count, 1);

		// Wait 20ms for power to stabilize on all ports (per EHCI spec)
		kernelCpuSpinMs(20);
	}

	// Remove all transactions from the queue heads and mark them as failed.
	if (usedList->numItems)
	{
		queueHeadItem = kernelLinkedListIterStart(usedList, &iter);
		while (queueHeadItem)
		{
			queueHeadItem->queueHead->currentQtd = USBEHCI_LINK_TERM;
			kernelMemClear((void *) &(queueHeadItem->queueHead->overlay),
				sizeof(ehciQtd));

			qtdItem = queueHeadItem->firstQtdItem;
			while (qtdItem)
			{
				qtdItem->qtd->token |= USBEHCI_QTDTOKEN_ERRXACT;
				qtdItem->qtd->token &= ~USBEHCI_QTDTOKEN_ACTIVE;
				qtdItem = qtdItem->nextQtdItem;
			}

			queueHeadItem = kernelLinkedListIterNext(usedList, &iter);
		}
	}

	// Restart the asynchronous schedule
	ehciData->opRegs->asynclstaddr = ehciData->reclaimHead->physicalAddr;
	startStopSched(controller, USBEHCI_STAT_ASYNCSCHED,
		USBEHCI_CMD_ASYNCSCHEDENBL, 1);

	// Restart the periodic schedule
	startStopSched(controller, USBEHCI_STAT_PERIODICSCHED,
		USBEHCI_CMD_PERSCHEDENBL, 1);

	return;
}


static int interrupt(usbController *controller)
{
	// This function gets called when the controller issues an interrupt.

	int status = 0;
	usbEhciData *ehciData = controller->data;
	usbEhciInterruptReg *intrReg = NULL;
	kernelLinkedListItem *iter = NULL;
	ehciQtd *qtd = NULL;
	unsigned bytes = 0;
	unsigned char dataToggle = 0;
	ehciQueueHead *queueHead = NULL;

	if (ehciData->opRegs->stat & USBEHCI_STAT_USBINTERRUPT)
	{
		//kernelDebug(debug_usb, "EHCI USB data interrupt, controller %d",
		//	controller->num);

		// Clear the USB interrupt bit
		setStatusBits(controller, USBEHCI_STAT_USBINTERRUPT);

		// Loop through the registered interrupts for ones that are no longer
		// active.
		intrReg = kernelLinkedListIterStart(&ehciData->intrRegs, &iter);
		while (intrReg)
		{
			queueHead = intrReg->transQueue.queueHeadItem->queueHead;
			qtd = intrReg->transQueue.qtdItems[0]->qtd;

			// If the QTD is no longer active, there might be some data there
			// for us.
			if (!(qtd->token & USBEHCI_QTDTOKEN_ACTIVE))
			{
				if (qtd->token & USBEHCI_QTDTOKEN_ERROR)
					goto intr_error;

				// Temporarily 'disconnect' the qTD
				queueHead->overlay.nextQtd = USBEHCI_LINK_TERM;

				bytes = (intrReg->maxLen -
					((qtd->token & USBEHCI_QTDTOKEN_TOTBYTES) >> 16));

				// If there's data and a callback function, do the callback.
				if (bytes && intrReg->callback)
				intrReg->callback(intrReg->usbDev,
					intrReg->transQueue.qtdItems[0]->buffer, bytes);

				// Get the data toggle
				dataToggle = ((qtd->token & USBEHCI_QTDTOKEN_DATATOGG) >> 31);

				// Reset the qTD
				status = setupQtdToken(qtd, &dataToggle, intrReg->maxLen, 1,
					USB_PID_IN);
				if (status < 0)
					goto intr_error;

				// Clear any buffer offset
				qtd->buffPage[0] &= 0xFFFFF000;

				// Reconnect the qTD to the queue head
				queueHead->overlay.nextQtd =
					intrReg->transQueue.qtdItems[0]->physicalAddr;
			}

			intrReg = kernelLinkedListIterNext(&ehciData->intrRegs, &iter);
			continue;

		intr_error:
			// If there was an error with this interrupt, remove it.
			unschedInterrupt(controller, intrReg->usbDev);

			// Restart list iteration
			intrReg = kernelLinkedListIterStart(&ehciData->intrRegs, &iter);
			continue;
		}
	}

	else if (ehciData->opRegs->stat & USBEHCI_STAT_HOSTSYSERROR)
	{
		kernelError(kernel_error, "USB host system error, controller %d",
			controller->num);

		debugEhciOpRegs(controller);

		// Clear the host system error bit
		setStatusBits(controller, USBEHCI_STAT_HOSTSYSERROR);

		// Try to get the controller running again
		hostSystemError(controller);
	}

	else if (ehciData->opRegs->stat & USBEHCI_STAT_USBERRORINT)
	{
		kernelDebug(debug_usb, "EHCI USB error interrupt, controller %d",
			controller->num);

		debugEhciOpRegs(controller);

		// Clear the USB error bit
		setStatusBits(controller, USBEHCI_STAT_USBERRORINT);
	}

	else
	{
		//kernelDebug(debug_usb, "EHCI no interrupt from controller %d",
		//	controller->num);
		return (status = ERR_NODATA);
	}

	return (status = 0);
}


static void doDetectDevices(usbHub *hub, int hotplug)
{
	// This function gets called to check for device connections (either cold-
	// plugged ones at boot time, or hot-plugged ones during operations.

	usbController *controller = hub->controller;
	usbEhciData *ehciData = controller->data;
	int count;

	if (controller == NULL)
	{
		kernelError(kernel_error, "Hub controller is NULL");
		return;
	}

	// Check to see whether any of the ports are showing a connection change
	for (count = 0; count < ehciData->numPorts; count ++)
	{
		if (ehciData->opRegs->portsc[count] & USBEHCI_PORTSC_CONNCHANGE)
		{
			kernelDebug(debug_usb, "EHCI port %d connection changed", count);

			if (ehciData->opRegs->portsc[count] & USBEHCI_PORTSC_CONNECTED)
				portConnected(controller, count, hotplug);
			else
				portDisconnected(controller, count);
		}
	}

	return;
}


static void detectDevices(usbHub *hub, int hotplug)
{
	// This function gets called once at startup to detect 'cold-plugged'
	// devices.

	kernelDebug(debug_usb, "EHCI initial device detection, hotplug=%d",
		hotplug);

	// Check params
	if (hub == NULL)
	{
		kernelError(kernel_error, "NULL hub pointer");
		return;
	}

	doDetectDevices(hub, hotplug);

	hub->doneColdDetect = 1;
}


static void threadCall(usbHub *hub)
{
	// This function gets called periodically by the USB thread, to give us
	// an opportunity to detect connections/disconnections, or whatever else
	// we want.

	usbController *controller = NULL;
	usbEhciData *ehciData = NULL;

	// Check params
	if (hub == NULL)
	{
		kernelError(kernel_error, "NULL hub pointer");
		return;
	}

	// Only continue if we've already completed 'cold' device connection
	// detection.  Don't want to interfere with that.
	if (!hub->doneColdDetect)
		return;

	controller = hub->controller;
	if (controller == NULL)
	{
		kernelError(kernel_error, "Hub controller is NULL");
		return;
	}

	ehciData = controller->data;

	if (ehciData->opRegs->stat & USBEHCI_STAT_PORTCHANGE)
	{
		doDetectDevices(hub, 1 /* hotplug */);

		// Clear the port change bit
		setStatusBits(controller, USBEHCI_STAT_PORTCHANGE);
	}
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
//  Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


kernelDevice *kernelUsbEhciDetect(kernelBusTarget *busTarget,
	kernelDriver *driver)
{
	// This routine is used to detect and initialize a potential EHCI USB
	// device, as well as registering it with the higher-level interfaces.

	int status = 0;
	pciDeviceInfo pciDevInfo;
	usbController *controller = NULL;
	usbEhciData *ehciData = NULL;
	kernelDevice *dev = NULL;
	char value[32];
	int count;

	// Get the PCI device header
	status = kernelBusGetTargetInfo(busTarget, &pciDevInfo);
	if (status < 0)
		goto err_out;

	// Make sure it's a non-bridge header
	if ((pciDevInfo.device.headerType & ~PCI_HEADERTYPE_MULTIFUNC) !=
		PCI_HEADERTYPE_NORMAL)
	{
		kernelDebugError("EHCI headertype not 'normal' (%02x)",
			(pciDevInfo.device.headerType & ~PCI_HEADERTYPE_MULTIFUNC));
		goto err_out;
	}

	// Make sure it's an EHCI controller (programming interface is 0x20 in
	// the PCI header)
	if (pciDevInfo.device.progIF != 0x20)
		goto err_out;

	// After this point, we believe we have a supported device.

	kernelDebug(debug_usb, "EHCI controller found");

	// Try to enable bus mastering
	if (pciDevInfo.device.commandReg & PCI_COMMAND_MASTERENABLE)
		kernelDebug(debug_usb, "EHCI bus mastering already enabled");
	else
		kernelBusSetMaster(busTarget, 1);

	// Disable the device's memory access and I/O decoder, if applicable
	kernelBusDeviceEnable(busTarget, 0);

	// Re-read target info
	status = kernelBusGetTargetInfo(busTarget, &pciDevInfo);
	if (status < 0)
		goto err_out;

	if (!(pciDevInfo.device.commandReg & PCI_COMMAND_MASTERENABLE))
		kernelDebugError("EHCI: Couldn't enable bus mastering");
	else
		kernelDebug(debug_usb, "EHCI bus mastering enabled in PCI");

	// Make sure the BAR refers to a memory decoder
	if (pciDevInfo.device.nonBridge.baseAddress[0] & 0x1)
	{
		kernelDebugError("EHCI: ABAR is not a memory decoder");
		goto err_out;
	}

	// Allocate memory for the controller
	controller = kernelMalloc(sizeof(usbController));
	if (controller == NULL)
		goto err_out;

	// Get the USB version number
	controller->usbVersion = kernelBusReadRegister(busTarget, 0x60, 8);

	// Get the interrupt line
	controller->interruptNum = (int) pciDevInfo.device.nonBridge.interruptLine;

	kernelLog("USB: EHCI controller USB %d.%d interrupt %d",
		((controller->usbVersion & 0xF0) >> 4),
		(controller->usbVersion & 0xF), controller->interruptNum);

	// Allocate memory for the EHCI data
	controller->data = kernelMalloc(sizeof(usbEhciData));
	if (controller->data == NULL)
		goto err_out;

	ehciData = controller->data;

	// Get the memory range address
	ehciData->physMemSpace =
		(pciDevInfo.device.nonBridge.baseAddress[0] & 0xFFFFFFF0);

	if (pciDevInfo.device.nonBridge.baseAddress[0] & 0x6)
	{
		kernelError(kernel_error, "Register memory must be mappable in "
			"32-bit address space");
		goto err_out;
	}

	// Determine the memory space size.  Write all 1s to the register.
	kernelBusWriteRegister(busTarget,
		PCI_CONFREG_BASEADDRESS0_32, 32, 0xFFFFFFFF);

	ehciData->memSpaceSize =
		(~(kernelBusReadRegister(busTarget,
			PCI_CONFREG_BASEADDRESS0_32, 32) & ~0xF) + 1);

	// Restore the register we clobbered.
	kernelBusWriteRegister(busTarget, PCI_CONFREG_BASEADDRESS0_32, 32,
		pciDevInfo.device.nonBridge.baseAddress[0]);

	// Map the physical memory address of the controller's registers into
	// our virtual address space.

	// Map the physical memory space pointed to by the decoder.
	status = kernelPageMapToFree(KERNELPROCID, (void *) ehciData->physMemSpace,
		(void **) &(ehciData->capRegs), ehciData->memSpaceSize);
	if (status < 0)
	{
		kernelDebugError("EHCI: Error mapping memory");
		goto err_out;
	}

	// Make it non-cacheable, since this memory represents memory-mapped
	// hardware registers.
	status =
		kernelPageSetAttrs(KERNELPROCID, 1 /* set */, PAGEFLAG_CACHEDISABLE,
			(void *) ehciData->capRegs, ehciData->memSpaceSize);
	if (status < 0)
	{
		kernelDebugError("EHCI: Error setting page attrs");
		goto err_out;
	}

	// Enable memory mapping access
	if (pciDevInfo.device.commandReg & PCI_COMMAND_MEMORYENABLE)
		kernelDebug(debug_usb, "EHCI memory access already enabled");
	else
		kernelBusDeviceEnable(busTarget, PCI_COMMAND_MEMORYENABLE);

	// Re-read target info
	kernelBusGetTargetInfo(busTarget, &pciDevInfo);

	if (!(pciDevInfo.device.commandReg & PCI_COMMAND_MEMORYENABLE))
	{
		kernelDebugError("EHCI couldn't enable memory access");
		goto err_out;
	}
	kernelDebug(debug_usb, "EHCI memory access enabled in PCI");

	debugEhciCapRegs(controller);
	debugEhciHcsParams(controller);
	debugEhciHccParams(controller);

	ehciData->opRegs = ((void *) ehciData->capRegs + ehciData->capRegs->capslen);

	ehciData->numPorts = (ehciData->capRegs->hcsparams & USBEHCI_HCSP_NUMPORTS);
	kernelDebug(debug_usb, "EHCI number of ports=%d", ehciData->numPorts);

	ehciData->debugPort =
		((ehciData->capRegs->hcsparams & USBEHCI_HCSP_DEBUGPORT) >> 20);
	kernelDebug(debug_usb, "EHCI debug port=%d", ehciData->debugPort);

	// If the extended capabilities registers are implemented, perform an
	// orderly ownership transfer from the BIOS. 
	status = handoff(controller, busTarget, &pciDevInfo);
	if (status < 0)
		goto err_out;

	// Reset the controller
	status = reset(controller);
	if (status < 0)
		goto err_out;

	// Set up the controller's data structures, etc.
	status = setup(controller);
	if (status < 0)
		goto err_out;

	// Start the controller
	status = startStop(controller, 1);
	if (status < 0)
		goto err_out;

	// Power on all the ports, if applicable
	if (ehciData->capRegs->hcsparams & USBEHCI_HCSP_PORTPOWERCTRL)
	{
		for (count = 0; count < ehciData->numPorts; count ++)
		{
			status = portPower(controller, count, 1);
			if (status < 0)
				goto err_out;
		}

		// Wait 20ms for power to stabilize on all ports (per EHCI spec)
		kernelCpuSpinMs(20);
	}

	debugEhciOpRegs(controller);

	// Set controller function calls
	controller->reset = &reset;
	controller->interrupt = &interrupt;
	controller->queue = &queue;
	controller->schedInterrupt = &schedInterrupt;
	controller->unschedInterrupt = &unschedInterrupt;

	controller->hub.controller = controller;
	controller->hub.detectDevices = &detectDevices;
	controller->hub.threadCall = &threadCall;

	// Allocate memory for the kernel device
	dev = kernelMalloc(sizeof(kernelDevice));
	if (dev == NULL)
		goto err_out;

	dev->device.class = kernelDeviceGetClass(DEVICECLASS_BUS);
	dev->device.subClass = kernelDeviceGetClass(DEVICESUBCLASS_BUS_USB);
	dev->driver = driver;
	dev->data = (void *) controller;

	// Initialize the variable list for attributes of the controller
	status = kernelVariableListCreate(&dev->device.attrs);
	if (status >= 0)
	{
		kernelVariableListSet(&dev->device.attrs, "controller.type", "EHCI");
		snprintf(value, 32, "%d", ehciData->numPorts);
		kernelVariableListSet(&dev->device.attrs, "controller.numPorts", value);
		if (ehciData->debugPort)
		{
			snprintf(value, 32, "%d", ehciData->debugPort);
			kernelVariableListSet(&dev->device.attrs, "controller.debugPort",
				value);
		}
	}

	status = kernelDeviceAdd(busTarget->bus->dev, dev);
	if (status < 0)
		goto err_out;
	else 
		return (dev);

err_out:

	if (dev)
		kernelFree(dev);
	if (controller)
	{
		if (controller->data)
			kernelFree(controller->data);
		kernelFree((void *) controller);
	}

	return (dev = NULL);
}
