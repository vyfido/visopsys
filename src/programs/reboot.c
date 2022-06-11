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
//  reboot.c
//

// This is the UNIX-style command for rebooting the system

#include <stdio.h>
#include <unistd.h>
#include <sys/api.h>

int main(int argc, char *argv[])
{
  // There's a nice system function for doing this.

  int status = 0;
  int force = 0;

  // Reboot forcefully?
  if (getopt(argc, argv, "f") == 'f')
    force = 1;

  status = shutdown(1, force);
  if (status < 0)
    {
      if (!force)
	printf("Use \"%s -f\" to force.\n", argv[0]);
      return (status);
    }

  // Wait for death
  while(1);
}
