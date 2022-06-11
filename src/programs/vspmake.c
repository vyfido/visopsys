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
//  vspmake.c
//

// This is a command for creating a Visopsys installer software package
// (.vsp) file from a manifest

/* This is the text that appears when a user requests help about this program
<help>

 -- vspmake --

Create software package files.

Usage:
  vspmake <manifest>

Manifests are text files with the following format:

  name=<short package name>
  desc=<longer single-line package description>
  arch=<none | x86 | x86_64 | all>
  version=<1-4 part numeric version string xxx.xxx.xxx.xxx)
  depend=<dependency short package name>|<relational e.g >= operator > \
    |<dependency numeric version string>
  obsolete=<obsolescence short package name>|<relational e.g. <= operator> \
    |<obsolescence numeric version string>
  preexec=<pathname of pre-install program>
  postexec=<pathname of pre-install program>
  <path to install file at creation time>=<target path at install time>
  ...

Apart from name and version, most of these fields are optional.

Lines beginning with no known variable name are assumed to be install files
(usually, one would list these at the end).

If the creation-time path of a file and the target path at install time are
the same, the =<target path at install time> can be omitted.

Parent directory creation is implied by file names, but directories can be
explicitly created by appending an '/' to a name, and need not exist at the
time of package creation.

Example:

  # Example manifest for foo
  name=foo
  desc=Foo Package!
  arch=x86
  version=0.91.0
  depend=bar|>=|4.51
  depend=baz|=|0.91
  obsolete=foo|<=|0.91.0

  preexec=build/premature
  postexec=/tmp/aprez-vous

  /create/this/directory/
  build/file1=/foo/file1
  build/file2=/foo/file2
  build/premature
  build/aprez-vous=/temp/aprez-vous

</help>
*/

#include <errno.h>
#include <libintl.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef PORTABLE
	#include <sys/env.h>
	#include <sys/install.h>
#endif

#define _(string) gettext(string)

#ifdef DEBUG
	extern int debugLibInstall;
#endif


static void usage(char *name)
{
	fprintf(stderr, _("usage:\n%s <manifest>\n"), name);
}


static int createFilesArchive(installInfo *info, char **archiveFileName)
{
	// Create an archive of the package files

	int status = 0;

	status = installArchiveCreate(info, archiveFileName);

	return (status);
}


int main(int argc, char *argv[])
{
	int status = 0;
	const char *inFileName = NULL;
	installInfo *info = NULL;
	char *archiveFileName = NULL;
	char *outFileName = NULL;

	setlocale(LC_ALL, getenv(ENV_LANG));
	textdomain("vspmake");

	if (argc != 2)
	{
		usage(argv[0]);
		return (status = ERR_ARGUMENTCOUNT);
	}

	inFileName = argv[argc - 1];

	// Turn the manifest into an installInfo structure
	status = installManifestFileInfoCreate(inFileName, &info);
	if (status < 0)
		goto out;

	// Create the files archive
	status = createFilesArchive(info, &archiveFileName);
	if (status < 0)
		goto out;

	// Compose the output filename

	outFileName = calloc(sizeof(INSTALL_PACKAGE_NAMEFORMAT) +
		strlen(info->name) + strlen(info->version) + strlen(info->arch), 1);
	if (!outFileName)
	{
		fprintf(stderr, "Memory error\n");
		status = ERR_MEMORY;
		goto out;
	}

	sprintf(outFileName, INSTALL_PACKAGE_NAMEFORMAT, info->name,
		info->version, info->arch);

	// Create the final package file
	status = installWrapArchivePackage(info, archiveFileName, outFileName);

out:
	if (outFileName)
		free(outFileName);

	if (archiveFileName)
	{
		unlink(archiveFileName);
		free(archiveFileName);
	}

	if (info)
		installInfoFree(info);

	return (status);
}

