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
//  kernelCryptHashSha1.c
//

// This file contains an implementation of the SHA1 one-way hashing algorithm,
// useful for passwords, checksums, and whatnot.
// Ref: https://en.wikipedia.org/wiki/SHA-1

#include "kernelCrypt.h"
#include "kernelError.h"
#include <string.h>
#include <sys/processor.h>

#define H0	0x67452301
#define H1	0xEFCDAB89
#define H2	0x98BADCFE
#define H3	0x10325476
#define H4	0xC3D2E1F0


static inline unsigned rol(unsigned x, unsigned n)
{
	return ((x << n) | (x >> (32 - n)));
}


static void hashChunk(const unsigned char *buffer, unsigned *hash)
{
	// Hash one 512-bit chunk

	unsigned w[80];
	unsigned a = hash[0];
	unsigned b = hash[1];
	unsigned c = hash[2];
	unsigned d = hash[3];
	unsigned e = hash[4];
	unsigned f, k, tmp;
	int count;

	// Break the chunk into 16 32-bit big-endian dwords in the work area
	for (count = 0; count < 16; count ++)
		w[count] = processorSwap32(((unsigned *) buffer)[count]);

	// Extend the 16 values into 80
	for (count = 16; count < 80; count ++)
	{
		w[count] = rol((w[count - 3] ^ w[count - 8] ^ w[count - 14] ^
			w[count - 16]), 1);
	}

	for (count = 0; count < 80; count ++)
	{
		if (count < 20)
		{
			f = ((b & c) | (~b & d));
			k = 0x5A827999;
		}
		else if (count < 40)
		{
			f = (b ^ c ^ d);
			k = 0x6ED9EBA1;
		}
		else if (count < 60)
		{
			f = ((b & c) | (b & d) | (c & d));
			k = 0x8F1BBCDC;
		}
		else
		{
			f = (b ^ c ^ d);
			k = 0xCA62C1D6;
		}

		tmp = (rol(a, 5) + f + e + k + w[count]);
		e = d;
		d = c;
		c = rol(b, 30);
		b = a;
		a = tmp;
	}

	// Add this chunk's hash to the result
	hash[0] += a;
	hash[1] += b;
	hash[2] += c;
	hash[3] += d;
	hash[4] += e;

	// Scrub
	memset(w, 0, sizeof(w));
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
//  Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////

int kernelCryptHashSha1(const unsigned char *message, unsigned len,
	unsigned char *output, int finalize, unsigned totalBytes)
{
	// Hash the message with the constant starting hash.  This is a wrapper
	// around kernelCryptHashSha1Cont(), which will check other parameters.

	int status = 0;

	// Check params
	if (!output)
	{
		kernelError(kernel_error, "NULL parameter");
		return (status = ERR_NULLPARAMETER);
	}

	// Fill in starting hash values, as big-endian dwords
	((unsigned *) output)[0] = processorSwap32(H0);
	((unsigned *) output)[1] = processorSwap32(H1);
	((unsigned *) output)[2] = processorSwap32(H2);
	((unsigned *) output)[3] = processorSwap32(H3);
	((unsigned *) output)[4] = processorSwap32(H4);

	return (status = kernelCryptHashSha1Cont(message, len, output,
		finalize, totalBytes));
}


int kernelCryptHashSha1Cont(const unsigned char *message, unsigned len,
	unsigned char *output, int finalize, unsigned totalBytes)
{
	// Hash the next message with the starting hash given in 'output'

	int status = 0;
	unsigned hash[5];
	unsigned char lastMessage[128];
	int count;

	// Check params
	if (!message || !output)
	{
		kernelError(kernel_error, "NULL parameter");
		return (status = ERR_NULLPARAMETER);
	}

	if (!finalize && (len % 64))
	{
		kernelError(kernel_error, "Non-final message must be a multiple of "
			"512 bits");
		return (status = ERR_RANGE);
	}

	// Turn the current hash into little-endian dwords
	for (count = 0; count < 5; count ++)
		hash[count] = processorSwap32(((unsigned *) output)[count]);

	// Do all the full chunks
	while (len >= 64)
	{
		hashChunk(message, hash);
		message += 64;
		len -= 64;
	}

	if (finalize)
	{
		// Do the last chunk(s)

		memset(lastMessage, 0, sizeof(lastMessage));
		memcpy(lastMessage, message, len);

		// Append a '1' bit
		lastMessage[len] = 0x80;

		// Append the length, in bits, as a 64-bit big-endian value (really
		// just 32 bits for our purposes) and hash
		if (len <= 55)
		{
			((unsigned *) lastMessage)[15] = processorSwap32(totalBytes << 3);
			hashChunk(lastMessage, hash);
		}
		else
		{
			((unsigned *) lastMessage)[31] = processorSwap32(totalBytes << 3);
			hashChunk(lastMessage, hash);
			hashChunk((lastMessage + 64), hash);
		}

		// Scrub
		memset(lastMessage, 0, len);
	}

	// Output the hash as big-endian dwords
	for (count = 0; count < 5; count ++)
		((unsigned *) output)[count] = processorSwap32(hash[count]);

	return (status = 0);
}

