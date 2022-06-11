//
//  Visopsys
//  Copyright (C) 1998-2020 J. Andrew McLaughlin
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
//  kernelNetworkTcp.h
//

#ifndef _KERNELNETWORKTCP_H
#define _KERNELNETWORKTCP_H

#include "kernelNetwork.h"

#define networkGetTcpHdrSize(tcpHdrP) \
	(((tcpHdrP)->dataOffsetFlags & 0xF0) >> 2)

#define networkSetTcpHdrSize(tcpHdrP, sz) \
	((tcpHdrP)->dataOffsetFlags = (((tcpHdrP)->dataOffsetFlags & 0xFF0F) | \
		(((sz) & 0x3C) << 2)))

#define networkGetTcpHdrFlags(tcpHdrP) \
	(((tcpHdrP)->dataOffsetFlags & 0x3F00) >> 8)

#define networkSetTcpHdrFlags(tcpHdrP, flgs) \
	((tcpHdrP)->dataOffsetFlags = (((tcpHdrP)->dataOffsetFlags & 0xC0FF) | \
 		(((flgs) & 0x3F) << 8)))

// Functions exported from kernelNetworkTcp.c
int kernelNetworkTcpOpenConnection(kernelNetworkConnection *);
int kernelNetworkTcpCloseConnection(kernelNetworkConnection *);
int kernelNetworkTcpSetupReceivedPacket(kernelNetworkPacket *);
int kernelNetworkTcpProcessPacket(kernelNetworkConnection *,
	kernelNetworkPacket *, int);
void kernelNetworkTcpPrependHeader(kernelNetworkPacket *);
void kernelNetworkTcpFinalizeSendPacket(kernelNetworkConnection *,
	kernelNetworkPacket *, int, int);
void kernelNetworkTcpSendState(kernelNetworkConnection *,
	kernelNetworkPacket *);
void kernelNetworkTcpProcessWaitQueue(kernelNetworkConnection *);
void kernelNetworkTcpThreadCall(kernelNetworkConnection *);

#endif

