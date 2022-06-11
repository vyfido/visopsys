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
//  kernelUsbUhciDriver.h
//

#if !defined(_KERNELUSBUHCIDRIVER_H)

// USB UHCI Host controller port offsets
#define USBUHCI_PORTOFFSET_CMD      0x00
#define USBUHCI_PORTOFFSET_STAT     0x02
#define USBUHCI_PORTOFFSET_INTR     0x04
#define USBUHCI_PORTOFFSET_FRNUM    0x06
#define USBUHCI_PORTOFFSET_FLBASE   0x08
#define USBUHCI_PORTOFFSET_SOF      0x0C
#define USBUHCI_PORTOFFSET_PORTSC1  0x10
#define USBUHCI_PORTOFFSET_PORTSC2  0x12

// Bitfields for the USB UHCI command register
#define USBUHCI_CMD_MAXP            0x80
#define USBUHCI_CMD_CF              0x40
#define USBUHCI_CMD_SWDBG           0x20
#define USBUHCI_CMD_FGR             0x10
#define USBUHCI_CMD_EGSM            0x08
#define USBUHCI_CMD_GRESET          0x04
#define USBUHCI_CMD_HCRESET         0x02
#define USBUHCI_CMD_RUNSTOP         0x01

// Bitfields for the USB UHCI status register
#define USBUHCI_STAT_HCHALTED       0x20
#define USBUHCI_STAT_HCPERROR       0x10
#define USBUHCI_STAT_HSERROR        0x08
#define USBUHCI_STAT_RESDET         0x04
#define USBUHCI_STAT_ERRINT         0x02
#define USBUHCI_STAT_USBINT         0x01

// Bitfields for the 2 USB UHCI port registers
#define USBUHCI_PORT_SUSPEND        0x1000
#define USBUHCI_PORT_RESET          0x0200
#define USBUHCI_PORT_LSDA           0x0100
#define USBUHCI_PORT_RESDET         0x0040
#define USBUHCI_PORT_LINESTAT       0x0030
#define USBUHCI_PORT_ENABCHG        0x0008
#define USBUHCI_PORT_ENABLED        0x0004
#define USBUHCI_PORT_CONNCHG        0x0002
#define USBUHCI_PORT_CONNSTAT       0x0001

// Bitfields for link pointers
#define USBUHCI_LINKPTR_QHEAD       0x00000002
#define USBUHCI_LINKPTR_TERM        0x00000001

// Bitfields for transfer descriptors
#define USBUHCI_TDLINK_DEPTHFIRST   0x00000004
#define USBUHCI_TDCONTSTAT_SPD      0x20000000
#define USBUHCI_TDCONTSTAT_ERRCNT   0x18000000
#define USBUHCI_TDCONTSTAT_LSPEED   0x04000000
#define USBUHCI_TDCONTSTAT_ISOC     0x02000000
#define USBUHCI_TDCONTSTAT_INTR     0x01000000
#define USBUHCI_TDCONTSTAT_STATUS   0x00FF0000
#define USBUHCI_TDCONTSTAT_ACTIVE   0x00800000
#define USBUHCI_TDCONTSTAT_ERROR    0x007E0000
#define USBUHCI_TDCONTSTAT_ESTALL   0x00400000
#define USBUHCI_TDCONTSTAT_EDBUFF   0x00200000
#define USBUHCI_TDCONTSTAT_EBABBLE  0x00100000
#define USBUHCI_TDCONTSTAT_ENAK     0x00080000
#define USBUHCI_TDCONTSTAT_ECRCTO   0x00040000
#define USBUHCI_TDCONTSTAT_EBSTUFF  0x00020000
#define USBUHCI_TDCONTSTAT_ACTLEN   0x000007FF
#define USBUHCI_TDTOKEN_MAXLEN      0xFFE00000
#define USBUHCI_TDTOKEN_DATATOGGLE  0x00080000
#define USBUHCI_TDTOKEN_ENDPOINT    0x00078000
#define USBUHCI_TDTOKEN_ADDRESS     0x00007F00
#define USBUHCI_TDTOKEN_PID         0x000000FF
#define USBUHCI_TD_NULLDATA         0x000007FF

typedef volatile struct {
  unsigned linkPointer;
  unsigned element;

} __attribute__((packed)) __attribute((aligned(16))) usbUhciQueueHead;

// One memory page worth of queue heads
#define USBUHCI_NUM_QUEUEHEADS      4
#define USBUHCI_QUEUEHEADS_MEMSIZE  (USBUHCI_NUM_QUEUEHEADS * \
				     sizeof(usbUhciQueueHead))

typedef volatile struct {
  unsigned linkPointer;
  unsigned contStatus;
  unsigned tdToken;
  void *buffer;
  // The last 4 dwords are reserved for our use; we use the first one to
  // store the virtual address of the buffer pointer.  The second one is
  // the buffer length
  void *buffVirtAddr;
  unsigned buffSize;

} __attribute__((packed)) __attribute((aligned(16))) usbUhciTransDesc;

typedef struct {
  void *frameListPhysical;
  unsigned *frameList;
  usbUhciQueueHead *intrQueueHead;
  usbUhciQueueHead *controlQueueHead;
  usbUhciQueueHead *bulkQueueHead;
  usbUhciQueueHead *termQueueHead;
  usbUhciTransDesc *termTransDesc;  
  
} usbUhciData;


#define _KERNELUSBUHCIDRIVER_H
#endif
