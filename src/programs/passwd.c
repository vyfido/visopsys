//
//  Visopsys
//  Copyright (C) 1998-2004 J. Andrew McLaughlin
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
//  59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
//
//  passwd.c
//

// This is the UNIX-style command for setting a password

#include <stdio.h>
#include <string.h>
#include <sys/api.h>
#include <sys/vsh.h>


static void usage(char *name)
{
  printf("usage:\n");
  printf("%s <username>\n", name);
  return;
}


int main(int argc, char *argv[])
{
  int status = 0;
  char oldPassword[17];
  char newPassword[17];
  char vrfyPassword[17];

  if (argc != 2)
    {
      usage((argc > 0)? argv[0] : "passwd");
      return (status = ERR_ARGUMENTCOUNT);
    }      

  // With the user name, we try to authenticate with no password
  status = userAuthenticate(argv[1], "");
  if (status < 0)
    {
      if (status == ERR_PERMISSION)
	vshPasswordPrompt("Enter current password: ", oldPassword);
      else
	{
	  errno = status;
	  perror(argv[0]);
	  return (status);
	}
    }

  status = userAuthenticate(argv[1], oldPassword);
  if (status < 0)
    {
      errno = status;

      if (status == ERR_PERMISSION)
	printf("Password incorrect\n");
      else
	perror(argv[0]);
      return (status);
    }

  while(1)
    {
      char prompt[80];
      sprintf(prompt, "Enter new password for %s: ", argv[1]);
      vshPasswordPrompt(prompt, newPassword);
      vshPasswordPrompt("Confirm password: ", vrfyPassword);

      if (!strcmp(newPassword, vrfyPassword))
	break;

      printf("\nPasswords do not match.\n\n");
    }

  status = userSetPassword(argv[1], oldPassword, newPassword);
  if (status < 0)
    {
      errno = status;
      perror(argv[0]);
      return (status);
    }

  printf("Password changed.\n");

  // Done
  return (errno = status = 0);
}