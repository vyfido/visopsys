//
//  Visopsys
//  Copyright (C) 1998-2015 J. Andrew McLaughlin
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
//  kernelLanceDriver.h
//

// Definitions for the driver for LANCE ethernet network adapters.  Based in
// part on a driver contributed by Jonas Zaddach: See the files in the
// directory contrib/jonas-net/src/kernel/

#if !defined(_KERNELLANCEDRIVER_H)

// The standard PCI device identifiers
#define LANCE_VENDOR_ID				0x1022
#define LANCE_DEVICE_ID				0x2000

// General constants
// Code for the number of ringbuffers:
// 2^LANCE_NUM_RINGBUFFERS_CODE == LANCE_NUM_RINGBUFFERS
#define LANCE_NUM_RINGBUFFERS_CODE	0x6  // 64 ring buffers
#define LANCE_NUM_RINGBUFFERS		(1 << LANCE_NUM_RINGBUFFERS_CODE)
#define LANCE_RINGBUFFER_SIZE		1536

// Port offsets in PC I/O space
#define LANCE_PORTOFFSET_PROM		0x00
#define LANCE_PORTOFFSET_RDP		0x10
#define LANCE_PORTOFFSET16_RAP		0x12
#define LANCE_PORTOFFSET16_RESET	0x14
#define LANCE_PORTOFFSET16_BDP		0x16
#define LANCE_PORTOFFSET16_VENDOR	0x18
#define LANCE_PORTOFFSET32_RAP		0x14
#define LANCE_PORTOFFSET32_RESET	0x18
#define LANCE_PORTOFFSET32_BDP		0x1C

// Control status register (CSR) and bus control register (BCR) numbers
// we care about
#define LANCE_CSR_STATUS			0
#define LANCE_CSR_IADR0				1
#define LANCE_CSR_IADR1				2
#define LANCE_CSR_IMASK				3
#define LANCE_CSR_FEAT				4
#define LANCE_CSR_EXTCTRL			5
#define LANCE_CSR_MODE				15
#define LANCE_CSR_STYLE				58
#define LANCE_CSR_MODEL1			88
#define LANCE_CSR_MODEL0			89
#define LANCE_BCR_MISC				2
#define LANCE_BCR_LINK				4
#define LANCE_BCR_BURST				18

// CSR0 status bits
#define LANCE_CSR_STATUS_ERR		0x8000
#define LANCE_CSR_STATUS_BABL		0x4000
#define LANCE_CSR_STATUS_CERR		0x2000
#define LANCE_CSR_STATUS_MISS		0x1000
#define LANCE_CSR_STATUS_MERR		0x0800
#define LANCE_CSR_STATUS_RINT		0x0400
#define LANCE_CSR_STATUS_TINT		0x0200
#define LANCE_CSR_STATUS_IDON		0x0100
#define LANCE_CSR_STATUS_INTR		0x0080
#define LANCE_CSR_STATUS_IENA		0x0040
#define LANCE_CSR_STATUS_RXON		0x0020
#define LANCE_CSR_STATUS_TXON		0x0010
#define LANCE_CSR_STATUS_TDMD		0x0008
#define LANCE_CSR_STATUS_STOP		0x0004
#define LANCE_CSR_STATUS_STRT		0x0002
#define LANCE_CSR_STATUS_INIT		0x0001

// CSR3 interrupt mask and deferral control bits
#define LANCE_CSR_IMASK_BABLM		0x4000
#define LANCE_CSR_IMASK_MISSM		0x1000
#define LANCE_CSR_IMASK_MERRM		0x0800
#define LANCE_CSR_IMASK_RINTM		0x0400
#define LANCE_CSR_IMASK_TINTM		0x0200
#define LANCE_CSR_IMASK_IDONM		0x0100
#define LANCE_CSR_IMASK_DXMT2PD		0x0010
#define LANCE_CSR_IMASK_EMBA		0x0008

// CSR4 test and features control bits
#define LANCE_CSR_FEAT_EN124		0x8000
#define LANCE_CSR_FEAT_DMAPLUS		0x4000
#define LANCE_CSR_FEAT_TIMER		0x2000
#define LANCE_CSR_FEAT_DPOLL		0x1000
#define LANCE_CSR_FEAT_APADXMT		0x0800
#define LANCE_CSR_FEAT_ASTRPRCV		0x0400
#define LANCE_CSR_FEAT_MFCO			0x0200
#define LANCE_CSR_FEAT_MFCOM		0x0100
#define LANCE_CSR_FEAT_UINTCMD		0x0080
#define LANCE_CSR_FEAT_UINT			0x0040
#define LANCE_CSR_FEAT_RCVCCO		0x0020
#define LANCE_CSR_FEAT_RCVCCOM		0x0010
#define LANCE_CSR_FEAT_TXSTRT		0x0008
#define LANCE_CSR_FEAT_TXSTRTM		0x0004
#define LANCE_CSR_FEAT_JAB			0x0002
#define LANCE_CSR_FEAT_JABM			0x0001

// CSR15 mode bits
#define LANCE_CSR_MODE_PROM			0x8000
#define LANCE_CSR_MODE_DRCVBC		0x4000
#define LANCE_CSR_MODE_DRCVPA		0x2000
#define LANCE_CSR_MODE_DLNKTST		0x1000
#define LANCE_CSR_MODE_DAPC			0x0800
#define LANCE_CSR_MODE_MENDECL		0x0400
#define LANCE_CSR_MODE_LRTTSEL		0x0200
#define LANCE_CSR_MODE_PORTSEL1		0x0100
#define LANCE_CSR_MODE_PORTSEL0		0x0080
#define LANCE_CSR_MODE_INTL			0x0040
#define LANCE_CSR_MODE_DRTY			0x0020
#define LANCE_CSR_MODE_FCOLL		0x0010
#define LANCE_CSR_MODE_DXMTFCS		0x0008
#define LANCE_CSR_MODE_LOOP			0x0004
#define LANCE_CSR_MODE_DTX			0x0002
#define LANCE_CSR_MODE_DRX			0x0001

// BCR20 led status bits we care about
#define LANCE_BCR_LINK_LEDOUT		0x0080

// Flags in transmit/receive ring descriptors
#define LANCE_DESCFLAG_OWN			0x80
#define LANCE_DESCFLAG_ERR			0x40
#define LANCE_DESCFLAG_TRANS_ADD	0x20
#define LANCE_DESCFLAG_RECV_FRAM	0x20
#define LANCE_DESCFLAG_TRANS_MORE	0x10
#define LANCE_DESCFLAG_RECV_OFLO	0x10
#define LANCE_DESCFLAG_TRANS_ONE	0x08
#define LANCE_DESCFLAG_RECV_CRC		0x08
#define LANCE_DESCFLAG_TRANS_DEF	0x04
#define LANCE_DESCFLAG_RECV_BUFF	0x04
#define LANCE_DESCFLAG_STP			0x02
#define LANCE_DESCFLAG_ENP			0x01
// More flags from transmit descriptors only
#define LANCE_DESCFLAG_TRANS_UFLO	0x40
#define LANCE_DESCFLAG_TRANS_LCOL	0x10
#define LANCE_DESCFLAG_TRANS_LCAR	0x80
#define LANCE_DESCFLAG_TRANS_RTRY	0x40

typedef enum {
	op_or, op_and
} opType;

typedef volatile struct {
	unsigned short buffAddrLow;
	unsigned char buffAddrHigh;
	unsigned char flags;
	short bufferSize;
	unsigned short messageSize;

} __attribute__((packed)) lanceRecvDesc16;

typedef volatile struct {
	unsigned short buffAddrLow;
	unsigned char buffAddrHigh;
	unsigned char flags;
	short bufferSize;
	unsigned short transFlags;

} __attribute__((packed)) lanceTransDesc16;

typedef struct {
  int head;
	int tail;
	union {
		lanceRecvDesc16 *recv;
		lanceTransDesc16 *trans;
	} desc;
	unsigned char *buffers[LANCE_NUM_RINGBUFFERS];

} lanceRing;

typedef struct {
	void *ioAddress;
	unsigned ioSpaceSize;
	void *memoryAddress;
	unsigned memorySize;
	unsigned chipVersion;
	lanceRing recvRing;
	lanceRing transRing;

} lanceDevice;

typedef struct {
	unsigned short mode;
	unsigned char physAddr[6];
	unsigned short addressFilter[4];
	unsigned short recvDescLow;
	unsigned char recvDescHigh;
	unsigned char recvRingLen;
	unsigned short transDescLow;
	unsigned char transDescHigh;
	unsigned char transRingLen;

} __attribute__((packed)) lanceInitBlock16;

#define _KERNELLANCEDRIVER_H
#endif

