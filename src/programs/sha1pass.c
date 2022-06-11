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
//  sha1pass.c
//

// Uses the kernel's built-in SHA1 hashing to create a digest string

/* This is the text that appears when a user requests help about this program
<help>

 -- sha1pass --

Calculate and print one or more SHA1 digests.  SHA1 is a one-way hashing
algorithm which can be used to calculate checksums or hash passwords.

Usage:
  sha1pass [string1] [string2] [...]

This command will print one line of SHA1 digest for each string parameter
supplied.  If no parameter is supplied, the digest will still be created,
but for an empty string (which is "da39a3ee5e6b4b0d3255bfef95601890afd80709").

</help>
*/

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/api.h>
#include <sys/crypt.h>


int main(int argc, char *argv[])
{
	int status = 0;
	unsigned char output[CRYPT_HASH_SHA1_BYTES];
	int count1, count2;

	// If no parameter is supplied, use an empty string.
	if (argc < 2)
	{
		argv[1] = "";
		argc = 2;
	}

	for (count1 = 1; count1 < argc; count1 ++)
	{
		status = cryptHashSha1((unsigned char *) argv[count1],
			strlen(argv[count1]), output, 1 /* finalize */,
			strlen(argv[count1]));
		if (status < 0)
		{
			errno = status;
			perror("cryptHashSha1");
			return (status);
		}

		for (count2 = 0; count2 < CRYPT_HASH_SHA1_BYTES; count2 ++)
			printf("%02x", output[count2]);

		printf("\n");
	}

	return (status = 0);
}

