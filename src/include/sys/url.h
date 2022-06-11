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
//  url.h
//

// This file contains definitions and structures for using URLs in Visopsys.

#ifndef _URL_H
#define _URL_H

// URL scheme names
#define URL_SCHEME_NAME_UNKNOWN		"unknown"
#define URL_SCHEME_NAME_FILE		"file"
#define URL_SCHEME_NAME_FTP			"ftp"
#define URL_SCHEME_NAME_HTTP		"http"
#define URL_SCHEME_NAME_HTTPS		"https"

// Additional unsupported ones that we want to be able to recognize as valid
#define URL_SCHEME_NAME_ABOUT		"about"
#define URL_SCHEME_NAME_AFS			"afs"
#define URL_SCHEME_NAME_DATA		"data"
#define URL_SCHEME_NAME_DNS			"dns"
#define URL_SCHEME_NAME_FACETIME	"facetime"
#define URL_SCHEME_NAME_FAX			"fax"
#define URL_SCHEME_NAME_GOPHER		"gopher"
#define URL_SCHEME_NAME_IM			"im"
#define URL_SCHEME_NAME_IMAP		"imap"
#define URL_SCHEME_NAME_IRC			"irc"
#define URL_SCHEME_NAME_JMS			"jms"
#define URL_SCHEME_NAME_LDAP		"ldap"
#define URL_SCHEME_NAME_MAILSERVER	"mailserver"
#define URL_SCHEME_NAME_MAILTO		"mailto"
#define URL_SCHEME_NAME_NEWS		"news"
#define URL_SCHEME_NAME_NFS			"nfs"
#define URL_SCHEME_NAME_NNTP		"nntp"
#define URL_SCHEME_NAME_PKCS11		"pkcs11"
#define URL_SCHEME_NAME_POP			"pop"
#define URL_SCHEME_NAME_RSYNC		"rsync"
#define URL_SCHEME_NAME_SHTTP		"shttp"
#define URL_SCHEME_NAME_SKYPE		"skype"
#define URL_SCHEME_NAME_SMS			"sms"
#define URL_SCHEME_NAME_SNMP		"snmp"
#define URL_SCHEME_NAME_TEL			"tel"
#define URL_SCHEME_NAME_TELNET		"telnet"
#define URL_SCHEME_NAME_TFTP		"tftp"
#define URL_SCHEME_NAME_VNC			"vnc"

typedef enum {
	scheme_unknown = 0,
	scheme_file,
	scheme_ftp,
	scheme_http,
	scheme_https,
	scheme_unsupported

} urlScheme;

// Structure for describing the components of a URL
typedef struct {
	urlScheme scheme;
	char *user;
	char *password;
	char *host;
	int port;
	char *path;
	char *query;
	char *fragment;
	char *data;

} urlInfo;

#endif

