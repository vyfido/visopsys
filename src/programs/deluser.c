//
//  Visopsys
//  Copyright (C) 1998-2014 J. Andrew McLaughlin
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
//  deluser.c
//

// This is the UNIX-style command for deleting a user

/* This is the text that appears when a user requests help about this program
<help>

 -- deluser --

Delete a user account from the system

Usage:
  deluser <user_name>

The deluser program is a very simple method of deleting a user account.

</help>
*/

#include <stdio.h>
#include <errno.h>
#include <sys/api.h>


static void usage(char *name)
{
	printf("usage:\n");
	printf("%s <username>\n", name);
	return;
}


int main(int argc, char *argv[])
{
	int status = 0;

	if (argc != 2)
	{
		usage(argv[0]);
		return (status = ERR_ARGUMENTCOUNT);
	}

	// Make sure the user exists
	if (!userExists(argv[1]))
	{
		fprintf(stderr, "User %s does not exist.\n", argv[1]);
		return (status = ERR_NOSUCHUSER);
	}

	status = userDelete(argv[1]);
	if (status < 0)
	{
		errno = status;
		return (status);
	}

	printf("User deleted.\n");

	// Done
	return (status = 0);
}

