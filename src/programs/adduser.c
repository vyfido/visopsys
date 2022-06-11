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
//  adduser.c
//

// This is the UNIX-style command for adding a user

/* This is the text that appears when a user requests help about this program
<help>

 -- adduser --

Add a user account to the system

Usage:
  adduser <user_name>

The adduser program is a very simple method of adding a user account.  The
resulting account has no password assigned (you can use the passwd command
to set the password).

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

	// With the user name, we try to authenticate with no password
	status = userAuthenticate(argv[1], "");
	if (status == ERR_PERMISSION)
	{
		errno = status;
		printf("User %s already exists.\n", argv[1]);
		return (status);
	}

	status = userAdd(argv[1], "");
	errno = status;

	printf("User added.\n");

	// Done
	return (status);
}
