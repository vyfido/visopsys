//
//  Visopsys
//  Copyright (C) 1998-2013 J. Andrew McLaughlin
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
//  kernelUsbUhciDriver.h
//

#if !defined(_KERNELUSBUHCIDRIVER_H)

#include "kernelLinkedList.h"

// USB UHCI Host controller port offsets
#define USBUHCI_PORTOFFSET_CMD		0x00
#define USBUHCI_PORTOFFSET_STAT		0x02
#define USBUHCI_PORTOFFSET_INTR		0x04
#define USBUHCI_PORTOFFSET_FRNUM	0x06
#define USBUHCI_PORTOFFSET_FLBASE	0x08
#define USBUHCI_PORTOFFSET_SOF		0x0C
#define USBUHCI_PORTOFFSET_PORTSC1	0x10
#define USBUHCI_PORTOFFSET_PORTSC2	0x12

// Bitfields for the USB UHCI command register
#define USBUHCI_CMD_MAXP			0x80
#define USBUHCI_CMD_CF				0x40
#define USBUHCI_CMD_SWDBG			0x20
#define USBUHCI_CMD_FGR				0x10
#define USBUHCI_CMD_EGSM			0x08
#define USBUHCI_CMD_GRESET			0x04
#define USBUHCI_CMD_HCRESET			0x02
#define USBUHCI_CMD_RUNSTOP			0x01

// Bitfields for the USB UHCI status register
#define USBUHCI_STAT_HCHALTED		0x20
#define USBUHCI_STAT_HCPERROR		0x10
#define USBUHCI_STAT_HSERROR		0x08
#define USBUHCI_STAT_RESDET			0x04
#define USBUHCI_STAT_ERRINT			0x02
#define USBUHCI_STAT_USBINT			0x01

// Bitfields for the USB UHCI interrupt enable register
#define USBUHCI_INTR_SPD			0x08
#define USBUHCI_INTR_IOC			0x04
#define USBUHCI_INTR_RESUME			0x02
#define USBUHCI_INTR_TIMEOUTCRC		0x01

// Bitfields for the 2 USB UHCI port registers
#define USBUHCI_PORT_SUSPEND		0x1000
#define USBUHCI_PORT_RESET			0x0200
#define USBUHCI_PORT_LSDA			0x0100
#define USBUHCI_PORT_RESDET			0x0040
#define USBUHCI_PORT_LINESTAT		0x0030
#define USBUHCI_PORT_ENABCHG		0x0008
#define USBUHCI_PORT_ENABLED		0x0004
#define USBUHCI_PORT_CONNCHG		0x0002
#define USBUHCI_PORT_CONNSTAT		0x0001
#define USBUHCI_PORT_RWC_BITS		(USBUHCI_PORT_ENABCHG | \
									USBUHCI_PORT_CONNCHG)

// Bitfields for link pointers
#define USBUHCI_LINKPTR_DEPTHFIRST	0x00000004
#define USBUHCI_LINKPTR_QHEAD		0x00000002
#define USBUHCI_LINKPTR_TERM		0x00000001

// Bitfields for transfer descriptors
#define USBUHCI_TDCONTSTAT_SPD		0x20000000
#define USBUHCI_TDCONTSTAT_ERRCNT	0x18000000
#define USBUHCI_TDCONTSTAT_LSPEED	0x04000000
#define USBUHCI_TDCONTSTAT_ISOC		0x02000000
#define USBUHCI_TDCONTSTAT_IOC		0x01000000
#define USBUHCI_TDCONTSTAT_STATUS	0x00FF0000
#define USBUHCI_TDCONTSTAT_ACTIVE	0x00800000
#define USBUHCI_TDCONTSTAT_ERROR	0x007E0000
#define USBUHCI_TDCONTSTAT_ESTALL	0x00400000
#define USBUHCI_TDCONTSTAT_EDBUFF	0x00200000
#define USBUHCI_TDCONTSTAT_EBABBLE	0x00100000
#define USBUHCI_TDCONTSTAT_ENAK		0x00080000
#define USBUHCI_TDCONTSTAT_ECRCTO	0x00040000
#define USBUHCI_TDCONTSTAT_EBSTUFF	0x00020000
#define USBUHCI_TDCONTSTAT_ACTLEN	0x000007FF
#define USBUHCI_TDTOKEN_MAXLEN		0xFFE00000
#define USBUHCI_TDTOKEN_DATATOGGLE	0x00080000
#define USBUHCI_TDTOKEN_ENDPOINT	0x00078000
#define USBUHCI_TDTOKEN_ADDRESS		0x00007F00
#define USBUHCI_TDTOKEN_PID			0x000000FF
#define USBUHCI_TD_NULLDATA			0x000007FF

// For the queue heads array
#define USBUHCI_QH_INT128			0
#define USBUHCI_QH_INT64			1
#define USBUHCI_QH_INT32			2
#define USBUHCI_QH_INT16			3
#define USBUHCI_QH_INT8				4
#define USBUHCI_QH_INT4				5
#define USBUHCI_QH_INT2				6
#define USBUHCI_QH_INT1				7
#define USBUHCI_QH_CONTROL			8
#define USBUHCI_QH_BULK				9
#define USBUHCI_QH_TERM				10

// Data structure memory sizes.  USBUHCI_QUEUEHEADS_MEMSIZE is below.
#define USBUHCI_NUM_FRAMES			1024
#define USBUHCI_FRAMELIST_MEMSIZE	(USBUHCI_NUM_FRAMES * sizeof(unsigned))
#define USBUHCI_NUM_QUEUEHEADS		11

typedef volatile struct _usbUhciTransDesc {
	unsigned linkPointer;
	unsigned contStatus;
	unsigned tdToken;
	void *buffer;
	// The last 4 dwords are reserved for our use, also helps ensure 16-byte
	// alignment.
	void *buffVirtual;
	unsigned buffSize;
	volatile struct _usbUhciTransDesc *prev;
	volatile struct _usbUhciTransDesc *next;

} __attribute__((packed)) __attribute__((aligned(16))) usbUhciTransDesc;

typedef volatile struct {
	unsigned linkPointer;
	unsigned element;
	// Our use, also helps ensure 16-byte alignment.
	unsigned saveElement;
	usbUhciTransDesc *transDescs;

} __attribute__((packed)) __attribute__((aligned(16))) usbUhciQueueHead;

// One memory page worth of queue heads
#define USBUHCI_QUEUEHEADS_MEMSIZE  (USBUHCI_NUM_QUEUEHEADS * \
	sizeof(usbUhciQueueHead))

typedef struct {
	usbDevice *usbDev;
	usbUhciQueueHead *queueHead;
	usbUhciTransDesc *transDesc;
	unsigned char endpoint;
	int interval;
	unsigned maxLen;
	void (*callback)(usbDevice *, void *, unsigned);

} usbUhciInterruptReg;

typedef struct {
	void *ioAddress;
	void *frameListPhysical;
	unsigned *frameList;
	usbUhciQueueHead *queueHeads[USBUHCI_NUM_QUEUEHEADS];
	usbUhciTransDesc *termTransDesc;  
	kernelLinkedList intrRegs;

} usbUhciData;

#define _KERNELUSBUHCIDRIVER_H
#endif
