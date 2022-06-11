//
//  Visopsys
//  Copyright (C) 1998-2021 J. Andrew McLaughlin
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
//  getaddrinfo.c
//

// This is the standard "getaddrinfo" function, as found in standard C
// libraries

#include <netdb.h>
#include <errno.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/api.h>
#include <sys/socket.h>


static int isNumeric(int ai_family, const char *string,
	networkAddress *address)
{
	if ((ai_family == AF_INET) || (ai_family == AF_UNSPEC))
	{
		// Try IPv4
		if (inet_pton(AF_INET, string, address) == 1)
			return (AF_INET);
	}

	if ((ai_family == AF_INET6) || (ai_family == AF_UNSPEC))
	{
		// Try IPv6
		if (inet_pton(AF_INET6, string, address) == 1)
			return (AF_INET6);
	}

	return (0);
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
//  Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////

int getaddrinfo(const char *node, const char *service,
	const struct addrinfo *hints, struct addrinfo **res)
{
	// This implementation is partial.  Additional functionality will be
	// added as needed.

	int status = 0;
	int family = 0;
	networkAddress address;
	struct protoent *proto = NULL;
	struct servent *serv = NULL;
	struct servent servPortOnly;
	unsigned short port = 0;
	struct sockaddr_in *sin = NULL;
	struct sockaddr_in6 *sin6 = NULL;

	// Check params.  Either 'node' or 'service' must be non-NULL.  'hints'
	// may be NULL.

	if (!node && !service)
		return (status = EAI_NONAME);

	if (!res)
	{
		errno = ERR_NULLPARAMETER;
		return (status = EAI_SYSTEM);
	}

	memset(&address, 0, sizeof(networkAddress));
	memset(&servPortOnly, 0, sizeof(struct servent));

	*res = calloc(1, sizeof(struct addrinfo));
	if (!*res)
		return (status = EAI_MEMORY);

	if (node)
	{
		// Is the caller telling us that 'node' is a numeric address string?
		if (hints && (hints->ai_flags & AI_NUMERICHOST))
		{
			family = isNumeric(hints->ai_family, node, &address);
			if (!family)
			{
				status = EAI_NONAME;
				goto out;
			}
		}

		// If not, do we think it is numeric?
		if (!family)
		{
			family = isNumeric((hints? hints->ai_family : AF_UNSPEC),
				node, &address);
		}

		if (!family)
		{
			// We think we need to look up a host name
			status = networkLookupNameAddress(node, &address, &family);
			if (status < 0)
			{
				status = EAI_FAIL;
				goto out;
			}
		}
	}
	else
	{
		if (hints && (hints->ai_flags & AI_PASSIVE))
		{
			// For bind()ing - use the wildcard address
			if (hints && (hints->ai_family == AF_INET6))
			{
				family = AF_INET6;
				memcpy(&address, &IN6ADDR_ANY_INIT, NETWORK_ADDRLENGTH_IP6);
			}
			else
			{
				family = AF_INET;
				address.dword[0] = INADDR_ANY;
			}
		}
		else
		{
			// For connect()ing - use the loopback address
			if (hints && (hints->ai_family == AF_INET6))
			{
				family = AF_INET6;
				memcpy(&address, &IN6ADDR_LOOPBACK_INIT,
					NETWORK_ADDRLENGTH_IP6);
			}
			else
			{
				family = AF_INET;
				address.dword[0] = INADDR_LOOPBACK;
			}
		}
	}

	if (service)
	{
		if (hints && hints->ai_protocol)
		{
			proto = getprotobynumber(hints->ai_protocol);
			if (!proto)
			{
				status = EAI_SERVICE;
				goto out;
			}
		}

		// Did the caller pass a service name?
		serv = getservbyname(service, (proto? proto->p_name : NULL));
		if (!serv)
		{
			// Try to parse it as a number
			serv = getservbyport(atoi(service), (proto? proto->p_name :
				NULL));
			if (!serv)
			{
				serv = &servPortOnly;
				serv->s_port = atoi(service);
			}
		}

		// We got a port matched to the name (and protocol, if specified)
		port = serv->s_port;
	}

	(*res)->ai_family = family;

	// Determine the length of the sockaddr structure
	switch (family)
	{
		case AF_INET:
			(*res)->ai_addrlen = sizeof(struct sockaddr_in);
			break;

		case AF_INET6:
			(*res)->ai_addrlen = sizeof(struct sockaddr_in6);
			break;

		default:
			status = EAI_FAMILY;
			goto out;
	}

	// Get memory for the sockaddr structure
	(*res)->ai_addr = calloc(1, (*res)->ai_addrlen);
	if (!(*res)->ai_addr)
	{
		status = EAI_MEMORY;
		goto out;
	}

	// Set up the sockaddr structure

	(*res)->ai_addr->sa_family = family;

	switch (family)
	{
		case AF_INET:
			sin = (struct sockaddr_in *) (*res)->ai_addr;
			sin->sin_port = htons(port);
			sin->sin_addr.s_addr = address.dword[0];
			break;

		case AF_INET6:
			sin6 = (struct sockaddr_in6 *) (*res)->ai_addr;
			sin6->sin6_port = htons(port);
			memcpy(&sin6->sin6_addr, &address, NETWORK_ADDRLENGTH_IP6);
			break;
	}

	// Success
	status = 0;

out:
	if (status < 0)
	{
		freeaddrinfo(*res);
		*res = NULL;
	}

	return (status);
}

