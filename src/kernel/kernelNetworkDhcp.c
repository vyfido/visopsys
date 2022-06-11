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
#include <arpa/inet.h>
#include <sys/network.h>
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
	sendDhcpPacket.transactionId = htonl(kernelRandomUnformatted());

	// Our ethernet hardware address
	networkAddressCopy(&sendDhcpPacket.clientHardwareAddr,
		&adapter->device.hardwareAddress, NETWORK_ADDRLENGTH_ETHERNET);

	// Magic DHCP cookie
	sendDhcpPacket.cookie = htonl(NETWORK_DHCP_COOKIE);

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
		&sendDhcpPacket, sizeof(networkDhcpPacket), 1 /* immediate */));
}


static int waitDhcpReply(kernelNetworkDevice *adapter,
	kernelNetworkPacket **packet)
{
	// Wait for a DHCP packet to appear in our input queue

	int status = 0;
	uquad_t timeout = (kernelCpuGetMs() + 1500);
	networkDhcpPacket *dhcpPacket = NULL;

	// Time out after ~1.5 seconds
	while (kernelCpuGetMs() <= timeout)
	{
		kernelMultitaskerYield();

		if (!adapter->inputStream.count)
			continue;

		// Read the packet from the stream
		status = kernelNetworkPacketStreamRead(&adapter->inputStream, packet);
		if (status < 0)
		{
			kernelDebugError("Couldn't read packet stream");
			continue;
		}

		// It should be an IP packet
		if ((*packet)->netProtocol != NETWORK_NETPROTOCOL_IP4)
		{
			kernelDebug(debug_net, "DHCP not an IP v4 packet");
			kernelNetworkPacketRelease(*packet);
			continue;
		}

		// Set up the received packet for further interpretation
		status = kernelNetworkSetupReceivedPacket(*packet);
		if (status < 0)
		{
			kernelDebugError("Set up received packet failed");
			kernelNetworkPacketRelease(*packet);
			continue;
		}

		// See if the input and output ports are appropriate for BOOTP/DHCP
		if (((*packet)->srcPort != NETWORK_PORT_BOOTPSERVER) ||
			((*packet)->destPort != NETWORK_PORT_BOOTPCLIENT))
		{
			kernelDebug(debug_net, "DHCP not a BOOTP/DHCP packet");
			kernelNetworkPacketRelease(*packet);
			continue;
		}

		dhcpPacket = (networkDhcpPacket *)((*packet)->memory +
			(*packet)->dataOffset);

		// Check for DHCP cookie
		if (ntohl(dhcpPacket->cookie) != NETWORK_DHCP_COOKIE)
		{
			kernelDebug(debug_net, "DHCP cookie missing");
			kernelNetworkPacketRelease(*packet);
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
	const char *hostName, const char *domainName,
	networkDhcpPacket *requestPacket)
{
	// Given the packet returned as an 'offer' from DHCP, accept the offer
	// by converting it into a 'request' and sending it back

	int status = 0;

	kernelDebug(debug_net, "DHCP send request");

	// Re-set the message type
	requestPacket->opCode = NETWORK_DHCPOPCODE_BOOTREQUEST;
	requestPacket->options[2] = NETWORK_DHCPMSG_DHCPREQUEST;

	// Add an option to request the supplied address
	setDhcpOption(requestPacket, NETWORK_DHCPOPTION_ADDRESSREQ,
		NETWORK_ADDRLENGTH_IP4, (void *) &requestPacket->yourLogicalAddr);

	// If the server did not specify a host name to us, specify one to it.
	if (!getSpecificDhcpOption(requestPacket, NETWORK_DHCPOPTION_HOSTNAME))
	{
		if (hostName && hostName[0])
		{
			setDhcpOption(requestPacket, NETWORK_DHCPOPTION_HOSTNAME,
				(strlen(hostName) + 1), (unsigned char *) hostName);
		}
	}

	// If the server did not specify a domain name to us, specify one to it.
	if (!getSpecificDhcpOption(requestPacket, NETWORK_DHCPOPTION_DOMAIN))
	{
		if (domainName && domainName[0])
		{
			setDhcpOption(requestPacket, NETWORK_DHCPOPTION_DOMAIN,
				(strlen(domainName) + 1), (unsigned char *) domainName);
		}
	}

	// Clear the 'your address' field
	memset(&requestPacket->yourLogicalAddr, 0, NETWORK_ADDRLENGTH_IP4);

	return (status = kernelNetworkSendData(connection, (unsigned char *)
		requestPacket, sizeof(networkDhcpPacket), 1 /* immediate */));
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
				networkAddressCopy(&adapter->device.netMask, option->data,
					min(option->length, NETWORK_ADDRLENGTH_IP4));
				break;

			case NETWORK_DHCPOPTION_ROUTER:
				// The server supplied the gateway address
				networkAddressCopy(&adapter->device.gatewayAddress,
					option->data, min(option->length,
						NETWORK_ADDRLENGTH_IP4));
				break;

			case NETWORK_DHCPOPTION_DNSSERVER:
				// The server supplied the DNS server address
				networkAddressCopy(&adapter->device.dnsAddress, option->data,
					min(option->length, NETWORK_ADDRLENGTH_IP4));
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
				networkAddressCopy(&adapter->device.broadcastAddress,
					option->data, min(option->length,
						NETWORK_ADDRLENGTH_IP4));
				break;

			case NETWORK_DHCPOPTION_LEASETIME:
				// The server specified the lease time
				adapter->dhcpConfig.leaseExpiry =
					(kernelRtcUptimeSeconds() +
				 		ntohl(*((unsigned *) option->data)));
				kernelDebug(debug_net, "DHCP lease expiry at %d seconds",
					adapter->dhcpConfig.leaseExpiry);
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

int kernelNetworkDhcpConfigure(kernelNetworkDevice *adapter,
	const char *hostName, const char *domainName, unsigned timeout)
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
	int haveOffer = 0;
	kernelNetworkPacket *packet = NULL;
	networkDhcpPacket *recvDhcpPacket = NULL;
	int haveAck = 0;

	// Make sure the adapter is stopped, and yield the timeslice to make sure
	// the network thread is not in the middle of anything
	adapter->device.flags &= ~NETWORK_ADAPTERFLAG_RUNNING;
	kernelMultitaskerYield();

	// Get a connection for sending and receiving

	kernelDebug(debug_net, "DHCP open connection");
	memset(&filter, 0, sizeof(networkFilter));
	filter.flags = (NETWORK_FILTERFLAG_TRANSPROTOCOL |
		NETWORK_FILTERFLAG_LOCALPORT | NETWORK_FILTERFLAG_REMOTEPORT);
	filter.transProtocol = NETWORK_TRANSPROTOCOL_UDP;
	filter.localPort = NETWORK_PORT_BOOTPCLIENT;
	filter.remotePort = NETWORK_PORT_BOOTPSERVER;

	connection = kernelNetworkConnectionOpen(adapter, NETWORK_MODE_WRITE,
		&NETWORK_BROADCAST_ADDR_IP4, &filter, 0 /* no input stream */);
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
			haveOffer = 0;

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

				recvDhcpPacket = (networkDhcpPacket *)(packet->memory +
					packet->dataOffset);

				// Should be a DHCP reply, and the first option should be a
				// DHCP 'offer' message type
				if ((recvDhcpPacket->opCode == NETWORK_DHCPOPCODE_BOOTREPLY) &&
					(getDhcpOption(recvDhcpPacket, 0)->data[0] ==
						NETWORK_DHCPMSG_DHCPOFFER))
				{
					// Success.  Copy the supplied data into our send packet.
					kernelDebug(debug_net, "DHCP offer received");
					memcpy(&sendDhcpPacket, recvDhcpPacket,
						sizeof(networkDhcpPacket));
					haveOffer = 1;
				}

				kernelNetworkPacketRelease(packet);

				if (haveOffer)
					break;

				kernelDebugError("DHCP not reply opcode or offer option");

				// Keep waiting
			}
		}

		if (!haveOffer)
			// Attempt to send the discovery again
			continue;

		// (Re-)accept the offer
		status = sendDhcpRequest(connection, hostName, domainName,
			&sendDhcpPacket);
		if (status < 0)
			continue;

		// Wait for a DHCP ACK
		while (kernelCpuGetMs() < endTime)
		{
			kernelDebug(debug_net, "DHCP wait ACK");
			haveAck = 0;

			// Wait for a DHCP server reply
			status = waitDhcpReply(adapter, &packet);
			if (status < 0)
				break;

			recvDhcpPacket = (networkDhcpPacket *)(packet->memory +
				packet->dataOffset);

			// Should be a DHCP reply, and the first option should be a DHCP
			// ACK message type.  If the reply is a DHCP NACK, then perhaps
			// the previously-supplied address has already been allocated to
			// someone else.
			if (recvDhcpPacket->opCode == NETWORK_DHCPOPCODE_BOOTREPLY)
			{
				if (getDhcpOption(recvDhcpPacket, 0)->data[0] ==
					NETWORK_DHCPMSG_DHCPACK)
				{
					kernelDebug(debug_net, "DHCP ACK received");
					haveAck = 1;
				}

				if (getDhcpOption(recvDhcpPacket, 0)->data[0] ==
					NETWORK_DHCPMSG_DHCPNAK)
				{
					// NACK - start again
					kernelDebugError("DHCP NAK - request refused");
					kernelNetworkPacketRelease(packet);
					break;
				}
			}

			kernelNetworkPacketRelease(packet);

			if (haveAck)
				break;

			kernelDebugError("DHCP not reply opcode or ACK option");

			// Keep waiting
		}

		// Were we successful?
		if (haveAck)
			break;

		// Attempt to send the discovery again
	}

	// Communication should be finished.
	kernelNetworkConnectionClose(connection, 0 /* not polite */);

	// Were we successful?
	if (!haveAck || (kernelCpuGetMs() >= endTime))
	{
		if (kernelCpuGetMs() >= endTime)
		{
			kernelError(kernel_error, "DHCP timed out");
			status = ERR_TIMEOUT;
		}

		kernelError(kernel_error, "DHCP auto-configuration of network "
			"adapter %s failed", adapter->device.name);
		return (status);
	}

	// Gather up the information.

	// Copy the host address
	networkAddressCopy(&adapter->device.hostAddress,
		&recvDhcpPacket->yourLogicalAddr, NETWORK_ADDRLENGTH_IP4);

	// Evaluate the options
	evaluateDhcpOptions(adapter, recvDhcpPacket);

	// Copy the DHCP packet into our config structure, so that we can renew,
	// release, etc., the configuration later
	memcpy((void *) &adapter->dhcpConfig.dhcpPacket, recvDhcpPacket,
		sizeof(networkDhcpPacket));

	// Set the adapter 'auto config' flag
	adapter->device.flags |= NETWORK_ADAPTERFLAG_AUTOCONF;

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
	filter.flags = (NETWORK_FILTERFLAG_TRANSPROTOCOL |
		NETWORK_FILTERFLAG_LOCALPORT | NETWORK_FILTERFLAG_REMOTEPORT);
	filter.transProtocol = NETWORK_TRANSPROTOCOL_UDP;
	filter.localPort = NETWORK_PORT_BOOTPCLIENT;
	filter.remotePort = NETWORK_PORT_BOOTPSERVER;
	connection = kernelNetworkConnectionOpen(adapter, NETWORK_MODE_WRITE,
		&NETWORK_BROADCAST_ADDR_IP4, &filter, 0 /* no input stream */);
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
		&sendDhcpPacket, sizeof(networkDhcpPacket), 1 /* immediate */);

	kernelNetworkConnectionClose(connection, 0 /* not polite */);
	return (status);
}

