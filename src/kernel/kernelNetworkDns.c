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
//  kernelNetworkDns.c
//

#include "kernelNetworkDns.h"
#include "kernelCpu.h"
#include "kernelDebug.h"
#include "kernelError.h"
#include "kernelMalloc.h"
#include "kernelMultitasker.h"
#include "kernelNetwork.h"
#include "kernelNetworkDevice.h"
#include "kernelRandom.h"
#include "kernelRtc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/vis.h>

typedef struct {
	char *name;
	networkAddress address;
	int type;
	unsigned accessed;
	unsigned expiry;

} dnsLookup;

// A list of cached query results
static linkedList lookups;

static int initialized = 0;


static void checkInit(void)
{
	if (!initialized)
	{
		memset(&lookups, 0, sizeof(linkedList));
		initialized = 1;
	}
}


static int addLookup(char *name, networkAddress *address, int type,
	unsigned timeToLive)
{
	int status = 0;
	dnsLookup *lookup = NULL;
	unsigned currentSecond = 0;

	kernelDebug(debug_net, "DNS add cached lookup %s %d.%d.%d.%d ttl %u",
		name, address->byte[0], address->byte[1], address->byte[2],
		address->byte[3], timeToLive);

	lookup = kernelMalloc(sizeof(dnsLookup));
	if (!lookup)
		return (status = ERR_MEMORY);

	currentSecond = kernelRtcUptimeSeconds();

	// Cache for at least 1 second
	if (!timeToLive)
		timeToLive = 1;

	lookup->name = name;
	memcpy(&lookup->address, address, sizeof(networkAddress));
	lookup->type = type;
	lookup->accessed = currentSecond;
	lookup->expiry = (currentSecond + timeToLive);

	status = linkedListAddFront(&lookups, lookup);

	return (status);
}


static void removeLookup(dnsLookup *lookup)
{
	kernelDebug(debug_net, "DNS remove cached lookup %s", lookup->name);

	linkedListRemove(&lookups, lookup);

	if (lookup->name)
		kernelFree((void *) lookup->name);

	kernelFree(lookup);
}


static dnsLookup *findLookupByName(const char *name)
{
	// Search through our list of lookups for one with a matching name

	dnsLookup *lookup = NULL;
	linkedListItem *iter = NULL;
	unsigned currentSecond = 0;

	kernelDebug(debug_net, "DNS search lookup cache for name %s", name);

	currentSecond = kernelRtcUptimeSeconds();

	lookup = linkedListIterStart(&lookups, &iter);

	while (lookup)
	{
		// Has this entry expired?  N.B. '>' so that replies with a 0 (no
		// cache) time to live will be cached for between 1-2 seconds - long
		// enough for the current lookup (and roughly concurrent ones) to
		// succeed.
		if (currentSecond > lookup->expiry)
		{
			kernelDebug(debug_net, "DNS cached lookup %s expired",
				lookup->name);
			removeLookup(lookup);
		}
		else
		{
			if (!strcmp(name, lookup->name))
			{
				kernelDebug(debug_net, "DNS returning cached result");
				lookup->accessed = currentSecond;
				return (lookup);
			}
		}

		lookup = linkedListIterNext(&lookups, &iter);
	}

	// Not found
	return (lookup = NULL);
}


static unsigned dnsNameLen(networkDnsName *dnsName, unsigned maxLen)
{
	// Return the actual length of a networkDnsName array.  Does not resolve
	// pointers, only adds the length of the pointer itself.

	unsigned len = 0;

	while (dnsName->len)
	{
		if (dnsName->len >= 0xC0)
		{
			// The name ends here with a pointer (2 bytes)
			return (len + 2);
		}

		len += (1 + dnsName->len);

		if (len > maxLen)
			// Something might be wrong
			return (len = 0);

		dnsName = ((void *) dnsName + (1 + dnsName->len));
	}

	// Add 1 for the terminating NULL
	return (len + 1);
}


static int readDnsName(void *replyHeader, unsigned replyLen, unsigned offset,
	char **name)
{
	// Turn an array of networkDnsName structures into a string.  Resolves
	// pointers as required.

	int status = 0;
	networkDnsName *dnsName = (replyHeader + offset);
	unsigned nameLen = 0;
	unsigned outCount = 0;
	unsigned tmp;

	kernelDebug(debug_net, "DNS read DNS name");

	// Get the size of the array
	nameLen = dnsNameLen(dnsName, (replyLen - offset));
	if (!nameLen || ((offset + nameLen) > replyLen))
		return (status = ERR_BADDATA);

	// The string requires one fewer bytes
	*name = kernelMalloc(nameLen - 1);
	if (!*name)
		return (status = ERR_MEMORY);

	while (dnsName->len)
	{
		if (dnsName->len >= 0xC0)
		{
			// The name has a pointer here

			// Set the new offset to the pointer
			offset = (ntohs(*((unsigned short *) dnsName)) & 0x3FFF);

			kernelDebug(debug_net, "DNS name has a pointer to %u", offset);

			if (offset >= replyLen)
			{
				kernelFree(*name);
				return (status = ERR_BADDATA);
			}

			dnsName = (replyHeader + offset);

			tmp = dnsNameLen(dnsName, (replyLen - offset));
			if (!tmp || ((offset + tmp) > replyLen))
			{
				kernelFree(*name);
				return (status = ERR_BADDATA);
			}

			nameLen += tmp;

			*name = kernelRealloc(*name, (nameLen - 1));
			if (!*name)
				return (status = ERR_MEMORY);
		}

		strncpy((*name + outCount), dnsName->name, dnsName->len);
		outCount += dnsName->len;

		dnsName = ((void *) dnsName + (1 + dnsName->len));

		if (dnsName->len)
			strcpy((*name + outCount++), ".");
	}

	kernelDebug(debug_net, "DNS name read %s", *name);
	return (status = 0);
}


static int writeDnsName(const char *_name, networkDnsName **_dnsName)
{
	// Turn a network address string into an array of networkDnsName
	// structures

	int status = 0;
	networkDnsName *dnsName = NULL;
	int nameLen = 0;
	char *name = NULL;
	char *token = NULL;
	char *savePtr = NULL;

	// Make a copy of the name string

	kernelDebug(debug_net, "DNS write DNS name for %s", _name);

	nameLen = strlen(_name);
	name = kernelMalloc(nameLen + 1);
	if (!name)
		return (status = ERR_MEMORY);

	strncpy(name, _name, nameLen);

	// Get memory for our array.  Requires 2 more bytes than the string.
	dnsName = kernelMalloc(nameLen + 2);
	if (!dnsName)
	{
		kernelFree(name);
		return (status = ERR_MEMORY);
	}

	// Parse the name into tokens delimited by '.'

	token = strtok_r(name, ".", &savePtr);
	if (!token)
	{
		kernelFree(dnsName);
		kernelFree(name);
		return (status = ERR_INVALID);
	}

	*_dnsName = dnsName;

	while (token)
	{
		dnsName->len = strlen(token);
		strncpy(dnsName->name, token, dnsName->len);

		// Move to the next bit
		token = strtok_r(NULL, ".", &savePtr);

		if (token)
			dnsName = ((void *) dnsName + (dnsName->len + 1));
	}

	// NULL-terminate the last bit
	dnsName->name[dnsName->len] = '\0';

	kernelFree(name);

	return (status = (nameLen + 2));
}


static int makeNameQuery(const char *name, unsigned char **query,
	unsigned *queryLen)
{
	// Compose a name query

	int status = 0;
	int nameLen = 0;
	networkDnsName *dnsName = NULL;
	networkDnsHeader *queryHeader = NULL;
	networkDnsQuesion *question = NULL;

	kernelDebug(debug_net, "DNS make name query for %s", name);

	nameLen = writeDnsName(name, &dnsName);
	if (nameLen < 0)
	{
		kernelError(kernel_error, "Couldn't compose DNS name");
		return (status = ERR_MEMORY);
	}

	// Get memory for our query packet

	*queryLen = (sizeof(networkDnsHeader) + nameLen +
		sizeof(networkDnsQuesion));

	*query = kernelMalloc(*queryLen);
	if (!*query)
	{
		*queryLen = 0;
		kernelFree(dnsName);
		return (status = ERR_MEMORY);
	}

	// Set up the query header
	queryHeader = (networkDnsHeader *) *query;
	queryHeader->transId = htons(kernelRandomFormatted(0, 0xFFFF));
	queryHeader->flags = htons(NETWORK_DNSFLAG_RECURSE_DESIRED);
	queryHeader->numQuestions = htons(1);

	// Copy the name to the question section
	memcpy((*query + sizeof(networkDnsHeader)), dnsName, nameLen);
	kernelFree(dnsName);

	// Set up the rest of the question section
	question = (networkDnsQuesion *)(*query + sizeof(networkDnsHeader) +
		nameLen);
	question->type = htons(NETWORK_DNSRECTYPE_HOST_A);
	question->classCode = htons(0x0001); /* Internet */

	return (status = 0);
}


static kernelNetworkConnection *sendQuery(kernelNetworkDevice *netDev,
	unsigned char *query, unsigned queryLen)
{
	// Open a connection, and send the query to the server.  If successful,
	// returns the connection to wait for a response.

	int status = 0;
	networkFilter filter;
	kernelNetworkConnection *connection = NULL;

	// Make sure that the network device has a DNS server available
	if (networkAddressEmpty(&netDev->device.dnsAddress,
		sizeof(networkAddress)))
	{
		kernelError(kernel_error, "No DNS server assigned to %s",
			netDev->device.name);
		return (connection = NULL);
	}

	// Get a connection for sending

	memset(&filter, 0, sizeof(networkFilter));
	filter.flags = (NETWORK_FILTERFLAG_NETPROTOCOL |
		NETWORK_FILTERFLAG_TRANSPROTOCOL | NETWORK_FILTERFLAG_REMOTEPORT);
	filter.netProtocol = NETWORK_NETPROTOCOL_IP4;
	filter.transProtocol = NETWORK_TRANSPROTOCOL_UDP;
	filter.remotePort = NETWORK_PORT_DNS;

	kernelDebug(debug_net, "DNS connecting to server %d.%d.%d.%d port %d",
		netDev->device.dnsAddress.byte[0], netDev->device.dnsAddress.byte[1],
		netDev->device.dnsAddress.byte[2], netDev->device.dnsAddress.byte[3],
		filter.remotePort);

	connection = kernelNetworkConnectionOpen(netDev, NETWORK_MODE_READWRITE,
		(networkAddress *) &netDev->device.dnsAddress, &filter,
		1 /* with input stream */);
	if (!connection)
	{
		kernelError(kernel_error, "Couldn't connect to DNS server");
		return (connection);
	}

	kernelDebug(debug_net, "DNS querying server");

	status = kernelNetworkSendData(connection, query, queryLen,
		1 /* immediate */);
	if (status < 0)
		kernelError(kernel_error, "Couldn't send to DNS server");

	return (connection);
}


static int parseResource(networkDnsHeader *replyHeader, unsigned replyLen,
	unsigned *offset, unsigned short type)
{
	// Parse a resource record looking for the requested type, and add to
	// the lookup cache

	int status = 0;
	networkDnsName *dnsName = NULL;
	unsigned itemLen = 0;
	char *name = NULL;
	networkDnsResource *resource = NULL;
	unsigned dataLen = 0;
	networkAddress address;

	memset(&address, 0, sizeof(networkAddress));

	dnsName = ((void *) replyHeader + *offset);

	itemLen = dnsNameLen(dnsName, (replyLen - *offset));
	if (!itemLen || ((*offset + itemLen) > replyLen))
		return (status = ERR_BADDATA);

	status = readDnsName(replyHeader, replyLen, *offset, &name);
	if (status < 0)
		return (status);

	kernelDebug(debug_net, "DNS read %s from resource at %u", name, *offset);

	*offset += itemLen;

	if ((*offset + sizeof(networkDnsResource)) > replyLen)
	{
		kernelFree(name);
		return (status = ERR_BADDATA);
	}

	resource = ((void *) replyHeader + *offset);
	dataLen = ntohs(resource->dataLen);

	if ((*offset + sizeof(networkDnsResource) + dataLen) > replyLen)
	{
		kernelFree(name);
		return (status = ERR_BADDATA);
	}

	*offset += sizeof(networkDnsResource);

	// Is it the requested record type?
	if (ntohs(resource->type) != type)
	{
		kernelDebug(debug_net, "DNS not the requested record type");
		*offset += dataLen;
		kernelFree(name);
		return (status = 0);
	}

	// Add it to our lookup cache

	switch (type)
	{
		case NETWORK_DNSRECTYPE_HOST_A:
			memcpy(&address, resource->data, min(dataLen,
				sizeof(networkAddress)));
			break;

		case NETWORK_DNSRECTYPE_REV_PTR:
			if (strstr(name, DNS_IN_ADDR_STR))
				*(strstr(name, DNS_IN_ADDR_STR)) = '\0';
			inet_pton(AF_INET, name, &address);
			kernelFree(name);
			status = readDnsName(replyHeader, replyLen, *offset, &name);
			if (status < 0)
			{
				*offset += dataLen;
				return (status);
			}
			break;

		default:
			// Ignore
			*offset += dataLen;
			kernelFree(name);
			return (status = 0);
	}

	// This is hard-coded to IPv4.  Need more to support IPv6 lookups.
	status = addLookup(name, &address, AF_INET, ntohl(resource->timeToLive));
	if (status < 0)
		kernelFree(name);

	*offset += dataLen;
	return (status);
}


static int parseReply(networkDnsHeader *replyHeader, unsigned replyLen,
	unsigned short type)
{
	// Parse a reply from the DNS server, and add anything we can to
	// our lookup list

	int status = 0;
	unsigned offset = 0;
	networkDnsName *dnsName = NULL;
	unsigned itemLen = 0;
	int count;

	kernelDebug(debug_net, "DNS parse server reply");

	// Check for errors
	if (ntohs(replyHeader->flags) & NETWORK_DNSFLAG_RETMASK)
	{
		// We don't treat this as an error; probably the server saying the
		// name doesn't exist, which is not a parsing error, and handled
		// elsewhere
		kernelDebug(debug_net, "DNS server reply error code");
		return (status = 0);
	}

	offset = sizeof(networkDnsHeader);

	// Skip the question section
	for (count = 0; (offset < replyLen) &&
		(count < ntohs(replyHeader->numQuestions)); count ++)
	{
		kernelDebug(debug_net, "DNS skip question %d", count);

		dnsName = ((void *) replyHeader + offset);

		itemLen = dnsNameLen(dnsName, (replyLen - offset));
		if (!itemLen)
			return (status = ERR_BADDATA);

		itemLen += sizeof(networkDnsQuesion);

		offset += itemLen;
	}

	// Search the answer section
	for (count = 0; (offset < replyLen) &&
		(count < ntohs(replyHeader->numAnswers)); count ++)
	{
		kernelDebug(debug_net, "DNS parse answer resource %d", count);

		status = parseResource(replyHeader, replyLen, &offset, type);
		if (status < 0)
			return (status);
	}

	// Search the authority section
	for (count = 0; (offset < replyLen) &&
		(count < ntohs(replyHeader->numAuthorities)); count ++)
	{
		kernelDebug(debug_net, "DNS parse authority resource %d", count);

		status = parseResource(replyHeader, replyLen, &offset, type);
		if (status < 0)
			return (status);
	}

	// Search the additional section
	for (count = 0; (offset < replyLen) &&
		(count < ntohs(replyHeader->numAdditional)); count ++)
	{
		kernelDebug(debug_net, "DNS parse additional resource %d", count);

		status = parseResource(replyHeader, replyLen, &offset, type);
		if (status < 0)
			return (status);
	}

	return (status = 0);
}


static int waitDnsReply(kernelNetworkConnection *connection,
	unsigned short transId, unsigned short type)
{
	// Wait for a DNS reply to appear in our input queue

	int status = 0;
	uquad_t timeout = (kernelCpuGetMs() + 5000);
	unsigned char *reply = NULL;
	networkDnsHeader *replyHeader = NULL;
	int bytes = 0;

	kernelDebug(debug_net, "DNS wait for server reply");

	// Get a buffer big enough to hold any reply
	reply = kernelMalloc(NETWORK_PACKET_MAX_LENGTH);
	if (!reply)
		return (status = ERR_MEMORY);

	replyHeader = (networkDnsHeader *) reply;

	// Time out after ~5 seconds
	while (kernelCpuGetMs() <= timeout)
	{
		kernelMultitaskerYield();

		bytes = kernelNetworkRead(connection, reply,
			NETWORK_PACKET_MAX_LENGTH);

		if (bytes <= 0)
			continue;

		kernelDebug(debug_net, "DNS got server reply");

		if (ntohs(replyHeader->transId) != transId)
		{
			kernelDebug(debug_net, "DNS reply wrong trans ID");
			continue;
		}

		parseReply(replyHeader, bytes, type);

		kernelFree(reply);
		return (status = 0);
	}

	kernelFree(reply);

	// No response from the server
	kernelDebugError("DNS timeout");
	return (status = ERR_NODATA);
}


static dnsLookup *findLookupByAddress(const networkAddress *address)
{
	// Search through our list of lookups for one with a matching address

	dnsLookup *lookup = NULL;
	linkedListItem *iter = NULL;
	unsigned currentSecond = 0;

	kernelDebug(debug_net, "DNS search lookup cache for address %d.%d.%d.%d",
		address->byte[0], address->byte[1], address->byte[2],
		address->byte[3]);

	currentSecond = kernelRtcUptimeSeconds();

	lookup = linkedListIterStart(&lookups, &iter);

	while (lookup)
	{
		// Has this entry expired?  N.B. '>' so that replies with a 0 (no
		// cache) time to live will be cached for between 1-2 seconds - long
		// enough for the current lookup (and roughly concurrent ones) to
		// succeed.
		if (currentSecond > lookup->expiry)
		{
			kernelDebug(debug_net, "DNS cached lookup %s expired",
				lookup->name);
			removeLookup(lookup);
		}
		else
		{
			if (!memcmp((networkAddress *) address, &lookup->address,
				sizeof(networkAddress)))
			{
				kernelDebug(debug_net, "DNS returning cached result");
				lookup->accessed = currentSecond;
				return (lookup);
			}
		}

		lookup = linkedListIterNext(&lookups, &iter);
	}

	// Not found
	return (lookup = NULL);
}


static int makeAddressQuery(const networkAddress *address,
	unsigned char **query, unsigned *queryLen)
{
	// Compose an address (reverse lookup) query

	int status = 0;
	char name[sizeof(DNS_IN_ADDR_FORMAT) + 4];
	int nameLen = 0;
	networkDnsName *dnsName = NULL;
	networkDnsHeader *queryHeader = NULL;
	networkDnsQuesion *question = NULL;

	// We need to transform the address into a name string in the format
	// (A0).(A1).(A2).(A3).in-addr.arpa
	sprintf(name, DNS_IN_ADDR_FORMAT, address->byte[0], address->byte[1],
		address->byte[2], address->byte[3]);

	kernelDebug(debug_net, "DNS make address query for %s", name);

	nameLen = writeDnsName(name, &dnsName);
	if (nameLen < 0)
	{
		kernelError(kernel_error, "Couldn't compose DNS name");
		return (status = ERR_MEMORY);
	}

	// Get memory for our query packet

	*queryLen = (sizeof(networkDnsHeader) + nameLen +
		sizeof(networkDnsQuesion));

	*query = kernelMalloc(*queryLen);
	if (!*query)
	{
		*queryLen = 0;
		kernelFree(dnsName);
		return (status = ERR_MEMORY);
	}

	// Set up the query header
	queryHeader = (networkDnsHeader *) *query;
	queryHeader->transId = htons(kernelRandomFormatted(0, 0xFFFF));
	queryHeader->flags = htons(NETWORK_DNSFLAG_RECURSE_DESIRED);
	queryHeader->numQuestions = htons(1);

	// Copy the name to the question section
	memcpy((*query + sizeof(networkDnsHeader)), dnsName, nameLen);
	kernelFree(dnsName);

	// Set up the rest of the question section
	question = (networkDnsQuesion *)(*query + sizeof(networkDnsHeader) +
		nameLen);
	question->type = htons(NETWORK_DNSRECTYPE_REV_PTR);
	question->classCode = htons(0x0001); /* Internet */

	return (status = 0);
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
//  Below here, the functions are exported for internal use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////

int kernelNetworkDnsQueryNameAddress(kernelNetworkDevice *netDev,
	const char *name, networkAddress *address, int *addressType)
{
	// This is the simplest form of DNS query, which attempts to resolve a
	// name string to an address

	int status = 0;
	dnsLookup *lookup = NULL;
	unsigned char *query = NULL;
	unsigned queryLen = 0;
	kernelNetworkConnection *connection = NULL;

	// Check params.  addressType may be NULL.
	if (!netDev || !name || !address)
	{
		kernelError(kernel_error, "NULL parameter");
		return (status = ERR_NULLPARAMETER);
	}

	// Make sure our lookup cache has been initialized
	checkInit();

	kernelDebug(debug_net, "DNS query address of %s on %s", name,
		netDev->device.name);

	// Check for a cached lookup
	lookup = findLookupByName(name);
	if (lookup)
	{
		// Found a cached one
		memcpy(address, &lookup->address, sizeof(networkAddress));
		if (addressType)
			*addressType = lookup->type;
		return (status = 0);
	}

	// Compose our query
	status = makeNameQuery(name, &query, &queryLen);
	if (status < 0)
		goto out;

	// Send the query
	connection = sendQuery(netDev, query, queryLen);
	if (!connection)
	{
		status = ERR_NOCONNECTION;
		goto out;
	}

	// Wait for the reply
	status = waitDnsReply(connection,
		ntohs(((networkDnsHeader *) query)->transId),
		NETWORK_DNSRECTYPE_HOST_A);

	kernelNetworkClose(connection);

	if (status < 0)
	{
		kernelError(kernel_error, "No/bad reply from DNS server");
		goto out;
	}

	// Now there should be a cached lookup
	lookup = findLookupByName(name);
	if (!lookup)
	{
		kernelDebug(debug_net, "DNS lookup failed");
		status = ERR_HOSTUNKNOWN;
		goto out;
	}

	// Got it
	kernelDebug(debug_net, "DNS lookup success");
	memcpy(address, &lookup->address, sizeof(networkAddress));
	if (addressType)
		*addressType = lookup->type;

	if (lookup->type == AF_INET)
	{
		kernelDebug(debug_net, "DNS %s resolved to %d.%d.%d.%d", name,
			address->byte[0], address->byte[1], address->byte[2],
			address->byte[3]);
	}

	status = 0;

out:
	if (query)
		kernelFree(query);

	return (status);
}


int kernelNetworkDnsQueryAddressName(kernelNetworkDevice *netDev,
	const networkAddress *address, char *name, unsigned nameLen)
{
	// This performs a reverse-lookup DNS query, which attempts to resolve an
	// address to a name string

	int status = 0;
	dnsLookup *lookup = NULL;
	unsigned char *query = NULL;
	unsigned queryLen = 0;
	kernelNetworkConnection *connection = NULL;

	// Check params
	if (!netDev || !address || !name || !nameLen)
	{
		kernelError(kernel_error, "NULL parameter");
		return (status = ERR_NULLPARAMETER);
	}

	// Make sure our lookup cache has been initialized
	checkInit();

	kernelDebug(debug_net, "DNS query name of %d.%d.%d.%d on %s",
		address->byte[0], address->byte[1], address->byte[2],
		address->byte[3], netDev->device.name);

	// Check for a cached lookup
	lookup = findLookupByAddress(address);
	if (lookup)
	{
		// Found a cached one
		strncpy(name, lookup->name, nameLen);
		return (status = 0);
	}

	// Compose our query
	status = makeAddressQuery(address, &query, &queryLen);
	if (status < 0)
		goto out;

	// Send the query
	connection = sendQuery(netDev, query, queryLen);
	if (!connection)
	{
		status = ERR_NOCONNECTION;
		goto out;
	}

	// Wait for the reply
	status = waitDnsReply(connection,
		ntohs(((networkDnsHeader *) query)->transId),
		NETWORK_DNSRECTYPE_REV_PTR);

	kernelNetworkClose(connection);

	if (status < 0)
	{
		kernelError(kernel_error, "No/bad reply from DNS server");
		goto out;
	}

	// Now there should be a cached lookup
	lookup = findLookupByAddress(address);
	if (!lookup)
	{
		kernelDebug(debug_net, "DNS lookup failed");
		status = ERR_HOSTUNKNOWN;
		goto out;
	}

	// Got it
	kernelDebug(debug_net, "DNS lookup success");
	strncpy(name, lookup->name, nameLen);

	if (lookup->type == AF_INET)
	{
		kernelDebug(debug_net, "DNS %d.%d.%d.%d resolved to %s",
			address->byte[0], address->byte[1], address->byte[2],
			address->byte[3], name);
	}

	status = 0;

out:
	if (query)
		kernelFree(query);

	return (status);
}

