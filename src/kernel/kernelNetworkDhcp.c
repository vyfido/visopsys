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
//  kernelNetworkDhcp.c
//

#include "kernelNetworkDhcp.h"
#include "kernelCpu.h"
#include "kernelDebug.h"
#include "kernelError.h"
#include "kernelMultitasker.h"
#include "kernelNetwork.h"
#include "kernelNetworkStream.h"
#include "kernelRandom.h"
#include "kernelRtc.h"
#include <stdlib.h>
#include <string.h>
#include <sys/network.h>
#include <sys/processor.h>
#include <sys/types.h>


static networkDhcpOption *getSpecificDhcpOption(networkDhcpPacket *packet,
	unsigned char optionNumber)
{
	// Returns the requested option, if present.

	networkDhcpOption *option = (networkDhcpOption *) packet->options;
	int count;

	// Loop through the options until we either get the requested one, or else
	// hit the end of the list
	for (count = 0; ; count ++)
	{
		if (option->code == optionNumber)
			return (option);

		if (option->code == NETWORK_DHCPOPTION_END)
			return (option = NULL);

		option = ((void *) option + 2 + option->length);
	}

	return (option = NULL);
}


static void setDhcpOption(networkDhcpPacket *packet, int code, int length,
	unsigned char *data)
{
	// Adds the supplied DHCP option to the packet

	networkDhcpOption *option = NULL;
	int count;

	// Check whether the option is already present
	if (!(option = getSpecificDhcpOption(packet, code)))
	{
		// Not present.
		option = (networkDhcpOption *) packet->options;

		// Loop through the options until we find the end
		while (1)
		{
			if (option->code == NETWORK_DHCPOPTION_END)
				break;

			option = ((void *) option + 2 + option->length);
		}
	}

	option->code = code;
	option->length = length;
	for (count = 0; count < length; count ++)
		option->data[count] = data[count];
	option->data[length] = NETWORK_DHCPOPTION_END;

	return;
}


static int sendDhcpDiscover(kernelNetworkDevice *adapter,
	kernelNetworkConnection *connection)
{
	int status = 0;
	networkDhcpPacket sendDhcpPacket;

	kernelDebug(debug_net, "DHCP send discover");

	// Clear our packet
	memset(&sendDhcpPacket, 0, sizeof(networkDhcpPacket));

	// Set up our DHCP payload

	// Opcode is boot request
	sendDhcpPacket.opCode = NETWORK_DHCPOPCODE_BOOTREQUEST;

	// Hardware address space is ethernet=1
	sendDhcpPacket.hardwareType = NETWORK_DHCPHARDWARE_ETHERNET;
	sendDhcpPacket.hardwareAddrLen = NETWORK_ADDRLENGTH_ETHERNET;
	sendDhcpPacket.transactionId = processorSwap32(kernelRandomUnformatted());

	// Our ethernet hardware address
	memcpy(&sendDhcpPacket.clientHardwareAddr, (void *)
		&adapter->device.hardwareAddress, NETWORK_ADDRLENGTH_ETHERNET);

	// Magic DHCP cookie
	sendDhcpPacket.cookie = processorSwap32(NETWORK_DHCP_COOKIE);

	// Options.  First one is mandatory message type
	sendDhcpPacket.options[0] = NETWORK_DHCPOPTION_END;
	setDhcpOption(&sendDhcpPacket, NETWORK_DHCPOPTION_MSGTYPE, 1,
		(unsigned char[]){ NETWORK_DHCPMSG_DHCPDISCOVER });

	// Request an infinite lease time
	unsigned tmpLeaseTime = 0xFFFFFFFF;
	setDhcpOption(&sendDhcpPacket, NETWORK_DHCPOPTION_LEASETIME, 4,
		(unsigned char *) &tmpLeaseTime);

	// Request some parameters
	setDhcpOption(&sendDhcpPacket, NETWORK_DHCPOPTION_PARAMREQ, 7,
		(unsigned char[]){ NETWORK_DHCPOPTION_SUBNET,
			NETWORK_DHCPOPTION_ROUTER,
			NETWORK_DHCPOPTION_DNSSERVER,
			NETWORK_DHCPOPTION_HOSTNAME,
			NETWORK_DHCPOPTION_DOMAIN,
			NETWORK_DHCPOPTION_BROADCAST,
			NETWORK_DHCPOPTION_LEASETIME });

	return (status = kernelNetworkSendData(connection, (unsigned char *)
		&sendDhcpPacket, sizeof(networkDhcpPacket), 1 /* immediate */,
		1 /* free memory */));
}


static int waitDhcpReply(kernelNetworkDevice *adapter,
	kernelNetworkPacket *packet)
{
	// Wait for a DHCP packet to appear in our input queue

	int status = 0;
	uquad_t timeout = (kernelCpuGetMs() + 1500);
	networkDhcpPacket *dhcpPacket = NULL;

	// Time out after ~1.5 seconds
	while (kernelCpuGetMs() <= timeout)
	{
		kernelMultitaskerYield();

		if (adapter->inputStream.count < (sizeof(kernelNetworkPacket) /
			sizeof(unsigned)))
		{
			continue;
		}

		// Read the packet from the stream.  This doesn't currently allocate
		// any memory for packets; the packet's memory buffer just points to
		// the adapter's static buffer, so we mustn't free() it after
		// processing.
		status = kernelNetworkPacketStreamRead(&adapter->inputStream, packet);
		if (status < 0)
		{
			kernelDebugError("Couldn't read packet stream");
			continue;
		}

		// It should be an IP packet
		if (packet->netProtocol != NETWORK_NETPROTOCOL_IP)
		{
			kernelDebug(debug_net, "DHCP not an IP packet");
			continue;
		}

		//kernelNetworkIpDebug(packet->netHeader);

		// Set up the received packet for further interpretation
		status = kernelNetworkSetupReceivedPacket(packet);
		if (status < 0)
		{
			kernelDebugError("Set up received packet failed");
			continue;
		}

		// See if the input and output ports are appropriate for BOOTP/DHCP
		if ((packet->srcPort != NETWORK_PORT_BOOTPSERVER) ||
			(packet->destPort != NETWORK_PORT_BOOTPCLIENT))
		{
			kernelDebug(debug_net, "DHCP not a BOOTP/DHCP packet");
			continue;
		}

		dhcpPacket = (networkDhcpPacket *) packet->data;

		// Check for DHCP cookie
		if (processorSwap32(dhcpPacket->cookie) != NETWORK_DHCP_COOKIE)
		{
			kernelDebug(debug_net, "DHCP cookie missing");
			continue;
		}

		// Looks okay to us
		return (status = 0);
	}

	// No response from the server
	kernelDebugError("DHCP timeout");
	return (status = ERR_NODATA);
}


static networkDhcpOption *getDhcpOption(networkDhcpPacket *packet, int idx)
{
	// Returns the indexed DHCP option

	networkDhcpOption *option = (networkDhcpOption *) packet->options;
	int count;

	// Loop through the options until we get to the one that's wanted
	for (count = 0; count < idx; count ++)
	{
		if (option->code == NETWORK_DHCPOPTION_END)
			// Because 'count' is less than the requested index, the caller is
			// requesting an option that doesn't exist
			return (option = NULL);

		option = ((void *) option + 2 + option->length);
	}

	return (option);
}


static int sendDhcpRequest(kernelNetworkConnection *connection,
	networkDhcpPacket *requestPacket)
{
	// Given the packet returned as an 'offer' from DHCP, accept the offer
	// by converting it into a 'request' and sending it back

	int status = 0;
	char hostName[NETWORK_MAX_HOSTNAMELENGTH];
	char domainName[NETWORK_MAX_DOMAINNAMELENGTH];

	kernelDebug(debug_net, "DHCP send request");

	// Re-set the message type
	requestPacket->opCode = NETWORK_DHCPOPCODE_BOOTREQUEST;
	requestPacket->options[2] = NETWORK_DHCPMSG_DHCPREQUEST;

	// Add an option to request the supplied address
	setDhcpOption(requestPacket, NETWORK_DHCPOPTION_ADDRESSREQ,
		NETWORK_ADDRLENGTH_IPV4, (void *) &requestPacket->yourLogicalAddr);

	// If the server did not specify a host name to us, specify one to it.
	if (!getSpecificDhcpOption(requestPacket, NETWORK_DHCPOPTION_HOSTNAME))
	{
		if (kernelNetworkGetHostName(hostName,
			NETWORK_MAX_HOSTNAMELENGTH) >= 0)
		{
			setDhcpOption(requestPacket, NETWORK_DHCPOPTION_HOSTNAME,
				(strlen(hostName) + 1), (unsigned char *) hostName);
		}
	}

	// If the server did not specify a domain name to us, specify one to it.
	if (!getSpecificDhcpOption(requestPacket, NETWORK_DHCPOPTION_DOMAIN))
	{
		if (kernelNetworkGetDomainName(domainName,
			NETWORK_MAX_DOMAINNAMELENGTH) >= 0)
		{
			setDhcpOption(requestPacket, NETWORK_DHCPOPTION_DOMAIN,
				(strlen(domainName) + 1), (unsigned char *) domainName);
		}
	}

	// Clear the 'your address' field
	memset(&requestPacket->yourLogicalAddr, 0, NETWORK_ADDRLENGTH_IPV4);

	return (status = kernelNetworkSendData(connection, (unsigned char *)
		requestPacket, sizeof(networkDhcpPacket), 1 /* immediate */,
		1 /* free memory */));
}


static void evaluateDhcpOptions(kernelNetworkDevice *adapter,
	networkDhcpPacket *ackPacket)
{
	networkDhcpOption *option = NULL;
	int count;

	kernelDebug(debug_net, "DHCP evaluate options");

	// Loop through all of the options
	for (count = 0; ; count ++)
	{
		option = getDhcpOption(ackPacket, count);

		if (option->code == NETWORK_DHCPOPTION_END)
			// That's the end of the options
			break;

		// Look for the options we desired
		switch (option->code)
		{
			case NETWORK_DHCPOPTION_SUBNET:
				// The server supplied the subnet mask
				memcpy((void *) &adapter->device.netMask, option->data,
					min(option->length, NETWORK_ADDRLENGTH_IPV4));
				break;

			case NETWORK_DHCPOPTION_ROUTER:
				// The server supplied the gateway address
				memcpy((void *) &adapter->device.gatewayAddress,
					option->data, min(option->length,
						NETWORK_ADDRLENGTH_IPV4));
				break;

			case NETWORK_DHCPOPTION_DNSSERVER:
				// The server supplied the DNS server address
				break;

			case NETWORK_DHCPOPTION_HOSTNAME:
				// The server supplied the host name
				kernelNetworkSetHostName((char *) option->data,
					option->length);
				break;

			case NETWORK_DHCPOPTION_DOMAIN:
				// The server supplied the domain name
				kernelNetworkSetDomainName((char *) option->data,
					option->length);
				break;

			case NETWORK_DHCPOPTION_BROADCAST:
				// The server supplied the broadcast address
				memcpy((void *) &adapter->device.broadcastAddress,
					option->data, min(option->length,
						NETWORK_ADDRLENGTH_IPV4));
				break;

			case NETWORK_DHCPOPTION_LEASETIME:
				// The server specified the lease time
				adapter->dhcpConfig.leaseExpiry =
					(kernelRtcUptimeSeconds() +
				 		processorSwap32(*((unsigned * ) option->data)));
				//kernelTextPrintLine("Lease expiry at %d seconds",
				//	adapter->dhcpConfig.leaseExpiry);
				break;

			default:
				// Unknown/unwanted information
				break;
		}
	}
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
//  Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////

int kernelNetworkDhcpConfigure(kernelNetworkDevice *adapter, unsigned timeout)
{
	// This function attempts to configure the supplied adapter via the DHCP
	// protocol.  The adapter needs to be stopped since it expects to be able
	// to poll the adapter's packet input stream, and not be interfered with
	// by the network thread.

	int status = 0;
	networkFilter filter;
	kernelNetworkConnection *connection = NULL;
	uquad_t endTime = (kernelCpuGetMs() + timeout);
	networkDhcpPacket sendDhcpPacket;
	kernelNetworkPacket packet;
	networkDhcpPacket *recvDhcpPacket = NULL;

	// Make sure the adapter is stopped, and yield the timeslice to make sure
	// the network thread is not in the middle of anything
	adapter->device.flags &= ~NETWORK_ADAPTERFLAG_RUNNING;
	kernelMultitaskerYield();

	// Get a connection for sending and receiving
	kernelDebug(debug_net, "DHCP open connection");
	memset(&filter, 0, sizeof(networkFilter));
	filter.transProtocol = NETWORK_TRANSPROTOCOL_UDP;
	filter.localPort = NETWORK_PORT_BOOTPCLIENT;
	filter.remotePort = NETWORK_PORT_BOOTPSERVER;
	connection = kernelNetworkConnectionOpen(adapter, NETWORK_MODE_WRITE,
		&NETWORK_BROADCAST_ADDR, &filter);
	if (!connection)
		return (status = ERR_INVALID);

	while (kernelCpuGetMs() < endTime)
	{
		if (adapter->device.flags & NETWORK_ADAPTERFLAG_AUTOCONF)
		{
			// If this adapter already has an existing DHCP configuration,
			// we will attempt to renew it.
			memcpy(&sendDhcpPacket, (void *) &adapter->dhcpConfig.dhcpPacket,
				sizeof(networkDhcpPacket));
		}
		else
		{
			// Send DHCP discovery
			status = sendDhcpDiscover(adapter, connection);
			if (status < 0)
				break;

			// Wait for a DHCP offer
			while (kernelCpuGetMs() < endTime)
			{
				kernelDebug(debug_net, "DHCP wait offer");

				// Wait for a DHCP server reply
				status = waitDhcpReply(adapter, &packet);
				if (status < 0)
					break;

				kernelDebug(debug_net, "DHCP offer received");

				recvDhcpPacket = (networkDhcpPacket *) packet.data;

				// Should be a DHCP reply, and the first option should be a
				// DHCP 'offer' message type
				status = ERR_INVALID;
				if ((recvDhcpPacket->opCode == NETWORK_DHCPOPCODE_BOOTREPLY) &&
					(getDhcpOption(recvDhcpPacket, 0)->data[0] ==
						NETWORK_DHCPMSG_DHCPOFFER))
				{
					// Success
					status = 0;
					break;
				}

				kernelDebugError("DHCP not reply opcode or offer option");

				// Keep waiting
			}

			if (status < 0)
				// Attempt to send the discovery again
				continue;

			// Good enough.  Send the reply back to the server as a DHCP
			// request.

			// Copy the supplied data into our send packet
			memcpy(&sendDhcpPacket, recvDhcpPacket,
				sizeof(networkDhcpPacket));
		}

		// (Re-)accept the offer
		status = sendDhcpRequest(connection, &sendDhcpPacket);
		if (status < 0)
			continue;

		// Wait for a DHCP ACK
		while (kernelCpuGetMs() < endTime)
		{
			kernelDebug(debug_net, "DHCP wait ACK");

			// Wait for a DHCP server reply
			status = waitDhcpReply(adapter, &packet);
			if (status < 0)
				break;

			kernelDebug(debug_net, "DHCP ACK received");

			recvDhcpPacket = (networkDhcpPacket *) packet.data;

			// Should be a DHCP reply, and the first option should be a DHCP
			// ACK message type.  If the reply is a DHCP NACK, then perhaps the
			// previously-supplied address has already been allocated to
			// someone else.
			status = ERR_INVALID;
			if ((recvDhcpPacket->opCode == NETWORK_DHCPOPCODE_BOOTREPLY) &&
				(getDhcpOption(recvDhcpPacket, 0)->data[0] ==
					NETWORK_DHCPMSG_DHCPACK))
			{
				status = 0;
				break;
			}

			if (getDhcpOption(recvDhcpPacket, 0)->data[0] ==
				NETWORK_DHCPMSG_DHCPNAK)
			{
				// NACK - start again
				kernelDebugError("DHCP NAK - request refused");
				break;
			}

			kernelDebugError("DHCP not reply opcode or ACK option");

			// Keep waiting
		}

		// Were we successful?
		if (status >= 0)
			break;
	}

	// Communication should be finished.
	kernelNetworkConnectionClose(connection);

	// Were we successful?
	if ((status < 0) || (kernelCpuGetMs() >= endTime))
	{
		if (kernelCpuGetMs() >= endTime)
		{
			kernelError(kernel_error, "DHCP timed out");
			status = ERR_TIMEOUT;
		}

		kernelError(kernel_error, "DHCP auto-configuration of network adapter "
			"%s failed", adapter->device.name);
		return (status);
	}

	// Gather up the information.

	// Copy the host address
	memcpy((void *) &adapter->device.hostAddress,
		&recvDhcpPacket->yourLogicalAddr, NETWORK_ADDRLENGTH_IPV4);

	// Evaluate the options
	evaluateDhcpOptions(adapter, recvDhcpPacket);

	// Copy the DHCP packet into our config structure, so that we can renew,
	// release, etc., the configuration later
	memcpy((void *) &adapter->dhcpConfig.dhcpPacket, recvDhcpPacket,
		sizeof(networkDhcpPacket));

	// Set the adapter 'auto config' flag and mark it as running
	adapter->device.flags |= (NETWORK_ADAPTERFLAG_RUNNING |
		NETWORK_ADAPTERFLAG_AUTOCONF);

	return (status = 0);
}


int kernelNetworkDhcpRelease(kernelNetworkDevice *adapter)
{
	// Tell the DHCP server we're finished with our lease

	int status = 0;
	networkFilter filter;
	kernelNetworkConnection *connection = NULL;
	networkDhcpPacket sendDhcpPacket;

	// Get a connection for sending and receiving
	memset(&filter, 0, sizeof(networkFilter));
	filter.transProtocol = NETWORK_TRANSPROTOCOL_UDP;
	filter.localPort = NETWORK_PORT_BOOTPCLIENT;
	filter.remotePort = NETWORK_PORT_BOOTPSERVER;
	connection = kernelNetworkConnectionOpen(adapter, NETWORK_MODE_WRITE,
		&NETWORK_BROADCAST_ADDR, &filter);
	if (!connection)
		return (status = ERR_INVALID);

	// Copy the saved configuration data into our send packet
	memcpy(&sendDhcpPacket, (void *) &adapter->dhcpConfig.dhcpPacket,
		sizeof(networkDhcpPacket));

	// Re-set the message type
	sendDhcpPacket.opCode = NETWORK_DHCPOPCODE_BOOTREQUEST;
	sendDhcpPacket.options[2] = NETWORK_DHCPMSG_DHCPRELEASE;

	// Send it.
	status = kernelNetworkSendData(connection, (unsigned char *)
		&sendDhcpPacket, sizeof(networkDhcpPacket), 1 /* immediate */,
		1 /* free memory */);

	kernelNetworkConnectionClose(connection);
	return (status);
}

