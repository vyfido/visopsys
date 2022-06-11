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
//  crypt.h
//

// This file contains public definitions and structures for hashing and
// cryptography.

#ifndef _CRYPT_H
#define _CRYPT_H

#define CRYPT_HASH_MD5_BITS			128
#define CRYPT_HASH_MD5_BYTES		(CRYPT_HASH_MD5_BITS >> 3)

#define CRYPT_HASH_SHA1_BITS		160
#define CRYPT_HASH_SHA1_BYTES		(CRYPT_HASH_SHA1_BITS >> 3)

#define CRYPT_HASH_SHA256_BITS		256
#define CRYPT_HASH_SHA256_BYTES		(CRYPT_HASH_SHA256_BITS >> 3)

#endif

