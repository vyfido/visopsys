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
//  who.c
//

// This is the UNIX-style command for viewing users logged in to the system

/* This is the text that appears when a user requests help about this program
<help>

 -- who --

Show who is logged in.

Usage:
  who

</help>
*/

#include <errno.h>
#include <libintl.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/api.h>
#include <sys/env.h>
#include <sys/network.h>
#include <sys/user.h>

#define MAX_SESSIONS	64

#define _(string) gettext(string)
#define gettext_noop(string) (string)


int main(void)
{
	int status = 0;
	userSession *sessions = NULL;
	int count = 0;

	setlocale(LC_ALL, getenv(ENV_LANG));
	textdomain("who");

	// Get memory to hold the list of user sessions
	sessions = calloc(MAX_SESSIONS, sizeof(userSession));
	if (!sessions)
	{
		status = errno;
		perror("calloc");
		goto out;
	}

	// Get the list of user sessions
	status = userGetSessions(sessions, MAX_SESSIONS);
	if (status <= 0)
	{
		errno = status;
		perror("userGetSessions");
		goto out;
	}

	for (count = 0; count < status; count ++)
	{
		printf("%s\t\t", sessions[count].name);
		if (sessions[count].type == session_local)
			printf("%s\t\t", _("local"));
		else
			printf("%s\t\t", _("network")); // implement later
		printf("pid=%d", sessions[count].loginPid);
		printf("\n");
	}

	status = 0;

out:
	if (sessions)
		free(sessions);

	return (status);
}

