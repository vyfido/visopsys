//
//  Visopsys
//  Copyright (C) 1998-2011 J. Andrew McLaughlin
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
//  uname.c
//

// This is the UNIX-style command for viewing basic info about the system

/* This is the text that appears when a user requests help about this program
<help>

 -- uname --

Prints system information.

Usage:
  uname

Options:
-a              : Show all information
-s              : Show the kernel name
-n              : Show the network host name
-r              : Show the kernel release
-v              : Show the kernel version
-m              : Show the machine hardware name
-p              : Show the processor type
-i              : Show the hardware platform
-o              : Show the operating system

</help>
*/

#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/api.h>
#include <sys/utsname.h>


int main(int argc, char *argv[])
{
  int status = 0;
  char opt;
  int sysname = 0, nodename = 0, release = 0, version = 0, machine = 0,
    domainname = 0;
  struct utsname data;

  if (argc > 1)
    {
      while (strchr("asnrvmpio", (opt = getopt(argc, argv, "asnrvmpio"))))
	{
	  switch (opt)
	    {
	    case 'a':
	      sysname = 1, nodename = 1, release = 1, version = 1, machine = 1,
		domainname = 1;
	      break;

	    case 's':
	    case 'o':
	      sysname = 1;
	      break;

	    case 'n':
	      nodename = 1;
	      break;
	  
	    case 'r':
	      release = 1;
	      break;

	    case 'v':
	      version = 1;
	      break;
	  
	    case 'm':
	    case 'p':
	    case 'i':
	      machine = 1;
	      break;
	    }
	}
    }
  else
    sysname = 1;

  bzero(&data, sizeof(struct utsname));
  status = systemInfo(&data);
  if (status < 0)
    {
      errno = status;
      perror(argv[0]);
      return (status);
    }

  if (sysname && data.sysname[0])
    printf("%s ", data.sysname);
  if (nodename && data.nodename[0])
    {
      printf("%s", data.nodename);
      if (data.domainname[0])
	printf(".%s", data.domainname);
      printf(" ");
    }
  if (release && data.release[0])
    printf("%s ", data.release);
  if (version && data.version[0])
    printf("%s ", data.version);
  if (machine && data.machine[0])
    printf("%s ", data.machine);
  printf("\n");

  return (status = 0);
}
