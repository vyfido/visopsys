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
//  kernelCryptHashSha256.c
//

// This file contains an implementation of the SHA256 one-way hashing
// algorithm, useful for passwords, checksums, and whatnot.
// Ref: https://en.wikipedia.org/wiki/SHA-2

#include "kernelCrypt.h"
#include "kernelError.h"
#include <string.h>
#include <sys/processor.h>

#define H0	0x6a09e667
#define H1	0xbb67ae85
#define H2	0x3c6ef372
#define H3	0xa54ff53a
#define H4	0x510e527f
#define H5	0x9b05688c
#define H6	0x1f83d9ab
#define H7	0x5be0cd19

static unsigned k[64] = {
	0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
	0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
	0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
	0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
	0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
	0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
	0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
	0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
	0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
	0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
	0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
	0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
	0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
	0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
	0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
	0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};


static inline unsigned ror(unsigned x, unsigned n)
{
	return ((x << (32 - n)) | (x >> n));
}


static void hashChunk(const unsigned char *buffer, unsigned *hash)
{
	// Hash one 512-bit chunk

	unsigned w[64];
	unsigned a = hash[0];
	unsigned b = hash[1];
	unsigned c = hash[2];
	unsigned d = hash[3];
	unsigned e = hash[4];
	unsigned f = hash[5];
	unsigned g = hash[6];
	unsigned h = hash[7];
	unsigned s0, s1;
	unsigned ch, maj;
	unsigned tmp1, tmp2;
	int count;

	// Break the chunk into 16 32-bit big-endian dwords in the work area
	for (count = 0; count < 16; count ++)
		w[count] = processorSwap32(((unsigned *) buffer)[count]);

	// Extend the 16 values into 64
	for (count = 16; count < 64; count ++)
	{
		s0 = (ror(w[count - 15], 7) ^ ror(w[count - 15], 18) ^
			(w[count - 15] >> 3));
		s1 = (ror(w[count - 2], 17) ^ ror(w[count - 2], 19) ^
			(w[count - 2] >> 10));
        w[count] = (w[count - 16] + s0 + w[count - 7] + s1);
	}

	for (count = 0; count < 64; count ++)
	{
		s1 = (ror(e, 6) ^ ror(e, 11) ^ ror(e, 25));
        ch = ((e & f) ^ (~e & g));
        tmp1 = (h + s1 + ch + k[count] + w[count]);
        s0 = (ror(a, 2) ^ ror(a, 13) ^ ror(a, 22));
        maj = ((a & b) ^ (a & c) ^ (b & c));
		tmp2 = (s0 + maj);

		h = g;
        g = f;
        f = e;
        e = (d + tmp1);
        d = c;
        c = b;
        b = a;
        a = (tmp1 + tmp2);
	}

	// Add this chunk's hash to the result
	hash[0] += a;
	hash[1] += b;
	hash[2] += c;
	hash[3] += d;
	hash[4] += e;
	hash[5] += f;
	hash[6] += g;
	hash[7] += h;

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

int kernelCryptHashSha256(const unsigned char *message, unsigned len,
	unsigned char *output, int finalize, unsigned totalBytes)
{
	// Hash the message with the constant starting hash.  This is a wrapper
	// around kernelCryptHashSha256Cont(), which will check other parameters.

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
	((unsigned *) output)[5] = processorSwap32(H5);
	((unsigned *) output)[6] = processorSwap32(H6);
	((unsigned *) output)[7] = processorSwap32(H7);

	return (status = kernelCryptHashSha256Cont(message, len, output,
		finalize, totalBytes));
}


int kernelCryptHashSha256Cont(const unsigned char *message, unsigned len,
	unsigned char *output, int finalize, unsigned totalBytes)
{
	// Hash the next message with the starting hash given in 'output'

	int status = 0;
	unsigned hash[8];
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
	for (count = 0; count < 8; count ++)
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
	for (count = 0; count < 8; count ++)
		((unsigned *) output)[count] = processorSwap32(hash[count]);

	return (status = 0);
}

