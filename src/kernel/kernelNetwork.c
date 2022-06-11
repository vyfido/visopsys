//
//  Visopsys
//  Copyright (C) 1998-2018 J. Andrew McLaughlin
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
//  kernelNetwork.c
//

#include "kernelNetwork.h"
#include "kernelDebug.h"
#include "kernelError.h"
#include "kernelLinkedList.h"
#include "kernelLog.h"
#include "kernelMalloc.h"
#include "kernelMultitasker.h"
#include "kernelNetworkArp.h"
#include "kernelNetworkDevice.h"
#include "kernelNetworkDhcp.h"
#include "kernelNetworkEthernet.h"
#include "kernelNetworkIcmp.h"
#include "kernelNetworkIp4.h"
#include "kernelNetworkLoopDriver.h"
#include "kernelNetworkStream.h"
#include "kernelNetworkUdp.h"
#include "kernelRtc.h"
#include "kernelVariableList.h"
#include <stdlib.h>
#include <string.h>
#include <sys/kernconf.h>

static char *hostName = NULL;
static char *domainName = NULL;
static kernelNetworkDevice *adapters[NETWORK_MAX_ADAPTERS];
static int numAdapters = 0;
static int netThreadPid = 0;
static int networkStop = 0;
static int initialized = 0;
static int enabled = 0;

extern variableList *kernelVariables;


static kernelNetworkDevice *getDevice(networkAddress *dest)
{
	// Given a destination address, determine the best/appropriate network
	// adapter to use for the connection.

	kernelNetworkDevice *adapter = NULL;
	int localNetwork = 0;
	int count;

	if (!numAdapters)
		return (adapter = NULL);

	// I expect that this will need to become more sophisticated over time.

	// If there's only one adapter, pick it
	if (numAdapters == 1)
		return (adapter = adapters[0]);

	// First default: the first adapter
	adapter = adapters[0];

	// Second default: an adapter that's running and not loopback
	for (count = 0; count < numAdapters; count ++)
	{
		if ((adapters[count]->device.flags & NETWORK_ADAPTERFLAG_RUNNING) &&
			(adapters[count]->device.linkProtocol !=
				NETWORK_LINKPROTOCOL_LOOP))
		{
			adapter = adapters[count];
			break;
		}
	}

	// If there's no destination address, that'll have to do
	if (!dest)
		return (adapter);

	// Look for an adapter that's running, and on the same network as the
	// destination address (including loopback this time)
	for (count = 0; count < numAdapters; count ++)
	{
		if ((adapters[count]->device.flags & NETWORK_ADAPTERFLAG_RUNNING) &&
			!networkAddressEmpty(&adapters[count]->device.netMask,
				sizeof(networkAddress)) &&
			networksEqualIp4(dest, &adapters[count]->device.netMask,
				&adapters[count]->device.hostAddress))
		{
			localNetwork = 1;
			adapter = adapters[count];
			break;
		}
	}

	// If the destination was an address that's local to one of our adapters,
	// choose that one
	if (localNetwork)
		return (adapter);

	// It's not on a local network, so try to pick an adapter that's running,
	// not loopback, and has a gateway address set
	for (count = 0; count < numAdapters; count ++)
	{
		if ((adapters[count]->device.flags & NETWORK_ADAPTERFLAG_RUNNING) &&
			(adapters[count]->device.linkProtocol !=
				NETWORK_LINKPROTOCOL_LOOP) &&
			!networkAddressEmpty(&adapters[count]->device.gatewayAddress,
				sizeof(networkAddress)))
		{
			adapter = adapters[count];
			break;
		}
	}

	return (adapter);
}


static kernelNetworkConnection *findMatchFilter(
	volatile kernelLinkedList *list, kernelLinkedListItem **iter,
	kernelNetworkPacket *packet)
{
	// Given a starting connection, loop through them until we find one whose
	// filter matches the supplied packet.

	kernelNetworkConnection *connection = NULL;
	networkIcmpHeader *icmpHeader = NULL;

	while (1)
	{
		if (!(*iter))
		{
			connection = kernelLinkedListIterStart((kernelLinkedList *) list,
				iter);
		}
		else
		{
			connection = kernelLinkedListIterNext((kernelLinkedList *) list,
				iter);
		}

		if (!connection)
			break;

		if (!networkAddressesEqual(&connection->address, &packet->srcAddress,
			sizeof(networkAddress)))
		{
			continue;
		}

		if ((connection->filter.flags & NETWORK_FILTERFLAG_LINKPROTOCOL) &&
			(packet->linkProtocol != connection->filter.linkProtocol))
		{
			continue;
		}

		if ((connection->filter.flags & NETWORK_FILTERFLAG_NETPROTOCOL) &&
			(packet->netProtocol != connection->filter.netProtocol))
		{
			continue;
		}

		if ((connection->filter.flags & NETWORK_FILTERFLAG_TRANSPROTOCOL) &&
			(packet->transProtocol != connection->filter.transProtocol))
		{
			continue;
		}

		if (connection->filter.flags & NETWORK_FILTERFLAG_SUBPROTOCOL)
		{
			switch (connection->filter.transProtocol)
			{
				case NETWORK_TRANSPROTOCOL_ICMP:
				{
					icmpHeader = (networkIcmpHeader *)(packet->memory +
						packet->transHeaderOffset);

					if (icmpHeader->type != connection->filter.subProtocol)
						continue;
				}
			}
		}

		if ((connection->filter.flags & NETWORK_FILTERFLAG_LOCALPORT) &&
			(packet->destPort != connection->filter.localPort))
		{
			continue;
		}

		if ((connection->filter.flags & NETWORK_FILTERFLAG_REMOTEPORT) &&
			(packet->srcPort != connection->filter.remotePort))
		{
			continue;
		}

		// The packet matches the filter.
		return (connection);
	}

	// If we fall through, we found none.
	return (connection = NULL);
}


static void networkThread(void)
{
	// This is the thread that processes our raw kernel packet input and
	// output streams

	int status = 0;
	kernelNetworkDevice *adapter = NULL;
	kernelNetworkPacket *packet = NULL;
	kernelNetworkConnection *connection = NULL;
	kernelLinkedListItem *iter = NULL;
	int count;

	while (!networkStop)
	{
		// Loop for each adapter
		for (count = 0; count < numAdapters; count ++)
		{
			adapter = adapters[count];

			if (!(adapter->device.flags & NETWORK_ADAPTERFLAG_RUNNING))
				continue;

			// If the adapter is dynamically configured, and there are fewer
			// than 60 seconds remaining on the lease, try to renew it
			if ((adapter->device.flags & NETWORK_ADAPTERFLAG_AUTOCONF) &&
				((int) kernelRtcUptimeSeconds() >=
					(adapter->dhcpConfig.leaseExpiry - 60)))
			{
				status = kernelNetworkDhcpConfigure(adapter, hostName,
					domainName, NETWORK_DHCP_DEFAULT_TIMEOUT);
				if (status < 0)
				{
					kernelError(kernel_error, "Attempt to renew DHCP "
						"configuration of network adapter %s failed",
						adapter->device.name);
					// Turn it off, sorry.
					adapter->device.flags &= ~NETWORK_ADAPTERFLAG_RUNNING;
					continue;
				}

				kernelLog("Renewed DHCP configuration for network adapter %s",
					adapter->device.name);
			}

			// Process received packets

			while (adapter->inputStream.count)
			{
				status = kernelNetworkPacketStreamRead(&adapter->inputStream,
					&packet);
				if (status < 0)
					// Try the next adapter
					break;

				kernelDebug(debug_net, "NET thread read a packet");

				// Parse the raw data to set up the packet data structure
				status = kernelNetworkSetupReceivedPacket(packet);
				if (status < 0)
				{
					// Discard this packet and try the next one
					kernelNetworkPacketRelease(packet);
					continue;
				}

				kernelDebug(debug_net, "NET thread accepted packet");
				kernelDebugHex(packet->memory, packet->length);

				// If this is an ARP packet
				if (packet->netProtocol == NETWORK_NETPROTOCOL_ARP)
					kernelNetworkArpProcessPacket(adapter, packet);

				// If this is an ICMP message
				if (packet->transProtocol == NETWORK_TRANSPROTOCOL_ICMP)
					kernelNetworkIcmpProcessPacket(adapter, packet);

				// Check whether there are connections with matching filters,
				// that might be eligible to receive this data

				iter = NULL;
				connection = findMatchFilter(&adapter->connections, &iter,
					packet);

				if (!connection)
				{
					// Nothing matched.  Discard the packet.  In theory, if
					// this is a TCP packet, we're supposed to send a reset.
					//  Rather, we choose silence.
					kernelNetworkPacketRelease(packet);
					continue;
				}

				// Loop through the applicable connections

				while (connection)
				{
					kernelDebug(debug_net, "NET thread found a suitable "
						"connection");

					// Deliver the data to the connection
					kernelNetworkDeliverData(connection, packet);

					// Continue for other connections that match
					connection = findMatchFilter(&adapter->connections,
						&iter, packet);
				}

				kernelNetworkPacketRelease(packet);
			}

			// Process the adapter's output packet stream.

			while (adapter->outputStream.count)
			{
				status = kernelNetworkPacketStreamRead(&adapter->outputStream,
					&packet);
				if (status < 0)
					// Try the next adapter
					break;

				// Send it.

				kernelDebug(debug_net, "NET thread send queued packet");

				status = kernelNetworkDeviceSend((char *)
					adapter->device.name, packet);

				kernelNetworkPacketRelease(packet);

				if (status < 0)
					break;
			}

			// Do any additional time-based processing
		}

		// Finished for this time slice
		kernelMultitaskerYield();
	}

	// Finished
	kernelMultitaskerTerminate(0);
}


static void checkSpawnNetworkThread(void)
{
	// Check the status of the network thread, and spawn a new one if it is
	// not running

	processState tmpState;

	if (!enabled)
		return;

	if (!netThreadPid ||
		(kernelMultitaskerGetProcessState(netThreadPid, &tmpState) < 0))
	{
		netThreadPid = kernelMultitaskerSpawnKernelThread(networkThread,
			"network thread", 0, NULL);
	}
}


static int connectionExists(kernelNetworkConnection *connection)
{
	// Returns 1 if the connection exists

	kernelNetworkDevice *adapter = NULL;
	kernelNetworkConnection *tmpConnection = NULL;
	kernelLinkedListItem *iter = NULL;
	int count;

	for (count = 0; count < numAdapters; count ++)
	{
		adapter = adapters[count];

		tmpConnection = kernelLinkedListIterStart((kernelLinkedList *)
			&adapter->connections, &iter);

		while (tmpConnection)
		{
			if (tmpConnection == connection)
				return (1);

			tmpConnection = kernelLinkedListIterNext((kernelLinkedList *)
				&adapter->connections, &iter);
		}
	}

	// Not found
	return (0);
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
//  Below here, the functions are exported for internal use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////

int kernelNetworkRegister(kernelNetworkDevice *adapter)
{
	// Called by the kernelNetworkDevice code to register a network adapter
	// for configuration (such as DHCP) and use.  That stuff is done later by
	// the kernelNetworkEnable() function.

	int status = 0;

	// Check params
	if (!adapter)
	{
		kernelError(kernel_error, "NULL parameter");
		return (status = ERR_NULLPARAMETER);
	}

	adapters[numAdapters++] = adapter;
	return (status = 0);
}


int kernelNetworkInitialize(void)
{
	// Initialize global networking stuff

	int status = 0;
	kernelNetworkDevice *adapter = NULL;
	int adapterCount, count;

	if (initialized)
		// Nothing to do.  No error.
		return (status = 0);

	hostName = kernelMalloc(NETWORK_MAX_HOSTNAMELENGTH);
	if (!hostName)
		return (status = ERR_MEMORY);

	// By default
	strcpy(hostName, "visopsys");

	if (kernelVariables)
	{
		// Check for a user-specified host name.
		if (kernelVariableListGet(kernelVariables, KERNELVAR_NET_HOSTNAME))
		{
			strncpy(hostName, kernelVariableListGet(kernelVariables,
				KERNELVAR_NET_HOSTNAME), NETWORK_MAX_HOSTNAMELENGTH);
		}
	}

	kernelDebug(debug_net, "NET hostName=%s", hostName);

	domainName = kernelMalloc(NETWORK_MAX_DOMAINNAMELENGTH);
	if (!domainName)
		return (status = ERR_MEMORY);

	// By default, empty
	domainName[0] = '\0';

	if (kernelVariables)
	{
		// Check for a user-specified domain name.
		if (kernelVariableListGet(kernelVariables, KERNELVAR_NET_DOMAINNAME))
		{
			strncpy(domainName, kernelVariableListGet(kernelVariables,
				KERNELVAR_NET_DOMAINNAME), NETWORK_MAX_DOMAINNAMELENGTH);
		}
	}

	kernelDebug(debug_net, "NET domainName=%s", domainName);

	// Attempt to create a loopback virtual adapter
	kernelNetworkLoopDeviceRegister();

	// Initialize all the network adapters
	for (adapterCount = 0; adapterCount < numAdapters; adapterCount ++)
	{
		adapter = adapters[adapterCount];

		kernelDebug(debug_net, "NET initialize adapter %s",
			adapter->device.name);

		// Initialize the adapter's network packet input and output streams

		status = kernelNetworkPacketStreamNew(&adapter->inputStream);
		if (status < 0)
			continue;

		status = kernelNetworkPacketStreamNew(&adapter->outputStream);
		if (status < 0)
			continue;

		// Get a 'pool' of packet memory for use by interrupt handlers, since
		// they can't allocate memory themselves.

		adapter->packetPool.freePackets = NETWORK_PACKETS_PER_STREAM;
		adapter->packetPool.data =
			kernelMalloc(adapter->packetPool.freePackets *
				sizeof(kernelNetworkPacket));
		if (!adapter->packetPool.data)
			continue;

		for (count = 0; count < adapter->packetPool.freePackets; count ++)
		{
			adapter->packetPool.packet[count] = (adapter->packetPool.data +
				(count * sizeof(kernelNetworkPacket)));
		}

		adapter->device.flags |= NETWORK_ADAPTERFLAG_INITIALIZED;
	}

	initialized = 1;

	kernelLog("Networking initialized.  Host name is \"%s\".", hostName);
	return (status = 0);
}


kernelNetworkConnection *kernelNetworkConnectionOpen(
	kernelNetworkDevice *adapter, int mode, networkAddress *address,
	networkFilter *filter, int inputStream)
{
	// This function opens up a connection.  A connection is necessary for
	// nearly any kind of network communication.  Thus, there will eventually
	// be plenty of checking here before we allow the connection.

	kernelNetworkConnection *connection = NULL;

	if (!adapter)
	{
		// Find the network adapter that's suitable for this destination
		// address
		adapter = getDevice(address);
		if (!adapter)
		{
			kernelError(kernel_error, "No appropriate adapter for "
				"destination address");
			return (connection = NULL);
		}
	}

	if ((filter->flags & NETWORK_FILTERFLAG_TRANSPROTOCOL) &&
		(filter->transProtocol == NETWORK_TRANSPROTOCOL_TCP))
	{
		// Not currently supported
		kernelError(kernel_error, "TCP connections are currently "
			"unsupported");
		return (connection = NULL);
	}

	// Allocate memory for the connection structure
	connection = kernelMalloc(sizeof(kernelNetworkConnection));
	if (!connection)
		return (connection);

	// Set the process ID and mode
	connection->processId = kernelMultitaskerGetCurrentProcessId();
	connection->mode = mode;

	// If the network address was specified, copy it.
	if (address)
	{
		networkAddressCopy(&connection->address, address,
			sizeof(networkAddress));
	}

	// If it's a read connection and an input stream was requested, get an
	// input stream
	if (inputStream && (mode & NETWORK_MODE_READ))
	{
		if (kernelStreamNew(&connection->inputStream,
			NETWORK_DATASTREAM_LENGTH, itemsize_byte) < 0)
		{
			kernelFree((void *) connection);
			return (connection = NULL);
		}
	}

	// If this is an IP connection, check/find the local port number
	if ((filter->flags & NETWORK_FILTERFLAG_NETPROTOCOL) &&
		(filter->netProtocol == NETWORK_NETPROTOCOL_IP4))
	{
		filter->flags |= NETWORK_FILTERFLAG_LOCALPORT;
		filter->localPort = kernelNetworkIp4GetLocalPort(adapter,
			filter->localPort);
		if (filter->localPort <= 0)
		{
			kernelFree((void *) connection);
			return (connection = NULL);
		}
	}

	// Copy the network filter
	memcpy((void *) &connection->filter, filter, sizeof(networkFilter));

	if ((connection->filter.flags & NETWORK_FILTERFLAG_NETPROTOCOL) &&
		(connection->filter.netProtocol == NETWORK_NETPROTOCOL_IP4))
	{
		// The ID of the IP packets is the lower 16 bits of the connection
		// pointer
		connection->ip.identification = (unsigned short)
			((unsigned long) connection & 0xFFFF);
	}

	// Add the connection to the adapter's list
	connection->adapter = adapter;
	kernelLinkedListAdd((kernelLinkedList *) &adapter->connections,
		(void *) connection);

	return (connection);
}


int kernelNetworkConnectionClose(kernelNetworkConnection *connection,
	int polite)
{
	// Closes and deallocates the specified network connection.  If a 'polite'
	// closure was requested, it will attept to shut down transport-level
	// (i.e. TCP) connections first.

	int status = 0;
	kernelNetworkDevice *adapter = connection->adapter;

	if (polite)
	{
	}

	// If there's an input stream, deallocate it
	if (connection->inputStream.buffer)
		kernelStreamDestroy(&connection->inputStream);

	// Remove the connection from the adapter's list.
	kernelLinkedListRemove((kernelLinkedList *) &adapter->connections,
		(void *) connection);

	// Deallocate it
	memset((void *) connection, 0, sizeof(kernelNetworkConnection));
	kernelFree((void *) connection);

	return (status = 0);
}


kernelNetworkPacket *kernelNetworkPacketGet(void)
{
	// Allocates a packet, and adds an initial reference count.

	kernelNetworkPacket *packet = NULL;

	packet = kernelMalloc(sizeof(kernelNetworkPacket));
	if (!packet)
		return (packet);

	// Can't set packet->release to kernelFree(), because that's a macro

	packet->refCount = 1;

	return (packet);
}


void kernelNetworkPacketHold(kernelNetworkPacket *packet)
{
	// Just adds a reference count to a packet

	// Check params
	if (!packet)
		return;

	packet->refCount += 1;
}


void kernelNetworkPacketRelease(kernelNetworkPacket *packet)
{
	// Removes a reference count from a packet, and if there are no more
	// references, frees the packet.

	// Check params
	if (!packet)
		return;

	packet->refCount -= 1;

	if (packet->refCount <= 0)
	{
		if (packet->release)
			packet->release(packet);
		else
			kernelFree(packet);
	}
}


int kernelNetworkSetupReceivedPacket(kernelNetworkPacket *packet)
{
	// This takes a semi-raw 'received' packet, as from the network adapter's
	// packet input stream, which must be already recognised/configured
	// appropriately by the link layer (currently, as an IP packet).  Tries
	// to interpret the rest and set up the remainder of the packet's fields.

	int status = 0;

	// The first bit depends on the network protocol
	switch (packet->netProtocol)
	{
		case NETWORK_NETPROTOCOL_ARP:
			kernelDebug(debug_net, "NET setup received ARP packet");
			status = kernelNetworkArpSetupReceivedPacket(packet);
			// Nothing more to do here
			return (status);

		case NETWORK_NETPROTOCOL_IP4:
			kernelDebug(debug_net, "NET setup received IP4 packet");
			status = kernelNetworkIp4SetupReceivedPacket(packet);
			break;

		default:
			kernelDebug(debug_net, "NET unsupported network protocol %d",
				packet->netProtocol);
			status = ERR_NOTIMPLEMENTED;
			break;
	}

	if (status < 0)
		return (status);

	// The rest depends upon the transport protocol
	switch (packet->transProtocol)
	{
		case NETWORK_TRANSPROTOCOL_ICMP:
			kernelDebug(debug_net, "NET setup received ICMP packet");
			status = kernelNetworkIcmpSetupReceivedPacket(packet);
			break;

		case NETWORK_TRANSPROTOCOL_UDP:
			kernelDebug(debug_net, "NET setup received UDP packet");
			status = kernelNetworkUdpSetupReceivedPacket(packet);
			break;

		default:
			kernelDebug(debug_net, "NET unsupported transport protocol %d",
				packet->transProtocol);
			status = ERR_NOTIMPLEMENTED;
			break;
	}

	return (status);
}


void kernelNetworkDeliverData(kernelNetworkConnection *connection,
	kernelNetworkPacket *packet)
{
	void *copyPtr = NULL;
	unsigned length = 0;

	// If it's not a 'read' connection with an input stream, skip the rest.
	if (!(connection->mode & NETWORK_MODE_READ) ||
		!connection->inputStream.buffer)
	{
		kernelError(kernel_error, "Connection can't receive data");
		return;
	}

	// Copy the packet into the input stream.

	// By default, just the data
	copyPtr = (packet->memory + packet->dataOffset);
	length = packet->dataLength;

	// (unless headers were requested)
	if (connection->filter.flags & NETWORK_FILTERFLAG_HEADERS)
	{
		if (connection->filter.headers == NETWORK_HEADERS_RAW)
		{
			copyPtr = packet->memory;
			length += packet->dataOffset;
		}
		else if (connection->filter.headers == NETWORK_HEADERS_LINK)
		{
			copyPtr = (packet->memory + packet->linkHeaderOffset);
			length += (packet->dataOffset - packet->linkHeaderOffset);
		}
		else if (connection->filter.headers == NETWORK_HEADERS_NET)
		{
			copyPtr = (packet->memory + packet->netHeaderOffset);
			length += (packet->dataOffset - packet->netHeaderOffset);
		}
		else if (connection->filter.headers == NETWORK_HEADERS_TRANSPORT)
		{
			copyPtr = (packet->memory + packet->transHeaderOffset);
			length += (packet->dataOffset - packet->transHeaderOffset);
		}
	}

	length = min(length, (NETWORK_DATASTREAM_LENGTH -
		connection->inputStream.count));

	kernelDebug(debug_net, "NET deliver %u bytes to connection", length);

	connection->inputStream.appendN(&connection->inputStream, length,
		copyPtr);
}


int kernelNetworkSetupSendPacket(kernelNetworkConnection *connection,
	kernelNetworkPacket *packet)
{
	// This takes an empty packet structure and does initial setup, filling in
	// addresses, allocating memory, reserving space for headers, and setting
	// up pointers to everthing depending on the connection protocols.

	int status = 0;

	// Set up the packet info

	// Adresses and ports
	networkAddressCopy(&packet->srcAddress,
		&connection->adapter->device.hostAddress, sizeof(networkAddress));
	packet->srcPort = connection->filter.localPort;
	networkAddressCopy(&packet->destAddress, &connection->address,
		sizeof(networkAddress));
	packet->destPort = connection->filter.remotePort;

	// Initially we have the whole packet memory available
	packet->dataLength = NETWORK_PACKET_MAX_LENGTH;

	// The packet's link protocol header is the protocol of the network
	// adapter
	packet->linkProtocol = connection->adapter->device.linkProtocol;

	// Prepend the link protocol header
	switch (packet->linkProtocol)
	{
		case NETWORK_LINKPROTOCOL_LOOP:
			status = 0;
			break;

		case NETWORK_LINKPROTOCOL_ETHERNET:
			status = kernelNetworkEthernetPrependHeader(connection->adapter,
				packet);
			break;

		default:
			kernelError(kernel_error, "Device %s has an unknown link "
				"protocol %d", connection->adapter->device.name,
				packet->linkProtocol);
			status = ERR_INVALID;
			break;
	}

	if (status < 0)
		return (status);

	if (connection->filter.flags & NETWORK_FILTERFLAG_TRANSPROTOCOL)
	{
		// Set the packet's transport protocol in case the network protocol
		// needs it to construct its header (a la the protocol field in an IP
		// header)
		packet->transProtocol = connection->filter.transProtocol;
	}

	// Prepend the network protocol header based on the requested transport
	// protocol
	switch (packet->transProtocol)
	{
		case NETWORK_TRANSPROTOCOL_ICMP:
		case NETWORK_TRANSPROTOCOL_UDP:
			packet->netProtocol = NETWORK_NETPROTOCOL_IP4;
			kernelNetworkIp4PrependHeader(packet);
			break;

		default:
			kernelError(kernel_error, "Unknown transport protocol %d",
				packet->transProtocol);
			status = ERR_INVALID;
			break;
	}

	if (status < 0)
		return (status);

	// Prepend the transport protocol header, if applicable
	switch (packet->transProtocol)
	{
		case NETWORK_TRANSPROTOCOL_UDP:
			kernelNetworkUdpPrependHeader(packet);
			break;
	}

	return (status);
}


void kernelNetworkFinalizeSendPacket(kernelNetworkConnection *connection,
	kernelNetworkPacket *packet)
{
	// This does any required finalizing and checksumming of a packet before
	// it is to be sent.

	// If the network protocol needs to post-process the packet, do that
	// now

	if (packet->netProtocol == NETWORK_NETPROTOCOL_IP4)
		kernelNetworkIp4FinalizeSendPacket(&connection->ip, packet);

	// If the transport protocol needs to post-process the packet, do that
	// now

	switch (packet->transProtocol)
	{
		case NETWORK_TRANSPROTOCOL_UDP:
			kernelNetworkUdpFinalizeSendPacket(packet);
			break;
	}
}


int kernelNetworkSendPacket(kernelNetworkDevice *adapter,
	kernelNetworkPacket *packet, int immediate)
{
	// Do the final step of sending a packet, either by sending it directly
	// to the adapter, or queueing it up in the adapter's output stream

	int status = 0;

	// We either send it or queue it.
	if (immediate)
	{
		kernelDebug(debug_net, "NET send packet immediate");

		status = kernelNetworkDeviceSend((char *) adapter->device.name,
			packet);

		if (status < 0)
			kernelError(kernel_error, "Error sending packet");
	}
	else
	{
		kernelDebug(debug_net, "NET queue packet");

		status = kernelNetworkPacketStreamWrite(&adapter->outputStream,
			packet);

		if (status < 0)
			kernelError(kernel_error, "Error queueing packet");
	}

	return (status);
}


int kernelNetworkSendData(kernelNetworkConnection *connection,
	unsigned char *buffer, unsigned bufferSize, int immediate)
{
	// This is the "guts" function for sending network data.  The caller
	// provides the active connection, the raw data, and whether or not the
	// transmission should be immediate or queued.

	int status = 0;
	kernelNetworkPacket *packet = NULL;
	unsigned sent = 0;
	int count;

	if (!bufferSize)
		// Nothing to do, we guess.  Should be an error, we suppose.
		return (status = ERR_NODATA);

	// Loop for each packet while there's still data in the buffer
	for (count = 0; bufferSize > 0; count ++)
	{
		packet = kernelNetworkPacketGet();
		if (!packet)
			return (status = ERR_MEMORY);

		// Set up the packet headers
		status = kernelNetworkSetupSendPacket(connection, packet);
		if (status < 0)
		{
			kernelNetworkPacketRelease(packet);
			return (status);
		}

		packet->dataLength = min(packet->dataLength, bufferSize);

		kernelDebug(debug_net, "NET packet data length %u",
			packet->dataLength);

		// Copy in the packet data
		memcpy((packet->memory + packet->dataOffset), (buffer + sent),
			packet->dataLength);

		packet->length = (packet->dataOffset + packet->dataLength);

		// Finalize checksums, etc.
		kernelNetworkFinalizeSendPacket(connection, packet);

		// Make the packet length even
		if (packet->length % 2)
			packet->length += 1;

		kernelDebug(debug_net, "NET packet total length %u", packet->length);

		status = kernelNetworkSendPacket(connection->adapter, packet,
			immediate);
		if (status < 0)
		{
			kernelNetworkPacketRelease(packet);
			break;
		}

		sent += packet->dataLength;
		bufferSize -= packet->dataLength;

		kernelNetworkPacketRelease(packet);
	}

	// Return the status from the last 'send' operation
	return (status);
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
//  Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////

int kernelNetworkEnabled(void)
{
	// Returns 1 if networking is currently enabled
	return (enabled);
}


int kernelNetworkEnable(void)
{
	// Enable networking

	int status = 0;
	const char *newHostName = NULL;
	const char *newDomainName = NULL;
	kernelNetworkDevice *adapter = NULL;
	int count;

	if (!initialized)
	{
		kernelError(kernel_error, "Networking is not initialized");
		return (status = ERR_NOTINITIALIZED);
	}

	if (enabled)
		// Nothing to do.  No error.
		return (status = 0);

	if (kernelVariables)
	{
		// Check for a user-specified host name.
		newHostName = kernelVariableListGet(kernelVariables,
			KERNELVAR_NET_HOSTNAME);

		if (newHostName && strncmp(hostName, newHostName,
			NETWORK_MAX_HOSTNAMELENGTH))
		{
			strncpy(hostName, newHostName, NETWORK_MAX_HOSTNAMELENGTH);
			kernelDebug(debug_net, "NET hostName=%s", hostName);
		}

		// Check for a user-specified domain name.
		newDomainName = kernelVariableListGet(kernelVariables,
			KERNELVAR_NET_DOMAINNAME);

		if (newDomainName && strncmp(domainName, newDomainName,
			NETWORK_MAX_DOMAINNAMELENGTH))
		{
			strncpy(domainName, newDomainName, NETWORK_MAX_DOMAINNAMELENGTH);
			kernelDebug(debug_net, "NET domainName=%s", domainName);
		}
	}

	// Configure all the network adapters
	for (count = 0; count < numAdapters; count ++)
	{
		adapter = adapters[count];

		kernelDebug(debug_net, "NET configure adapter %s",
			adapter->device.name);

		if (!(adapter->device.flags & NETWORK_ADAPTERFLAG_LINK))
			// No network link
			continue;

		if (networkAddressEmpty(&adapter->device.hostAddress,
			sizeof(networkAddress)))
		{
			kernelDebug(debug_net, "NET configure %s DHCP",
				adapter->device.name);

			status = kernelNetworkDhcpConfigure(adapter, hostName, domainName,
				NETWORK_DHCP_DEFAULT_TIMEOUT);
			if (status < 0)
			{
				kernelError(kernel_error, "DHCP configuration of network "
					"adapter %s failed.", adapter->device.name);
				continue;
			}
		}

		adapter->device.flags |= NETWORK_ADAPTERFLAG_RUNNING;

		kernelLog("Network adapter %s configured with IP=%d.%d.%d.%d "
			"netmask=%d.%d.%d.%d", adapter->device.name,
			adapter->device.hostAddress.byte[0],
			adapter->device.hostAddress.byte[1],
			adapter->device.hostAddress.byte[2],
			adapter->device.hostAddress.byte[3],
			adapter->device.netMask.byte[0],
			adapter->device.netMask.byte[1],
			adapter->device.netMask.byte[2],
			adapter->device.netMask.byte[3]);

		kernelDebug(debug_net, "NET adapter %s configured",
			adapter->device.name);
	}

	enabled = 1;

	// Start the regular processing of the packet input and output streams
	networkStop = 0;
	checkSpawnNetworkThread();

	kernelLog("Networking enabled.  Host name is \"%s\".", hostName);
	return (status = 0);
}


int kernelNetworkShutdown(void)
{
	// Perform a nice, orderly network shutdown.

	int status = 0;
	kernelNetworkDevice *adapter = NULL;
	kernelNetworkConnection *connection = NULL;
	kernelLinkedListItem *iter = NULL;
	int count;

	if (!enabled)
		// Nothing to do.  No error.
		return (status = 0);

	// Close all connections
	for (count = 0; count < numAdapters; count ++)
	{
		adapter = adapters[count];

		connection = kernelLinkedListIterStart((kernelLinkedList *)
			&adapter->connections, &iter);

		while (connection)
		{
			kernelNetworkClose(connection);

			connection = kernelLinkedListIterNext((kernelLinkedList *)
				&adapter->connections, &iter);
		}
	}

	// Set the 'network stop' flag and yield to let the network thread finish
	// whatever it might have been doing
	networkStop = 1;
	kernelMultitaskerYield();

	// Stop each network adapter
	for (count = 0; count < numAdapters; count ++)
	{
		adapter = adapters[count];

		if (!(adapter->device.flags & NETWORK_ADAPTERFLAG_LINK) ||
			!(adapter->device.flags & NETWORK_ADAPTERFLAG_RUNNING))
		{
			continue;
		}

		// If the device was configured with DHCP, tell the server we're
		// relinquishing the address.
		if (adapter->device.flags & NETWORK_ADAPTERFLAG_AUTOCONF)
			kernelNetworkDhcpRelease(adapter);

		// The device is still initialized, but no longer running.
		adapter->device.flags &= ~NETWORK_ADAPTERFLAG_RUNNING;
	}

	enabled = 0;

	return (status = 0);
}


kernelNetworkConnection *kernelNetworkOpen(int mode, networkAddress *address,
	networkFilter *filter)
{
	// This function is a wrapper for the kernelNetworkConnectionOpen()
	// function, above, but also finds the best network adapter to use.

	kernelNetworkDevice *adapter = NULL;
	kernelNetworkConnection *connection = NULL;

	if (!enabled)
	{
		kernelError(kernel_error, "Networking is not enabled");
		return (connection = NULL);
	}

	// Make sure the network thread is running
	checkSpawnNetworkThread();

	// Check params.

	if (!mode)
	{
		kernelError(kernel_error, "A connection mode must be specified");
		return (connection = NULL);
	}

	if (!filter)
	{
		kernelError(kernel_error, "NULL parameter");
		return (connection = NULL);
	}

	adapter = getDevice(address);
	if (!adapter)
	{
		kernelError(kernel_error, "No appropriate adapter for destination "
			"address");
		return (connection = NULL);
	}

	connection = kernelNetworkConnectionOpen(adapter, mode, address, filter,
		1 /* input stream, if read mode */);

	return (connection);
}


int kernelNetworkAlive(kernelNetworkConnection *connection)
{
	// Returns 1 if the connection exists and is alive

	if (!enabled)
	{
		kernelError(kernel_error, "Networking is not enabled");
		return (0);
	}

	if (!connectionExists(connection))
		// No longer exists
		return (0);

	return (1);
}


int kernelNetworkClose(kernelNetworkConnection *connection)
{
	// This is just a wrapper for the kernelNetworkConnectionClose() function

	int status = 0;

	if (!enabled)
	{
		kernelError(kernel_error, "Networking is not enabled");
		return (status = ERR_NOTINITIALIZED);
	}

	// Make sure the network thread is running
	checkSpawnNetworkThread();

	// Check params
	if (!connection)
	{
		kernelError(kernel_error, "NULL parameter");
		return (status = ERR_NULLPARAMETER);
	}

	// Make sure the connection exists.  If not, no worries, just return.
	if (!connectionExists(connection))
		return (status = 0);

	// Close the connection
	status = kernelNetworkConnectionClose(connection, 1 /* polite */);

	return (status);
}


int kernelNetworkCloseAll(int processId)
{
	// Search for and close all network connections owned by the process ID.
	// This assumes that the process is being terminated (for example by the
	// multitasker) and so it requests an 'impolite' closure.

	int status = 0;
	kernelNetworkConnection *connection = NULL;
	kernelLinkedListItem *iter = NULL;
	int count;

	if (!enabled)
	{
		kernelError(kernel_error, "Networking is not enabled");
		return (status = ERR_NOTINITIALIZED);
	}

	// Don't need to make sure the network thread is running; the calls to
	// kernelNetworkClose() will do it.

	for (count = 0; count < numAdapters; count ++)
	{
		connection = kernelLinkedListIterStart((kernelLinkedList *)
			&adapters[count]->connections, &iter);

		while (connection)
		{
			if (connection->processId == processId)
				kernelNetworkConnectionClose(connection, 0 /* not polite */);

			connection = kernelLinkedListIterNext((kernelLinkedList *)
				&adapters[count]->connections, &iter);
		}
	}

	return (status = 0);
}


int kernelNetworkCount(kernelNetworkConnection *connection)
{
	// Returns the number of bytes currently in the connection's input stream

	if (!enabled)
	{
		kernelError(kernel_error, "Networking is not enabled");
		return (ERR_NOTINITIALIZED);
	}

	// Make sure the network thread is running
	checkSpawnNetworkThread();

	// Check params
	if (!connection)
	{
		kernelError(kernel_error, "NULL parameter");
		return (ERR_NULLPARAMETER);
	}

	// Make sure the connection exists
	if (!kernelNetworkAlive(connection))
	{
		kernelError(kernel_error, "Connection is not alive");
		return (ERR_IO);
	}

	return (connection->inputStream.count);
}


int kernelNetworkRead(kernelNetworkConnection *connection,
	unsigned char *buffer, unsigned bufferSize)
{
	// Given a network connection, read up to 'bufferSize' bytes into the
	// buffer from the connection's input stream.

	int status = 0;

	if (!enabled)
	{
		kernelError(kernel_error, "Networking is not enabled");
		return (status = ERR_NOTINITIALIZED);
	}

	// Make sure the network thread is running
	checkSpawnNetworkThread();

	// Check params
	if (!connection || !buffer)
	{
		kernelError(kernel_error, "NULL parameter");
		return (status = ERR_NULLPARAMETER);
	}

	// Make sure the connection exists
	if (!kernelNetworkAlive(connection))
	{
		kernelError(kernel_error, "Connection is not alive");
		return (status = ERR_IO);
	}

	// Make sure we're reading
	if (!(connection->mode & NETWORK_MODE_READ))
	{
		kernelError(kernel_error, "Network connection is not open for "
			"reading");
		return (status = ERR_INVALID);
	}

	// Use the smaller of the buffer size, or the bytes available to read
	bufferSize = min(connection->inputStream.count, bufferSize);

	// Anything to do?
	if (!bufferSize)
		return (status = 0);

	// Read into the buffer
	status = connection->inputStream.popN(&connection->inputStream,
		bufferSize, buffer);

	return (status);
}


int kernelNetworkWrite(kernelNetworkConnection *connection,
	unsigned char *buffer, unsigned bufferSize)
{
	// Given a network connection, write up to 'bufferSize' bytes from the
	// buffer to the connection's output.

	int status = 0;

	if (!enabled)
	{
		kernelError(kernel_error, "Networking is not enabled");
		return (status = ERR_NOTINITIALIZED);
	}

	// Make sure the network thread is running
	checkSpawnNetworkThread();

	// Check params
	if (!connection || !buffer)
	{
		kernelError(kernel_error, "NULL parameter");
		return (status = ERR_NULLPARAMETER);
	}

	// Make sure the connection exists
	if (!kernelNetworkAlive(connection))
	{
		kernelError(kernel_error, "Connection is not alive");
		return (status = ERR_IO);
	}

	// Make sure we're writing
	if (!(connection->mode & NETWORK_MODE_WRITE))
	{
		kernelError(kernel_error, "Network connection is not open for "
			"writing");
		return (status = ERR_INVALID);
	}

	return (status = kernelNetworkSendData(connection, buffer, bufferSize,
		0 /* not immediate */));
}


int kernelNetworkPing(kernelNetworkConnection *connection, int sequenceNum,
	unsigned char *buffer, unsigned bufferSize)
{
	// Send a ping.

	int status = 0;

	if (!enabled)
	{
		kernelError(kernel_error, "Networking is not enabled");
		return (status = ERR_NOTINITIALIZED);
	}

	// Make sure the network thread is running
	checkSpawnNetworkThread();

	// Check params
	if (!connection)
	{
		kernelError(kernel_error, "NULL parameter");
		return (status = ERR_NULLPARAMETER);
	}

	// Make sure the connection exists
	if (!kernelNetworkAlive(connection))
	{
		kernelError(kernel_error, "Connection is not alive");
		return (status = ERR_IO);
	}

	status = kernelNetworkIcmpPing(connection, sequenceNum, buffer,
		bufferSize);

	return (status);
}


int kernelNetworkGetHostName(char *buffer, int bufferSize)
{
	// Get the system's network hostname

	int status = 0;

	if (!initialized)
	{
		kernelError(kernel_error, "Networking is not initialized");
		return (status = ERR_NOTINITIALIZED);
	}

	// Check params
	if (!buffer)
		return (status = ERR_NULLPARAMETER);

	strncpy(buffer, hostName, min(bufferSize, NETWORK_MAX_HOSTNAMELENGTH));
	return (status = 0);
}


int kernelNetworkSetHostName(const char *buffer, int bufferSize)
{
	// Set the system's network hostname

	int status = 0;

	if (!initialized)
	{
		kernelError(kernel_error, "Networking is not initialized");
		return (status = ERR_NOTINITIALIZED);
	}

	// Check params
	if (!buffer)
		return (status = ERR_NULLPARAMETER);

	strncpy(hostName, buffer, min(bufferSize, NETWORK_MAX_HOSTNAMELENGTH));
	return (status = 0);
}


int kernelNetworkGetDomainName(char *buffer, int bufferSize)
{
	// Get the system's network domain name

	int status = 0;

	if (!initialized)
	{
		kernelError(kernel_error, "Networking is not initialized");
		return (status = ERR_NOTINITIALIZED);
	}

	// Check params
	if (!buffer)
		return (status = ERR_NULLPARAMETER);

	strncpy(buffer, domainName, min(bufferSize, NETWORK_MAX_DOMAINNAMELENGTH));
	return (status = 0);
}


int kernelNetworkSetDomainName(const char *buffer, int bufferSize)
{
	// Set the system's network domain name

	int status = 0;

	if (!initialized)
	{
		kernelError(kernel_error, "Networking is not initialized");
		return (status = ERR_NOTINITIALIZED);
	}

	// Check params
	if (!buffer)
		return (status = ERR_NULLPARAMETER);

	strncpy(domainName, buffer, min(bufferSize,
		NETWORK_MAX_DOMAINNAMELENGTH));

	return (status = 0);
}

