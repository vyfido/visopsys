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
//  store.h
//

// This file contains definitions and structures for communicating with the
// Visopsys software store

#ifndef _STORE_H
#define _STORE_H

#ifndef PORTABLE
	#include <sys/install.h>
#endif

#define STORE_SERVER_HOST			"visopsys.org"
#define STORE_SERVER_PORT			7673 // ("vs" in hex)
#define STORE_SERVER_CONNBACKLOG	10

#define STORE_HEADER_MAGIC			0x5653484D // 'VSHM' in hex
#define STORE_HEADER_VERSION		0x00010000
#define STORE_VERSION_FIELDS		INSTALL_VERSION_FIELDS
#define STORE_VERSION_MAX			INSTALL_VERSION_MAX

// Store requests
#define STORE_REQUEST_MAX			1024
#define STORE_REQUEST_PACKAGES		1
#define STORE_REQUEST_DOWNLOAD		2
#define STORE_REPLY_ERROR			(-1)

typedef struct {
	unsigned magic;
	unsigned len;
	unsigned version;
	int request;

} storeRequestHeader;

typedef struct {
	storeRequestHeader header;
	int osVersion[STORE_VERSION_FIELDS];
	installArch osArch;

} storeRequestPackages;

typedef struct {
	storeRequestHeader header;
	int numPackages;
	installPackageHeader packageHeader[];

} storeReplyPackages;

typedef struct {
	storeRequestHeader header;
	int osVersion[STORE_VERSION_FIELDS];
	installArch osArch;
	char name[];

} storeRequestDownload;

typedef struct {
	storeRequestHeader header;
	unsigned char data[];

} storeReplyDownload;

typedef struct {
	storeRequestHeader header;
	int error;
	char message[];

} storeReplyError;

#endif

