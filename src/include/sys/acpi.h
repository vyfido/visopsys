// 
//  Visopsys
//  Copyright (C) 1998-2013 J. Andrew McLaughlin
//  
//  This library is free software; you can redistribute it and/or modify it
//  under the terms of the GNU Lesser General Public License as published by
//  the Free Software Foundation; either version 2.1 of the License, or (at
//  your option) any later version.
//
//  This library is distributed in the hope that it will be useful, but
//  WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
//  General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public License
//  along with this library; if not, write to the Free Software Foundation,
//  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
//
//  acpi.h
//

// This file contains definitions and structures defined by the ACPI
// power management standard.

#if !defined(_ACPI_H)

#define ACPI_SIG_RSDP         "RSD PTR "
#define ACPI_SIG_APIC         "APIC"
#define ACPI_SIG_DSDT         "DSDT"
#define ACPI_SIG_FACP         "FACP"
#define ACPI_SIG_FACS         "FACS"
#define ACPI_SIG_PSDT         "PSDT"
#define ACPI_SIG_RSDT         "RSDT"
#define ACPI_SIG_SSDT         "SSDT"
#define ACPI_SIG_SBST         "SBST"

#define ACPI_PMCTRL_SCI_EN    0x0001
#define ACPI_PMCTRL_BM_RLD    0x0002
#define ACPI_PMCTRL_GBL_RLS   0x0004
#define ACPI_PMCTRL_SLP_TYPX  0x1C00
#define ACPI_PMCTRL_SLP_EN    0x2000

//
// ACPI version 1.0x structures
//

typedef struct {
  char signature[8];
  unsigned char checksum;
  char oemId[6];
  unsigned char revision;
  void *rsdtAddr;

} __attribute__((packed)) acpiRsdp;

typedef struct {
  char signature[4];
  unsigned length;
  unsigned char revision;
  unsigned char checksum;
  char oemId[6];
  char oemTableId[8];
  unsigned oemRevision;
  unsigned creatorId;
  unsigned creatorRevision;

} __attribute__((packed)) acpiDescHeader;

typedef struct {
  acpiDescHeader header;
  void *entry[];

} __attribute__((packed)) acpiRsdt;

typedef struct {
  acpiDescHeader header;
  void *facsAddr;
  void *dsdtAddr;
  unsigned char intMode;
  unsigned char res1;
  unsigned short sciInt;
  unsigned sciCmdPort;
  unsigned char acpiEnable;
  unsigned char acpiDisable;
  unsigned char s4BiosReq;
  unsigned char res2;
  unsigned pm1aEventBlock;
  unsigned pm1bEventBlock;
  unsigned pm1aCtrlBlock;
  unsigned pm1bCtrlBlock;
  unsigned pm2CtrlBlock;
  unsigned pmTimerBlock;
  unsigned genEvent0Block;
  unsigned genEvent1Block;
  unsigned char pm1EventBlockLen;
  unsigned char pm1CtrlBlockLen;
  unsigned char pm2CtrlBlockLen;
  unsigned char pmTimerBlockLen;
  unsigned char genEvent0BlockLen;
  unsigned char genEvent1BlockLen;
  unsigned char genEvent1Bbase;
  unsigned char res3;
  unsigned short c2Latency;
  unsigned short c3Latency;
  unsigned short flushSize;
  unsigned short flushStride;
  unsigned char dutyOffset;
  unsigned char dutyWidth;
  unsigned char dayAlarm;
  unsigned char monthAlarm;
  unsigned char century;
  unsigned char res4[3];
  unsigned flags;

} __attribute__((packed)) acpiFacp;

typedef struct {
  char signature[4];
  unsigned length;
  unsigned hardwareSignature;
  void *wakingVector;
  unsigned globalLock;
  unsigned flags;
  unsigned char res[40];

} __attribute__((packed)) acpiFacs;

typedef struct {
  acpiDescHeader header;
  unsigned char data[];

} __attribute__((packed)) acpiDsdt;

#define _ACPI_H
#endif
