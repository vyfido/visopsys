//
//  Visopsys
//  Copyright (C) 1998-2020 J. Andrew McLaughlin
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
//  netdb.h
//

// This is the Visopsys version of the standard C library header file
// <netdb.h>

#ifndef _NETDB_H
#define _NETDB_H

// Error codes
#include <sys/errors.h>

// Contains the socklen_t and sockaddr definitions
#include <sys/socket.h>

// Supported ai_flags for getaddrinfo() hints are non-zero
#define AI_ADDRCONFIG			0x0000
#define AI_ALL					0x0000
#define AI_V4MAPPED				0x0000
#define AI_NUMERICHOST			0x0004  // no name resolution - node is addr
#define AI_CANONNAME			0x0000
#define AI_PASSIVE				0x0001	// no node - fill in addr

// getaddrinfo() errror codes, mapped roughly to system error codes
#define EAI_ADDRFAMILY			ERR_DOMAIN
#define EAI_AGAIN				ERR_BUSY
#define EAI_BADFLAGS			ERR_BADDATA
#define EAI_FAIL				ERR_IO
#define EAI_FAMILY				ERR_NOTIMPLEMENTED
#define EAI_MEMORY				ERR_MEMORY
#define EAI_NODATA				ERR_NODATA
#define EAI_NONAME				ERR_HOSTUNKNOWN
#define EAI_SERVICE				ERR_NOSUCHENTRY
#define EAI_SOCKTYPE			ERR_INVALID
#define EAI_SYSTEM				ERR_ERROR

struct addrinfo {
	int ai_flags;				// additional options
	int ai_family;				// address family (e.g. AF_INET, AF_UNSPEC)
	int ai_socktype;			// socket type (e.g. SOCK_STREAM, SOCK_DGRAM)
	int ai_protocol;			// protocol (0 for any)
	socklen_t ai_addrlen;		// length of ai_addr
	struct sockaddr *ai_addr;	// address
	char *ai_canonname;			// full canonical hostname
	struct addrinfo *ai_next;	// next in the linked list
};

struct protoent {
	char *p_name;				// official protocol name
	char **p_aliases;			// alias list
	int p_proto;				// protocol number
};

struct servent {
	char *s_name;				// official service name
	char **s_aliases;			// alias list
	int s_port;					// port number
	char *s_proto;				// protocol to use
};

void freeaddrinfo(struct addrinfo *);
const char *gai_strerror(int);
int getaddrinfo(const char *, const char *, const struct addrinfo *,
	struct addrinfo **);
struct protoent *getprotobyname(const char *);
struct protoent *getprotobynumber(int);
struct servent *getservbyname(const char *, const char *);
struct servent *getservbyport(int, const char *);

#endif

