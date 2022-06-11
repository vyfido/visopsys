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
//  software.c
//

// This is a program for connecting to a software store, and downloading,
// installing, and updating software

/* This is the text that appears when a user requests help about this program
<help>

 -- software --

Connect to a software store to download, install, and update software.

Usage:
  software [-T] [-l] [-q] [-i name | file]

If no options are specified, a list of available packages will be
displayed.

Options:
-i <name | file>  : Install a package from the store by name, or from a file
-l                : List installed packages
-q                : Query available packages from the store
-T                : Force text mode operation

</help>
*/

#include <errno.h>
#include <netdb.h>
#include <libintl.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/api.h>
#include <sys/env.h>
#include <sys/file.h>
#include <sys/install.h>
#include <sys/paths.h>
#include <sys/progress.h>
#include <sys/socket.h>
#include <sys/store.h>
#include <sys/utsname.h>
#include <sys/vsh.h>
#include <sys/window.h>

#define _(string) gettext(string)

// The name of the 'software' package, and its version number.  In case this
// program needs to update itself to the latest version.
#define SOFTWARE_PACKAGE_NAME		"software"
#define SOFTWARE_PACKAGE_VERSION	"0.9"

#define WINDOW_TITLE				_("Software")
#define AVAILABLE_STRING			_("Available packages")
#define INSTALLED_STRING			_("Installed packages")
#define NOPACKAGES_STRING			_("[no packages]                   ")
#define PACKAGENAME_STRING			_("Package name:")
#define PACKAGEVERSION_STRING		_("Version:")
#define PACKAGEARCH_STRING			_("Architecture:")
#define PACKAGEDESC_STRING			_("Description:")
#define INSTALL_STRING				_("Install")
#define UNINSTALL_STRING			_("Uninstall")

typedef enum {
	operation_none,
	operation_listinstalled,
	operation_listavailable,
	operation_install

} operation_type;

typedef struct {
	installInfo *instInfo;
	listItemParameters itemParams;

} packageInfo;

static int processId = 0;
static int privilege = 0;
static int graphics = 0;
static struct utsname uts;
static installArch osArch = arch_unknown;
static objectKey window = NULL;
static objectKey availableLabel = NULL;
static objectKey availableList = NULL;
static objectKey installedLabel = NULL;
static objectKey installedList = NULL;
static objectKey packageNameLabel = NULL;
static objectKey packageNameValueLabel = NULL;
static objectKey packageVersionLabel = NULL;
static objectKey packageVersionValueLabel = NULL;
static objectKey packageArchLabel = NULL;
static objectKey packageArchValueLabel = NULL;
static objectKey packageDescLabel = NULL;
static objectKey packageDescTextArea = NULL;
static objectKey installButton = NULL;
static objectKey uninstallButton = NULL;
static linkedList installedInfoList;
static linkedList availableInfoList;


__attribute__((format(printf, 1, 2)))
static void error(const char *format, ...)
{
	// Generic error message code for either text or graphics modes

	va_list list;
	char output[MAXSTRINGLENGTH + 1];

	va_start(list, format);
	vsnprintf(output, MAXSTRINGLENGTH, format, list);
	va_end(list);

	if (graphics)
		windowNewErrorDialog(window, _("Error"), output);
	else
		fprintf(stderr, "\n%s\n", output);
}


static void usage(char *name)
{
	error(_("usage:\n%s [-T] [-l] [-q] [-i name | file]"), name);
}


__attribute__((format(printf, 2, 3)))
static objectKey info(int button, const char *format, ...)
{
	// Generic info message code for either text or graphics modes

	objectKey dialogWindow = NULL;
	va_list list;
	char output[MAXSTRINGLENGTH + 1];

	va_start(list, format);
	vsnprintf(output, MAXSTRINGLENGTH, format, list);
	va_end(list);

	if (graphics)
	{
		if (button)
			windowNewInfoDialog(window, _("Info"), output);
		else
			dialogWindow = windowNewBannerDialog(window, _("Info"), output);
	}
	else
	{
		fprintf(stdout, "\n%s\n", output);
	}

	return (dialogWindow);
}


static int openConnection(const char *host, int port)
{
	int status = 0;
	char portString[6];
	objectKey dialogWindow = NULL;
	int fd = 0;
	struct addrinfo hints, *addrInfo;
	char addrString[INET6_ADDRSTRLEN];

	// Get the store server info

	snprintf(portString, sizeof(portString), "%u", (unsigned short) port);

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	status = getaddrinfo(host, portString, &hints, &addrInfo);
	if (status)
	{
		status = errno;
		perror("socket");
		error("%s: %s", _("Failed to lookup store host info"),
			gai_strerror(status));
		goto out;
	}

	if (addrInfo->ai_family == AF_INET)
	{
		struct sockaddr_in *addr =
			(struct sockaddr_in *) addrInfo->ai_addr;
		inet_ntop(AF_INET, &addr->sin_addr, addrString, INET_ADDRSTRLEN);
		port = ntohs(addr->sin_port);
	}
	else
	{
		struct sockaddr_in6 *addr6 =
			(struct sockaddr_in6 *) addrInfo->ai_addr;
		inet_ntop(AF_INET6, &addr6->sin6_addr, addrString, INET6_ADDRSTRLEN);
		port = ntohs(addr6->sin6_port);
	}

	// Open our socket

	dialogWindow = info(0 /* no button */, _("Trying %s:%d..."), addrString,
		port);

	fd = socket(addrInfo->ai_family, addrInfo->ai_socktype,
		addrInfo->ai_protocol);
	if (fd < 0)
	{
		status = errno;
		perror("socket");
		error("%s", _("Couldn't open socket"));
		goto out;
	}

	status = connect(fd, addrInfo->ai_addr, addrInfo->ai_addrlen);
	if (status < 0)
	{
		status = errno;
		perror("connect");
		error("%s", _("Couldn't connect"));
		goto out;
	}

	if (!graphics)
	{
		printf(_("Connected to %s (%s) port %u\n"), STORE_SERVER_HOST,
			addrString, port);
	}

	status = fd;

out:
	if (dialogWindow)
		windowDestroy(dialogWindow);

	// Free the addrinfo list
	if (addrInfo)
		freeaddrinfo(addrInfo);

	return (status);
}


static void sendRequest(int sockFd, unsigned char *buffer, unsigned buffLen)
{
	ssize_t status = 0;
	ssize_t sent = 0;

	while (sent < (ssize_t) buffLen)
	{
		status = send(sockFd, (buffer + sent), (buffLen - sent), 0);
		if (status > 0)
			sent += status;
	}
}


static int receiveToFile(int sockFd, unsigned len, char **outFileName,
	progress *prog)
{
	// Received file data directly from the network and save it to a temporary
	// file

	int status = 0;
	unsigned char *buffer = NULL;
	int fileFd = 0;
	ssize_t buffered = 0;
	ssize_t tmpWritten = 0;
	ssize_t written = 0;

	#define BUFFERSIZE	1048576

	// Temporary buffer of limited size
	buffer = malloc(BUFFERSIZE);
	if (!buffer)
	{
		status = errno;
		perror("malloc");
		error("%s", _("Memory error"));
		return (status);
	}

	// Get a name for the temporary file
	*outFileName = strdup("downloadtmp-XXXXXX");
	if (!*outFileName)
	{
		status = errno;
		perror("strdup");
		error("%s", _("Memory error"));
		free(buffer);
		return (status);
	}

	// Open the temporary file
	fileFd = mkstemp(*outFileName);
	if (fileFd < 0)
	{
		status = errno;
		perror("mkstemp");
		error("%s", _("Temp file error"));
		free(*outFileName);
		*outFileName = NULL;
		free(buffer);
		return (status);
	}

	if (prog)
		prog->numTotal = len;

	while (written < (ssize_t) len)
	{
		buffered = recv(sockFd, buffer, BUFFERSIZE, 0 /* flags */);
		if (buffered <= 0)
			goto out;

		for (tmpWritten = 0; (buffered && (written < (ssize_t) len)); )
		{
			tmpWritten = write(fileFd, (buffer + tmpWritten), buffered);
			if (tmpWritten <= 0)
				goto out;

			buffered -= tmpWritten;
			written += tmpWritten;

			if (prog)
			{
				prog->numFinished += tmpWritten;
				prog->percentFinished = (int)((prog->numFinished * 100) /
					prog->numTotal);
			}
		}
	}

out:
	close(fileFd);
	free(buffer);

	if (written < (ssize_t) len)
	{
		error("%s", _("I/O error"));
		free(*outFileName);
		*outFileName = NULL;
		if (prog)
			prog->cancel = 1;
		return (status = ERR_IO);
	}

	if (prog)
		prog->complete = 1;

	return (status = 0);
}


static unsigned char *receiveReply(int sockFd, unsigned headerLen,
	char **outFileName, progress *prog)
{
	ssize_t status = 0;
	unsigned char *reply = NULL;
	unsigned magic = 0;
	unsigned len = 0;
	ssize_t received = 0;
	storeRequestHeader *header = NULL;

	// Get the magic number of the reply

	status = recv(sockFd, &magic, sizeof(unsigned), 0 /* flags */);
	if (status < (ssize_t) sizeof(unsigned))
	{
		perror("recv");
		error("%s", _("I/O error"));
		return (reply = NULL);
	}

	received += status;

	// Get the length of the reply

	status = recv(sockFd, &len, sizeof(unsigned), 0 /* flags */);
	if (status < (ssize_t) sizeof(unsigned))
	{
		perror("recv");
		error("%s", _("I/O error"));
		return (reply = NULL);
	}

	received += status;

	// Are we supposed to write trailing data to a file?
	if (!headerLen || !outFileName)
	{
		// No, it's all going into memory
		headerLen = len;
	}

	reply = calloc(headerLen, 1);
	if (!reply)
	{
		perror("calloc");
		error("%s", _("Memory error"));
		return (reply);
	}

	header = (storeRequestHeader *) reply;
	header->magic = magic;
	header->len = len;

	// Receive the reply data that will be returned in memory
	while (received < (ssize_t) headerLen)
	{
		status = recv(sockFd, (reply + received), (headerLen - received),
			0 /* flags */);

		if (status <= 0)
			break;

		received += status;
	}

	if (received < (ssize_t) headerLen)
	{
		error("%s", _("I/O error"));
		free(reply);
		return (reply = NULL);
	}

	// Check reply header version
	if (header->version > STORE_HEADER_VERSION)
	{
		error("%s=%u.%u %s", _("Reply version"), (header->version >> 16),
			(header->version & 0xFFFF), _("not supported"));
		free(reply);
		return (reply = NULL);
	}

	// Receive any additional data that will be stored in a file
	if (received < (ssize_t) len)
	{
		if (receiveToFile(sockFd, (len - received), outFileName, prog) < 0)
		{
			free(reply);
			return (reply = NULL);
		}
	}

	return (reply);
}


static void closeConnection(int sockFd)
{
	if (sockFd >= 0)
	{
		if (!graphics)
			printf("%s\n", _("Closing connection"));
		close(sockFd);
	}
}


static int getPackageList(storeReplyPackages **reply)
{
	int status = 0;
	int sockFd = 0;
	storeRequestPackages request;

	*reply = NULL;

	// Connect to the store
	sockFd = openConnection(STORE_SERVER_HOST, STORE_SERVER_PORT);
	if (sockFd < 0)
		return (status = sockFd);

	memset(&request, 0, sizeof(storeRequestPackages));
	request.header.magic = STORE_HEADER_MAGIC;
	request.header.len = sizeof(storeRequestPackages);
	request.header.version = STORE_HEADER_VERSION;
	request.header.request = STORE_REQUEST_PACKAGES;
	installString2Version(uts.release, request.osVersion);
	request.osArch = osArch;

	sendRequest(sockFd, (unsigned char *) &request,
		sizeof(storeRequestPackages));

	*reply = (storeReplyPackages *) receiveReply(sockFd,
		0 /* all in memory */, NULL /* no file */, NULL /* no progress */);
	if (!*reply)
	{
		status = ERR_IO;
		goto out;
	}

	status = 0;

out:
	closeConnection(sockFd);
	return (status);
}


static int getPackageInfo(const char *name, installInfo **instInfo)
{
	int status = 0;
	storeReplyPackages *replyPackages = NULL;
	unsigned char *header = NULL;
	int count;

	// Send a request
	status = getPackageList(&replyPackages);
	if (status < 0)
	{
		error("%s", _("Couldn't get package list"));
		return (status);
	}

	header = (unsigned char *) replyPackages->packageHeader;

	for (count = 0; count < replyPackages->numPackages; count ++)
	{
		status = installHeaderInfoGet(header, instInfo);
		if (status < 0)
		{
			error("%s", _("Error parsing package list"));
			break;
		}

		if (!strcmp((*instInfo)->name, name))
			break;

		installInfoFree(*instInfo);
		*instInfo = NULL;

		header += ((installPackageHeader *) header)->headerLen;
	}

	free(replyPackages);

	if (status < 0)
		return (status);

	if (!*instInfo)
	{
		error("%s", _("No matching package found"));
		return (status = ERR_NOSUCHENTRY);
	}

	return (status = 0);
}


static int getDownload(char *name, char **downloadFileName, progress *prog)
{
	int status = 0;
	int sockFd = 0;
	storeRequestDownload request;
	storeReplyDownload *reply = NULL;

	// Connect to the store
	sockFd = openConnection(STORE_SERVER_HOST, STORE_SERVER_PORT);
	if (sockFd < 0)
		return (status = sockFd);

	memset(&request, 0, sizeof(storeRequestDownload));
	request.header.magic = STORE_HEADER_MAGIC;
	request.header.len = (sizeof(storeRequestDownload) + strlen(name) + 1);
	request.header.version = STORE_HEADER_VERSION;
	request.header.request = STORE_REQUEST_DOWNLOAD;
	installString2Version(uts.release, request.osVersion);
	request.osArch = osArch;

	if (prog)
	{
		snprintf((char *) prog->statusMessage, PROGRESS_MAX_MESSAGELEN,
			"%s %s (%s) %s %s", _("Requesting download of"), name,
			installArch2String(request.osArch), _("for"), uts.release);
	}

	sendRequest(sockFd, (unsigned char *) &request,
		sizeof(storeRequestDownload));
	sendRequest(sockFd, (unsigned char *) name, (strlen(name) + 1));

	if (prog)
	{
		snprintf((char *) prog->statusMessage, PROGRESS_MAX_MESSAGELEN,
			"%s %s (%s) %s %s", _("Downloading"), name,
			installArch2String(request.osArch), _("for"), uts.release);
	}

	reply = (storeReplyDownload *) receiveReply(sockFd,
		sizeof(storeReplyDownload), downloadFileName, prog);

	closeConnection(sockFd);

	if (!reply)
		return (status = ERR_IO);

	free(reply);

	return (status = 0);
}


static int installPackage(const char *name)
{
	int status = 0;
	file fileStruct;
	installInfo *instInfo = NULL;
	const char *packageFileName = NULL;
	progress prog;
	objectKey progressDialog = NULL;
	char *downloadFileName = NULL;
	char *archiveFileName = NULL;
	char *filesDirName = NULL;

	memset((void *) &prog, 0, sizeof(progress));

	// Is the name a filename?
	if (!fileFind(name, &fileStruct) && (fileStruct.type == fileT))
	{
		status = installPackageInfoGet(name, &instInfo);
		if (status < 0)
		{
			error("%s %s", _("Couldn't get package info from"), name);
			return (status);
		}

		packageFileName = name;
	}
	else
	{
		status = getPackageInfo(name, &instInfo);
		if (status < 0)
		{
			error("%s %s", _("Couldn't get package info for"), name);
			return (status);
		}
	}

	if (graphics)
	{
		progressDialog = windowNewProgressDialog(window, _("Installing"),
			&prog);
	}
	else
	{
		vshProgressBar(&prog);
	}

	if (!packageFileName)
	{
		// Send a request
		status = getDownload(instInfo->name, &downloadFileName, &prog);
		if (status < 0)
		{
			error("%s %s", _("Couldn't download"), instInfo->name);
			goto out;
		}

		packageFileName = downloadFileName;
	}

	snprintf((char *) prog.statusMessage, PROGRESS_MAX_MESSAGELEN,
		"%s %s %s (%s)", _("Installing"), instInfo->name, instInfo->version,
		instInfo->arch);

	installInfoFree(instInfo);
	instInfo = NULL;

	// Extract the installInfo struct and the files archive
	status = installUnwrapPackageArchive(packageFileName, &instInfo,
		&archiveFileName);
	if (status < 0)
	{
		error("%s %s", _("Couldn't process"), instInfo->name);
		goto out;
	}

	// Check whether installation is expected to succeed
	status = installCheck(instInfo, NULL /* root directory */);
	if (status < 0)
	{
		error("%s %s", _("Installation check failed for"), instInfo->name);
		goto out;
	}

	// Extract the files archive
	status = installArchiveExtract(archiveFileName, &filesDirName);
	if (status < 0)
	{
		error("%s %s", _("Couldn't extract"), instInfo->name);
		goto out;
	}

	// Perform the installation
	status = installPackageAdd(instInfo, filesDirName,
		NULL /* root directory */);
	if (status < 0)
		error("%s %s", _("Error installing"), instInfo->name);

out:
	if (filesDirName)
		free(filesDirName);

	if (archiveFileName)
	{
		unlink(archiveFileName);
		free(archiveFileName);
	}

	if (downloadFileName)
	{
		unlink(downloadFileName);
		free(downloadFileName);
	}

	if (!graphics)
		vshProgressBarDestroy(&prog);
	else if (progressDialog)
		windowProgressDialogDestroy(progressDialog);

	if (instInfo)
		installInfoFree(instInfo);

	return (status);
}


static int uninstallPackage(installInfo *instInfo)
{
	int status = 0;
	progress prog;
	objectKey progressDialog = NULL;

	memset((void *) &prog, 0, sizeof(progress));

	if (graphics)
	{
		progressDialog = windowNewProgressDialog(window, _("Uninstalling"),
			&prog);
	}
	else
	{
		vshProgressBar(&prog);
	}

	snprintf((char *) prog.statusMessage, PROGRESS_MAX_MESSAGELEN,
		"%s %s %s (%s)", _("Uninstalling"), instInfo->name, instInfo->version,
		instInfo->arch);

	// Perform the uninstallation
	status = installPackageRemove(instInfo, NULL /* root directory */);
	if (status < 0)
		error("%s %s", _("Error uninstalling"), instInfo->name);

	if (!graphics)
		vshProgressBarDestroy(&prog);
	else if (progressDialog)
		windowProgressDialogDestroy(progressDialog);

	return (status);
}


static int listPackage(installInfo *instInfo, objectKey guiList,
	linkedList *memList)
{
	int status = 0;
	packageInfo *pkgInfo = NULL;

	if (graphics)
	{
		// Get memory for our package info structure
		pkgInfo = calloc(1, sizeof(packageInfo));
		if (!pkgInfo)
		{
			status = errno;
			perror("calloc");
			error("%s", _("Memory error"));
			return (status);
		}

		snprintf(pkgInfo->itemParams.text, WINDOW_MAX_LABEL_LENGTH, "%s",
			instInfo->name);
		pkgInfo->instInfo = instInfo;

		if (!memList->numItems)
		{
			status = windowComponentSetData(guiList, &pkgInfo->itemParams,
				1 /* size */, 1 /* render */);
		}
		else
		{
			status = windowComponentAppendData(guiList, &pkgInfo->itemParams,
				1 /* size */, 1 /* render */);
		}
		if (status < 0)
		{
			errno = status;
			perror("windowComponentAppendData");
			free(pkgInfo);
			return (status);
		}

		// Add it to the linked list
		status = linkedListAddBack(memList, pkgInfo);
		if (status < 0)
		{
			errno = status;
			perror("linkedListAddBack");
			free(pkgInfo);
			return (status);
		}
	}
	else
	{
		printf("%s %s (%s):\n\t%s\n", instInfo->name, instInfo->version,
			instInfo->arch, instInfo->desc);
	}

	return (status = 0);
}


static int listInstalledPackages(void)
{
	int status = 0;
	installInfo **databaseInfos = NULL;
	int numInfos = 0;
	listItemParameters dummyParams;
	installInfo *instInfo = NULL;
	int count;

	// Query the installation database for installed packages
	status = installDatabaseRead(&databaseInfos, &numInfos,
		NULL /* root directory */);
	if (status < 0)
	{
		error("%s", _("Couldn't get installed package list"));
		return (status);
	}

	if (graphics && !numInfos)
	{
		memset(&dummyParams, 0, sizeof(listItemParameters));
		strncpy(dummyParams.text, NOPACKAGES_STRING, WINDOW_MAX_LABEL_LENGTH);
		status = windowComponentSetData(installedList, &dummyParams,
			1 /* size */, 1 /* render */);
		return (status);
	}

	if (!graphics)
		printf("%s: %d\n", _("total packages"), numInfos);

	for (count = 0; count < numInfos; count ++)
	{
		instInfo = databaseInfos[count];

		status = listPackage(instInfo, installedList, &installedInfoList);
		if (status < 0)
			break;

		if (!graphics)
			installInfoFree(instInfo);
	}

	if (databaseInfos)
		free(databaseInfos);

	return (status);
}


static packageInfo *inPackageInfoList(linkedList *pkgInfoList,
	const char *name, const char *version)
{
	packageInfo *pkgInfo = NULL;
	linkedListItem *iter = NULL;

	pkgInfo = linkedListIterStart(pkgInfoList, &iter);

	while (pkgInfo)
	{
		if (!strcmp(pkgInfo->instInfo->name, name))
		{
			if (!version)
				return (pkgInfo);

			if (!strcmp(pkgInfo->instInfo->version, version))
				return (pkgInfo);
		}

		pkgInfo = linkedListIterNext(pkgInfoList, &iter);
	}

	return (NULL);
}


static int listAvailablePackages(void)
{
	int status = 0;
	storeReplyPackages *reply = NULL;
	unsigned char *header = NULL;
	installInfo *instInfo = NULL;
	packageInfo *pkgInfo = NULL;
	int listedPackages = 0;
	listItemParameters dummyParams;
	int count;

	// Send a request to the store for available packages
	status = getPackageList(&reply);
	if (status < 0)
	{
		error("%s", _("Couldn't get available package list"));
		return (status);
	}

	if (!graphics)
		printf("%s: %d\n", _("total packages"), reply->numPackages);

	header = (unsigned char *) reply->packageHeader;

	for (count = 0; count < reply->numPackages; count ++)
	{
		status = installHeaderInfoGet(header, &instInfo);
		if (status < 0)
		{
			error("%s", _("Error parsing package list"));
			break;
		}

		// Omit the package if an up-to-date version is already installed
		pkgInfo = inPackageInfoList(&installedInfoList, instInfo->name,
			NULL /* version */);
		if (pkgInfo)
		{
			if (!installVersionStringIsNewer(instInfo->version,
				pkgInfo->instInfo->version))
			{
				goto next;
			}
		}

		status = listPackage(instInfo, availableList, &availableInfoList);
		if (status < 0)
			break;

		listedPackages += 1;

	next:
		if (!graphics)
			installInfoFree(instInfo);

		header += ((installPackageHeader *) header)->headerLen;
	}

	if (graphics && !listedPackages)
	{
		memset(&dummyParams, 0, sizeof(listItemParameters));
		strncpy(dummyParams.text, NOPACKAGES_STRING, WINDOW_MAX_LABEL_LENGTH);
		status = windowComponentSetData(availableList, &dummyParams,
			1 /* size */, 1 /* render */);
	}

	free(reply);

	return (status);
}


static void refreshWindow(void)
{
	// We got a 'window refresh' event (probably because of a language
	// switch), so we need to update things

	// Re-get the language setting
	setlocale(LC_ALL, getenv(ENV_LANG));
	textdomain("software");

	// Re-get the character set
	if (getenv(ENV_CHARSET))
		windowSetCharSet(window, getenv(ENV_CHARSET));

	// Refresh the 'available packages' label
	windowComponentSetData(availableLabel, AVAILABLE_STRING,
		strlen(AVAILABLE_STRING), 1 /* redraw */);

	// Refresh the 'installed packages' label
	windowComponentSetData(installedLabel, INSTALLED_STRING,
		strlen(INSTALLED_STRING), 1 /* redraw */);

	// Refresh the 'package name' label
	windowComponentSetData(packageNameLabel, PACKAGENAME_STRING,
		strlen(PACKAGENAME_STRING), 1 /* redraw */);

	// Refresh the 'package version' label
	windowComponentSetData(packageVersionLabel, PACKAGEVERSION_STRING,
		strlen(PACKAGEVERSION_STRING), 1 /* redraw */);

	// Refresh the 'package architecture' label
	windowComponentSetData(packageArchLabel, PACKAGEARCH_STRING,
		strlen(PACKAGEARCH_STRING), 1 /* redraw */);

	// Refresh the 'package description' label
	windowComponentSetData(packageDescLabel, PACKAGEDESC_STRING,
		strlen(PACKAGEDESC_STRING), 1 /* redraw */);

	// Refresh the 'install' button
	windowComponentSetData(installButton, INSTALL_STRING,
		strlen(INSTALL_STRING), 1 /* redraw */);

	// Refresh the 'uninstall' button
	windowComponentSetData(uninstallButton, UNINSTALL_STRING,
		strlen(UNINSTALL_STRING), 1 /* redraw */);

	// Refresh the window title
	windowSetTitle(window, WINDOW_TITLE);

	// Re-layout the window
	windowLayout(window);
}


static packageInfo *selectedPackageInfo(objectKey list)
{
	// Find a packageInfo based on the selected package

	linkedList *pkgInfoList = NULL;
	int packageNumber = 0;
	packageInfo *pkgInfo = NULL;
	linkedListItem *iter = NULL;

	if (list == installedList)
		pkgInfoList = &installedInfoList;
	else if (list == availableList)
		pkgInfoList = &availableInfoList;
	else
		return (NULL);

	windowComponentGetSelected(list, &packageNumber);
	if (packageNumber < 0)
		return (NULL);

	pkgInfo = linkedListIterStart(pkgInfoList, &iter);

	while (pkgInfo && (packageNumber > 0))
	{
		pkgInfo = linkedListIterNext(pkgInfoList, &iter);
		packageNumber -= 1;
	}

	if (!packageNumber)
		return (pkgInfo);
	else
		return (NULL);
}


static void clearPackageLabels(void)
{
	windowComponentSetData(packageNameValueLabel, NULL, 0, 1 /* render */);
	windowComponentSetData(packageVersionValueLabel, NULL, 0, 1 /* render */);
	windowComponentSetData(packageArchValueLabel, NULL, 0, 1 /* render */);
	windowComponentSetData(packageDescTextArea, NULL, 0, 1 /* render */);
}


static void clearPackageInfoLists(void)
{
	packageInfo *pkgInfo = NULL;
	linkedListItem *iter = NULL;

	pkgInfo = linkedListIterStart(&installedInfoList, &iter);

	while (pkgInfo)
	{
		if (pkgInfo->instInfo)
			installInfoFree(pkgInfo->instInfo);

		free(pkgInfo);

		pkgInfo = linkedListIterNext(&installedInfoList, &iter);
	}

	linkedListClear(&installedInfoList);

	pkgInfo = linkedListIterStart(&availableInfoList, &iter);

	while (pkgInfo)
	{
		if (pkgInfo->instInfo)
			installInfoFree(pkgInfo->instInfo);

		free(pkgInfo);

		pkgInfo = linkedListIterNext(&availableInfoList, &iter);
	}

	linkedListClear(&availableInfoList);
}


static void eventHandler(objectKey key, windowEvent *event)
{
	packageInfo *pkgInfo = NULL;

	// Check for window events
	if (key == window)
	{
		// Check for window refresh
		if (event->type == WINDOW_EVENT_WINDOW_REFRESH)
		{
			refreshWindow();
		}

		// Check for the window being closed
		else if (event->type == WINDOW_EVENT_WINDOW_CLOSE)
		{
			windowGuiStop();
			windowDestroy(window);
		}
	}

	else if (((key == installedList) || (key == availableList)) &&
		(event->type & WINDOW_EVENT_SELECTION) &&
		((event->type & WINDOW_EVENT_MOUSE_DOWN) ||
			(event->type & WINDOW_EVENT_KEY_DOWN)))
	{
		windowComponentSetEnabled(installButton, ((key == availableList) &&
			(availableInfoList.numItems != 0)));
		windowComponentSetEnabled(uninstallButton, ((key == installedList) &&
			(installedInfoList.numItems != 0)));

		pkgInfo = selectedPackageInfo(key);
		if (!pkgInfo)
		{
			clearPackageLabels();
			return;
		}

		windowComponentSetData(packageNameValueLabel, pkgInfo->instInfo->name,
			strlen(pkgInfo->instInfo->name), 1 /* render */);
		windowComponentSetData(packageVersionValueLabel,
			pkgInfo->instInfo->version, strlen(pkgInfo->instInfo->version),
			1 /* render */);
		windowComponentSetData(packageArchValueLabel, pkgInfo->instInfo->arch,
			strlen(pkgInfo->instInfo->arch), 1 /* render */);
		windowComponentSetData(packageDescTextArea, pkgInfo->instInfo->desc,
			strlen(pkgInfo->instInfo->desc), 1 /* render */);
	}

	else if (((key == installButton) || (key == uninstallButton))
		&& (event->type == WINDOW_EVENT_MOUSE_LEFTUP))
	{
		if (key == installButton)
			pkgInfo = selectedPackageInfo(availableList);
		else
			pkgInfo = selectedPackageInfo(installedList);

		if (!pkgInfo)
			return;

		windowComponentSetEnabled(installButton, 0);
		windowComponentSetEnabled(uninstallButton, 0);

		if (key == installButton)
			installPackage(pkgInfo->instInfo->name);
		else
			uninstallPackage(pkgInfo->instInfo);

		clearPackageInfoLists();

		if (key == uninstallButton)
			clearPackageLabels();

		listInstalledPackages();
		listAvailablePackages();
	}
}


static int constructWindow(void)
{
	// If we are in graphics mode, make a window rather than operating on the
	// command line

	int status = 0;
	listItemParameters dummyParams;
	objectKey container = NULL;
	componentParameters params;

	// Create a new window
	window = windowNew(multitaskerGetCurrentProcessId(), WINDOW_TITLE);
	if (!window)
		return (status = ERR_NOCREATE);

	// Create a label for the list of available packages
	memset(&params, 0, sizeof(componentParameters));
	params.padLeft = params.padRight = params.padTop = 5;
	params.gridWidth = params.gridHeight = 1;
	params.orientationX = orient_left;
	params.orientationY = orient_top;
	params.flags |= COMP_PARAMS_FLAG_FIXEDHEIGHT;
	availableLabel = windowNewTextLabel(window, AVAILABLE_STRING, &params);

	// Create the list of available packages
	params.gridY += 1;
	params.padBottom = 5;
	params.flags = 0;
	memset(&dummyParams, 0, sizeof(listItemParameters));
	strncpy(dummyParams.text, NOPACKAGES_STRING, WINDOW_MAX_LABEL_LENGTH);
	availableList = windowNewList(window, windowlist_textonly, 12 /* rows */,
		1 /* columns */, 0 /* selectMultiple */, &dummyParams,
		1 /* numItems */, &params);
	windowRegisterEventHandler(availableList, &eventHandler);
	windowComponentFocus(availableList);

	// Create a label for the list of installed packages
	params.gridX += 1;
	params.gridY = 0;
	params.padBottom = 0;
	params.flags |= COMP_PARAMS_FLAG_FIXEDHEIGHT;
	installedLabel = windowNewTextLabel(window, INSTALLED_STRING, &params);

	// Create the list of installed packages
	params.gridY += 1;
	params.padBottom = 5;
	params.flags = 0;
	installedList = windowNewList(window, windowlist_textonly, 12 /* rows */,
		1 /* columns */, 0 /* selectMultiple */, &dummyParams,
		1 /* numItems */, &params);
	windowRegisterEventHandler(installedList, &eventHandler);

	// Create a container for things on the right hand side
	params.gridX += 1;
	params.gridY = 0;
	params.gridHeight = 2;
	params.flags |= (COMP_PARAMS_FLAG_FIXEDWIDTH |
		COMP_PARAMS_FLAG_FIXEDHEIGHT);
	container = windowNewContainer(window, "rightContainer", &params);

	// Create a 'package name' label
	params.gridX = params.gridY = 0;
	params.gridHeight = 1;
	params.padLeft = params.padRight = 5;
	params.padBottom = 0;
	packageNameLabel = windowNewTextLabel(container, PACKAGENAME_STRING,
		&params);

	// Create a 'package name value' label
	params.padTop = 5;
	params.gridX += 1;
	packageNameValueLabel = windowNewTextLabel(container,
		"                                ", &params);

	// Create a 'package version' label
	params.gridX = 0;
	params.gridY += 1;
	packageVersionLabel = windowNewTextLabel(container, PACKAGEVERSION_STRING,
		&params);

	// Create a 'package version value' label
	params.gridX += 1;
	packageVersionValueLabel = windowNewTextLabel(container, "", &params);

	// Create a 'package architecture' label
	params.gridX = 0;
	params.gridY += 1;
	packageArchLabel = windowNewTextLabel(container, PACKAGEARCH_STRING,
		&params);

	// Create a 'package architecture value' label
	params.gridX += 1;
	packageArchValueLabel = windowNewTextLabel(container, "", &params);

	// Create a 'package description' label
	params.gridX = 0;
	params.gridY += 1;
	packageDescLabel = windowNewTextLabel(container, PACKAGEDESC_STRING,
		&params);

	// Create a 'package description value' text area
	params.gridY += 1;
	params.gridWidth = 2;
	packageDescTextArea = windowNewTextArea(container, 40 /* columns */,
		10 /* rows */, 100 /* bufferLines */, &params);

	// Create an 'install' button
	params.gridY += 1;
	params.gridWidth = 1;
	params.padLeft = params.padRight = 2;
	params.orientationX = orient_right;
	installButton = windowNewButton(container, INSTALL_STRING, NULL, &params);
	windowComponentSetEnabled(installButton, 0);
	windowRegisterEventHandler(installButton, &eventHandler);

	// Create an 'uninstall' button
	params.gridX += 1;
	params.orientationX = orient_left;
	uninstallButton = windowNewButton(container, UNINSTALL_STRING, NULL,
		&params);
	windowComponentSetEnabled(uninstallButton, 0);
	windowRegisterEventHandler(uninstallButton, &eventHandler);

	// Register an event handler to catch window close events
	windowRegisterEventHandler(window, &eventHandler);

	// Go live
	windowSetVisible(window, 1);

	return (status = 0);
}


static int askUpdateSelf(void)
{
	// Returns 1 if the user will let us install a new version

	char *question = NULL;
	int response = 0;
	char character;

	question = _("A newer version of this program is available.\n"
		"Install it now?");

	if (graphics)
	{
		response = windowNewChoiceDialog(window, _("New Version"), question,
			(char *[]){ _("Install"), _("Cancel") }, 2 /* numChoices */,
			0 /* defaultChoice */);

		if (!response)
			return (1);
		else // ((response < 0) || (response > 0))
			return (0);
	}
	else
	{
		printf(_("\n%s (I)nstall/(C)ancel?: "), question);
		textInputSetEcho(0);

		while (1)
		{
			character = getchar();

			if ((character == 'i') || (character == 'I'))
			{
				printf("%s\n", _("Install"));
				textInputSetEcho(1);
				return (1);
			}
			else if ((character == 'c') || (character == 'C'))
			{
				printf("%s\n", _("Cancel"));
				textInputSetEcho(1);
				return (0);
			}
		}
	}
}


static void checkUpdateSelf(void)
{
	packageInfo *pkgInfo = NULL;

	pkgInfo = inPackageInfoList(&availableInfoList, SOFTWARE_PACKAGE_NAME,
		NULL /* version */);
	if (!pkgInfo)
		return;

	if (!installVersionStringIsNewer(pkgInfo->instInfo->version,
		SOFTWARE_PACKAGE_VERSION))
	{
		return;
	}

	// The package is newer than us.  Ask whether we can install it.
	if (!askUpdateSelf())
		return;

	if (installPackage(pkgInfo->instInfo->name) >= 0)
	{
		if (graphics)
		{
			windowGuiStop();
			windowDestroy(window);
		}

		// Relaunch the program.  This update check only gets run in the
		// 'default' interactive path, so we don't need to replicate any
		// command-line arguments.
		loaderLoadAndExec(PATH_PROGRAMS "/software", privilege,
			0 /* block */);

		exit(0);
	}
}


int main(int argc, char *argv[])
{
	int status = 0;
	char opt = '\0';
	operation_type operation = operation_none;
	char *name = NULL;

	setlocale(LC_ALL, getenv(ENV_LANG));
	textdomain("software");

	memset(&uts, 0, sizeof(struct utsname));
	memset(&installedInfoList, 0, sizeof(variableList));
	memset(&availableInfoList, 0, sizeof(variableList));

	// What is my process id?
	processId = multitaskerGetCurrentProcessId();

	// What is my privilege level?
	privilege = multitaskerGetProcessPrivilege(processId);

	// Are graphics enabled?
	graphics = graphicsAreEnabled();

	// Check options
	while (strchr("i:lqT?", (opt = getopt(argc, argv, "i:lqT"))))
	{
		switch (opt)
		{
			case 'i':
				// Install a package
				if (!optarg)
				{
					error("%s", _("Missing name argument for '-i' option"));
					usage(argv[0]);
					return (status = ERR_NULLPARAMETER);
				}
				operation = operation_install;
				name = optarg;
				break;

			case 'l':
				// List installed packages
				if (!graphics)
					operation = operation_listinstalled;
				break;

			case 'q':
				// Query available packages
				if (!graphics)
					operation = operation_listavailable;
				break;

			case ':':
				error(_("Missing parameter for %s option"), argv[optind - 1]);
				usage(argv[0]);
				return (status = ERR_NULLPARAMETER);

			case 'T':
				// Force text mode
				graphics = 0;
				break;

			default:
				error(_("Unknown option '%c'"), optopt);
				usage(argv[0]);
				return (status = ERR_INVALID);
		}
	}

	// Get the OS version
	status = uname(&uts);
	if (status < 0)
	{
		status = errno;
		perror("uname");
		error("%s", _("Couldn't get OS version"));
		goto out;
	}

	// Hard-code this for the time being
	osArch = arch_x86;

	switch (operation)
	{
		case operation_install:
		{
			status = installPackage(name);
			break;
		}

		case operation_listinstalled:
		{
			status = listInstalledPackages();
			break;
		}

		case operation_listavailable:
		{
			status = listAvailablePackages();
			break;
		}

		case operation_none:
		default:
		{
			if (graphics)
			{
				// Make our window
				status = constructWindow();
				if (status < 0)
					return (status);
			}

			listInstalledPackages();
			listAvailablePackages();

			// Check whether we need to update ourselves
			checkUpdateSelf();

			if (graphics)
			{
				// Run the GUI
				windowGuiRun();
			}

			break;
		}
	}

out:
	if (graphics)
		clearPackageInfoLists();

	return (status);
}

