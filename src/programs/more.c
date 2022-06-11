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
//  more.c
//

// This is the UNIX-style command for reading files page by page

/* This is the text that appears when a user requests help about this program
<help>

 -- more --

Display file's contents, one screenfull at a time.

Usage:
  more <file1> [file2] [...]

Each file name listed after the command name will be printed in sequence.
This is similar to the 'cat' command, except that the file contents are
displayed one screenfull at a time.  To page forward to the next screenfull,
press the [SPACE] key.  To quit, press the [Q] key.  To advance by a single
line, press any other key.

</help>
*/

#include <errno.h>
#include <libintl.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/env.h>
#include <sys/vsh.h>

#define _(string) gettext(string)


static void usage(char *name)
{
	printf("%s", _("usage:\n"));
	printf(_("%s <file1> [file2] [...]\n"), name);
}


int main(int argc, char *argv[])
{
	int status = 0;
	int count = 0;

	setlocale(LC_ALL, getenv(ENV_LANG));
	textdomain("more");

	if (argc < 2)
	{
		usage(argv[0]);
		return (status = ERR_ARGUMENTCOUNT);
	}

	// Loop through all of our file name arguments
	for (count = 1; count < argc; count ++)
	{
		// Make sure the name isn't NULL
		if (!argv[count])
			return (status = ERR_NULLPARAMETER);

		status = vshPageFile(argv[count], _("--More--(%d%%)"));
		if (status < 0)
		{
			fprintf(stderr, "%s: ", argv[0]);
			errno = status;
			perror(argv[count]);
			if (count < (argc - 1))
				continue;
			else
				return (status);
		}
	}

	// Return success
	return (status = 0);
}

