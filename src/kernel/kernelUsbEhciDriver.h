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
//  kernelUsbEhciDriver.h
//

#if !defined(_KERNELUSBEHCIDRIVER_H)

#include <sys/types.h>

// Bitfields for the USB EHCI command register
#define USBEHCI_CMD_INTTHRCTL       0x00FF0000
#define USBEHCI_CMD_ASYNCSPME       0x00000800
#define USBEHCI_CMD_ASYNCSPMC       0x00000300
#define USBEHCI_CMD_LIGHTHCRESET    0x00000080
#define USBEHCI_CMD_INTASYNCADVRST  0x00000040
#define USBEHCI_CMD_ASYNCSCHEDENBL  0x00000020
#define USBEHCI_CMD_PERSCHEDENBL    0x00000010
#define USBEHCI_CMD_FRAMELISTSIZE   0x0000000C
#define USBEHCI_CMD_HCRESET         0x00000002
#define USBEHCI_CMD_RUNSTOP         0x00000001

typedef volatile struct {
  // Capability registers
  unsigned char capslen;
  unsigned char res;
  unsigned short hciver;
  unsigned hcsparams;
  unsigned hccparams;
  uquad_t hcsp_portroute;

} __attribute__((packed)) ehciCapRegs;

typedef volatile struct {
  // Operational registers
  unsigned cmd;
  unsigned sts;
  unsigned intr;
  unsigned frindex;
  unsigned ctrldseg;
  unsigned perlstbase;
  unsigned asynclstaddr;
  unsigned res[9];
  unsigned configflag;
  unsigned portsc[];

} __attribute__((packed)) ehciOpRegs;

typedef volatile struct {
  unsigned physMemSpace;
  unsigned memSpaceSize;
  ehciCapRegs *capRegs;
  ehciOpRegs *opRegs;

} usbEhciData;

#define _KERNELUSBEHCIDRIVER_H
#endif
