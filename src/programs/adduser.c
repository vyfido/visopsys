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
//  adduser.c
//

// This is the UNIX-style command for adding a user

#include <stdio.h>
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
      usage((argc > 0)? argv[0] : "adduser");
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