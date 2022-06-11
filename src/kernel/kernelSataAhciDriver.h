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
//  kernelSataAhciDriver.h
//

// This header file contains definitions for the kernel's AHCI SATA driver

#if !defined(_KERNELSATAAHCIDRIVER_H)


typedef volatile struct {
  unsigned CLB;
  unsigned CLBU;
  unsigned FB;
  unsigned FBU;
  unsigned IS;
  unsigned IE;
  unsigned CMD;
  unsigned res1;
  unsigned TFD;
  unsigned SIG;
  unsigned SSTS;
  unsigned SCTL;
  unsigned SERR;
  unsigned SACT;
  unsigned CI;
  unsigned SNTF;
  unsigned res2;
  unsigned res3[11];
  unsigned VS[4];

} __attribute__((packed)) ahciPort;

typedef volatile struct {

  // Generic host control
  unsigned caps;
  unsigned ghc;
  unsigned intStat;
  unsigned portsImpl;
  unsigned version;
  unsigned pad[3];

  unsigned reserved[32];
  unsigned vendSpec[24];

  // Ports
  ahciPort ports[32];

} __attribute__((packed)) ahciRegs;

typedef volatile struct {
  int interrupt;
  unsigned physMemSpace;
  unsigned memSpaceSize;
  ahciRegs *regs;

} ahciController;

#define _KERNELSATAAHCIDRIVER_
#endif
