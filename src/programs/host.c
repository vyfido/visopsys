//
//  Visopsys
//  Copyright (C) 1998-2021 J. Andrew McLaughlin
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
//  host.c
//

// This is the UNIX-style command for looking up network addresses

/* This is the text that appears when a user requests help about this program
<help>

 -- host --

Look up network names and addresses.

Usage:
  host <address | hostname>

The 'host' command queries network services (e.g. DNS) to resolve a name to
an address, or an address to a name.

</help>
*/

#include <errno.h>
#include <libintl.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/api.h>
#include <sys/env.h>
#include <sys/network.h>
#include <sys/socket.h>

#define _(string) gettext(string)


static void usage(char *name)
{
	printf(_("usage:\n%s <address | hostname>\n"), name);
	return;
}


static int getAddress(char *string, networkAddress *address, int *type)
{
	int status = 0;

	// Try IPv4
	*type = AF_INET;
	status = inet_pton(*type, string, address);
	if (status == 1)
		return (status = 0);

	// Try IPv6
	*type = AF_INET6;
	status = inet_pton(*type, string, address);
	if (status == 1)
		return (status = 0);

	return (status = ERR_INVALID);
}


int main(int argc, char *argv[])
{
	int status = 0;
	char *arg = NULL;
	char *name = NULL;
	networkAddress address;
	int addressType = 0;
	int reverse = 0;
	char addrString[INET6_ADDRSTRLEN + 1];

	setlocale(LC_ALL, getenv(ENV_LANG));
	textdomain("host");

	if (argc != 2)
	{
		usage(argv[0]);
		return (status = ERR_ARGUMENTCOUNT);
	}

	// Make sure networking is enabled
	if (!networkEnabled())
	{
		fprintf(stderr, "%s", _("Networking is not currently enabled\n"));
		return (status = ERR_NOTINITIALIZED);
	}

	// For the moment, we only accept the one argument
	arg = argv[1];

	// Try to determine whether it's a name or an address
	memset(&address, 0, sizeof(networkAddress));
	status = getAddress(arg, &address, &addressType);
	if (status >= 0)
		reverse = 1;

	if (reverse)
	{
		// Do a reverse query

		name = calloc((MAXSTRINGLENGTH + 1), 1);
		if (!name)
		{
			fprintf(stderr, "%s", _("Memory allocation error\n"));
			return (status = ERR_MEMORY);
		}

		// Attempt the query
		status = networkLookupAddressName(&address, name, MAXSTRINGLENGTH);
		if (status < 0)
		{
			fprintf(stderr, _("Host %s not found\n"), arg);
			return (status);
		}

		printf(_("%s domain name pointer %s\n"), arg, name);
		free(name);
	}
	else
	{
		// Do a standard address query

		name = arg;

		// Attempt the query
		status = networkLookupNameAddress(name, &address, &addressType);
		if (status < 0)
		{
			fprintf(stderr, _("Host %s not found\n"), name);
			return (status);
		}

		// Turn the address into a printable string
		memset(addrString, 0, sizeof(addrString));
		inet_ntop(addressType, &address, addrString, sizeof(addrString));

		printf(_("%s has address %s\n"), name, addrString);
	}

	// Return success
	return (status = 0);
}

