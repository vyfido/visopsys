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
//  kernelNetworkDevice.c
//

// This file contains functions for abstracting and managing network adapter
// devices.  This is the portion of the link layer that is not a hardware
// driver, but which does all the interfacing with the hardware drivers.

#include "kernelNetworkDevice.h"
#include "kernelDebug.h"
#include "kernelError.h"
#include "kernelInterrupt.h"
#include "kernelLog.h"
#include "kernelMalloc.h"
#include "kernelMultitasker.h"
#include "kernelNetwork.h"
#include "kernelNetworkArp.h"
#include "kernelNetworkStream.h"
#include "kernelPic.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/processor.h>

// An array of pointers to all network devices.
static kernelDevice *devices[NETWORK_MAX_ADAPTERS];
static int numDevices = 0;

// Saved old interrupt handlers
static void **oldIntHandlers = NULL;
static int numOldHandlers = 0;


static void poolPacketRelease(kernelNetworkPacket *packet)
{
	// This is called by kernelNetworkPacketRelease to release packets
	// allocated from the device's packet pool.

	kernelNetworkDevice *adapter = NULL;

	// Check params
	if (!packet)
	{
		kernelError(kernel_error, "NULL parameter");
		return;
	}

	adapter = packet->context;
	if (!adapter)
	{
		kernelError(kernel_error, "No packet adapter context");
		return;
	}

	if (adapter->packetPool.freePackets >= NETWORK_PACKETS_PER_STREAM)
		return;

	adapter->packetPool.packet[adapter->packetPool.freePackets] = packet;
	adapter->packetPool.freePackets += 1;
}


static kernelNetworkPacket *poolPacketGet(kernelNetworkDevice *adapter)
{
	// Get a packet from the adapter's packet pool

	kernelNetworkPacket *packet = NULL;

	if (adapter->packetPool.freePackets <= 0)
		return (packet = NULL);

	adapter->packetPool.freePackets -= 1;
	packet = adapter->packetPool.packet[adapter->packetPool.freePackets];

	if (!packet)
		return (packet);

	memset(packet, 0, sizeof(kernelNetworkPacket));
	packet->release = &poolPacketRelease;
	packet->context = (void *) adapter;

	return (packet);
}


static void processHooks(kernelNetworkDevice *adapter,
	kernelNetworkPacket *packet, int input)
{
	kernelLinkedList *list = NULL;
	kernelNetworkPacketStream *theStream = NULL;
	kernelLinkedListItem *iter = NULL;

	// If there are hooks on this adapter, emit the raw packet data

	if (input)
		list = (kernelLinkedList *) &adapter->inputHooks;
	else
		list = (kernelLinkedList *) &adapter->outputHooks;

	theStream = kernelLinkedListIterStart(list, &iter);
	if (!theStream)
		return;

	while (theStream)
	{
		kernelNetworkPacketStreamWrite(theStream, packet);
		theStream = kernelLinkedListIterNext(list, &iter);
	}
}


static int processLoop(kernelNetworkDevice *adapter __attribute__((unused)),
	kernelNetworkPacket *packet)
{
	// Interpret the link protocol header for loopback (but the loopback
	// protocol has no link header)

	kernelDebug(debug_net, "NETDEV receive %d: loopback msgsz %u",
		adapter->device.recvPackets, packet->length);

	// Assume IP v4 for the time being
	packet->netProtocol = NETWORK_NETPROTOCOL_IP4;

	return (0);
}


static int processEthernet(kernelNetworkDevice *adapter
	 __attribute__((unused)), kernelNetworkPacket *packet)
{
	// Interpret the link protocol header for ethernet

	networkEthernetHeader *header = NULL;
	unsigned short type = 0;

	header = (networkEthernetHeader *) packet->memory;
	type = ntohs(header->type);

	// If the packet is not ethernet IP v4 or ARP, we are finished
	if ((type != NETWORK_ETHERTYPE_IP4) && (type != NETWORK_ETHERTYPE_ARP))
		return (ERR_NOTIMPLEMENTED);

	kernelDebug(debug_net, "NETDEV receive %d: ethernet type=%x "
		"%02x:%02x:%02x:%02x:%02x:%02x -> %02x:%02x:%02x:%02x:%02x:%02x "
		"msgsz %u", adapter->device.recvPackets, ntohs(header->type),
		header->source[0], header->source[1], header->source[2],
		header->source[3], header->source[4], header->source[5],
		header->dest[0], header->dest[1], header->dest[2], header->dest[3],
		header->dest[4], header->dest[5], packet->length);

	if (type == NETWORK_ETHERTYPE_IP4)
		packet->netProtocol = NETWORK_NETPROTOCOL_IP4;
	else if (type == NETWORK_ETHERTYPE_ARP)
		packet->netProtocol = NETWORK_NETPROTOCOL_ARP;

	packet->netHeaderOffset = (packet->linkHeaderOffset +
		sizeof(networkEthernetHeader));

	return (0);
}


static int readData(kernelDevice *dev)
{
	int status = 0;
	kernelNetworkDevice *adapter = NULL;
	kernelNetworkDeviceOps *ops = dev->driver->ops;
	unsigned char buffer[NETWORK_PACKET_MAX_LENGTH];
	kernelNetworkPacket *packet = NULL;

	adapter = dev->data;

	kernelDebug(debug_net, "NETDEV read data from %s", adapter->device.name);

	if (!(adapter->device.flags & NETWORK_ADAPTERFLAG_INITIALIZED))
	{
		// We can't process this data, but we can service the adapter
		if (ops->driverReadData)
			ops->driverReadData(adapter, buffer);

		return (status = 0);
	}

	adapter->device.recvPackets += 1;

	packet = poolPacketGet(adapter);
	if (!packet)
		return (status = ERR_MEMORY);

	if (ops->driverReadData)
		packet->length = ops->driverReadData(adapter, packet->memory);

	// If there's no data, we are finished
	if (!packet->length)
	{
		poolPacketRelease(packet);
		return (status = 0);
	}

	// If there are input hooks on this adapter, emit the raw packet data
	processHooks(adapter, packet, 1 /* input */);

	// Set up the the packet structure's link and network protocol fields

	packet->linkProtocol = adapter->device.linkProtocol;
	packet->linkHeaderOffset = 0;

	switch (adapter->device.linkProtocol)
	{
		case NETWORK_LINKPROTOCOL_LOOP:
			status = processLoop(adapter, packet);
			break;

		case NETWORK_LINKPROTOCOL_ETHERNET:
			status = processEthernet(adapter, packet);
			break;

		default:
			status = ERR_NOTIMPLEMENTED;
			break;
	}

	if (status < 0)
	{
		kernelNetworkPacketRelease(packet);
		return (status);
	}

	// Set the data section to start at the network header
	packet->dataOffset = packet->netHeaderOffset;
	packet->dataLength = (packet->length - packet->dataOffset);

	// Insert it into the input packet stream
	status = kernelNetworkPacketStreamWrite(&adapter->inputStream, packet);

	kernelNetworkPacketRelease(packet);

	if (status < 0)
	{
		// It would be good if we had a collection of 'deferred packets' for
		// cases like this, so we can try to insert them next time, since by
		// doing this we actually drop the packet
		kernelError(kernel_error, "Couldn't write input stream; packet "
			"dropped");
		adapter->device.recvDropped += 1;
		return (status);
	}

	return (status = 0);
}


static void networkInterrupt(void)
{
	// This is the network interrupt handler.  It calls the network driver
	// for the device in order to actually service the interrupt

	void *address = NULL;
	int interruptNum = 0;
	kernelDevice *dev = NULL;
	kernelNetworkDevice *adapter = NULL;
	kernelNetworkDeviceOps *ops = NULL;
	int serviced = 0;
	int count;

	processorIsrEnter(address);

	// Which interrupt number is active?
	interruptNum = kernelPicGetActive();
	if (interruptNum < 0)
		goto out;

	kernelInterruptSetCurrent(interruptNum);

	// Find the devices that use this interrupt
	for (count = 0; (count < numDevices) && !serviced; count ++)
	{
		if (((kernelNetworkDevice *)
			devices[count]->data)->device.interruptNum == interruptNum)
		{
			dev = devices[count];
			adapter = dev->data;
			ops = dev->driver->ops;

			if (ops->driverInterruptHandler)
			{
				// Try to get a lock, though it might fail since we are are
				// inside an interrupt
				kernelLockGet(&adapter->lock);

				// Call the driver function.
				if (ops->driverInterruptHandler(adapter) >= 0)
				{
					// Read the data from all queued packets
					while (adapter->device.recvQueued)
						readData(dev);

					serviced = 1;
				}

				kernelLockRelease(&adapter->lock);
			}
		}
	}

	if (serviced)
		kernelPicEndOfInterrupt(interruptNum);

	kernelInterruptClearCurrent();

	if (!serviced)
	{
		if (oldIntHandlers[interruptNum])
		{
			// We didn't service this interrupt, and we're sharing this PCI
			// interrupt with another device whose handler we saved.  Call it.
			kernelDebug(debug_net, "NETDEV interrupt not serviced - "
				"chaining");
			processorIsrCall(oldIntHandlers[interruptNum]);
		}
		else
		{
			// We'd better acknowledge the interrupt, or else it wouldn't be
			// cleared, and our controllers using this vector wouldn't receive
			// any more.
			kernelDebugError("Interrupt not serviced and no saved ISR");
			kernelPicEndOfInterrupt(interruptNum);
		}
	}

out:
	processorIsrExit(address);
}


static kernelDevice *findDeviceByName(const char *adapterName)
{
	// Find the named adapter

	kernelNetworkDevice *adapter = NULL;
	int count;

	for (count = 0; count < numDevices; count ++)
	{
		adapter = devices[count]->data;
		if (!strcmp((char *) adapter->device.name, adapterName))
			return (devices[count]);
	}

	// Not found
	return (NULL);
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
//  Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////

int kernelNetworkDeviceRegister(kernelDevice *dev)
{
	// This function is called by the network drivers' detection functions
	// to tell us about a new adapter device.

	int status = 0;
	kernelNetworkDevice *adapter = NULL;

	// Check params
	if (!dev || !dev->data || !dev->driver || !dev->driver->ops)
	{
		kernelError(kernel_error, "NULL parameter");
		return (status = ERR_NULLPARAMETER);
	}

	adapter = dev->data;

	if (adapter->device.linkProtocol == NETWORK_LINKPROTOCOL_LOOP)
		strcpy((char *) adapter->device.name, "loop");
	else
		sprintf((char *) adapter->device.name, "net%d", numDevices);

	if (adapter->device.interruptNum >= 0)
	{
		// Save any existing handler for the interrupt we're hooking

		if (numOldHandlers <= adapter->device.interruptNum)
		{
			numOldHandlers = (adapter->device.interruptNum + 1);

			oldIntHandlers = kernelRealloc(oldIntHandlers,
				(numOldHandlers * sizeof(void *)));
			if (!oldIntHandlers)
				return (status = ERR_MEMORY);
		}

		if (!oldIntHandlers[adapter->device.interruptNum] &&
			(kernelInterruptGetHandler(adapter->device.interruptNum) !=
				networkInterrupt))
		{
			oldIntHandlers[adapter->device.interruptNum] =
				kernelInterruptGetHandler(adapter->device.interruptNum);
		}

		// Register our interrupt handler for this device
		status = kernelInterruptHook(adapter->device.interruptNum,
			&networkInterrupt, NULL);
		if (status < 0)
			return (status);
	}

	devices[numDevices++] = dev;

	if (adapter->device.interruptNum >= 0)
	{
		// Turn on the interrupt
		status = kernelPicMask(adapter->device.interruptNum, 1);
		if (status < 0)
			return (status);
	}

	// Register the adapter with the upper-level kernelNetwork functions
	status = kernelNetworkRegister(adapter);
	if (status < 0)
		return (status);

	kernelLog("Added network adapter %s, link=%s", adapter->device.name,
		((adapter->device.flags & NETWORK_ADAPTERFLAG_LINK)? "UP" : "DOWN"));

	return (status = 0);
}


int kernelNetworkDeviceSetFlags(const char *name, unsigned flags, int onOff)
{
	// Changes any user-settable flags associated with a network device

	int status = 0;
	kernelDevice *dev = NULL;
	kernelNetworkDevice *adapter = NULL;
	kernelNetworkDeviceOps *ops = NULL;

	// Check params
	if (!name)
	{
		kernelError(kernel_error, "NULL parameter");
		return (status = ERR_NULLPARAMETER);
	}

	// Find the adapter by name
	dev = findDeviceByName(name);
	if (!dev)
	{
		kernelError(kernel_error, "No such network adapter \"%s\"", name);
		return (status = ERR_NOSUCHENTRY);
	}

	adapter = dev->data;
	ops = dev->driver->ops;

	// Lock the adapter
	status = kernelLockGet(&adapter->lock);
	if (status < 0)
		return (status);

	if (ops->driverSetFlags)
		// Call the driver flag-setting function.
		status = ops->driverSetFlags(adapter, flags, onOff);

	// Release the lock
	kernelLockRelease(&adapter->lock);

	return (status);
}


int kernelNetworkDeviceGetAddress(const char *name,
	networkAddress *logicalAddress, networkAddress *physicalAddress)
{
	// This function attempts to use the named network adapter to determine
	// the physical address of the host with the supplied logical address.
	// The Address Resolution Protocol (ARP) is used for this.

	int status = 0;
	kernelDevice *dev = NULL;
	kernelNetworkDevice *adapter = NULL;
	int arpPosition = 0;
	int count;

	// Check params
	if (!name || !logicalAddress || !physicalAddress)
	{
		kernelError(kernel_error, "NULL parameter");
		return (status = ERR_NULLPARAMETER);
	}

	// Find the adapter by name
	dev = findDeviceByName(name);
	if (!dev)
	{
		kernelError(kernel_error, "No such network adapter \"%s\"", name);
		return (status = ERR_NOSUCHENTRY);
	}

	adapter = dev->data;

	// Shortcut (necessary for loopback) if the address is the address of the
	// adapter itself
	if (networkAddressesEqual(logicalAddress, &adapter->device.hostAddress,
		sizeof(networkAddress)))
	{
		networkAddressCopy(physicalAddress, &adapter->device.hardwareAddress,
			sizeof(networkAddress));
		return (status = 0);
	}

	// Test whether the logical address is in this device's network, using
	// the netmask.  If it's a different network, substitute the address of
	// the default gateway.
	if (!networksEqualIp4(logicalAddress, &adapter->device.netMask,
		&adapter->device.hostAddress))
	{
		kernelDebug(debug_net, "NETDEV routing via default gateway");
		logicalAddress = (networkAddress *) &adapter->device.gatewayAddress;
	}

	// Try up to 6 attempts to get an address.  This is arbitrary.  Is it
	// right?  From network activity, it looks like Linux tries approx 6
	// times, when we don't reply to it; once per second.
	for (count = 0; count < 6; count ++)
	{
		// Is the address in the adapter's ARP cache?
		arpPosition = kernelNetworkArpSearchCache(adapter, logicalAddress);
		if (arpPosition >= 0)
		{
			// Found it.
			kernelDebug(debug_net, "NETDEV found ARP cache request");
			networkAddressCopy(physicalAddress,
				&adapter->arpCache[arpPosition].physicalAddress,
				sizeof(networkAddress));
			return (status = 0);
		}

		// Construct and send our ethernet packet with the ARP request
		// (not queued; immediately)
		status = kernelNetworkArpSend(adapter, logicalAddress, NULL,
			NETWORK_ARPOP_REQUEST, 1 /* immediate */);
		if (status < 0)
			return (status);

		// Expect a quick reply the first time
		if (!count)
			kernelMultitaskerYield();
		else
			// Delay for 1/2 second
			kernelMultitaskerWait(500);
	}

	// If we fall through, we didn't find it.
	return (status = ERR_NOSUCHENTRY);
}


int kernelNetworkDeviceSend(const char *name, kernelNetworkPacket *packet)
{
	// Send a prepared packet using the named network adapter

	int status = 0;
	kernelDevice *dev = NULL;
	kernelNetworkDevice *adapter = NULL;
	kernelNetworkDeviceOps *ops = NULL;

	// Check params
	if (!name || !packet)
	{
		kernelError(kernel_error, "NULL parameter");
		return (status = ERR_NULLPARAMETER);
	}

	kernelDebug(debug_net, "NETDEV send %u on %s", packet->length, name);

	// Find the adapter by name
	dev = findDeviceByName(name);
	if (!dev)
	{
		kernelError(kernel_error, "No such network adapter \"%s\"", name);
		return (status = ERR_NOSUCHENTRY);
	}

	if (!packet->length)
		// Nothing to do?  Hum.
		return (status = 0);

	adapter = dev->data;
	ops = dev->driver->ops;

	// If there are output hooks on this adapter, emit the raw packet data
	processHooks(adapter, packet, 0 /* output */);

	// Lock the adapter
	status = kernelLockGet(&adapter->lock);
	if (status < 0)
		return (status);

	if (ops->driverWriteData)
		// Call the driver transmit function.
		status = ops->driverWriteData(adapter, packet->memory, packet->length);

	// Wait until all packets are transmitted before returning, since the
	// memory is needed by the adapter
	while (adapter->device.transQueued)
		kernelMultitaskerYield();

	// Release the lock
	kernelLockRelease(&adapter->lock);

	if (status >= 0)
	{
		adapter->device.transPackets += 1;

		switch (adapter->device.linkProtocol)
		{
			case NETWORK_LINKPROTOCOL_LOOP:
			{
				kernelDebug(debug_net, "NETDEV send %d: loopback msgsz %d",
					adapter->device.transPackets, packet->length);
				break;
			}

			case NETWORK_LINKPROTOCOL_ETHERNET:
			{
			#if defined(DEBUG)
				networkEthernetHeader *header = (networkEthernetHeader *)
					packet->memory;

				kernelDebug(debug_net, "NETDEV send %d: ethernet type=%x "
					"%02x:%02x:%02x:%02x:%02x:%02x -> "
					"%02x:%02x:%02x:%02x:%02x:%02x msgsz %d",
					adapter->device.transPackets, ntohs(header->type),
					header->source[0], header->source[1], header->source[2],
					header->source[3], header->source[4], header->source[5],
					header->dest[0], header->dest[1], header->dest[2],
					header->dest[3], header->dest[4], header->dest[5],
					packet->length);
			#endif
				break;
			}
		}
	}

	// If the adapter is a loop device, attempt to process the input now
	if (adapter->device.linkProtocol == NETWORK_LINKPROTOCOL_LOOP)
		readData(dev);

	return (status);
}


int kernelNetworkDeviceGetCount(void)
{
	// Returns the count of real network devices (not including loopback)

	int devCount = 0;
	kernelNetworkDevice *adapter = NULL;
	int count;

	for (count = 0; count < numDevices; count ++)
	{
		adapter = devices[count]->data;

		if (adapter->device.linkProtocol != NETWORK_LINKPROTOCOL_LOOP)
			devCount += 1;
	}

	return (devCount);
}


int kernelNetworkDeviceGet(const char *name, networkDevice *dev)
{
	// Returns the user-space portion of the requested (by name) network
	// device.

	int status = 0;
	kernelDevice *kernelDev = NULL;
	kernelNetworkDevice *adapter = NULL;

	// Check params
	if (!name || !dev)
	{
		kernelError(kernel_error, "NULL parameter");
		return (status = ERR_NULLPARAMETER);
	}

	// Find the adapter by name
	kernelDev = findDeviceByName(name);
	if (!kernelDev)
	{
		kernelError(kernel_error, "No such network adapter \"%s\"", name);
		return (status = ERR_NOSUCHENTRY);
	}

	adapter = kernelDev->data;

	memcpy(dev, (networkDevice *) &adapter->device, sizeof(networkDevice));

	return (status = 0);
}


int kernelNetworkDeviceHook(const char *name, void **streamPtr, int input)
{
	// Allocates a new network packet stream and associates it with the
	// named adapter, 'hooking' either the input or output, and returning a
	// pointer to the stream.

	int status = 0;
	kernelDevice *kernelDev = NULL;
	kernelNetworkDevice *adapter = NULL;
	kernelNetworkPacketStream *theStream = NULL;
	kernelLinkedList *list = NULL;

	// Check params
	if (!name || !streamPtr)
	{
		kernelError(kernel_error, "NULL parameter");
		return (status = ERR_NULLPARAMETER);
	}

	// Find the adapter by name
	kernelDev = findDeviceByName(name);
	if (!kernelDev)
	{
		kernelError(kernel_error, "No such network adapter \"%s\"", name);
		return (status = ERR_NOSUCHENTRY);
	}

	adapter = kernelDev->data;

	*streamPtr = kernelMalloc(sizeof(kernelNetworkPacketStream));
	if (!*streamPtr)
	{
		kernelError(kernel_error, "Couldn't allocate network packet stream");
		return (status = ERR_MEMORY);
	}

	theStream = *streamPtr;

	// Try to get a new network packet stream
	status = kernelNetworkPacketStreamNew(theStream);
	if (status < 0)
	{
		kernelError(kernel_error, "Couldn't allocate network packet stream");
		kernelFree(*streamPtr);
		return (status);
	}

	// Which list are we adding to?
	if (input)
		list = (kernelLinkedList *) &adapter->inputHooks;
	else
		list = (kernelLinkedList *) &adapter->outputHooks;

	// Add it to the list
	status = kernelLinkedListAdd(list, *streamPtr);
	if (status < 0)
	{
		kernelError(kernel_error, "Couldn't link network packet stream");
		kernelNetworkPacketStreamDestroy(theStream);
		kernelFree(*streamPtr);
		return (status);
	}

	return (status = 0);
}


int kernelNetworkDeviceUnhook(const char *name, void *streamPtr, int input)
{
	// 'Unhooks' the supplied network packet stream from the input or output
	// of the named adapter and deallocates the stream.

	int status = 0;
	kernelDevice *kernelDev = NULL;
	kernelNetworkDevice *adapter = NULL;
	kernelNetworkPacketStream *theStream = streamPtr;
	kernelLinkedList *list = NULL;

	// Check params
	if (!name || !streamPtr)
	{
		kernelError(kernel_error, "NULL parameter");
		return (status = ERR_NULLPARAMETER);
	}

	// Find the adapter by name
	kernelDev = findDeviceByName(name);
	if (!kernelDev)
	{
		kernelError(kernel_error, "No such network adapter \"%s\"", name);
		return (status = ERR_NOSUCHENTRY);
	}

	adapter = kernelDev->data;

	// Which list are we removing from?
	if (input)
		list = (kernelLinkedList *) &adapter->inputHooks;
	else
		list = (kernelLinkedList *) &adapter->outputHooks;

	// Remove it from the list
	status = kernelLinkedListRemove(list, streamPtr);
	if (status < 0)
	{
		kernelError(kernel_error, "Couldn't unlink network packet stream");
		return (status);
	}

	kernelNetworkPacketStreamDestroy(theStream);
	kernelFree(streamPtr);

	return (status = 0);
}


unsigned kernelNetworkDeviceSniff(void *streamPtr, unsigned char *buffer,
	unsigned len)
{
	// Given a pointer to a network packet stream 'hooked' to the input or
	// output of a device, attempt to retrieve a packet, and copy at most the
	// requested number of bytes to the buffer.

	unsigned bytes = 0;
	kernelNetworkPacketStream *theStream = streamPtr;
	kernelNetworkPacket *packet = NULL;

	// Check params
	if (!streamPtr || !buffer)
	{
		kernelError(kernel_error, "NULL parameter");
		return (bytes = 0);
	}

	// Try to read a packet
	if (kernelNetworkPacketStreamRead(theStream, &packet) < 0)
		return (bytes = 0);

	bytes = min(len, packet->length);

	// Copy data
	memcpy(buffer, packet->memory, bytes);

	kernelNetworkPacketRelease(packet);

	return (bytes);
}

