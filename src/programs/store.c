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
//  store.c
//

// This is a program for running a software store, so that other Visopsys
// systems can connect and download, install, and update software

/* This is the text that appears when a user requests help about this program
<help>

 -- store --

Run a software store (server) for other Visopsys systems to connect and
download, install, and update software.

Usage:
  store

</help>
*/

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <libintl.h>
#include <netdb.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>

#ifndef PORTABLE
	#include <sys/store.h>
#endif

#define _(string)		gettext(string)
#define STR_HELPER(x)	#x
#define STR(x)			STR_HELPER(x)

typedef struct {
	int sockFd;
	struct sockaddr_storage addrStorage;

} connection;

static int srvSockFd = 0;
static int stop = 0;


static void interrupt(int sig)
{
	// This is our interrupt signal handler.
	if (sig == SIGINT)
	{
		printf("%s\n", _("Shutting down"));

		stop = 1;

		if (srvSockFd >= 0)
		{
			close(srvSockFd);
			srvSockFd = -1;
		}
	}
}


static unsigned char *receiveRequest(int sockFd)
{
	unsigned magic = 0;
	unsigned len = 0;
	unsigned char *request = NULL;
	ssize_t received = 0;
	ssize_t status = 0;
	storeRequestHeader *header = NULL;

	while (!stop)
	{
		// Get the magic number that should start any request
		status = recv(sockFd, &magic, sizeof(unsigned), MSG_DONTWAIT);
		if ((status == -1) && ((errno == EAGAIN) || (errno == EWOULDBLOCK)))
		{
			sched_yield();
			continue;
		}

		if (status < (ssize_t) sizeof(unsigned))
		{
			perror("recv");
			fprintf(stderr, "%s\n", _("I/O error"));
			return (request = NULL);
		}

		if (magic != STORE_HEADER_MAGIC)
		{
			fprintf(stderr, "%s\n", _("Invalid request"));
			return (request = NULL);
		}

		received += status;

		// Get the length of the request
		status = recv(sockFd, &len, sizeof(unsigned), MSG_DONTWAIT);
		if ((status == -1) && ((errno == EAGAIN) || (errno == EWOULDBLOCK)))
		{
			sched_yield();
			continue;
		}

		if (received < (ssize_t) sizeof(unsigned))
		{
			perror("recv");
			fprintf(stderr, "%s\n", _("I/O error"));
			return (request = NULL);
		}

		if (len > STORE_REQUEST_MAX)
		{
			fprintf(stderr, "%s\n", _("Invalid request"));
			return (request = NULL);
		}

		received += status;

		break;
	}

	printf("%s %u\n", _("Request length"), len);

	request = calloc(len, 1);
	if (!request)
	{
		perror("calloc");
		fprintf(stderr, "%s\n", _("Memory error"));
		return (request);
	}

	header = (storeRequestHeader *) request;
	header->magic = magic;
	header->len = len;

	while (!stop && (received < (ssize_t) len))
	{
		status = recv(sockFd, (request + received), (len - received),
			MSG_DONTWAIT);
		if ((status == -1) && ((errno == EAGAIN) || (errno == EWOULDBLOCK)))
		{
			sched_yield();
			continue;
		}

		if (status <= 0)
			break;

		received += status;
	}

	if (received < (ssize_t) len)
	{
		fprintf(stderr, "%s\n", _("I/O error"));
		free(request);
		return (request = NULL);
	}

	printf("%s %u.%u\n", _("Request version"), (header->version >> 16),
		(header->version & 0xFFFF));
	printf("%s %d\n", _("Request"), header->request);

	return (request);
}


static ssize_t sendReply(int sockFd, unsigned char *buffer, unsigned buffLen)
{
	ssize_t status = 0;
	ssize_t sent = 0;

	printf(_("Send %u bytes\n"), buffLen);

	while (!stop && (sent < (ssize_t) buffLen))
	{
		status = send(sockFd, (buffer + sent), (buffLen - sent), 0);

		if (status < 0)
			break;

		sent += status;
	}

	return (sent);
}


static void replyError(int sockFd, int error, char *message)
{
	storeReplyError reply;

	memset(&reply, 0, sizeof(storeReplyError));

	reply.header.len = sizeof(storeReplyError);
	reply.header.version = STORE_HEADER_VERSION;
	reply.header.request = STORE_REPLY_ERROR;

	reply.error = error;

	if (message)
		reply.header.len += (strlen(message) + 1);

	sendReply(sockFd, (unsigned char *) &reply, sizeof(storeReplyError));

	if (message)
		sendReply(sockFd, (unsigned char *) message, (strlen(message) + 1));
}


static char *iterateDirectory(DIR **dir, char *osVersion)
{
	char *fileName = NULL;
	char packageDir[64];
	struct dirent entry;
	struct dirent *result;

	sprintf(packageDir, "packages/%s", osVersion);

	if (!*dir)
	{
		// Open the version-specific directory
		*dir = opendir(packageDir);
		if (!*dir)
			return (fileName = NULL);

		printf("%s %s\n", _("Opened directory"), packageDir);
	}

	while (!stop)
	{
		if (readdir_r(*dir, &entry, &result))
		{
			perror("readdir_r");
			fprintf(stderr, "%s\n", _("Error reading directory"));
			break;
		}

		if (!result)
			break;

		if (result->d_type != DT_REG)
			continue;

		fileName = malloc(strlen(packageDir) + 1 + strlen(result->d_name) + 1);
		if (!fileName)
		{
			perror("malloc");
			fprintf(stderr, "%s\n", _("Memory error"));
			break;
		}

		sprintf(fileName, "%s/%s", packageDir, result->d_name);
		break;
	}

	return (fileName);
}


static int replyPackages(int sockFd, storeRequestPackages *request)
{
	int status = 0;
	storeReplyPackages reply;
	char osVersion[STORE_VERSION_MAX];
	DIR *dir = NULL;
	char *fileName = NULL;
	FILE *inStream = NULL;
	installPackageHeader header;
	installPackageHeader **headers = NULL;
	int count;

	memset(&reply, 0, sizeof(storeReplyPackages));
	memset(&header, 0, sizeof(installPackageHeader));

	installVersion2String(request->osVersion, osVersion);

	printf("%s %s\n", _("Sending package list for"), osVersion);

	reply.header.len = sizeof(storeReplyPackages);
	reply.header.version = STORE_HEADER_VERSION;
	reply.header.request = STORE_REQUEST_PACKAGES;

	// See what packages we have
	while (!stop)
	{
		fileName = iterateDirectory(&dir, osVersion);
		if (!fileName)
			break;

		// Open the file
		inStream = fopen(fileName, "r");
		if (!inStream)
		{
			perror("fopen");
			fprintf(stderr, "%s %s\n", _("Error opening"), fileName);
			free(fileName);
			continue;
		}

		printf("%s %s\n", _("Reading"), fileName);

		// Try to read the fixed header
		if (fread(&header, sizeof(installPackageHeader), 1, inStream) < 1)
		{
			perror("fread");
			fprintf(stderr, "%s %s\n", _("Skipping"), fileName);
			fclose(inStream);
			free(fileName);
			continue;
		}

		// Was an architecture requested?
		if (request->osArch != arch_all)
		{
			if (header.packageArch != request->osArch)
			{
				fclose(inStream);
				free(fileName);
				continue;
			}
		}

		// Get memory

		headers = realloc(headers, ((reply.numPackages + 1) *
			sizeof(installPackageHeader *)));
		if (!headers)
		{
			perror("realloc");
			fprintf(stderr, "%s\n", _("Memory error"));
			fclose(inStream);
			free(fileName);
			goto out;
		}

		headers[reply.numPackages] = calloc(1, header.headerLen);
		if (!headers[reply.numPackages])
		{
			perror("calloc");
			fprintf(stderr, "%s\n", _("Memory error"));
			fclose(inStream);
			free(fileName);
			goto out;
		}

		memcpy(headers[reply.numPackages], &header,
			sizeof(installPackageHeader));

		printf("%s %s\n", _("Reading more"), fileName);

		// Try to read the rest of the header
		if (fread(((void *) headers[reply.numPackages] +
			sizeof(installPackageHeader)), (header.headerLen -
			sizeof(installPackageHeader)), 1, inStream) < 1)
		{
			perror("fread");
			fprintf(stderr, "%s %s\n", _("Error reading"), fileName);
			free(headers[reply.numPackages]);
			fclose(inStream);
			free(fileName);
			continue;
		}

		printf("%s %s\n", _("Added"), fileName);

		reply.header.len += header.headerLen;
		reply.numPackages += 1;

		fclose(inStream);
		free(fileName);
	}

	printf("%s\n", _("Sending reply"));

	sendReply(sockFd, (unsigned char *) &reply, sizeof(storeReplyPackages));

	for (count = 0; count < reply.numPackages; count ++)
	{
		sendReply(sockFd, (unsigned char *) headers[count],
			headers[count]->headerLen);
	}

	status = 0;

out:
	if (headers)
	{
		for (count = 0; count < reply.numPackages; count ++)
		{
			if (headers[count])
				free(headers[count]);
		}

		free(headers);
	}

	if (dir)
		closedir(dir);

	return (status);
}


static int sendFile(int sockFd, char *inFileName, unsigned len)
{
	// Send file data directly to the network

	int status = 0;
	unsigned char *buffer = NULL;
	int fileFd = 0;
	ssize_t buffered = 0;
	ssize_t tmpSent = 0;
	ssize_t sent = 0;

	#define BUFFERSIZE	1048576

	// Temporary buffer of limited size
	buffer = malloc(BUFFERSIZE);
	if (!buffer)
	{
		status = errno;
		perror("malloc");
		fprintf(stderr, "%s\n", _("Memory error"));
		return (status);
	}

	// Open the file
	fileFd = open(inFileName, O_RDONLY);
	if (fileFd < 0)
	{
		status = errno;
		perror("open");
		fprintf(stderr, "%s %s\n", _("Couldn't open"), inFileName);
		free(buffer);
		return (status);
	}

	while (!stop && (sent < (ssize_t) len))
	{
		buffered = read(fileFd, buffer, BUFFERSIZE);
		if (buffered <= 0)
		{
			status = errno;
			perror("read");
			goto out;
		}

		for (tmpSent = 0; (!stop && buffered && (sent < (ssize_t) len)); )
		{
			tmpSent = sendReply(sockFd, (buffer + tmpSent), buffered);
			if (tmpSent <= 0)
				goto out;

			buffered -= tmpSent;
			sent += tmpSent;
		}
	}

	status = 0;

out:
	close(fileFd);
	free(buffer);

	if (sent < (ssize_t) len)
	{
		fprintf(stderr, "%s\n", _("I/O error"));
		return (status = ERR_IO);
	}

	return (status);
}


static int replyDownload(int sockFd, storeRequestDownload *request)
{
	int status = 0;
	storeReplyDownload reply;
	char osVersion[STORE_VERSION_MAX];
	DIR *dir = NULL;
	char *fileName = NULL;
	installInfo *info = NULL;
	struct stat st;

	memset(&reply, 0, sizeof(storeReplyDownload));

	installVersion2String(request->osVersion, osVersion);

	printf("%s %s (%s) %s %s\n", _("Request download of"), request->name,
		installArch2String(request->osArch), _("for"), osVersion);

	reply.header.len = sizeof(storeReplyDownload);
	reply.header.version = STORE_HEADER_VERSION;
	reply.header.request = STORE_REQUEST_DOWNLOAD;

	// See what packages we have
	while (!stop)
	{
		fileName = iterateDirectory(&dir, osVersion);
		if (!fileName)
		{
			status = ERR_NOSUCHFILE;
			replyError(sockFd, status, _("No matching package found"));
			goto out;
		}

		status = installPackageInfoGet(fileName, &info);
		if (status < 0)
		{
			fprintf(stderr, "%s %s\n", _("Error getting info for"), fileName);
			free(fileName);
			continue;
		}

		// Was an architecture requested?
		if (request->osArch != arch_all)
		{
			if (strcmp(info->arch, installArch2String(request->osArch)))
			{
				installInfoFree(info);
				free(fileName);
				continue;
			}
		}

		// Does the name match?
		if (strcmp(info->name, request->name))
		{
			installInfoFree(info);
			free(fileName);
			continue;
		}

		installInfoFree(info);

		printf("%s %s\n", _("Found package file"), fileName);

		// We need the file size
		status = stat(fileName, &st);
		if (status < 0)
		{
			status = errno;
			perror("stat");
			fprintf(stderr, "%s %s\n", _("Couldn't stat()"), fileName);
			goto out;
		}

		printf("%s\n", _("Sending reply"));

		reply.header.len += st.st_size;

		sendReply(sockFd, (unsigned char *) &reply,
			sizeof(storeReplyDownload));
		sendFile(sockFd, fileName, st.st_size);

		free(fileName);
		break;
	}

	status = 0;

out:
	if (dir)
		closedir(dir);

	return (status);
}


static void *handleConnection(void *arg)
{
	int status = 0;
	connection *conn = arg;
	struct sockaddr_in *addr = (struct sockaddr_in *) &conn->addrStorage;
	struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *) &conn->addrStorage;
	char addrString[INET6_ADDRSTRLEN];
	storeRequestHeader *request = NULL;

	if (conn->addrStorage.ss_family == AF_INET)
	{
		inet_ntop(conn->addrStorage.ss_family, (const void *)
			&addr->sin_addr, addrString, INET_ADDRSTRLEN);
	}
	else if (conn->addrStorage.ss_family == AF_INET6)
	{
		inet_ntop(conn->addrStorage.ss_family, (const void *)
			&addr6->sin6_addr, addrString, INET6_ADDRSTRLEN);
	}
	else
	{
		fprintf(stderr, "%s %d\n", _("Unknown address family"),
			conn->addrStorage.ss_family);
		strcpy(addrString, "(unknown)");
	}

	printf("%s %s\n", _("Connection from"), addrString);

	request = (storeRequestHeader *) receiveRequest(conn->sockFd);
	if (!request)
	{
		status = ERR_NODATA;
		goto out;
	}

	switch (request->request)
	{
		case STORE_REQUEST_PACKAGES:
			status = replyPackages(conn->sockFd, (storeRequestPackages *)
				request);
			break;

		case STORE_REQUEST_DOWNLOAD:
			status = replyDownload(conn->sockFd, (storeRequestDownload *)
				request);
			break;

		default:
			fprintf(stderr, "%s %d\n", _("Unknown request"),
				request->request);
			status = ERR_INVALID;
			break;
	}

	free(request);

	if (status < 0)
	{
		fprintf(stderr, "%s %d\n", _("Error handling request"),
			request->request);
	}

out:
	printf("%s %s\n\n", _("disconnecting"), addrString);
	shutdown(conn->sockFd, SHUT_RDWR);
	close(conn->sockFd);
	free(conn);

	pthread_exit((void *)(long) status);
	return (NULL);
}


int main(void)
{
	int status = 0;
	struct addrinfo hints, *addrInfo;
	int connFd = 0;
	struct sockaddr_storage connAddr;
	socklen_t connAddrSize = 0;
	connection *conn = NULL;
	pthread_t threadId;

	textdomain("store");

	printf("\n%s v%s\n", _("Visopsys store"), _VERSION_);

	// Set up the signal handler for catching CTRL-C interrupt
	if (signal(SIGINT, &interrupt) == SIG_ERR)
	{
		status = errno;
		perror("signal");
		fprintf(stderr, "%s\n", _("Error setting signal handler"));
		goto out;
	}

	// Get our host server info

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	status = getaddrinfo(NULL, STR(STORE_SERVER_PORT), &hints, &addrInfo);
	if (status)
	{
		perror("getaddrinfo");
		fprintf(stderr, "%s: %s\n", _("Failed to get host info"),
			gai_strerror(status));
		status = errno;
		goto out;
	}

	// Open our server socket

	srvSockFd = socket(addrInfo->ai_family, addrInfo->ai_socktype,
		addrInfo->ai_protocol);
	if (srvSockFd < 0)
	{
		status = errno;
		perror("socket");
		fprintf(stderr, "%s\n", _("Couldn't open socket"));
		goto out;
	}

	// Bind to the port
	status = bind(srvSockFd, addrInfo->ai_addr, addrInfo->ai_addrlen);
	if (status < 0)
	{
		status = errno;
		perror("bind");
		fprintf(stderr, "%s\n", _("Couldn't bind to port"));
		goto out;
	}

	status = listen(srvSockFd, STORE_SERVER_CONNBACKLOG);
	if (status < 0)
	{
		status = errno;
		perror("listen");
		fprintf(stderr, "%s\n", _("Couldn't listen for connections"));
		goto out;
	}

	printf("%s %d\n\n", _("Waiting for connections on port"),
		STORE_SERVER_PORT);
	while (!stop)
	{
		connAddrSize = sizeof(struct sockaddr_storage);

		connFd = accept(srvSockFd, (struct sockaddr *) &connAddr,
			&connAddrSize);
		if (connFd < 0)
		{
			perror("accept");
			fprintf(stderr, "%s\n", _("Connection attempt failed"));
			continue;
		}

		conn = calloc(1, sizeof(connection));
		if (!conn)
		{
			perror("calloc");
			fprintf(stderr, "%s\n", _("Memory error"));
			continue;
		}

		conn->sockFd = connFd;
		memcpy(&conn->addrStorage, &connAddr, connAddrSize);

		status = pthread_create(&threadId, NULL /* attr */, &handleConnection,
			conn);
	}

	status = 0;

out:
	// Free the addrinfo list
	if (addrInfo)
		freeaddrinfo(addrInfo);

	if (srvSockFd >= 0)
		close(srvSockFd);

	return (status);
}

