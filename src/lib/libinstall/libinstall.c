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
//  libinstall.c
//

// This is the main entry point for our library of installation functions

#include "libinstall.h"
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#ifdef PORTABLE
	#define min(a, b) ((a) < (b) ? (a) : (b))
#else
	#include <sys/compress.h>
	#include <sys/install.h>
	#include <sys/loader.h>
	#include <sys/paths.h>
#endif

#ifdef DEBUG
	#define DEBUG_OUTMAX	160
	int debugLibInstall = 0;
	static char debugOutput[DEBUG_OUTMAX];

	static void __attribute__((format(printf, 1, 2)))
		DEBUGMSG(const char *message, ...)
	{
		va_list list;

		if (debugLibInstall)
		{
			va_start(list, message);
			vsnprintf(debugOutput, DEBUG_OUTMAX, message, list);
			printf("%s", debugOutput);
		}
	}
#else
	#define DEBUGMSG(message, arg...) do { } while (0)
#endif


static int checkHeader(installPackageHeader *header)
{
	// Try to ensure that lengths and offsets in the header are legal

	installDependHeader *dependHeader = NULL;
	installObsoleteHeader *obsoleteHeader = NULL;
	installFileHeader *fileHeader = NULL;
	int count;

	DEBUGMSG("Check header\n");

	if ((header->packageName >= header->headerLen) ||
		(header->packageDesc >= header->headerLen) ||
		(header->preExec >= header->headerLen) ||
		(header->postExec >= header->headerLen) ||
		(header->headerLen < (sizeof(installPackageHeader) +
			(header->numDepends * sizeof(installDependHeader)) +
			(header->numObsoletes * sizeof(installObsoleteHeader)) +
			(header->numFiles * sizeof(installFileHeader)))))
	{
		fprintf(stderr, "Invalid header lengths or offsets\n");
		return (ERR_BADDATA);
	}

	dependHeader = (installDependHeader *)((void *) header +
		sizeof(installPackageHeader));

	for (count = 0; count < header->numDepends; count ++)
	{
		if (dependHeader[count].name >= header->headerLen)
		{
			fprintf(stderr, "Invalid dependency offset\n");
			return (ERR_BADDATA);
		}
	}

	obsoleteHeader = (installObsoleteHeader *)((void *) dependHeader +
		(header->numDepends * sizeof(installDependHeader)));

	for (count = 0; count < header->numObsoletes; count ++)
	{
		if (obsoleteHeader[count].name >= header->headerLen)
		{
			fprintf(stderr, "Invalid obsolescency offset\n");
			return (ERR_BADDATA);
		}
	}

	fileHeader = (installFileHeader *)((void *) obsoleteHeader +
		(header->numObsoletes * sizeof(installObsoleteHeader)));

	for (count = 0; count < header->numFiles; count ++)
	{
		if ((fileHeader[count].archiveName >= header->headerLen) ||
			(fileHeader[count].targetName >= header->headerLen))
		{
			fprintf(stderr, "Invalid file offset\n");
			return (ERR_BADDATA);
		}
	}

	return (0);
}


static int header2Info(installPackageHeader *header, installInfo **info)
{
	// Interpret a header and create an installInfo struct

	int status = 0;
	unsigned fileHeadersLen = 0;
	unsigned dependsLen = 0;
	unsigned obsoletesLen = 0;
	unsigned filesLen = 0;
	unsigned stringsLen = 0;
	char *stringData = NULL;
	installDependHeader *dependHeader = NULL;
	installObsoleteHeader *obsoleteHeader = NULL;
	installFileHeader *fileHeader = NULL;
	int count;

	DEBUGMSG("Convert header to info\n");

	// Allocate the installInfo structure
	*info = calloc(1, sizeof(installInfo));
	if (!*info)
	{
		fprintf(stderr, "Memory error\n");
		return (status = ERR_MEMORY);
	}

	fileHeadersLen = (header->numFiles * sizeof(installFileHeader));
	dependsLen = (header->numDepends * sizeof(installDepend));
	obsoletesLen = (header->numObsoletes * sizeof(installObsolete));
	filesLen = (header->numFiles * sizeof(installFile));
	stringsLen = (header->headerLen - fileHeadersLen);

	// Get enough memory to contain the dependency, obsolescency, and file
	// entries strings

	(*info)->data = calloc((dependsLen + obsoletesLen + filesLen +
		stringsLen), 1);
	if (!(*info)->data)
	{
		fprintf(stderr, "Memory error\n");
		free(*info);
		*info = NULL;
		return (status = ERR_MEMORY);
	}

	(*info)->depend = (*info)->data;
	(*info)->obsolete = ((*info)->data + dependsLen);
	(*info)->file = ((*info)->data + dependsLen + obsoletesLen);
	stringData = ((*info)->data + dependsLen + obsoletesLen + filesLen);

	// Package name
	if (header->packageName)
	{
		(*info)->name = stringData;
		strcpy(stringData, ((char *) header + header->packageName));
		stringData += (strlen(stringData) + 1);
	}

	// Description
	if (header->packageDesc)
	{
		(*info)->desc = stringData;
		strcpy(stringData, ((char *) header + header->packageDesc));
		stringData += (strlen(stringData) + 1);
	}

	// Version
	installVersion2String(header->packageVersion, (*info)->version);

	// Architecture
	strcpy((*info)->arch, installArch2String(header->packageArch));

	// Pre-install execution
	if (header->preExec)
	{
		(*info)->preExec = stringData;
		strcpy(stringData, ((char *) header + header->preExec));
		stringData += (strlen(stringData) + 1);
	}

	// Post-install execution
	if (header->postExec)
	{
		(*info)->postExec = stringData;
		strcpy(stringData, ((char *) header + header->postExec));
		stringData += (strlen(stringData) + 1);
	}

	// The package dependencies

	(*info)->numDepends = header->numDepends;

	dependHeader = (installDependHeader *)((void *) header +
		sizeof(installPackageHeader));

	for (count = 0; count < header->numDepends; count ++)
	{
		if (dependHeader[count].name)
		{
			(*info)->depend[count].name = stringData;
			strcpy(stringData, ((char *) header + dependHeader[count].name));
			stringData += (strlen(stringData) + 1);
		}

		(*info)->depend[count].rel = dependHeader->rel;

		installVersion2String(dependHeader[count].version,
			(*info)->depend[count].version);
	}

	// The package obsolescences

	(*info)->numObsoletes = header->numObsoletes;

	obsoleteHeader = (installObsoleteHeader *)((void *) dependHeader +
		(header->numDepends * sizeof(installDependHeader)));

	for (count = 0; count < header->numObsoletes; count ++)
	{
		if (obsoleteHeader[count].name)
		{
			(*info)->obsolete[count].name = stringData;
			strcpy(stringData, ((char *) header +
				obsoleteHeader[count].name));
			stringData += (strlen(stringData) + 1);
		}

		(*info)->obsolete[count].rel = obsoleteHeader->rel;

		installVersion2String(obsoleteHeader[count].version,
			(*info)->obsolete[count].version);
	}

	// The files in the package

	(*info)->numFiles = header->numFiles;

	fileHeader = (installFileHeader *)((void *) obsoleteHeader +
		(header->numObsoletes * sizeof(installObsoleteHeader)));

	for (count = 0; count < header->numFiles; count ++)
	{
		if (fileHeader[count].archiveName)
		{
			(*info)->file[count].archiveName = stringData;
			strcpy(stringData, ((char *) header +
				fileHeader[count].archiveName));
			stringData += (strlen(stringData) + 1);
		}

		if (fileHeader[count].targetName)
		{
			(*info)->file[count].targetName = stringData;
			strcpy(stringData, ((char *) header +
				fileHeader[count].targetName));
			stringData += (strlen(stringData) + 1);
		}
	}

	return (status = 0);
}


static int info2Header(installInfo *info, installPackageHeader **header)
{
	// Interpret an installInfo struct and create an installPackageHeader

	int status = 0;
	unsigned dependHeadersLen = 0;
	unsigned obsoleteHeadersLen = 0;
	unsigned fileHeadersLen = 0;
	unsigned stringsLen = 0;
	unsigned headerLen = 0;
	installDependHeader *dependHeader = NULL;
	installObsoleteHeader *obsoleteHeader = NULL;
	installFileHeader *fileHeader = NULL;
	char *stringData = NULL;
	int count;

	DEBUGMSG("Convert info to header\n");

	// How much memory will we need for package dependencies?
	dependHeadersLen = (info->numDepends * sizeof(installDependHeader));

	// How much memory will we need for package obsolescences?
	obsoleteHeadersLen = (info->numObsoletes * sizeof(installObsoleteHeader));

	// How much memory will we need for file headers?
	fileHeadersLen = (info->numFiles * sizeof(installFileHeader));

	// Calculate how much memory we will need for strings

	if (info->name)
		stringsLen += (strlen(info->name) + 1);
	if (info->desc)
		stringsLen += (strlen(info->desc) + 1);
	if (info->preExec)
		stringsLen += (strlen(info->preExec) + 1);
	if (info->postExec)
		stringsLen += (strlen(info->postExec) + 1);

	DEBUGMSG("%d dependencies\n", info->numDepends);
	for (count = 0; count < info->numDepends; count ++)
	{
		if (info->depend[count].name)
			stringsLen += (strlen(info->depend[count].name) + 1);
	}

	DEBUGMSG("%d obsolescences\n", info->numObsoletes);
	for (count = 0; count < info->numObsoletes; count ++)
	{
		if (info->obsolete[count].name)
			stringsLen += (strlen(info->obsolete[count].name) + 1);
	}

	DEBUGMSG("%d files\n", info->numFiles);
	for (count = 0; count < info->numFiles; count ++)
	{
		if (info->file[count].archiveName)
			stringsLen += (strlen(info->file[count].archiveName) + 1);
		if (info->file[count].targetName)
			stringsLen += (strlen(info->file[count].targetName) + 1);
	}

	headerLen = (sizeof(installPackageHeader) + dependHeadersLen +
		obsoleteHeadersLen + fileHeadersLen + stringsLen);

	// Allocate the installPackageHeader structure
	*header = calloc(headerLen, 1);
	if (!*header)
	{
		fprintf(stderr, "Memory error\n");
		return (status = ERR_MEMORY);
	}

	dependHeader = ((void *) *header + sizeof(installPackageHeader));
	obsoleteHeader = ((void *) dependHeader + dependHeadersLen);
	fileHeader = ((void *) obsoleteHeader + obsoleteHeadersLen);
	stringData = ((void *) fileHeader + fileHeadersLen);

	// Fill out non-string package header fields

	(*header)->vspMagic = INSTALL_PACKAGE_MAGIC;
	(*header)->vspVersion = INSTALL_PACKAGE_VSP_1_0;
	(*header)->headerLen = headerLen;

	// Version
	installString2Version(info->version, (*header)->packageVersion);

	DEBUGMSG("Header package version=%d.%d.%d.%d\n",
		(*header)->packageVersion[0], (*header)->packageVersion[1],
		(*header)->packageVersion[2], (*header)->packageVersion[3]);

	// Architecture
	(*header)->packageArch = installString2Arch(info->arch);

	(*header)->numDepends = info->numDepends;
	(*header)->numObsoletes = info->numObsoletes;
	(*header)->numFiles = info->numFiles;

	// Save the package header strings

	if (info->name)
	{
		(*header)->packageName = (unsigned)(stringData - (char *) *header);
		strcpy(stringData, info->name);
		stringData += (strlen(stringData) + 1);
	}

	if (info->desc)
	{
		(*header)->packageDesc = (unsigned)(stringData - (char *) *header);
		strcpy(stringData, info->desc);
		stringData += (strlen(stringData) + 1);
	}

	if (info->preExec)
	{
		(*header)->preExec = (unsigned)(stringData - (char *) *header);
		strcpy(stringData, info->preExec);
		stringData += (strlen(stringData) + 1);
	}

	if (info->postExec)
	{
		(*header)->postExec = (unsigned)(stringData - (char *) *header);
		strcpy(stringData, info->postExec);
		stringData += (strlen(stringData) + 1);
	}

	// Create the dependency headers
	for (count = 0; count < info->numDepends; count ++)
	{
		if (info->depend[count].name)
		{
			dependHeader[count].name = (unsigned)(stringData -
				(char *) *header);
			strcpy(stringData, info->depend[count].name);
			stringData += (strlen(stringData) + 1);
		}

		dependHeader[count].rel = info->depend[count].rel;

		installString2Version(info->depend[count].version,
			dependHeader[count].version);
	}

	// Create the obsolescency headers
	for (count = 0; count < info->numObsoletes; count ++)
	{
		if (info->obsolete[count].name)
		{
			obsoleteHeader[count].name = (unsigned)(stringData -
				(char *) *header);
			strcpy(stringData, info->obsolete[count].name);
			stringData += (strlen(stringData) + 1);
		}

		obsoleteHeader[count].rel = info->obsolete[count].rel;

		installString2Version(info->obsolete[count].version,
			obsoleteHeader[count].version);
	}

	// Create the file headers
	for (count = 0; count < info->numFiles; count ++)
	{
		if (info->file[count].archiveName)
		{
			fileHeader[count].archiveName = (unsigned)(stringData -
				(char *) *header);
			strcpy(stringData, info->file[count].archiveName);
			stringData += (strlen(stringData) + 1);
		}

		if (info->file[count].targetName)
		{
			fileHeader[count].targetName = (unsigned)(stringData -
				(char *) *header);
			strcpy(stringData, info->file[count].targetName);
			stringData += (strlen(stringData) + 1);
		}
	}

	return (status = 0);
}


static installPackageHeader *readHeader(FILE *inStream)
{
	// Attempt to allocate and read in 'headerLen' bytes of the file

	off_t filePos = ftell(inStream);
	installPackageHeader *header = NULL;
	unsigned vspMagic = 0;
	unsigned vspVersion = 0;
	unsigned headerLen = 0;

	DEBUGMSG("Read file header\n");

	// Go to the file position, and read the first 3 fields
	if ((fread(&vspMagic, sizeof(unsigned), 1, inStream) < 1) ||
		(fread(&vspVersion, sizeof(unsigned), 1, inStream) < 1) ||
		(fread(&headerLen, sizeof(unsigned), 1, inStream) < 1))
	{
		fprintf(stderr, "Couldn't read file header\n");
		return (header = NULL);
	}

	// Check the magic number and the version
	if ((vspMagic != INSTALL_PACKAGE_MAGIC) ||
		(vspVersion != INSTALL_PACKAGE_VSP_1_0))
	{
		fprintf(stderr, "Bad VSP magic number or version\n");
		return (header = NULL);
	}

	// Try to allocate memory for the entire header
	header = calloc(headerLen, 1);
	if (!header)
	{
		fprintf(stderr, "Memory error\n");
		return (header);
	}

	// Go back to the file position
	if (fseek(inStream, filePos, SEEK_SET))
	{
		fprintf(stderr, "File header seek error\n");
		free(header);
		return (header = NULL);
	}

	// Try to read the entire header
	if (fread(header, 1, headerLen, inStream) < headerLen)
	{
		fprintf(stderr, "Couldn't read file header\n");
		free(header);
		return (header = NULL);
	}

	return (header);
}


static int readLine(char **buffer, unsigned bufferLen, char *line,
	unsigned lineLen)
{
	int gotChars = 0;

	*line = '\0';

	while (bufferLen && lineLen)
	{
		// Start of a buffer line

		// Discard leading whitespace
		while (isspace(**buffer))
		{
			*buffer += 1;
			bufferLen -= 1;
		}

		while (bufferLen && lineLen)
		{
			// Content of buffer line

			// Discard comments until newline
			if (**buffer == '#')
			{
				DEBUGMSG("Discarding comment: ");

				while (**buffer != '\n')
				{
					DEBUGMSG("%c", **buffer);
					*buffer += 1;
					bufferLen -= 1;
				}

				DEBUGMSG("\n");
				continue;
			}

			// Ignore Windows carriage-return
			if (**buffer == '\r')
			{
				*buffer += 1;
				bufferLen -= 1;
				continue;
			}

			if (**buffer == '\n')
			{
				*buffer += 1;
				bufferLen -= 1;

				if (gotChars)
				{
					// We got something, and then hit a newline, so we're
					// finished.
					*line = '\0';
					return (gotChars);
				}
				else
				{
					// Empty, whitespace-only, or comment-only line.  Look at
					// the next line.
					break;
				}
			}

			*line = **buffer;
			*buffer += 1;
			bufferLen -= 1;
			line += 1;
			lineLen -= 1;
			gotChars += 1;
		}
	}

	return (gotChars);
}


static int manifest2Info(char *buffer, unsigned bufferLen, installInfo **info)
{
	// Parse a manifest, and convert it into an installInfo structure

	int status = 0;
	char *ptr = NULL;
	char line[256];
	char *value = NULL;
	unsigned memory = 0;
	int numDepends = 0;
	int numObsoletes = 0;
	int numFiles = 0;
	char *stringData = NULL;

	DEBUGMSG("Convert manifest to info\n");

	// For the time being, the format is quite rigid, and this parser is very
	// simplistic.

	// First pass, just attempt to determine how much memory we will need
	ptr = buffer;
	while ((unsigned)(ptr - buffer) < bufferLen)
	{
		if (!readLine(&ptr, (bufferLen - (unsigned)(ptr - buffer)), line,
			sizeof(line)))
		{
			break;
		}

		DEBUGMSG("Read %s\n", line);

		// If there's a '=' character, separate the strings
		value = strchr(line, '=');
		if (value)
		{
			*value = '\0';
			value += 1;
		}

		if (!strcmp(line, INSTALL_NAME) ||
			!strcmp(line, INSTALL_DESC) ||
			!strcmp(line, INSTALL_VERSION) ||
			!strcmp(line, INSTALL_ARCH) ||
			!strcmp(line, INSTALL_PREEXEC) ||
			!strcmp(line, INSTALL_POSTEXEC))
		{
			if (!strcmp(line, INSTALL_VERSION) ||
				!strcmp(line, INSTALL_ARCH))
			{
				// Not dynamically allocated strings; ignore
				continue;
			}

			// Just need to count the characters on the right hand side of the
			// '=' character

			if (!value)
			{
				fprintf(stderr, "Malformed line (no value): %s\n", line);
				return (status = ERR_BADDATA);
			}

			memory += (strlen(value) + 1);
		}

		else if (!strcmp(line, INSTALL_DEPEND))
		{
			memory += sizeof(installDepend);
			numDepends += 1;

			if (!value)
			{
				fprintf(stderr, "Malformed line (no value): %s\n", line);
				return (status = ERR_BADDATA);
			}

			memory += (strlen(value) + 1);

			DEBUGMSG("Dependency %s\n", value);

			// Dependencies are a triple, separated by '|' characters

			if (!strchr(value, '|'))
			{
				fprintf(stderr, "Malformed line (no relation): %s\n", value);
				return (status = ERR_BADDATA);
			}

			value = (strchr(value, '|') + 1);

			if (!strchr(value, '|'))
			{
				fprintf(stderr, "Malformed line (no version): %s\n", value);
				return (status = ERR_BADDATA);
			}

			*strchr(value, '|') = '\0';

			if (installString2Rel(value) == rel_unknown)
			{
				fprintf(stderr, "Malformed line (bad relation): %s\n", value);
				return (status = ERR_BADDATA);
			}
		}

		else if (!strcmp(line, INSTALL_OBSOLETE))
		{
			memory += sizeof(installObsolete);
			numObsoletes += 1;

			if (!value)
			{
				fprintf(stderr, "Malformed line (no value): %s\n", line);
				return (status = ERR_BADDATA);
			}

			memory += (strlen(value) + 1);

			DEBUGMSG("Obsolescency %s\n", value);

			// Obsolescences are a triple, separated by '|' characters

			if (!strchr(value, '|'))
			{
				fprintf(stderr, "Malformed line (no relation): %s\n", value);
				return (status = ERR_BADDATA);
			}

			value = (strchr(value, '|') + 1);

			if (!strchr(value, '|'))
			{
				fprintf(stderr, "Malformed line (no version): %s\n", value);
				return (status = ERR_BADDATA);
			}

			*strchr(value, '|') = '\0';

			if (installString2Rel(value) == rel_unknown)
			{
				fprintf(stderr, "Malformed line (bad relation): %s\n", value);
				return (status = ERR_BADDATA);
			}
		}

		else
		{
			// Assume this is a file entry.  Need to count strings on either
			// side of any '=' character

			memory += sizeof(installFile);
			numFiles += 1;

			// Add the LHS
			memory += (strlen(line) + 1);

			DEBUGMSG("File %s\n", line);

			if (value)
				// Add the RHS
				memory += (strlen(value) + 1);
		}
	}

	// Allocate the installInfo structure
	*info = calloc(1, sizeof(installInfo));
	if (!*info)
	{
		fprintf(stderr, "Memory error\n");
		return (status = ERR_MEMORY);
	}

	// Get enough memory to contain the file entries and strings
	(*info)->data = calloc(memory, 1);
	if (!(*info)->data)
	{
		fprintf(stderr, "Memory error\n");
		free(*info);
		*info = NULL;
		return (status = ERR_MEMORY);
	}

	(*info)->depend = (*info)->data;
	(*info)->obsolete = ((void *) (*info)->depend + (numDepends *
		sizeof(installDepend)));
	(*info)->file = ((void *) (*info)->obsolete + (numObsoletes *
		sizeof(installObsolete)));
	stringData = ((void *) (*info)->file + (numFiles * sizeof(installFile)));
	numDepends = 0;
	numObsoletes = 0;
	numFiles = 0;

	// Second pass - construct the installInfo
	ptr = buffer;
	while ((unsigned)(ptr - buffer) < bufferLen)
	{
		if (!readLine(&ptr, (bufferLen - (unsigned)(ptr - buffer)), line,
			sizeof(line)))
		{
			break;
		}

		// If there's a '=' character, separate the strings
		value = strchr(line, '=');
		if (value)
		{
			*value = '\0';
			value += 1;
		}

		if (!strcmp(line, INSTALL_NAME) ||
			!strcmp(line, INSTALL_DESC) ||
			!strcmp(line, INSTALL_VERSION) ||
			!strcmp(line, INSTALL_ARCH) ||
			!strcmp(line, INSTALL_PREEXEC) ||
			!strcmp(line, INSTALL_POSTEXEC))
		{
			if (!strcmp(line, INSTALL_VERSION) ||
				!strcmp(line, INSTALL_ARCH))
			{
				if (!strcmp(line, INSTALL_VERSION))
					strncpy((*info)->version, value, INSTALL_VERSION_MAX);
				else if (!strcmp(line, INSTALL_ARCH))
					strncpy((*info)->arch, value, INSTALL_ARCH_MAX);

				continue;
			}

			if (!strcmp(line, INSTALL_NAME))
				(*info)->name = stringData;
			else if (!strcmp(line, INSTALL_DESC))
				(*info)->desc = stringData;
			else if (!strcmp(line, INSTALL_PREEXEC))
				(*info)->preExec = stringData;
			else if (!strcmp(line, INSTALL_POSTEXEC))
				(*info)->postExec = stringData;

			strcpy(stringData, value);
			stringData += (strlen(value) + 1);
		}

		else if (!strcmp(line, INSTALL_OBSOLETE))
		{
			// Obsolescences are a triple, separated by '|' characters

			(*info)->obsolete[numObsoletes].name = stringData;

			*strchr(value, '|') = '\0';
			strcpy(stringData, value);
			stringData += (strlen(value) + 1);

			value += (strlen(value) + 1);
			*strchr(value, '|') = '\0';

			(*info)->obsolete[numObsoletes].rel = installString2Rel(value);
			if ((*info)->obsolete[numObsoletes].rel == rel_unknown)
			{
				fprintf(stderr, "Malformed line (bad relation): %s\n", value);
				return (status = ERR_BADDATA);
			}

			value += (strlen(value) + 1);

			strcpy((*info)->obsolete[numObsoletes].version, value);

			numObsoletes += 1;
		}

		else if (!strcmp(line, INSTALL_DEPEND))
		{
			// Dependencies are a triple, separated by '|' characters

			(*info)->depend[numDepends].name = stringData;

			*strchr(value, '|') = '\0';
			strcpy(stringData, value);
			stringData += (strlen(value) + 1);

			value += (strlen(value) + 1);
			*strchr(value, '|') = '\0';

			(*info)->depend[numDepends].rel = installString2Rel(value);
			if ((*info)->depend[numObsoletes].rel == rel_unknown)
			{
				fprintf(stderr, "Malformed line (bad relation): %s\n", value);
				return (status = ERR_BADDATA);
			}

			value += (strlen(value) + 1);

			strcpy((*info)->depend[numDepends].version, value);

			numDepends += 1;
		}

		else
		{
			// Assume this is a file listing.  Need to record strings from
			// either side of any '=' character

			(*info)->file[numFiles].archiveName = stringData;

			strcpy(stringData, line);
			stringData += (strlen(line) + 1);

			if (value)
			{
				(*info)->file[numFiles].targetName = stringData;

				strcpy(stringData, value);
				stringData += (strlen(value) + 1);
			}

			numFiles += 1;
		}
	}

	(*info)->numDepends = numDepends;
	(*info)->numObsoletes = numObsoletes;
	(*info)->numFiles = numFiles;

	return (status = 0);
}


static int makeTmpFile(char **tmpFileName)
{
	int status = 0;
	int tmpFd = 0;

	// Get the name for the temporary file
	*tmpFileName = strdup(INSTALL_TMP_NAMEFORMAT);
	if (!*tmpFileName)
	{
		fprintf(stderr, "Memory error\n");
		return (status = ERR_MEMORY);
	}

	// Get a temporary file
	tmpFd = mkstemp(*tmpFileName);
	if (tmpFd < 0)
	{
		fprintf(stderr, "Couldn't create temporary file\n");
		free(*tmpFileName); *tmpFileName = NULL;
		return (status = ERR_NOCREATE);
	}

	DEBUGMSG("Created temporary file %s\n", *tmpFileName);

	// We don't write to it directly, so close
	close(tmpFd);

	return (status = 0);
}


static int copyFileData(FILE *inStream, FILE *outStream, unsigned totalBytes)
{
	int status = 0;
	unsigned maxBytes = 0;
	unsigned char *buffer = NULL;
	unsigned doneBytes = 0;

	DEBUGMSG("Copy file data\n");

	maxBytes = min(totalBytes, INSTALL_MAX_BUFFERSIZE);

	buffer = calloc(maxBytes, 1);
	if (!buffer)
	{
		fprintf(stderr, "Memory error\n");
		return (status = ERR_MEMORY);
	}

	while (doneBytes < totalBytes)
	{
		maxBytes = min(maxBytes, (totalBytes - doneBytes));

		DEBUGMSG("Reading %u bytes\n", maxBytes);

		if (fread(buffer, 1, maxBytes, inStream) < maxBytes)
		{
			fprintf(stderr, "Error reading\n");
			status = ERR_IO;
			break;
		}

		DEBUGMSG("Writing %u bytes\n", maxBytes);
		if (fwrite(buffer, 1, maxBytes, outStream) < maxBytes)
		{
			fprintf(stderr, "Error writing\n");
			status = ERR_IO;
			break;
		}

		doneBytes += maxBytes;
	}

	free(buffer);

	return (status);
}


static char *makeFilePath(const char *targetRootName, const char *fileName)
{
	char *path = NULL;
	int count;

	path = calloc((strlen(targetRootName) + strlen(fileName) + 2), 1);
	if (!path)
	{
		fprintf(stderr, "Memory error\n");
		return (path);
	}

	// Copy the root part
	strcpy(path, targetRootName);

	// Remove any trailing '/' characters
	for (count = (strlen(path) - 1); (count >= 0) && (path[count] == '/');
		count --)
	{
		path[count] = '\0';
	}

	// Append our own '/'
	strcat(path, "/");

	// Remove any leading '/' characters
	for (count = 0; (count < (int) strlen(fileName)) &&
		(fileName[count] == '/'); count ++)
	{
		fileName += 1;
	}

	// Copy the file name part
	strcat(path, fileName);

	return (path);
}


static char *makeInstallDbFileName(const char *targetRootName)
{
	// Construct the filename of the installation database file

	char *installDbFileName = NULL;

	installDbFileName = makeFilePath((targetRootName? targetRootName : "/"),
		INSTALL_DB_PATH);

	return (installDbFileName);
}


static FILE *openInstallDb(const char *installDbFileName, const char *mode)
{
	// This will check open the installation database in the requested mode.
	// If it does not yet exist, it will be created.

	struct stat st;
	FILE *outStream = NULL;
	FILE *inStream = NULL;

	DEBUGMSG("Open installation database %s\n", installDbFileName);

	// Does the file exist?
	if (!stat(installDbFileName, &st))
	{
		// Yes - open it
		outStream = fopen(installDbFileName, mode);
		if (!outStream)
			fprintf(stderr, "Couldn't open %s\n", installDbFileName);
	}
	else
	{
		// No - create it.

		DEBUGMSG("Create installation database %s\n", installDbFileName);

		outStream = fopen(installDbFileName, "w+");
		if (!outStream)
		{
			fprintf(stderr, "Couldn't open %s\n", installDbFileName);
			return (outStream);
		}
	}

	if (inStream)
		fclose(inStream);

	return (outStream);
}


static int makeDirRecursive(char *path)
{
	int status = 0;
	struct stat st;
	char *parent = NULL;

	DEBUGMSG("Create directory %s\n", path);

	status = stat(path, &st);
	if (status >= 0)
		return (status);

	parent = dirname(path);
	if (!parent)
		return (status = ERR_NOSUCHENTRY);

	status = makeDirRecursive(parent);

#ifndef PORTABLE
	free(parent);
#endif

	if (status < 0)
		return (status);

	status = mkdir(path, 0755);
	if (status < 0)
		return (status = errno);

	return (status = 0);
}


static int removeDirRecursive(const char *path)
{
	int status = 0;
	DIR *dir = NULL;
	struct dirent entry;
	struct dirent *result;
	char *childPath = NULL;

	DEBUGMSG("Remove directory %s\n", path);

	dir = opendir(path);
	if (!dir)
		return (status = ERR_NOSUCHDIR);

	// Recursively remove the directory contents
	while (1)
	{
		status = readdir_r(dir, &entry, &result);
		if (status)
			return (status = errno);

		if (!result)
			break;

		if (!strcmp(result->d_name, ".") || !strcmp(result->d_name, ".."))
			continue;

		childPath = calloc((strlen(path) + strlen(result->d_name) + 2),	1);
		if (!childPath)
			return (status = ERR_MEMORY);

		sprintf(childPath, "%s/%s", path, result->d_name);

		if (result->d_type == DT_DIR)
		{
			status = removeDirRecursive(childPath);

			free(childPath);

			if (status < 0)
				return (status);
		}
		else
		{
			status = remove(childPath);

			free(childPath);

			if (status < 0)
				return (status = errno);
		}
	}

	// Remove the directory itself
	status = rmdir(path);
	if (status < 0)
		return (errno = status);

	return (status = 0);
}


static int copyFile(const char *dest, const char *src)
{
	int status = 0;
	struct stat st;
	FILE *inStream = NULL;
	char *outDir = NULL;
	FILE *outStream = NULL;

	DEBUGMSG("Copy file %s to %s\n", src, dest);

	if (stat(src, &st) < 0)
	{
		fprintf(stderr, "Couldn't open %s\n", src);
		status = ERR_NOSUCHFILE;
		goto out;
	}

	// Open the source
	inStream = fopen(src, "r");
	if (!inStream)
	{
		fprintf(stderr, "Couldn't open %s\n", src);
		status = errno;
		goto out;
	}

	// Make sure the destination directory exists

	outDir = dirname((char *) dest);
	if (!outDir)
	{
		fprintf(stderr, "Couldn't determine destination directory of %s\n",
			dest);
		status = errno;
		goto out;
	}

	status = makeDirRecursive(outDir);
	if (status < 0)
	{
		fprintf(stderr, "Couldn't create destination directory of %s\n",
			dest);
		goto out;
	}

	// Open the destination
	outStream = fopen(dest, "w+");
	if (!outStream)
	{
		fprintf(stderr, "Couldn't open %s\n", dest);
		status = errno;
		goto out;
	}

	status = copyFileData(inStream, outStream, st.st_size);

out:
#ifndef PORTABLE
	if (outDir)
		free(outDir);
#endif

	if (outStream)
		fclose(outStream);

	if (inStream)
		fclose(inStream);

	return (status);
}


static int databaseRead(FILE *inStream, installInfo **info)
{
	// Read the next package info from the opened installation database

	int status = 0;
	installPackageHeader *header = NULL;

	DEBUGMSG("Read package from database\n");

	// Are we at the end of the database?
	if (feof(inStream))
		return (status = ERR_NOSUCHENTRY);

	// Read a header
	header = readHeader(inStream);
	if (!header)
	{
		status = ERR_NODATA;
		goto out;
	}

	// Check and interpret the header
	status = installHeaderInfoGet((unsigned char *) header, info);
	if (status < 0)
		goto out;

	DEBUGMSG("Got package %s from database\n", (*info)->name);

	status = 0;

out:
	if (header)
		free(header);

	if (status < 0)
		errno = status;

	return (status);
}


static int databaseWrite(FILE *outStream, installInfo *info)
{
	// Append the package info to the opened installation database

	int status = 0;
	installPackageHeader *header = NULL;

	DEBUGMSG("Add package %s to database\n", info->name);

	// Convert the installInfo to a header
	status = info2Header(info, &header);
	if (status < 0)
		goto out;

	// Check the header
	status = checkHeader(header);
	if (status < 0)
		goto out;

	// Write the header
	if (fwrite(header, 1, header->headerLen, outStream) < header->headerLen)
	{
		status = errno;
		fprintf(stderr, "Couldn't write file header\n");
		goto out;
	}

	status = 0;

out:
	if (header)
		free(header);

	if (status < 0)
		errno = status;

	return (status);
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////

int installHeaderInfoGet(unsigned char *_header, installInfo **info)
{
	// Return information from the header in the form of an installInfo
	// structure.

	int status = 0;
	installPackageHeader *header = (installPackageHeader *) _header;

	// Check params
	if (!header || !info)
	{
		fprintf(stderr, "NULL parameter\n");
		status = ERR_NULLPARAMETER;
		goto out;
	}

	DEBUGMSG("Get install info from header\n");

	*info = NULL;

	// Check the header
	status = checkHeader(header);
	if (status < 0)
		goto out;

	// Interpret the header
	status = header2Info(header, info);

out:
	if (status < 0)
		errno = status;

	return (status);
}


int installPackageInfoGet(const char *inFileName, installInfo **info)
{
	// Read a package file, and return information in the form of an
	// installInfo structure.  This is a wrapper around installInfoGet().

	int status = 0;
	FILE *inStream = NULL;
	installPackageHeader *header = NULL;

	// Check params
	if (!inFileName || !info)
	{
		fprintf(stderr, "NULL parameter\n");
		status = ERR_NULLPARAMETER;
		goto out;
	}

	DEBUGMSG("Get install info from %s\n", inFileName);

	*info = NULL;

	// Open the file
	inStream = fopen(inFileName, "r");
	if (!inStream)
	{
		fprintf(stderr, "Couldn't open %s\n", inFileName);
		status = ERR_NOSUCHFILE;
		goto out;
	}

	// Read the header
	header = readHeader(inStream);
	if (!header)
	{
		status = ERR_NODATA;
		goto out;
	}

	// Call the wrapped function to check and interpret it
	status = installHeaderInfoGet((unsigned char *) header, info);

out:
	if (header)
		free(header);

	if (inStream)
		fclose(inStream);

	if (status < 0)
		errno = status;

	return (status);
}


void installInfoFree(installInfo *info)
{
	if (info)
	{
		if (info->data)
			free(info->data);

		free(info);
	}
}


void installVersion2String(int *version, char *string)
{
	int count;

	sprintf(string, "%d", version[0]);

	for (count = 1; count < INSTALL_VERSION_FIELDS; count ++)
	{
		if (version[count] == -1)
			break;

		sprintf((string + strlen(string)), ".%d", version[count]);
	}
}


void installString2Version(const char *string, int *version)
{
	int count;

	for (count = 0; count < INSTALL_VERSION_FIELDS; count ++)
		version[count] = -1;

	for (count = 0; count < INSTALL_VERSION_FIELDS; count ++)
	{
		if (strlen(string))
		{
			version[count] = atoi(string);

			string = strchr(string, '.');
			if (!string)
				break;

			string += 1;
		}
	}
}


const char *installArch2String(installArch arch)
{
	switch (arch)
	{
		case arch_all:
			return (INSTALL_ARCH_ALL);

		case arch_x86:
			return (INSTALL_ARCH_X86);

		case arch_x86_64:
			return (INSTALL_ARCH_X86_64);

		default:
			return (INSTALL_ARCH_UNKNOWN);
	}
}


installArch installString2Arch(const char *string)
{
	if (!strcmp(string, INSTALL_ARCH_ALL))
		return (arch_all);
	else if (!strcmp(string, INSTALL_ARCH_X86))
		return (arch_x86);
	else if (!strcmp(string, INSTALL_ARCH_X86_64))
		return (arch_x86_64);
	else
		return (arch_unknown);
}


const char *installRel2String(installRel rel)
{
	switch (rel)
	{
		case rel_less:
			return ("<");

		case rel_lessEqual:
			return ("<=");

		case rel_equal:
			return ("=");

		case rel_greaterEqual:
			return (">=");

		case rel_greater:
			return (">");

		default:
			return (NULL);
	}
}


installRel installString2Rel(const char *string)
{
	// Check params
	if (!string)
	{
		fprintf(stderr, "NULL parameter\n");
		return (rel_unknown);
	}

	if (!strcmp(string, "<"))
		return (rel_less);
	else if (!strcmp(string, "<="))
		return (rel_lessEqual);
	else if (!strcmp(string, "="))
		return (rel_equal);
	else if (!strcmp(string, ">="))
		return (rel_greaterEqual);
	else if (!strcmp(string, ">"))
		return (rel_greater);

	return (rel_unknown);
}


int installVersionIsNewer(int *version1, int *version2)
{
	// Returns 1 if version1 is greater than version2

	int count;

	// Check params
	if (!version1 || !version2)
	{
		fprintf(stderr, "NULL parameter\n");
		errno = ERR_NULLPARAMETER;
		return (0);
	}

	for (count = 0; count < INSTALL_VERSION_FIELDS; count ++)
	{
		if (version1[count] > version2[count])
			return (1);
	}

	return (0);
}


int installVersionStringIsNewer(const char *version1, const char *version2)
{
	// Returns 1 if version1 is greater than version2

	int version1Num[INSTALL_VERSION_FIELDS];
	int version2Num[INSTALL_VERSION_FIELDS];

	// Check params
	if (!version1 || !version2)
	{
		fprintf(stderr, "NULL parameter\n");
		errno = ERR_NULLPARAMETER;
		return (0);
	}

	// Get the versions in numeric format
	installString2Version(version1, version1Num);
	installString2Version(version2, version2Num);

	return (installVersionIsNewer(version1Num, version2Num));
}


int installManifestBufferInfoCreate(char *buffer, unsigned bufferLen,
	installInfo **info)
{
	// Create an installInfo structure from the manifest text supplied in the
	// buffer.

	int status = 0;

	// Check params
	if (!buffer || !info)
	{
		fprintf(stderr, "NULL parameter\n");
		status = ERR_NULLPARAMETER;
		goto out;
	}

	*info = NULL;

	DEBUGMSG("Create install info\n");

	status = manifest2Info(buffer, bufferLen, info);

out:
	if (status < 0)
	{
		if (*info)
		{
			installInfoFree(*info);
			*info = NULL;
		}

		errno = status;
	}

	return (status);
}


int installManifestFileInfoCreate(const char *inFileName, installInfo **info)
{
	// Read a 'manifest' file, and attempt to create an installInfo
	// structure.  This is a wrapper around installManifestInfoCreate().

	int status = 0;
	struct stat st;
	int inFd = 0;
	char *buffer = NULL;

	// Check params
	if (!inFileName || !info)
	{
		fprintf(stderr, "NULL parameter\n");
		status = ERR_NULLPARAMETER;
		goto out;
	}

	DEBUGMSG("Create install info from %s\n", inFileName);

	*info = NULL;

	// We need the file size
	status = stat(inFileName, &st);
	if (status < 0)
	{
		fprintf(stderr, "Couldn't stat() %s\n", inFileName);
		status = errno;
		goto out;
	}

	// Open the file
	inFd = open(inFileName, O_RDONLY);
	if (inFd <= 0)
	{
		fprintf(stderr, "Couldn't open %s\n", inFileName);
		status = ERR_NOSUCHFILE;
		goto out;
	}

	// Try to get memory
	buffer = calloc(st.st_size, 1);
	if (!buffer)
	{
		fprintf(stderr, "Memory error\n");
		status = ERR_MEMORY;
		goto out;
	}

	// Read in the whole file
	if (read(inFd, buffer, st.st_size) < (ssize_t) st.st_size)
	{
		fprintf(stderr, "Couldn't read %s\n", inFileName);
		status = errno;
		goto out;
	}

	// Call the wrapped function to check and interpret it
	status = installManifestBufferInfoCreate(buffer, st.st_size, info);

out:
	if (buffer)
		free(buffer);

	if (inFd > 0)
		close(inFd);

	if (status < 0)
		errno = status;

	return (status);
}


int installArchiveCreate(installInfo *info, char **archiveFileName)
{
	// Create an archive of the package files

	int status = 0;
	char *tarFileName = NULL;
	char *gzipFileName = NULL;
	char *archiveName = NULL;
#ifdef PORTABLE
	char command[512];
#endif
	int count;

	// Check params
	if (!info || !archiveFileName)
	{
		fprintf(stderr, "NULL parameter\n");
		status = ERR_NULLPARAMETER;
		goto out;
	}

	DEBUGMSG("Create archive file for %s\n", info->name);

	// Get a temporary file for tar-ing
	status = makeTmpFile(&tarFileName);
	if (status < 0)
		goto out;

	// Get a temporary file for gzip-ing
	status = makeTmpFile(&gzipFileName);
	if (status < 0)
		goto out;

	// Create a .tar archive of all the files
	for (count = 0; count < info->numFiles; count ++)
	{
		archiveName = info->file[count].archiveName;

		// Things that end with a '/' character are presumed to be
		// directories.  They go in the files list, but don't get added to the
		// archive.
		if (archiveName[strlen(archiveName) - 1] != '/')
		{
#ifdef PORTABLE
			snprintf(command, sizeof(command), "tar rf %s %s", tarFileName,
				 archiveName);
			if (system(command))
			{
				status = ERR_NOCREATE;
				goto out;
			}
#else
			status = archiveAddMember(archiveName, tarFileName,
				LOADERFILESUBCLASS_TAR, NULL /* comment */,
				NULL /* progress */);
			if (status < 0)
				goto out;
#endif
		}
	}

	// Gzip the archive
#ifdef PORTABLE
	snprintf(command, sizeof(command), "gzip -c9 %s > %s", tarFileName,
		gzipFileName);
	if (system(command))
	{
		status = ERR_NOCREATE;
		goto out;
	}
#else
	status = archiveAddMember(tarFileName, gzipFileName,
		LOADERFILESUBCLASS_GZIP, "Visopsys package files",
		NULL /* progress */);
	if (status < 0)
		goto out;
#endif

	*archiveFileName = gzipFileName;
	status = 0;

out:
	if (gzipFileName)
	{
		if (status < 0)
		{
			unlink(gzipFileName);
			free(gzipFileName);
		}
	}

	if (tarFileName)
	{
		unlink(tarFileName);
		free(tarFileName);
	}

	if (status < 0)
		errno = status;

	return (status);
}


#ifndef PORTABLE

int installArchiveExtract(const char *archiveFileName, char **filesDirName)
{
	// Extract an archive of the package files

	int status = 0;
	char cwd[512];
	char *tarFileName = NULL;
	char *tmpDirName = NULL;
	char *absTarFileName = NULL;

	memset(cwd, 0, sizeof(cwd));

	// Check params
	if (!archiveFileName || !filesDirName)
	{
		fprintf(stderr, "NULL parameter\n");
		status = ERR_NULLPARAMETER;
		goto out;
	}

	DEBUGMSG("Extract archive file %s\n", archiveFileName);

	// Get the current directory
	if (!getcwd(cwd, sizeof(cwd)))
	{
		fprintf(stderr, "Couldn't determine current directory\n");
		status = errno;
		goto out;
	}

	// Get a temporary file for unzip-ing
	status = makeTmpFile(&tarFileName);
	if (status < 0)
		goto out;

	status = archiveExtractMember(archiveFileName, NULL /* memberName */,
		0 /* memberIndex */, tarFileName, NULL /* progress */);
	if (status < 0)
		goto out;

	// Get the name for a temporary directory
	tmpDirName = strdup(INSTALL_TMP_NAMEFORMAT);
	if (!tmpDirName)
	{
		fprintf(stderr, "Memory error\n");
		status = ERR_MEMORY;
		goto out;
	}

	// Create the temporary directory
	if (!mkdtemp(tmpDirName))
	{
		fprintf(stderr, "Couldn't create temporary directory\n");
		status = ERR_NOCREATE;
		goto out;
	}

	DEBUGMSG("Created temporary directory %s\n", tmpDirName);

	// Change to the temporary directory
	status = chdir(tmpDirName);
	if (status < 0)
	{
		fprintf(stderr, "Couldn't change to temporary directory\n");
		status = errno;
		goto out;
	}

	// We need to refer to the tar file in the parent directory

	absTarFileName = calloc((strlen(cwd) + strlen(tarFileName) + 2), 1);
	if (!absTarFileName)
	{
		fprintf(stderr, "Memory error\n");
		status = ERR_MEMORY;
		goto out;
	}

	sprintf(absTarFileName, "%s/%s", cwd, tarFileName);

	status = archiveExtract(absTarFileName, NULL /* progress */);
	if (status < 0)
		goto out;

	*filesDirName = tmpDirName;
	status = 0;

out:
	if (absTarFileName)
		free(absTarFileName);

	if (cwd[0])
		chdir(cwd);

	if (tarFileName)
	{
		unlink(tarFileName);
		free(tarFileName);
	}

	if (tmpDirName)
	{
		if (status < 0)
		{
			removeDirRecursive(tmpDirName);
			free(tmpDirName);
		}
	}

	if (status < 0)
		errno = status;

	return (status);
}

#endif // !PORTABLE


int installWrapArchivePackage(installInfo *info, const char *archiveFileName,
	const char *outFileName)
{
	// Given an installInfo structure, and (usually) the name of an archive
	// file, create the binary software package file

	int status = 0;
	installPackageHeader *header = NULL;
	FILE *outStream = NULL;
	struct stat st;
	FILE *inStream = NULL;

	// Check params.  'archiveFileName' may be NULL.
	if (!info || !outFileName)
	{
		fprintf(stderr, "NULL parameter\n");
		status = ERR_NULLPARAMETER;
		goto out;
	}

	DEBUGMSG("Wrap software package file %s\n", outFileName);

	// Create the header
	status = info2Header(info, &header);
	if (status < 0)
		goto out;

	// Open/create/truncate the output file
	outStream = fopen(outFileName, "w");
	if (!outStream)
	{
		fprintf(stderr, "Couldn't open %s\n", outFileName);
		status = ERR_NOSUCHFILE;
		goto out;
	}

	// Write the header
	if (fwrite(header, 1, header->headerLen, outStream) < header->headerLen)
	{
		fprintf(stderr, "Couldn't write file header\n");
		status = ERR_IO;
		goto out;
	}

	if (archiveFileName)
	{
		// Get the size of the archive file
		status = stat(archiveFileName, &st);
		if (status < 0)
		{
			fprintf(stderr, "Couldn't stat() %s\n", archiveFileName);
			status = errno;
			goto out;
		}

		// Open the archive file
		inStream = fopen(archiveFileName, "r");
		if (!inStream)
		{
			fprintf(stderr, "Couldn't open %s\n", archiveFileName);
			status = ERR_NOSUCHFILE;
			goto out;
		}

		// Append the archive to the output file
		status = copyFileData(inStream, outStream, st.st_size);
		if (status < 0)
		{
			fprintf(stderr, "Couldn't copy %s\n", archiveFileName);
			goto out;
		}
	}

	status = 0;

out:
	if (inStream)
		fclose(inStream);

	if (outStream)
		fclose(outStream);

	if (header)
		free(header);

	if (status < 0)
		errno = status;

	return (status);
}


int installUnwrapPackageArchive(const char *inFileName, installInfo **info,
	char **archiveFileName)
{
	// Given a binary software package file, extract the installInfo struct
	// and the archive file

	int status = 0;
	struct stat st;
	FILE *inStream = NULL;
	installPackageHeader *header = NULL;
	char *outFileName = NULL;
	FILE *outStream = NULL;

	// Check params
	if (!inFileName || !info || !archiveFileName)
	{
		fprintf(stderr, "NULL parameter\n");
		status = ERR_NULLPARAMETER;
		goto out;
	}

	DEBUGMSG("Unwrap software package file %s\n", inFileName);

	*info = NULL;
	*archiveFileName = NULL;

	// We need the file size
	status = stat(inFileName, &st);
	if (status < 0)
	{
		fprintf(stderr, "Couldn't stat() %s\n", inFileName);
		status = errno;
		goto out;
	}

	// Open the input file
	inStream = fopen(inFileName, "r");
	if (!inStream)
	{
		fprintf(stderr, "Couldn't open %s\n", inFileName);
		status = ERR_NOSUCHFILE;
		goto out;
	}

	// Read the header
	header = readHeader(inStream);
	if (!header)
	{
		status = ERR_NODATA;
		goto out;
	}

	// Check and interpret the header
	status = installHeaderInfoGet((unsigned char *) header, info);
	if (status < 0)
		goto out;

	// Get a temporary file for the archive
	status = makeTmpFile(&outFileName);
	if (status < 0)
		goto out;

	// Open/create/truncate the archive output file
	outStream = fopen(outFileName, "w");
	if (!outStream)
	{
		fprintf(stderr, "Couldn't open %s\n", outFileName);
		status = ERR_NOSUCHFILE;
		goto out;
	}

	status = copyFileData(inStream, outStream, (st.st_size -
		header->headerLen));
	if (status < 0)
		goto out;

	*archiveFileName = outFileName;
	status = 0;

out:
	if (outStream)
		fclose(outStream);

	if (outFileName)
	{
		if (status < 0)
		{
			unlink(outFileName);
			free(outFileName);
		}
	}

	if (header)
		free(header);

	if (inStream)
		fclose(inStream);

	if (status < 0)
	{
		if (*info)
		{
			installInfoFree(*info);
			*info = NULL;
		}

		errno = status;
	}

	return (status);
}


int installDatabaseRead(installInfo ***infoArray, int *numInfos,
	const char *targetRootName)
{
	// Read the installation database, and return an array of pointers to
	// installInfo structs

	int status = 0;
	char *installDbFileName = NULL;
	FILE *inStream = NULL;
	installInfo *info = NULL;

	DEBUGMSG("Read installation database with root=%s\n", (targetRootName?
		targetRootName : "/"));

	// Check params.  'targetRootName' may be NULL (means current system root)
	if (!infoArray || !numInfos)
	{
		fprintf(stderr, "NULL parameter\n");
		status = ERR_NULLPARAMETER;
		goto out;
	}

	*infoArray = NULL;
	*numInfos = 0;

	// Construct the filename of the installation database
	installDbFileName = makeInstallDbFileName(targetRootName);
	if (!installDbFileName)
	{
		status = ERR_MEMORY;
		goto out;
	}

	DEBUGMSG("(install DB=%s)\n", installDbFileName);

	// Open the installation database for reading
	inStream = openInstallDb(installDbFileName, "r");
	if (!inStream)
	{
		status = ERR_NOSUCHFILE;
		goto out;
	}

	// Read headers
	while ((status = databaseRead(inStream, &info)) >= 0)
	{
		*infoArray = realloc(*infoArray, ((*numInfos + 1) *
			sizeof(installInfo *)));
		if (!*infoArray)
		{
			fprintf(stderr, "Memory error\n");
			status = ERR_MEMORY;
			goto out;
		}

		(*infoArray)[*numInfos] = info;
		*numInfos += 1;
	}

	status = 0;

out:
	if (inStream)
		fclose(inStream);

	if (installDbFileName)
		free(installDbFileName);

	if (status < 0)
		errno = status;

	return (status);
}


int installDatabaseWrite(installInfo **infoArray, int numInfos,
	const char *targetRootName)
{
	// Write the installation database, from an array of pointers to
	// installInfo structs

	int status = 0;
	char *installDbFileName = NULL;
	FILE *outStream = NULL;
	int count;

	DEBUGMSG("Write installation database with root=%s\n", (targetRootName?
		targetRootName : "/"));

	// Check params.  'targetRootName' may be NULL (means current system root)
	if (!infoArray)
	{
		fprintf(stderr, "NULL parameter\n");
		status = ERR_NULLPARAMETER;
		goto out;
	}

	// Construct the filename of the installation database
	installDbFileName = makeInstallDbFileName(targetRootName);
	if (!installDbFileName)
	{
		status = ERR_MEMORY;
		goto out;
	}

	DEBUGMSG("(install DB=%s)\n", installDbFileName);

	// Open the installation database for writing
	outStream = openInstallDb(installDbFileName, "w");
	if (!outStream)
	{
		status = ERR_NOSUCHFILE;
		goto out;
	}

	for (count = 0; count < numInfos; count ++)
	{
		status = databaseWrite(outStream, infoArray[count]);
		if (status < 0)
			goto out;
	}

	status = 0;

out:
	if (outStream)
		fclose(outStream);

	if (installDbFileName)
		free(installDbFileName);

	if (status < 0)
		errno = status;

	return (status);
}


int installCheck(installInfo *info, const char *targetRootName)
{
	// Given an installInfo struct and the name of a target root filesystem,
	// check whether we think that installation will be successful.

	int status = 0;
	char *installDbFileName = NULL;
	FILE *inStream = NULL;

	// Check params.  'targetRootName' may be NULL (means current system root)
	if (!info)
	{
		fprintf(stderr, "NULL parameter\n");
		status = ERR_NULLPARAMETER;
		goto out;
	}

	// Construct the filename of the installation database
	installDbFileName = makeInstallDbFileName(targetRootName);
	if (!installDbFileName)
	{
		status = ERR_MEMORY;
		goto out;
	}

	DEBUGMSG("Check software package '%s' with root=%s\n", info->name,
		(targetRootName? targetRootName : "/"));
	DEBUGMSG("(install DB=%s)\n", installDbFileName);

	// Open the installation database for reading
	inStream = openInstallDb(installDbFileName, "r");
	if (!inStream)
	{
		status = ERR_NOSUCHFILE;
		goto out;
	}

	// TODO: checking

	status = 0;

out:
	if (inStream)
		fclose(inStream);

	if (installDbFileName)
		free(installDbFileName);

	if (status < 0)
		errno = status;

	return (status);
}


int installPackageAdd(installInfo *info, const char *filesDirName,
	const char *targetRootName)
{
	// Given an installInfo struct, a directory containing the extracted files
	// archive, and the name of a target root filesystem, install the package.

	int status = 0;
	char cwd[512];
	char *installDbFileName = NULL;
	FILE *outStream = NULL;
	char *targetFileName = NULL;
	int count;

	// Check params.  'targetRootName' may be NULL (means current system root)
	if (!info || !filesDirName)
	{
		fprintf(stderr, "NULL parameter\n");
		status = ERR_NULLPARAMETER;
		goto out;
	}

	DEBUGMSG("Install software package '%s' with root=%s\n", info->name,
		(targetRootName? targetRootName : "/"));

	// Check the attempted installation
	status = installCheck(info, targetRootName);
	if (status < 0)
		goto out;

	// Get the current directory
	if (!getcwd(cwd, sizeof(cwd)))
	{
		fprintf(stderr, "Couldn't determine current directory\n");
		status = errno;
		goto out;
	}

	// Construct the filename of the installation database
	installDbFileName = makeInstallDbFileName(targetRootName);
	if (!installDbFileName)
	{
		status = ERR_MEMORY;
		goto out;
	}

	DEBUGMSG("(install DB=%s)\n", installDbFileName);

	// Open the installation database for writing/appending
	outStream = openInstallDb(installDbFileName, "a");
	if (!outStream)
	{
		status = ERR_NOSUCHFILE;
		goto out;
	}

	// Change to the files directory
	status = chdir(filesDirName);
	if (status < 0)
	{
		fprintf(stderr, "Couldn't change to files directory\n");
		status = errno;
		goto out;
	}

	// Are we supposed to run a pre-installation program?
	if (info->preExec)
	{
		status = system(info->preExec);
		if (status < 0)
			goto out;
	}

	// Copy files
	for (count = 0; count < info->numFiles; count ++)
	{
		if (!info->file[count].archiveName)
		{
			fprintf(stderr, "File archive name is NULL\n");
			status = ERR_NULLPARAMETER;
			goto out;
		}

		targetFileName = info->file[count].targetName;
		if (!targetFileName)
		{
			targetFileName = makeFilePath((targetRootName? targetRootName :
				"/"), info->file[count].archiveName);
			if (!targetFileName)
			{
				status = ERR_BADDATA;
				goto out;
			}
		}

		// File or directory?
		if (targetFileName[strlen(targetFileName) - 1] == '/')
			status = makeDirRecursive(targetFileName);
		else
			status = copyFile(targetFileName, info->file[count].archiveName);

		if (!info->file[count].targetName)
			free(targetFileName);

		if (status < 0)
			goto out;
	}

	// Change back to the original directory
	status = chdir(cwd);
	if (status)
	{
		status = errno;
		goto out;
	}

	// Are we supposed to run a post-installation program?
	if (info->postExec)
	{
		status = system(info->postExec);
		if (status < 0)
			goto out;
	}

	// Update the installation database
	status = databaseWrite(outStream, info);
	if (status < 0)
		goto out;

	// Clean up the files directory
	removeDirRecursive(filesDirName);

	status = 0;

out:
	if (cwd[0])
		if (chdir(cwd)) {}

	if (outStream)
		fclose(outStream);

	if (installDbFileName)
		free(installDbFileName);

	if (status < 0)
		errno = status;

	return (status);
}


int installPackageRemove(installInfo *info, const char *targetRootName)
{
	// Given an installInfo struct, and the name of a target root filesystem,
	// un-install the package.

	int status = 0;
	char *targetFileName = NULL;
	installInfo **databaseInfos = NULL;
	int numInfos = 0;
	int count1, count2;

	// Check params.  'targetRootName' may be NULL (means current system root)
	if (!info)
	{
		fprintf(stderr, "NULL parameter\n");
		status = ERR_NULLPARAMETER;
		goto out;
	}

	DEBUGMSG("Remove software package '%s' with root=%s\n", info->name,
		(targetRootName? targetRootName : "/"));

	// Try to delete files
	for (count1 = 0; count1 < info->numFiles; count1 ++)
	{
		if (!info->file[count1].archiveName)
		{
			fprintf(stderr, "File archive name is NULL\n");
			status = ERR_NULLPARAMETER;
			goto out;
		}

		targetFileName = info->file[count1].targetName;
		if (!targetFileName)
		{
			targetFileName = makeFilePath((targetRootName? targetRootName :
				"/"), info->file[count1].archiveName);
			if (!targetFileName)
			{
				status = ERR_BADDATA;
				goto out;
			}
		}

		// File or directory?
		if (targetFileName[strlen(targetFileName) - 1] != '/')
			unlink(targetFileName);

		if (!info->file[count1].targetName)
			free(targetFileName);
	}

	// Try to delete empty directories.  Go through the list backwards so that
	// child directories might get removed before we try to do parents.
	for (count1 = (info->numFiles - 1); count1 >= 0; count1 --)
	{
		if (!info->file[count1].archiveName)
		{
			fprintf(stderr, "File archive name is NULL\n");
			status = ERR_NULLPARAMETER;
			goto out;
		}

		targetFileName = info->file[count1].targetName;
		if (!targetFileName)
		{
			targetFileName = makeFilePath((targetRootName? targetRootName :
				"/"), info->file[count1].archiveName);
			if (!targetFileName)
			{
				status = ERR_BADDATA;
				goto out;
			}
		}

		// File or directory?
		if (targetFileName[strlen(targetFileName) - 1] == '/')
			rmdir(targetFileName);

		if (!info->file[count1].targetName)
			free(targetFileName);
	}

	// Try to remove the package from the database

	status = installDatabaseRead(&databaseInfos, &numInfos, targetRootName);
	if (status < 0)
		goto out;

	for (count1 = 0; count1 < numInfos; count1 ++)
	{
		if (!strcmp(databaseInfos[count1]->name, info->name))
		{
			installInfoFree(databaseInfos[count1]);

			numInfos -= 1;

			for (count2 = count1; count2 < numInfos; count2 ++)
				databaseInfos[count2] = databaseInfos[count2 + 1];

			status = installDatabaseWrite(databaseInfos, numInfos,
				targetRootName);

			break;
		}
	}

	for (count1 = 0; count1 < numInfos; count1 ++)
		installInfoFree(databaseInfos[count1]);

	free(databaseInfos);

out:
	if (status < 0)
		errno = status;

	return (status);
}

