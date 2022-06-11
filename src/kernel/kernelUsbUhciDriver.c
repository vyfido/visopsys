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
//  kernelUsbUhciDriver.c
//

#include "kernelUsbDriver.h"
#include "kernelUsbUhciDriver.h"
#include "kernelCpu.h"
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
#include "kernelVariableList.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef DEBUG
static inline void debugUhciRegs(usbController *controller)
{
	usbUhciData *uhciData = controller->data;
	unsigned short cmd = 0;
	unsigned short stat = 0;
	unsigned short intr = 0;
	unsigned short frnum = 0;
	unsigned flbase = 0;
	unsigned char sof = 0;
	unsigned short portsc1 = 0;
	unsigned short portsc2 = 0;

	kernelProcessorInPort16((uhciData->ioAddress + USBUHCI_PORTOFFSET_CMD),
		cmd);
	kernelProcessorInPort16((uhciData->ioAddress + USBUHCI_PORTOFFSET_STAT),
		stat);
	kernelProcessorInPort16((uhciData->ioAddress + USBUHCI_PORTOFFSET_INTR),
		intr);
	kernelProcessorInPort16((uhciData->ioAddress + USBUHCI_PORTOFFSET_FRNUM),
		frnum);
	kernelProcessorInPort32((uhciData->ioAddress + USBUHCI_PORTOFFSET_FLBASE),
		flbase);
	kernelProcessorInPort8((uhciData->ioAddress + USBUHCI_PORTOFFSET_SOF),
		sof);
	kernelProcessorInPort16((uhciData->ioAddress + USBUHCI_PORTOFFSET_PORTSC1),
		portsc1);
	kernelProcessorInPort16((uhciData->ioAddress + USBUHCI_PORTOFFSET_PORTSC2),
		portsc2);

	kernelDebug(debug_usb, "UHCI registers:\n"
		"  cmd=%04x\n"
		"  stat=%04x\n"
		"  intr=%04x\n"
		"  frnum=%04x\n"
		"  flbase=%08x\n"
		"  sof=%02x\n"
		"  portsc1=%04x\n"
		"  portsc2=%04x\n", cmd, stat, intr, frnum, flbase, sof,
		portsc1, portsc2);
}


static inline void debugDeviceReq(usbDeviceRequest *req)
{
	kernelDebug(debug_usb, "UHCI device request:\n"
		"  requestType=%02x\n"
		"  request=%02x\n"
		"  value=%04x\n"
		"  index=%04x\n"
		"  length=%04x", req->requestType, req->request, req->value,
		req->index, req->length);
}


static inline void debugQueueHead(usbUhciQueueHead *queueHead)
{
	kernelDebug(debug_usb, "UHCI queue head:\n"
		"  linkPointer=%04x\n"
		"  element=%04x\n"
		"  saveElement=%04x\n"
		"  transDescs=%p", queueHead->linkPointer,
		queueHead->element, queueHead->saveElement,
		queueHead->transDescs);
}


static inline void debugTransDesc(usbUhciTransDesc *desc)
{
	kernelDebug(debug_usb, "UHCI transfer descriptor:\n"
		"  linkPointer=%08x\n"
		"  contStatus=%08x\n"
		"    spd=%d\n"
		"    errcount=%d\n"
		"    lowspeed=%d\n"
		"    isochronous=%d\n"
		"    interrupt=%d\n"
		"    status=%02x\n"
		"    actlen=%d (%03x)\n"
		"  tdToken=%08x\n"
		"    maxlen=%d (%03x)\n"
		"    datatoggle=%d\n"
		"    endpoint=%02x\n"
		"    address=%d\n"
		"    pid=%02x\n"
		"  buffer=%08x", desc->linkPointer, desc->contStatus,
		((desc->contStatus & USBUHCI_TDCONTSTAT_SPD) >> 29),
		((desc->contStatus & USBUHCI_TDCONTSTAT_ERRCNT) >> 27),
		((desc->contStatus & USBUHCI_TDCONTSTAT_LSPEED) >> 26),
		((desc->contStatus & USBUHCI_TDCONTSTAT_ISOC) >> 25),
		((desc->contStatus & USBUHCI_TDCONTSTAT_IOC) >> 24),
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


static void debugTransError(usbUhciTransDesc *desc)
{
	char *errorText = NULL;
	char *transString = NULL;

	errorText = kernelMalloc(MAXSTRINGLENGTH);
	if (errorText)
	{
		switch (desc->tdToken & USBUHCI_TDTOKEN_PID)
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
		sprintf(errorText, "UHCI: Trans desc %s: ", transString);
		if (desc->contStatus & USBUHCI_TDCONTSTAT_ESTALL)
			strcat(errorText, "stalled, ");
		if (desc->contStatus & USBUHCI_TDCONTSTAT_EDBUFF)
			strcat(errorText, "data buffer error, ");
		if (desc->contStatus & USBUHCI_TDCONTSTAT_EBABBLE)
			strcat(errorText, "babble, ");
		if (desc->contStatus & USBUHCI_TDCONTSTAT_ENAK)
			strcat(errorText, "NAK, ");
		if (desc->contStatus & USBUHCI_TDCONTSTAT_ECRCTO)
			strcat(errorText, "CRC/timeout, ");
		if (desc->contStatus & USBUHCI_TDCONTSTAT_EBSTUFF)
			strcat(errorText, "bitstuff error, ");

		if (desc->contStatus & USBUHCI_TDCONTSTAT_ACTIVE)
			strcat(errorText, "TD is still active");

		kernelDebugError("%s", errorText);
		kernelFree(errorText);
	}

	debugTransDesc(desc);
}
#else
	#define debugUhciRegs(usb) do { } while (0)
	#define debugDeviceReq(req) do { } while (0)
	#define debugQueueHead(qhead) do { } while (0)
	#define debugTransDesc(desc) do { } while (0)
	#define debugTransError(desc) do { } while (0)
#endif // DEBUG


static inline unsigned char readCommand(usbController *controller)
{
	usbUhciData *uhciData = controller->data;
	unsigned short command = 0;
	kernelProcessorInPort16((uhciData->ioAddress + USBUHCI_PORTOFFSET_CMD),
		command);
	return ((unsigned char)(command & 0xFF));
}


static inline void writeCommand(usbController *controller,
	unsigned char command)
{
	usbUhciData *uhciData = controller->data;
	unsigned short tmp = 0;
	kernelProcessorInPort16((uhciData->ioAddress + USBUHCI_PORTOFFSET_CMD),
		tmp);
	tmp = ((tmp & 0xFF00) | command);
	kernelProcessorOutPort16((uhciData->ioAddress + USBUHCI_PORTOFFSET_CMD),
		tmp);
}


static inline unsigned char readStatus(usbController *controller)
{
	usbUhciData *uhciData = controller->data;
	unsigned short status = 0;
	kernelProcessorInPort16((uhciData->ioAddress + USBUHCI_PORTOFFSET_STAT),
		status);
	return ((unsigned char)(status & 0x3F));
}


static inline void writeStatus(usbController *controller, unsigned char status)
{
	usbUhciData *uhciData = controller->data;
	unsigned short tmp = 0;
	kernelProcessorInPort16((uhciData->ioAddress + USBUHCI_PORTOFFSET_STAT),
		tmp);
	tmp |= (status & 0x3F);
	kernelProcessorOutPort16((uhciData->ioAddress + USBUHCI_PORTOFFSET_STAT),
		tmp);
}


static inline unsigned short readFrameNum(usbController *controller)
{
	usbUhciData *uhciData = controller->data;
	unsigned short num = 0;
	kernelProcessorInPort16((uhciData->ioAddress + USBUHCI_PORTOFFSET_FRNUM),
		num);
	return (num & 0x7FF);
}


static inline void writeFrameNum(usbController *controller, unsigned short num)
{
	usbUhciData *uhciData = controller->data;
	unsigned short tmp = 0;
	kernelProcessorInPort16((uhciData->ioAddress + USBUHCI_PORTOFFSET_FRNUM),
		tmp);
	tmp = ((tmp & 0xF800) | (num & 0x7FF));
	kernelProcessorOutPort16((uhciData->ioAddress + USBUHCI_PORTOFFSET_FRNUM),
		tmp);
}


static int startStop(usbController *controller, int start)
{
	// Start or stop the USB controller

	int status = 0;
	unsigned char command = 0;
	unsigned char statReg = 0;
	int count;

	kernelDebug(debug_usb, "UHCI %s controller", (start? "start" : "stop"));

	command = readCommand(controller);

	if (start)
		command |= USBUHCI_CMD_RUNSTOP;
	else
		command &= ~USBUHCI_CMD_RUNSTOP;

	writeCommand(controller, command);

	if (start)
	{
		// Wait for started
		for (count = 0; count < 20; count ++)
		{
			statReg = readStatus(controller);
			if (!(statReg & USBUHCI_STAT_HCHALTED))
			{
				kernelDebug(debug_usb, "UHCI starting controller took %dms",
					count);
				break;
			}

			kernelCpuSpinMs(1);
		}

		// Started?
		if (statReg & USBUHCI_STAT_HCHALTED)
		{
			kernelError(kernel_error, "Couldn't clear controller halted bit");
			status = ERR_TIMEOUT;
		}
	}
	else
	{
		// Wait for stopped
		for (count = 0; count < 20; count ++)
		{
			statReg = readStatus(controller);
			if (statReg & USBUHCI_STAT_HCHALTED)
			{
				kernelDebug(debug_usb, "UHCI stopping controller took %dms",
					count);
				break;
			}

			kernelCpuSpinMs(1);
		}

		// Stopped?
		if (!(statReg & USBUHCI_STAT_HCHALTED))
		{
			kernelError(kernel_error, "Couldn't set controller halted bit");
			status = ERR_TIMEOUT;
		}
	}

	// Clear the status register
	writeStatus(controller, statReg);

	return (status);
}


static int reset(usbController *controller)
{
	// Do complete UHCI controller reset

	unsigned char command = 0;

	// Set global reset
	command = readCommand(controller);
	command |= USBUHCI_CMD_GRESET;
	writeCommand(controller, command);

	// Delay 100 ms.
	kernelDebug(debug_usb, "UHCI delay for global reset");
	kernelCpuSpinMs(100);

	// Clear global reset
	command = readCommand(controller);
	command &= ~USBUHCI_CMD_GRESET;
	writeCommand(controller, command);

	// Clear the lock
	kernelMemClear((void *) &controller->lock, sizeof(lock));

	kernelDebug(debug_usb, "UHCI controller reset");
	return (0);
}


static inline unsigned short readPortStatus(usbController *controller, int num)
{
	usbUhciData *uhciData = controller->data;
	unsigned portOffset = 0;
	unsigned short status = 0;

	if (!num)
		portOffset = USBUHCI_PORTOFFSET_PORTSC1;
	else if (num == 1)
		portOffset = USBUHCI_PORTOFFSET_PORTSC2;
	else
		return (status = 0);

	kernelProcessorInPort16((uhciData->ioAddress + portOffset), status);

	return (status);
}


static inline void writePortStatus(usbController *controller, int num,
	unsigned short status)
{
	usbUhciData *uhciData = controller->data;
	unsigned portOffset = 0;

	if (!num)
		portOffset = USBUHCI_PORTOFFSET_PORTSC1;
	else if (num == 1)
		portOffset = USBUHCI_PORTOFFSET_PORTSC2;
	else
		return;

	// Don't write any read-only/reserved bits
	status &= 0x124E;

	kernelProcessorOutPort16((uhciData->ioAddress + portOffset), status);
	return;
}


static inline void setPortStatusBits(usbController *controller, int num,
	unsigned short bits, int on)
{
	unsigned short status = 0;

	// Get the current register
	status = readPortStatus(controller, num);

	// Don't inadvertently clear any RWC (read/write-clear) bits, but allow them
	// to be set in the next step.
	status &= ~USBUHCI_PORT_RWC_BITS;

	if (on)
		status |= bits;
	else
		status &= ~bits;

	writePortStatus(controller, num, status);

	return;
}


#ifdef DEBUG
static inline void printPortStatus(usbController *controller)
{
	kernelDebug(debug_usb, "UHCI controller %d, port 0: %04x  port 1: %04x "
		"frnum %d", controller->num, readPortStatus(controller, 0),
		readPortStatus(controller, 1), (readFrameNum(controller) & 0x3FF));
}
#else
	#define printPortStatus(usb) do { } while (0)
#endif


static void portReset(usbController *controller, int num)
{
	unsigned short status = 0;
	int count;

	kernelDebug(debug_usb, "UHCI before port reset");
	printPortStatus(controller);

	for (count = 0; count < 20; count ++)
	{
		// Set the reset bit
		setPortStatusBits(controller, num, USBUHCI_PORT_RESET, 1);

		status = readPortStatus(controller, num);
		if (status & USBUHCI_PORT_RESET)
			break;
	}

	if (!(status & USBUHCI_PORT_RESET))
		kernelError(kernel_error, "Couldn't set port reset bit");

	kernelDebug(debug_usb, "UHCI after reset asserted");
	printPortStatus(controller);

	// Delay 50ms
	kernelDebug(debug_usb, "UHCI delay for port reset");
	kernelCpuSpinMs(50);

	// Clear the reset bit
	setPortStatusBits(controller, num, USBUHCI_PORT_RESET, 0);

	kernelDebug(debug_usb, "UHCI after reset cleared");
	printPortStatus(controller);

	for (count = 0; count < 20; count ++)
	{
		// Set the enabled bit
		setPortStatusBits(controller, num, USBUHCI_PORT_ENABLED, 1);

		status = readPortStatus(controller, num);
		if (status & USBUHCI_PORT_ENABLED)
			break;
	}

	if (!(status & USBUHCI_PORT_ENABLED))
		kernelError(kernel_error, "Couldn't set port enabled bit");

	kernelDebug(debug_usb, "UHCI after enable set");
	printPortStatus(controller);

	// Delay another 10ms
	kernelDebug(debug_usb, "UHCI delay after port reset");
	kernelCpuSpinMs(10);

	status = readPortStatus(controller, num);

	if (status & USBUHCI_PORT_RESET)
		kernelError(kernel_error, "Couldn't clear port reset bit");

	if (!(status & USBUHCI_PORT_ENABLED))
		kernelError(kernel_error, "Couldn't enable port");

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
	*physical = kernelMemoryGetPhysical(memSize, MEMORY_PAGE_SIZE,
		"usb xfer descriptors");

	if (!*physical || ((unsigned) *physical & 0x0F))
	{
		kernelError(kernel_error, "Unable to get USB transfer descriptor "
			"memory");
		return (status = ERR_MEMORY);
	}

	// Map it to a virtual address
	status = kernelPageMapToFree(KERNELPROCID, *physical, (void **) descs,
		memSize);
	if (status < 0)
	{
		kernelError(kernel_error, "Unable to map USB transfer descriptor "
			"memory");
		kernelMemoryReleasePhysical(*physical);
		return (status);
	}

	// Make it non-cacheable
	status = kernelPageSetAttrs(KERNELPROCID, 1 /* set */,
		PAGEFLAG_CACHEDISABLE, (void *) *descs, memSize);
	if (status < 0)
	{
		kernelError(kernel_error, "Error setting page attrs");
		kernelMemoryReleasePhysical(*physical);
		return (status);
	}

	// Clear the memory, since the 'allocate physical' can't do it for us.
	kernelMemClear((void *) *descs, memSize);

	// Connect the descriptors
	for (count = 0; count < numDescs; count ++)
	{
		if (count)
			(*descs)[count].prev = &(*descs)[count - 1];
		if (count < (numDescs - 1))
			(*descs)[count].next = &(*descs)[count + 1];
	}

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


static void deallocUhciMemory(usbController *controller)
{
	usbUhciData *uhciData = controller->data;
	void *queueHeadsPhysical = NULL;

	if (uhciData)
	{
		if (uhciData->frameList)
			kernelPageUnmap(KERNELPROCID, uhciData->frameList,
				USBUHCI_FRAMELIST_MEMSIZE);

		if (uhciData->frameListPhysical)
			kernelMemoryReleasePhysical(uhciData->frameListPhysical);

		if (uhciData->queueHeads[0])
		{
			queueHeadsPhysical =
			kernelPageGetPhysical(KERNELPROCID,
				(void *) uhciData->queueHeads[0]);
			kernelPageUnmap(KERNELPROCID, (void *) uhciData->queueHeads[0],
				USBUHCI_QUEUEHEADS_MEMSIZE);
			if (queueHeadsPhysical)
				kernelMemoryReleasePhysical(queueHeadsPhysical);
		}

		if (uhciData->termTransDesc)
			deallocTransDescs(uhciData->termTransDesc, 1);

		kernelFree(uhciData);
		controller->data = NULL;
	}
}


static int allocUhciMemory(usbController *controller)
{
	// Allocate all of the memory bits specific to the the UHCI controller.

	int status = 0;
	usbUhciData *uhciData = controller->data;
	void *queueHeadsPhysical = NULL;
	usbUhciQueueHead *queueHeads = NULL;
	void *transDescPhysical = NULL;
	int count;

	// Allocate the UHCI hub's private data

	// Allocate the USB frame list.  USBUHCI_NUM_FRAMES (1024) 32-bit values,
	// so one page of memory, page-aligned.  We need to put the physical
	// address into the register.

	uhciData->frameListPhysical =
		kernelMemoryGetPhysical(USBUHCI_FRAMELIST_MEMSIZE, MEMORY_PAGE_SIZE,
			"usb frame list");
	if (!uhciData->frameListPhysical ||
		((unsigned) uhciData->frameListPhysical & 0x0FFF))
	{
		kernelError(kernel_error, "Unable to get USB frame list memory");
		status = ERR_MEMORY;
		goto err_out;
	}

	// Map it to a virtual address
	status =
		kernelPageMapToFree(KERNELPROCID, uhciData->frameListPhysical,
			(void **) &(uhciData->frameList),
			USBUHCI_FRAMELIST_MEMSIZE);
	if (status < 0)
	{
		kernelError(kernel_error, "Unable to map USB frame list memory");
		goto err_out;
	}

	// Fill the list with 32-bit 'term' (1) values, indicating that all pointers
	// are currently invalid
	kernelProcessorWriteDwords(USBUHCI_LINKPTR_TERM, uhciData->frameList,
		USBUHCI_NUM_FRAMES);

	// Allocate an array of USBUHCI_NUM_QUEUEHEADS queue heads, page-aligned.

	queueHeadsPhysical =
		kernelMemoryGetPhysical(USBUHCI_QUEUEHEADS_MEMSIZE, MEMORY_PAGE_SIZE,
			"usb queue heads");
	if (!queueHeadsPhysical || ((unsigned) queueHeadsPhysical & 0x0F))
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

	// Clear the memory, since the 'allocate physical' can't do it for us.
	kernelMemClear((void *) queueHeads, USBUHCI_QUEUEHEADS_MEMSIZE);

	// Assign the queue head pointers and set the link pointers invalid
	for (count = 0; count < USBUHCI_NUM_QUEUEHEADS; count ++)
	{
		uhciData->queueHeads[count] = &queueHeads[count];
		uhciData->queueHeads[count]->linkPointer = USBUHCI_LINKPTR_TERM;
		uhciData->queueHeads[count]->element = USBUHCI_LINKPTR_TERM;
		uhciData->queueHeads[count]->saveElement =
			uhciData->queueHeads[count]->element;
	}

	// Allocate a blank transfer descriptor to attach to the terminating queue
	// head
	status = allocTransDescs(1, &transDescPhysical, &(uhciData->termTransDesc));
	if (status < 0)
		goto err_out;

	// Success
	return (status = 0);

err_out:
	deallocUhciMemory(controller);
	return (status);
}


static int queueDescriptors(usbController *controller,
	usbUhciQueueHead *queueHead, usbUhciTransDesc *descs, unsigned numDescs)
{
	// Attach the supplied transfer descriptor(s) to the supplied queue head.

	int status = 0;
	unsigned descPhysical = NULL;
	unsigned firstPhysical = NULL;
	unsigned count;

	// Check params
	if (!controller || !queueHead || !descs)
	{
		kernelError(kernel_error, "NULL parameter");
		return (status = ERR_NULLPARAMETER);
	}

	kernelDebug(debug_usb, "UHCI queue transaction with %d transfers", numDescs);

	// Process the transfer descriptors
	for (count = 0; count < numDescs; count ++)
	{
		// Isochronous?
		if (descs[count].contStatus & USBUHCI_TDCONTSTAT_ISOC)
		{
			kernelError(kernel_error, "Isochronous transfers not yet supported");
			return (status = ERR_NOTIMPLEMENTED);
		}

		// Get the physical address of the TD
		descPhysical = (unsigned)
		kernelPageGetPhysical(KERNELPROCID, (void *) &(descs[count]));
		if (!descPhysical)
		{
			kernelError(kernel_error, "Can't get xfer descriptor physical "
				"address");
			return (status = ERR_MEMORY);
		}
		if (descPhysical & 0xF)
		{
			kernelError(kernel_error, "Xfer descriptor not 16-byte aligned");
			return (status = ERR_ALIGN);
		}

		if (count)
			// Attach this TD to the previous TD.
			descs[count - 1].linkPointer =
				(descPhysical | USBUHCI_LINKPTR_DEPTHFIRST);
		else
			// Save the address because we'll attach it to the queue head in
			// a minute.
			firstPhysical = descPhysical;

		// Blank the descriptor's link pointer and set the 'terminate' bit
		descs[count].linkPointer = USBUHCI_LINKPTR_TERM;
	}

	// Everything's queued up.  Lock the controller.
	status = kernelLockGet(&controller->lock);
	if (status < 0)
	{
		kernelError(kernel_error, "Can't get controller lock");
		return (status);
	}

	// Attach the first TD to the queue head

	// Are there existing descriptors in the queue?  If so, link them to the
	// end of our descriptors.
	if (queueHead->transDescs)
	{
		kernelDebug(debug_usb, "UHCI existing descriptors in the queue");
		descs[numDescs - 1].linkPointer =
			((queueHead->saveElement & 0xFFFFFFF0) | USBUHCI_LINKPTR_DEPTHFIRST);
		descs[numDescs - 1].next = queueHead->transDescs;
		descs[numDescs - 1].next->prev = &descs[numDescs - 1];
	}

	// Point the queue head at our descriptors.
	queueHead->element = firstPhysical;
	queueHead->saveElement = queueHead->element;
	queueHead->transDescs = descs;

	kernelLockRelease(&controller->lock);
	return (status = 0);
}


static int deQueueDescriptors(usbController *controller,
	usbUhciQueueHead *queueHead, usbUhciTransDesc *descs, unsigned numDescs)
{
	// Given string of queued transfer descriptors, detach them from the queue
	// head

	int status = 0;

	// Lock the controller.
	status = kernelLockGet(&controller->lock);
	if (status < 0)
	{
		kernelError(kernel_error, "Can't get controller lock");
		return (status);
	}

	if (descs[numDescs - 1].next)
		descs[numDescs - 1].next->prev = descs[0].prev;

	// Are our descriptors at the head of the queue?
	if (queueHead->transDescs == descs)
	{
		if (descs[numDescs - 1].next)
		{
			queueHead->element = (descs[numDescs - 1].linkPointer & 0xFFFFFFF0);
			queueHead->saveElement = queueHead->element;
			queueHead->transDescs = descs[numDescs - 1].next;
			descs[numDescs - 1].next->prev = NULL;
		}
		else
		{
			queueHead->element = USBUHCI_LINKPTR_TERM;
			queueHead->saveElement = queueHead->element;
			queueHead->transDescs = NULL;
		}
	}
	else
	{
		if (descs[numDescs - 1].next)
		{
			descs[0].prev->linkPointer = descs[numDescs - 1].linkPointer;
			descs[0].prev->next = descs[numDescs - 1].next;
			descs[numDescs - 1].next->prev = descs[0].prev;
		}
		else
		{
			descs[0].prev->linkPointer = USBUHCI_LINKPTR_TERM;
			descs[0].prev->next = NULL;
		}
	}

	kernelLockRelease(&controller->lock);
	return (status = 0);
}


static int runQueue(usbUhciTransDesc *descs, unsigned numDescs)
{
	// Given a list of transfer descriptors associated with a single transaction,
	// queue them up on the controller.

	int status = 0;
	unsigned prevFirstActive = 0;
	unsigned firstActive = 0;
	unsigned currTime = 0;
	unsigned activeTime = 0;
	int active = 0;
	int error = 0;
	unsigned count;

	kernelDebug(debug_usb, "UHCI run transaction with %d transfers", numDescs);

	currTime = kernelSysTimerRead();
	activeTime = currTime;

	// Now wait while some TD is active, or until we detect an error
	while (1)
	{
		active = 0;

		// See if there are still any active TDs, or if any have an error
		for (count = 0; count < numDescs; count ++)
		{
			if (descs[count].contStatus & USBUHCI_TDCONTSTAT_ACTIVE)
			{
				active = 1;
				firstActive = count;
				break;
			}
			else if (descs[count].contStatus & USBUHCI_TDCONTSTAT_ERROR)
			{
				kernelDebugError("Transaction error on TD %d contStatus=%08x "
					"first active=%d", count,
					descs[count].contStatus, firstActive);
				debugTransError(&descs[count]);
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
				kernelDebug(debug_usb, "UHCI transaction completed "
					"successfully");
				status = 0;
			}

			break;
		}

		currTime = kernelSysTimerRead();

		// If the controller is moving through the queue, reset the timeout
		if (firstActive != prevFirstActive)
		{
			activeTime = currTime;
			prevFirstActive = firstActive;
		}
		if (currTime > (activeTime + 200))
		{
			// Software timeout after ~10 seconds per descriptor, or ~5 seconds
			// for the whole operation.
			kernelDebugError("Software timeout on TD %d", firstActive);
			status = ERR_NODATA;
			break;
		}

		// Yielding here really hits performance.  Perhaps need to experiment
		// with interrupt-driven system.
		// kernelMultitaskerYield();
	}

	return (status);
}


static int allocTransDescBuffer(usbUhciTransDesc *desc, unsigned buffSize)
{
	// Allocate a data buffer for a transfer descriptor.

	int status = 0;

	desc->buffVirtual = kernelMalloc(buffSize);
	if (!desc->buffVirtual)
	{
		kernelDebugError("Can't alloc trans desc buffer size %u", buffSize);
		return (status = ERR_MEMORY);
	}

	// Get the physical address of this memory
	desc->buffer = kernelPageGetPhysical(KERNELPROCID, desc->buffVirtual);
	if (!desc->buffer)
	{
		kernelDebugError("Can't get buffer physical address");
		kernelFree(desc->buffVirtual);
		return (status = ERR_MEMORY);
	}

	desc->buffSize = buffSize;
	return (status = 0);
}


static int setupTransDesc(usbUhciTransDesc *desc, usbXferType type,
	int address, int endpoint, usbDevSpeed speed, char dataToggle,
	unsigned char pid)
{
	// Do the nuts-n-bolts setup for a transfer descriptor

	int status = 0;
	const char *pidString __attribute__((unused)) = NULL;

	// Initialize the 'control and status' field.
	desc->contStatus = (USBUHCI_TDCONTSTAT_ACTIVE | USBUHCI_TDCONTSTAT_ERRCNT);
	if (type == usbxfer_isochronous)
		desc->contStatus |= USBUHCI_TDCONTSTAT_ISOC;
	if (type == usbxfer_interrupt)
		desc->contStatus |= USBUHCI_TDCONTSTAT_IOC;
	if (speed == usbspeed_low)
		desc->contStatus |= USBUHCI_TDCONTSTAT_LSPEED;

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

	kernelDebug(debug_usb, "UHCI setup transfer %s address %d:%02x, %d bytes, "
		"dataToggle %d", pidString, address, endpoint, desc->buffSize,
		dataToggle);

	return (status = 0);
}


static int queue(usbController *controller, usbDevice *usbDev,
	usbTransaction *trans, int numTrans)
{
	// This function contains the intelligence necessary to initiate a
	// transaction (all phases)

	int status = 0;
	unsigned bytesPerTransfer = 0;
	usbEndpointDesc *endpoint = NULL;
	int numDescs = 0;
	void *descsPhysical = NULL;
	usbUhciTransDesc *descs = NULL;
	usbUhciData *uhciData = controller->data;
	usbUhciQueueHead *queueHead = NULL;
	usbUhciTransDesc *setupDesc = NULL;
	usbDeviceRequest *req = NULL;
	const char *opString __attribute__((unused)) = NULL;
	volatile unsigned char *dataToggle = NULL;
	usbUhciTransDesc *dataDesc = NULL;
	unsigned bytesToTransfer = 0;
	usbUhciTransDesc *statusDesc = NULL;
	void *buffer = NULL;
	int descCount = 0;
	int count;

	// Check params
	if (!controller || !usbDev || !trans)
	{
		kernelError(kernel_error, "NULL parameter");
		return (status = ERR_NULLPARAMETER);
	}

	// Figure out how many transfer descriptors we're going to need for the
	// transactions
	for (count = 0; count < numTrans; count ++)
	{
		if (trans[count].type == usbxfer_control)
			// At least one each for setup and status
			numDescs += 2;

		if (trans[count].length)
		{
			// Figure out the maximum number of bytes per transfer, depending on
			// the endpoint we're addressing.
			if (!trans[count].endpoint && !usbDev->numEndpoints)
				bytesPerTransfer = usbDev->deviceDesc.maxPacketSize0;
			else
			{
				endpoint = kernelUsbGetEndpointDesc(usbDev,
					trans[count].endpoint);
				if (!endpoint)
				{
					kernelError(kernel_error, "No such endpoint %02x",
						trans[count].endpoint);
					status = ERR_NOSUCHFUNCTION;
					goto out;
				}

				bytesPerTransfer = endpoint->maxPacketSize;
			}

			// If we haven't yet got the descriptors, etc., 8 is the minimum size
			if (bytesPerTransfer < 8)
			{
				kernelDebug(debug_usb, "UHCI using minimum endpoint transfer "
					"size 8 instead of %d for endpoint %02x",
				bytesPerTransfer, trans[count].endpoint);
				bytesPerTransfer = 8;
			}

			numDescs += ((trans[count].length + (bytesPerTransfer - 1)) /
				bytesPerTransfer);
		}
	}

	// Get memory for the transfer descriptors
	kernelDebug(debug_usb, "UHCI transaction requires %d descriptors", numDescs);
	status = allocTransDescs(numDescs, &descsPhysical, &descs);
	if (status < 0)
		goto out;

	if (trans[0].type == usbxfer_control)
		// Use the control queue head
		queueHead = uhciData->queueHeads[USBUHCI_QH_CONTROL];
	else if (trans[0].type == usbxfer_bulk)
		// Use the bulk queue head
		queueHead = uhciData->queueHeads[USBUHCI_QH_BULK];
	else
	{
		kernelError(kernel_error, "Unsupported transaction type %d",
			trans[0].type);
		status = ERR_NOTIMPLEMENTED;
		goto out;
	}

	for (count = 0; count < numTrans; count ++)
	{
		// Get the data toggle for the endpoint
		dataToggle =
			kernelUsbGetEndpointDataToggle(usbDev, trans[count].endpoint);
		if (!dataToggle)
		{
			kernelError(kernel_error, "No data toggle for endpoint %02x",
				trans[count].endpoint);
			status = ERR_NOSUCHFUNCTION;
			goto out;
		}

		if (trans[count].type == usbxfer_control)
		{
			// Get the transfer descriptor for the setup phase
			setupDesc = &(descs[descCount++]);

			// Begin setting up the device request

			// Get a buffer for the device request memory
			status = allocTransDescBuffer(setupDesc, sizeof(usbDeviceRequest));
			if (status < 0)
				goto out;
			req = setupDesc->buffVirtual;

			status = kernelUsbSetupDeviceRequest(&trans[count], req);
			if (status < 0)
				goto out;

			// Data toggle is always 0 for the setup transfer
			*dataToggle = 0;

			// Setup the transfer descriptor for the setup phase
			status = setupTransDesc(setupDesc, trans[count].type,
				trans[count].address, trans[count].endpoint,
				usbDev->speed, *dataToggle, USB_PID_SETUP);
			if (status < 0)
				goto out;

			// Data toggle
			*dataToggle ^= 1;
		}

		// If there is a data phase, setup the transfer descriptor(s) for the
		// data phase
		if (trans[count].length)
		{
			buffer = trans[count].buffer;
			bytesToTransfer = trans[count].length;
			trans[count].bytes = 0; // Do this here?

			while (bytesToTransfer)
			{
				unsigned doBytes = min(bytesToTransfer, bytesPerTransfer);

				dataDesc = &(descs[descCount++]);

				// Point the data descriptor's buffer to the relevent portion
				// of the transaction buffer
				dataDesc->buffVirtual = buffer;
				dataDesc->buffer =
				kernelPageGetPhysical((((unsigned) dataDesc->buffVirtual <
					KERNEL_VIRTUAL_ADDRESS)?
						kernelCurrentProcess->processId :
						KERNELPROCID), dataDesc->buffVirtual);
				if (!dataDesc->buffer)
				{
					kernelDebugError("Can't get physical address for buffer "
						"fragment at %p", dataDesc->buffVirtual);
					status = ERR_MEMORY;
					goto out;
				}

				dataDesc->buffSize = doBytes;

				status =
					setupTransDesc(dataDesc, trans[count].type,
						trans[count].address, trans[count].endpoint,
						usbDev->speed, *dataToggle, trans[count].pid);
				if (status < 0)
					goto out;

				// Data toggle
				*dataToggle ^= 1;

				buffer += doBytes;
				bytesToTransfer -= doBytes;
				trans[count].bytes += doBytes; // Do this here?
			}
		}

		if (trans[count].type == usbxfer_control)
		{
			// Setup the transfer descriptor for the status phase

			statusDesc = &(descs[descCount++]);

			// Data toggle is always 1 for the status transfer
			*dataToggle = 1;

			// Setup the status packet
			status =
				setupTransDesc(statusDesc, trans[count].type,
					trans[count].address, trans[count].endpoint, usbDev->speed,
					*dataToggle, ((trans[count].pid == USB_PID_OUT)?
						USB_PID_IN : USB_PID_OUT));
			if (status < 0)
				goto out;
		}
	}

	// Queue the descriptors
	status = queueDescriptors(controller, queueHead, descs, numDescs);
	if (status < 0)
		goto out;

	// Run the transaction
	status = runQueue(descs, numDescs);
	if (status < 0)
	{
		// Check for errors
		for (count = 0; count < numDescs; count ++)
		{
			if (descs[count].contStatus & USBUHCI_TDCONTSTAT_ERROR)
			{
				status = ERR_IO;
				break;
			}
		}
	}

	// Dequeue the descriptors
	if (deQueueDescriptors(controller, queueHead, descs, numDescs) < 0)
		goto out;

out:
	if (setupDesc && setupDesc->buffVirtual)
		kernelFree(setupDesc->buffVirtual);

	if (descs)
		deallocTransDescs(descs, numDescs);

	return (status);
}


static usbUhciQueueHead *findIntQueueHead(usbController *controller,
	int interval)
{
	// Figure out which interrupt queue head to use, given an interval which
	// is a maximum frequency -- so we locate the first one which is less than
	// or equal to the specified interval.

	usbUhciData *uhciData = controller->data;
	int queues[8] = { 128, 64, 32, 16, 8, 4, 2, 1 };
	int count;

	for (count = 0; count < 8; count ++)
		if (queues[count] <= interval)
			return (uhciData->queueHeads[count]);

	// Should never fall through
	return (NULL);
}


static int schedInterrupt(usbController *controller, usbDevice *usbDev,
	unsigned char endpoint, int interval, unsigned maxLen,
	void (*callback)(usbDevice *, void *, unsigned))
{
	// This function is used to schedule an interrupt.

	int status = 0;
	usbUhciData *uhciData = NULL;
	usbUhciInterruptReg *intrReg = NULL;
	void *descPhysical = NULL;

	// Check params
	if (!controller || !usbDev || !callback)
	{
		kernelError(kernel_error, "NULL parameter");
		return (status = ERR_NULLPARAMETER);
	}

	kernelDebug(debug_usb, "UHCI schedule interrupt for device %d endpoint %02x "
		"interval %d len %u", usbDev->address, endpoint, interval, maxLen);

	uhciData = controller->data;

	// Get memory to hold info about the interrupt
	intrReg = kernelMalloc(sizeof(usbUhciInterruptReg));
	if (!intrReg)
		return (status = ERR_MEMORY);

	intrReg->usbDev = usbDev;

	// Find the appropriate interrupt queue head
	intrReg->queueHead = findIntQueueHead(controller, interval);
	if (!intrReg->queueHead)
	{
		kernelDebugError("Couldn't find QH for interrupt interval %d", interval);
		kernelFree(intrReg);
		return (status = ERR_BUG);
	}

	// Get a transfer descriptor for it.
	status = allocTransDescs(1, &descPhysical, &intrReg->transDesc);
	if (status < 0)
	{
		kernelFree(intrReg);
		return (status);
	}

	// Get the buffer for the transfer descriptor
	status = allocTransDescBuffer(intrReg->transDesc, maxLen);
	if (status < 0)
	{
		kernelFree(intrReg);
		return (status);
	}

	// Set up the transfer descriptor
	status =
		setupTransDesc(intrReg->transDesc, usbxfer_interrupt,
			usbDev->address, endpoint, usbDev->speed, 0, USB_PID_IN);
	if (status < 0)
	{
		kernelFree(intrReg);
		return (status);
	}

	intrReg->endpoint = endpoint;
	intrReg->interval = interval;
	intrReg->maxLen = maxLen;
	intrReg->callback = callback;

	// Add the interrupt registration to the hub's list.
	status = kernelLinkedListAdd(&uhciData->intrRegs, intrReg);
	if (status < 0)
	{
		kernelFree(intrReg);
		return (status);
	}

	// Queue the transfer descriptor on the queue head
	status =
		queueDescriptors(controller, intrReg->queueHead, intrReg->transDesc, 1);
	if (status < 0)
	{
		kernelFree(intrReg);
		return (status);
	}

	return (status);
}


static int unschedInterrupt(usbController *controller, usbDevice *usbDev)
{
	// This function is used to unschedule an interrupt.

	int status = ERR_NOSUCHENTRY;
	usbUhciData *uhciData = NULL;
	kernelLinkedListItem *iter = NULL;
	usbUhciInterruptReg *intrReg = NULL;

	// Check params
	if (!controller || !usbDev)
	{
		kernelError(kernel_error, "NULL parameter");
		return (status = ERR_NULLPARAMETER);
	}

	kernelDebug(debug_usb, "UHCI unschedule interrupt for device %d",
		usbDev->address);

	uhciData = controller->data;

	// Find the interrupt registration
	intrReg = kernelLinkedListIterStart(&uhciData->intrRegs, &iter);
	while (intrReg)
	{
		if (intrReg->usbDev == usbDev)
		{
			kernelLinkedListRemove(&uhciData->intrRegs, intrReg);

			if (intrReg->queueHead && intrReg->transDesc)
				deQueueDescriptors(controller, intrReg->queueHead,
					intrReg->transDesc, 1);

			// Free the memory
			if (intrReg->transDesc && intrReg->transDesc->buffVirtual)
				kernelFree(intrReg->transDesc->buffVirtual);
			kernelFree(intrReg);

			status = 0;
			break;
		}

		intrReg = kernelLinkedListIterNext(&uhciData->intrRegs, &iter);
	}

	return (status);
}


static int setup(usbController *controller)
{
	// Do USB setup

	int status = 0;
	unsigned char command = 0;
	usbUhciData *uhciData = controller->data;
	usbUhciQueueHead *intQueueHead = NULL;
	int count;

	// Stop the controller
	status = startStop(controller, 0);
	if (status < 0)
		return (status);

	// Reset the controller
	reset(controller);

	// Set interrupt mask.
	kernelProcessorOutPort16((uhciData->ioAddress + USBUHCI_PORTOFFSET_INTR),
		(USBUHCI_INTR_IOC | USBUHCI_INTR_TIMEOUTCRC));

	// Allocate memory
	status = allocUhciMemory(controller);
	if (status < 0)
		return (status);

	// Set up the queue heads
	for (count = 0; count < USBUHCI_NUM_QUEUEHEADS; count ++)
	{
		// Each queue head points to the one that follows it, except the
		// terminating queue head.
		if (count < (USBUHCI_NUM_QUEUEHEADS - 1))
		{
			uhciData->queueHeads[count]->linkPointer =
				((unsigned)(kernelPageGetPhysical(KERNELPROCID, (void *)
					uhciData->queueHeads[count + 1])) |
				USBUHCI_LINKPTR_QHEAD);
		}
		else
		{
			// The terminating queue head points back to control queue head
			// for bandwidth reclamation.
			uhciData->queueHeads[count]->linkPointer =
				uhciData->queueHeads[USBUHCI_QH_CONTROL - 1]->linkPointer;
		}
	}

	// Attach the terminating transfer descriptor to the terminating queue head
	uhciData->queueHeads[USBUHCI_QH_TERM]->element = (unsigned)
		kernelPageGetPhysical(KERNELPROCID, (void *) uhciData->termTransDesc);
	uhciData->queueHeads[USBUHCI_QH_TERM]->saveElement =
		uhciData->queueHeads[USBUHCI_QH_TERM]->element;
	uhciData->termTransDesc->linkPointer = USBUHCI_LINKPTR_TERM;

	// Point all frame list pointers at the appropriate queue heads.  Each
	// one will point to one of the interrupt queue heads, depending on
	// the interval of the frame (the modulus of the frame number).
	for (count = 0; count < USBUHCI_NUM_FRAMES; count ++)
	{
		if (!(count % 128))
			intQueueHead = uhciData->queueHeads[USBUHCI_QH_INT128];
		else if (!(count % 64))
			intQueueHead = uhciData->queueHeads[USBUHCI_QH_INT64];
		else if (!(count % 32))
			intQueueHead = uhciData->queueHeads[USBUHCI_QH_INT32];
		else if (!(count % 16))
			intQueueHead = uhciData->queueHeads[USBUHCI_QH_INT16];
		else if (!(count % 8))
			intQueueHead = uhciData->queueHeads[USBUHCI_QH_INT8];
		else if (!(count % 4))
			intQueueHead = uhciData->queueHeads[USBUHCI_QH_INT4];
		else if (!(count % 2))
			intQueueHead = uhciData->queueHeads[USBUHCI_QH_INT2];
		else
			// By default, use the 'int 1' queue head which gets run every frame
			intQueueHead = uhciData->queueHeads[USBUHCI_QH_INT1];

		uhciData->frameList[count] = (unsigned)
			kernelPageGetPhysical(KERNELPROCID, (void *) intQueueHead);
		uhciData->frameList[count] |= USBUHCI_LINKPTR_QHEAD;
	}

	// Put the physical address of the frame list into the frame list base
	// address register
	kernelProcessorOutPort32((uhciData->ioAddress + USBUHCI_PORTOFFSET_FLBASE),
		uhciData->frameListPhysical);

	command = readCommand(controller);
	// Clear: software debug
	command &= ~USBUHCI_CMD_SWDBG;
	// Set: max packet size to 64 bytes, configure flag
	command |= (USBUHCI_CMD_MAXP | USBUHCI_CMD_CF);
	writeCommand(controller, command);

	// Clear the frame number
	writeFrameNum(controller, 0);

	// Start the controller
	status = startStop(controller, 1);
	if (status < 0)
		return (status);

	return (status = 0);
}


static int interrupt(usbController *controller)
{
	// This function gets called when the controller issues an interrupt.

	unsigned char status = 0;
	usbUhciData *uhciData = controller->data;
	usbUhciInterruptReg *intrReg = NULL;
	kernelLinkedListItem *iter = NULL;
	unsigned bytes = 0;

	status = readStatus(controller);

	// Has an interrupt transfer occurred?
	if (status & USBUHCI_STAT_USBINT)
	{
		//kernelDebug(debug_usb, "UHCI device interrupt controller %d",
		//	  controller->num);

		// Loop through the registered interrupts for ones that are no longer
		// active.
		intrReg = kernelLinkedListIterStart(&uhciData->intrRegs, &iter);
		while (intrReg)
		{
			// If the transfer descriptor is no longer active, there might be
			// some data there for us.
			if (!(intrReg->transDesc->contStatus & USBUHCI_TDCONTSTAT_ACTIVE))
			{
				if (status & USBUHCI_STAT_ERRINT)
				{
					// If there was an error with this interrupt, remove it.
					unschedInterrupt(controller, intrReg->usbDev);
					// Restart list iteration
					intrReg =
						kernelLinkedListIterStart(&uhciData->intrRegs, &iter);
					continue;
				}

				bytes = (((intrReg->transDesc->contStatus &
					USBUHCI_TDCONTSTAT_ACTLEN) + 1) &
						USBUHCI_TDCONTSTAT_ACTLEN);

				// If there's data and a callback function, do the callback.
				if (bytes && intrReg->callback)
					intrReg->callback(intrReg->usbDev,
						intrReg->transDesc->buffVirtual, bytes);

				// Mark the transfer descriptor active again.
				intrReg->transDesc->contStatus &= ~USBUHCI_TDCONTSTAT_STATUS;
				intrReg->transDesc->contStatus |=
					(USBUHCI_TDCONTSTAT_ERRCNT | USBUHCI_TDCONTSTAT_IOC |
					USBUHCI_TDCONTSTAT_ACTIVE | USBUHCI_TDCONTSTAT_ACTLEN);
				intrReg->transDesc->tdToken ^= USBUHCI_TDTOKEN_DATATOGGLE;

				// Reset the queue head's element pointer.  Should probably
				// take a lock here.
				intrReg->queueHead->element = intrReg->queueHead->saveElement;
			}

			intrReg = kernelLinkedListIterNext(&uhciData->intrRegs, &iter);
		}
	}

	// Or was it an error interrupt?
	else if (status & USBUHCI_STAT_ERRINT)
	{
		kernelDebug(debug_usb, "UHCI error interrupt controller %d",
			controller->num);
		debugUhciRegs(controller);
	}

	else
	{
		kernelDebug(debug_usb, "UHCI no interrupt from controller %d",
			controller->num);
		return (ERR_NODATA);
	}

	// Clear the status register
	writeStatus(controller, status);
	return (0);
}


static void doDetectDevices(usbHub *hub, int hotplug)
{
	// Detect devices connected to the root hub

	usbController *controller = hub->controller;
	unsigned short status = 0;
	usbDevSpeed speed = usbspeed_unknown;
	int count;

	//kernelDebug(debug_usb, "UHCI detect devices");

	for (count = 0; count < 2; count ++)
	{
		status = readPortStatus(controller, count);

		if (status & USBUHCI_PORT_CONNCHG)
		{
			printPortStatus(controller);

			kernelDebug(debug_usb, "UHCI port %d connection changed", count);

			if (status & USBUHCI_PORT_CONNSTAT)
			{
				kernelDebug(debug_usb, "UHCI port %d connected", count);

				// Something connected, so wait 100ms
				kernelDebug(debug_usb, "UHCI delay after port status change");
				kernelCpuSpinMs(100);

				// Reset and enable the port
				portReset(controller, count);

				if (status & USBUHCI_PORT_LSDA)
					speed = usbspeed_low;
				else
					speed = usbspeed_full;

				if (kernelUsbDevConnect(controller, &controller->hub, count,
					speed, hotplug) < 0)
				{
					kernelError(kernel_error, "Error enumerating new USB "
						"device");
				}

				kernelDebug(debug_usb, "UHCI port %d is connected", count);
			}
			else
			{
				kernelUsbDevDisconnect(controller, &controller->hub, count);
				kernelDebug(debug_usb, "UHCI port %d is disconnected",
					count);
			}

			// Reset the port 'changed' bits by writing 1s to them.
			setPortStatusBits(controller, count, USBUHCI_PORT_RWC_BITS, 1);

			printPortStatus(controller);
		}
	}
}


static void detectDevices(usbHub *hub, int hotplug)
{
	// This function gets called once at startup to detect 'cold-plugged'
	// devices.

	kernelDebug(debug_usb, "UHCI initial device detection, hotplug=%d",
		hotplug);

	// Check params
	if (!hub)
	{
		kernelError(kernel_error, "NULL parameter");
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

	// Check params
	if (!hub)
	{
		kernelError(kernel_error, "NULL parameter");
		return;
	}

	// Only continue if we've already completed 'cold' device connection
	// detection.  Don't want to interfere with that.
	if (!hub->doneColdDetect)
		return;

	doDetectDevices(hub, 1 /* hotplug */);
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
//  Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


kernelDevice *kernelUsbUhciDetect(kernelBusTarget *busTarget,
	kernelDriver *driver)
{
	// This routine is used to detect and initialize each device, as well as
	// registering each one with any higher-level interfaces.

	int status = 0;
	pciDeviceInfo pciDevInfo;
	usbController *controller = NULL;
	usbUhciData *uhciData = NULL;
	kernelDevice *dev = NULL;

	// Get the PCI device header
	status = kernelBusGetTargetInfo(busTarget, &pciDevInfo);
	if (status < 0)
		goto err_out;

	// Make sure it's a non-bridge header
	if ((pciDevInfo.device.headerType & ~PCI_HEADERTYPE_MULTIFUNC) !=
		PCI_HEADERTYPE_NORMAL)
	{
		kernelDebug(debug_usb, "UHCI headertype not 'normal' (%02x)",
			(pciDevInfo.device.headerType & ~PCI_HEADERTYPE_MULTIFUNC));
		goto err_out;
	}

	// Make sure it's a UHCI controller (programming interface is 0 in the
	// PCI header)
	if (pciDevInfo.device.progIF != 0)
		goto err_out;

	// After this point, we believe we have a supported device.

	// Enable the device on the PCI bus as a bus master
	if ((kernelBusDeviceEnable(busTarget, PCI_COMMAND_IOENABLE) < 0) ||
		(kernelBusSetMaster(busTarget, 1) < 0))
	{
		goto err_out;
	}

	// Allocate memory for the controller
	controller = kernelMalloc(sizeof(usbController));
	if (!controller)
		goto err_out;

	// Get the USB version number
	controller->usbVersion = kernelBusReadRegister(busTarget, 0x60, 8);

	controller->interruptNum = pciDevInfo.device.nonBridge.interruptLine;

	kernelLog("USB: UHCI controller USB %d.%d interrupt %d",
		((controller->usbVersion & 0xF0) >> 4),
		(controller->usbVersion & 0xF), controller->interruptNum);

	// Allocate our private driver data
	controller->data = kernelMalloc(sizeof(usbUhciData));
	if (!controller->data)
		goto err_out;

	uhciData = controller->data;

	// Get the I/O space base address.  For USB, it comes in the 5th
	// PCI base address register
	uhciData->ioAddress =
		(void *)(kernelBusReadRegister(busTarget, 0x08, 32) & 0xFFFFFFE0);

	if (!uhciData->ioAddress)
	{
		kernelDebugError("Unknown USB controller I/O address");
		goto err_out;
	}

	// Disable legacy support
	kernelBusWriteRegister(busTarget, 0x60, 16, 0x2000);

	// Set up the controller
	status = setup(controller);
	if (status < 0)
	{
		kernelError(kernel_error, "Error setting up USB operation");
		goto err_out;
	}

	controller->hub.controller = controller;
	controller->hub.detectDevices = &detectDevices;
	controller->hub.threadCall = &threadCall;

	// Set controller function calls
	controller->reset = &reset;
	controller->interrupt = &interrupt;
	controller->queue = &queue;
	controller->schedInterrupt = &schedInterrupt;
	controller->unschedInterrupt = &unschedInterrupt;

	// Allocate memory for the kernel device
	dev = kernelMalloc(sizeof(kernelDevice));
	if (!dev)
		goto err_out;

	dev->device.class = kernelDeviceGetClass(DEVICECLASS_BUS);
	dev->device.subClass = kernelDeviceGetClass(DEVICESUBCLASS_BUS_USB);
	dev->driver = driver;
	dev->data = (void *) controller;

	// Initialize the variable list for attributes of the controller
	status = kernelVariableListCreate(&dev->device.attrs);
	if (status >= 0)
	{
		kernelVariableListSet(&dev->device.attrs, "controller.type", "UHCI");
		kernelVariableListSet(&dev->device.attrs, "controller.numPorts", "2");
	}

	status = kernelDeviceAdd(busTarget->bus->dev, dev);
	if (status < 0)
		goto err_out;

	// Finished
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

