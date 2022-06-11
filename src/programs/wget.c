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
//  wget.c
//

// Retrieve files from the web via HTTP

/* This is the text that appears when a user requests help about this program
<help>

 -- wget --

Download files from the web.

Usage:
  wget <URL>

This command will send HTTP commands to download documents and elements
referenced by them - for example images.

</help>
*/

#include <errno.h>
#include <libgen.h>
#include <libintl.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/api.h>
#include <sys/env.h>
#include <sys/file.h>
#include <sys/http.h>
#include <sys/url.h>
#include <sys/vis.h>
#include <sys/vsh.h>

#define DEFAULT_RECURSIONS	16

#define _(string) gettext(string)

extern int debugLibHttp;


static void usage(char *name)
{
	fprintf(stderr, _("usage:\n%s <URL>\n"), name);
	return;
}


static int getUrl(const char *urlString, int maxRecursions)
{
	int status = 0;
	urlInfo *url = NULL;
	char fullPath[MAX_PATH_NAME_LENGTH + 1];
	char *dirName = NULL;
	variableList header;
	const char *statusCode = NULL;
	const char *redirect = NULL;
	char *content = NULL;
	int contentLen = 0;
	fileStream outStream;

	printf("Parsing '%s'\n", urlString);

	status = httpParseUrl(urlString, &url);
	if (status < 0)
	{
		fprintf(stderr, "%s\n", _("Error parsing URL"));
		goto out;
	}

	// Compose the full path to the file we'll be saving
	strncpy(fullPath, urlString, MAX_PATH_NAME_LENGTH);
	if (url->host && url->path)
	{
		snprintf(fullPath, MAX_PATH_NAME_LENGTH, "%s/%s", url->host,
			url->path);
	}

	// Get the directory part and try to make sure it exists
	dirName = dirname(fullPath);
	if (dirName)
	{
		vshMakeDirRecursive(dirName);
		free(dirName);
	}

	status = httpGet(url, &header, &content, &contentLen,
		10000 /* 10 second timeout */);
	if (status < 0)
	{
		statusCode = variableListGet(&header, HTTP_STATUSCODE_VAR);
		if (statusCode)
		{
			switch (atoi(statusCode))
			{
				case HTTP_STATUSCODE_MOVEDPERMANENTLY:
				case HTTP_STATUSCODE_SEEOTHER:
				case HTTP_STATUSCODE_TEMPORARYREDIRECT:
				case HTTP_STATUSCODE_PERMANENTREDIRECT:
				{
					// See whether we can retrieve it at another URL
					redirect = variableListGet(&header, HTTP_HEADER_LOCATION);
					if (redirect)
					{
						if (!strcmp(urlString, redirect))
						{
							fprintf(stderr, "%s %s\n", _("Error redirection "
								"to same URL"), urlString);
							status = ERR_ALREADY;
							goto out;
						}

						status = getUrl(redirect, (maxRecursions - 1));
						goto out;
					}
				}
			}
		}

		fprintf(stderr, "%s %s\n", _("Error retrieving"), urlString);
		goto out;
	}

	status = fileStreamOpen(fullPath, (OPENMODE_WRITE | OPENMODE_CREATE |
		OPENMODE_TRUNCATE), &outStream);
	if (status < 0)
	{
		fprintf(stderr, "%s %s\n", _("Error writing"), urlString);
		goto out;
	}

	status = fileStreamWrite(&outStream, contentLen, content);

	fileStreamClose(&outStream);

	if (status < 0)
	{
		fprintf(stderr, "%s %s\n", _("Error writing"), urlString);
		goto out;
	}

	status = 0;

out:
	if (content)
		free(content);
	if (header.memory)
		variableListDestroy(&header);
	if (url)
		httpFreeUrlInfo(url);

	return (status);
}


int main(int argc, char *argv[])
{
	int status = 0;
	int count;

	setlocale(LC_ALL, getenv(ENV_LANG));
	textdomain("wget");

	// Make sure networking is enabled
	if (!networkEnabled())
	{
		fprintf(stderr, "%s\n", _("Networking is not currently enabled"));
		return (status = ERR_NOTINITIALIZED);
	}

	if (argc < 2)
	{
		usage(argv[0]);
		return (status = ERR_ARGUMENTCOUNT);
	}

	for (count = 1; count < argc; count ++)
	{
		status = getUrl(argv[count], DEFAULT_RECURSIONS);
		if (status < 0)
			continue;
	}

	return (status);
}

