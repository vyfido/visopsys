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
//  kernelCrypt.h
//

// Declarations for any crypto/hashing code

#ifndef _KERNELCRYPT_H
#define _KERNELCRYPT_H

#include <sys/crypt.h>

int kernelCryptHashMd5(const unsigned char *, unsigned, unsigned char *);
int kernelCryptHashSha1(const unsigned char *, unsigned, unsigned char *,
	int, unsigned);
int kernelCryptHashSha1Cont(const unsigned char *, unsigned, unsigned char *,
	int, unsigned);
int kernelCryptHashSha256(const unsigned char *, unsigned, unsigned char *,
	int, unsigned);
int kernelCryptHashSha256Cont(const unsigned char *, unsigned,
	unsigned char *, int, unsigned);

#endif

