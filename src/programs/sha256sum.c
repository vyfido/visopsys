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
//  sha256sum.c
//

// Uses the kernel's built-in SHA256 hashing to create a digest string

/* This is the text that appears when a user requests help about this program
<help>

 -- sha256sum --

Calculate and print one or more SHA256 digests.  SHA256 is a one-way hashing
algorithm which can be used to calculate checksums or hash passwords.

Usage:
  sha256sum <file1> [file2] [...]

This command will print one line of SHA256 digest for each file parameter
supplied.

</help>
*/

#include <errno.h>
#include <libintl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/api.h>
#include <sys/crypt.h>

#define _(string)	gettext(string)
#define BUFFERSIZE	1048576 // Multiple of 64


static void usage(char *name)
{
	printf(_("usage:\n%s <file1> [file2] [...]\n"), name);
}


int main(int argc, char *argv[])
{
	int status = 0;
	fileStream sumFileStream;
	unsigned sumBytes = 0;
	unsigned buffLen = 0;
	unsigned char *buffer = NULL;
	int first = 1;
	unsigned doBytes = 0;
	unsigned doneBytes = 0;
	unsigned char output[CRYPT_HASH_SHA256_BYTES];
	int count1, count2;

	if (argc < 2)
	{
		usage(argv[0]);
		return (status = ERR_ARGUMENTCOUNT);
	}

	for (count1 = 1; count1 < argc; count1 ++)
	{
		status = fileStreamOpen(argv[count1], OPENMODE_READ, &sumFileStream);
		if (status < 0)
		{
			errno = status;
			perror("fileStreamOpen");
			return (status);
		}

		// How many bytes to read?
		sumBytes = sumFileStream.size;

		// Get a buffer for reading, at least one byte (for empty files)
		buffLen = max(min(BUFFERSIZE, sumBytes), 1);
		buffer = calloc(buffLen, 1);
		if (!buffer)
		{
			perror("calloc");
			return (status);
		}

		first = 1;

		do
		{
			doBytes = min(buffLen, sumBytes);

			// Read data
			doneBytes = 0;
			if (doBytes)
			{
				status = fileStreamRead(&sumFileStream, doBytes, (char *)
					buffer);
				if (status < 0)
				{
					errno = status;
					perror("fileStreamRead");
					fileStreamClose(&sumFileStream);
					free(buffer);
					return (status);
				}

				doneBytes = status;
			}

			sumBytes -= doneBytes;

			// Hash
			if (first)
			{
				status = cryptHashSha256(buffer, doneBytes, output,
					(sumBytes == 0) /* finalize? */, sumFileStream.size);
				if (status < 0)
				{
					errno = status;
					perror("cryptHashSha256");
				}

				first = 0;
			}
			else
			{
				status = cryptHashSha256Cont(buffer, doneBytes, output,
					(sumBytes == 0) /* finalize? */, sumFileStream.size);
				if (status < 0)
				{
					errno = status;
					perror("cryptHashSha256Cont");
				}
			}

			// Scrub
			memset(buffer, 0, doneBytes);

			if (status < 0)
			{
				fileStreamClose(&sumFileStream);
				free(buffer);
				return (status);
			}

		} while (sumBytes);

		fileStreamClose(&sumFileStream);
		free(buffer);

		for (count2 = 0; count2 < CRYPT_HASH_SHA256_BYTES; count2 ++)
			printf("%02x", output[count2]);

		printf("  %s\n", argv[count1]);
	}

	return (status = 0);
}

