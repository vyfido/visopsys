//
//  Visopsys
//  Copyright (C) 1998-2021 J. Andrew McLaughlin
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
//  http.h
//

// This file contains definitions and structures for using the HTTP protocol
// and libhttp library in Visopsys.

#ifndef _HTTP_H
#define _HTTP_H

#include <sys/url.h>
#include <sys/vis.h>

// Requests
#define HTTP_REQUEST_GET					"GET"

// Variable names for parts of response status lines
#define HTTP_VERSION_VAR					"__http_version_var__"
#define HTTP_STATUSCODE_VAR					"__http_status_code__"
#define HTTP_STATUSSTRING_VAR				"__http_status_string__"

// Header fields
#define HTTP_HEADER_ACCEPT					"Accept"			// req
#define HTTP_HEADER_ACCEPTCHARSET			"Accept-Charset"	// req
#define HTTP_HEADER_ACCEPTDATETIME			"Accept-Datetime"	// req
#define HTTP_HEADER_ACCEPTENCODING			"Accept-Encoding"	// req
#define HTTP_HEADER_ACCEPTLANGUAGE			"Accept-Language"	// req
#define HTTP_HEADER_ACCEPTPATCH				"Accept-Patch"		// resp
#define HTTP_HEADER_ACCEPTRANGES			"Accept-Ranges"		// resp
#define HTTP_HEADER_ACCCTRLALLOWCREDS \
	"Access-Control-Allow-Credentials"							// resp
#define HTTP_HEADER_ACCCTRLALLOWHDRS \
	"Access-Control-Allow-Headers"								// resp
#define HTTP_HEADER_ACCCTRLALLOWMETHODS \
	"Access-Control-Allow-Methods"								// resp
#define HTTP_HEADER_ACCCTRLALLOWORIGIN \
	"Access-Control-Allow-Origin"								// resp
#define HTTP_HEADER_ACCCTRLEXPHDRS \
	"Access-Control-Expose-Headers"								// resp
#define HTTP_HEADER_ACCCTRLMAXAGE \
	"Access-Control-Max-Age"									// resp
#define HTTP_HEADER_ACCCTRLREQMETHOD \
	"Access-Control-Request-Method"								// req
#define HTTP_HEADER_ACCCTRLREQHEADERS \
	"Access-Control-Request-Headers"							// req
#define HTTP_HEADER_AGE						"Age"				// resp
#define HTTP_HEADER_ALLOW					"Allow"				// resp
#define HTTP_HEADER_ALTSVC					"Alt-Svc"			// resp
#define HTTP_HEADER_AUTHORIZATION			"Authorization"		// req
#define HTTP_HEADER_CACHECONTROL			"Cache-Control"		// req, resp
#define HTTP_HEADER_CONNECTION				"Connection"		// req, resp
#define HTTP_HEADER_CONTENTDISPOSITION \
	"Content-Disposition"										// resp
#define HTTP_HEADER_CONTENTENCODING			"Content-Encoding"	// resp
#define HTTP_HEADER_CONTENTLANGUAGE			"Content-Language"	// resp
#define HTTP_HEADER_CONTENTLENGTH			"Content-Length"	// req, resp
#define HTTP_HEADER_CONTENTLOCATION			"Content-Location"	// resp
#define HTTP_HEADER_CONTENTMD5				"Content-MD5"		// req, resp
#define HTTP_HEADER_CONTENTRANGE			"Content-Range"		// resp
#define HTTP_HEADER_CONTENTTYPE				"Content-Type"		// req, resp
#define HTTP_HEADER_COOKIE					"Cookie"			// req
#define HTTP_HEADER_DATE					"Date"				// req, resp
#define HTTP_HEADER_ETAG					"ETag"				// resp
#define HTTP_HEADER_EXPECT					"Expect"			// req
#define HTTP_HEADER_EXPIRES					"Expires"			// resp
#define HTTP_HEADER_FORWARDED				"Forwarded"			// req
#define HTTP_HEADER_FROM					"From"				// req
#define HTTP_HEADER_HOST					"Host"				// req
#define HTTP_HEADER_IFMATCH					"If-Match"			// req
#define HTTP_HEADER_IFMODIFIEDSINCE \
	"If-Modified-Since"											// req
#define HTTP_HEADER_IFNONEMATCH				"If-None-Match"		// req
#define HTTP_HEADER_IFRANGE					"If-Range"			// req
#define HTTP_HEADER_IFUNMODIFIEDSINCE \
	"If-Unmodified-Since"										// req
#define HTTP_HEADER_LASTMODIFIED			"Last-Modified"		// resp
#define HTTP_HEADER_LINK					"Link"				// resp
#define HTTP_HEADER_LOCATION				"Location"			// resp
#define HTTP_HEADER_MAXFORWARDS				"Max-Forwards"		// req
#define HTTP_HEADER_ORIGIN					"Origin"			// req
#define HTTP_HEADER_P3P						"P3P"				// resp
#define HTTP_HEADER_PRAGMA					"Pragma"			// req, resp
#define HTTP_HEADER_PROXYAUTHENTICATE \
	"Proxy-Authenticate"										// resp
#define HTTP_HEADER_PROXYAUTHORIZATION \
	"Proxy-Authorization"										// req
#define HTTP_HEADER_PUBLICKEYPINS			"Public-Key-Pins"	// resp
#define HTTP_HEADER_RANGE					"Range"				// req
#define HTTP_HEADER_REFERER					"Referer"			// req
#define HTTP_HEADER_RETRYAFTER				"Retry-After"		// resp
#define HTTP_HEADER_SERVER					"Server"			// resp
#define HTTP_HEADER_SETCOOKIE				"Set-Cookie"		// resp
#define HTTP_HEADER_STRICTTRANSSEC \
	"Strict-Transport-Security"									// resp
#define HTTP_HEADER_TE						"TE"				// req
#define HTTP_HEADER_TRAILER					"Trailer"			// resp
#define HTTP_HEADER_TRANSFERENCODING		"Transfer-Encoding"	// resp
#define HTTP_HEADER_TK						"Tk"				// resp
#define HTTP_HEADER_USERAGENT				"User-Agent"		// req
#define HTTP_HEADER_UPGRADE					"Upgrade"			// req, resp
#define HTTP_HEADER_VARY					"Vary"				// resp
#define HTTP_HEADER_VIA						"Via"				// req, resp
#define HTTP_HEADER_WARNING					"Warning"			// req, resp
#define HTTP_HEADER_WWWAUTHENTICATE			"WWW-Authenticate"	// resp
#define HTTP_HEADER_XFRAMEOPTIONS			"X-Frame-Options"	// resp

// Status codes

// 1xx Informational responses
#define HTTP_STATUSCODE_CONTINUE			100
#define HTTP_STATUSCODE_SWITCHINGPROTOCOLS	101
#define HTTP_STATUSCODE_PROCESSING			102
#define HTTP_STATUSCODE_EARLYHINTS			103
// 2xx Success
#define HTTP_STATUSCODE_OK					200
#define HTTP_STATUSCODE_CREATED				201
#define HTTP_STATUSCODE_ACCEPTED			202
#define HTTP_STATUSCODE_NONAUTHINFO			203
#define HTTP_STATUSCODE_NOCONTENT			204
#define HTTP_STATUSCODE_RESETCONTENT		205
#define HTTP_STATUSCODE_PARTIALCONTENT		206
#define HTTP_STATUSCODE_MULTISTATUS			207
#define HTTP_STATUSCODE_ALREADYREPORTED		208
#define HTTP_STATUSCODE_IMUSED				226
// 3xx Redirection
#define HTTP_STATUSCODE_MULTIPLECHOICES		300
#define HTTP_STATUSCODE_MOVEDPERMANENTLY	301
#define HTTP_STATUSCODE_FOUND				302
#define HTTP_STATUSCODE_SEEOTHER			303
#define HTTP_STATUSCODE_NOTMODIFIED			304
#define HTTP_STATUSCODE_USEPROXY			305
#define HTTP_STATUSCODE_SWITCHPROXY			306
#define HTTP_STATUSCODE_TEMPORARYREDIRECT	307
#define HTTP_STATUSCODE_PERMANENTREDIRECT	308
// 4xx Client errors
#define HTTP_STATUSCODE_BADREQUEST			400
#define HTTP_STATUSCODE_UNAUTHORIZED		401
#define HTTP_STATUSCODE_PAYMENTREQUIRED		402
#define HTTP_STATUSCODE_FORBIDDEN			403
#define HTTP_STATUSCODE_NOTFOUND			404
#define HTTP_STATUSCODE_METHODNOTALLOWED	405
#define HTTP_STATUSCODE_NOTAPPLICABLE		406
#define HTTP_STATUSCODE_PROXYAUTHREQUIRED	407
#define HTTP_STATUSCODE_REQUESTTIMEOUT		408
#define HTTP_STATUSCODE_CONFLICT			409
#define HTTP_STATUSCODE_GONE				410
#define HTTP_STATUSCODE_LENGTHREQUIRED		411
#define HTTP_STATUSCODE_PRECONDITIONFAILED	412
#define HTTP_STATUSCODE_PAYLOADTOOLARGE		413
#define HTTP_STATUSCODE_URITOOLONG			414
#define HTTP_STATUSCODE_UNSUPPMEDIATYPE		415
#define HTTP_STATUSCODE_RANGENOTSATISFIABLE	416
#define HTTP_STATUSCODE_EXPECTATIONFAILED	417
#define HTTP_STATUSCODE_IMATEAPOT			418
#define HTTP_STATUSCODE_MISDIRECTEDREQUEST	421
#define HTTP_STATUSCODE_UNPROCESSABLEENTITY	422
#define HTTP_STATUSCODE_LOCKED				423
#define HTTP_STATUSCODE_FAILEDDEPENDENCY	424
#define HTTP_STATUSCODE_UPGRADEREQUIRED		426
#define HTTP_STATUSCODE_PRECONDREQUIRED		428
#define HTTP_STATUSCODE_TOOMANYREQUESTS		429
#define HTTP_STATUSCODE_REQHDRFLDSTOOLARGE	431
#define HTTP_STATUSCODE_UNAVAILABLELEGAL	451
// 5xx Server errors
#define HTTP_STATUSCODE_INTERNALSERVER		500
#define HTTP_STATUSCODE_NOTIMPLEMENTED		501
#define HTTP_STATUSCODE_BADGATEWAY			502
#define HTTP_STATUSCODE_SERVICEUNAVAILABLE	503
#define HTTP_STATUSCODE_GATEWAYTIMEOUT		504
#define HTTP_STATUSCODE_HTTPVERSIONNOTSUPP	505
#define HTTP_STATUSCODE_VARALSONEGOTIAGES	506
#define HTTP_STATUSCODE_INSUFFICIENTSTORAGE	507
#define HTTP_STATUSCODE_LOOPDETECTED		508
#define HTTP_STATUSCODE_NOTEXTENDED			510
#define HTTP_STATUSCODE_NETAUTHREQUIRED		511

// Functions exported from the libhttp library
int httpParseUrl(const char *, urlInfo **);
void httpFreeUrlInfo(urlInfo *);
const char *httpUrlSchemeToString(urlScheme);
int httpGet(urlInfo *, variableList *, char **, int *, unsigned);

#endif

