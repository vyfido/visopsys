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
//  vspinst.c
//

// This is a command for installing a Visopsys installer software package
// file

/* This is the text that appears when a user requests help about this program
<help>

 -- vspinst --

Install software package files.

Usage:
  vspinst <package>

Software package files are binary files, usually with a .vsp extension.

</help>
*/

#include <errno.h>
#include <libintl.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/env.h>
#include <sys/install.h>

#define _(string) gettext(string)

#ifdef DEBUG
	extern int debugLibInstall;
#endif


static void usage(char *name)
{
	fprintf(stderr, _("usage:\n%s <package>\n"), name);
}


int main(int argc, char *argv[])
{
	int status = 0;
	const char *inFileName = NULL;
	installInfo *info = NULL;
	char *archiveFileName = NULL;
	char *filesDirName = NULL;

	setlocale(LC_ALL, getenv(ENV_LANG));
	textdomain("vspinst");

	if (argc != 2)
	{
		usage(argv[0]);
		return (status = ERR_ARGUMENTCOUNT);
	}

	inFileName = argv[argc - 1];

	// Extract the installInfo struct and the files archive
	status = installUnwrapPackageArchive(inFileName, &info, &archiveFileName);
	if (status < 0)
		goto out;

	// Check whether installation is expected to succeed
	status = installCheck(info, NULL /* root directory */);
	if (status < 0)
		goto out;

	// Extract the files archive
	status = installArchiveExtract(archiveFileName, &filesDirName);
	if (status < 0)
		goto out;

	// Perform the installation
	status = installPackageAdd(info, filesDirName, NULL /* root directory */);
	if (status < 0)
		goto out;

	status = 0;

out:
	if (filesDirName)
		free(filesDirName);

	if (archiveFileName)
	{
		unlink(archiveFileName);
		free(archiveFileName);
	}

	if (info)
		installInfoFree(info);

	return (status);
}

