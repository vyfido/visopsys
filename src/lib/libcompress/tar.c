//
//  Visopsys
//  Copyright (C) 1998-2018 J. Andrew McLaughlin
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
//  tar.c
//

// This is common library code for the TAR file format.

#include "libcompress.h"
#include <errno.h>
#include <libgen.h>
#include <stdlib.h>
#include <string.h>
#include <sys/api.h>
#include <sys/loader.h>
#include <sys/stat.h>
#include <sys/tar.h>

#ifdef DEBUG
	#define DEBUG_OUTMAX	160
	int debugTar = 0;
	static char debugOutput[DEBUG_OUTMAX];

	static void DEBUGMSG(const char *message, ...)
	{
		va_list list;

		if (debugTar)
		{
			va_start(list, message);
			vsnprintf(debugOutput, DEBUG_OUTMAX, message, list);
			printf("%s", debugOutput);
		}
	}
#else
	#define DEBUGMSG(message, arg...) do { } while (0)
#endif


static unsigned tarMemberHeaderChecksum(tarHeader *header)
{
	unsigned checksum = 0;
	unsigned char *ptr = (unsigned char *) header;
	unsigned char *end = (ptr + sizeof(tarHeader));

	for ( ; ptr < end; ptr += 1)
	{
		if (ptr == (unsigned char *) header->checksum)
		{
			checksum += (8 * ' ');
			ptr += 8;
		}

		checksum += (unsigned) *ptr;
	}

	return (checksum);
}


static int tarReadMemberHeader(FILE *inStream, archiveMemberInfo *info)
{
	// Read the next member header of a TAR file, and return the relevant
	// info from it.

	int status = 0;
	tarHeader header;
	int prefixLen = 0;
	int nameLen = 0;
	int count;

	memset(&header, 0, sizeof(tarHeader));

	DEBUGMSG("Read TAR member header\n");

	// Record where the member starts
	info->startOffset = ftell(inStream);

	status = fread((void *) &header, sizeof(tarHeader), 1, inStream);
	if (status < 1)
		return (status = errno);

	// Look out for an empty block - indicates end of archive (actually 2 of
	// them
	for (count = 0; count < 512; count ++)
	{
		if (((char *) &header)[count])
			break;
	}

	if (count >= 512)
	{
		// Finished, we guess.  No more members.
		DEBUGMSG("End of TAR archive\n");

		// Try to put the file pointer back to the start of the NULL blocks
		fseek(inStream, info->startOffset, SEEK_SET);

		return (status = 0);
	}

	if (memcmp(header.magic, TAR_MAGIC, sizeof(TAR_MAGIC)) &&
		memcmp(header.magic, TAR_OLDMAGIC, sizeof(TAR_OLDMAGIC)))
	{
		fprintf(stderr, "Not a valid TAR entry\n");
		return (status = ERR_BADDATA);
	}

	if (tarMemberHeaderChecksum(&header) != strtoul(header.checksum, NULL,
		 8))
	{
		fprintf(stderr, "TAR entry checksum failure\n");
		return (status = ERR_BADDATA);
	}

	// Get the file name

	// The 'prefix' and 'name' are NULL-terminated -- unless they're full,
	// in which case they're not.  Bah!

	if (header.prefix[0])
	{
		if (header.prefix[TAR_MAX_PREFIX - 1])
			prefixLen = TAR_MAX_PREFIX;
		else
			prefixLen = strlen(header.prefix);
	}

	if (header.name[TAR_MAX_NAMELEN - 1])
		nameLen = TAR_MAX_NAMELEN;
	else
		nameLen = strlen(header.name);

	DEBUGMSG("Member name length: %d\n", nameLen);

	info->name = malloc(nameLen + prefixLen + 1);
	if (!info->name)
	{
		fprintf(stderr, "Error allocating memory\n");
		status = ERR_MEMORY;
		goto out;
	}

	if (header.prefix[0])
	{
		strncpy(info->name, header.prefix, prefixLen);
		info->name[prefixLen] = '\0';
		strncat(info->name, header.name, nameLen);
	}
	else
	{
		strncpy(info->name, header.name, nameLen);
	}

	DEBUGMSG("Member file name: %s\n", info->name);

	// Directory, link?
	switch (header.typeFlag)
	{
		case TAR_TYPEFLAG_DIR:
			info->mode = (unsigned) dirT;
			break;

		case TAR_TYPEFLAG_SYMLINK:
			info->mode = (unsigned) linkT;
			break;

		default:
			info->mode = (unsigned) fileT;
			break;
	}

	// Get the modification time
	info->modTime = strtoul(header.modTime, NULL, 8);

	info->dataOffset = ftell(inStream);

	DEBUGMSG("Member data offset: %u\n", info->dataOffset);

	info->compressedDataSize = info->decompressedDataSize =
		strtoul(header.size, NULL, 8);

	DEBUGMSG("Member data size: %u\n", info->compressedDataSize);

	// Total size is aligned to a 512-byte coundary
	info->totalSize = (sizeof(tarHeader) + info->compressedDataSize);
	if (info->compressedDataSize & 0x1FF /* % 512 */)
		info->totalSize += (0x200 - (info->compressedDataSize & 0x1FF));

	DEBUGMSG("Member total size: %u\n", info->totalSize);

	// Return success
	status = 1;

out:
	if (status < 0)
	{
		archiveInfoContentsFree(info);
		errno = status;
	}

	return (status);
}


static int tarTerminate(FILE *outStream)
{
	unsigned char empty[512];

	memset(empty, 0, sizeof(empty));

	// Append two empty 512-blocks
	if ((fwrite(empty, sizeof(empty), 1, outStream) < 1) ||
		(fwrite(empty, sizeof(empty), 1, outStream) < 1))
	{
		fprintf(stderr, "Error writing empty blocks\n");
		return (ERR_IO);
	}

	return (0);
}


static int makeDirRecursive(char *path)
{
	int status = 0;
	char *parent = NULL;

	if (fileFind(path, NULL) >= 0)
		return (status = 0);

	parent = dirname(path);
	if (!parent)
		return (status = ERR_NOSUCHENTRY);

	status = makeDirRecursive(parent);

	free(parent);

	if (status < 0)
		return (status);

	return (status = fileMakeDir(path));
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////

int tarAddMember(const char *inFileName, const char *outFileName,
	progress *prog)
{
	int status = 0;
	struct stat st;
	loaderFileClass class;
	FILE *inStream = NULL;
	char constOutFileName[MAX_NAME_LENGTH];
	FILE *outStream = NULL;
	archiveMemberInfo info;
	tarHeader header;

	if (visopsys_in_kernel)
	{
		status = ERR_BUG;
		goto out;
	}

	// Check params.  It's OK for outFileName and prog to be NULL.
	if (!inFileName)
	{
		fprintf(stderr, "NULL parameter\n");
		status = ERR_NULLPARAMETER;
		goto out;
	}

	memset(&st, 0, sizeof(struct stat));
	memset(&class, 0, sizeof(loaderFileClass));
	memset(constOutFileName, 0, sizeof(constOutFileName));
	memset(&info, 0, sizeof(archiveMemberInfo));
	memset(&header, 0, sizeof(tarHeader));

	DEBUGMSG("TAR add %s\n", inFileName);

	// Stat() the file.
	status = stat(inFileName, &st);
	if (status < 0)
	{
		fprintf(stderr, "Couldn't stat() %s\n", inFileName);
		status = errno;
		goto out;
	}

	// We don't add anything but regular files and directories here
	if (st.st_mode && (st.st_mode != S_IFREG) && (st.st_mode != S_IFDIR))
	{
		DEBUGMSG("TAR skipping non-regular file/directory %s\n", inFileName);
		status = 0;
		goto out;
	}

	if (st.st_mode == S_IFREG)
	{
		// Open the input stream
		inStream = fopen(inFileName, "r");
		if (!inStream)
		{
			fprintf(stderr, "Couldn't open %s\n", inFileName);
			status = ERR_NOSUCHFILE;
			goto out;
		}
	}

	if (!outFileName)
	{
		// Create the output file name
		snprintf(constOutFileName, MAX_NAME_LENGTH, "%s.tar", inFileName);
		outFileName = constOutFileName;
	}

	// Open output stream
	outStream = fopen(outFileName, "a+");
	if (!outStream)
	{
		fprintf(stderr, "Couldn't open %s\n", outFileName);
		status = ERR_NOSUCHFILE;
		goto out;
	}

	// In Visopsys, the nonstandard "a+" mode doesn't automatically read from
	// the beginning of the file, so seek to it
	if (fseek(outStream, 0, SEEK_SET))
	{
		fprintf(stderr, "Couldn't seek %s\n", outFileName);
		status = ERR_IO;
		goto out;
	}

	// Loop through the members until we hit the end of the archive
	while (tarMemberInfo(outStream, &info, NULL /* progress */) >= 1)
		archiveInfoContentsFree(&info);

	// Add the member

	// Create the header

	strncpy(header.name, ((inFileName[0] == '/')? (inFileName + 1) :
		inFileName), TAR_MAX_NAMELEN);

	if ((st.st_mode == S_IFDIR) && strlen(inFileName) &&
		(inFileName[strlen(inFileName) - 1] != '/'))
	{
		strncat(header.name, "/", TAR_MAX_NAMELEN);
	}

	strncpy(header.mode, "0000644", 8);
	if (st.st_mode == S_IFDIR)
		strncpy(header.mode, "0000755", 8);

	snprintf(header.uid, 8, "%o", st.st_uid);
	snprintf(header.gid, 8, "%o", st.st_gid);

	if (st.st_mode == S_IFREG)
		snprintf(header.size, 12, "%o", st.st_size);

	snprintf(header.modTime, 12, "%o", st.st_mtime);

	header.typeFlag = TAR_TYPEFLAG_NORMAL;
	if (st.st_mode == S_IFDIR)
		header.typeFlag = TAR_TYPEFLAG_DIR;

	strncpy(header.magic, TAR_OLDMAGIC, 8);
	strncpy(header.uname, "user", 32);
	strncpy(header.gname, "group", 32);
	snprintf(header.checksum, 8, "%o", tarMemberHeaderChecksum(&header));

	// Write the header
	if (fwrite(&header, sizeof(tarHeader), 1, outStream) < 1)
	{
		fprintf(stderr, "Error writing %s\n", outFileName);
		status = ERR_IO;
		goto out;
	}

	if (st.st_mode == S_IFREG)
	{
		// Append the file data
		status = archiveCopyFileData(inStream, outStream, st.st_size, prog);
		if (status < 0)
			goto out;
	}

	memset(&header, 0, sizeof(tarHeader));

	// Extend it to a 512-block boundary, if necessary
	if (st.st_size & 0x1FF /* % 512 */)
	{
		if (fwrite(&header, (0x200 - (st.st_size & 0x1FF)), 1, outStream) < 1)
		{
			fprintf(stderr, "Error writing %s\n", outFileName);
			status = ERR_IO;
			goto out;
		}
	}

	// Append two empty 512-blocks
	status = tarTerminate(outStream);
	if (status < 0)
		goto out;

	status = 0;

out:
	if (outStream)
		fclose(outStream);

	if (inStream)
		fclose(inStream);

	if ((status < 0) && outStream)
		// Delete the incomplete output file
		fileDelete(outFileName);

	if (status < 0)
		errno = status;

	return (status);
}


int tarMemberInfo(FILE *inStream, archiveMemberInfo *info,
	progress *prog __attribute__((unused)))
{
	// Fill in the info structure from data pointed to by the current file
	// pointer.

	int status = 0;

	if (visopsys_in_kernel)
	{
		status = ERR_BUG;
		goto out;
	}

	// Check params.  It's OK for prog to be NULL.
	if (!inStream || !info)
	{
		fprintf(stderr, "NULL parameter\n");
		return (status = ERR_NULLPARAMETER);
	}

	memset(info, 0, sizeof(archiveMemberInfo));

	DEBUGMSG("TAR get member info\n");

	status = tarReadMemberHeader(inStream, info);
	if (status <= 0)
		goto out;

	// Seek to the end of the member
	status = fseek(inStream, (info->totalSize - sizeof(tarHeader)), SEEK_CUR);
	if (status < 0)
		goto out;

	// Return success
	status = 1;

out:
	if (status <= 0)
	{
		archiveInfoContentsFree(info);

		if (status < 0)
			errno = status;
	}

	return (status);
}


int tarExtractNextMember(FILE *inStream, progress *prog)
{
	// Extract the current member of a TAR file.

	int status = 0;
	archiveMemberInfo info;
	char *destDir = NULL;
	FILE *outStream = NULL;

	if (visopsys_in_kernel)
	{
		status = ERR_BUG;
		goto out;
	}

	// Check params.  It's OK for prog to be NULL.
	if (!inStream)
	{
		fprintf(stderr, "NULL parameter\n");
		status = ERR_NULLPARAMETER;
		goto out;
	}

	memset(&info, 0, sizeof(archiveMemberInfo));

	DEBUGMSG("TAR extract member\n");

	status = tarReadMemberHeader(inStream, &info);
	if (status <= 0)
		// Finished, we guess
		goto out;

	// Create output parent directory, if necessary
	destDir = dirname(info.name);
	if (destDir && strcmp(destDir, "."))
	{
		DEBUGMSG("TAR make parent directory %s\n", destDir);
		status = makeDirRecursive(destDir);
		if (status < 0)
			goto out;
	}

	if ((fileType) info.mode == dirT)
	{
		DEBUGMSG("TAR make directory %s\n", info.name);
		status = makeDirRecursive(info.name);
		if (status < 0)
			goto out;
	}
	else if ((fileType) info.mode == linkT)
	{
		// Ignore this for the time being
		DEBUGMSG("TAR ignoring link %s\n", info.name);
	}
	else
	{
		// Open output stream
		DEBUGMSG("TAR create file %s\n", info.name);
		outStream = fopen(info.name, "w");
		if (!outStream)
		{
			fprintf(stderr, "Couldn't open %s\n", info.name);
			status = ERR_NOCREATE;
			goto out;
		}

		if (info.compressedDataSize)
		{
			// Copy the data
			DEBUGMSG("TAR write %u bytes\n", info.compressedDataSize);
			status = archiveCopyFileData(inStream, outStream,
				info.compressedDataSize, prog);
		}

		fclose(outStream);

		if (status < 0)
		{
			// Delete the incomplete output file
			fileDelete(info.name);
			goto out;
		}
	}

	// Return success
	status = 1;

out:
	if (destDir)
		free(destDir);

	// Seek to the end of the member
	fseek(inStream, (info.startOffset + info.totalSize), SEEK_SET);

	archiveInfoContentsFree(&info);

	if (status < 0)
		errno = status;

	return (status);
}


int tarExtractMember(const char *inFileName, const char *memberName,
	int memberIndex, progress *prog)
{
	// Extract a member from a TAR file, either using the member name or the
	// index of the member -- a member name need not be unique, or it may not
	// be known

	int status = 0;
	FILE *inStream = NULL;
	archiveMemberInfo info;
	int memberCount;

	if (visopsys_in_kernel)
	{
		status = ERR_BUG;
		goto out;
	}

	// Check params.  It's OK for memberName and prog to be NULL.
	if (!inFileName)
	{
		fprintf(stderr, "NULL parameter\n");
		status = ERR_NULLPARAMETER;
		goto out;
	}

	if (memberName)
		DEBUGMSG("TAR extract %s from %s\n", memberName, inFileName);
	else
		DEBUGMSG("TAR extract member %d from %s\n", memberIndex, inFileName);

	inStream = fopen(inFileName, "r");
	if (!inStream)
	{
		fprintf(stderr, "Couldn't open %s\n", inFileName);
		status = ERR_NOSUCHFILE;
		goto out;
	}

	for (memberCount = 0; ; memberCount ++)
	{
		status = tarMemberInfo(inStream, &info, NULL);
		if (!status)
		{
			// No more entries
			fprintf(stderr, "Member not found\n");
			status = ERR_NOSUCHENTRY;
			break;
		}
		else if (status < 0)
		{
			fprintf(stderr, "Couldn't get member info\n");
			break;
		}

		if ((memberName && !strcmp(memberName, info.name)) ||
			(!memberName && (memberCount == memberIndex)))
		{
			// This is the one we're extracting
			DEBUGMSG("Found member to extract, offset %u\n", info.startOffset);

			fseek(inStream, info.startOffset, SEEK_SET);

			status = tarExtractNextMember(inStream, prog);

			archiveInfoContentsFree(&info);
			break;
		}

		archiveInfoContentsFree(&info);
	}

out:
	if (inStream)
		fclose(inStream);

	if (status < 0)
		errno = status;

	return (status);
}


int tarExtract(const char *inFileName, progress *prog)
{
	// Extract a TAR file.

	int status = 0;
	FILE *inStream = NULL;

	if (visopsys_in_kernel)
	{
		status = ERR_BUG;
		goto out;
	}

	// Check params.  It's OK for prog to be NULL.
	if (!inFileName)
	{
		fprintf(stderr, "NULL parameter\n");
		status = ERR_NULLPARAMETER;
		goto out;
	}

	DEBUGMSG("TAR extract %s\n", inFileName);

	inStream = fopen(inFileName, "r");
	if (!inStream)
	{
		fprintf(stderr, "Couldn't open %s\n", inFileName);
		status = ERR_NOSUCHFILE;
		goto out;
	}

	for (status = 1; status > 0; )
		status = tarExtractNextMember(inStream, prog);

	if (status < 0)
		goto out;

	// Return success
	status = 0;

out:
	if (inStream)
		fclose(inStream);

	if (status < 0)
		errno = status;

	return (status);
}


int tarDeleteMember(const char *inFileName, const char *memberName,
	int memberIndex, progress *prog)
{
	// Delete a member from a TAR file, either using the member name or the
	// index of the member -- a member name need not be unique, or it may not
	// be known

	int status = 0;
	FILE *inStream = NULL;
	char outFileName[MAX_PATH_NAME_LENGTH];
	FILE *outStream = NULL;
	archiveMemberInfo info;
	unsigned outputSize = 0;
	int deleted = 0;
	int memberCount;

	if (visopsys_in_kernel)
	{
		status = ERR_BUG;
		goto out;
	}

	// Check params.  It's OK for memberName and prog to be NULL.
	if (!inFileName)
	{
		fprintf(stderr, "NULL parameter\n");
		status = ERR_NULLPARAMETER;
		goto out;
	}

	if (memberName)
		DEBUGMSG("TAR delete %s from %s\n", memberName, inFileName);
	else
		DEBUGMSG("TAR delete member %d from %s\n", memberIndex, inFileName);

	inStream = fopen(inFileName, "r");
	if (!inStream)
	{
		fprintf(stderr, "Couldn't open %s\n", inFileName);
		status = ERR_NOSUCHFILE;
		goto out;
	}

	if (prog)
	{
		memset((void *) prog, 0, sizeof(progress));
		prog->numTotal = inStream->f.size;
	}

	// Open a temporary file for output
	sprintf(outFileName, "%s.tmp", inFileName);
	outStream = fopen(outFileName, "w");
	if (!outStream)
	{
		fprintf(stderr, "Couldn't open %s\n", outFileName);
		status = ERR_NOSUCHFILE;
		goto out;
	}

	DEBUGMSG("Using temporary file %s\n", outFileName);

	for (memberCount = 0; ; memberCount ++)
	{
		status = tarMemberInfo(inStream, &info, NULL);
		if (!status)
		{
			// Finished
			break;
		}
		else if (status < 0)
		{
			fprintf(stderr, "Couldn't get member info\n");
			goto out;
		}

		if (deleted || (memberName && strcmp(memberName, info.name)) ||
			(!memberName && (memberCount != memberIndex)))
		{
			// We're not deleting this one.  Write it out to the temporary
			// file.

			DEBUGMSG("Re-write member from offset %u\n", info.startOffset);

			fseek(inStream, info.startOffset, SEEK_SET);

			status = archiveCopyFileData(inStream, outStream, info.totalSize,
				NULL /* progress */);
			if (status < 0)
				goto out;

			outputSize += info.totalSize;
		}
		else
		{
			// This is the one we're deleting
			DEBUGMSG("Found member to delete, offset %u\n", info.startOffset);
			deleted = 1;
		}

		archiveInfoContentsFree(&info);

		if (prog)
		{
			prog->numFinished = ftell(inStream);
			prog->percentFinished = ((prog->numFinished * 100) /
				prog->numTotal);
			lockRelease(&prog->progLock);
		}
	}

	status = tarTerminate(outStream);
	if (status < 0)
		goto out;

	fclose(outStream);
	outStream = NULL;
	fclose(inStream);
	inStream = NULL;

	if (!deleted)
	{
		fprintf(stderr, "Member not found\n");
		status = ERR_NOSUCHENTRY;
		goto out;
	}

	// Replace the input file with the temporary file
	status = fileMove(outFileName, inFileName);

	// Did we just create an empty file?
	if (!outputSize)
		fileDelete(inFileName);

out:
	if (outStream)
		fclose(outStream);

	if (inStream)
		fclose(inStream);

	if (status < 0)
		fileDelete(outFileName);

	if (status < 0)
		errno = status;

	return (status);
}

