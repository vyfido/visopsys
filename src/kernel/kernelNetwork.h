//
//  Visopsys
//  Copyright (C) 1998-2017 J. Andrew McLaughlin
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
//  kernelNetwork.h
//

#if !defined(_KERNELNETWORK_H)

#include "kernelStream.h"
#include "kernelLock.h"
#include <sys/network.h>

#define NETWORK_DATASTREAM_LENGTH		0xFFFF

// Number of ARP items cached per network adapter.
#define NETWORK_ARPCACHE_SIZE			64

// This broadcast address works for both ethernet and IP
#define NETWORK_BROADCAST_ADDR \
	((networkAddress){ { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0, 0 } })

// A structure to describe and point to sections inside a buffer of packet
// data
typedef struct {
	networkAddress srcAddress;
	int srcPort;
	networkAddress destAddress;
	int destPort;
	int linkProtocol;
	int netProtocol;
	int transProtocol;
	void *memory;
	unsigned length;
	void *linkHeader;
	void *netHeader;
	void *transHeader;
	unsigned char *data;
	unsigned dataLength;

} kernelNetworkPacket;

// Items in the network adapter's ARP cache
typedef struct {
	networkAddress logicalAddress;
	networkAddress physicalAddress;

} kernelArpCacheItem;

// This represents a queue of network packets as a stream.
typedef stream kernelNetworkPacketStream;

typedef struct {
	int leaseExpiry;
	networkDhcpPacket dhcpPacket;

} kernelDhcpConfig;

// The network adapter structure
typedef volatile struct {
	networkDevice device;
	kernelDhcpConfig dhcpConfig;
	lock adapterLock;
	kernelArpCacheItem arpCache[NETWORK_ARPCACHE_SIZE];
	int numArpCaches;
	kernelNetworkPacketStream inputStream;
	lock inputStreamLock;
	kernelNetworkPacketStream outputStream;
	lock outputStreamLock;
	volatile struct _kernelNetworkConnection *connections;
	unsigned char buffer[NETWORK_PACKET_MAX_LENGTH];

	// Driver-specific private data.
	void *data;

} kernelNetworkDevice;

// This structure holds everything that's needed to keep track of a
// linked list of network 'connections', including a key code to identify
// it and packet input and output streams.
typedef volatile struct _kernelNetworkConnection {
	int processId;
	int mode;
	networkAddress address;
	networkFilter filter;
	lock inputStreamLock;
	networkStream inputStream;
	kernelNetworkDevice *adapter;
	struct {
		unsigned short identification;
	} ip;

	volatile struct _kernelNetworkConnection *previous;
	volatile struct _kernelNetworkConnection *next;

} kernelNetworkConnection;

// Functions exported from kernelNetwork.c
int kernelNetworkRegister(kernelNetworkDevice *);
kernelNetworkConnection *kernelNetworkConnectionOpen(kernelNetworkDevice *,
	int, networkAddress *, networkFilter *);
int kernelNetworkConnectionClose(kernelNetworkConnection *);
int kernelNetworkSetupReceivedPacket(kernelNetworkPacket *);
int kernelNetworkSendData(kernelNetworkConnection *, unsigned char *,
	unsigned, int, int);
// More functions, but also exported to user space
int kernelNetworkInitialized(void);
int kernelNetworkInitialize(void);
int kernelNetworkShutdown(void);
kernelNetworkConnection *kernelNetworkOpen(int, networkAddress *,
	networkFilter *);
int kernelNetworkAlive(kernelNetworkConnection *);
int kernelNetworkClose(kernelNetworkConnection *);
int kernelNetworkCloseAll(int);
int kernelNetworkCount(kernelNetworkConnection *);
int kernelNetworkRead(kernelNetworkConnection *, unsigned char *, unsigned);
int kernelNetworkWrite(kernelNetworkConnection *, unsigned char *, unsigned);
int kernelNetworkPing(kernelNetworkConnection *, int, unsigned char *,
	unsigned);
int kernelNetworkGetHostName(char *, int);
int kernelNetworkSetHostName(const char *, int);
int kernelNetworkGetDomainName(char *, int);
int kernelNetworkSetDomainName(const char *, int);
void kernelNetworkIpDebug(unsigned char *);

#define _KERNELNETWORK_H
#endif

