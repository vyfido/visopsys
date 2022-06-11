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
//  libhttp.c
//

// This is the library for using the HTTP protocol

#include "libhttp.h"
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/api.h>
#include <sys/http.h>
#include <sys/network.h>
#include <sys/types.h>
#include <sys/url.h>
#include <sys/vis.h>

// Supported URL schemes
struct {
	const char *name;
	urlScheme scheme;
	int defaultPort;

} urlSchemes[] = {
	{ URL_SCHEME_NAME_FILE, scheme_file, 0 /* no default port */ },
	{ URL_SCHEME_NAME_FTP, scheme_ftp, NETWORK_PORT_FTP },
	{ URL_SCHEME_NAME_HTTP, scheme_http, NETWORK_PORT_HTTP },
	{ URL_SCHEME_NAME_HTTPS, scheme_https, NETWORK_PORT_HTTPS },
	{ NULL, scheme_unknown, 0 }
};

// Unsupported URL schemes we want to recognize
const char *unsuppSchemes[] = {
	URL_SCHEME_NAME_ABOUT,
	URL_SCHEME_NAME_AFS,
	URL_SCHEME_NAME_DATA,
	URL_SCHEME_NAME_DNS,
	URL_SCHEME_NAME_FACETIME,
	URL_SCHEME_NAME_FAX,
	URL_SCHEME_NAME_GOPHER,
	URL_SCHEME_NAME_IM,
	URL_SCHEME_NAME_IMAP,
	URL_SCHEME_NAME_IRC,
	URL_SCHEME_NAME_JMS,
	URL_SCHEME_NAME_LDAP,
	URL_SCHEME_NAME_MAILSERVER,
	URL_SCHEME_NAME_MAILTO,
	URL_SCHEME_NAME_NEWS,
	URL_SCHEME_NAME_NFS,
	URL_SCHEME_NAME_NNTP,
	URL_SCHEME_NAME_PKCS11,
	URL_SCHEME_NAME_POP,
	URL_SCHEME_NAME_RSYNC,
	URL_SCHEME_NAME_SHTTP,
	URL_SCHEME_NAME_SKYPE,
	URL_SCHEME_NAME_SMS,
	URL_SCHEME_NAME_SNMP,
	URL_SCHEME_NAME_TEL,
	URL_SCHEME_NAME_TELNET,
	URL_SCHEME_NAME_TFTP,
	URL_SCHEME_NAME_VNC,
	NULL
};

#ifdef DEBUG
	#define DEBUG_OUTMAX	160
	int debugLibHttp = 0;
	static char debugOutput[DEBUG_OUTMAX];

	static void DEBUGMSG(const char *message, ...)
	{
		va_list list;

		if (debugLibHttp)
		{
			va_start(list, message);
			vsnprintf(debugOutput, DEBUG_OUTMAX, message, list);
			printf("%s", debugOutput);
		}
	}
#else
	#define DEBUGMSG(message, arg...) do { } while (0)
#endif


static urlScheme parseUrlScheme(char *string, char **sep, int *defaultPort)
{
	int count;

	if (!(*sep = strchr(string, ':')))
		return (scheme_unknown);

	*(*sep) = '\0';

	DEBUGMSG("Parse scheme component '%s'\n", string);

	// Look for supported schemes
	for (count = 0; urlSchemes[count].name; count ++)
	{
		if (!strcasecmp(string, urlSchemes[count].name))
		{
			*defaultPort = urlSchemes[count].defaultPort;
			return (urlSchemes[count].scheme);
		}
	}

	// Look for unsupported schemes
	for (count = 0; unsuppSchemes[count]; count ++)
	{
		if (!strcasecmp(string, unsuppSchemes[count]))
			return (scheme_unsupported);
	}

	// Not found
	return (scheme_unknown);
}


static objectKey openConnection(urlInfo *url)
{
	objectKey connection = NULL;
	networkAddress address;
	int addressType = 0;
	char addrString[INET6_ADDRSTRLEN + 1];
	networkFilter filter;

	DEBUGMSG("Looking up host '%s'\n", url->host);

	// Look up the host
	if (networkLookupNameAddress(url->host, &address, &addressType) < 0)
	{
		fprintf(stderr, "Host lookup failure: '%s'\n", url->host);
		return (connection = NULL);
	}

	// Turn the address into a printable string
	memset(addrString, 0, sizeof(addrString));
	inet_ntop(addressType, &address, addrString, sizeof(addrString));

	DEBUGMSG("Contacting host %s:%d\n", addrString, url->port);

	// Set up the filter
	memset(&filter, 0, sizeof(networkFilter));
	filter.flags |= NETWORK_FILTERFLAG_NETPROTOCOL;
	filter.netProtocol = NETWORK_NETPROTOCOL_IP4;
	filter.flags |= NETWORK_FILTERFLAG_TRANSPROTOCOL;
	filter.transProtocol = NETWORK_TRANSPROTOCOL_TCP;
	filter.flags |= NETWORK_FILTERFLAG_REMOTEPORT;
	filter.remotePort = url->port;

	connection = networkOpen(NETWORK_MODE_READWRITE, &address, &filter);
	if (!connection)
	{
		fprintf(stderr, "Couldn't connect to %s\n", addrString);
		return (connection);
	}

	DEBUGMSG("Connected to %s\n", addrString);

	return (connection);
}


static inline int writeLine(objectKey connection, const char *line)
{
	DEBUGMSG("%s", line);

	return (networkWrite(connection, (unsigned char *) line, strlen(line)));
}


static int writeRequestGet(objectKey connection, urlInfo *url)
{
	int status = 0;
	char lineBuffer[256];

	// Compose the request

	DEBUGMSG("Sending request:\n");

	snprintf(lineBuffer, sizeof(lineBuffer), "%s /%s HTTP/1.1\r\n",
		HTTP_REQUEST_GET, (url->path? url->path : ""));

	status = writeLine(connection, lineBuffer);
	if (status < 0)
		goto out;

	snprintf(lineBuffer, sizeof(lineBuffer), "%s: %s:%d\r\n",
		HTTP_HEADER_HOST, url->host, url->port);

	// Add an empty last line
	strncat(lineBuffer, "\r\n", (sizeof(lineBuffer) - strlen(lineBuffer)));

	status = writeLine(connection, lineBuffer);
	if (status < 0)
		goto out;

	status = 0;

out:
	if (status < 0)
		fprintf(stderr, "Error sending request\n");

	return (status);
}


static int readHeader(objectKey connection, variableList *header,
	unsigned timeout)
{
	int status = 0;
	char lineBuffer[256];
	int bufferBytes = 0;
	char *sep = NULL;
	int lineLen = 0;
	int statusLine = 0;
	char *tmp = NULL;

	DEBUGMSG("Receiving response:\n");

	while (!timeout || (cpuGetMs() < timeout))
	{
		// Read the header one byte at a time, so we can process complete
		// lines, and stop when we reach the empty line marking the end of the
		// header

		if (bufferBytes >= (int)(sizeof(lineBuffer) - 1))
		{
			fprintf(stderr, "Buffer full, no EOL detected\n");
			status = ERR_BADDATA;
			break;
		}

		status = networkRead(connection, (unsigned char *)(lineBuffer +
			bufferBytes), 1);
		if (status < 0)
		{
			fprintf(stderr, "Error receiving response header\n");
			break;
		}

		if (!status)
			continue;

		bufferBytes += 1;
		lineBuffer[bufferBytes] = '\0';

		while (bufferBytes && (sep = strstr(lineBuffer, "\r\n")))
		{
			lineLen = (sep - lineBuffer);

			*sep = '\0';

			DEBUGMSG("%s\n", lineBuffer);

			if (!lineLen)
			{
				// Empty line
				status = 0;
				goto out;
			}

			if (!statusLine)
			{
				sep = strstr(lineBuffer, " ");
				if (!sep)
				{
					fprintf(stderr, "Syntax error in line '%s'\n",
						lineBuffer);
					status = ERR_BADDATA;
					goto out;
				}

				*sep = '\0';

				status = variableListSet(header, HTTP_VERSION_VAR,
					lineBuffer);
				if (status < 0)
				{
					fprintf(stderr, "Error saving line\n");
					goto out;
				}

				tmp = (sep + 1);

				sep = strstr(tmp, " ");
				if (!sep)
				{
					fprintf(stderr, "Syntax error in status line '%s'\n",
						tmp);
					status = ERR_BADDATA;
					goto out;
				}

				*sep = '\0';

				status = variableListSet(header, HTTP_STATUSCODE_VAR, tmp);
				if (status < 0)
				{
					fprintf(stderr, "Error saving line\n");
					goto out;
				}

				tmp = (sep + 1);

				if (strlen(tmp))
				{
					status = variableListSet(header, HTTP_STATUSSTRING_VAR,
						tmp);
					if (status < 0)
					{
						fprintf(stderr, "Error saving line\n");
						goto out;
					}
				}

				statusLine = 1;
			}
			else
			{
				sep = strstr(lineBuffer, ": ");
				if (!sep)
				{
					fprintf(stderr, "Syntax error in line '%s'\n",
						lineBuffer);
					status = ERR_BADDATA;
					goto out;
				}

				*sep = '\0';

				status = variableListSet(header, lineBuffer, (sep + 2));
				if (status < 0)
				{
					fprintf(stderr, "Error saving line\n");
					goto out;
				}
			}

			lineLen += 2;

			memmove(lineBuffer, (lineBuffer + lineLen), (bufferBytes -
				lineLen));

			bufferBytes -= lineLen;
		}
	}

out:
	return (status);
}


static int checkStatus(variableList *header)
{
	int status = 0;
	const char *statusCode = NULL;
	const char *statusString = NULL;
	int statusVal = 0;

	statusCode = variableListGet(header, HTTP_STATUSCODE_VAR);
	if (!statusCode)
	{
		fprintf(stderr, "No status code\n");
		return (status = ERR_BADDATA);
	}

	statusString = variableListGet(header, HTTP_STATUSSTRING_VAR);
	if (!statusString)
		statusString = "Unknown";

	statusVal = atoi(statusCode);

	switch (statusVal / 100)
	{
		case 2: // 2xx Success
		{
			DEBUGMSG("HTTP %d: %s\n", statusVal, statusString);
			return (status = 0);
		}

		// Everything else we currently consider to be an error
		default:
		{
			fprintf(stderr, "HTTP error %d: %s\n", statusVal, statusString);
			return (status = ERR_IO);
		}
	}
}


static int readContent(objectKey connection, char *content, int len,
	unsigned timeout)
{
	int status = 0;
	int read = 0;

	while ((read < len) && (!timeout || (cpuGetMs() < timeout)))
	{
		status = networkRead(connection, (unsigned char *)(content + read),
			(len - read));
		if (status < 0)
		{
			fprintf(stderr, "Error receiving content\n");
			break;
		}

		read += status;
		status = 0;
	}

	return (status);
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
//  Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////

int httpParseUrl(const char *string, urlInfo **url)
{
	// Given a URL string, parse it into its component parts for use with this
	// library
	//
	// scheme:[//[user[:password]@]host[:port]][/path][?query][#fragment]

	int status = 0;
	char *sep = NULL;
	char *ptr = NULL;
	int haveHost = 0, havePath = 0, haveQuery = 0, haveFragment = 0;
	int haveUser = 0, havePassword = 0, havePort = 0;
	char *tmp = NULL;

	// Check params
	if (!string || !url)
	{
		fprintf(stderr, "NULL parameter\n");
		status = ERR_NULLPARAMETER;
		goto out;
	}

	DEBUGMSG("Parse URL '%s'\n", string);

	*url = calloc(1, sizeof(urlInfo));
	if (!*url)
	{
		fprintf(stderr, "Memory error\n");
		status = ERR_MEMORY;
		goto out;
	}

	// Copy the string
	(*url)->data = strdup(string);
	if (!(*url)->data)
	{
		fprintf(stderr, "Memory error\n");
		status = ERR_MEMORY;
		goto out;
	}

	// Identify the scheme
	(*url)->scheme = parseUrlScheme((*url)->data, &sep, &(*url)->port);
	if ((*url)->scheme == scheme_unknown)
	{
		// Try HTTP

		DEBUGMSG("Unknown scheme - trying HTTP\n");

		free((*url)->data);

		(*url)->data = malloc(strlen(string) + 8);
		if (!(*url)->data)
		{
			fprintf(stderr, "Memory error\n");
			status = ERR_MEMORY;
			goto out;
		}

		sprintf((*url)->data, "%s://%s", URL_SCHEME_NAME_HTTP, string);

		DEBUGMSG("Trying %s\n", (*url)->data);

		(*url)->scheme = parseUrlScheme((*url)->data, &sep, &(*url)->port);
		if ((*url)->scheme == scheme_unknown)
		{
			fprintf(stderr, "Unknown scheme\n");
			status = ERR_INVALID;
			goto out;
		}
	}
	else if ((*url)->scheme == scheme_unsupported)
	{
		fprintf(stderr, "Unsupported scheme '%s'\n", (*url)->data);
		status = ERR_NOTIMPLEMENTED;
		goto out;
	}

	DEBUGMSG("URL scheme: %s\n", httpUrlSchemeToString((*url)->scheme));

	ptr = (sep + 1);
	if (!strlen(ptr))
	{
		status = 0;
		goto out;
	}

	if ((*url)->scheme == scheme_file)
	{
		// See whether there's a path component
		if (!strncmp(ptr, "///", 3))
		{
			havePath = 1;
			ptr += 3;
		}

		tmp = ptr;
	}
	else
	{
		// See whether there's a user/password/host/port component
		if (!strncmp(ptr, "//", 2))
		{
			haveHost = 1;
			ptr += 2;
		}

		tmp = ptr;

		// See whether there's a path component
		if (strlen(tmp) && (sep = strchr(tmp, '/')))
		{
			havePath = 1;
			*sep = '\0';
			tmp = (sep + 1);
		}
	}

	// See whether there's a query component
	if (strlen(tmp) && (sep = strchr(tmp, '?')))
	{
		haveQuery = 1;
		*sep = '\0';
		tmp = (sep + 1);
	}

	// See whether there's a fragment component
	if (strlen(tmp) && (sep = strchr(tmp, '#')))
	{
		haveFragment = 1;
		*sep = '\0';
	}

	if (haveHost)
	{
		// [user[:password]@]host[:port]

		DEBUGMSG("Parse authority component '%s'\n", ptr);

		tmp = ptr;

		// See whether there are user/password subcomponents
		if (strlen(tmp) && (sep = strchr(tmp, '@')))
		{
			haveUser = 1;
			*sep = '\0';

			// See whether there's a password subcomponent
			if (strlen(tmp) && (sep = strchr(tmp, ':')))
			{
				havePassword = 1;
				*sep = '\0';
				tmp = (sep + 1);
			}

			tmp += (strlen(tmp) + 1);
		}

		// See whether there's a port subcomponent
		if (strlen(tmp) && (sep = strchr(tmp, ':')))
		{
			havePort = 1;
			*sep = '\0';
		}

		if (haveUser)
		{
			DEBUGMSG("Parse user subcomponent '%s'\n", ptr);
			(*url)->user = ptr;
			ptr += (strlen(ptr) + 1);
		}

		if (havePassword)
		{
			DEBUGMSG("Parse password subcomponent '%s'\n", ptr);
			(*url)->password = ptr;
			ptr += (strlen(ptr) + 1);
		}

		DEBUGMSG("Parse host subcomponent '%s'\n", ptr);
		(*url)->host = ptr;
		ptr += (strlen(ptr) + 1);

		if (havePort)
		{
			DEBUGMSG("Parse port subcomponent '%s'\n", ptr);
			(*url)->port = atoi(ptr);
			DEBUGMSG("Host port=%d\n", (*url)->port);
			ptr += (strlen(ptr) + 1);
		}
	}

	if (havePath)
	{
		DEBUGMSG("Parse path component '%s'\n", ptr);
		(*url)->path = ptr;
		ptr += (strlen(ptr) + 1);
	}
	else
	{
		(*url)->path = "index.html";
	}

	if (haveQuery)
	{
		DEBUGMSG("Parse query component '%s'\n", ptr);
		(*url)->query = ptr;
		ptr += (strlen(ptr) + 1);
	}

	if (haveFragment)
	{
		DEBUGMSG("Parse fragment component '%s'\n", ptr);
		(*url)->fragment = ptr;
	}

	status = 0;

out:
	if (status < 0)
	{
		if (url && *url)
		{
			httpFreeUrlInfo(*url);
			*url = NULL;
		}

		errno = status;
	}

	return (status);
}


void httpFreeUrlInfo(urlInfo *url)
{
	if (url)
	{
		if (url->data)
			free(url->data);

		free(url);
	}
}


const char *httpUrlSchemeToString(urlScheme scheme)
{
	int count;

	for (count = 0; urlSchemes[count].name; count ++)
	{
		if (scheme == urlSchemes[count].scheme)
			return (urlSchemes[count].name);
	}

	// Not found
	return (NULL);
}


int httpGet(urlInfo *url, variableList *header, char **content, int *len,
	unsigned timeoutMs)
{
	int status = 0;
	uquad_t timeout = 0;
	objectKey connection = NULL;

	// Check params
	if (!url || !header || !content || !len)
	{
		fprintf(stderr, "NULL parameter\n");
		return (status = ERR_NULLPARAMETER);
	}

	if (timeoutMs)
		timeout = (cpuGetMs() + timeoutMs);

	connection = openConnection(url);
	if (!connection)
	{
		status = ERR_NOCONNECTION;
		goto out;
	}

	status = writeRequestGet(connection, url);
	if (status < 0)
		goto out;

	status = variableListCreate(header);
	if (status < 0)
	{
		fprintf(stderr, "Error allocating response header\n");
		goto out;
	}

	status = readHeader(connection, header, timeout);
	if (status < 0)
		goto out;

	status = checkStatus(header);
	if (status < 0)
		goto out;

	// Get the content length
	if (!variableListGet(header, HTTP_HEADER_CONTENTLENGTH))
	{
		fprintf(stderr, "No content length\n");
		goto out;
	}

	*len = atoi(variableListGet(header, HTTP_HEADER_CONTENTLENGTH));

	DEBUGMSG("Content length = %d\n", *len);

	if (!*len)
	{
		status = ERR_NODATA;
		goto out;
	}

	// Allocate the buffer
	*content = malloc(*len);
	if (!*content)
	{
		fprintf(stderr, "Memory error\n");
		status = ERR_MEMORY;
		goto out;
	}

	status = readContent(connection, *content, *len, timeout);
	if (status < 0)
	{
		free(*content) ; *content = NULL; *len = 0;
		goto out;
	}

	status = 0;

out:
	if (connection)
		networkClose(connection);

	return (status);
}

