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
//  kill.c
//

// This is the UNIX-style command for killing processes

/* This is the text that appears when a user requests help about this program
<help>

 -- kill --

Kill (stop) programs or processes.

Usage:
  kill <process1> [process2] [...]

The 'kill' command can be used to stop and eliminate one or more programs or
processes.  The only mandatory parameter is a process ID number (and,
optionally, any number of additional process ID numbers).  To see a list of
running processes, use the 'ps' command.

</help>
*/

#include <errno.h>
#include <libintl.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/api.h>
#include <sys/env.h>

#define _(string) gettext(string)


static void usage(char *name)
{
	printf("%s", _("usage:\n"));
	printf(_("%s <process1> [process2] [...]\n"), name);
}


int main(int argc, char *argv[])
{
	// This command will prompt the multitasker to kill the process with
	// the supplied process id

	int status = 0;
	int processId = 0;
	int count = 1;

	setlocale(LC_ALL, getenv(ENV_LANG));
	textdomain("kill");

	if (argc < 2)
	{
		usage(argv[0]);
		return (status = ERR_ARGUMENTCOUNT);
	}

	// Loop through all of our process ID arguments
	for (count = 1; count < argc; count ++)
	{
		// Make sure our argument isn't NULL
		if (!argv[count])
			return (status = ERR_NULLPARAMETER);

		processId = atoi(argv[count]);

		// OK?
		if (errno)
		{
			status = errno;
			perror("atoi");
			usage(argv[0]);
			return (status);
		}

		// Kill a process
		status = multitaskerKillProcess(processId);
		if (status < 0)
		{
			fprintf(stderr, "%s: ", argv[0]);
			errno = status;
			perror(argv[count]);
		}
		else
		{
			printf(_("%d killed\n"), processId);
		}
	}

	// Return success
	return (status = 0);
}

